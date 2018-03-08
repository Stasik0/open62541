/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2018 (c) Ari Breitkreuz, fortiss GmbH
 */

#include "ua_server_internal.h"
#include "ua_subscription_events.h"

#ifdef UA_ENABLE_EVENTS

typedef struct node_iteration {
    LIST_ENTRY(node_iteration) listEntry;
    UA_NodeId *node;
} node_iteration;

typedef LIST_HEAD(nodeList, node_iteration) nodeList;

struct getNodesHandle {
    UA_Server *server;
    nodeList *nodes;
};

UA_StatusCode UA_Event_generateEventId(UA_Server *server, UA_ByteString *generatedId) {
    /* EventId is a ByteString, which is basically just a string
     * We will use a 16-Byte ByteString as an identifier */
    generatedId->length = 16;
    generatedId->data = (UA_Byte *) UA_malloc(16 * sizeof(UA_Byte));
    if (!generatedId->data) {
        UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_USERLAND,
                       "Server unable to allocate memory for EventId data.");
        UA_free(generatedId);
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    /* GUIDs are unique, have a size of 16 byte and already have
     * a generator so use that.
     * Make sure GUIDs really do have 16 byte, in case someone may
     * have changed that struct */
    UA_assert(sizeof(UA_Guid) == 16);
    UA_Guid tmpGuid = UA_Guid_random();
    memcpy(generatedId->data, &tmpGuid, 16);
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode UA_Server_getEventId(UA_Server *server, UA_NodeId *eventNodeId, UA_ByteString *outId) {
    UA_RelativePathElement rpe;
    UA_RelativePathElement_init(&rpe);
    rpe.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
    rpe.isInverse = false;
    rpe.includeSubtypes = false;
    rpe.targetName = UA_QUALIFIEDNAME(0, "EventId");

    UA_BrowsePath bp;
    UA_BrowsePath_init(&bp);
    bp.startingNode = *eventNodeId;
    bp.relativePath.elementsSize = 1;
    bp.relativePath.elements = &rpe;

    UA_StatusCode retval;

    UA_BrowsePathResult bpr = UA_Server_translateBrowsePathToNodeIds(server, &bp);
    if (bpr.statusCode != UA_STATUSCODE_GOOD || bpr.targetsSize < 1) {
        UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_USERLAND, "Event is missing EventId attribute.\n");
        return bpr.statusCode;
    }

    UA_Variant result;
    UA_Variant_init(&result);
    retval = UA_Server_readValue(server, bpr.targets[0].targetId.nodeId, &result);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_BrowsePathResult_deleteMembers(&bpr);
        return retval;
    }
#ifdef UA_DEBUG_EVENTS
    UA_assert(result.type == &UA_TYPES[UA_TYPES_BYTESTRING]);
#endif

    UA_ByteString_copy((UA_ByteString *) result.data, outId);
    UA_Variant_deleteMembers(&result);
    UA_BrowsePathResult_deleteMembers(&bpr);
    return retval;
}

static UA_StatusCode findAllSubtypesNodeIteratorCallback(UA_NodeId parentId, UA_Boolean isInverse,
                                                         UA_NodeId referenceTypeId, void *handle) {
    /* only subtypes of hasSubtype */
    UA_NodeId hasSubtypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE);
    if (isInverse || !UA_NodeId_equal(&referenceTypeId, &hasSubtypeId)) {
        return UA_STATUSCODE_GOOD;
    }

    node_iteration *entry = (node_iteration *) UA_malloc(sizeof(node_iteration));
    if (!entry) {
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    entry->node = UA_NodeId_new();
    if (!entry->node) {
        UA_free(entry);
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    UA_NodeId_copy(&parentId, entry->node);
    LIST_INSERT_HEAD(((struct getNodesHandle *) handle)->nodes, entry, listEntry);

    /* recursion */
    UA_Server_forEachChildNodeCall(((struct getNodesHandle *) handle)->server,
                                   parentId, findAllSubtypesNodeIteratorCallback, handle);
    return UA_STATUSCODE_GOOD;
}

void UA_Event_findVariableNode(UA_Server *server, UA_QualifiedName *name, size_t relativePathSize, UA_NodeId *event,
                               UA_BrowsePathResult *out) {
    /* get a list with all subtypes of aggregates */
    struct getNodesHandle handle;
    nodeList list;
    LIST_INIT(&list);
    handle.server = server;
    handle.nodes = &list;
    UA_StatusCode retval = UA_Server_forEachChildNodeCall(server, UA_NODEID_NUMERIC(0, UA_NS0ID_AGGREGATES),
                                                          findAllSubtypesNodeIteratorCallback, &handle);
    if (retval != UA_STATUSCODE_GOOD) {
        out->statusCode = retval;
    }

    /* check if you can find the node with any of the subtypes of aggregates */
    UA_Boolean nodeFound = UA_FALSE;
    node_iteration *iter, *tmp_iter;
    LIST_FOREACH_SAFE(iter, &list, listEntry, tmp_iter) {
        if (!nodeFound) {
            UA_RelativePathElement rpe;
            UA_RelativePathElement_init(&rpe);
            rpe.referenceTypeId = *iter->node;
            rpe.isInverse = false;
            rpe.includeSubtypes = false;
            rpe.targetName = *name;
            /* TODO: test larger browsepath perhaps put browsepath in a loop */
            UA_BrowsePath bp;
            UA_BrowsePath_init(&bp);
            bp.relativePath.elementsSize = relativePathSize;
            bp.startingNode = *event;
            bp.relativePath.elements = &rpe;

            *out = UA_Server_translateBrowsePathToNodeIds(server, &bp);
            if (out->statusCode == UA_STATUSCODE_GOOD) {
                nodeFound = UA_TRUE;
            }
        }
        LIST_REMOVE(iter, listEntry);
        UA_NodeId_delete(iter->node);
        UA_free(iter);
    }
}

UA_StatusCode UA_Server_createEvent(UA_Server *server, UA_NodeId eventType, UA_NodeId *outNodeId) {
    UA_StatusCode retval;

    /* make sure the eventType is a subtype of BaseEventType */
    UA_NodeId hasSubtypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE);
    UA_NodeId baseEventTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE);
    if (!isNodeInTree(&server->config.nodestore, &eventType, &baseEventTypeId, &hasSubtypeId, 1)) {
        UA_LOG_ERROR(server->config.logger, UA_LOGCATEGORY_USERLAND, "Event type must be a subtype of BaseEventType!");
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    }

    /* set the eventId attribute */
    UA_ByteString eventId;
    UA_ByteString_init(&eventId);
    retval = UA_Event_generateEventId(server, &eventId);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_ByteString_delete(&eventId);
        return retval;
    }

    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    oAttr.displayName.locale = UA_STRING("en-US");
    oAttr.displayName.text = eventId;
    oAttr.description = UA_LOCALIZEDTEXT("en-US", "An event in its most basic form.");

    UA_QualifiedName name;
    UA_QualifiedName_init(&name);

    /* create an ObjectNode which represents the event */
    retval = UA_Server_addObjectNode(server,
                                     UA_NODEID_NULL, /* the user may not have control over the nodeId */
                                     UA_NODEID_NULL, /* an event does not have a parent */
                                     UA_NODEID_NULL, /* an event does not have any references */
                                     name,           /* an event does not have a name */
                                     eventType,      /* the type of the event */
                                     oAttr,          /* default attributes are fine */
                                     NULL,           /* no node context */
                                     outNodeId);

    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(server->config.logger, UA_LOGCATEGORY_USERLAND,
                     "Adding event failed. StatusCode %s", UA_StatusCode_name(retval));
        return retval;
    }

    /* find the eventId VariableNode */
    UA_BrowsePathResult bpr;
    UA_BrowsePathResult_init(&bpr);
    UA_QualifiedName findName = UA_QUALIFIEDNAME(0, "EventId");
    UA_Event_findVariableNode(server, &findName, 1, outNodeId, &bpr);
    if (bpr.statusCode != UA_STATUSCODE_GOOD) {
        return bpr.statusCode;
    }

    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setScalar(&value, &eventId, &UA_TYPES[UA_TYPES_BYTESTRING]);
    UA_Server_writeValue(server, bpr.targets[0].targetId.nodeId, value);

    UA_BrowsePathResult_deleteMembers(&bpr);

    /* find the eventType variableNode */
    findName = UA_QUALIFIEDNAME(0, "EventType");
    UA_Event_findVariableNode(server, &findName, 1, outNodeId, &bpr);
    if (bpr.statusCode != UA_STATUSCODE_GOOD || bpr.targetsSize < 1) {
        return bpr.statusCode;
    }
    UA_Variant_setScalar(&value, &eventType, &UA_TYPES[UA_TYPES_NODEID]);
    UA_Server_writeValue(server, bpr.targets[0].targetId.nodeId, value);
    UA_ByteString_deleteMembers(&eventId);
    UA_BrowsePathResult_deleteMembers(&bpr);

    /* the object is not put in any queues until it is triggered */
    return retval;
}

static UA_Boolean isValidEvent(UA_Server *server, UA_NodeId *validEventParent, UA_NodeId *eventId) {
    /* find the eventType variableNode */
    UA_BrowsePathResult bpr;
    UA_BrowsePathResult_init(&bpr);
    UA_QualifiedName findName = UA_QUALIFIEDNAME(0, "EventType");
    UA_Event_findVariableNode(server, &findName, 1, eventId, &bpr);
    if (bpr.statusCode != UA_STATUSCODE_GOOD || bpr.targetsSize < 1) {
        return UA_FALSE;
    }
    UA_NodeId hasSubtypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE);
    UA_Boolean tmp = isNodeInTree(&server->config.nodestore, &bpr.targets[0].targetId.nodeId, validEventParent,
                                  &hasSubtypeId, 1);
    UA_BrowsePathResult_deleteMembers(&bpr);
    return tmp;
}


static UA_StatusCode whereClausesApply(UA_Server *server, const UA_ContentFilter whereClause, UA_EventFieldList *efl,
                                       UA_Boolean *result) {
    /* if the where clauses aren't specified leave everything as is */
    if (whereClause.elementsSize == 0) {
        *result = UA_TRUE;
        return UA_STATUSCODE_GOOD;
    }
    /* where clauses were specified */
    UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_USERLAND, "Where clauses are not supported by the server.");
    *result = UA_TRUE;
    return UA_STATUSCODE_BADNOTSUPPORTED;
}

UA_StatusCode UA_Server_filterEvent(UA_Server *server, UA_NodeId *eventNode, UA_EventFilter *filter,
                                    UA_EventNotification *notification) {
    if (filter->selectClausesSize == 0) {
        return UA_STATUSCODE_BADEVENTFILTERINVALID;
    }
    UA_StatusCode retval;
    /* setup */
    notification->fields = UA_EventFieldList_new();
    if (!notification->fields) {
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    UA_EventFieldList_init(notification->fields);

    /* EventFilterResult isn't being used currently
    notification->result = UA_EventFilterResult_new();
    if (!notification->result) {
        UA_free(notification->fields);
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    UA_EventFilterResult_init(notification->result); */

    notification->fields->eventFieldsSize = filter->selectClausesSize;
    notification->fields->eventFields = (UA_Variant *) UA_Array_new(notification->fields->eventFieldsSize,
                                                                    &UA_TYPES[UA_TYPES_VARIANT]);
    if (!notification->fields->eventFields) {
        UA_free(notification->fields);
        /* EventFilterResult currently isn't being used
        UA_free(notification->result); */
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    /* EventFilterResult currently isn't being used
    notification->result->selectClauseResultsSize = filter->selectClausesSize;
    notification->result->selectClauseResults = (UA_StatusCode *) UA_Array_new(filter->selectClausesSize,
                                                                               &UA_TYPES[UA_TYPES_VARIANT]);
    if (!notification->result->selectClauseResults) {
        UA_free(notification->fields->eventFields);
        UA_free(notification->fields);
        UA_free(notification->result);
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    */

    /* ================ apply the filter ===================== */
    /* check if the browsePath is BaseEventType, in which case nothing more needs to be checked */
    UA_NodeId baseEventTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE);
    /* iterate over the selectClauses */
    for (size_t i = 0; i < filter->selectClausesSize; i++) {
        if (!UA_NodeId_equal(&filter->selectClauses[i].typeDefinitionId, &baseEventTypeId)
            && !isValidEvent(server, &filter->selectClauses[0].typeDefinitionId, eventNode)) {
            UA_Variant_init(&notification->fields->eventFields[i]);
            /* EventFilterResult currently isn't being used
            notification->result->selectClauseResults[i] = UA_STATUSCODE_BADTYPEDEFINITIONINVALID; */
            continue;
        }
        /* type is correct */
        /* find the variable node with the data being looked for */
        UA_BrowsePathResult bpr;
        UA_BrowsePathResult_init(&bpr);
        UA_Event_findVariableNode(server, filter->selectClauses[i].browsePath, filter->selectClauses[i].browsePathSize,
                                  eventNode, &bpr);
        if (bpr.statusCode != UA_STATUSCODE_GOOD || bpr.targetsSize < 1) {
            UA_Variant_init(&notification->fields->eventFields[i]);
            continue;
        }
        /* copy the value */
        UA_Boolean whereClauseResult = UA_TRUE;
        UA_Boolean whereClausesUsed = UA_FALSE;     /* placeholder until whereClauses are implemented */
        retval = whereClausesApply(server, filter->whereClause, notification->fields, &whereClauseResult);
        if (retval == UA_STATUSCODE_BADNOTSUPPORTED) {
            whereClausesUsed = UA_TRUE;
        }
        if (whereClauseResult) {
            retval = UA_Server_readValue(server, bpr.targets[0].targetId.nodeId, &notification->fields->eventFields[i]);
            if (retval != UA_STATUSCODE_GOOD) {
                UA_Variant_init(&notification->fields->eventFields[i]);
            }
            if (whereClausesUsed) {
                return UA_STATUSCODE_BADNOTSUPPORTED;
            }
        } else {
            UA_Variant_init(&notification->fields->eventFields[i]);
            /* TODO: better statuscode for failing at where clauses */
            /* EventFilterResult currently isn't being used
            notification->result->selectClauseResults[i] = UA_STATUSCODE_BADDATAUNAVAILABLE; */
        }
        UA_BrowsePathResult_deleteMembers(&bpr);
    }
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode eventSetConstants(UA_Server *server, UA_NodeId *event, UA_NodeId *origin) {
    UA_BrowsePathResult bpr;
    UA_BrowsePathResult_init(&bpr);
    /* set the source */
    UA_QualifiedName name = UA_QUALIFIEDNAME(0, "SourceNode");
    UA_Event_findVariableNode(server, &name, 1, event, &bpr);
    if (bpr.statusCode != UA_STATUSCODE_GOOD || bpr.targetsSize < 1) {
        UA_StatusCode tmp = bpr.statusCode;
        UA_BrowsePathResult_deleteMembers(&bpr);
        return tmp;
    }
    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setScalar(&value, origin, &UA_TYPES[UA_TYPES_NODEID]);
    UA_Server_writeValue(server, bpr.targets[0].targetId.nodeId, value);
    UA_BrowsePathResult_deleteMembers(&bpr);

    /* set the receive time */
    name = UA_QUALIFIEDNAME(0, "ReceiveTime");
    UA_Event_findVariableNode(server, &name, 1, event, &bpr);
    if (bpr.statusCode != UA_STATUSCODE_GOOD || bpr.targetsSize < 1) {
        UA_StatusCode tmp = bpr.statusCode;
        UA_BrowsePathResult_deleteMembers(&bpr);
        return tmp;
    }
    UA_DateTime time = UA_DateTime_now();
    UA_Variant_setScalar(&value, &time, &UA_TYPES[UA_TYPES_DATETIME]);
    UA_Server_writeValue(server, bpr.targets[0].targetId.nodeId, value);

    UA_BrowsePathResult_deleteMembers(&bpr);
    return UA_STATUSCODE_GOOD;
}


/* insert each node into the list (passed as handle) */
static UA_StatusCode getParentsNodeIteratorCallback(UA_NodeId parentId, UA_Boolean isInverse,
                                                    UA_NodeId referenceTypeId, void *handle) {
    /* parents have an inverse reference */
    if (!isInverse) {
        return UA_STATUSCODE_GOOD;
    }

    node_iteration *entry = (node_iteration *) UA_malloc(sizeof(node_iteration));
    if (!entry) {
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    entry->node = UA_NodeId_new();
    if (!entry->node) {
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    UA_NodeId_copy(&parentId, entry->node);
    LIST_INSERT_HEAD(((struct getNodesHandle *) handle)->nodes, entry, listEntry);

    /* recursion */
    UA_Server_forEachChildNodeCall(((struct getNodesHandle *) handle)->server,
                                   parentId, getParentsNodeIteratorCallback, handle);
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode UA_Event_addEventToMonitoredItem(UA_Server *server, UA_NodeId *event, UA_MonitoredItem *mon) {
    UA_Notification *notification = (UA_Notification *) UA_malloc(sizeof(UA_Notification));
    if (!notification) {
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    /* apply the filter */
    notification->data.event = (UA_EventNotification *) UA_malloc(sizeof(UA_EventNotification));
    if (!notification->data.event) {
        UA_free(notification);
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    UA_StatusCode retval = UA_Server_filterEvent(server, event, mon->filter.eventFilter, notification->data.event);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_free(notification);
        UA_free(notification->data.event);
        return retval;
    }
    notification->mon = mon;

    /* add to the monitored item queue */
    MonitoredItem_ensureQueueSpace(server, mon);
    TAILQ_INSERT_TAIL(&mon->queue, notification, listEntry);
    ++mon->queueSize;
    /* add to the subscription queue */
    TAILQ_INSERT_TAIL(&mon->subscription->notificationQueue, notification, globalEntry);
    ++mon->subscription->notificationQueueSize;
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode UA_Server_triggerEvent(UA_Server *server, UA_NodeId *event, UA_NodeId *origin, UA_ByteString *outId) {
    /* make sure the origin is in the ObjectsFolder (TODO: or in the ViewsFolder) */
    UA_NodeId objectsFolderId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId references[2] = {
            {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_ORGANIZES}},
            {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_HASCOMPONENT}}
    };
    if (!isNodeInTree(&server->config.nodestore, origin, &objectsFolderId, references, 2)) {
        UA_LOG_ERROR(server->config.logger, UA_LOGCATEGORY_USERLAND, "Node for event must be in ObjectsFolder!");
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    }

    UA_StatusCode retval = eventSetConstants(server, event, origin);
    if (retval != UA_STATUSCODE_GOOD) {
        return retval;
    }

    /* get an array with all parents */
    struct getNodesHandle handle;
    nodeList list;
    LIST_INIT(&list);
    handle.server = server;
    handle.nodes = &list;
    retval = getParentsNodeIteratorCallback(*origin, UA_TRUE, UA_NODEID_NULL, &handle);
    if (retval != UA_STATUSCODE_GOOD) {
        return retval;
    }

    /* add the event to each node's monitored items */
    node_iteration *parentIter, *tmp_parentIter;
    LIST_FOREACH_SAFE(parentIter, &list, listEntry, tmp_parentIter) {
        UA_MonitoredItemQueueEntry *monIter;
        const UA_ObjectNode *node = (const UA_ObjectNode *) UA_Nodestore_get(server, parentIter->node);
        LIST_FOREACH(monIter, &node->monitoredItemQueue, listEntry) {
            retval = UA_Event_addEventToMonitoredItem(server, event, monIter->mon);
            if (retval != UA_STATUSCODE_GOOD) {
                UA_Nodestore_release(server, (const UA_Node *) node);
                return retval;
            }
        }
        UA_Nodestore_release(server, (const UA_Node *) node);
        LIST_REMOVE(parentIter, listEntry);
        UA_NodeId_delete(parentIter->node);
        UA_free(parentIter);
    }

    /* get the eventId */
    retval = UA_Server_getEventId(server, event, outId);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_SERVER,
                       "getEventId failed. StatusCode %s", UA_StatusCode_name(retval));
        return retval;
    }

    /* delete the node representation of the event */
    retval = UA_Server_deleteNode(server, *event, UA_TRUE);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Attempt to remove event using deleteNode failed. StatusCode %s", UA_StatusCode_name(retval));
        return retval;
    }
    return UA_STATUSCODE_GOOD;
}

#ifdef UA_DEBUG_EVENTS
void UA_Event_generateExampleEvent(UA_Server *server) {
    UA_StatusCode retval;
    UA_NodeId outId;
    UA_NodeId_init(&outId);
    UA_NodeId eventType = UA_NODEID_NUMERIC(0, UA_NS0ID_TRANSITIONEVENTTYPE);
    retval = UA_Server_createEvent(server, eventType, &outId);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_SERVER,
                       "createEvent failed. StatusCode %s", UA_StatusCode_name(retval));
        return;
    }

    UA_Variant value;
    UA_Variant_init(&value);
    UA_RelativePathElement rpe;
    UA_RelativePathElement_init(&rpe);
    rpe.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
    rpe.isInverse = false;
    rpe.includeSubtypes = false;

    UA_BrowsePath bp;
    UA_BrowsePath_init(&bp);
    bp.startingNode = outId;
    bp.relativePath.elementsSize = 1;
    bp.relativePath.elements = &rpe;

    /* severity */
    rpe.targetName = UA_QUALIFIEDNAME(0, "Severity");
    UA_BrowsePathResult bpr = UA_Server_translateBrowsePathToNodeIds(server, &bp);
    if (bpr.statusCode != UA_STATUSCODE_GOOD || bpr.targetsSize < 1) {
        UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_USERLAND, "Event is missing severity attribute.\n");
        return;
    }
    UA_UInt16 eventSeverity = 100;
    UA_Variant_setScalar(&value, &eventSeverity, &UA_TYPES[UA_TYPES_UINT16]);
    UA_Server_writeValue(server, bpr.targets[0].targetId.nodeId, value);

    UA_BrowsePathResult_deleteMembers(&bpr);

    /* message */
    rpe.targetName = UA_QUALIFIEDNAME(0, "Message");
    bpr = UA_Server_translateBrowsePathToNodeIds(server, &bp);
    if (bpr.statusCode != UA_STATUSCODE_GOOD || bpr.targetsSize < 1) {
        UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_USERLAND, "Event is missing message attribute.\n");
        return;
    }
    UA_LocalizedText eventMessage = UA_LOCALIZEDTEXT("en-US", "An event has been generated.");
    UA_Variant_setScalar(&value, &eventMessage, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
    UA_Server_writeValue(server, bpr.targets[0].targetId.nodeId, value);

    UA_BrowsePathResult_deleteMembers(&bpr);
    UA_NodeId serverId = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER);
    UA_ByteString id;
    UA_Server_triggerEvent(server, &outId, &serverId, &id);
    UA_ByteString_deleteMembers(&id);
    UA_LOG_INFO(server->config.logger, UA_LOGCATEGORY_SERVER, "Event generated.");
}
#endif

#endif
