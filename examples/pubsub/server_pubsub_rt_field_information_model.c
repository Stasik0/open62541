/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */

#include <open62541/server.h>
#include <open62541/plugin/log.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/server_config_default.h>
#include <open62541/plugin/pubsub_udp.h>

UA_Boolean running = true;
UA_NodeId publishedDataSetIdent, dataSetFieldIdent, writerGroupIdent, connectionIdentifier;
UA_UInt32 *integerRTValue, *integerRTValue2;
UA_NodeId addedNodId1, addedNodId2;

static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = false;
}

/* The following PubSub configuration does not differ from the 'normal' configuration */
static void
addMinimalPubSubConfiguration(UA_Server * server){
    /* Add one PubSubConnection */
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING("UDP-UADP Connection 1");
    connectionConfig.transportProfileUri = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    connectionConfig.enabled = UA_TRUE;
    UA_NetworkAddressUrlDataType networkAddressUrl = {UA_STRING_NULL , UA_STRING("opc.udp://224.0.0.22:4840/")};
    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl, &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.publisherId.numeric = UA_UInt32_random();
    UA_Server_addPubSubConnection(server, &connectionConfig, &connectionIdentifier);
    /* Add one PublishedDataSet */
    UA_PublishedDataSetConfig publishedDataSetConfig;
    memset(&publishedDataSetConfig, 0, sizeof(UA_PublishedDataSetConfig));
    publishedDataSetConfig.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
    publishedDataSetConfig.name = UA_STRING("Demo PDS");
    /* Add one DataSetField to the PDS */
    UA_Server_addPublishedDataSet(server, &publishedDataSetConfig, &publishedDataSetIdent);
}

static UA_NodeId
addVariable(UA_Server *server, char *name) {
    UA_NodeId outNodeId;
    /* Define the attribute of the myInteger variable node */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_UInt32 myInteger = 42;
    UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_UINT32]);
    attr.description = UA_LOCALIZEDTEXT("en-US", name);
    attr.displayName = UA_LOCALIZEDTEXT("en-US", name);
    attr.dataType = UA_TYPES[UA_TYPES_UINT32].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    /* Add the variable node to the information model */
    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, name);
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(server, UA_NODEID_NULL, parentNodeId,
                              parentReferenceNodeId, myIntegerName,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, &outNodeId);
    return outNodeId;
}

/* If the external data source is written over the information model, the
 * ,externalDataWriteCallback, will be triggered. The user has to take care and assure
 * that the write leads not to synchronization issues and race conditions. */
static void
externalDataWriteCallback(UA_Server *server, const UA_NodeId *sessionId,
                                  void *sessionContext, const UA_NodeId *nodeId,
                                  void *nodeContext, const UA_NumericRange *range,
                                  const UA_DataValue *data){
                                      printf("TODO Implement compare and switch");
    printf("TODO Implement compare and switch");
    //The user must take about synchronization.
    if(UA_NodeId_equal(nodeId, &addedNodId1)){
        memcpy(data->value.data, integerRTValue, sizeof(UA_UInt32));
    } else if(UA_NodeId_equal(nodeId, &addedNodId2)){
        memcpy(data->value.data, integerRTValue2, sizeof(UA_UInt32));
    }
}

static void
cyclicValueUpdateCallback_UpdateToMemory(UA_Server *server, void *data) {
    *integerRTValue = (*integerRTValue)+1;
    *integerRTValue2 = (*integerRTValue2)+1;
}

int main(void){
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);
    config->pubsubTransportLayers = (UA_PubSubTransportLayer *) UA_malloc(sizeof(UA_PubSubTransportLayer));
    if(!config->pubsubTransportLayers) {
        UA_Server_delete(server);
        return -1;
    }
    config->pubsubTransportLayers[0] = UA_PubSubTransportLayerUDPMP();
    config->pubsubTransportLayersSize++;

    addMinimalPubSubConfiguration(server);

    /*add rt configuration*/
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("Demo WriterGroup");
    writerGroupConfig.publishingInterval = 1000;
    writerGroupConfig.enabled = UA_FALSE;
    writerGroupConfig.writerGroupId = 100;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;
    writerGroupConfig.messageSettings.encoding             = UA_EXTENSIONOBJECT_DECODED;
    writerGroupConfig.messageSettings.content.decoded.type = &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE];
    UA_UadpWriterGroupMessageDataType writerGroupMessage;
    UA_UadpWriterGroupMessageDataType_init(&writerGroupMessage);
    writerGroupMessage.networkMessageContentMask = (UA_UadpNetworkMessageContentMask) ((UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_PUBLISHERID |
                                                                                       (UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_GROUPHEADER |
                                                                                       (UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_WRITERGROUPID |(UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_PAYLOADHEADER);
    writerGroupConfig.messageSettings.content.decoded.data = &writerGroupMessage;
    writerGroupConfig.rtLevel = UA_PUBSUB_RT_FIXED_SIZE;
    UA_Server_addWriterGroup(server, connectionIdentifier, &writerGroupConfig, &writerGroupIdent);

    /* Add one DataSetWriter */
    UA_NodeId dataSetWriterIdent;
    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(UA_DataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("Demo DataSetWriter");
    dataSetWriterConfig.dataSetWriterId = 62541;
    dataSetWriterConfig.keyFrameCount = 10;
    UA_Server_addDataSetWriter(server, writerGroupIdent, publishedDataSetIdent, &dataSetWriterConfig, &dataSetWriterIdent);

    /* add new node to the information model with external data source backend*/
    addedNodId1 = addVariable(server, "RT value source 1");
    /* create external value source for the node */
    integerRTValue = UA_UInt32_new();
    UA_DataValue *dataValueRT = UA_DataValue_new();
    dataValueRT->hasValue = UA_TRUE;
    UA_Variant_setScalar(&dataValueRT->value, integerRTValue, &UA_TYPES[UA_TYPES_UINT32]);
    /* Set the value backend of the above create node to 'external value source' */
    UA_ValueBackend valueBackend;
    valueBackend.backendType = UA_VALUEBACKENDTYPE_EXTERNAL;
    valueBackend.backend.external.value = &dataValueRT;
    valueBackend.backend.external.callback.onWrite = externalDataWriteCallback;
    valueBackend.backend.external.callback.onRead = NULL;
    UA_Server_setVariableNode_valueBackend(server, addedNodId1, valueBackend);

    /* setup RT DataSetField config */
    UA_DataSetFieldConfig dsfConfig;
    memset(&dsfConfig, 0, sizeof(UA_DataSetFieldConfig));
    dsfConfig.field.variable.publishParameters.publishedVariable = addedNodId1;
    // -> This is removed and the node is checkt to be form type external value source
    // TODO remove the following flags
    // dsfConfig.field.variable.staticValueSourceEnabled = UA_TRUE;
    // dsfConfig.field.variable.staticValueSource.value = variantRT;
    UA_NodeId dsfNodeId;
    // TODO check external data source during the RT profile creation
    UA_Server_addDataSetField(server, publishedDataSetIdent, &dsfConfig, &dsfNodeId);

    /* add second new node to the information model with external data source backend*/
    addedNodId2 = addVariable(server, "RT value source 2");
    /* create external value source for the node */
    integerRTValue2 = UA_UInt32_new();
    *integerRTValue2 = 1000;
    UA_DataValue *dataValue2RT = UA_DataValue_new();
    dataValue2RT->hasValue = UA_TRUE;
    UA_Variant_setScalar(&dataValue2RT->value, integerRTValue2, &UA_TYPES[UA_TYPES_UINT32]);
    /* Set the value backend of the above create node to 'external value source' */
    UA_ValueBackend valueBackend2;
    valueBackend2.backendType = UA_VALUEBACKENDTYPE_EXTERNAL;
    valueBackend2.backend.external.value = &dataValue2RT;
    valueBackend2.backend.external.callback.onWrite = externalDataWriteCallback;
    valueBackend2.backend.external.callback.onRead = NULL;
    UA_Server_setVariableNode_valueBackend(server, addedNodId2, valueBackend2);

    /* setup second DataSetField config */
    UA_DataSetFieldConfig dsfConfig2;
    memset(&dsfConfig2, 0, sizeof(UA_DataSetFieldConfig));
    dsfConfig2.field.variable.publishParameters.publishedVariable = addedNodId2;
    UA_Server_addDataSetField(server, publishedDataSetIdent, &dsfConfig2, NULL);

    /* Freeze the PubSub configuration (and start implicitly the publish callback) */
    UA_Server_freezeWriterGroupConfiguration(server, writerGroupIdent);
    UA_Server_setWriterGroupOperational(server, writerGroupIdent);

    /* Disable PubSub and remove the second RT field */
    UA_Server_setWriterGroupDisabled(server, writerGroupIdent);
    UA_Server_unfreezeWriterGroupConfiguration(server, writerGroupIdent);
    UA_Server_removeDataSetField(server, dsfNodeId);

    /* Enable PubSub */
    UA_Server_freezeWriterGroupConfiguration(server, writerGroupIdent);
    UA_Server_setWriterGroupOperational(server, writerGroupIdent);

    UA_UInt64 callbackId;
    UA_Server_addRepeatedCallback(server, cyclicValueUpdateCallback_UpdateToMemory, NULL,
                                  1000, &callbackId);

    UA_StatusCode retval = UA_Server_run(server, &running);
    UA_Server_delete(server);

    /* Free external data source */
    UA_DataValue_clear(dataValueRT);
    UA_DataValue_clear(dataValue2RT);

    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
