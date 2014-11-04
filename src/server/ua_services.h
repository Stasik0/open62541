#ifndef UA_SERVICES_H_
#define UA_SERVICES_H_

#include "ua_types.h"
#include "ua_types_generated.h"
#include "ua_server.h"
#include "ua_session.h"


enum UA_AttributeId {
    UA_ATTRIBUTEID_NODEID                  = 1,
    UA_ATTRIBUTEID_NODECLASS               = 2,
    UA_ATTRIBUTEID_BROWSENAME              = 3,
    UA_ATTRIBUTEID_DISPLAYNAME             = 4,
    UA_ATTRIBUTEID_DESCRIPTION             = 5,
    UA_ATTRIBUTEID_WRITEMASK               = 6,
    UA_ATTRIBUTEID_USERWRITEMASK           = 7,
    UA_ATTRIBUTEID_ISABSTRACT              = 8,
    UA_ATTRIBUTEID_SYMMETRIC               = 9,
    UA_ATTRIBUTEID_INVERSENAME             = 10,
    UA_ATTRIBUTEID_CONTAINSNOLOOPS         = 11,
    UA_ATTRIBUTEID_EVENTNOTIFIER           = 12,
    UA_ATTRIBUTEID_VALUE                   = 13,
    UA_ATTRIBUTEID_DATATYPE                = 14,
    UA_ATTRIBUTEID_VALUERANK               = 15,
    UA_ATTRIBUTEID_ARRAYDIMENSIONS         = 16,
    UA_ATTRIBUTEID_ACCESSLEVEL             = 17,
    UA_ATTRIBUTEID_USERACCESSLEVEL         = 18,
    UA_ATTRIBUTEID_MINIMUMSAMPLINGINTERVAL = 19,
    UA_ATTRIBUTEID_HISTORIZING             = 20,
    UA_ATTRIBUTEID_EXECUTABLE              = 21,
    UA_ATTRIBUTEID_USEREXECUTABLE          = 22
};
/*
UA_ExpandedNodeId RefTypeId_References; NS0EXPANDEDNODEID(RefTypeId_References, 31);
UA_ExpandedNodeId RefTypeId_NonHierarchicalReferences; NS0EXPANDEDNODEID(RefTypeId_NonHierarchicalReferences, 32);
UA_ExpandedNodeId RefTypeId_HierarchicalReferences; NS0EXPANDEDNODEID(RefTypeId_HierarchicalReferences, 33);
UA_ExpandedNodeId RefTypeId_HasChild; NS0EXPANDEDNODEID(RefTypeId_HasChild, 34);
UA_ExpandedNodeId RefTypeId_Organizes; NS0EXPANDEDNODEID(RefTypeId_Organizes, 35);
UA_ExpandedNodeId RefTypeId_HasEventSource; NS0EXPANDEDNODEID(RefTypeId_HasEventSource, 36);
UA_ExpandedNodeId RefTypeId_HasModellingRule; NS0EXPANDEDNODEID(RefTypeId_HasModellingRule, 37);
UA_ExpandedNodeId RefTypeId_HasEncoding; NS0EXPANDEDNODEID(RefTypeId_HasEncoding, 38);
UA_ExpandedNodeId RefTypeId_HasDescription; NS0EXPANDEDNODEID(RefTypeId_HasDescription, 39);
UA_ExpandedNodeId RefTypeId_HasTypeDefinition; NS0EXPANDEDNODEID(RefTypeId_HasTypeDefinition, 40);
UA_ExpandedNodeId RefTypeId_GeneratesEvent; NS0EXPANDEDNODEID(RefTypeId_GeneratesEvent, 41);
UA_ExpandedNodeId RefTypeId_Aggregates; NS0EXPANDEDNODEID(RefTypeId_Aggregates, 44);
UA_ExpandedNodeId RefTypeId_HasSubtype; NS0EXPANDEDNODEID(RefTypeId_HasSubtype, 45);
UA_ExpandedNodeId RefTypeId_HasProperty; NS0EXPANDEDNODEID(RefTypeId_HasProperty, 46);
UA_ExpandedNodeId RefTypeId_HasComponent; NS0EXPANDEDNODEID(RefTypeId_HasComponent, 47);
UA_ExpandedNodeId RefTypeId_HasNotifier; NS0EXPANDEDNODEID(RefTypeId_HasNotifier, 48);
UA_ExpandedNodeId RefTypeId_HasOrderedComponent; NS0EXPANDEDNODEID(RefTypeId_HasOrderedComponent, 49);
UA_ExpandedNodeId RefTypeId_HasModelParent; NS0EXPANDEDNODEID(RefTypeId_HasModelParent, 50);
UA_ExpandedNodeId RefTypeId_FromState; NS0EXPANDEDNODEID(RefTypeId_FromState, 51);
UA_ExpandedNodeId RefTypeId_ToState; NS0EXPANDEDNODEID(RefTypeId_ToState, 52);
UA_ExpandedNodeId RefTypeId_HasCause; NS0EXPANDEDNODEID(RefTypeId_HasCause, 53);
UA_ExpandedNodeId RefTypeId_HasEffect; NS0EXPANDEDNODEID(RefTypeId_HasEffect, 54);
UA_ExpandedNodeId RefTypeId_HasHistoricalConfiguration; NS0EXPANDEDNODEID(RefTypeId_HasHistoricalConfiguration, 56);
*/
/**
 * @defgroup services Services
 *
 * @brief This module describes all the services used to communicate in in OPC UA.
 *
 * @{
 */

/**
 * @name Discovery Service Set
 *
 * This Service Set defines Services used to discover the Endpoints implemented
 * by a Server and to read the security configuration for those Endpoints.
 *
 * @{
 */
// Service_FindServers

/**
 * @brief This Service returns the Endpoints supported by a Server and all of
 * the configuration information required to establish a SecureChannel and a
 * Session.
 */
void Service_GetEndpoints(UA_Server                    *server,
                          const UA_GetEndpointsRequest *request, UA_GetEndpointsResponse *response);
// Service_RegisterServer
/** @} */

/**
 * @name SecureChannel Service Set
 *
 * This Service Set defines Services used to open a communication channel that
 * ensures the confidentiality and Integrity of all Messages exchanged with the
 * Server.
 *
 * @{
 */

/** @brief This Service is used to open or renew a SecureChannel that can be
   used to ensure Confidentiality and Integrity for Message exchange during a
   Session. */
void Service_OpenSecureChannel(UA_Server *server, UA_Connection *connection,
                               const UA_OpenSecureChannelRequest *request,
                               UA_OpenSecureChannelResponse *response);

/** @brief This Service is used to terminate a SecureChannel. */
void Service_CloseSecureChannel(UA_Server *server, UA_Int32 channelId);
/** @} */

/**
 * @name Session Service Set
 *
 * This Service Set defines Services for an application layer connection
 * establishment in the context of a Session.
 *
 * @{
 */

/**
 * @brief This Service is used by an OPC UA Client to create a Session and the
 * Server returns two values which uniquely identify the Session. The first
 * value is the sessionId which is used to identify the Session in the audit
 * logs and in the Server’s address space. The second is the authenticationToken
 * which is used to associate an incoming request with a Session.
 */
void Service_CreateSession(UA_Server *server, UA_SecureChannel *channel,
                           const UA_CreateSessionRequest *request, UA_CreateSessionResponse *response);

/**
 * @brief This Service is used by the Client to submit its SoftwareCertificates
 * to the Server for validation and to specify the identity of the user
 * associated with the Session. This Service request shall be issued by the
 * Client before it issues any other Service request after CreateSession.
 * Failure to do so shall cause the Server to close the Session.
 */
void Service_ActivateSession(UA_Server *server, UA_SecureChannel *channel,
                             const UA_ActivateSessionRequest *request, UA_ActivateSessionResponse *response);

/**
 * @brief This Service is used to terminate a Session.
 */
void Service_CloseSession(UA_Server *server, const UA_CloseSessionRequest *request, UA_CloseSessionResponse *response);
// Service_Cancel
/** @} */

/**
 * @name NodeManagement Service Set
 *
 * This Service Set defines Services to add and delete AddressSpace Nodes and References between
 * them. All added Nodes continue to exist in the AddressSpace even if the Client that created them
 * disconnects from the Server.
 *
 * @{
 */

/**
 * @brief This Service is used to add one or more Nodes into the AddressSpace hierarchy.
 */
void Service_AddNodes(UA_Server *server, UA_Session *session,
                      const UA_AddNodesRequest *request, UA_AddNodesResponse *response);

/**
 * @brief This Service is used to add one or more References to one or more Nodes
 */
void Service_AddReferences(UA_Server *server, UA_Session *session,
                           const UA_AddReferencesRequest *request, UA_AddReferencesResponse *response);

// Service_DeleteNodes
// Service_DeleteReferences
/** @} */

/**
 * @name View Service Set
 *
 * Clients use the browse Services of the View Service Set to navigate through
 * the AddressSpace or through a View which is a subset of the AddressSpace.
 *
 * @{
 */

/**
 * @brief This Service is used to discover the References of a specified Node.
 * The browse can be further limited by the use of a View. This Browse Service
 * also supports a primitive filtering capability.
 */
void Service_Browse(UA_Server *server, UA_Session *session,
                    const UA_BrowseRequest *request, UA_BrowseResponse *response);

/**
 * @brief This Service is used to translate textual node paths to their respective ids.
 */
void Service_TranslateBrowsePathsToNodeIds(UA_Server *server, UA_Session *session,
                                           const UA_TranslateBrowsePathsToNodeIdsRequest *request,
                                           UA_TranslateBrowsePathsToNodeIdsResponse *response);
// Service_BrowseNext
// Service_TranslateBrowsePathsToNodeIds
// Service_RegisterNodes
// Service_UnregisterNodes
/** @} */


/* Part 4: 5.9 Query Service Set */
/**
 * @name Query Service Set
 *
 * This Service Set is used to issue a Query to a Server. OPC UA Query is
 * generic in that it provides an underlying storage mechanism independent Query
 * capability that can be used to access a wide variety of OPC UA data stores
 * and information management systems. OPC UA Query permits a Client to access
 * data maintained by a Server without any knowledge of the logical schema used
 * for internal storage of the data. Knowledge of the AddressSpace is
 * sufficient.
 *
 * @{
 */
// Service_QueryFirst
// Service_QueryNext
/** @} */

/* Part 4: 5.10 Attribute Service Set */
/**
 * @name Attribute Service Set
 *
 * This Service Set provides Services to access Attributes that are part of
 * Nodes.
 *
 * @{
 */

/**
 * @brief This Service is used to read one or more Attributes of one or more
 * Nodes. For constructed Attribute values whose elements are indexed, such as
 * an array, this Service allows Clients to read the entire set of indexed
 * values as a composite, to read individual elements or to read ranges of
 * elements of the composite.
 */
void Service_Read(UA_Server *server, UA_Session *session,
                  const UA_ReadRequest *request, UA_ReadResponse *response);
// Service_HistoryRead
/**
 * @brief This Service is used to write one or more Attributes of one or more
 *  Nodes. For constructed Attribute values whose elements are indexed, such as
 *  an array, this Service allows Clients to write the entire set of indexed
 *  values as a composite, to write individual elements or to write ranges of
 *  elements of the composite.
 */
void Service_Write(UA_Server *server, UA_Session *session,
                   const UA_WriteRequest *request, UA_WriteResponse *response);
// Service_HistoryUpdate
/** @} */

/**
 * @name Method Service Set
 *
 * The Method Service Set defines the means to invoke methods. A method shall be
   a component of an Object.
 *
 * @{
 */
// Service_Call
/** @} */

/**
 * @name MonitoredItem Service Set
 *
 * Clients define MonitoredItems to subscribe to data and Events. Each
 * MonitoredItem identifies the item to be monitored and the Subscription to use
 * to send Notifications. The item to be monitored may be any Node Attribute.
 *
 * @{
 */

/**
 * @brief This Service is used to create and add one or more MonitoredItems to a
 * Subscription. A MonitoredItem is deleted automatically by the Server when the
 * Subscription is deleted. Deleting a MonitoredItem causes its entire set of
 * triggered item links to be deleted, but has no effect on the MonitoredItems
 * referenced by the triggered items.
 */
/* UA_Int32 Service_CreateMonitoredItems(UA_Server *server, UA_Session *session, */
/*                                       const UA_CreateMonitoredItemsRequest *request, */
/*                                       UA_CreateMonitoredItemsResponse *response); */
// Service_ModifyMonitoredItems
// Service_SetMonitoringMode
// Service_SetTriggering
// Service_DeleteMonitoredItems
/** @} */

/**
 * @name Subscription Service Set
 *
 * Subscriptions are used to report Notifications to the Client.
 *
 * @{
 */
// Service_CreateSubscription
/* UA_Int32 Service_CreateSubscription(UA_Server *server, UA_Session *session, */
/*                                     const UA_CreateSubscriptionRequest *request, */
/*                                     UA_CreateSubscriptionResponse *response); */
// Service_ModifySubscription
// Service_SetPublishingMode
/* UA_Int32 Service_SetPublishingMode(UA_Server *server, UA_Session *session, */
/*                                    const UA_SetPublishingModeRequest *request, */
/*                                    UA_SetPublishingModeResponse *response); */

/* UA_Int32 Service_Publish(UA_Server *server, UA_Session *session, */
/*                          const UA_PublishRequest *request, */
/*                          UA_PublishResponse *response); */

// Service_Republish
// Service_TransferSubscription
// Service_DeleteSubscription
/** @} */

/** @} */ // end of group

/**
 * @brief this macro browses through a request, looking for nodeIds belonging to the same namespace (same namespaceIndex). For each found different namespaceIndex an entry in
 * ASSOCIATED_INDEX_ARRAY is saved. Furthermore for each occurence of the same namespace the corresponding entry of NUMBER_OF_FOUND_INDICES_ARRAY is incremented
 *
 *
 * a request with 10 nodeIds, with the following namespaceIndices is received
 * 1
 * 1
 * 1
 * 3
 * 4
 * 5
 * 5
 * 5
 * 1
 * 1
 * After a call of the macro the outputs would look like that:
 * ASSOCIATED_INDEX_ARRAY[0] = 1
 * ASSOCIATED_INDEX_ARRAY[1] = 3
 * ASSOCIATED_INDEX_ARRAY[2] = 4
 * ASSOCIATED_INDEX_ARRAY[3] = 5
 *
 * NUMBER_OF_FOUND_INDICES_ARRAY[0] = 5
 * NUMBER_OF_FOUND_INDICES_ARRAY[1] = 1
 * NUMBER_OF_FOUND_INDICES_ARRAY[2] = 1
 * NUMBER_OF_FOUND_INDICES_ARRAY[3] = 3
 *
 *
 */
#define BUILD_INDEX_ARRAYS(SIZE,REQUEST_ARRAY,NODEID_PROPERTY,DIFFERENT_INDEX_COUNT,ASSOCIATED_INDEX_ARRAY,NUMBER_OF_FOUND_INDICES_ARRAY) do{ \
		DIFFERENT_INDEX_COUNT = 0;\
		for (UA_Int32 i = 0; i < SIZE; i++) { \
			UA_UInt32 j = 0; \
			do { \
				if (ASSOCIATED_INDEX_ARRAY[j] \
						== REQUEST_ARRAY[i].NODEID_PROPERTY.namespaceIndex) { \
					if (DIFFERENT_INDEX_COUNT == 0) { \
						DIFFERENT_INDEX_COUNT++; \
					} \
					NUMBER_OF_FOUND_INDICES_ARRAY[j]++; \
					break; \
				} else if (j == (DIFFERENT_INDEX_COUNT - 1)) { \
					ASSOCIATED_INDEX_ARRAY[j + 1] = \
					REQUEST_ARRAY[i].NODEID_PROPERTY.namespaceIndex; \
					NUMBER_OF_FOUND_INDICES_ARRAY[j + 1] = 1; \
					DIFFERENT_INDEX_COUNT++; \
					break; \
			} \
			j++; \
		} while (j <= DIFFERENT_INDEX_COUNT); \
	}\
}while(0)
#endif
