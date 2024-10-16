/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2019 (c) Fraunhofer IOSB (Author: Klaus Schick)
 *    Copyright 2019 (c) Fraunhofer IOSB (Author: Julius Pfrommer)
 *    Copyright 2023 (c) Fraunhofer IOSB (Author: Andreas Ebner)
 */

#include "ua_server_internal.h"

#if UA_MULTITHREADING >= 100

static void
UA_AsyncOperation_delete(UA_AsyncOperation *ar) {
    UA_CallMethodRequest_clear(&ar->request_call);
    UA_CallMethodResult_clear(&ar->response_call);
    UA_ReadRequest_clear(&ar->request_read);
    UA_DataValue_clear(&ar->response_read);
    UA_WriteRequest_clear(&ar->request_write);
    UA_free(ar);
}

static void
UA_AsyncManager_sendAsyncResponse(UA_AsyncManager *am, UA_Server *server,
                                  UA_AsyncResponse *ar) {
    UA_LOCK_ASSERT(&server->serviceMutex);
    UA_LOCK_ASSERT(&am->queueLock);

    /* Get the session */
    UA_StatusCode res = UA_STATUSCODE_GOOD;
    UA_ResponseHeader *responseHeader = NULL;
    UA_Session* session = getSessionById(server, &ar->sessionId);
    if(!session) {
        UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                       "Async Service: Session %N no longer exists", ar->sessionId);
        UA_AsyncManager_removeAsyncResponse(&server->asyncManager, ar);
        return;
    }

    /* Check the channel */
    UA_SecureChannel *channel = session->channel;
    if(!channel) {
        UA_LOG_WARNING_SESSION(server->config.logging, session,
                               "Async Service Response cannot be sent. "
                               "No SecureChannel for the session.");
        UA_AsyncManager_removeAsyncResponse(&server->asyncManager, ar);
        return;
    }

    if(ar->operationType == UA_ASYNCOPERATIONTYPE_CALL){
        /* Okay, here we go, send the UA_CallResponse */
        responseHeader = (UA_ResponseHeader*)
                         &ar->response.callResponse.responseHeader;
        responseHeader->requestHandle = ar->requestHandle;
        res = sendResponse(server, channel, ar->requestId,
                     (UA_Response*)&ar->response, &UA_TYPES[UA_TYPES_CALLRESPONSE]);
    } else if(ar->operationType == UA_ASYNCOPERATIONTYPE_READ) {
        responseHeader = (UA_ResponseHeader*)
                         &ar->response.readResponse.responseHeader;
        responseHeader->requestHandle = ar->requestHandle;
        res = sendResponse(server, channel, ar->requestId,
                     (UA_Response*)&ar->response, &UA_TYPES[UA_TYPES_READRESPONSE]);
    } else if(ar->operationType == UA_ASYNCOPERATIONTYPE_WRITE) {
        responseHeader = (UA_ResponseHeader*)
                         &ar->response.writeResponse.responseHeader;
        responseHeader->requestHandle = ar->requestHandle;
        res = sendResponse(server, channel, ar->requestId,
                     (UA_Response*)&ar->response, &UA_TYPES[UA_TYPES_WRITERESPONSE]);
    }

    if(res != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING_SESSION(server->config.logging, session,
                               "Async Response for Req# %" PRIu32 " failed "
                               "with StatusCode %s", ar->requestId,
                               UA_StatusCode_name(res));
    }
    UA_AsyncManager_removeAsyncResponse(&server->asyncManager, ar);

}

/* Integrate operation result in the AsyncResponse and send out the response if
 * it is ready. */
static UA_Boolean
integrateOperationResult(UA_AsyncManager *am, UA_Server *server,
                         UA_AsyncOperation *ao) {
    UA_LOCK_ASSERT(&server->serviceMutex);
    UA_LOCK_ASSERT(&am->queueLock);

    /* Grab the open request, so we can continue to construct the response */
    UA_AsyncResponse *ar = ao->parent;

    /* Reduce the number of open results */
    ar->opCountdown -= 1;

    UA_LOG_DEBUG(server->config.logging, UA_LOGCATEGORY_SERVER,
                 "Return result in the server thread with %" PRIu32 " remaining",
                 ar->opCountdown);

    if(ao->operationType == UA_ASYNCOPERATIONTYPE_CALL) {
        /* Move the UA_CallMethodResult to UA_CallResponse */
        ar->response.callResponse.results[ao->index] = ao->response_call;
        UA_CallMethodResult_init(&ao->response_call);
    } else if (ao->operationType == UA_ASYNCOPERATIONTYPE_READ) {
        ar->response.readResponse.results[ao->index] = ao->response_read;
        UA_DataValue_init(&ao->response_read);
    } else if(ao->operationType == UA_ASYNCOPERATIONTYPE_WRITE) {
        ar->response.writeResponse.results[ao->index] = ao->response_write;
        UA_StatusCode_init(&ao->response_write);
    }

    /* Done with all operations -> send the response */
    UA_Boolean done = (ar->opCountdown == 0);
    if(done)
        UA_AsyncManager_sendAsyncResponse(am, server, ar);
    return done;
}

/* Process all operations in the result queue -> move content over to the
 * AsyncResponse. This is only done by the server thread. Returns the nmber of
 * completed async sesponses. */
static UA_UInt32
processAsyncResults(UA_Server *server) {
    UA_AsyncManager *am = &server->asyncManager;
    UA_LOCK_ASSERT(&server->serviceMutex);

    UA_UInt32 count = 0;
    UA_AsyncOperation *ao;
    UA_LOCK(&am->queueLock);
    while((ao = TAILQ_FIRST(&am->resultQueue))) {
        TAILQ_REMOVE(&am->resultQueue, ao, pointers);
        if(integrateOperationResult(am, server, ao))
            count++;
        UA_AsyncOperation_delete(ao);
        /* Pacify clang-analyzer */
        UA_assert(TAILQ_FIRST(&am->resultQueue) != ao);
        am->opsCount--;
    }
    UA_UNLOCK(&am->queueLock);
    return count;
}

/* Check if any operations have timed out */
static void
checkTimeouts(UA_Server *server, void *_) {
    /* Timeouts are not configured */
    if(server->config.asyncOperationTimeout <= 0.0)
        return;

    UA_EventLoop *el = server->config.eventLoop;
    UA_AsyncManager *am = &server->asyncManager;
    const UA_DateTime tNow = el->dateTime_nowMonotonic(el);

    UA_LOCK(&am->queueLock);

    /* Loop over the queue of dispatched ops */
    UA_AsyncOperation *op = NULL, *op_tmp = NULL;
    TAILQ_FOREACH_SAFE(op, &am->dispatchedQueue, pointers, op_tmp) {
        /* The timeout has not passed. Also for all elements following in the queue. */
        if(tNow <= op->parent->timeout)
            break;

        /* Mark as timed out and put it into the result queue */
        if(op->operationType == UA_ASYNCOPERATIONTYPE_CALL){
            op->response_call.statusCode = UA_STATUSCODE_BADTIMEOUT;
        } else if(op->operationType == UA_ASYNCOPERATIONTYPE_READ){
            op->response_read.status = UA_STATUSCODE_BADTIMEOUT;
        } else if(op->operationType == UA_ASYNCOPERATIONTYPE_WRITE){
            op->response_write = UA_STATUSCODE_BADTIMEOUT;
        }
        TAILQ_REMOVE(&am->dispatchedQueue, op, pointers);
        TAILQ_INSERT_TAIL(&am->resultQueue, op, pointers);
        UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                       "Operation was removed due to a timeout");
    }

    /* Loop over the queue of new ops */
    TAILQ_FOREACH_SAFE(op, &am->newQueue, pointers, op_tmp) {
        /* The timeout has not passed. Also for all elements following in the queue. */
        if(tNow <= op->parent->timeout)
            break;

        /* Mark as timed out and put it into the result queue */
        if(op->operationType == UA_ASYNCOPERATIONTYPE_CALL){
            op->response_call.statusCode = UA_STATUSCODE_BADTIMEOUT;
        } else if(op->operationType == UA_ASYNCOPERATIONTYPE_READ){
            op->response_read.status = UA_STATUSCODE_BADTIMEOUT;
        } else if(op->operationType == UA_ASYNCOPERATIONTYPE_WRITE){
            op->response_write = UA_STATUSCODE_BADTIMEOUT;
        }
        TAILQ_REMOVE(&am->newQueue, op, pointers);
        TAILQ_INSERT_TAIL(&am->resultQueue, op, pointers);
        UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                       "Operation was removed due to a timeout");
    }

    UA_UNLOCK(&am->queueLock);

    /* Integrate async results and send out complete responses */
    UA_LOCK(&server->serviceMutex);
    processAsyncResults(server);
    UA_UNLOCK(&server->serviceMutex);
}

void
UA_AsyncManager_init(UA_AsyncManager *am, UA_Server *server) {
    memset(am, 0, sizeof(UA_AsyncManager));
    TAILQ_INIT(&am->asyncResponses);
    TAILQ_INIT(&am->newQueue);
    TAILQ_INIT(&am->dispatchedQueue);
    TAILQ_INIT(&am->resultQueue);
    UA_LOCK_INIT(&am->queueLock);
}

void UA_AsyncManager_start(UA_AsyncManager *am, UA_Server *server) {
    /* Add a regular callback for checking timeouts and sending finished
     * responses at a 100ms interval. */
    addRepeatedCallback(server, (UA_ServerCallback)checkTimeouts,
                        NULL, 100.0, &am->checkTimeoutCallbackId);
}

void UA_AsyncManager_stop(UA_AsyncManager *am, UA_Server *server) {
    /* Add a regular callback for checking timeouts and sending finished
     * responses at a 100ms interval. */
    removeCallback(server, am->checkTimeoutCallbackId);
}

void
UA_AsyncManager_clear(UA_AsyncManager *am, UA_Server *server) {
    UA_AsyncOperation *ar, *ar_tmp;

    /* Clean up queues */
    UA_LOCK(&am->queueLock);
    TAILQ_FOREACH_SAFE(ar, &am->newQueue, pointers, ar_tmp) {
        TAILQ_REMOVE(&am->newQueue, ar, pointers);
        UA_AsyncOperation_delete(ar);
    }
    TAILQ_FOREACH_SAFE(ar, &am->dispatchedQueue, pointers, ar_tmp) {
        TAILQ_REMOVE(&am->dispatchedQueue, ar, pointers);
        UA_AsyncOperation_delete(ar);
    }
    TAILQ_FOREACH_SAFE(ar, &am->resultQueue, pointers, ar_tmp) {
        TAILQ_REMOVE(&am->resultQueue, ar, pointers);
        UA_AsyncOperation_delete(ar);
    }
    UA_UNLOCK(&am->queueLock);

    /* Remove responses */
    UA_AsyncResponse *current, *temp;
    TAILQ_FOREACH_SAFE(current, &am->asyncResponses, pointers, temp) {
        UA_AsyncManager_removeAsyncResponse(am, current);
    }

    /* Delete all locks */
    UA_LOCK_DESTROY(&am->queueLock);
}

UA_StatusCode
UA_AsyncManager_createAsyncResponse(UA_AsyncManager *am, UA_Server *server,
                                    const UA_NodeId *sessionId,
                                    const UA_UInt32 requestId, const UA_UInt32 requestHandle,
                                    const UA_AsyncOperationType operationType,
                                    UA_AsyncResponse **outAr) {
    UA_AsyncResponse *newentry = (UA_AsyncResponse*)UA_calloc(1, sizeof(UA_AsyncResponse));
    if(!newentry)
        return UA_STATUSCODE_BADOUTOFMEMORY;

    UA_StatusCode res = UA_NodeId_copy(sessionId, &newentry->sessionId);
    if(res != UA_STATUSCODE_GOOD) {
        UA_free(newentry);
        return res;
    }

    UA_EventLoop *el = server->config.eventLoop;

    am->asyncResponsesCount += 1;
    newentry->requestId = requestId;
    newentry->requestHandle = requestHandle;
    newentry->timeout = el->dateTime_nowMonotonic(el);
    newentry->operationType = operationType;
    if(server->config.asyncOperationTimeout > 0.0)
        newentry->timeout += (UA_DateTime)
            (server->config.asyncOperationTimeout * (UA_DateTime)UA_DATETIME_MSEC);
    TAILQ_INSERT_TAIL(&am->asyncResponses, newentry, pointers);

    *outAr = newentry;
    return UA_STATUSCODE_GOOD;
}

/* Remove entry and free all allocated data */
void
UA_AsyncManager_removeAsyncResponse(UA_AsyncManager *am, UA_AsyncResponse *ar) {
    TAILQ_REMOVE(&am->asyncResponses, ar, pointers);
    am->asyncResponsesCount -= 1;
    if(ar->operationType == UA_ASYNCOPERATIONTYPE_CALL){
        UA_CallResponse_clear(&ar->response.callResponse);
    }else if (ar->operationType == UA_ASYNCOPERATIONTYPE_READ) {
        UA_ReadResponse_clear(&ar->response.readResponse);
    }else if(ar->operationType == UA_ASYNCOPERATIONTYPE_WRITE) {
        UA_WriteResponse_clear(&ar->response.writeResponse);
    }
    UA_NodeId_clear(&ar->sessionId);
    UA_free(ar);
}

/* Enqueue next AsyncRequest */
UA_StatusCode
UA_AsyncManager_createAsyncOp(UA_AsyncManager *am, UA_Server *server,
                              UA_AsyncResponse *ar, size_t opIndex,
                              UA_AsyncOperationType operationType,
                              const void *opRequest) {
    if(server->config.maxAsyncOperationQueueSize != 0 &&
       am->opsCount >= server->config.maxAsyncOperationQueueSize) {
        UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                       "UA_Server_SetNextAsyncMethod: Queue exceeds limit (%d).",
                       (int unsigned)server->config.maxAsyncOperationQueueSize);
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    UA_AsyncOperation *ao = (UA_AsyncOperation*)UA_calloc(1, sizeof(UA_AsyncOperation));
    if(!ao) {
        UA_LOG_ERROR(server->config.logging, UA_LOGCATEGORY_SERVER,
                     "UA_Server_SetNextAsyncMethod: Mem alloc failed.");
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    UA_StatusCode result;
    switch(operationType) {
        case UA_ASYNCOPERATIONTYPE_INVALID:
            break;
        case UA_ASYNCOPERATIONTYPE_CALL:
            ao->operationType = UA_ASYNCOPERATIONTYPE_CALL;
            result = UA_CallMethodRequest_copy((const UA_CallMethodRequest *) opRequest, &ao->request_call);
            if(result != UA_STATUSCODE_GOOD) {
                UA_LOG_ERROR(server->config.logging, UA_LOGCATEGORY_SERVER,
                             "UA_Server_SetAsyncMethodResult: UA_CallMethodRequest_copy failed.");
                UA_free(ao);
                return result;
            }
            UA_CallMethodResult_init(&ao->response_call);
            break;
        case UA_ASYNCOPERATIONTYPE_READ:
            ao->operationType = UA_ASYNCOPERATIONTYPE_READ;
            result = UA_ReadRequest_copy((const UA_ReadRequest *) opRequest, &ao->request_read);
            if(result != UA_STATUSCODE_GOOD) {
                UA_LOG_ERROR(server->config.logging, UA_LOGCATEGORY_SERVER,
                             "UA_Server_SetAsyncMethodResult: UA_ReadRequest_copy failed.");
                UA_free(ao);
                return result;
            }
            UA_DataValue_init(&ao->response_read);
            break;
        case UA_ASYNCOPERATIONTYPE_WRITE:
            ao->operationType = UA_ASYNCOPERATIONTYPE_WRITE;
            result = UA_WriteRequest_copy((const UA_WriteRequest *) opRequest, &ao->request_write);
            if(result != UA_STATUSCODE_GOOD) {
                UA_LOG_ERROR(server->config.logging, UA_LOGCATEGORY_SERVER,
                             "UA_Server_SetAsyncMethodResult: UA_WriteRequest_copy failed.");
                UA_free(ao);
                return result;
            }
            UA_DataValue_init(&ao->response_read);
            break;
    }

    ao->index = opIndex;
    ao->parent = ar;

    UA_LOCK(&am->queueLock);
    TAILQ_INSERT_TAIL(&am->newQueue, ao, pointers);
    am->opsCount++;
    ar->opCountdown++;
    UA_UNLOCK(&am->queueLock);

    if(server->config.asyncOperationNotifyCallback)
        server->config.asyncOperationNotifyCallback(server);

    return UA_STATUSCODE_GOOD;
}

/* Get and remove next Method Call Request */
UA_Boolean
UA_Server_getAsyncOperationNonBlocking(UA_Server *server, UA_AsyncOperationType *type,
                                       const UA_AsyncOperationRequest **request,
                                       void **context, UA_DateTime *timeout,
                                       UA_NodeId *sessionId, size_t *opIndex) {
    UA_AsyncManager *am = &server->asyncManager;

    UA_Boolean bRV = false;
    *type = UA_ASYNCOPERATIONTYPE_INVALID;
    UA_LOCK(&am->queueLock);
    UA_AsyncOperation *ao = TAILQ_FIRST(&am->newQueue);
    if(ao) {
        TAILQ_REMOVE(&am->newQueue, ao, pointers);
        TAILQ_INSERT_TAIL(&am->dispatchedQueue, ao, pointers);
        *type = ao->operationType;
        if(ao->operationType == UA_ASYNCOPERATIONTYPE_CALL){
           *request = (UA_AsyncOperationRequest*)&ao->request_call;
        } else if (ao->operationType == UA_ASYNCOPERATIONTYPE_READ){
           if(opIndex)
               *opIndex = ao->index;
           *request = (UA_AsyncOperationRequest*)&ao->request_read;
        } else if (ao->operationType == UA_ASYNCOPERATIONTYPE_WRITE){
           *request = (UA_AsyncOperationRequest*)&ao->request_write.nodesToWrite[ao->index];
        }
        *context = (void*)ao;
        if(timeout)
            *timeout = ao->parent->timeout;
        if(sessionId)
            *sessionId = ao->parent->sessionId;
        bRV = true;
    }
    UA_UNLOCK(&am->queueLock);

    return bRV;
}

/* Worker submits Method Call Response */
void
UA_Server_setAsyncOperationResult(UA_Server *server,
                                  const UA_AsyncOperationResponse *response,
                                  void *context) {
    UA_AsyncManager *am = &server->asyncManager;

    UA_AsyncOperation *ao = (UA_AsyncOperation*)context;
    if(!ao) {
        /* Something went wrong. Not a good AsyncOp. */
        UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                       "UA_Server_SetAsyncMethodResult: Invalid context");
        return;
    }

    UA_LOCK(&am->queueLock);

    /* See if the operation is still in the dispatched queue. Otherwise it has
     * been removed due to a timeout.
     *
     * TODO: Add a tree-structure for the dispatch queue. The linear lookup does
     * not scale. */
    UA_AsyncOperation *op = NULL;
    TAILQ_FOREACH(op, &am->dispatchedQueue, pointers) {
        if(op == ao)
            break;
    }
    if(!op) {
        UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                       "UA_Server_SetAsyncMethodResult: The operation has timed out");
        UA_UNLOCK(&am->queueLock);
        return;
    }
    UA_StatusCode result;
    if(ao->operationType == UA_ASYNCOPERATIONTYPE_CALL){
        result = UA_CallMethodResult_copy(&response->callMethodResult, &ao->response_call);
        /* Copy the result into the internal AsyncOperation */
        if(result != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                           "UA_Server_SetAsyncMethodResult: UA_CallMethodResult_copy failed.");
            ao->response_call.statusCode = UA_STATUSCODE_BADOUTOFMEMORY;
        }
    } else if(ao->operationType == UA_ASYNCOPERATIONTYPE_READ){
        result = UA_DataValue_copy(&response->readResult, &ao->response_read);
        /* Copy the result into the internal AsyncOperation */
        if(result != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                           "UA_Server_SetAsyncMethodResult: UA_DataValue_copy failed.");
            ao->response_read.status = UA_STATUSCODE_BADOUTOFMEMORY;
        }
    } else if(ao->operationType == UA_ASYNCOPERATIONTYPE_WRITE){
        result = UA_StatusCode_copy(&response->writeResult, &ao->response_write);
        /* Copy the result into the internal AsyncOperation */
        if(result != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                           "UA_Server_SetAsyncMethodResult: UA_StatusCode_copy failed.");
            ao->response_read.status = UA_STATUSCODE_BADOUTOFMEMORY;
        }
    }

    /* Move to the result queue */
    TAILQ_REMOVE(&am->dispatchedQueue, ao, pointers);
    TAILQ_INSERT_TAIL(&am->resultQueue, ao, pointers);

    UA_UNLOCK(&am->queueLock);

    UA_LOG_DEBUG(server->config.logging, UA_LOGCATEGORY_SERVER,
                 "Set the result from the worker thread");
}

/******************/
/* Server Methods */
/******************/

static UA_StatusCode
setMethodNodeAsync(UA_Server *server, UA_Session *session,
                   UA_Node *node, UA_Boolean *isAsync) {
    if(node->head.nodeClass != UA_NODECLASS_METHOD)
        return UA_STATUSCODE_BADNODECLASSINVALID;
    node->head.async = *isAsync;
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_Server_setMethodNodeAsync(UA_Server *server, const UA_NodeId id,
                             UA_Boolean isAsync) {
    UA_StatusCode res = UA_STATUSCODE_GOOD;
    UA_LOCK(&server->serviceMutex);
    UA_Node *node =
        UA_NODESTORE_GET_EDIT_SELECTIVE(server, &id, UA_NODEATTRIBUTESMASK_NONE,
                                        UA_REFERENCETYPESET_NONE,
                                        UA_BROWSEDIRECTION_INVALID);
    if(node) {
        if(node->head.nodeClass == UA_NODECLASS_METHOD)
            node->head.async = isAsync;
        else
            res = UA_STATUSCODE_BADNODECLASSINVALID;
        UA_NODESTORE_RELEASE(server, node);
    } else {
        res = UA_STATUSCODE_BADNODEIDINVALID;
    }
    UA_UNLOCK(&server->serviceMutex);
    return res;
}

UA_StatusCode
UA_Server_processServiceOperationsAsync(UA_Server *server, UA_Session *session,
                                        UA_UInt32 requestId, UA_UInt32 requestHandle,
                                        UA_AsyncServiceOperation operationCallback,
                                        const size_t *requestOperations,
                                        const void *requests,
                                        const UA_DataType *requestOperationsType,
                                        size_t *responseOperations,
                                        const UA_DataType *responseOperationsType,
                                        UA_AsyncResponse **ar) {
    size_t ops = *requestOperations;
    if(ops == 0)
        return UA_STATUSCODE_BADNOTHINGTODO;

    /* Allocate the response array. No padding after size_t */
    void **respPos = (void**)((uintptr_t)responseOperations + sizeof(size_t));
    *respPos = UA_Array_new(ops, responseOperationsType);
    if(!*respPos)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    *responseOperations = ops;

    /* Finish / dispatch the operations. This may allocate a new AsyncResponse internally */
    uintptr_t respOp = (uintptr_t)*respPos;
    uintptr_t reqOp = *(uintptr_t*)((uintptr_t) requests);
    for(size_t i = 0; i < ops; i++) {
        operationCallback(server, session, requestId, requestHandle,
                          i, (void*)reqOp, (void*)respOp, ar);
        respOp += responseOperationsType->memSize;
    }

    return UA_STATUSCODE_GOOD;
}

UA_UInt32
UA_AsyncManager_cancel(UA_Server *server, UA_Session *session, UA_UInt32 requestHandle) {
    UA_LOCK_ASSERT(&server->serviceMutex);
    UA_AsyncManager *am = &server->asyncManager;

    UA_LOCK(&am->queueLock);

    /* Loop over the queue of dispatched ops */
    UA_AsyncOperation *op = NULL, *op_tmp = NULL;
    TAILQ_FOREACH_SAFE(op, &am->dispatchedQueue, pointers, op_tmp) {
        if(op->parent->requestHandle != requestHandle ||
           !UA_NodeId_equal(&session->sessionId, &op->parent->sessionId))
            continue;

        /* Set status and put it into the result queue */
        switch(op->operationType) {
            case UA_ASYNCOPERATIONTYPE_CALL:
                op->response_call.statusCode = UA_STATUSCODE_BADREQUESTCANCELLEDBYCLIENT;
                break;
            case UA_ASYNCOPERATIONTYPE_READ:
                op->response_read.status = UA_STATUSCODE_BADREQUESTCANCELLEDBYCLIENT;
                break;
            case UA_ASYNCOPERATIONTYPE_WRITE:
                op->response_write = UA_STATUSCODE_BADREQUESTCANCELLEDBYCLIENT;
                break;
            default:
                UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                               "Async Service: Cancel reefers to unknown operation type.");
        }
        TAILQ_REMOVE(&am->dispatchedQueue, op, pointers);
        TAILQ_INSERT_TAIL(&am->resultQueue, op, pointers);

        /* Also set the status of the overall response */
        op->parent->response.callResponse.responseHeader.
            serviceResult = UA_STATUSCODE_BADREQUESTCANCELLEDBYCLIENT;
    }

    /* Idem for waiting ops */
    TAILQ_FOREACH_SAFE(op, &am->newQueue, pointers, op_tmp) {
        if(op->parent->requestHandle != requestHandle ||
           !UA_NodeId_equal(&session->sessionId, &op->parent->sessionId))
            continue;

        /* Mark as timed out and put it into the result queue */
        switch(op->operationType) {
            case UA_ASYNCOPERATIONTYPE_CALL:
                op->response_call.statusCode = UA_STATUSCODE_BADREQUESTCANCELLEDBYCLIENT;
                break;
            case UA_ASYNCOPERATIONTYPE_READ:
                op->response_read.status = UA_STATUSCODE_BADREQUESTCANCELLEDBYCLIENT;
                break;
            case UA_ASYNCOPERATIONTYPE_WRITE:
                op->response_write = UA_STATUSCODE_BADREQUESTCANCELLEDBYCLIENT;
                break;
            default:
                UA_LOG_WARNING(server->config.logging, UA_LOGCATEGORY_SERVER,
                               "Async Service: Cancel reefers to unknown operation type.");
        }
        TAILQ_REMOVE(&am->newQueue, op, pointers);
        TAILQ_INSERT_TAIL(&am->resultQueue, op, pointers);

        op->parent->response.callResponse.responseHeader.
            serviceResult = UA_STATUSCODE_BADREQUESTCANCELLEDBYCLIENT;
    }

    UA_UNLOCK(&am->queueLock);

    /* Process messages that have all ops completed */
    return processAsyncResults(server);
}

static UA_StatusCode
setVariableNodeAsync(UA_Server *server, UA_Session *session,
                   UA_Node *node, UA_Boolean *isAsync) {
    if(node->head.nodeClass != UA_NODECLASS_VARIABLE)
        return UA_STATUSCODE_BADNODECLASSINVALID;
    node->head.async = *isAsync;
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_Server_setVariableNodeAsync(UA_Server *server, const UA_NodeId id,
                             UA_Boolean isAsync) {
    UA_LOCK(&server->serviceMutex);
    UA_StatusCode res =
        UA_Server_editNode(server, &server->adminSession, &id, UA_ATTRIBUTEID_INVALID,
                            UA_REFERENCETYPESET_ALL, UA_BROWSEDIRECTION_BOTH,
                           (UA_EditNodeCallback)setVariableNodeAsync, &isAsync);
    UA_UNLOCK(&server->serviceMutex);
    return res;
}

#endif
