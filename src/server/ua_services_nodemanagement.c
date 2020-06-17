/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2014-2017 (c) Fraunhofer IOSB (Author: Julius Pfrommer)
 *    Copyright 2014-2017 (c) Florian Palm
 *    Copyright 2015-2016 (c) Sten Grüner
 *    Copyright 2015-2016 (c) Chris Iatrou
 *    Copyright 2015-2016 (c) Oleksiy Vasylyev
 *    Copyright 2017 (c) Julian Grothoff
 *    Copyright 2016 (c) LEvertz
 *    Copyright 2016 (c) Lorenz Haas
 *    Copyright 2017 (c) frax2222
 *    Copyright 2017-2018 (c) Stefan Profanter, fortiss GmbH
 *    Copyright 2017 (c) Christian von Arnim
 *    Copyright 2017 (c) Henrik Norrman
 */

#include "ua_server_internal.h"
#include "ua_services.h"

/*********************/
/* Edit Node Context */
/*********************/

UA_StatusCode
UA_Server_getNodeContext(UA_Server *server, UA_NodeId nodeId,
                         void **nodeContext) {
    UA_LOCK(server->serviceMutex);
    UA_StatusCode retval = getNodeContext(server, nodeId, nodeContext);
    UA_UNLOCK(server->serviceMutex);
    return retval;
}

UA_StatusCode
getNodeContext(UA_Server *server, UA_NodeId nodeId,
                         void **nodeContext) {
    const UA_Node *node = UA_NODESTORE_GET(server, &nodeId);
    if(!node)
        return UA_STATUSCODE_BADNODEIDUNKNOWN;
    *nodeContext = node->head.context;
    UA_NODESTORE_RELEASE(server, node);
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
setDeconstructedNode(UA_Server *server, UA_Session *session,
                     UA_NodeHead *head, void *context) {
    head->constructed = false;
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
setConstructedNodeContext(UA_Server *server, UA_Session *session,
                          UA_NodeHead *head, void *context) {
    head->context = context;
    head->constructed = true;
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
editNodeContext(UA_Server *server, UA_Session* session,
                UA_NodeHead *head, void *context) {
    head->context = context;
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_Server_setNodeContext(UA_Server *server, UA_NodeId nodeId,
                         void *nodeContext) {
    UA_LOCK(server->serviceMutex);
    UA_StatusCode retval = UA_Server_editNode(server, &server->adminSession, &nodeId,
                              (UA_EditNodeCallback)editNodeContext, nodeContext);
    UA_UNLOCK(server->serviceMutex);
    return retval;
}

/**********************/
/* Consistency Checks */
/**********************/

#define UA_PARENT_REFERENCES_COUNT 2

const UA_NodeId parentReferences[UA_PARENT_REFERENCES_COUNT] = {
    {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_HASSUBTYPE}},
    {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_HASCOMPONENT}}
};

/* Check if the requested parent node exists, has the right node class and is
 * referenced with an allowed (hierarchical) reference type. For "type" nodes,
 * only hasSubType references are allowed. */
static UA_StatusCode
checkParentReference(UA_Server *server, UA_Session *session, UA_NodeClass nodeClass,
                     const UA_NodeId *parentNodeId, const UA_NodeId *referenceTypeId) {
    /* Objects do not need a parent (e.g. mandatory/optional modellingrules) */
    /* Also, there are some variables which do not have parents, e.g. EnumStrings, EnumValues */
    if((nodeClass == UA_NODECLASS_OBJECT || nodeClass == UA_NODECLASS_VARIABLE) &&
       UA_NodeId_isNull(parentNodeId) && UA_NodeId_isNull(referenceTypeId))
        return UA_STATUSCODE_GOOD;

    /* See if the parent exists */
    const UA_Node *parent = UA_NODESTORE_GET(server, parentNodeId);
    if(!parent) {
        UA_LOG_NODEID_WRAP(parentNodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                            "AddNodes: Parent node %.*s not found",
                            (int)nodeIdStr.length, nodeIdStr.data));
        return UA_STATUSCODE_BADPARENTNODEIDINVALID;
    }

    UA_NodeClass parentNodeClass = parent->head.nodeClass;
    UA_NODESTORE_RELEASE(server, parent);

    /* Check the referencetype exists */
    const UA_Node *referenceType = UA_NODESTORE_GET(server, referenceTypeId);
    if(!referenceType) {
        UA_LOG_NODEID_WRAP(referenceTypeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                           "AddNodes: Reference type %.*s to the parent not found",
                           (int)nodeIdStr.length, nodeIdStr.data));
        return UA_STATUSCODE_BADREFERENCETYPEIDINVALID;
    }

    /* Check if the referencetype is a reference type node */
    if(referenceType->head.nodeClass != UA_NODECLASS_REFERENCETYPE) {
        UA_LOG_NODEID_WRAP(referenceTypeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                           "AddNodes: Reference type %.*s to the parent is not a ReferenceTypeNode",
                           (int)nodeIdStr.length, nodeIdStr.data));
        UA_NODESTORE_RELEASE(server, referenceType);
        return UA_STATUSCODE_BADREFERENCETYPEIDINVALID;
    }

    /* Check that the reference type is not abstract */
    UA_Boolean referenceTypeIsAbstract = referenceType->referenceTypeNode.isAbstract;
    UA_NODESTORE_RELEASE(server, referenceType);
    if(referenceTypeIsAbstract == true) {
        UA_LOG_NODEID_WRAP(referenceTypeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                           "AddNodes: Abstract reference type %.*s to the parent not allowed",
                           (int)nodeIdStr.length, nodeIdStr.data));
        return UA_STATUSCODE_BADREFERENCENOTALLOWED;
    }

    /* Check hassubtype relation for type nodes */
    if(nodeClass == UA_NODECLASS_DATATYPE ||
       nodeClass == UA_NODECLASS_VARIABLETYPE ||
       nodeClass == UA_NODECLASS_OBJECTTYPE ||
       nodeClass == UA_NODECLASS_REFERENCETYPE) {
        /* type needs hassubtype reference to the supertype */
        if(referenceType->referenceTypeNode.referenceTypeIndex != UA_REFERENCETYPEINDEX_HASSUBTYPE) {
            UA_LOG_INFO_SESSION(&server->config.logger, session,
                                "AddNodes: Type nodes need to have a HasSubType "
                                "reference to the parent");
            return UA_STATUSCODE_BADREFERENCENOTALLOWED;
        }
        /* supertype needs to be of the same node type  */
        if(parentNodeClass != nodeClass) {
            UA_LOG_INFO_SESSION(&server->config.logger, session,
                                "AddNodes: Type nodes needs to be of the same node "
                                "type as their parent");
            return UA_STATUSCODE_BADPARENTNODEIDINVALID;
        }
        return UA_STATUSCODE_GOOD;
    }

    /* Test if the referencetype is hierarchical */
    const UA_NodeId hierarchRefs = UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
    if(!isNodeInTree_singleRef(server, referenceTypeId, &hierarchRefs,
                               UA_REFERENCETYPEINDEX_HASSUBTYPE)) {
        UA_LOG_INFO_SESSION(&server->config.logger, session,
                            "AddNodes: Reference type to the parent is not hierarchical");
        return UA_STATUSCODE_BADREFERENCETYPEIDINVALID;
    }

    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
typeCheckVariableNode(UA_Server *server, UA_Session *session,
                      const UA_VariableNode *node,
                      const UA_VariableTypeNode *vt) {
    /* The value might come from a datasource, so we perform a
     * regular read. */
    UA_DataValue value;
    UA_DataValue_init(&value);
    UA_StatusCode retval = readValueAttribute(server, session, node, &value);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    UA_NodeId baseDataType = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATATYPE);

    /* Check the datatype against the vt */
    /* If the node does not have any value and the dataType is BaseDataType,
     * then it's also fine. This is the default for empty nodes. */
    if(!compatibleDataType(server, &node->dataType, &vt->dataType, false) &&
       (value.hasValue || !UA_NodeId_equal(&node->dataType, &baseDataType))) {
        UA_LOG_NODEID_WRAP(&node->head.nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                              "AddNodes: The value of %.*s is incompatible with "
                              "the datatype of the VariableType",
                              (int)nodeIdStr.length, nodeIdStr.data));
        UA_DataValue_clear(&value);
        return UA_STATUSCODE_BADTYPEMISMATCH;
    }

    /* Check valueRank against array dimensions */
    if(!compatibleValueRankArrayDimensions(server, session, node->valueRank,
                                           node->arrayDimensionsSize)) {
        UA_LOG_NODEID_WRAP(&node->head.nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                           "AddNodes: The value rank of %.*s is incompatible "
                           "with its array dimensions", (int)nodeIdStr.length, nodeIdStr.data));
        UA_DataValue_clear(&value);
        return UA_STATUSCODE_BADTYPEMISMATCH;
    }

    /* Check valueRank against the vt */
    if(!compatibleValueRanks(node->valueRank, vt->valueRank)) {
        UA_LOG_NODEID_WRAP(&node->head.nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                           "AddNodes: The value rank of %.*s is incompatible "
                           "with the value rank of the VariableType",
                           (int)nodeIdStr.length, nodeIdStr.data));
        UA_DataValue_clear(&value);
        return UA_STATUSCODE_BADTYPEMISMATCH;
    }

    /* Check array dimensions against the vt */
    if(!compatibleArrayDimensions(vt->arrayDimensionsSize, vt->arrayDimensions,
                                  node->arrayDimensionsSize, node->arrayDimensions)) {
        UA_LOG_NODEID_WRAP(&node->head.nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                           "AddNodes: The array dimensions of %.*s are "
                           "incompatible with the array dimensions of the VariableType",
                           (int)nodeIdStr.length, nodeIdStr.data));
        UA_DataValue_clear(&value);
        return UA_STATUSCODE_BADTYPEMISMATCH;
    }

    /* Typecheck the value */
    if(value.hasValue && value.value.data) {
        /* If the type-check failed write the same value again. The
         * write-service tries to convert to the correct type... */
        if(!compatibleValue(server, session, &node->dataType, node->valueRank,
                            node->arrayDimensionsSize, node->arrayDimensions,
                            &value.value, NULL)) {
            retval = writeWithWriteValue(server, &node->head.nodeId, UA_ATTRIBUTEID_VALUE, &UA_TYPES[UA_TYPES_VARIANT], &value.value);
        }

        UA_DataValue_clear(&value);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_NODEID_WRAP(&node->head.nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                               "AddNodes: The value of %.*s is incompatible with the "
                               "variable definition", (int)nodeIdStr.length, nodeIdStr.data));
        }
    }

    return retval;
}

/********************/
/* Instantiate Node */
/********************/

static const UA_NodeId baseDataVariableType =
    {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_BASEDATAVARIABLETYPE}};
static const UA_NodeId baseObjectType =
    {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_BASEOBJECTTYPE}};
static const UA_NodeId hasTypeDefinition =
    {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_HASTYPEDEFINITION}};

/* Use attributes from the variable type wherever required. Reload the node if
 * changes were made. */
static UA_StatusCode
useVariableTypeAttributes(UA_Server *server, UA_Session *session,
                          const UA_VariableNode **node_ptr,
                          const UA_VariableTypeNode *vt) {
    const UA_VariableNode *node = *node_ptr;
    UA_Boolean modified = false;

    /* If no value is set, see if the vt provides one and copy it. This needs to
     * be done before copying the datatype from the vt, as setting the datatype
     * triggers a typecheck. */
    UA_DataValue orig;
    UA_DataValue_init(&orig);
    UA_StatusCode retval = readValueAttribute(server, session, node, &orig);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    if(orig.value.type) {
        /* A value is present */
        UA_DataValue_clear(&orig);
    } else {
        UA_WriteValue v;
        UA_WriteValue_init(&v);
        retval = readValueAttribute(server, session, (const UA_VariableNode*)vt, &v.value);
        if(retval == UA_STATUSCODE_GOOD && v.value.hasValue) {
            v.nodeId = node->head.nodeId;
            v.attributeId = UA_ATTRIBUTEID_VALUE;
            retval = writeWithSession(server, session, &v);
            modified = true;
        }
        UA_DataValue_clear(&v.value);
        if(retval != UA_STATUSCODE_GOOD)
            return retval;
    }

    /* If no datatype is given, use the datatype of the vt */
    if(UA_NodeId_isNull(&node->dataType)) {
        UA_LOG_INFO_SESSION(&server->config.logger, session, "AddNodes: "
                            "No datatype given; Copy the datatype attribute "
                            "from the TypeDefinition");
        UA_WriteValue v;
        UA_WriteValue_init(&v);
        v.nodeId = node->head.nodeId;
        v.attributeId = UA_ATTRIBUTEID_DATATYPE;
        v.value.hasValue = true;
        UA_Variant_setScalar(&v.value.value, (void*)(uintptr_t)&vt->dataType,
                             &UA_TYPES[UA_TYPES_NODEID]);
        retval = writeWithSession(server, session, &v);
        modified = true;
        if(retval != UA_STATUSCODE_GOOD)
            return retval;
    }

    /* Use the ArrayDimensions of the vt */
    if(node->arrayDimensionsSize == 0 && vt->arrayDimensionsSize > 0) {
        UA_WriteValue v;
        UA_WriteValue_init(&v);
        v.nodeId = node->head.nodeId;
        v.attributeId = UA_ATTRIBUTEID_ARRAYDIMENSIONS;
        v.value.hasValue = true;
        UA_Variant_setArray(&v.value.value, vt->arrayDimensions,
                            vt->arrayDimensionsSize, &UA_TYPES[UA_TYPES_UINT32]);
        retval = writeWithSession(server, session, &v);
        modified = true;
        if(retval != UA_STATUSCODE_GOOD)
            return retval;
    }

    /* If the node was modified, update the pointer to the new version */
    if(modified) {
        const UA_Node *updated = UA_NODESTORE_GET(server, &node->head.nodeId);
        if(!updated)
            return UA_STATUSCODE_BADINTERNALERROR;
        UA_NODESTORE_RELEASE(server, (const UA_Node*)node);
        *node_ptr = (const UA_VariableNode*)updated;
    }

    return UA_STATUSCODE_GOOD;
}

/* Search for an instance of "browseName" in node searchInstance. Used during
 * copyChildNodes to find overwritable/mergable nodes. Does not touch
 * outInstanceNodeId if no child is found. */
static UA_StatusCode
findChildByBrowsename(UA_Server *server, UA_Session *session,
                      const UA_NodeId *searchInstance,
                      const UA_QualifiedName *browseName,
                      UA_NodeId *outInstanceNodeId) {
    UA_BrowseDescription bd;
    UA_BrowseDescription_init(&bd);
    bd.nodeId = *searchInstance;
    bd.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_AGGREGATES);
    bd.includeSubtypes = true;
    bd.browseDirection = UA_BROWSEDIRECTION_FORWARD;
    bd.nodeClassMask = UA_NODECLASS_OBJECT | UA_NODECLASS_VARIABLE | UA_NODECLASS_METHOD;
    bd.resultMask = UA_BROWSERESULTMASK_BROWSENAME;

    UA_BrowseResult br;
    UA_BrowseResult_init(&br);
    UA_UInt32 maxrefs = 0;
    Operation_Browse(server, session, &maxrefs, &bd, &br);
    if(br.statusCode != UA_STATUSCODE_GOOD)
        return br.statusCode;

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    for(size_t i = 0; i < br.referencesSize; ++i) {
        UA_ReferenceDescription *rd = &br.references[i];
        if(rd->browseName.namespaceIndex == browseName->namespaceIndex &&
           UA_String_equal(&rd->browseName.name, &browseName->name)) {
            retval = UA_NodeId_copy(&rd->nodeId.nodeId, outInstanceNodeId);
            break;
        }
    }

    UA_BrowseResult_clear(&br);
    return retval;
}

static const UA_NodeId mandatoryId =
    {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_MODELLINGRULE_MANDATORY}};

static UA_Boolean
isMandatoryChild(UA_Server *server, UA_Session *session,
                 const UA_NodeId *childNodeId) {
    /* Get the child */
    const UA_Node *child = UA_NODESTORE_GET(server, childNodeId);
    if(!child)
        return false;

    /* Look for the reference making the child mandatory */
    for(size_t i = 0; i < child->head.referencesSize; ++i) {
        UA_NodeReferenceKind *refs = &child->head.references[i];
        if(refs->referenceTypeIndex != UA_REFERENCETYPEINDEX_HASMODELLINGRULE)
            continue;
        if(refs->isInverse)
            continue;
        for(size_t j = 0; j < refs->refTargetsSize; ++j) {
            if(UA_NodeId_equal(&mandatoryId, &refs->refTargets[j].targetId.nodeId)) {
                UA_NODESTORE_RELEASE(server, child);
                return true;
            }
        }
    }

    UA_NODESTORE_RELEASE(server, child);
    return false;
}

static UA_StatusCode
copyAllChildren(UA_Server *server, UA_Session *session,
                const UA_NodeId *source, const UA_NodeId *destination);

static UA_StatusCode
recursiveTypeCheckAddChildren(UA_Server *server, UA_Session *session,
                              const UA_Node **node, const UA_Node *type);

static void
Operation_addReference(UA_Server *server, UA_Session *session, void *context,
                       const UA_AddReferencesItem *item, UA_StatusCode *retval);

static UA_StatusCode
copyChild(UA_Server *server, UA_Session *session, const UA_NodeId *destinationNodeId,
          const UA_ReferenceDescription *rd) {
    /* Is there an existing child with the browsename? */
    UA_NodeId existingChild = UA_NODEID_NULL;
    UA_StatusCode retval = findChildByBrowsename(server, session, destinationNodeId,
                                                 &rd->browseName, &existingChild);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    /* Have a child with that browseName. Deep-copy missing members. */
    if(!UA_NodeId_isNull(&existingChild)) {
        if(rd->nodeClass == UA_NODECLASS_VARIABLE ||
           rd->nodeClass == UA_NODECLASS_OBJECT)
            retval = copyAllChildren(server, session, &rd->nodeId.nodeId, &existingChild);
        UA_NodeId_clear(&existingChild);
        return retval;
    }

    /* Is the child mandatory? If not, ask callback whether child should be instantiated.
     * If not, skip. */
    if(!isMandatoryChild(server, session, &rd->nodeId.nodeId)) {
        if(!server->config.nodeLifecycle.createOptionalChild)
            return UA_STATUSCODE_GOOD;

        UA_UNLOCK(server->serviceMutex);
        retval = server->config.nodeLifecycle.createOptionalChild(server,
                                                                 &session->sessionId,
                                                                 session->sessionHandle,
                                                                 &rd->nodeId.nodeId,
                                                                 destinationNodeId,
                                                                 &rd->referenceTypeId);
        UA_LOCK(server->serviceMutex);
        if(retval == UA_FALSE) {
            return UA_STATUSCODE_GOOD;
        }
    }

    /* Child is a method -> create a reference */
    if(rd->nodeClass == UA_NODECLASS_METHOD) {
        UA_AddReferencesItem newItem;
        UA_AddReferencesItem_init(&newItem);
        newItem.sourceNodeId = *destinationNodeId;
        newItem.referenceTypeId = rd->referenceTypeId;
        newItem.isForward = true;
        newItem.targetNodeId = rd->nodeId;
        newItem.targetNodeClass = UA_NODECLASS_METHOD;
        Operation_addReference(server, session, NULL, &newItem, &retval);
        return retval;
    }

    /* Child is a variable or object */
    if(rd->nodeClass == UA_NODECLASS_VARIABLE ||
       rd->nodeClass == UA_NODECLASS_OBJECT) {
        /* Make a copy of the node */
        UA_Node *node;
        retval = UA_NODESTORE_GETCOPY(server, &rd->nodeId.nodeId, &node);
        if(retval != UA_STATUSCODE_GOOD)
            return retval;

        /* Remove the context of the copied node */
        node->head.context = NULL;
        node->head.constructed = false;

        /* Reset the NodeId (random numeric id will be assigned in the nodestore) */
        UA_NodeId_clear(&node->head.nodeId);
        node->head.nodeId.namespaceIndex = destinationNodeId->namespaceIndex;

        if (server->config.nodeLifecycle.generateChildNodeId) {
            UA_UNLOCK(server->serviceMutex);
            retval = server->config.nodeLifecycle.generateChildNodeId(server,
                                                                      &session->sessionId, session->sessionHandle,
                                                                      &rd->nodeId.nodeId,
                                                                      destinationNodeId,
                                                                      &rd->referenceTypeId,
                                                                      &node->head.nodeId);
            UA_LOCK(server->serviceMutex);
            if(retval != UA_STATUSCODE_GOOD) {
                UA_NODESTORE_DELETE(server, node);
                return retval;
            }
        }

        /* Remove references, they are re-created from scratch in addnode_finish */
        /* TODO: Be more clever in removing references that are re-added during
         * addnode_finish. That way, we can call addnode_finish also on children that were
         * manually added by the user during addnode_begin and addnode_finish. */
        /* For now we keep all the modelling rule references and delete all others */
        UA_ReferenceTypeSet reftypes_modellingrule =
            UA_REFTYPESET(UA_REFERENCETYPEINDEX_HASMODELLINGRULE);
        UA_Node_deleteReferencesSubset(node, &reftypes_modellingrule);

        /* Add the node to the nodestore */
        UA_NodeId newNodeId;
        retval = UA_NODESTORE_INSERT(server, node, &newNodeId);
        if(retval != UA_STATUSCODE_GOOD)
            return retval;

        /* Add the node references */
        retval = AddNode_addRefs(server, session, &newNodeId, destinationNodeId,
                                 &rd->referenceTypeId, &rd->typeDefinition.nodeId);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_NODESTORE_REMOVE(server, &newNodeId);
            return retval;
        }

        /* For the new child, recursively copy the members of the original. No
         * typechecking is performed here. Assuming that the original is
         * consistent. */
        retval = copyAllChildren(server, session, &rd->nodeId.nodeId, &newNodeId);
    }

    return retval;
}

/* Copy any children of Node sourceNodeId to another node destinationNodeId. */
static UA_StatusCode
copyAllChildren(UA_Server *server, UA_Session *session,
                const UA_NodeId *source, const UA_NodeId *destination) {
    /* Browse to get all children of the source */
    UA_BrowseDescription bd;
    UA_BrowseDescription_init(&bd);
    bd.nodeId = *source;
    bd.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_AGGREGATES);
    bd.includeSubtypes = true;
    bd.browseDirection = UA_BROWSEDIRECTION_FORWARD;
    bd.nodeClassMask = UA_NODECLASS_OBJECT | UA_NODECLASS_VARIABLE | UA_NODECLASS_METHOD;
    bd.resultMask = UA_BROWSERESULTMASK_REFERENCETYPEID | UA_BROWSERESULTMASK_NODECLASS |
        UA_BROWSERESULTMASK_BROWSENAME | UA_BROWSERESULTMASK_TYPEDEFINITION;

    UA_BrowseResult br;
    UA_BrowseResult_init(&br);
    UA_UInt32 maxrefs = 0;
    Operation_Browse(server, session, &maxrefs, &bd, &br);
    if(br.statusCode != UA_STATUSCODE_GOOD)
        return br.statusCode;

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    for(size_t i = 0; i < br.referencesSize; ++i) {
        UA_ReferenceDescription *rd = &br.references[i];
        retval = copyChild(server, session, destination, rd);
        if(retval != UA_STATUSCODE_GOOD)
            return retval;
    }

    UA_BrowseResult_clear(&br);
    return retval;
}

static UA_StatusCode
addTypeChildren(UA_Server *server, UA_Session *session,
                const UA_NodeHead *head, const UA_NodeHead *typeHead) {
    /* Get the hierarchy of the type and all its supertypes */
    UA_NodeId *hierarchy = NULL;
    size_t hierarchySize = 0;
    UA_StatusCode retval = getParentTypeAndInterfaceHierarchy(server, &typeHead->nodeId,
                                                              &hierarchy, &hierarchySize);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;
    UA_assert(hierarchySize < 1000);

    /* Copy members of the type and supertypes (and instantiate them) */
    for(size_t i = 0; i < hierarchySize; ++i) {
        retval = copyAllChildren(server, session, &hierarchy[i], &head->nodeId);
        if(retval != UA_STATUSCODE_GOOD)
            break;
    }

    UA_Array_delete(hierarchy, hierarchySize, &UA_TYPES[UA_TYPES_NODEID]);
    return retval;
}

static UA_StatusCode
addRef(UA_Server *server, UA_Session *session, const UA_NodeId *nodeId,
       const UA_NodeId *referenceTypeId, const UA_NodeId *parentNodeId,
       UA_Boolean forward) {
    UA_AddReferencesItem ref_item;
    UA_AddReferencesItem_init(&ref_item);
    ref_item.sourceNodeId = *nodeId;
    ref_item.referenceTypeId = *referenceTypeId;
    ref_item.isForward = forward;
    ref_item.targetNodeId.nodeId = *parentNodeId;

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    Operation_addReference(server, session, NULL, &ref_item, &retval);
    return retval;
}

/************/
/* Add Node */
/************/

static const UA_NodeId hasSubtype = {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_HASSUBTYPE}};

UA_StatusCode
AddNode_addRefs(UA_Server *server, UA_Session *session, const UA_NodeId *nodeId,
                const UA_NodeId *parentNodeId, const UA_NodeId *referenceTypeId,
                const UA_NodeId *typeDefinitionId) {
    /* Get the node */
    const UA_Node *type = NULL;
    const UA_Node *node = UA_NODESTORE_GET(server, nodeId);
    if(!node)
        return UA_STATUSCODE_BADNODEIDUNKNOWN;

    /* Use the typeDefinition as parent for type-nodes */
    const UA_NodeHead *head = &node->head;
    if(head->nodeClass == UA_NODECLASS_VARIABLETYPE ||
       head->nodeClass == UA_NODECLASS_OBJECTTYPE ||
       head->nodeClass == UA_NODECLASS_REFERENCETYPE ||
       head->nodeClass == UA_NODECLASS_DATATYPE) {
        if(UA_NodeId_equal(referenceTypeId, &UA_NODEID_NULL))
            referenceTypeId = &hasSubtype;
        const UA_Node *parentNode = UA_NODESTORE_GET(server, parentNodeId);
        if(parentNode) {
            if(parentNode->head.nodeClass == head->nodeClass)
                typeDefinitionId = parentNodeId;
            UA_NODESTORE_RELEASE(server, parentNode);
        }
    }

    UA_StatusCode retval;
    /* Make sure newly created node does not have itself as parent */
    if (UA_NodeId_equal(nodeId, parentNodeId)) {
        UA_LOG_NODEID_WRAP(nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                                                       "AddNodes: The node %.*s can not have "
                                                       "itself as parent",
                                                       (int)nodeIdStr.length, nodeIdStr.data));
        retval = UA_STATUSCODE_BADINVALIDARGUMENT;
        goto cleanup;
    }


    /* Check parent reference. Objects may have no parent. */
    retval = checkParentReference(server, session, head->nodeClass,
                                  parentNodeId, referenceTypeId);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_NODEID_WRAP(nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                            "AddNodes: The parent reference for %.*s is invalid "
                            "with status code %s",
                            (int)nodeIdStr.length, nodeIdStr.data,
                            UA_StatusCode_name(retval)));
        goto cleanup;
    }

    /* Replace empty typeDefinition with the most permissive default */
    if((head->nodeClass == UA_NODECLASS_VARIABLE ||
        head->nodeClass == UA_NODECLASS_OBJECT) &&
       UA_NodeId_isNull(typeDefinitionId)) {
        UA_LOG_NODEID_WRAP(nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                            "AddNodes: No TypeDefinition for %.*s; Use the default "
                            "TypeDefinition for the Variable/Object",
                            (int)nodeIdStr.length, nodeIdStr.data));
        if(head->nodeClass == UA_NODECLASS_VARIABLE)
            typeDefinitionId = &baseDataVariableType;
        else
            typeDefinitionId = &baseObjectType;
    }

    /* Get the node type. There must be a typedefinition for variables, objects
     * and type-nodes. See the above checks. */
    if(!UA_NodeId_isNull(typeDefinitionId)) {
        /* Get the type node */
        type = UA_NODESTORE_GET(server, typeDefinitionId);
        if(!type) {
            UA_LOG_NODEID_WRAP(typeDefinitionId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                                "AddNodes: Node type %.*s not found",
                                (int)nodeIdStr.length, nodeIdStr.data));
            retval = UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
            goto cleanup;
        }

        UA_Boolean typeOk = false;
        const UA_NodeHead *typeHead = &type->head;
        switch(head->nodeClass) {
            case UA_NODECLASS_DATATYPE:
                typeOk = typeHead->nodeClass == UA_NODECLASS_DATATYPE;
                break;
            case UA_NODECLASS_METHOD:
                typeOk = typeHead->nodeClass == UA_NODECLASS_METHOD;
                break;
            case UA_NODECLASS_OBJECT:
                typeOk = typeHead->nodeClass == UA_NODECLASS_OBJECTTYPE;
                break;
            case UA_NODECLASS_OBJECTTYPE:
                typeOk = typeHead->nodeClass == UA_NODECLASS_OBJECTTYPE;
                break;
            case UA_NODECLASS_REFERENCETYPE:
                typeOk = typeHead->nodeClass == UA_NODECLASS_REFERENCETYPE;
                break;
            case UA_NODECLASS_VARIABLE:
                typeOk = typeHead->nodeClass == UA_NODECLASS_VARIABLETYPE;
                break;
            case UA_NODECLASS_VARIABLETYPE:
                typeOk = typeHead->nodeClass == UA_NODECLASS_VARIABLETYPE;
                break;
            case UA_NODECLASS_VIEW:
                typeOk = typeHead->nodeClass == UA_NODECLASS_VIEW;
                break;
            default:
                typeOk = false;
        }
        if(!typeOk) {
            UA_LOG_NODEID_WRAP(nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                                "AddNodes: Type for %.*s does not match node class",
                                (int)nodeIdStr.length, nodeIdStr.data));
            retval = UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
            goto cleanup;
        }

        /* See if the type has the correct node class. For type-nodes, we know
         * that type has the same nodeClass from checkParentReference. */
        if(head->nodeClass == UA_NODECLASS_VARIABLE &&
           type->variableTypeNode.isAbstract) {
            /* Get subtypes of the parent reference types */
            UA_ReferenceTypeSet refTypes1, refTypes2;
            retval |= referenceTypeIndices(server, &parentReferences[0], &refTypes1, true);
            retval |= referenceTypeIndices(server, &parentReferences[1], &refTypes2, true);
            UA_ReferenceTypeSet refTypes = UA_ReferenceTypeSet_union(refTypes1, refTypes2);
            if(retval != UA_STATUSCODE_GOOD)
                goto cleanup;
            
            /* Abstract variable is allowed if parent is a children of a
             * base data variable. An abstract variable may be part of an
             * object type which again is below BaseObjectType */
            const UA_NodeId variableTypes = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);
            const UA_NodeId objectTypes = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE);
            if(!isNodeInTree(server, parentNodeId, &variableTypes, &refTypes) &&
               !isNodeInTree(server, parentNodeId, &objectTypes, &refTypes)) {
                UA_LOG_NODEID_WRAP(nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                                                               "AddNodes: Type of variable node %.*s must "
                                                               "be VariableType and not cannot be abstract",
                                                               (int)nodeIdStr.length, nodeIdStr.data));
                retval = UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
                goto cleanup;
            }
        }

        if(head->nodeClass == UA_NODECLASS_OBJECT &&
           type->objectTypeNode.isAbstract) {
            /* Get subtypes of the parent reference types */
            UA_ReferenceTypeSet refTypes1, refTypes2;
            retval |= referenceTypeIndices(server, &parentReferences[0], &refTypes1, true);
            retval |= referenceTypeIndices(server, &parentReferences[1], &refTypes2, true);
            UA_ReferenceTypeSet refTypes = UA_ReferenceTypeSet_union(refTypes1, refTypes2);
            if(retval != UA_STATUSCODE_GOOD)
                goto cleanup;


            /* Object node created of an abstract ObjectType. Only allowed if
             * within BaseObjectType folder or if it's an event (subType of
             * BaseEventType) */
            const UA_NodeId objectTypes = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE);
            UA_Boolean isInBaseObjectType =
                isNodeInTree(server, parentNodeId, &objectTypes, &refTypes);
            
            const UA_NodeId eventTypes = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE);
            UA_Boolean isInBaseEventType =
                isNodeInTree_singleRef(server, &type->head.nodeId, &eventTypes,
                                       UA_REFERENCETYPEINDEX_HASSUBTYPE);
            
            if(!isInBaseObjectType && !(isInBaseEventType && UA_NodeId_isNull(parentNodeId))) {
                UA_LOG_NODEID_WRAP(nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                                                               "AddNodes: Type of object node %.*s must "
                                                               "be ObjectType and not be abstract",
                                                               (int)nodeIdStr.length, nodeIdStr.data));
                retval = UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
                goto cleanup;
            }
        }
    }

    /* Add reference to the parent */
    if(!UA_NodeId_isNull(parentNodeId)) {
        if(UA_NodeId_isNull(referenceTypeId)) {
            UA_LOG_NODEID_WRAP(nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                                "AddNodes: Reference to parent of %.*s cannot be null",
                                (int)nodeIdStr.length, nodeIdStr.data));
            retval = UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
            goto cleanup;
        }

        retval = addRef(server, session, &head->nodeId, referenceTypeId, parentNodeId, false);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_NODEID_WRAP(nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                                "AddNodes: Adding reference to parent of %.*s failed",
                                (int)nodeIdStr.length, nodeIdStr.data));
            goto cleanup;
        }
    }

    /* Add a hasTypeDefinition reference */
    if(head->nodeClass == UA_NODECLASS_VARIABLE ||
       head->nodeClass == UA_NODECLASS_OBJECT) {
        UA_assert(type != NULL); /* see above */
        retval = addRef(server, session, &head->nodeId, &hasTypeDefinition, &type->head.nodeId, true);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_NODEID_WRAP(nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                                "AddNodes: Adding a reference to the type "
                                "definition of %.*s failed with error code %s",
                                (int)nodeIdStr.length, nodeIdStr.data,
                                UA_StatusCode_name(retval)));
        }
    }

 cleanup:
    UA_NODESTORE_RELEASE(server, node);
    if(type)
        UA_NODESTORE_RELEASE(server, type);
    return retval;
}

/* Create the node and add it to the nodestore. But don't typecheck and add
 * references so far */
UA_StatusCode
AddNode_raw(UA_Server *server, UA_Session *session, void *nodeContext,
            const UA_AddNodesItem *item, UA_NodeId *outNewNodeId) {
    /* Do not check access for server */
    if(session != &server->adminSession && server->config.accessControl.allowAddNode) {
        UA_UNLOCK(server->serviceMutex)
        if (!server->config.accessControl.allowAddNode(server, &server->config.accessControl,
                                                       &session->sessionId, session->sessionHandle, item)) {
            UA_LOCK(server->serviceMutex);
            return UA_STATUSCODE_BADUSERACCESSDENIED;
        }
        UA_LOCK(server->serviceMutex);
    }

    /* Check the namespaceindex */
    if(item->requestedNewNodeId.nodeId.namespaceIndex >= server->namespacesSize) {
        UA_LOG_INFO_SESSION(&server->config.logger, session,
                            "AddNodes: Namespace invalid");
        return UA_STATUSCODE_BADNODEIDINVALID;
    }

    if(item->nodeAttributes.encoding != UA_EXTENSIONOBJECT_DECODED &&
       item->nodeAttributes.encoding != UA_EXTENSIONOBJECT_DECODED_NODELETE) {
        UA_LOG_INFO_SESSION(&server->config.logger, session,
                            "AddNodes: Node attributes invalid");
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    /* Create a node */
    UA_Node *node = UA_NODESTORE_NEW(server, item->nodeClass);
    if(!node) {
        UA_LOG_INFO_SESSION(&server->config.logger, session,
                            "AddNodes: Node could not create a node "
                            "in the nodestore");
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }

    /* Fill the node attributes */
    node->head.context = nodeContext;
    UA_StatusCode retval = UA_NodeId_copy(&item->requestedNewNodeId.nodeId, &node->head.nodeId);
    if(retval != UA_STATUSCODE_GOOD)
        goto create_error;

    retval = UA_QualifiedName_copy(&item->browseName, &node->head.browseName);
    if(retval != UA_STATUSCODE_GOOD)
        goto create_error;

    retval = UA_Node_setAttributes(node, item->nodeAttributes.content.decoded.data,
                                   item->nodeAttributes.content.decoded.type);
    if(retval != UA_STATUSCODE_GOOD)
        goto create_error;

    /* Add the node to the nodestore */
    UA_NodeId tmpOutId = UA_NODEID_NULL;
    if(!outNewNodeId)
        outNewNodeId = &tmpOutId;
    retval = UA_NODESTORE_INSERT(server, node, outNewNodeId);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_INFO_SESSION(&server->config.logger, session,
                            "AddNodes: Node could not add the new node "
                            "to the nodestore with error code %s",
                            UA_StatusCode_name(retval));
        return retval;
    }

    if(outNewNodeId == &tmpOutId)
        UA_NodeId_clear(&tmpOutId);

    return UA_STATUSCODE_GOOD;

create_error:
    UA_LOG_INFO_SESSION(&server->config.logger, session,
                        "AddNodes: Node could not create a node "
                        "with error code %s", UA_StatusCode_name(retval));
    UA_NODESTORE_DELETE(server, node);
    return retval;
}

static UA_StatusCode
findDefaultInstanceBrowseNameNode(UA_Server *server, UA_NodeId startingNode,
                                  UA_NodeId *foundId) {
    UA_NodeId_init(foundId);
    UA_RelativePathElement rpe;
    UA_RelativePathElement_init(&rpe);
    rpe.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
    rpe.targetName = UA_QUALIFIEDNAME(0, "DefaultInstanceBrowseName");
    UA_BrowsePath bp;
    UA_BrowsePath_init(&bp);
    bp.startingNode = startingNode;
    bp.relativePath.elementsSize = 1;
    bp.relativePath.elements = &rpe;
    UA_BrowsePathResult bpr = translateBrowsePathToNodeIds(server, &bp);
    UA_StatusCode retval = bpr.statusCode;
    if(retval == UA_STATUSCODE_GOOD && bpr.targetsSize > 0)
        retval = UA_NodeId_copy(&bpr.targets[0].targetId.nodeId, foundId);
    UA_BrowsePathResult_clear(&bpr);
    return retval;
}

/* Check if we got a valid browse name for the new node. For object nodes the
 * BrowseName may only be null if the parent type has a
 * 'DefaultInstanceBrowseName' property. */
static UA_StatusCode
checkSetBrowseName(UA_Server *server, UA_Session *session, UA_AddNodesItem *item) {
    /* If the object node already has a browse name we are done here. */
    if(!UA_QualifiedName_isNull(&item->browseName))
        return UA_STATUSCODE_GOOD;

    /* Nodes other than Objects must have a BrowseName */
    if(item->nodeClass != UA_NODECLASS_OBJECT)
        return UA_STATUSCODE_BADBROWSENAMEINVALID;

    /* At this point we have an object with an empty browse name. Check the type
     * node if it has a DefaultInstanceBrowseName property. */
    UA_NodeId defaultBrowseNameNode;
    UA_StatusCode retval =
        findDefaultInstanceBrowseNameNode(server, item->typeDefinition.nodeId,
                                          &defaultBrowseNameNode);
    if(retval != UA_STATUSCODE_GOOD)
        return UA_STATUSCODE_BADBROWSENAMEINVALID;

    UA_Variant defaultBrowseName;
    retval = readWithReadValue(server, &defaultBrowseNameNode,
                               UA_ATTRIBUTEID_VALUE, &defaultBrowseName);
    UA_NodeId_clear(&defaultBrowseNameNode);
    if(retval != UA_STATUSCODE_GOOD)
        return UA_STATUSCODE_BADBROWSENAMEINVALID;

    if(UA_Variant_hasScalarType(&defaultBrowseName, &UA_TYPES[UA_TYPES_QUALIFIEDNAME])) {
        item->browseName = *(UA_QualifiedName*)defaultBrowseName.data;
        UA_QualifiedName_init((UA_QualifiedName*)defaultBrowseName.data);
    } else {
        retval = UA_STATUSCODE_BADBROWSENAMEINVALID;
    }

    UA_Variant_clear(&defaultBrowseName);
    return retval;
}

/* Prepare the node, then add it to the nodestore */
static UA_StatusCode
Operation_addNode_begin(UA_Server *server, UA_Session *session, void *nodeContext,
                        const UA_AddNodesItem *item, const UA_NodeId *parentNodeId,
                        const UA_NodeId *referenceTypeId, UA_NodeId *outNewNodeId) {
    /* Create a temporary NodeId if none is returned */
    UA_NodeId newId;
    if(!outNewNodeId) {
        UA_NodeId_init(&newId);
        outNewNodeId = &newId;
    }

    /* Set the BrowsenName before adding to the Nodestore. The BrowseName is
     * immutable afterwards. */
    UA_Boolean noBrowseName = UA_QualifiedName_isNull(&item->browseName);
    UA_StatusCode retval = checkSetBrowseName(server, session, (UA_AddNodesItem*)(uintptr_t)item);
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    /* Create the node and add it to the nodestore */
    retval = AddNode_raw(server, session, nodeContext, item, outNewNodeId);
    if(retval != UA_STATUSCODE_GOOD)
        goto cleanup;

    /* Typecheck and add references to parent and type definition */
    retval = AddNode_addRefs(server, session, outNewNodeId, parentNodeId,
                             referenceTypeId, &item->typeDefinition.nodeId);
    if(retval != UA_STATUSCODE_GOOD)
        deleteNode(server, *outNewNodeId, true);

    if(outNewNodeId == &newId)
        UA_NodeId_clear(&newId);

 cleanup:
    if(noBrowseName)
        UA_QualifiedName_clear((UA_QualifiedName*)(uintptr_t)&item->browseName);
    return retval;
}

static UA_StatusCode
recursiveTypeCheckAddChildren(UA_Server *server, UA_Session *session,
                              const UA_Node **nodeptr, const UA_Node *type) {
    UA_assert(type != NULL);
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    const UA_Node *node = *nodeptr;

    /* Use attributes from the type. The value and value constraints are the
     * same for the variable and variabletype attribute structs. */
    if(node->head.nodeClass == UA_NODECLASS_VARIABLE ||
       node->head.nodeClass == UA_NODECLASS_VARIABLETYPE) {
        retval = useVariableTypeAttributes(server, session, (const UA_VariableNode**)nodeptr,
                                           &type->variableTypeNode);
        node = *nodeptr; /* If the node was replaced */
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_NODEID_WRAP(&node->head.nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                               "AddNodes: Using attributes for %.*s from the variable type "
                               "failed with error code %s", (int)nodeIdStr.length,
                               nodeIdStr.data, UA_StatusCode_name(retval)));
            return retval;
        }

        /* Check NodeClass for 'hasSubtype'. UA_NODECLASS_VARIABLE not allowed to have subtype */
        if(node->head.nodeClass == UA_NODECLASS_VARIABLE) {
            for(size_t i = 0; i < node->head.referencesSize; i++) {
                if(node->head.references[i].referenceTypeIndex == UA_REFERENCETYPEINDEX_HASSUBTYPE) {
                    UA_LOG_INFO_SESSION(&server->config.logger, session,
                                        "AddNodes: VariableType not allowed to have HasSubType");
                    return UA_STATUSCODE_BADREFERENCENOTALLOWED;
                }
            }
        }

        /* Check if all attributes hold the constraints of the type now. The initial
         * attributes must type-check. The constructor might change the attributes
         * again. Then, the changes are type-checked by the normal write service. */
        retval = typeCheckVariableNode(server, session, &node->variableNode, &type->variableTypeNode);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_NODEID_WRAP(&node->head.nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                               "AddNodes: Type-checking the variable node %.*s "
                               "failed with error code %s", (int)nodeIdStr.length,
                               nodeIdStr.data, UA_StatusCode_name(retval)));
            return retval;
        }
    }

    /* Add (mandatory) child nodes from the type definition */
    if(node->head.nodeClass == UA_NODECLASS_VARIABLE ||
       node->head.nodeClass == UA_NODECLASS_OBJECT) {
        retval = addTypeChildren(server, session, &node->head, &type->head);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_NODEID_WRAP(&node->head.nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                               "AddNodes: Adding child nodes of  %.*s failed with error code %s",
                               (int)nodeIdStr.length, nodeIdStr.data, UA_StatusCode_name(retval)));
        }
    }

    return UA_STATUSCODE_GOOD;
}

/* Construct children first */
static UA_StatusCode
recursiveCallConstructors(UA_Server *server, UA_Session *session,
                          const UA_NodeHead *head, const UA_Node *type) {
    if(head->constructed)
        return UA_STATUSCODE_GOOD;
    
    /* Construct the children */
    UA_BrowseDescription bd;
    UA_BrowseDescription_init(&bd);
    bd.nodeId = head->nodeId;
    bd.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_AGGREGATES);
    bd.includeSubtypes = true;
    bd.browseDirection = UA_BROWSEDIRECTION_FORWARD;

    UA_BrowseResult br;
    UA_BrowseResult_init(&br);
    UA_UInt32 maxrefs = 0;
    Operation_Browse(server, session, &maxrefs, &bd, &br);
    if(br.statusCode != UA_STATUSCODE_GOOD)
        return br.statusCode;

    /* Call the constructor for every unconstructed node */
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    for(size_t i = 0; i < br.referencesSize; ++i) {
        UA_ReferenceDescription *rd = &br.references[i];
        const UA_Node *target = UA_NODESTORE_GET(server, &rd->nodeId.nodeId);
        if(!target)
            continue;
        if(target->head.constructed) {
            UA_NODESTORE_RELEASE(server, target);
            continue;
        }

        const UA_Node *targetType = NULL;
        if(head->nodeClass == UA_NODECLASS_VARIABLE ||
           head->nodeClass == UA_NODECLASS_OBJECT) {
            targetType = getNodeType(server, &target->head);
            if(!targetType) {
                UA_NODESTORE_RELEASE(server, target);
                retval = UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
                break;
            }
        }
        retval = recursiveCallConstructors(server, session, &target->head, targetType);
        UA_NODESTORE_RELEASE(server, target);
        if(targetType)
            UA_NODESTORE_RELEASE(server, targetType);
        if(retval != UA_STATUSCODE_GOOD)
            break;
    }

    UA_BrowseResult_clear(&br);

    /* If a child could not be constructed or the node is already constructed */
    if(retval != UA_STATUSCODE_GOOD)
        return retval;

    /* Get the node type constructor */
    const UA_NodeTypeLifecycle *lifecycle = NULL;
    if(type && head->nodeClass == UA_NODECLASS_OBJECT) {
        lifecycle = &type->objectTypeNode.lifecycle;
    } else if(type && head->nodeClass == UA_NODECLASS_VARIABLE) {
        lifecycle = &type->variableTypeNode.lifecycle;
    }

    /* Call the global constructor */
    void *context = head->context;
    if(server->config.nodeLifecycle.constructor) {
        UA_UNLOCK(server->serviceMutex);
        retval = server->config.nodeLifecycle.constructor(server, &session->sessionId,
                                                          session->sessionHandle,
                                                          &head->nodeId, &context);
        UA_LOCK(server->serviceMutex);
    }

    /* Call the type constructor */
    if(retval == UA_STATUSCODE_GOOD && lifecycle && lifecycle->constructor) {
        UA_UNLOCK(server->serviceMutex)
        retval = lifecycle->constructor(server, &session->sessionId,
                                        session->sessionHandle, &type->head.nodeId,
                                        type->head.context, &head->nodeId, &context);
        UA_LOCK(server->serviceMutex);
    }
    if(retval != UA_STATUSCODE_GOOD)
        goto fail1;

    /* Set the context *and* mark the node as constructed */
    if(retval == UA_STATUSCODE_GOOD)
        retval = UA_Server_editNode(server, &server->adminSession, &head->nodeId,
                                    (UA_EditNodeCallback)setConstructedNodeContext,
                                    context);

    /* All good, return */
    if(retval == UA_STATUSCODE_GOOD)
        return retval;

    /* Fail. Call the destructors. */
    if(lifecycle && lifecycle->destructor) {
        UA_UNLOCK(server->serviceMutex);
        lifecycle->destructor(server, &session->sessionId,
                              session->sessionHandle, &type->head.nodeId,
                              type->head.context, &head->nodeId, &context);
        UA_LOCK(server->serviceMutex)
    }


 fail1:
    if(server->config.nodeLifecycle.destructor) {
        UA_UNLOCK(server->serviceMutex);
        server->config.nodeLifecycle.destructor(server, &session->sessionId,
                                                session->sessionHandle,
                                                &head->nodeId, context);
        UA_LOCK(server->serviceMutex);
    }

    return retval;
}

static void
recursiveDeconstructNode(UA_Server *server, UA_Session *session,
                         UA_ReferenceTypeSet *hierarchRefsSet,
                         const UA_NodeHead *head);

static void
recursiveDeleteNode(UA_Server *server, UA_Session *session,
                    const UA_ReferenceTypeSet *hierarchRefsSet,
                    const UA_NodeHead *head, UA_Boolean removeTargetRefs);

/* Add new ReferenceType to the subtypes bitfield */
static UA_StatusCode
addReferenceTypeSubtype(UA_Server *server, UA_Session *session,
                        UA_Node *node, void *context) {
    node->referenceTypeNode.subTypes =
        UA_ReferenceTypeSet_union(node->referenceTypeNode.subTypes,
                                  *(UA_ReferenceTypeSet*)context);
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
setReferenceTypeSubtypes(UA_Server *server, const UA_ReferenceTypeNode *node) {
    /* Get the ReferenceTypes upwards in the hierarchy */
    size_t parentsSize = 0;
    UA_ExpandedNodeId *parents = NULL;
    UA_ReferenceTypeSet reftypes_subtype = UA_REFTYPESET(UA_REFERENCETYPEINDEX_HASSUBTYPE);
    UA_StatusCode res =
        browseRecursive(server, 1, &node->head.nodeId, &reftypes_subtype,
                        UA_BROWSEDIRECTION_INVERSE, false, &parentsSize, &parents);
    if(res != UA_STATUSCODE_GOOD)
        return res;

    /* Add the ReferenceTypeIndex of this node */
    const UA_ReferenceTypeSet *newRefSet = &node->subTypes;
    for(size_t i = 0; i < parentsSize; i++) {
        UA_Server_editNode(server, &server->adminSession, &parents[i].nodeId,
                           addReferenceTypeSubtype, (void*)(uintptr_t)newRefSet);
    }

    UA_Array_delete(parents, parentsSize, &UA_TYPES[UA_TYPES_EXPANDEDNODEID]);
    return UA_STATUSCODE_GOOD;
}

/* Children, references, type-checking, constructors. */
UA_StatusCode
AddNode_finish(UA_Server *server, UA_Session *session, const UA_NodeId *nodeId) {
    /* Get the node */
    const UA_Node *node = UA_NODESTORE_GET(server, nodeId);
    if(!node)
        return UA_STATUSCODE_BADNODEIDUNKNOWN;

    const UA_Node *type = NULL;
    const UA_NodeHead *head = &node->head;

    /* Set the ReferenceTypesSet of subtypes in the ReferenceTypeNode */
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    if(node->head.nodeClass == UA_NODECLASS_REFERENCETYPE) {
        retval = setReferenceTypeSubtypes(server, (const UA_ReferenceTypeNode*)node);
        if(retval != UA_STATUSCODE_GOOD)
            goto cleanup;
    }

    /* Instantiate variables and objects */
    if(head->nodeClass == UA_NODECLASS_VARIABLE ||
       head->nodeClass == UA_NODECLASS_VARIABLETYPE ||
       head->nodeClass == UA_NODECLASS_OBJECT) {
        /* Get the type node */
        type = getNodeType(server, head);
        if(!type) {
            if(server->bootstrapNS0)
                goto constructor;
            UA_LOG_NODEID_WRAP(&head->nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                               "AddNodes: Node type for %.*s not found",
                               (int)nodeIdStr.length, nodeIdStr.data));
            retval = UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
            goto cleanup;
        }

        retval = recursiveTypeCheckAddChildren(server, session, &node, type);
        head = &node->head; /* Pointer might have changed */
        if(retval != UA_STATUSCODE_GOOD)
            goto cleanup;
    }

    /* Call the constructor(s) */
 constructor:
    retval = recursiveCallConstructors(server, session, head, type);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_NODEID_WRAP(&node->head.nodeId, UA_LOG_INFO_SESSION(&server->config.logger, session,
                           "AddNodes: Calling the node constructor(s) of %.*s failed "
                           "with status code %s", (int)nodeIdStr.length,
                           nodeIdStr.data, UA_StatusCode_name(retval)));
    }

 cleanup:
    if(type)
        UA_NODESTORE_RELEASE(server, type);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_ReferenceTypeSet emptyRefs;
        UA_ReferenceTypeSet_init(&emptyRefs);
        recursiveDeconstructNode(server, session, &emptyRefs, head);
        recursiveDeleteNode(server, session, &emptyRefs, head, true);
    }
    UA_NODESTORE_RELEASE(server, node);
    return retval;
}

static void
Operation_addNode(UA_Server *server, UA_Session *session, void *nodeContext,
                  const UA_AddNodesItem *item, UA_AddNodesResult *result) {
    result->statusCode =
        Operation_addNode_begin(server, session, nodeContext, item, &item->parentNodeId.nodeId,
                                &item->referenceTypeId, &result->addedNodeId);
    if(result->statusCode != UA_STATUSCODE_GOOD)
        return;

    /* AddNodes_finish */
    result->statusCode = AddNode_finish(server, session, &result->addedNodeId);

    /* If finishing failed, the node was deleted */
    if(result->statusCode != UA_STATUSCODE_GOOD)
        UA_NodeId_clear(&result->addedNodeId);
}

void
Service_AddNodes(UA_Server *server, UA_Session *session,
                 const UA_AddNodesRequest *request,
                 UA_AddNodesResponse *response) {
    UA_LOG_DEBUG_SESSION(&server->config.logger, session, "Processing AddNodesRequest");
    UA_LOCK_ASSERT(server->serviceMutex, 1);

    if(server->config.maxNodesPerNodeManagement != 0 &&
       request->nodesToAddSize > server->config.maxNodesPerNodeManagement) {
        response->responseHeader.serviceResult = UA_STATUSCODE_BADTOOMANYOPERATIONS;
        return;
    }

    response->responseHeader.serviceResult =
        UA_Server_processServiceOperations(server, session,
                                           (UA_ServiceOperation)Operation_addNode, NULL,
                                           &request->nodesToAddSize, &UA_TYPES[UA_TYPES_ADDNODESITEM],
                                           &response->resultsSize, &UA_TYPES[UA_TYPES_ADDNODESRESULT]);
}

UA_StatusCode
addNode(UA_Server *server, const UA_NodeClass nodeClass, const UA_NodeId *requestedNewNodeId,
        const UA_NodeId *parentNodeId, const UA_NodeId *referenceTypeId,
        const UA_QualifiedName browseName, const UA_NodeId *typeDefinition,
        const UA_NodeAttributes *attr, const UA_DataType *attributeType,
        void *nodeContext, UA_NodeId *outNewNodeId) {
    UA_LOCK_ASSERT(server->serviceMutex, 1);

    /* Create the AddNodesItem */
    UA_AddNodesItem item;
    UA_AddNodesItem_init(&item);
    item.nodeClass = nodeClass;
    item.requestedNewNodeId.nodeId = *requestedNewNodeId;
    item.browseName = browseName;
    item.parentNodeId.nodeId = *parentNodeId;
    item.referenceTypeId = *referenceTypeId;
    item.typeDefinition.nodeId = *typeDefinition;
    item.nodeAttributes.encoding = UA_EXTENSIONOBJECT_DECODED_NODELETE;
    item.nodeAttributes.content.decoded.type = attributeType;
    item.nodeAttributes.content.decoded.data = (void*)(uintptr_t)attr;

    /* Call the normal addnodes service */
    UA_AddNodesResult result;
    UA_AddNodesResult_init(&result);
    Operation_addNode(server, &server->adminSession, nodeContext, &item, &result);
    if(outNewNodeId)
        *outNewNodeId = result.addedNodeId;
    else
        UA_NodeId_clear(&result.addedNodeId);
    return result.statusCode;
}

UA_StatusCode
__UA_Server_addNode(UA_Server *server, const UA_NodeClass nodeClass,
                    const UA_NodeId *requestedNewNodeId,
                    const UA_NodeId *parentNodeId,
                    const UA_NodeId *referenceTypeId,
                    const UA_QualifiedName browseName,
                    const UA_NodeId *typeDefinition,
                    const UA_NodeAttributes *attr,
                    const UA_DataType *attributeType,
                    void *nodeContext, UA_NodeId *outNewNodeId) {
    UA_LOCK(server->serviceMutex)
    UA_StatusCode reval =
        addNode(server, nodeClass, requestedNewNodeId, parentNodeId,
                referenceTypeId, browseName, typeDefinition, attr,
                attributeType, nodeContext, outNewNodeId);
    UA_UNLOCK(server->serviceMutex);
    return reval;
}

UA_StatusCode
UA_Server_addNode_begin(UA_Server *server, const UA_NodeClass nodeClass,
                        const UA_NodeId requestedNewNodeId, const UA_NodeId parentNodeId,
                        const UA_NodeId referenceTypeId, const UA_QualifiedName browseName,
                        const UA_NodeId typeDefinition, const void *attr,
                        const UA_DataType *attributeType, void *nodeContext,
                        UA_NodeId *outNewNodeId) {
    UA_AddNodesItem item;
    UA_AddNodesItem_init(&item);
    item.nodeClass = nodeClass;
    item.requestedNewNodeId.nodeId = requestedNewNodeId;
    item.browseName = browseName;
    item.typeDefinition.nodeId = typeDefinition;
    item.nodeAttributes.encoding = UA_EXTENSIONOBJECT_DECODED_NODELETE;
    item.nodeAttributes.content.decoded.type = attributeType;
    item.nodeAttributes.content.decoded.data = (void*)(uintptr_t)attr;

    UA_LOCK(server->serviceMutex);
    UA_StatusCode retval =
        Operation_addNode_begin(server, &server->adminSession, nodeContext, &item,
                                &parentNodeId, &referenceTypeId, outNewNodeId);
    UA_UNLOCK(server->serviceMutex);
    return retval;
}

UA_StatusCode
UA_Server_addNode_finish(UA_Server *server, const UA_NodeId nodeId) {
    UA_LOCK(server->serviceMutex);
    UA_StatusCode retval = AddNode_finish(server, &server->adminSession, &nodeId);
    UA_UNLOCK(server->serviceMutex);
    return retval;
}

/****************/
/* Delete Nodes */
/****************/

static void
Operation_deleteReference(UA_Server *server, UA_Session *session, void *context,
                          const UA_DeleteReferencesItem *item, UA_StatusCode *retval);

/* Remove references to this node (in the other nodes) */
static void
removeIncomingReferences(UA_Server *server, UA_Session *session, const UA_NodeHead *head) {
    UA_DeleteReferencesItem item;
    UA_DeleteReferencesItem_init(&item);
    item.targetNodeId.nodeId = head->nodeId;
    item.deleteBidirectional = false;
    UA_StatusCode dummy;
    for(size_t i = 0; i < head->referencesSize; ++i) {
        UA_NodeReferenceKind *refs = &head->references[i];
        item.isForward = refs->isInverse;
        item.referenceTypeId = *UA_NODESTORE_GETREFERENCETYPEID(server, refs->referenceTypeIndex);
        for(size_t j = 0; j < refs->refTargetsSize; ++j) {
            item.sourceNodeId = refs->refTargets[j].targetId.nodeId;
            Operation_deleteReference(server, session, NULL, &item, &dummy);
        }
    }
}

/* A node can only be deleted if it has at most one incoming hierarchical */
static UA_Boolean
multipleHierarchicalRefs(const UA_NodeHead *head, const UA_ReferenceTypeSet *refSet) {
    size_t incomingRefs = 0;
    for(size_t i = 0; i < head->referencesSize; i++) {
        const UA_NodeReferenceKind *k = &head->references[i];
        if(!k->isInverse)
            continue;
        if(!UA_ReferenceTypeSet_contains(refSet, k->referenceTypeIndex))
            continue;
        incomingRefs += k->refTargetsSize;
        if(incomingRefs > 1)
            return true;
    }
    return false;
}

/* Recursively call the destructors of this node and all child nodes.
 * Deconstructs the parent before its children. */
static void
recursiveDeconstructNode(UA_Server *server, UA_Session *session,
                         UA_ReferenceTypeSet *hierarchRefsSet, const UA_NodeHead *head) {
    /* Was the constructor called for the node? */
    if(!head->constructed)
        return;

    /* Call the type-level destructor */
    void *context = head->context; /* No longer needed after this function */
    if(head->nodeClass == UA_NODECLASS_OBJECT ||
       head->nodeClass == UA_NODECLASS_VARIABLE) {
        const UA_Node *type = getNodeType(server, head);
        if(type) {
            const UA_NodeTypeLifecycle *lifecycle;
            if(head->nodeClass == UA_NODECLASS_OBJECT)
                lifecycle = &type->objectTypeNode.lifecycle;
            else
                lifecycle = &type->variableTypeNode.lifecycle;
            if(lifecycle->destructor) {
                UA_UNLOCK(server->serviceMutex);
                lifecycle->destructor(server,
                                      &session->sessionId, session->sessionHandle,
                                      &type->head.nodeId, type->head.context,
                                      &head->nodeId, &context);
                UA_LOCK(server->serviceMutex);
            }
            UA_NODESTORE_RELEASE(server, type);
        }
    }

    /* Call the global destructor */
    if(server->config.nodeLifecycle.destructor) {
        UA_UNLOCK(server->serviceMutex);
        server->config.nodeLifecycle.destructor(server, &session->sessionId,
                                                session->sessionHandle,
                                                &head->nodeId, context);
        UA_LOCK(server->serviceMutex);
    }

    /* Set the constructed flag to false */
    UA_Server_editNode(server, &server->adminSession, &head->nodeId,
                       (UA_EditNodeCallback)setDeconstructedNode, context);

    /* Browse to get all children of the node */
    UA_BrowseDescription bd;
    UA_BrowseDescription_init(&bd);
    bd.nodeId = head->nodeId;
    bd.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_AGGREGATES);
    bd.includeSubtypes = true;
    bd.browseDirection = UA_BROWSEDIRECTION_FORWARD;

    UA_BrowseResult br;
    UA_BrowseResult_init(&br);
    UA_UInt32 maxrefs = 0;
    Operation_Browse(server, session, &maxrefs, &bd, &br);
    if(br.statusCode != UA_STATUSCODE_GOOD)
        return;

    /* Deconstruct every child node that has not other parent */
    for(size_t i = 0; i < br.referencesSize; ++i) {
        UA_ReferenceDescription *rd = &br.references[i];
        const UA_Node *child = UA_NODESTORE_GET(server, &rd->nodeId.nodeId);
        if(!child)
            continue;
        if(!multipleHierarchicalRefs(&child->head, hierarchRefsSet))
            recursiveDeconstructNode(server, session, hierarchRefsSet, &child->head);
        UA_NODESTORE_RELEASE(server, child);
    }

    UA_BrowseResult_clear(&br);
}

static void
recursiveDeleteNode(UA_Server *server, UA_Session *session,
                    const UA_ReferenceTypeSet *hierarchRefsSet,
                    const UA_NodeHead *head, UA_Boolean removeTargetRefs) {
    /* Browse to get all children of the node */
    UA_BrowseDescription bd;
    UA_BrowseDescription_init(&bd);
    bd.nodeId = head->nodeId;
    bd.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_AGGREGATES);
    bd.includeSubtypes = true;
    bd.browseDirection = UA_BROWSEDIRECTION_FORWARD;

    UA_BrowseResult br;
    UA_BrowseResult_init(&br);
    UA_UInt32 maxrefs = 0;
    Operation_Browse(server, session, &maxrefs, &bd, &br);
    if(br.statusCode != UA_STATUSCODE_GOOD)
        return;

    /* Remove every child that has no other parent */
    for(size_t i = 0; i < br.referencesSize; ++i) {
        UA_ReferenceDescription *rd = &br.references[i];
        /* Check for self-reference to avoid endless loop */
        if(UA_NodeId_equal(&head->nodeId, &rd->nodeId.nodeId))
            continue;
        const UA_Node *child = UA_NODESTORE_GET(server, &rd->nodeId.nodeId);
        if(!child)
            continue;
        /* Only delete child nodes that have no other parent */
        if(!multipleHierarchicalRefs(&child->head, hierarchRefsSet))
            recursiveDeleteNode(server, session, hierarchRefsSet, &child->head, true);
        UA_NODESTORE_RELEASE(server, child);
    }

    UA_BrowseResult_clear(&br);

    if(removeTargetRefs)
        removeIncomingReferences(server, session, head);

    UA_NODESTORE_REMOVE(server, &head->nodeId);
}

static void
deleteNodeOperation(UA_Server *server, UA_Session *session, void *context,
                    const UA_DeleteNodesItem *item, UA_StatusCode *result) {
    /* Do not check access for server */
    if(session != &server->adminSession && server->config.accessControl.allowDeleteNode) {
        UA_UNLOCK(server->serviceMutex);
        if ( !server->config.accessControl.allowDeleteNode(server, &server->config.accessControl,
                &session->sessionId, session->sessionHandle, item)) {
            UA_LOCK(server->serviceMutex);
            *result = UA_STATUSCODE_BADUSERACCESSDENIED;
            return;
        }
        UA_LOCK(server->serviceMutex);
    }

    const UA_Node *node = UA_NODESTORE_GET(server, &item->nodeId);
    if(!node) {
        *result = UA_STATUSCODE_BADNODEIDUNKNOWN;
        return;
    }

    if(UA_Node_hasSubTypeOrInstances(&node->head)) {
        UA_LOG_INFO_SESSION(&server->config.logger, session,
                            "Delete Nodes: Cannot delete a type node "
                            "with active instances or subtypes");
        UA_NODESTORE_RELEASE(server, node);
        *result = UA_STATUSCODE_BADINTERNALERROR;
        return;
    }

    /* TODO: Check if the information model consistency is violated */
    /* TODO: Check if the node is a mandatory child of a parent */

    /* A node can be referenced with hierarchical references from several
     * parents in the information model. (But not in a circular way.) The
     * hierarchical references are checked to see if a node can be deleted.
     * Getting the type hierarchy can fail in case of low RAM. In that case the
     * nodes are always deleted. */
    UA_ReferenceTypeSet hierarchRefsSet;
    UA_NodeId hr = UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
    referenceTypeIndices(server, &hr, &hierarchRefsSet, true);

    recursiveDeconstructNode(server, session, &hierarchRefsSet, &node->head);
    recursiveDeleteNode(server, session, &hierarchRefsSet, &node->head,
                        item->deleteTargetReferences);
    
    UA_NODESTORE_RELEASE(server, node);
}

void
Service_DeleteNodes(UA_Server *server, UA_Session *session,
                    const UA_DeleteNodesRequest *request,
                    UA_DeleteNodesResponse *response) {
    UA_LOG_DEBUG_SESSION(&server->config.logger, session,
                         "Processing DeleteNodesRequest");
    UA_LOCK_ASSERT(server->serviceMutex, 1);

    if(server->config.maxNodesPerNodeManagement != 0 &&
       request->nodesToDeleteSize > server->config.maxNodesPerNodeManagement) {
        response->responseHeader.serviceResult = UA_STATUSCODE_BADTOOMANYOPERATIONS;
        return;
    }

    response->responseHeader.serviceResult =
        UA_Server_processServiceOperations(server, session,
                                           (UA_ServiceOperation)deleteNodeOperation,
                                           NULL, &request->nodesToDeleteSize,
                                           &UA_TYPES[UA_TYPES_DELETENODESITEM],
                                           &response->resultsSize, &UA_TYPES[UA_TYPES_STATUSCODE]);
}

UA_StatusCode
UA_Server_deleteNode(UA_Server *server, const UA_NodeId nodeId,
                     UA_Boolean deleteReferences) {
    UA_LOCK(server->serviceMutex)
    UA_StatusCode retval = deleteNode(server, nodeId, deleteReferences);
    UA_UNLOCK(server->serviceMutex);
    return retval;
}

UA_StatusCode
deleteNode(UA_Server *server, const UA_NodeId nodeId,
                     UA_Boolean deleteReferences) {
    UA_LOCK_ASSERT(server->serviceMutex, 1);
    UA_DeleteNodesItem item;
    item.deleteTargetReferences = deleteReferences;
    item.nodeId = nodeId;
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    deleteNodeOperation(server, &server->adminSession, NULL, &item, &retval);
    return retval;
}

/******************/
/* Add References */
/******************/

struct AddNodeInfo {
    UA_Byte refTypeIndex;
    UA_Boolean isForward;
    const UA_ExpandedNodeId *targetNodeId;
    UA_UInt32 targetBrowseNameHash;
};

static UA_StatusCode
addOneWayReference(UA_Server *server, UA_Session *session, UA_Node *node,
                   const struct AddNodeInfo *info) {
    return UA_Node_addReference(node, info->refTypeIndex, info->isForward,
                                info->targetNodeId, info->targetBrowseNameHash);
}

static UA_StatusCode
deleteOneWayReference(UA_Server *server, UA_Session *session, UA_Node *node,
                      const UA_DeleteReferencesItem *item) {
    const UA_Node *refType = UA_NODESTORE_GET(server, &item->referenceTypeId);
    if(!refType)
        return UA_STATUSCODE_BADREFERENCETYPEIDINVALID;
    if(refType->head.nodeClass != UA_NODECLASS_REFERENCETYPE) {
        UA_NODESTORE_RELEASE(server, refType);
        return UA_STATUSCODE_BADREFERENCETYPEIDINVALID;
    }
    UA_Byte refTypeIndex = refType->referenceTypeNode.referenceTypeIndex;
    UA_NODESTORE_RELEASE(server, refType);
    return UA_Node_deleteReference(node, refTypeIndex, item->isForward, &item->targetNodeId);
}

static void
Operation_addReference(UA_Server *server, UA_Session *session, void *context,
                       const UA_AddReferencesItem *item, UA_StatusCode *retval) {
    /* Check access rights */
    if(session != &server->adminSession && server->config.accessControl.allowAddReference) {
        UA_UNLOCK(server->serviceMutex);
        if (!server->config.accessControl.
                allowAddReference(server, &server->config.accessControl,
                                  &session->sessionId, session->sessionHandle, item)) {
            UA_LOCK(server->serviceMutex);
            *retval = UA_STATUSCODE_BADUSERACCESSDENIED;
            return;
        }
        UA_LOCK(server->serviceMutex);
    }

    /* TODO: Currently no expandednodeids are allowed */
    if(item->targetServerUri.length > 0) {
        *retval = UA_STATUSCODE_BADNOTIMPLEMENTED;
        return;
    }

    /* Check the ReferenceType and get the index */
    const UA_Node *refType = UA_NODESTORE_GET(server, &item->referenceTypeId);
    if(!refType) {
        *retval = UA_STATUSCODE_BADREFERENCETYPEIDINVALID;
        return;
    }
    if(refType->head.nodeClass != UA_NODECLASS_REFERENCETYPE) {
        UA_NODESTORE_RELEASE(server, refType);
        *retval = UA_STATUSCODE_BADREFERENCETYPEIDINVALID;
        return;
    }
    UA_Byte refTypeIndex = ((const UA_ReferenceTypeNode*)refType)->referenceTypeIndex;
    UA_NODESTORE_RELEASE(server, refType);

    /* Get the source and target node BrowseName hash */
    const UA_Node *targetNode = UA_NODESTORE_GET(server, &item->targetNodeId.nodeId);
    if(!targetNode) {
        *retval = UA_STATUSCODE_BADTARGETNODEIDINVALID;
        return;
    }
    UA_UInt32 targetNameHash = UA_QualifiedName_hash(&targetNode->head.browseName);
    UA_NODESTORE_RELEASE(server, targetNode);

    const UA_Node *sourceNode = UA_NODESTORE_GET(server, &item->sourceNodeId);
    if(!sourceNode) {
        *retval = UA_STATUSCODE_BADSOURCENODEIDINVALID;
        return;
    }
    UA_UInt32 sourceNameHash = UA_QualifiedName_hash(&sourceNode->head.browseName);
    UA_NODESTORE_RELEASE(server, sourceNode);

    /* Compute the BrowseName hash and release the target */
    struct AddNodeInfo info;
    info.refTypeIndex = refTypeIndex;
    info.targetNodeId = &item->targetNodeId;
    info.isForward = item->isForward;
    info.targetBrowseNameHash = targetNameHash;

    /* Add the first direction */
    *retval = UA_Server_editNode(server, session, &item->sourceNodeId,
                                 (UA_EditNodeCallback)addOneWayReference, &info);
    UA_Boolean firstExisted = false;
    if(*retval == UA_STATUSCODE_BADDUPLICATEREFERENCENOTALLOWED) {
        *retval = UA_STATUSCODE_GOOD;
        firstExisted = true;
    }
    if(*retval != UA_STATUSCODE_GOOD)
        return;

    /* Add the second direction */
    UA_ExpandedNodeId target2;
    UA_ExpandedNodeId_init(&target2);
    target2.nodeId = item->sourceNodeId;
    info.targetNodeId = &target2;
    info.isForward = !info.isForward;
    info.targetBrowseNameHash = sourceNameHash;
    *retval = UA_Server_editNode(server, session, &item->targetNodeId.nodeId,
                                 (UA_EditNodeCallback)addOneWayReference, &info);

    /* Second direction existed already */
    if(*retval == UA_STATUSCODE_BADDUPLICATEREFERENCENOTALLOWED) {
        /* Calculate common duplicate reference not allowed result and set bad
         * result if BOTH directions already existed */
        if(firstExisted) {
            *retval = UA_STATUSCODE_BADDUPLICATEREFERENCENOTALLOWED;
            return;
        }
        *retval = UA_STATUSCODE_GOOD;
    }

    /* Remove first direction if the second direction failed */
    if(*retval != UA_STATUSCODE_GOOD && !firstExisted) {
        UA_DeleteReferencesItem deleteItem;
        deleteItem.sourceNodeId = item->sourceNodeId;
        deleteItem.referenceTypeId = item->referenceTypeId;
        deleteItem.isForward = item->isForward;
        deleteItem.targetNodeId = item->targetNodeId;
        deleteItem.deleteBidirectional = false;
        /* Ignore status code */
        UA_Server_editNode(server, session, &item->sourceNodeId,
                           (UA_EditNodeCallback)deleteOneWayReference, &deleteItem);
    }
}

void
Service_AddReferences(UA_Server *server, UA_Session *session,
                      const UA_AddReferencesRequest *request,
                      UA_AddReferencesResponse *response) {
    UA_LOG_DEBUG_SESSION(&server->config.logger, session,
                         "Processing AddReferencesRequest");
    UA_LOCK_ASSERT(server->serviceMutex, 1);

    if(server->config.maxNodesPerNodeManagement != 0 &&
       request->referencesToAddSize > server->config.maxNodesPerNodeManagement) {
        response->responseHeader.serviceResult = UA_STATUSCODE_BADTOOMANYOPERATIONS;
        return;
    }

    response->responseHeader.serviceResult =
        UA_Server_processServiceOperations(server, session,
                                           (UA_ServiceOperation)Operation_addReference,
                                           NULL, &request->referencesToAddSize,
                                           &UA_TYPES[UA_TYPES_ADDREFERENCESITEM],
                                           &response->resultsSize, &UA_TYPES[UA_TYPES_STATUSCODE]);
}

UA_StatusCode
UA_Server_addReference(UA_Server *server, const UA_NodeId sourceId,
                       const UA_NodeId refTypeId,
                       const UA_ExpandedNodeId targetId,
                       UA_Boolean isForward) {
    UA_AddReferencesItem item;
    UA_AddReferencesItem_init(&item);
    item.sourceNodeId = sourceId;
    item.referenceTypeId = refTypeId;
    item.isForward = isForward;
    item.targetNodeId = targetId;

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_LOCK(server->serviceMutex);
    Operation_addReference(server, &server->adminSession, NULL, &item, &retval);
    UA_UNLOCK(server->serviceMutex)
    return retval;
}

/*********************/
/* Delete References */
/*********************/

static void
Operation_deleteReference(UA_Server *server, UA_Session *session, void *context,
                          const UA_DeleteReferencesItem *item, UA_StatusCode *retval) {
    /* Do not check access for server */
    if(session != &server->adminSession && server->config.accessControl.allowDeleteReference) {
        UA_UNLOCK(server->serviceMutex);
        if (!server->config.accessControl.
                allowDeleteReference(server, &server->config.accessControl,
                                     &session->sessionId, session->sessionHandle, item)){
            UA_LOCK(server->serviceMutex);
            *retval = UA_STATUSCODE_BADUSERACCESSDENIED;
            return;
        }
        UA_LOCK(server->serviceMutex)
    }

    // TODO: Check consistency constraints, remove the references.
    *retval = UA_Server_editNode(server, session, &item->sourceNodeId,
                                 (UA_EditNodeCallback)deleteOneWayReference,
                                 /* cast away const qualifier because callback uses it anyway */
                                 (UA_DeleteReferencesItem *)(uintptr_t)item);
    if(*retval != UA_STATUSCODE_GOOD)
        return;

    if(!item->deleteBidirectional || item->targetNodeId.serverIndex != 0)
        return;

    UA_DeleteReferencesItem secondItem;
    UA_DeleteReferencesItem_init(&secondItem);
    secondItem.isForward = !item->isForward;
    secondItem.sourceNodeId = item->targetNodeId.nodeId;
    secondItem.targetNodeId.nodeId = item->sourceNodeId;
    secondItem.referenceTypeId = item->referenceTypeId;
    *retval = UA_Server_editNode(server, session, &secondItem.sourceNodeId,
                                 (UA_EditNodeCallback)deleteOneWayReference,
                                 &secondItem);
}

void
Service_DeleteReferences(UA_Server *server, UA_Session *session,
                         const UA_DeleteReferencesRequest *request,
                         UA_DeleteReferencesResponse *response) {
    UA_LOG_DEBUG_SESSION(&server->config.logger, session,
                         "Processing DeleteReferencesRequest");
    UA_LOCK_ASSERT(server->serviceMutex, 1);

    if(server->config.maxNodesPerNodeManagement != 0 &&
       request->referencesToDeleteSize > server->config.maxNodesPerNodeManagement) {
        response->responseHeader.serviceResult = UA_STATUSCODE_BADTOOMANYOPERATIONS;
        return;
    }

    response->responseHeader.serviceResult =
        UA_Server_processServiceOperations(server, session,
                                           (UA_ServiceOperation)Operation_deleteReference,
                                           NULL, &request->referencesToDeleteSize,
                                           &UA_TYPES[UA_TYPES_DELETEREFERENCESITEM],
                                           &response->resultsSize, &UA_TYPES[UA_TYPES_STATUSCODE]);
}

UA_StatusCode
UA_Server_deleteReference(UA_Server *server, const UA_NodeId sourceNodeId,
                          const UA_NodeId referenceTypeId, UA_Boolean isForward,
                          const UA_ExpandedNodeId targetNodeId,
                          UA_Boolean deleteBidirectional) {
    UA_DeleteReferencesItem item;
    item.sourceNodeId = sourceNodeId;
    item.referenceTypeId = referenceTypeId;
    item.isForward = isForward;
    item.targetNodeId = targetNodeId;
    item.deleteBidirectional = deleteBidirectional;

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_LOCK(server->serviceMutex);
    Operation_deleteReference(server, &server->adminSession, NULL, &item, &retval);
    UA_UNLOCK(server->serviceMutex);
    return retval;
}

/**********************/
/* Set Value Callback */
/**********************/

static UA_StatusCode
setValueCallback(UA_Server *server, UA_Session *session,
                 UA_VariableNode *node, const UA_ValueCallback *callback) {
    if(node->head.nodeClass != UA_NODECLASS_VARIABLE)
        return UA_STATUSCODE_BADNODECLASSINVALID;
    node->value.data.callback = *callback;
    return UA_STATUSCODE_GOOD;
}



UA_StatusCode
UA_Server_setVariableNode_valueCallback(UA_Server *server,
                                        const UA_NodeId nodeId,
                                        const UA_ValueCallback callback) {
    UA_LOCK(server->serviceMutex);
    UA_StatusCode retval = UA_Server_editNode(server, &server->adminSession, &nodeId,
                                              (UA_EditNodeCallback)setValueCallback,
                                              /* cast away const because callback uses const anyway */
                                              (UA_ValueCallback *)(uintptr_t) &callback);
    UA_UNLOCK(server->serviceMutex);
    return retval;
}

/***************************************************/
/* Special Handling of Variables with Data Sources */
/***************************************************/

UA_StatusCode
UA_Server_addDataSourceVariableNode(UA_Server *server, const UA_NodeId requestedNewNodeId,
                                    const UA_NodeId parentNodeId, const UA_NodeId referenceTypeId,
                                    const UA_QualifiedName browseName, const UA_NodeId typeDefinition,
                                    const UA_VariableAttributes attr, const UA_DataSource dataSource,
                                    void *nodeContext, UA_NodeId *outNewNodeId) {
    UA_AddNodesItem item;
    UA_AddNodesItem_init(&item);
    item.nodeClass = UA_NODECLASS_VARIABLE;
    item.requestedNewNodeId.nodeId = requestedNewNodeId;
    item.browseName = browseName;
    UA_ExpandedNodeId typeDefinitionId;
    UA_ExpandedNodeId_init(&typeDefinitionId);
    typeDefinitionId.nodeId = typeDefinition;
    item.typeDefinition = typeDefinitionId;
    item.nodeAttributes.encoding = UA_EXTENSIONOBJECT_DECODED_NODELETE;
    item.nodeAttributes.content.decoded.data = (void*)(uintptr_t)&attr;
    item.nodeAttributes.content.decoded.type = &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES];
    UA_NodeId newNodeId;
    if(!outNewNodeId) {
        newNodeId = UA_NODEID_NULL;
        outNewNodeId = &newNodeId;
    }

    UA_LOCK(server->serviceMutex);
    /* Create the node and add it to the nodestore */
    UA_StatusCode retval = AddNode_raw(server, &server->adminSession, nodeContext,
                                       &item, outNewNodeId);
    if(retval != UA_STATUSCODE_GOOD)
        goto cleanup;

    /* Set the data source */
    retval = setVariableNode_dataSource(server, *outNewNodeId, dataSource);
    if(retval != UA_STATUSCODE_GOOD)
        goto cleanup;

    /* Typecheck and add references to parent and type definition */
    retval = AddNode_addRefs(server, &server->adminSession, outNewNodeId, &parentNodeId,
                             &referenceTypeId, &typeDefinition);
    if(retval != UA_STATUSCODE_GOOD)
        goto cleanup;

    /* Call the constructors */
    retval = AddNode_finish(server, &server->adminSession, outNewNodeId);

 cleanup:
    UA_UNLOCK(server->serviceMutex);
    if(outNewNodeId == &newNodeId)
        UA_NodeId_clear(&newNodeId);

    return retval;
}

static UA_StatusCode
setDataSource(UA_Server *server, UA_Session *session,
              UA_VariableNode *node, const UA_DataSource *dataSource) {
    if(node->head.nodeClass != UA_NODECLASS_VARIABLE)
        return UA_STATUSCODE_BADNODECLASSINVALID;
    if(node->valueSource == UA_VALUESOURCE_DATA)
        UA_DataValue_clear(&node->value.data.value);
    node->value.dataSource = *dataSource;
    node->valueSource = UA_VALUESOURCE_DATASOURCE;
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
setVariableNode_dataSource(UA_Server *server, const UA_NodeId nodeId,
                                     const UA_DataSource dataSource) {
    UA_LOCK_ASSERT(server->serviceMutex, 1);
    return UA_Server_editNode(server, &server->adminSession, &nodeId,
                              (UA_EditNodeCallback)setDataSource,
                              /* casting away const because callback casts it back anyway */
                              (UA_DataSource *) (uintptr_t)&dataSource);
}

UA_StatusCode
UA_Server_setVariableNode_dataSource(UA_Server *server, const UA_NodeId nodeId,
                                     const UA_DataSource dataSource) {
    UA_LOCK(server->serviceMutex);
    UA_StatusCode retval = setVariableNode_dataSource(server, nodeId, dataSource);
    UA_UNLOCK(server->serviceMutex);
    return retval;
}

/******************************/
/* Set External Value Source  */
/******************************/
static UA_StatusCode
setExternalValueSource(UA_Server *server, UA_Session *session,
                 UA_VariableNode *node, const UA_ValueBackend *externalValueSource) {
    if(node->nodeClass != UA_NODECLASS_VARIABLE)
        return UA_STATUSCODE_BADNODECLASSINVALID;
    node->valueBackend.backendType = UA_VALUEBACKENDTYPE_EXTERNAL;
    node->valueBackend.backend.external.value = externalValueSource->backend.external.value;
    node->valueBackend.backend.external.callback.onWrite = externalValueSource->backend.external.callback.onWrite;
    node->valueBackend.backend.external.callback.onRead = externalValueSource->backend.external.callback.onRead;
    return UA_STATUSCODE_GOOD;
}

/**********************/
/* Set Value Backend  */
/**********************/

UA_StatusCode
UA_Server_setVariableNode_valueBackend(UA_Server *server, const UA_NodeId nodeId,
                                       const UA_ValueBackend valueBackend){
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_LOCK(server->serviceMutex);
    switch(valueBackend.backendType){
        case UA_VALUEBACKENDTYPE_NONE:
            return UA_STATUSCODE_BADCONFIGURATIONERROR;
        case UA_VALUEBACKENDTYPE_CALLBACK:
            retval = UA_Server_editNode(server, &server->adminSession, &nodeId,
                                        (UA_EditNodeCallback) setValueCallback,
                /* cast away const because callback uses const anyway */
                                        (UA_ValueCallback *)(uintptr_t) &valueBackend.backend.dataSource);
            break;
        case UA_VALUEBACKENDTYPE_INTERNAL:
            break;
        case UA_VALUEBACKENDTYPE_EXTERNAL:
            retval = UA_Server_editNode(server, &server->adminSession, &nodeId,
                                        (UA_EditNodeCallback) setExternalValueSource,
                /* cast away const because callback uses const anyway */
                                        (UA_ValueCallback *)(uintptr_t) &valueBackend);
            break;
    }


    // UA_StatusCode retval = UA_Server_editNode(server, &server->adminSession, &nodeId,
    // (UA_EditNodeCallback)setValueCallback,
    /* cast away const because callback uses const anyway */
    // (UA_ValueCallback *)(uintptr_t) &callback);


    UA_UNLOCK(server->serviceMutex);
    return retval;
}


/************************************/
/* Special Handling of Method Nodes */
/************************************/

#ifdef UA_ENABLE_METHODCALLS

static const UA_NodeId hasproperty = {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_HASPROPERTY}};
static const UA_NodeId propertytype = {0, UA_NODEIDTYPE_NUMERIC, {UA_NS0ID_PROPERTYTYPE}};

static UA_StatusCode
UA_Server_addMethodNodeEx_finish(UA_Server *server, const UA_NodeId nodeId, UA_MethodCallback method,
                                 const size_t inputArgumentsSize, const UA_Argument *inputArguments,
                                 const UA_NodeId inputArgumentsRequestedNewNodeId,
                                 UA_NodeId *inputArgumentsOutNewNodeId,
                                 const size_t outputArgumentsSize, const UA_Argument *outputArguments,
                                 const UA_NodeId outputArgumentsRequestedNewNodeId,
                                 UA_NodeId *outputArgumentsOutNewNodeId) {
    /* Browse to see which argument nodes exist */
    UA_BrowseDescription bd;
    UA_BrowseDescription_init(&bd);
    bd.nodeId = nodeId;
    bd.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
    bd.includeSubtypes = false;
    bd.browseDirection = UA_BROWSEDIRECTION_FORWARD;
    bd.nodeClassMask = UA_NODECLASS_VARIABLE;
    bd.resultMask = UA_BROWSERESULTMASK_BROWSENAME;

    UA_BrowseResult br;
    UA_BrowseResult_init(&br);
    UA_UInt32 maxrefs = 0;
    Operation_Browse(server, &server->adminSession, &maxrefs, &bd, &br);

    UA_StatusCode retval = br.statusCode;
    if(retval != UA_STATUSCODE_GOOD) {
        deleteNode(server, nodeId, true);
        UA_BrowseResult_clear(&br);
        return retval;
    }

    /* Filter out the argument nodes */
    UA_NodeId inputArgsId = UA_NODEID_NULL;
    UA_NodeId outputArgsId = UA_NODEID_NULL;
    const UA_QualifiedName inputArgsName = UA_QUALIFIEDNAME(0, "InputArguments");
    const UA_QualifiedName outputArgsName = UA_QUALIFIEDNAME(0, "OutputArguments");
    for(size_t i = 0; i < br.referencesSize; i++) {
        UA_ReferenceDescription *rd = &br.references[i];
        if(rd->browseName.namespaceIndex == 0 &&
           UA_String_equal(&rd->browseName.name, &inputArgsName.name))
            inputArgsId = rd->nodeId.nodeId;
        else if(rd->browseName.namespaceIndex == 0 &&
                UA_String_equal(&rd->browseName.name, &outputArgsName.name))
            outputArgsId = rd->nodeId.nodeId;
    }

    /* Add the Input Arguments VariableNode */
    if(inputArgumentsSize > 0 && UA_NodeId_isNull(&inputArgsId)) {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        char *name = "InputArguments";
        attr.displayName = UA_LOCALIZEDTEXT("", name);
        attr.dataType = UA_TYPES[UA_TYPES_ARGUMENT].typeId;
        attr.valueRank = UA_VALUERANK_ONE_DIMENSION;
        UA_UInt32 inputArgsSize32 = (UA_UInt32)inputArgumentsSize;
        attr.arrayDimensions = &inputArgsSize32;
        attr.arrayDimensionsSize = 1;
        UA_Variant_setArray(&attr.value, (void *)(uintptr_t)inputArguments,
                            inputArgumentsSize, &UA_TYPES[UA_TYPES_ARGUMENT]);
        retval = addNode(server, UA_NODECLASS_VARIABLE, &inputArgumentsRequestedNewNodeId,
                         &nodeId, &hasproperty, UA_QUALIFIEDNAME(0, name),
                         &propertytype, (const UA_NodeAttributes*)&attr,
                         &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],
                         NULL, &inputArgsId);
        if(retval != UA_STATUSCODE_GOOD)
            goto error;
    }

    /* Add the Output Arguments VariableNode */
    if(outputArgumentsSize > 0 && UA_NodeId_isNull(&outputArgsId)) {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        char *name = "OutputArguments";
        attr.displayName = UA_LOCALIZEDTEXT("", name);
        attr.dataType = UA_TYPES[UA_TYPES_ARGUMENT].typeId;
        attr.valueRank = UA_VALUERANK_ONE_DIMENSION;
        UA_UInt32 outputArgsSize32 = (UA_UInt32)outputArgumentsSize;
        attr.arrayDimensions = &outputArgsSize32;
        attr.arrayDimensionsSize = 1;
        UA_Variant_setArray(&attr.value, (void *)(uintptr_t)outputArguments,
                            outputArgumentsSize, &UA_TYPES[UA_TYPES_ARGUMENT]);
        retval = addNode(server, UA_NODECLASS_VARIABLE, &outputArgumentsRequestedNewNodeId,
                         &nodeId, &hasproperty, UA_QUALIFIEDNAME(0, name),
                         &propertytype, (const UA_NodeAttributes*)&attr,
                         &UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES],
                         NULL, &outputArgsId);
        if(retval != UA_STATUSCODE_GOOD)
            goto error;
    }

    retval = setMethodNode_callback(server, nodeId, method);
    if(retval != UA_STATUSCODE_GOOD)
        goto error;

    /* Call finish to add the parent reference */
    retval = AddNode_finish(server, &server->adminSession, &nodeId);
    if(retval != UA_STATUSCODE_GOOD)
        goto error;

    if(inputArgumentsOutNewNodeId != NULL) {
        UA_NodeId_copy(&inputArgsId, inputArgumentsOutNewNodeId);
    }
    if(outputArgumentsOutNewNodeId != NULL) {
        UA_NodeId_copy(&outputArgsId, outputArgumentsOutNewNodeId);
    }
    UA_BrowseResult_clear(&br);
    return retval;

error:
    deleteNode(server, nodeId, true);
    deleteNode(server, inputArgsId, true);
    deleteNode(server, outputArgsId, true);
    UA_BrowseResult_clear(&br);
    return retval;
}

UA_StatusCode
UA_Server_addMethodNode_finish(UA_Server *server, const UA_NodeId nodeId,
                               UA_MethodCallback method,
                               size_t inputArgumentsSize, const UA_Argument* inputArguments,
                               size_t outputArgumentsSize, const UA_Argument* outputArguments) {
    UA_LOCK(server->serviceMutex)
    UA_StatusCode retval = UA_Server_addMethodNodeEx_finish(server, nodeId, method,
                                            inputArgumentsSize, inputArguments, UA_NODEID_NULL, NULL,
                                            outputArgumentsSize, outputArguments, UA_NODEID_NULL, NULL);
    UA_UNLOCK(server->serviceMutex)
    return retval;
}

UA_StatusCode
UA_Server_addMethodNodeEx(UA_Server *server, const UA_NodeId requestedNewNodeId,
                          const UA_NodeId parentNodeId,
                          const UA_NodeId referenceTypeId,
                          const UA_QualifiedName browseName,
                          const UA_MethodAttributes attr, UA_MethodCallback method,
                          size_t inputArgumentsSize, const UA_Argument *inputArguments,
                          const UA_NodeId inputArgumentsRequestedNewNodeId,
                          UA_NodeId *inputArgumentsOutNewNodeId,
                          size_t outputArgumentsSize, const UA_Argument *outputArguments,
                          const UA_NodeId outputArgumentsRequestedNewNodeId,
                          UA_NodeId *outputArgumentsOutNewNodeId,
                          void *nodeContext, UA_NodeId *outNewNodeId) {
    UA_AddNodesItem item;
    UA_AddNodesItem_init(&item);
    item.nodeClass = UA_NODECLASS_METHOD;
    item.requestedNewNodeId.nodeId = requestedNewNodeId;
    item.browseName = browseName;
    item.nodeAttributes.encoding = UA_EXTENSIONOBJECT_DECODED_NODELETE;
    item.nodeAttributes.content.decoded.data = (void*)(uintptr_t)&attr;
    item.nodeAttributes.content.decoded.type = &UA_TYPES[UA_TYPES_METHODATTRIBUTES];

    UA_NodeId newId;
    if(!outNewNodeId) {
        UA_NodeId_init(&newId);
        outNewNodeId = &newId;
    }
    UA_LOCK(server->serviceMutex);
    UA_StatusCode retval = Operation_addNode_begin(server, &server->adminSession,
                                                   nodeContext, &item, &parentNodeId,
                                                   &referenceTypeId, outNewNodeId);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_UNLOCK(server->serviceMutex);
        return retval;
    }

    retval = UA_Server_addMethodNodeEx_finish(server, *outNewNodeId, method,
                                              inputArgumentsSize, inputArguments,
                                              inputArgumentsRequestedNewNodeId,
                                              inputArgumentsOutNewNodeId,
                                              outputArgumentsSize, outputArguments,
                                              outputArgumentsRequestedNewNodeId,
                                              outputArgumentsOutNewNodeId);
    UA_UNLOCK(server->serviceMutex);
    if(outNewNodeId == &newId)
        UA_NodeId_clear(&newId);
    return retval;
}

static UA_StatusCode
editMethodCallback(UA_Server *server, UA_Session* session,
                   UA_Node *node, UA_MethodCallback methodCallback) {
    if(node->head.nodeClass != UA_NODECLASS_METHOD)
        return UA_STATUSCODE_BADNODECLASSINVALID;
    node->methodNode.method = methodCallback;
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
setMethodNode_callback(UA_Server *server,
                                 const UA_NodeId methodNodeId,
                                 UA_MethodCallback methodCallback) {
    UA_LOCK_ASSERT(server->serviceMutex, 1);
    return UA_Server_editNode(server, &server->adminSession, &methodNodeId,
                                              (UA_EditNodeCallback)editMethodCallback,
                                              (void*)(uintptr_t)methodCallback);
}

UA_StatusCode
UA_Server_setMethodNode_callback(UA_Server *server,
                                 const UA_NodeId methodNodeId,
                                 UA_MethodCallback methodCallback) {
    UA_LOCK(server->serviceMutex);
    UA_StatusCode retVal = setMethodNode_callback(server, methodNodeId, methodCallback);
    UA_UNLOCK(server->serviceMutex);
    return retVal;
}

#endif

/************************/
/* Lifecycle Management */
/************************/

void UA_EXPORT
UA_Server_setAdminSessionContext(UA_Server *server,
                                 void *context) {
    server->adminSession.sessionHandle = context;
}

static UA_StatusCode
setNodeTypeLifecycle(UA_Server *server, UA_Session *session,
                     UA_Node *node, UA_NodeTypeLifecycle *lifecycle) {
    if(node->head.nodeClass == UA_NODECLASS_OBJECTTYPE) {
        node->objectTypeNode.lifecycle = *lifecycle;
    } else if(node->head.nodeClass == UA_NODECLASS_VARIABLETYPE) {
        node->variableTypeNode.lifecycle = *lifecycle;
    } else {
        return UA_STATUSCODE_BADNODECLASSINVALID;
    }
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_Server_setNodeTypeLifecycle(UA_Server *server, UA_NodeId nodeId,
                               UA_NodeTypeLifecycle lifecycle) {
    UA_LOCK(server->serviceMutex);
    UA_StatusCode retval = UA_Server_editNode(server, &server->adminSession, &nodeId,
                                             (UA_EditNodeCallback)setNodeTypeLifecycle,
                                              &lifecycle);
    UA_UNLOCK(server->serviceMutex);
    return retval;
}
