/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2015 (c) Chris Iatrou
 *    Copyright 2015-2017 (c) Florian Palm
 *    Copyright 2015-2018 (c) Fraunhofer IOSB (Author: Julius Pfrommer)
 *    Copyright 2015-2016 (c) Sten Grüner
 *    Copyright 2015 (c) Oleksiy Vasylyev
 *    Copyright 2016 (c) LEvertz
 *    Copyright 2017 (c) Stefan Profanter, fortiss GmbH
 *    Copyright 2017 (c) Julian Grothoff
 *    Copyright 2020 (c) Hilscher Gesellschaft für Systemautomation mbH (Author: Martin Lang)
 */

#include "ua_services.h"
#include "ua_server_internal.h"

#ifdef UA_ENABLE_METHODCALLS /* conditional compilation */

static const UA_VariableNode *
getArgumentsVariableNode(UA_Server *server, const UA_MethodNode *ofMethod,
                         UA_String withBrowseName) {
    UA_NodeId hasProperty = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
    for(size_t i = 0; i < ofMethod->referencesSize; ++i) {
        UA_NodeReferenceKind *rk = &ofMethod->references[i];

        if(rk->isInverse != false)
            continue;

        if(!UA_NodeId_equal(&hasProperty, &rk->referenceTypeId))
            continue;

        for(size_t j = 0; j < rk->refTargetsSize; ++j) {
            const UA_Node *refTarget =
                UA_NODESTORE_GET(server, &rk->refTargets[j].targetId.nodeId);
            if(!refTarget)
                continue;
            if(refTarget->nodeClass == UA_NODECLASS_VARIABLE &&
               refTarget->browseName.namespaceIndex == 0 &&
               UA_String_equal(&withBrowseName, &refTarget->browseName.name)) {
                return (const UA_VariableNode*)refTarget;
            }
            UA_NODESTORE_RELEASE(server, refTarget);
        }
    }
    return NULL;
}

/* inputArgumentResults has the length request->inputArgumentsSize */
static UA_StatusCode
typeCheckArguments(UA_Server *server, UA_Session *session,
                   const UA_VariableNode *argRequirements, size_t argsSize,
                   UA_Variant *args, UA_StatusCode *inputArgumentResults) {
    /* Verify that we have a Variant containing UA_Argument (scalar or array) in
     * the "InputArguments" node */
    if(argRequirements->valueSource != UA_VALUESOURCE_DATA)
        return UA_STATUSCODE_BADINTERNALERROR;
    if(!argRequirements->value.data.value.hasValue)
        return UA_STATUSCODE_BADINTERNALERROR;
    if(argRequirements->value.data.value.value.type != &UA_TYPES[UA_TYPES_ARGUMENT])
        return UA_STATUSCODE_BADINTERNALERROR;

    /* Verify the number of arguments. A scalar argument value is interpreted as
     * an array of length 1. */
    size_t argReqsSize = argRequirements->value.data.value.value.arrayLength;
    if(UA_Variant_isScalar(&argRequirements->value.data.value.value))
        argReqsSize = 1;
    if(argReqsSize > argsSize)
        return UA_STATUSCODE_BADARGUMENTSMISSING;
    if(argReqsSize < argsSize)
        return UA_STATUSCODE_BADTOOMANYARGUMENTS;

    /* Type-check every argument against the definition */
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_Argument *argReqs = (UA_Argument*)argRequirements->value.data.value.value.data;
    for(size_t i = 0; i < argReqsSize; ++i) {
        if(!compatibleValue(server, session, &argReqs[i].dataType, argReqs[i].valueRank,
                            argReqs[i].arrayDimensionsSize, argReqs[i].arrayDimensions,
                            &args[i], NULL)) {
            inputArgumentResults[i] = UA_STATUSCODE_BADTYPEMISMATCH;
            retval = UA_STATUSCODE_BADINVALIDARGUMENT;
        }
    }
    return retval;
}

/* inputArgumentResults has the length request->inputArgumentsSize */
static UA_StatusCode
validMethodArguments(UA_Server *server, UA_Session *session, const UA_MethodNode *method,
                     const UA_CallMethodRequest *request,
                     UA_StatusCode *inputArgumentResults) {
    /* Get the input arguments node */
    const UA_VariableNode *inputArguments =
        getArgumentsVariableNode(server, method, UA_STRING("InputArguments"));
    if(!inputArguments) {
        if(request->inputArgumentsSize > 0)
            return UA_STATUSCODE_BADTOOMANYARGUMENTS;
        return UA_STATUSCODE_GOOD;
    }

    /* Verify the request */
    UA_StatusCode retval = typeCheckArguments(server, session, inputArguments,
                                              request->inputArgumentsSize,
                                              request->inputArguments,
                                              inputArgumentResults);

    /* Release the input arguments node */
    UA_NODESTORE_RELEASE(server, (const UA_Node*)inputArguments);
    return retval;
}

static const UA_NodeId hasComponentNodeId = {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_HASCOMPONENT}};
static const UA_NodeId hasSubTypeNodeId = {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_HASSUBTYPE}};
static const UA_NodeId organizedByNodeId = {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_ORGANIZES}};
static const UA_String namespaceDiModel = UA_STRING_STATIC("http://opcfoundation.org/UA/DI/");
static const UA_NodeId hasTypeDefinitionNodeId = {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_HASTYPEDEFINITION}};
// ns=0 will be replace dynamically. DI-Spec. 1.01: <UAObjectType NodeId="ns=1;i=1005" BrowseName="1:FunctionalGroupType">
static UA_NodeId functionGroupNodeId = {0, UA_NODEIDTYPE_NUMERIC, {1005}};

static void
callWithMethodAndObject(UA_Server *server, UA_Session *session,
                        const UA_CallMethodRequest *request, UA_CallMethodResult *result,
                        const UA_MethodNode *method, const UA_ObjectNode *object) {
    /* Verify the object's NodeClass */
    if(object->nodeClass != UA_NODECLASS_OBJECT &&
       object->nodeClass != UA_NODECLASS_OBJECTTYPE) {
        result->statusCode = UA_STATUSCODE_BADNODECLASSINVALID;
        return;
    }

    /* Verify the method's NodeClass */
    if(method->nodeClass != UA_NODECLASS_METHOD) {
        result->statusCode = UA_STATUSCODE_BADNODECLASSINVALID;
        return;
    }

    /* Is there a method to execute? */
    if(!method->method) {
        result->statusCode = UA_STATUSCODE_BADINTERNALERROR;
        return;
    }

    /* Verify method/object relations. Object must have a hasComponent or a
     * subtype of hasComponent reference to the method node. Therefore, check
     * every reference between the parent object and the method node if there is
     * a hasComponent (or subtype) reference */
    UA_Boolean found = false;
    for(size_t i = 0; i < object->referencesSize && !found; ++i) {
        UA_NodeReferenceKind *rk = &object->references[i];
        if(rk->isInverse)
            continue;
        if(!isNodeInTree(server, &rk->referenceTypeId,
                         &hasComponentNodeId, &hasSubTypeNodeId, 1))
            continue;
        for(size_t j = 0; j < rk->refTargetsSize; ++j) {
            if(UA_NodeId_equal(&rk->refTargets[j].targetId.nodeId, &request->methodId)) {
                found = true;
                break;
            }
        }
    }

    if(!found) {
        /* The following ParentObject evaluation is a workaround only to fulfill
         * the OPC UA Spec. Part 100 - Devices requirements regarding functional
         * groups. Compare OPC UA Spec. Part 100 - Devices, Release 1.02
         *    - 5.4 FunctionalGroupType
         *    - B.1 Functional Group Usages
         * A functional group is a sub-type of the FolderType and is used to
         * organize the Parameters and Methods from the complete set (named
         * ParameterSet and MethodSet) in (Functional) groups for instance
         * Configuration or Identification. The same Property, Parameter or
         * Method can be referenced from more than one FunctionalGroup. */

        /* Check whether the DI namespace is available */
        size_t foundNamespace = 0;
        UA_StatusCode res = UA_Server_getNamespaceByName(server, namespaceDiModel,
                                                         &foundNamespace);
        if(res != UA_STATUSCODE_GOOD) {
            result->statusCode = UA_STATUSCODE_BADMETHODINVALID;
            return;
        }
        functionGroupNodeId.namespaceIndex = (UA_UInt16)foundNamespace;

        /* Search for a HasTypeDefinition (or sub-) reference in the parent object */
        for(size_t i = 0; i < object->referencesSize && !found; ++i) {
            UA_NodeReferenceKind *rk = &object->references[i];
            if(rk->isInverse)
                continue;
            if(!isNodeInTree(server, &rk->referenceTypeId,
                             &hasTypeDefinitionNodeId, &hasSubTypeNodeId, 1))
                continue;
            
            /* Verify that the HasTypeDefinition is equal to FunctionGroupType
             * (or sub-type) from the DI model */
            for(size_t j = 0; j < rk->refTargetsSize && !found; ++j) {
                if(!isNodeInTree(server, &rk->refTargets[j].targetId.nodeId,
                                 &functionGroupNodeId, &hasSubTypeNodeId, 1))
                    continue;
                
                /* Search for the called method with reference Organize (or
                 * sub-type) from the parent object */
                for(size_t k = 0; k < object->referencesSize && !found; ++k) {
                    UA_NodeReferenceKind *rkInner = &object->references[k];
                    if(rkInner->isInverse)
                        continue;
                    if(!isNodeInTree(server, &rkInner->referenceTypeId,
                                     &organizedByNodeId, &hasSubTypeNodeId, 1))
                        continue;
                    
                    for(size_t m = 0; m < rkInner->refTargetsSize; ++m) {
                        if(UA_NodeId_equal(&rkInner->refTargets[m].targetId.nodeId,
                                           &request->methodId)) {
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
        if(!found) {
            result->statusCode = UA_STATUSCODE_BADMETHODINVALID;
            return;
        }
    }

    /* Verify access rights */
    UA_Boolean executable = method->executable;
    if(session != &server->adminSession) {
        UA_UNLOCK(server->serviceMutex);
        executable = executable && server->config.accessControl.
            getUserExecutableOnObject(server, &server->config.accessControl, &session->sessionId,
                                      session->sessionHandle, &request->methodId, method->context,
                                      &request->objectId, object->context);
        UA_LOCK(server->serviceMutex);
    }

    if(!executable) {
        result->statusCode = UA_STATUSCODE_BADNOTEXECUTABLE;
        return;
    }

    /* Allocate the inputArgumentResults array */
    result->inputArgumentResults = (UA_StatusCode*)
        UA_Array_new(request->inputArgumentsSize, &UA_TYPES[UA_TYPES_STATUSCODE]);
    if(!result->inputArgumentResults) {
        result->statusCode = UA_STATUSCODE_BADOUTOFMEMORY;
        return;
    }
    result->inputArgumentResultsSize = request->inputArgumentsSize;

    /* Verify Input Arguments */
    result->statusCode = validMethodArguments(server, session, method, request, result->inputArgumentResults);

    /* Return inputArgumentResults only for BADINVALIDARGUMENT */
    if(result->statusCode != UA_STATUSCODE_BADINVALIDARGUMENT) {
        UA_Array_delete(result->inputArgumentResults, result->inputArgumentResultsSize,
                        &UA_TYPES[UA_TYPES_STATUSCODE]);
        result->inputArgumentResults = NULL;
        result->inputArgumentResultsSize = 0;
    }

    /* Error during type-checking? */
    if(result->statusCode != UA_STATUSCODE_GOOD)
        return;

    /* Get the output arguments node */
    const UA_VariableNode *outputArguments =
        getArgumentsVariableNode(server, method, UA_STRING("OutputArguments"));

    /* Allocate the output arguments array */
    size_t outputArgsSize = 0;
    if(outputArguments)
        outputArgsSize = outputArguments->value.data.value.value.arrayLength;
    result->outputArguments = (UA_Variant*)
        UA_Array_new(outputArgsSize, &UA_TYPES[UA_TYPES_VARIANT]);
    if(!result->outputArguments) {
        result->statusCode = UA_STATUSCODE_BADOUTOFMEMORY;
        return;
    }
    result->outputArgumentsSize = outputArgsSize;

    /* Release the output arguments node */
    UA_NODESTORE_RELEASE(server, (const UA_Node*)outputArguments);

    /* Call the method */
    UA_UNLOCK(server->serviceMutex);
    result->statusCode = method->method(server, &session->sessionId, session->sessionHandle,
                                        &method->nodeId, method->context,
                                        &object->nodeId, object->context,
                                        request->inputArgumentsSize, request->inputArguments,
                                        result->outputArgumentsSize, result->outputArguments);
    UA_LOCK(server->serviceMutex);
    /* TODO: Verify Output matches the argument definition */
}

#if UA_MULTITHREADING >= 100

static void
Operation_CallMethodAsync(UA_Server *server, UA_Session *session, UA_UInt32 requestId,
                          UA_UInt32 requestHandle, size_t opIndex,
                          UA_CallMethodRequest *opRequest, UA_CallMethodResult *opResult,
                          UA_AsyncResponse **ar) {
    /* Get the method node */
    const UA_MethodNode *method = (const UA_MethodNode*)
        UA_NODESTORE_GET(server, &opRequest->methodId);
    if(!method) {
        opResult->statusCode = UA_STATUSCODE_BADNODEIDUNKNOWN;
        return;
    }

    /* Get the object node */
    const UA_ObjectNode *object = (const UA_ObjectNode*)
        UA_NODESTORE_GET(server, &opRequest->objectId);
    if(!object) {
        opResult->statusCode = UA_STATUSCODE_BADNODEIDUNKNOWN;
        UA_NODESTORE_RELEASE(server, (const UA_Node*)method);
        return;
    }

    /* Synchronous execution */
    if(!method->async) {
        callWithMethodAndObject(server, session, opRequest, opResult, method, object);
        goto cleanup;
    }

    /* <-- Async method call --> */

    /* No AsyncResponse allocated so far */
    if(!*ar) {
        opResult->statusCode =
            UA_AsyncManager_createAsyncResponse(&server->asyncManager, server,
                                                &session->sessionId, requestId,
                                                requestHandle, UA_ASYNCOPERATIONTYPE_CALL,
                                                ar);
        if(opResult->statusCode != UA_STATUSCODE_GOOD)
            goto cleanup;
    }

    /* Create the Async Request to be taken by workers */
    opResult->statusCode =
        UA_AsyncManager_createAsyncOp(&server->asyncManager,
                                      server, *ar, opIndex, opRequest);

 cleanup:
    /* Release the method and object node */
    UA_NODESTORE_RELEASE(server, (const UA_Node*)method);
    UA_NODESTORE_RELEASE(server, (const UA_Node*)object);
}

void
Service_CallAsync(UA_Server *server, UA_Session *session, UA_UInt32 requestId,
                  const UA_CallRequest *request, UA_CallResponse *response,
                  UA_Boolean *finished) {
    UA_LOG_DEBUG_SESSION(&server->config.logger, session, "Processing CallRequestAsync");
    if(server->config.maxNodesPerMethodCall != 0 &&
        request->methodsToCallSize > server->config.maxNodesPerMethodCall) {
        response->responseHeader.serviceResult = UA_STATUSCODE_BADTOOMANYOPERATIONS;
        return;
    }

    UA_AsyncResponse *ar = NULL;
    response->responseHeader.serviceResult =
        UA_Server_processServiceOperationsAsync(server, session, requestId,
                                                request->requestHeader.requestHandle,
                                                (UA_AsyncServiceOperation)Operation_CallMethodAsync,
                                                &request->methodsToCallSize,
                                                &UA_TYPES[UA_TYPES_CALLMETHODREQUEST],
                                                &response->resultsSize,
                                                &UA_TYPES[UA_TYPES_CALLMETHODRESULT], &ar);

    if(ar) {
        if(ar->opCountdown > 0) {
            /* Move all results to the AsyncResponse. The async operation results
             * will be overwritten when the workers return results. */
            ar->response.callResponse = *response;
            UA_CallResponse_init(response);
            *finished = false;
        } else {
            /* If there is a new AsyncResponse, ensure it has at least one pending
             * operation */
            UA_AsyncManager_removeAsyncResponse(&server->asyncManager, ar);
        }
    }
}
#endif

static void
Operation_CallMethod(UA_Server *server, UA_Session *session, void *context,
                     const UA_CallMethodRequest *request, UA_CallMethodResult *result) {
    /* Get the method node */
    const UA_MethodNode *method = (const UA_MethodNode*)
        UA_NODESTORE_GET(server, &request->methodId);
    if(!method) {
        result->statusCode = UA_STATUSCODE_BADNODEIDUNKNOWN;
        return;
    }

    /* Get the object node */
    const UA_ObjectNode *object = (const UA_ObjectNode*)
        UA_NODESTORE_GET(server, &request->objectId);
    if(!object) {
        result->statusCode = UA_STATUSCODE_BADNODEIDUNKNOWN;
        UA_NODESTORE_RELEASE(server, (const UA_Node*)method);
        return;
    }

    /* Continue with method and object as context */
    callWithMethodAndObject(server, session, request, result, method, object);

    /* Release the method and object node */
    UA_NODESTORE_RELEASE(server, (const UA_Node*)method);
    UA_NODESTORE_RELEASE(server, (const UA_Node*)object);
}

void Service_Call(UA_Server *server, UA_Session *session,
                  const UA_CallRequest *request,
                  UA_CallResponse *response) {
    UA_LOG_DEBUG_SESSION(&server->config.logger, session, "Processing CallRequest");
    UA_LOCK_ASSERT(server->serviceMutex, 1);

    if(server->config.maxNodesPerMethodCall != 0 &&
       request->methodsToCallSize > server->config.maxNodesPerMethodCall) {
        response->responseHeader.serviceResult = UA_STATUSCODE_BADTOOMANYOPERATIONS;
        return;
    }

    response->responseHeader.serviceResult =
        UA_Server_processServiceOperations(server, session, (UA_ServiceOperation)Operation_CallMethod, NULL,
                                           &request->methodsToCallSize, &UA_TYPES[UA_TYPES_CALLMETHODREQUEST],
                                           &response->resultsSize, &UA_TYPES[UA_TYPES_CALLMETHODRESULT]);
}

UA_CallMethodResult UA_EXPORT
UA_Server_call(UA_Server *server, const UA_CallMethodRequest *request) {
    UA_CallMethodResult result;
    UA_CallMethodResult_init(&result);
    UA_LOCK(server->serviceMutex);
    Operation_CallMethod(server, &server->adminSession, NULL, request, &result);
    UA_UNLOCK(server->serviceMutex);
    return result;
}

#endif /* UA_ENABLE_METHODCALLS */
