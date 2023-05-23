/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017-2022 Fraunhofer IOSB (Author: Andreas Ebner)
 * Copyright (c) 2019, 2022 Fraunhofer IOSB (Author: Julius Pfrommer)
 * Copyright (c) 2019 Kalycito Infotech Private Limited
 * Copyright (c) 2021 Fraunhofer IOSB (Author: Jan Hermes)
 * Copyright (c) 2022 Siemens AG (Author: Thomas Fischer)
 * Copyright (c) 2022 Fraunhofer IOSB (Author: Noel Graf)
 */

#include "ua_pubsub.h"
#include "server/ua_server_internal.h"

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
#include "ua_pubsub_ns0.h"
#endif

#ifdef UA_ENABLE_PUBSUB_MQTT
#include "../../plugins/mqtt/ua_mqtt-c_adapter.h"
#include "mqtt.h"
#endif

#ifdef UA_ENABLE_PUBSUB /* conditional compilation */

#define UDP_MAX_URL_LENGTH 512
#define UDP_MAX_PORT_CHARACTER_COUNT 6

typedef struct  {
    UA_String profileURI;
    UA_String protocol;
    UA_Boolean json;
} ProfileMapping;

#define UA_PUBSUB_PROFILES_SIZE 4

static ProfileMapping transportProfiles[UA_PUBSUB_PROFILES_SIZE] = {
    {UA_STRING_STATIC("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp"),
         UA_STRING_STATIC("udp"), false},
    {UA_STRING_STATIC("http://opcfoundation.org/UA-Profile/Transport/pubsub-mqtt-uadp"),
         UA_STRING_STATIC("mqtt"), false},
    {UA_STRING_STATIC("http://opcfoundation.org/UA-Profile/Transport/pubsub-mqtt-json"),
         UA_STRING_STATIC("mqtt"), true},
    {UA_STRING_STATIC("http://opcfoundation.org/UA-Profile/Transport/pubsub-eth-uadp"),
         UA_STRING_STATIC("eth"), false}
};

/* The entry-point for all events on a a PubSubConnection.
 *
 * The connectionContext is a tagged pointer. If the lowest bit is 0x1, then the
 * callback refers to the recv connection. If it is 0x0, then the callback
 * refers to the send connection. */
static void
PubSubConnectionCallback(UA_ConnectionManager *cm, uintptr_t connectionId,
                         void *application, void **connectionContext,
                         UA_ConnectionState state, const UA_KeyValueMap *params,
                         UA_ByteString msg);

static void
UA_PubSubConnection_removeConnection(UA_PubSubConnection *c, uintptr_t connectionId) {
    /* Send connection? */
    if(c->sendConnection == connectionId) {
        c->sendConnection = 0;
        return;
    }

    /* Remove recv connection in the array. Occupy the array contiguously. */
    size_t last_recv = UA_PUBSUB_MAXRECVCONNECTIONS - 1;
    for(; last_recv > 0; last_recv--) { if(c->recvConnections[last_recv] != 0) break; }
    for(size_t i = 0; i < UA_PUBSUB_MAXRECVCONNECTIONS; i++) {
        if(c->recvConnections[i] != connectionId)
            continue;
        c->recvConnections[i] = c->recvConnections[last_recv];
        c->recvConnections[last_recv] = 0;
        break;
    }
}

static void
UA_PubSubConnection_addConnection(UA_PubSubConnection *c, uintptr_t connectionId,
                                  UA_Boolean recv) {
    if(recv) {
        for(size_t i = 0; i < UA_PUBSUB_MAXRECVCONNECTIONS; i++) {
            if(c->recvConnections[i] == 0 || c->recvConnections[i] == connectionId) {
                c->recvConnections[i] = connectionId;
                return;
            }
        }
    } else if(c->sendConnection == 0 || c->sendConnection == connectionId) {
        c->sendConnection = connectionId;
        return;
    }

    /* No more space, close this connection. */
    if(c->cm)
        c->cm->closeConnection(c->cm, connectionId);
}

static void
UA_PubSubConnection_shutdown(UA_PubSubConnection *c) {   
    if(!c->cm)
        return;
    if(c->sendConnection != 0)
        c->cm->closeConnection(c->cm, c->sendConnection);
    for(size_t i = 0; i < UA_PUBSUB_MAXRECVCONNECTIONS; i++) {
        if(c->recvConnections[i] != 0)
            c->cm->closeConnection(c->cm, c->recvConnections[i]);
    }
}

static UA_StatusCode
UA_PubSubConnection_connectUDP(UA_PubSubConnection *c, UA_Server *server) {
    /* Check the configuration address type */
    if(!UA_Variant_hasScalarType(&c->config.address,
                                 &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE])) {
        UA_LOG_ERROR_CONNECTION(&server->config.logger, c, "No NetworkAddressUrlDataType "
                                "for the address configuration");
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    UA_NetworkAddressUrlDataType *address = (UA_NetworkAddressUrlDataType*)
        c->config.address.data;

    /* Extract hostname and port */
    UA_String hostname;
    UA_UInt16 port;
    UA_StatusCode res = UA_parseEndpointUrl(&address->url, &hostname, &port, NULL);
    if(res != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR_CONNECTION(&server->config.logger, c,
                                "Could not parse the UDP network URL");
        return res;
    }

    /* Set up the connection parameters */
    UA_Boolean listen = false;
    UA_KeyValuePair kvp[4];
    UA_KeyValueMap kvm = {3, kvp};
    kvp[0].key = UA_QUALIFIEDNAME(0, "hostname");
    UA_Variant_setScalar(&kvp[0].value, &hostname, &UA_TYPES[UA_TYPES_STRING]);
    kvp[1].key = UA_QUALIFIEDNAME(0, "port");
    UA_Variant_setScalar(&kvp[1].value, &port, &UA_TYPES[UA_TYPES_UINT16]);
    kvp[2].key = UA_QUALIFIEDNAME(0, "listen");
    UA_Variant_setScalar(&kvp[2].value, &listen, &UA_TYPES[UA_TYPES_BOOLEAN]);
    if(!UA_String_isEmpty(&address->networkInterface)) {
        kvp[3].key = UA_QUALIFIEDNAME(0, "interface");
        UA_Variant_setScalar(&kvp[3].value, &address->networkInterface,
                             &UA_TYPES[UA_TYPES_STRING]);
        kvm.mapSize = 4;
    }

    /* Open a send connection */
    if(c->sendConnection == 0) {
        UA_UNLOCK(&server->serviceMutex);
        res = c->cm->openConnection(c->cm, &kvm, server, c, PubSubConnectionCallback);
        UA_LOCK(&server->serviceMutex);
        if(res != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR_CONNECTION(&server->config.logger, c, "Could not open an UDP "
                                    "connection for sending");
            return res;
        }
    }

    /* Open a receive connection if there is a readergroup configured */
    if(c->recvConnections[0] == 0 && c->readerGroupsSize > 0) {
        listen = true;
        UA_UNLOCK(&server->serviceMutex);
        res = c->cm->openConnection(c->cm, &kvm, server, (void*)(((uintptr_t)c) | 0x1),
                                    PubSubConnectionCallback);
        UA_LOCK(&server->serviceMutex);
        if(res != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR_CONNECTION(&server->config.logger, c, "Could not open an UDP "
                                    "connection for sending");
            return res;
        }
    }

    return res;
}

static UA_StatusCode
UA_PubSubConnection_connect(UA_PubSubConnection *c, UA_Server *server) {
    UA_LOCK_ASSERT(&server->serviceMutex, 1);

    /* Connections are already open or nothing to do */
    if(c->sendConnection != 0 &&
       (c->recvConnections[0] != 0 || c->readerGroupsSize == 0))
        return UA_STATUSCODE_GOOD;

    UA_EventLoop *el = server->config.eventLoop;
    if(!el) {
        UA_LOG_ERROR_CONNECTION(&server->config.logger, c, "No EventLoop configured");
        UA_PubSubConnection_setPubSubState(server, c, UA_PUBSUBSTATE_ERROR,
                                           UA_STATUSCODE_BADINTERNALERROR);
        return UA_STATUSCODE_BADINTERNALERROR;;
    }

    /* Look up the connection manager for the connection */
    const UA_String *protocol = NULL;
    for(size_t i = 0; i < UA_PUBSUB_PROFILES_SIZE; i++) {
        if(!UA_String_equal(&c->config.transportProfileUri, &transportProfiles[i].profileURI))
            continue;
        for(UA_EventSource *es = el->eventSources; es != NULL; es = es->next) {
            if(es->eventSourceType != UA_EVENTSOURCETYPE_CONNECTIONMANAGER)
                continue;
            UA_ConnectionManager *cm = (UA_ConnectionManager*)es;
            if(!UA_String_equal(&transportProfiles[i].protocol, &cm->protocol))
                continue;
            if(c->cm && c->cm != cm) {
                UA_LOG_ERROR_CONNECTION(&server->config.logger, c,
                                        "The protocol cannot be changed for an "
                                        "existing PubSub connection");
                UA_PubSubConnection_setPubSubState(server, c, UA_PUBSUBSTATE_ERROR,
                                                   UA_STATUSCODE_BADINTERNALERROR);
                return UA_STATUSCODE_BADINTERNALERROR;;
            }
            protocol = &transportProfiles[i].protocol;
            c->json = transportProfiles[i].json;
            c->cm = cm;
            break;
        }
        break;
    }
    if(!protocol) {
        UA_LOG_ERROR_CONNECTION(&server->config.logger, c,
                                  "The requested protocol is not supported");
        UA_PubSubConnection_setPubSubState(server, c, UA_PUBSUBSTATE_ERROR,
                                           UA_STATUSCODE_BADINTERNALERROR);
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    /* Connect */
    UA_StatusCode res = UA_STATUSCODE_GOOD;
    const UA_String udpStr = UA_STRING_STATIC("udp");
    const UA_String ethStr = UA_STRING_STATIC("eth");
    const UA_String mqttStr = UA_STRING_STATIC("mqtt");
    if(UA_String_equal(protocol, &udpStr)) {
        res = UA_PubSubConnection_connectUDP(c, server);
    } else if(UA_String_equal(protocol, &ethStr)) {
        //res = UA_PubSubConnection_connectETH(c, server);
    } else if(UA_String_equal(protocol, &mqttStr)) {
        //res = UA_PubSubConnection_connectMQTT(c, server);
    } else {
        UA_LOG_ERROR_CONNECTION(&server->config.logger, c,
                                  "The requested protocol is not supported");
        res = UA_STATUSCODE_BADINTERNALERROR;
    }

    /* Connecting failed */
    if(res != UA_STATUSCODE_GOOD) {
        UA_PubSubConnection_setPubSubState(server, c, UA_PUBSUBSTATE_ERROR, res);
        return res;
    }

    /* Set to preoperational. Set the state "manually" to avoid recursion. Also
     * this is the only place to set pre-operational for PubSubConnections. The
     * state will be set to operational in the network callback when the
     * connection has fully opened. */
    if(c->state != UA_PUBSUBSTATE_OPERATIONAL)
        c->state = UA_PUBSUBSTATE_PREOPERATIONAL;

    return UA_STATUSCODE_GOOD;
}

static void
delayedPubSubConnection_delete(void *application, void *context) {
    UA_PubSubConnection *c = (UA_PubSubConnection*)context;
    UA_PubSubConnectionConfig_clear(&c->config);
    UA_NodeId_clear(&c->identifier);
    UA_free(c);
}

UA_StatusCode
decodeNetworkMessage(UA_Server *server, UA_ByteString *buffer, size_t *pos,
                     UA_NetworkMessage *nm, UA_PubSubConnection *connection) {
#ifdef UA_DEBUG_DUMP_PKGS
    UA_dump_hex_pkg(buffer->data, buffer->length);
#endif

    UA_StatusCode rv = UA_NetworkMessage_decodeHeaders(buffer, pos, nm);
    if(rv != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING_CONNECTION(&server->config.logger, connection,
                                  "PubSub receive. decoding headers failed");
        return rv;
    }

#ifdef UA_ENABLE_PUBSUB_ENCRYPTION
    UA_Boolean processed = false;
    UA_ReaderGroup *readerGroup;
    UA_DataSetReader *reader;

    /* Choose a correct readergroup for decrypt/verify this message
     * (there could be multiple) */
    LIST_FOREACH(readerGroup, &connection->readerGroups, listEntry) {
        LIST_FOREACH(reader, &readerGroup->readers, listEntry) {
            UA_StatusCode retval = checkReaderIdentifier(server, nm, reader, readerGroup->config);
            if(retval != UA_STATUSCODE_GOOD)
                continue;
            processed = true;
            rv = verifyAndDecryptNetworkMessage(&server->config.logger, buffer, pos,
                                                nm, readerGroup);
            if(rv != UA_STATUSCODE_GOOD) {
                UA_LOG_WARNING_CONNECTION(&server->config.logger, connection,
                                          "Subscribe failed, verify and decrypt "
                                          "network message failed.");
                return rv;
            }
            
            /* break out of all loops when first verify & decrypt was successful */
            goto loops_exit;
        }
    }

loops_exit:
    if(!processed) {
        UA_LOG_INFO_CONNECTION(&server->config.logger, connection,
                               "Dataset reader not found. Check PublisherId, "
                               "WriterGroupId and DatasetWriterId");
        /* Possible multicast scenario: there are multiple connections (with one
         * or more ReaderGroups) within a multicast group every connection
         * receives all network messages, even if some of them are not meant for
         * the connection currently processed -> therefore it is ok if the
         * connection does not have a DataSetReader for every received network
         * message. We must not return an error here, but continue with the
         * buffer decoding and see if we have a matching DataSetReader for the
         * next network message. */
    }
#endif

    rv = UA_NetworkMessage_decodePayload(buffer, pos, nm, server->config.customDataTypes);
    UA_CHECK_STATUS(rv, return rv);

    rv = UA_NetworkMessage_decodeFooters(buffer, pos, nm);
    UA_CHECK_STATUS(rv, return rv);

    return UA_STATUSCODE_GOOD;
}

static void
PubSubConnectionCallback(UA_ConnectionManager *cm, uintptr_t connectionId,
                         void *application, void **connectionContext,
                         UA_ConnectionState state, const UA_KeyValueMap *params,
                         UA_ByteString msg) {
    if(!connectionContext)
        return;

    /* Get the context pointers */
    UA_Server *server = (UA_Server*)application;
    uintptr_t context = (uintptr_t)*connectionContext;
    UA_Boolean recv = !!(context & 0x1);
    UA_PubSubConnection *psc = (UA_PubSubConnection*)
        (context & ~(uintptr_t)0x1);

    UA_LOCK(&server->serviceMutex);

    /* The connection is closing in the EventLoop. This is the last callback
     * from that connection. Clean up the SecureChannel in the client. */
    if(state == UA_CONNECTIONSTATE_CLOSING) {
        /* Reset the connection identifiers */
        UA_PubSubConnection_removeConnection(psc, connectionId);

        /* PSC marked for deletion and the last EventLoop connection has closed */
        if(psc->deleteFlag && psc->recvConnections[0] == 0 && psc->sendConnection == 0) {
            delayedPubSubConnection_delete(NULL, psc);
            UA_UNLOCK(&server->serviceMutex);
            return;
        }

        /* Reconnect automatically if the connection was operational. This sets
         * the connection state if connecting fails. Attention! If there are
         * several send or recv sockets, then the connection is only reopened if
         * all of them close - which is usually the case. */
        if(psc->state == UA_PUBSUBSTATE_OPERATIONAL)
            UA_PubSubConnection_connect(psc, server);

        UA_UNLOCK(&server->serviceMutex);
        return;
    }

    /* Store the connectionId (if a new connection) */
    UA_PubSubConnection_addConnection(psc, connectionId, recv);

    /* Connection open, set to operational if not already done */
    if(psc->state != UA_PUBSUBSTATE_OPERATIONAL)
        UA_PubSubConnection_setPubSubState(server, psc, UA_PUBSUBSTATE_OPERATIONAL,
                                           UA_STATUSCODE_GOOD);
    
    /* No message received */
    if(!recv || msg.length == 0) {
        UA_UNLOCK(&server->serviceMutex);
        return;
    }

    UA_NetworkMessage nm;
    UA_Boolean processed = false;
    UA_StatusCode res = UA_STATUSCODE_GOOD;
    memset(&nm, 0, sizeof(UA_NetworkMessage));

    /* Process buffer for realtime ReaderGroups */
    UA_ReaderGroup *readerGroup;
    UA_ReaderGroup *normalReaderGroup = NULL;
    LIST_FOREACH(readerGroup, &psc->readerGroups, listEntry) {
        if(readerGroup->config.rtLevel != UA_PUBSUB_RT_FIXED_SIZE) {
            normalReaderGroup = readerGroup;
            continue;
        }
        if(readerGroup->state != UA_PUBSUBSTATE_OPERATIONAL &&
           readerGroup->state != UA_PUBSUBSTATE_PREOPERATIONAL)
            continue;
        processed |= UA_ReaderGroup_decodeAndProcessRT(server, readerGroup, &msg);
    }
    
    if(!normalReaderGroup)
        goto done;

    /* Decode the received message for the non-realtime ReaderGroups */
    if(normalReaderGroup->config.encodingMimeType == UA_PUBSUB_ENCODING_UADP) {
        size_t currentPosition = 0;
        res = decodeNetworkMessage(server, &msg, &currentPosition, &nm, psc);
    } else { /* if(writerGroup->config.encodingMimeType == UA_PUBSUB_ENCODING_JSON) */
#ifdef UA_ENABLE_JSON_ENCODING
        res = UA_NetworkMessage_decodeJson(&nm, &msg);
#else
        res = UA_STATUSCODE_BADNOTSUPPORTED;
#endif
    }
    if(res != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING_CONNECTION(&server->config.logger, psc,
                                  "Verify, decrypt and decode network message failed.");
        goto done;
    }

    /* Process the received message */
    LIST_FOREACH(readerGroup, &psc->readerGroups, listEntry) {
        if(readerGroup->config.rtLevel == UA_PUBSUB_RT_FIXED_SIZE)
            continue;
        if(readerGroup->state != UA_PUBSUBSTATE_OPERATIONAL &&
           readerGroup->state != UA_PUBSUBSTATE_PREOPERATIONAL)
            continue;
        processed |= UA_ReaderGroup_process(server, readerGroup, &nm);
    }

 done:
    if(!processed) {
        UA_LOG_WARNING_CONNECTION(&server->config.logger, psc,
                                  "Message received that could not be processed. "
                                  "Check PublisherID, WriterGroupID and DatasetWriterID.");
    }

    UA_NetworkMessage_clear(&nm);

    UA_UNLOCK(&server->serviceMutex);
}

UA_StatusCode
UA_PubSubConnectionConfig_copy(const UA_PubSubConnectionConfig *src,
                               UA_PubSubConnectionConfig *dst) {
    UA_StatusCode res = UA_STATUSCODE_GOOD;
    memcpy(dst, src, sizeof(UA_PubSubConnectionConfig));
    if (src->publisherIdType == UA_PUBLISHERIDTYPE_STRING) {
        res |= UA_String_copy(&src->publisherId.string, &dst->publisherId.string);
    }
    res |= UA_String_copy(&src->name, &dst->name);
    res |= UA_Variant_copy(&src->address, &dst->address);
    res |= UA_String_copy(&src->transportProfileUri, &dst->transportProfileUri);
    res |= UA_Variant_copy(&src->connectionTransportSettings,
                           &dst->connectionTransportSettings);
    res |= UA_KeyValueMap_copy(&src->connectionProperties,
                               &dst->connectionProperties);
    if(res != UA_STATUSCODE_GOOD)
        UA_PubSubConnectionConfig_clear(dst);
    return res;
}

UA_StatusCode
UA_Server_getPubSubConnectionConfig(UA_Server *server, const UA_NodeId connection,
                                    UA_PubSubConnectionConfig *config) {
    if(!config)
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    UA_LOCK(&server->serviceMutex);
    UA_PubSubConnection *currentPubSubConnection =
        UA_PubSubConnection_findConnectionbyId(server, connection);
    UA_StatusCode res = UA_STATUSCODE_BADNOTFOUND;
    if(currentPubSubConnection)
        res = UA_PubSubConnectionConfig_copy(&currentPubSubConnection->config, config);
    UA_UNLOCK(&server->serviceMutex);
    return res;
}

UA_PubSubConnection *
UA_PubSubConnection_findConnectionbyId(UA_Server *server, UA_NodeId connectionIdentifier) {
    UA_PubSubConnection *pubSubConnection;
    TAILQ_FOREACH(pubSubConnection, &server->pubSubManager.connections, listEntry){
        if(UA_NodeId_equal(&connectionIdentifier, &pubSubConnection->identifier))
            break;
    }
    return pubSubConnection;
}

void
UA_PubSubConnectionConfig_clear(UA_PubSubConnectionConfig *connectionConfig) {
    if(connectionConfig->publisherIdType == UA_PUBLISHERIDTYPE_STRING)
        UA_String_clear(&connectionConfig->publisherId.string);
    UA_String_clear(&connectionConfig->name);
    UA_String_clear(&connectionConfig->transportProfileUri);
    UA_Variant_clear(&connectionConfig->connectionTransportSettings);
    UA_Variant_clear(&connectionConfig->address);
    UA_KeyValueMap_clear(&connectionConfig->connectionProperties);
}

UA_StatusCode
UA_PubSubConnection_create(UA_Server *server,
                           const UA_PubSubConnectionConfig *connectionConfig,
                           UA_NodeId *connectionIdentifier) {
    /* Validate preconditions */
    UA_CHECK_MEM(server, return UA_STATUSCODE_BADINTERNALERROR);
    UA_CHECK_MEM_ERROR(connectionConfig, return UA_STATUSCODE_BADINTERNALERROR,
                       &server->config.logger, UA_LOGCATEGORY_SERVER,
                       "PubSub Connection creation failed. No connection configuration supplied.");

    /* Allocate */
    UA_PubSubConnection *connection = (UA_PubSubConnection *)
        UA_calloc(1, sizeof(UA_PubSubConnection));
    if(!connection) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "PubSub Connection creation failed. Out of Memory.");
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    connection->componentType = UA_PUBSUB_COMPONENT_CONNECTION;

    /* Copy the connection config */
    UA_StatusCode ret =
        UA_PubSubConnectionConfig_copy(connectionConfig, &connection->config);
    UA_CHECK_STATUS(ret, UA_free(connection); return ret);

    /* Assign the connection identifier */
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
    /* Internally create a unique id */
    addPubSubConnectionRepresentation(server, connection);
#else
    /* Create a unique NodeId that does not correspond to a Node */
    UA_PubSubManager_generateUniqueNodeId(&server->pubSubManager,
                                          &connection->identifier);
#endif
    if(connectionIdentifier)
        UA_NodeId_copy(&connection->identifier, connectionIdentifier);

    /* Register */
    UA_PubSubManager *pubSubManager = &server->pubSubManager;
    TAILQ_INSERT_HEAD(&pubSubManager->connections, connection, listEntry);
    pubSubManager->connectionsSize++;

    /* Make the connection operational */
    ret = UA_PubSubConnection_setPubSubState(server, connection, UA_PUBSUBSTATE_OPERATIONAL,
                                             UA_STATUSCODE_GOOD);
    if(ret != UA_STATUSCODE_GOOD)
        UA_PubSubConnection_delete(server, connection);
    return ret;
}

UA_StatusCode
UA_Server_addPubSubConnection(UA_Server *server,
                              const UA_PubSubConnectionConfig *connectionConfig,
                              UA_NodeId *connectionIdentifier) {
    UA_LOCK(&server->serviceMutex);
    UA_StatusCode res = UA_PubSubConnection_create(server, connectionConfig, connectionIdentifier);
    UA_UNLOCK(&server->serviceMutex);
    return res;
}

/* Clean up the PubSubConnection. If no EventLoop connection is attached we can
 * immediately free. Otherwise we close the EventLoop connections and free in
 * the connection callback. */
void
UA_PubSubConnection_delete(UA_Server *server, UA_PubSubConnection *c) {
    UA_LOCK_ASSERT(&server->serviceMutex, 1);

    /* Stop, unfreeze and delete all WriterGroups attached to the Connection */
    UA_WriterGroup *writerGroup, *tmpWriterGroup;
    LIST_FOREACH_SAFE(writerGroup, &c->writerGroups, listEntry, tmpWriterGroup) {
        UA_WriterGroup_setPubSubState(server, writerGroup, UA_PUBSUBSTATE_DISABLED,
                                      UA_STATUSCODE_BADSHUTDOWN);
        UA_Server_unfreezeWriterGroupConfiguration(server, writerGroup->identifier);
        UA_Server_removeWriterGroup(server, writerGroup->identifier);
    }

    /* Stop, unfreeze and delete all ReaderGroups attached to the Connection */
    UA_ReaderGroup *readerGroup, *tmpReaderGroup;
    LIST_FOREACH_SAFE(readerGroup, &c->readerGroups, listEntry, tmpReaderGroup) {
        UA_ReaderGroup_setPubSubState(server, readerGroup, UA_PUBSUBSTATE_DISABLED,
                                      UA_STATUSCODE_BADSHUTDOWN);
        UA_Server_unfreezeReaderGroupConfiguration(server, readerGroup->identifier);
        UA_Server_removeReaderGroup(server, readerGroup->identifier);
    }

    /* Remove from the information model */
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
    deleteNode(server, c->identifier, true);
#endif

    /* Unlink from the server */
    TAILQ_REMOVE(&server->pubSubManager.connections, c, listEntry);
    server->pubSubManager.connectionsSize--;

    /* Mark as to-be-deleted */
    c->deleteFlag = true;

    /* No open EventLoop connections -> delete the PubSubConnection */
    UA_EventLoop *el = server->config.eventLoop;
    if(!el) {
        delayedPubSubConnection_delete(NULL, c);
        return;
    }
    if(c->sendConnection == 0 && c->recvConnections[0] == 0) {
        c->dc.callback = delayedPubSubConnection_delete;
        c->dc.context = c;
        el->addDelayedCallback(el, &c->dc);
        return;
    }

    /* Close the EventLoop connection and finalize the deletion in the callback */
    UA_PubSubConnection_shutdown(c);
}

UA_StatusCode
UA_Server_removePubSubConnection(UA_Server *server, const UA_NodeId connection) {
    UA_LOCK(&server->serviceMutex);
    UA_PubSubConnection *psc =
        UA_PubSubConnection_findConnectionbyId(server, connection);
    if(!psc) {
        UA_UNLOCK(&server->serviceMutex);
        return UA_STATUSCODE_BADNOTFOUND;
    }
    UA_PubSubConnection_delete(server, psc);
    UA_UNLOCK(&server->serviceMutex);
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_PubSubConnection_setPubSubState(UA_Server *server, UA_PubSubConnection *c,
                                   UA_PubSubState state, UA_StatusCode cause) {
    UA_LOCK_ASSERT(&server->serviceMutex, 1);

    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    UA_PubSubState oldState = c->state;
    UA_WriterGroup *writerGroup;
    UA_ReaderGroup *readerGroup;

    if(state == UA_PUBSUBSTATE_PREOPERATIONAL) {
        UA_LOG_WARNING_CONNECTION(&server->config.logger, c,
                                  "Cannot manually set Connection as preoperational");
        return UA_STATUSCODE_BADINTERNALERROR;
    } else if(state < UA_PUBSUBSTATE_DISABLED || state > UA_PUBSUBSTATE_ERROR) {
        UA_LOG_WARNING_CONNECTION(&server->config.logger, c,
                                  "Received unknown PubSub state!");
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    switch(state) {
        case UA_PUBSUBSTATE_ERROR:
        case UA_PUBSUBSTATE_PAUSED:
        case UA_PUBSUBSTATE_DISABLED:
            if(state == oldState)
                break;

            /* Close the EventLoop connection */
            c->state = state;
            UA_PubSubConnection_shutdown(c);

            /* Disable Reader and WriterGroups */
            LIST_FOREACH(readerGroup, &c->readerGroups, listEntry) {
                UA_ReaderGroup_setPubSubState(server, readerGroup, state,
                                              UA_STATUSCODE_BADRESOURCEUNAVAILABLE);
            }
            LIST_FOREACH(writerGroup, &c->writerGroups, listEntry) {
                UA_WriterGroup_setPubSubState(server, writerGroup, state,
                                              UA_STATUSCODE_BADRESOURCEUNAVAILABLE);
            }
            break;

        case UA_PUBSUBSTATE_OPERATIONAL:
        default:
            /* Called also if the connection is already operational. We might to
             * open an additional recv connection, etc. Sets the new state
             * internally. */
            ret = UA_PubSubConnection_connect(c, server);
            break;
    }

    /* Inform application about state change */
    if(c->state != oldState) {
        UA_ServerConfig *config = &server->config;
        if(config->pubSubConfig.stateChangeCallback)
            config->pubSubConfig.stateChangeCallback(server, &c->identifier, state, cause);
    }
    return ret;
}

#endif /* UA_ENABLE_PUBSUB */
