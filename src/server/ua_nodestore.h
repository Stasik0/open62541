/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef UA_NODESTORE_H_
#define UA_NODESTORE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_nodestore_interface.h"
#include "ua_nodes.h"

/**
 * Nodestore
 * ---------
 * Stores nodes that can be indexed by their NodeId. Internally, it is based on
 * a hash-map implementation. */
struct UA_NodeStore;
typedef struct UA_NodeStore UA_NodeStore;

/**
 * Nodestore Lifecycle
 * ^^^^^^^^^^^^^^^^^^^ */
/* Create a new nodestore */
UA_NodeStore * UA_NodeStore_new(void);

/* Delete the nodestore and all nodes in it. Do not call from a read-side
   critical section (multithreading). */
void UA_NodeStore_delete(UA_NodeStore *ns, UA_UInt16 namespaceIndex);

/* Link a namespace index to this nodestore*/
UA_StatusCode UA_NodeStore_linkNamespace(UA_NodeStore *ns, UA_UInt16 namespaceIndex);

UA_StatusCode UA_NodeStore_unlinkNamespace(UA_NodeStore *ns, UA_UInt16 namespaceIndex);

/**
 * Node Lifecycle
 * ^^^^^^^^^^^^^^
 *
 * The following definitions are used to create empty nodes of the different
 * node types. The memory is managed by the nodestore. Therefore, the node has
 * to be removed via a special deleteNode function. (If the new node is not
 * added to the nodestore.) */
/* Create an editable node of the given NodeClass. */
UA_Node * UA_NodeStore_newNode(UA_NodeClass nodeClass);

/* Delete an editable node. */
void UA_NodeStore_deleteNode(UA_Node *node);

/**
 * Insert / Get / Replace / Remove
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
/* Inserts a new node into the nodestore. If the nodeid is zero, then a fresh
 * numeric nodeid from namespace 1 is assigned. If insertion fails, the node is
 * deleted. */
UA_StatusCode UA_NodeStore_insert(UA_NodeStore *ns, UA_Node *node, UA_NodeId *addedNodeId);

/* The returned node is immutable. */
const UA_Node * UA_NodeStore_get(UA_NodeStore *ns, const UA_NodeId *nodeid);

/* Returns an editable copy of a node (needs to be deleted with the deleteNode
   function or inserted / replaced into the nodestore). */
UA_Node * UA_NodeStore_getCopy(UA_NodeStore *ns, const UA_NodeId *nodeid);

/* To replace a node, get an editable copy of the node, edit and replace with
 * this function. If the node was already replaced since the copy was made,
 * UA_STATUSCODE_BADINTERNALERROR is returned. If the nodeid is not found,
 * UA_STATUSCODE_BADNODEIDUNKNOWN is returned. In both error cases, the editable
 * node is deleted. */
UA_StatusCode UA_NodeStore_replace(UA_NodeStore *ns, UA_Node *node);

/* Remove a node in the nodestore. */
UA_StatusCode UA_NodeStore_remove(UA_NodeStore *ns, const UA_NodeId *nodeid);

/**
 * Iteration
 * ^^^^^^^^^
 * The following definitions are used to call a callback for every node in the
 * nodestore. */
void UA_NodeStore_iterate(UA_NodeStore *ns, void *visitorHandle, UA_NodestoreInterface_nodeVisitor visitor);

/**
 * Release
 * ^^^^^^^^
 * Only used in nodestore_concurrent. Indicates that a node is no longer referenced by the caller.
 */
void UA_NodeStore_release(UA_NodeStore *ns, const UA_Node *node);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* UA_NODESTORE_H_ */
