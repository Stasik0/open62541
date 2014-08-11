#include "ua_namespace.h"

#include <urcu.h>
#include <urcu/compiler.h> // for caa_container_of
#include <urcu/uatomic.h>
#include <urcu/rculfhash.h>


#define ALIVE_BIT (1 << 15) /* Alive bit in the readcount */
typedef struct Namespace_Entry {
	struct cds_lfht_node htn; /* contains next-ptr for urcu-hashmap */
	struct rcu_head rcu_head; /* For call-rcu */
	UA_UInt16 readcount;      /* Counts the amount of readers on it [alive-bit, 15 counter-bits] */
	UA_Node   node;           /* Might be cast from any _bigger_ UA_Node* type. Allocate enough memory! */
} Namespace_Entry;

struct Namespace {
	UA_UInt32       namespaceId;
	struct cds_lfht *ht; /* Hash table */
};

/********/
/* Hash */
/********/

typedef UA_UInt32 hash_t;

/* Based on Murmur-Hash 3 by Austin Appleby (public domain, freely usable) */
static inline hash_t hash_array(const UA_Byte *data, UA_UInt32 len) {
	static const uint32_t c1 = 0xcc9e2d51;
	static const uint32_t c2 = 0x1b873593;
	static const uint32_t r1 = 15;
	static const uint32_t r2 = 13;
	static const uint32_t m  = 5;
	static const uint32_t n  = 0xe6546b64;
	hash_t hash = len;

	if(data == UA_NULL) return 0;

	const int32_t   nblocks = len / 4;
	const uint32_t *blocks  = (const uint32_t *)data;
	for(int32_t i = 0;i < nblocks;i++) {
		uint32_t k = blocks[i];
		k    *= c1;
		k     = (k << r1) | (k >> (32 - r1));
		k    *= c2;
		hash ^= k;
		hash  = ((hash << r2) | (hash >> (32 - r2))) * m + n;
	}

	const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);
	uint32_t       k1   = 0;

	switch(len & 3) {
	case 3:
		k1 ^= tail[2] << 16;

	case 2:
		k1 ^= tail[1] << 8;

	case 1:
		k1   ^= tail[0];
		k1   *= c1;
		k1    = (k1 << r1) | (k1 >> (32 - r1));
		k1   *= c2;
		hash ^= k1;
	}

	hash ^= len;
	hash ^= (hash >> 16);
	hash *= 0x85ebca6b;
	hash ^= (hash >> 13);
	hash *= 0xc2b2ae35;
	hash ^= (hash >> 16);

	return hash;
}

static inline hash_t hash(const UA_NodeId *n) {
	switch(n->encodingByte) {
	case UA_NODEIDTYPE_TWOBYTE:
	case UA_NODEIDTYPE_FOURBYTE:
	case UA_NODEIDTYPE_NUMERIC:
		/*  Knuth's multiplicative hashing */
		return n->identifier.numeric * 2654435761;   // mod(2^32) is implicit

	case UA_NODEIDTYPE_STRING:
		return hash_array(n->identifier.string.data, n->identifier.string.length);

	case UA_NODEIDTYPE_GUID:
		return hash_array((UA_Byte *)&(n->identifier.guid), sizeof(UA_Guid));

	case UA_NODEIDTYPE_BYTESTRING:
		return hash_array((UA_Byte *)n->identifier.byteString.data, n->identifier.byteString.length);

	default:
		return 0;
	}
}

/*************/
/* Namespace */
/*************/

static inline void node_deleteMembers(const UA_Node *node) {
	switch(node->nodeClass) {
	case UA_NODECLASS_OBJECT:
		UA_ObjectNode_deleteMembers((UA_ObjectNode *)node);
		break;

	case UA_NODECLASS_VARIABLE:
		UA_VariableNode_deleteMembers((UA_VariableNode *)node);
		break;

	case UA_NODECLASS_METHOD:
		UA_MethodNode_deleteMembers((UA_MethodNode *)node);
		break;

	case UA_NODECLASS_OBJECTTYPE:
		UA_ObjectTypeNode_deleteMembers((UA_ObjectTypeNode *)node);
		break;

	case UA_NODECLASS_VARIABLETYPE:
		UA_VariableTypeNode_deleteMembers((UA_VariableTypeNode *)node);
		break;

	case UA_NODECLASS_REFERENCETYPE:
		UA_ReferenceTypeNode_deleteMembers((UA_ReferenceTypeNode *)node);
		break;

	case UA_NODECLASS_DATATYPE:
		UA_DataTypeNode_deleteMembers((UA_DataTypeNode *)node);
		break;

	case UA_NODECLASS_VIEW:
		UA_ViewNode_deleteMembers((UA_ViewNode *)node);
		break;

	default:
		break;
	}
}

/* We are in a rcu_read lock. So the node will not be freed under our feet. */
static int compare(struct cds_lfht_node *htn, const void *orig) {
	UA_NodeId *origid = (UA_NodeId*)orig;
	UA_NodeId   *newid  = &((Namespace_Entry *)htn)->node.nodeId; /* The htn is first in the entry structure. */

	return UA_NodeId_equal(newid, origid) == UA_EQUAL;
}

/* The entry was removed from the hashtable. No more readers can get it. Since
   all readers using the node for a longer time (outside the rcu critical
   section) increased the readcount, we only need to wait for the readcount
   to reach zero. */
static void markDead(struct rcu_head *head) {
	Namespace_Entry *entry = caa_container_of(head, Namespace_Entry, rcu_head);
	if(uatomic_sub_return(&entry->readcount, ALIVE_BIT) > 0)
		return;

	node_deleteMembers(&entry->node);
	UA_free(entry);
	return;
}

/* Free the entry if it is dead and nobody uses it anymore */
void Namespace_releaseManagedNode(const UA_Node *managed) {
	if(managed == UA_NULL)
		return;
	
	Namespace_Entry *entry = caa_container_of(managed, Namespace_Entry, node); // pointer to the first entry
	if(uatomic_sub_return(&entry->readcount, 1) > 0)
		return;

	node_deleteMembers(managed);
	UA_free(entry);
	return;
}

UA_Int32 Namespace_new(Namespace **result, UA_UInt32 namespaceId) {
	Namespace *ns;
	if(UA_alloc((void **)&ns, sizeof(Namespace)) != UA_SUCCESS)
		return UA_ERR_NO_MEMORY;

	/* 32 is the minimum size for the hashtable. */
	ns->ht = cds_lfht_new(32, 32, 0, CDS_LFHT_AUTO_RESIZE, NULL);
	if(!ns->ht) {
		UA_free(ns);
		return UA_ERR_NO_MEMORY;
	}

	ns->namespaceId = namespaceId;
	*result = ns;
	return UA_SUCCESS;
}

UA_Int32 Namespace_delete(Namespace *ns) {
	if(ns == UA_NULL)
		return UA_ERROR;

	struct cds_lfht *ht = ns->ht;
	struct cds_lfht_iter iter;
	struct cds_lfht_node *found_htn;

	rcu_read_lock();
	cds_lfht_first(ht, &iter);
	while(iter.node != UA_NULL) {
		found_htn = cds_lfht_iter_get_node(&iter);
		if(!cds_lfht_del(ht, found_htn)) {
			Namespace_Entry *entry = caa_container_of(found_htn, Namespace_Entry, htn);
			call_rcu(&entry->rcu_head, markDead);
		}
		cds_lfht_next(ht, &iter);
	}
	rcu_read_unlock();

	if(!cds_lfht_destroy(ht, UA_NULL)) {
		UA_free(ns);
		return UA_SUCCESS;
	}
	else
		return UA_ERROR;
}

UA_Int32 Namespace_insert(Namespace *ns, UA_Node **node, UA_Byte flags) {
	if(ns == UA_NULL || node == UA_NULL || *node == UA_NULL || (*node)->nodeId.namespace != ns->namespaceId)
		return UA_ERROR;

	UA_UInt32 nodesize;
	/* Copy the node into the entry. Then reset the original node. It shall no longer be used. */
	switch((*node)->nodeClass) {
	case UA_NODECLASS_OBJECT:
		nodesize = sizeof(UA_ObjectNode);
		break;

	case UA_NODECLASS_VARIABLE:
		nodesize = sizeof(UA_VariableNode);
		break;

	case UA_NODECLASS_METHOD:
		nodesize = sizeof(UA_MethodNode);
		break;

	case UA_NODECLASS_OBJECTTYPE:
		nodesize = sizeof(UA_ObjectTypeNode);
		break;

	case UA_NODECLASS_VARIABLETYPE:
		nodesize = sizeof(UA_VariableTypeNode);
		break;

	case UA_NODECLASS_REFERENCETYPE:
		nodesize = sizeof(UA_ReferenceTypeNode);
		break;

	case UA_NODECLASS_DATATYPE:
		nodesize = sizeof(UA_DataTypeNode);
		break;

	case UA_NODECLASS_VIEW:
		nodesize = sizeof(UA_ViewNode);
		break;

	default:
		return UA_ERROR;
	}

	Namespace_Entry *entry;
	if(UA_alloc((void **)&entry, sizeof(Namespace_Entry) - sizeof(UA_Node) + nodesize))
		return UA_ERR_NO_MEMORY;
	memcpy(&entry->node, *node, nodesize);

	cds_lfht_node_init(&entry->htn);
	entry->readcount = ALIVE_BIT;
	if(flags & NAMESPACE_INSERT_GETMANAGED)
		entry->readcount++;

	hash_t nhash = hash(&(*node)->nodeId);
	struct cds_lfht_node *result;
	if(flags & NAMESPACE_INSERT_UNIQUE) {
		rcu_read_lock();
		result = cds_lfht_add_unique(ns->ht, nhash, compare, &entry->node.nodeId, &entry->htn);
		rcu_read_unlock();

		/* If the nodeid exists already */
		if(result != &entry->htn) {
			UA_free(entry);
			return UA_ERROR;     // TODO: define a UA_EXISTS_ALREADY
		}
	} else {
		rcu_read_lock();
		result = cds_lfht_add_replace(ns->ht, nhash, compare, &(*node)->nodeId, &entry->htn);
		/* If an entry got replaced, mark it as dead. */
		if(result) {
			Namespace_Entry *entry = caa_container_of(result, Namespace_Entry, htn);
			call_rcu(&entry->rcu_head, markDead);      /* Queue this for the next time when no readers are on the entry.*/
		}
		rcu_read_unlock();
	}

	UA_free((UA_Node*)*node);     /* The old node is replaced by a managed node. */
	if(flags & NAMESPACE_INSERT_GETMANAGED)
		*node = &entry->node;
	else
		*node = UA_NULL;

	return UA_SUCCESS;
}

UA_Int32 Namespace_remove(Namespace *ns, const UA_NodeId *nodeid) {
	hash_t nhash = hash(nodeid);
	struct cds_lfht_iter iter;

	rcu_read_lock();
	cds_lfht_lookup(ns->ht, nhash, compare, &nodeid, &iter);
	struct cds_lfht_node *found_htn = cds_lfht_iter_get_node(&iter);

	/* If this fails, then the node has already been removed. */
	if(!found_htn || cds_lfht_del(ns->ht, found_htn) != 0) {
		rcu_read_unlock();
		return UA_ERROR;
	}
	
	Namespace_Entry *entry = caa_container_of(found_htn, Namespace_Entry, htn);
	call_rcu(&entry->rcu_head, markDead);
	rcu_read_unlock();

	return UA_SUCCESS;
}

UA_Int32 Namespace_get(const Namespace *ns, const UA_NodeId *nodeid, const UA_Node **managedNode) {
	hash_t nhash     = hash(nodeid);
	struct cds_lfht_iter iter;

	rcu_read_lock();
	cds_lfht_lookup(ns->ht, nhash, compare, nodeid, &iter);
	Namespace_Entry *found_entry = (Namespace_Entry *)cds_lfht_iter_get_node(&iter);

	if(!found_entry) {
		rcu_read_unlock();
		return UA_ERROR;  // TODO: UA_NOTFOUND
	}

	/* This is done within a read-lock. The node will not be marked dead within a read-lock. */
	uatomic_inc(&found_entry->readcount);
	rcu_read_unlock();

	*managedNode = &found_entry->node;
	return UA_SUCCESS;
}

UA_Int32 Namespace_iterate(const Namespace *ns, Namespace_nodeVisitor visitor) {
	if(ns == UA_NULL || visitor == UA_NULL)
		return UA_ERROR;
	
	struct cds_lfht *ht = ns->ht;
	struct cds_lfht_iter iter;

	rcu_read_lock();
	cds_lfht_first(ht, &iter);
	while(iter.node != UA_NULL) {
		Namespace_Entry *found_entry = (Namespace_Entry *)cds_lfht_iter_get_node(&iter);
		uatomic_inc(&found_entry->readcount);
		const UA_Node *node = &found_entry->node;
		rcu_read_unlock();
		visitor(node);
		Namespace_releaseManagedNode((UA_Node *)node);
		rcu_read_lock();
		cds_lfht_next(ht, &iter);
	}
	rcu_read_unlock();

	return UA_SUCCESS;
}
