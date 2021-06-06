/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017-2018 Fraunhofer IOSB (Author: Andreas Ebner)
 * Copyright (c) 2019 Fraunhofer IOSB (Author: Julius Pfrommer)
 * Copyright (c) 2019 Kalycito Infotech Private Limited
 * Copyright (c) 2021 Fraunhofer IOSB (Author: Jan Hermes)
 */

#include <open62541/server_pubsub.h>
#include <open62541/types_generated_encoding_binary.h>

#include "server/ua_server_internal.h"

#ifdef UA_ENABLE_PUBSUB /* conditional compilation */

#include "ua_pubsub.h"

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
#include "ua_pubsub_networkmessage.h"
#include "ua_pubsub_ns0.h"

#endif

#ifdef UA_ENABLE_PUBSUB_DELTAFRAMES
#include "ua_types_encoding_binary.h"
#endif

#ifdef UA_ENABLE_PUBSUB_BUFMALLOC
#include "ua_pubsub_bufmalloc.h"
#endif

/* This functionality of this API will be used in future to create mirror Variables - TODO */
/* #define UA_MAX_SIZENAME           64 */ /* Max size of Qualified Name of Subscribed Variable */

/* Clear ReaderGroup */
static void
UA_Server_ReaderGroup_clear(UA_Server* server, UA_ReaderGroup *readerGroup);
/* Clear DataSetReader */
static void
UA_DataSetReader_clear(UA_Server *server, UA_DataSetReader *dataSetReader);

static void
UA_PubSubDSRDataSetField_sampleValue(UA_Server *server, UA_DataSetReader *dataSetReader,
                                     UA_DataValue *value, UA_FieldTargetVariable *ftv) {
    /* TODO: Static value source without RT information model
     * This API supports only to external datasource in RT configutation
     * TODO: Extend to support other configuration if required */

    /* Get the Node */
    const UA_VariableNode *rtNode = (const UA_VariableNode *)
        UA_NODESTORE_GET(server, &ftv->targetVariable.targetNodeId);
    if(!rtNode)
        return;

    if(rtNode->valueBackend.backendType == UA_VALUEBACKENDTYPE_EXTERNAL) {
        /* Set the external source in the dataset reader config */
        ftv->externalDataValue = rtNode->valueBackend.backend.external.value;

        /* Get the value to compute the offsets */
        *value = **rtNode->valueBackend.backend.external.value;
        value->value.storageType = UA_VARIANT_DATA_NODELETE;
    }

    UA_NODESTORE_RELEASE(server, (const UA_Node *) rtNode);
}

static UA_StatusCode
UA_PubSubDataSetReader_generateKeyFrameMessage(UA_Server *server,
                                               UA_DataSetMessage *dataSetMessage,
                                               UA_DataSetReader *dataSetReader) {
    /* Prepare DataSetMessageContent */
    UA_TargetVariables *tv = &dataSetReader->config.subscribedDataSet.subscribedDataSetTarget;
    dataSetMessage->header.dataSetMessageValid = true;
    dataSetMessage->header.dataSetMessageType = UA_DATASETMESSAGE_DATAKEYFRAME;
    dataSetMessage->data.keyFrameData.fieldCount = (UA_UInt16) tv->targetVariablesSize;
    dataSetMessage->data.keyFrameData.dataSetFields = (UA_DataValue *)
            UA_Array_new(tv->targetVariablesSize, &UA_TYPES[UA_TYPES_DATAVALUE]);
    if(!dataSetMessage->data.keyFrameData.dataSetFields)
        return UA_STATUSCODE_BADOUTOFMEMORY;

     for(size_t counter = 0; counter < tv->targetVariablesSize; counter++) {
        /* Sample the value and set the source in the reader config */
        UA_DataValue *dfv = &dataSetMessage->data.keyFrameData.dataSetFields[counter];
        UA_FieldTargetVariable *ftv = &tv->targetVariables[counter];
        UA_PubSubDSRDataSetField_sampleValue(server, dataSetReader, dfv, ftv);

        /* Deactivate statuscode? */
        if(((u64)dataSetReader->config.dataSetFieldContentMask &
            (u64)UA_DATASETFIELDCONTENTMASK_STATUSCODE) == 0)
            dfv->hasStatus = false;

        /* Deactivate timestamps */
        if(((u64)dataSetReader->config.dataSetFieldContentMask &
            (u64)UA_DATASETFIELDCONTENTMASK_SOURCETIMESTAMP) == 0)
            dfv->hasSourceTimestamp = false;
        if(((u64)dataSetReader->config.dataSetFieldContentMask &
            (u64)UA_DATASETFIELDCONTENTMASK_SOURCEPICOSECONDS) == 0)
            dfv->hasSourcePicoseconds = false;
        if(((u64)dataSetReader->config.dataSetFieldContentMask &
            (u64)UA_DATASETFIELDCONTENTMASK_SERVERTIMESTAMP) == 0)
            dfv->hasServerTimestamp = false;
        if(((u64)dataSetReader->config.dataSetFieldContentMask &
            (u64)UA_DATASETFIELDCONTENTMASK_SERVERPICOSECONDS) == 0)
            dfv->hasServerPicoseconds = false;
    }

    return UA_STATUSCODE_GOOD;
}

/**
 * Generate a DataSetMessage for the given reader.
 *
 * @param dataSetReader ptr to corresponding reader
 * @return ptr to generated DataSetMessage
 */
static UA_StatusCode
UA_DataSetReader_generateDataSetMessage(UA_Server *server, UA_DataSetMessage *dataSetMessage,
                                        UA_DataSetReader *dataSetReader) {
    /* Reset the message */
    memset(dataSetMessage, 0, sizeof(UA_DataSetMessage));

    /* Support only for UADP configuration
     * TODO: JSON encoding if UA_DataSetReader_generateDataSetMessage used other that RT configuration
     */

    if(dataSetReader->config.messageSettings.content.decoded.type != &UA_TYPES[UA_TYPES_UADPDATASETREADERMESSAGEDATATYPE]) {
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Only UADP encoding is supported.");
        return UA_STATUSCODE_BADNOTSUPPORTED;
    }

    /* The configuration Flags are included inside the std. defined UA_UadpDataSetReaderMessageDataType */
    UA_UadpDataSetReaderMessageDataType defaultUadpConfiguration;
    UA_UadpDataSetReaderMessageDataType *dataSetReaderMessageDataType = (UA_UadpDataSetReaderMessageDataType *)
        dataSetReader->config.messageSettings.content.decoded.data;

    if(!(dataSetReader->config.messageSettings.encoding == UA_EXTENSIONOBJECT_DECODED ||
       dataSetReader->config.messageSettings.encoding == UA_EXTENSIONOBJECT_DECODED_NODELETE) ||
       !dataSetReaderMessageDataType->dataSetMessageContentMask) {
        /* create default flag configuration if no dataSetMessageContentMask or even messageSettings in
         * UadpDataSetWriterMessageDataType was passed in */
        memset(&defaultUadpConfiguration, 0, sizeof(UA_UadpDataSetReaderMessageDataType));
        defaultUadpConfiguration.dataSetMessageContentMask = (UA_UadpDataSetMessageContentMask)
            ((u64)UA_UADPDATASETMESSAGECONTENTMASK_TIMESTAMP |
             (u64)UA_UADPDATASETMESSAGECONTENTMASK_MAJORVERSION |
             (u64)UA_UADPDATASETMESSAGECONTENTMASK_MINORVERSION);
        dataSetReaderMessageDataType = &defaultUadpConfiguration;
    }

    /* Sanity-test the configuration */
    if(dataSetReaderMessageDataType &&
       (dataSetReaderMessageDataType->networkMessageNumber != 0 ||
        dataSetReaderMessageDataType->dataSetOffset != 0)) {
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Static DSM configuration not supported. Using defaults");
        dataSetReaderMessageDataType->networkMessageNumber = 0;
        dataSetReaderMessageDataType->dataSetOffset = 0;
    }

    /* The field encoding depends on the flags inside the reader config. */
    if(dataSetReader->config.dataSetFieldContentMask &
       (u64)UA_DATASETFIELDCONTENTMASK_RAWDATA) {
        dataSetMessage->header.fieldEncoding = UA_FIELDENCODING_RAWDATA;
    } else if((u64)dataSetReader->config.dataSetFieldContentMask &
              ((u64)UA_DATASETFIELDCONTENTMASK_SOURCETIMESTAMP |
               (u64)UA_DATASETFIELDCONTENTMASK_SERVERPICOSECONDS |
               (u64)UA_DATASETFIELDCONTENTMASK_SOURCEPICOSECONDS |
               (u64)UA_DATASETFIELDCONTENTMASK_STATUSCODE)) {
        dataSetMessage->header.fieldEncoding = UA_FIELDENCODING_DATAVALUE;
    } else {
        dataSetMessage->header.fieldEncoding = UA_FIELDENCODING_VARIANT;
    }

    /* Std: 'The DataSetMessageContentMask defines the flags for the content
     * of the DataSetMessage header.' */
    if((u64)dataSetReaderMessageDataType->dataSetMessageContentMask &
       (u64)UA_UADPDATASETMESSAGECONTENTMASK_MAJORVERSION) {
        dataSetMessage->header.configVersionMajorVersionEnabled = true;
        dataSetMessage->header.configVersionMajorVersion =
            dataSetReader->config.dataSetMetaData.configurationVersion.majorVersion;
    }

    if((u64)dataSetReaderMessageDataType->dataSetMessageContentMask &
       (u64)UA_UADPDATASETMESSAGECONTENTMASK_MINORVERSION) {
        dataSetMessage->header.configVersionMinorVersionEnabled = true;
        dataSetMessage->header.configVersionMinorVersion =
            dataSetReader->config.dataSetMetaData.configurationVersion.minorVersion;
    }

    if((u64)dataSetReaderMessageDataType->dataSetMessageContentMask &
       (u64)UA_UADPDATASETMESSAGECONTENTMASK_SEQUENCENUMBER) {
        dataSetMessage->header.dataSetMessageSequenceNrEnabled = true;
        dataSetMessage->header.dataSetMessageSequenceNr = 1; // Will be modified when subscriber receives new nw msg.
    }

    if((u64)dataSetReaderMessageDataType->dataSetMessageContentMask &
       (u64)UA_UADPDATASETMESSAGECONTENTMASK_TIMESTAMP) {
        dataSetMessage->header.timestampEnabled = true;
        dataSetMessage->header.timestamp = UA_DateTime_now();
    }

    /* TODO: Picoseconds resolution not supported atm */
    if((u64)dataSetReaderMessageDataType->dataSetMessageContentMask &
       (u64)UA_UADPDATASETMESSAGECONTENTMASK_PICOSECONDS) {
        dataSetMessage->header.picoSecondsIncluded = false;
    }
    /* TODO: Statuscode not supported yet */
    if((u64)dataSetReaderMessageDataType->dataSetMessageContentMask &
       (u64)UA_UADPDATASETMESSAGECONTENTMASK_STATUS) {
        dataSetMessage->header.statusEnabled = false;
    }

    /* Not supported for Delta frames atm*/

    return UA_PubSubDataSetReader_generateKeyFrameMessage(server, dataSetMessage, dataSetReader);
}

static UA_StatusCode
UA_DataSetReader_generateNetworkMessage(UA_PubSubConnection *pubSubConnection, UA_DataSetReader *dataSetReader,
                                        UA_DataSetMessage *dsm, UA_UInt16 *writerId, UA_Byte dsmCount,
                                        UA_NetworkMessage *nm) {
    if(dataSetReader->config.messageSettings.content.decoded.type != &UA_TYPES[UA_TYPES_UADPDATASETREADERMESSAGEDATATYPE])
        return UA_STATUSCODE_BADNOTSUPPORTED;

    UA_UadpDataSetReaderMessageDataType *dsrm = (UA_UadpDataSetReaderMessageDataType *)
        dataSetReader->config.messageSettings.content.decoded.data;
    nm->publisherIdEnabled = ((u64)dsrm->networkMessageContentMask &
                              (u64)UA_UADPNETWORKMESSAGECONTENTMASK_PUBLISHERID) != 0;
    nm->groupHeaderEnabled = ((u64)dsrm->networkMessageContentMask &
                              (u64)UA_UADPNETWORKMESSAGECONTENTMASK_GROUPHEADER) != 0;
    nm->groupHeader.writerGroupIdEnabled = ((u64)dsrm->networkMessageContentMask &
                                            (u64)UA_UADPNETWORKMESSAGECONTENTMASK_WRITERGROUPID) != 0;
    nm->groupHeader.groupVersionEnabled = ((u64)dsrm->networkMessageContentMask &
                                           (u64)UA_UADPNETWORKMESSAGECONTENTMASK_GROUPVERSION) != 0;
    nm->groupHeader.networkMessageNumberEnabled = ((u64)dsrm->networkMessageContentMask &
                                                   (u64)UA_UADPNETWORKMESSAGECONTENTMASK_NETWORKMESSAGENUMBER) != 0;
    nm->groupHeader.sequenceNumberEnabled = ((u64)dsrm->networkMessageContentMask &
                                             (u64)UA_UADPNETWORKMESSAGECONTENTMASK_SEQUENCENUMBER) != 0;
    nm->payloadHeaderEnabled = ((u64)dsrm->networkMessageContentMask &
                                (u64)UA_UADPNETWORKMESSAGECONTENTMASK_PAYLOADHEADER) != 0;
    nm->timestampEnabled = ((u64)dsrm->networkMessageContentMask &
                            (u64)UA_UADPNETWORKMESSAGECONTENTMASK_TIMESTAMP) != 0;
    nm->picosecondsEnabled = ((u64)dsrm->networkMessageContentMask &
                              (u64)UA_UADPNETWORKMESSAGECONTENTMASK_PICOSECONDS) != 0;
    nm->dataSetClassIdEnabled = ((u64)dsrm->networkMessageContentMask &
                                 (u64)UA_UADPNETWORKMESSAGECONTENTMASK_DATASETCLASSID) != 0;
    nm->promotedFieldsEnabled = ((u64)dsrm->networkMessageContentMask &
                                 (u64)UA_UADPNETWORKMESSAGECONTENTMASK_PROMOTEDFIELDS) != 0;
    nm->version = 1;
    nm->networkMessageType = UA_NETWORKMESSAGE_DATASET;

    if(!UA_DataType_isNumeric(dataSetReader->config.publisherId.type))
        return UA_STATUSCODE_BADNOTSUPPORTED;

    switch(dataSetReader->config.publisherId.type->typeKind) {
    case UA_DATATYPEKIND_BYTE:
        nm->publisherIdType = UA_PUBLISHERDATATYPE_BYTE;
        nm->publisherId.publisherIdByte = *(UA_Byte *) dataSetReader->config.publisherId.data;
        break;
    case UA_DATATYPEKIND_UINT16:
        nm->publisherIdType = UA_PUBLISHERDATATYPE_UINT16;
        nm->publisherId.publisherIdUInt16 = *(UA_UInt16 *) dataSetReader->config.publisherId.data;
        break;
    case UA_DATATYPEKIND_UINT32:
        nm->publisherIdType = UA_PUBLISHERDATATYPE_UINT32;
        nm->publisherId.publisherIdUInt32 = *(UA_UInt32 *) dataSetReader->config.publisherId.data;
        break;
    case UA_DATATYPEKIND_UINT64:
        nm->publisherIdType = UA_PUBLISHERDATATYPE_UINT64;
        nm->publisherId.publisherIdUInt64 = *(UA_UInt64 *) dataSetReader->config.publisherId.data;
        break;
    default:
        return UA_STATUSCODE_BADNOTSUPPORTED;
    }

    if(nm->groupHeader.sequenceNumberEnabled)
        nm->groupHeader.sequenceNumber = 1; /* Will be modified when subscriber receives new nw msg. */

    /* Compute the length of the dsm separately for the header */
    UA_UInt16 *dsmLengths = (UA_UInt16 *) UA_calloc(dsmCount, sizeof(UA_UInt16));
    if(!dsmLengths)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    for(UA_Byte i = 0; i < dsmCount; i++)
        dsmLengths[i] = (UA_UInt16) UA_DataSetMessage_calcSizeBinary(&dsm[i], NULL, 0);

    nm->payloadHeader.dataSetPayloadHeader.count = dsmCount;
    nm->payloadHeader.dataSetPayloadHeader.dataSetWriterIds = writerId;
    nm->groupHeader.writerGroupId = dataSetReader->config.writerGroupId;
    nm->groupHeader.networkMessageNumber = 1; /* number of the NetworkMessage inside a PublishingInterval */
    nm->payload.dataSetPayload.sizes = dsmLengths;
    nm->payload.dataSetPayload.dataSetMessages = dsm;
    return UA_STATUSCODE_GOOD;
}

/***************/
/* ReaderGroup */
/***************/

UA_StatusCode
UA_Server_addReaderGroup(UA_Server *server, UA_NodeId connectionIdentifier,
                         const UA_ReaderGroupConfig *readerGroupConfig,
                         UA_NodeId *readerGroupIdentifier) {
    UA_StatusCode retval = UA_STATUSCODE_GOOD;

    /* Check for valid readergroup configuration */
    if(!readerGroupConfig)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    if(!readerGroupConfig->pubsubManagerCallback.addCustomCallback &&
       readerGroupConfig->enableBlockingSocket) {
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Adding ReaderGroup failed, blocking socket functionality only supported in customcallback");
        return UA_STATUSCODE_BADNOTSUPPORTED;
    }

    /* Search the connection by the given connectionIdentifier */
    UA_PubSubConnection *currentConnectionContext =
        UA_PubSubConnection_findConnectionbyId(server, connectionIdentifier);
    if(!currentConnectionContext)
        return UA_STATUSCODE_BADNOTFOUND;

    if(currentConnectionContext->configurationFrozen){
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Adding ReaderGroup failed. Subscriber configuration is frozen.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

    /* Regist (bind) the connection channel if it is not already registered */
    if(!currentConnectionContext->isRegistered) {
        retval |= UA_PubSubConnection_regist(server, &connectionIdentifier);
        if(retval != UA_STATUSCODE_GOOD)
            return retval;
    }

    /* Allocate memory for new reader group */
    UA_ReaderGroup *newGroup = (UA_ReaderGroup *)UA_calloc(1, sizeof(UA_ReaderGroup));
    if(!newGroup)
        return UA_STATUSCODE_BADOUTOFMEMORY;

    newGroup->componentType = UA_PUBSUB_COMPONENT_READERGROUP;
    /* Generate nodeid for the readergroup identifier */
    newGroup->linkedConnection = currentConnectionContext->identifier;

    /* Deep copy of the config */
    retval |= UA_ReaderGroupConfig_copy(readerGroupConfig, &newGroup->config);
    /* Check user configured params and define it accordingly */
    if(newGroup->config.subscribingInterval <= 0.0)
        newGroup->config.subscribingInterval = 5; // Set default to 5 ms

    if(newGroup->config.enableBlockingSocket)
        newGroup->config.timeout = 0; // Set timeout to 0 for blocking socket

    if((!newGroup->config.enableBlockingSocket) && (!newGroup->config.timeout))
        newGroup->config.timeout = 1000; // Set default to 1ms socket timeout when non-blocking socket allows with zero timeout

    LIST_INSERT_HEAD(&currentConnectionContext->readerGroups, newGroup, listEntry);
    currentConnectionContext->readerGroupsSize++;

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
    retval |= addReaderGroupRepresentation(server, newGroup);
#else
    UA_PubSubManager_generateUniqueNodeId(&server->pubSubManager, &newGroup->identifier);
#endif
    if(readerGroupIdentifier)
        UA_NodeId_copy(&newGroup->identifier, readerGroupIdentifier);

    return retval;
}

UA_StatusCode
UA_Server_removeReaderGroup(UA_Server *server, UA_NodeId groupIdentifier) {
    UA_ReaderGroup* readerGroup = UA_ReaderGroup_findRGbyId(server, groupIdentifier);
    if(readerGroup == NULL)
        return UA_STATUSCODE_BADNOTFOUND;

    if(readerGroup->configurationFrozen){
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Remove ReaderGroup failed. Subscriber configuration is frozen.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

    /* Search the connection to which the given readergroup is connected to */
    UA_PubSubConnection *connection =
        UA_PubSubConnection_findConnectionbyId(server, readerGroup->linkedConnection);
    if(connection == NULL)
        return UA_STATUSCODE_BADNOTFOUND;

    /* Unregister subscribe callback */
    if(readerGroup->state == UA_PUBSUBSTATE_OPERATIONAL) {
        if(readerGroup->config.pubsubManagerCallback.removeCustomCallback)
            readerGroup->config.pubsubManagerCallback.
                removeCustomCallback(server, readerGroup->identifier, readerGroup->subscribeCallbackId);
        else
            UA_PubSubManager_removeRepeatedPubSubCallback(server, readerGroup->subscribeCallbackId);
    }

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
    removeReaderGroupRepresentation(server, readerGroup);
#endif

    /* UA_Server_ReaderGroup_clear also removes itself from the list */
    UA_Server_ReaderGroup_clear(server, readerGroup);
    /* Remove readerGroup from Connection */
    LIST_REMOVE(readerGroup, listEntry);
    UA_free(readerGroup);
    return UA_STATUSCODE_GOOD;
}

/* TODO: Implement
UA_StatusCode
UA_Server_ReaderGroup_updateConfig(UA_Server *server, UA_NodeId readerGroupIdentifier,
                                   const UA_ReaderGroupConfig *config) {
    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}
*/

UA_StatusCode
UA_Server_ReaderGroup_getConfig(UA_Server *server, UA_NodeId readerGroupIdentifier,
                                UA_ReaderGroupConfig *config) {
    if(!config)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    /* Identify the readergroup through the readerGroupIdentifier */
    UA_ReaderGroup *currentReaderGroup = UA_ReaderGroup_findRGbyId(server, readerGroupIdentifier);
    if(!currentReaderGroup)
        return UA_STATUSCODE_BADNOTFOUND;

    UA_ReaderGroupConfig tmpReaderGroupConfig;
    /* deep copy of the actual config */
    UA_ReaderGroupConfig_copy(&currentReaderGroup->config, &tmpReaderGroupConfig);
    *config = tmpReaderGroupConfig;
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_Server_ReaderGroup_getState(UA_Server *server, UA_NodeId readerGroupIdentifier,
                               UA_PubSubState *state) {
    if((server == NULL) || (state == NULL))
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    UA_ReaderGroup *currentReaderGroup = UA_ReaderGroup_findRGbyId(server, readerGroupIdentifier);
    if(!currentReaderGroup)
        return UA_STATUSCODE_BADNOTFOUND;
    *state = currentReaderGroup->state;
    return UA_STATUSCODE_GOOD;
}

void
UA_ReaderGroupConfig_clear(UA_ReaderGroupConfig *readerGroupConfig) {
    //delete readerGroup config
    UA_String_clear(&readerGroupConfig->name);
}

static void
UA_Server_ReaderGroup_clear(UA_Server* server, UA_ReaderGroup *readerGroup) {
    UA_ReaderGroupConfig_clear(&readerGroup->config);
    UA_DataSetReader *dataSetReader;
    UA_DataSetReader *tmpDataSetReader;
    LIST_FOREACH_SAFE(dataSetReader, &readerGroup->readers, listEntry, tmpDataSetReader) {
        UA_Server_removeDataSetReader(server, dataSetReader->identifier);
    }
    UA_PubSubConnection* pConn =
        UA_PubSubConnection_findConnectionbyId(server, readerGroup->linkedConnection);
    if(pConn != NULL)
        pConn->readerGroupsSize--;

    /* Delete ReaderGroup and its members */
    UA_String_clear(&readerGroup->config.name);
    UA_NodeId_clear(&readerGroup->linkedConnection);
    UA_NodeId_clear(&readerGroup->identifier);

#ifdef UA_ENABLE_PUBSUB_ENCRYPTION
    if(readerGroup->config.securityPolicy && readerGroup->securityPolicyContext) {
        readerGroup->config.securityPolicy->deleteContext(readerGroup->securityPolicyContext);
        readerGroup->securityPolicyContext = NULL;
    }
#endif
}

UA_StatusCode
UA_ReaderGroupConfig_copy(const UA_ReaderGroupConfig *src,
                          UA_ReaderGroupConfig *dst) {
    /* Currently simple memcpy only */
    memcpy(dst, src, sizeof(UA_ReaderGroupConfig));
    memcpy(&dst->securityParameters, &src->securityParameters, sizeof(UA_PubSubSecurityParameters));
    UA_String_copy(&src->name, &dst->name);
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_ReaderGroup_setPubSubState(UA_Server *server, UA_PubSubState state, UA_ReaderGroup *readerGroup) {
    UA_DataSetReader *dataSetReader;
    switch(state){
        case UA_PUBSUBSTATE_DISABLED:
            switch (readerGroup->state){
                case UA_PUBSUBSTATE_DISABLED:
                    return UA_STATUSCODE_GOOD;
                case UA_PUBSUBSTATE_PAUSED:
                    break;
                case UA_PUBSUBSTATE_OPERATIONAL:
                    if(readerGroup->config.pubsubManagerCallback.removeCustomCallback)
                        readerGroup->config.pubsubManagerCallback.removeCustomCallback(server, readerGroup->identifier, readerGroup->subscribeCallbackId);
                    else
                        UA_PubSubManager_removeRepeatedPubSubCallback(server, readerGroup->subscribeCallbackId);

                    LIST_FOREACH(dataSetReader, &readerGroup->readers, listEntry){
                        UA_DataSetReader_setPubSubState(server, UA_PUBSUBSTATE_DISABLED, dataSetReader);
                    }
                    readerGroup->state = UA_PUBSUBSTATE_DISABLED;
                    break;
                case UA_PUBSUBSTATE_ERROR:
                    break;
                default:
                    UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                   "Received unknown PubSub state!");
            }
            break;
        case UA_PUBSUBSTATE_PAUSED:
            UA_LOG_DEBUG(&server->config.logger, UA_LOGCATEGORY_SERVER,
                            "PubSub state paused is unsupported at the moment!");
            switch (readerGroup->state){
                case UA_PUBSUBSTATE_DISABLED:
                    break;
                case UA_PUBSUBSTATE_PAUSED:
                    return UA_STATUSCODE_GOOD;
                case UA_PUBSUBSTATE_OPERATIONAL:
                    break;
                case UA_PUBSUBSTATE_ERROR:
                    break;
                default:
                    UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                   "Received unknown PubSub state!");
            }
            break;
        case UA_PUBSUBSTATE_OPERATIONAL:
            switch (readerGroup->state){
                case UA_PUBSUBSTATE_DISABLED:
                    readerGroup->state = UA_PUBSUBSTATE_OPERATIONAL;
                    if(readerGroup->config.pubsubManagerCallback.removeCustomCallback)
                        readerGroup->config.pubsubManagerCallback.removeCustomCallback(server, readerGroup->identifier, readerGroup->subscribeCallbackId);
                    else
                        UA_PubSubManager_removeRepeatedPubSubCallback(server, readerGroup->subscribeCallbackId);

                    LIST_FOREACH(dataSetReader, &readerGroup->readers, listEntry){
                        UA_DataSetReader_setPubSubState(server, UA_PUBSUBSTATE_OPERATIONAL, dataSetReader);
                    }
                    UA_ReaderGroup_addSubscribeCallback(server, readerGroup);
                    break;
                case UA_PUBSUBSTATE_PAUSED:
                    break;
                case UA_PUBSUBSTATE_OPERATIONAL:
                    return UA_STATUSCODE_GOOD;
                case UA_PUBSUBSTATE_ERROR:
                    break;
                default:
                    UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                   "Received unknown PubSub state!");
            }
            break;
        case UA_PUBSUBSTATE_ERROR:
            switch (readerGroup->state){
                case UA_PUBSUBSTATE_DISABLED:
                    break;
                case UA_PUBSUBSTATE_PAUSED:
                    break;
                case UA_PUBSUBSTATE_OPERATIONAL:
                    UA_PubSubManager_removeRepeatedPubSubCallback(server, readerGroup->subscribeCallbackId);
                    LIST_FOREACH(dataSetReader, &readerGroup->readers, listEntry){
                        UA_DataSetReader_setPubSubState(server, UA_PUBSUBSTATE_ERROR, dataSetReader);
                    }
                    break;
                case UA_PUBSUBSTATE_ERROR:
                    return UA_STATUSCODE_GOOD;
                default:
                    UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                   "Received unknown PubSub state!");
            }
            readerGroup->state = UA_PUBSUBSTATE_ERROR;
            break;
        default:
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "Received unknown PubSub state!");
    }
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_Server_freezeReaderGroupConfiguration(UA_Server *server, const UA_NodeId readerGroupId) {
    UA_ReaderGroup *rg = UA_ReaderGroup_findRGbyId(server, readerGroupId);
    if(!rg)
        return UA_STATUSCODE_BADNOTFOUND;

    //PubSubConnection freezeCounter++
    UA_NodeId pubSubConnectionId =  rg->linkedConnection;
    UA_PubSubConnection *pubSubConnection = UA_PubSubConnection_findConnectionbyId(server, pubSubConnectionId);
    pubSubConnection->configurationFreezeCounter++;
    pubSubConnection->configurationFrozen = UA_TRUE;
    //ReaderGroup freeze
    rg->configurationFrozen = UA_TRUE;
    // TODO: Clarify on the freeze functionality in multiple DSR, multiple networkMessage conf in a RG
    //DataSetReader freeze
    UA_DataSetReader *dataSetReader;
    UA_UInt16 dsrCount = 0;
    LIST_FOREACH(dataSetReader, &rg->readers, listEntry){
    	dataSetReader->configurationFrozen = UA_TRUE;
        dsrCount++;
        /* TODO: Configuration frozen for subscribedDataSet once
         * UA_Server_DataSetReader_addTargetVariables API modified to support
         * adding target variable one by one or in a group stored in a list.
         */
    }

    if(rg->config.rtLevel == UA_PUBSUB_RT_FIXED_SIZE) {
        if(dsrCount > 1) {
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "Mutiple DSR in a readerGroup not supported in RT fixed size configuration");
            return UA_STATUSCODE_BADNOTIMPLEMENTED;
        }

        dataSetReader = LIST_FIRST(&rg->readers);
        // Support only to UADP encoding
        if(dataSetReader->config.messageSettings.content.decoded.type != &UA_TYPES[UA_TYPES_UADPDATASETREADERMESSAGEDATATYPE]) {
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "PubSub-RT configuration fail: Non-RT capable encoding.");
            return UA_STATUSCODE_BADNOTSUPPORTED;
        }

        size_t fieldsSize = dataSetReader->config.dataSetMetaData.fieldsSize;
        for(size_t i = 0; i < fieldsSize; i++) {
            const UA_VariableNode *rtNode = (const UA_VariableNode *)
                UA_NODESTORE_GET(server, &dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariable.targetNodeId);
            if(rtNode != NULL && rtNode->valueBackend.backendType != UA_VALUEBACKENDTYPE_EXTERNAL){
                UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                               "PubSub-RT configuration fail: PDS contains field without external data source.");
                UA_NODESTORE_RELEASE(server, (const UA_Node *) rtNode);
                return UA_STATUSCODE_BADNOTSUPPORTED;
            }

            UA_NODESTORE_RELEASE(server, (const UA_Node *) rtNode);
            if((UA_NodeId_equal(&dataSetReader->config.dataSetMetaData.fields[i].dataType, &UA_TYPES[UA_TYPES_STRING].typeId) ||
                UA_NodeId_equal(&dataSetReader->config.dataSetMetaData.fields[i].dataType,
                                &UA_TYPES[UA_TYPES_BYTESTRING].typeId)) &&
                                dataSetReader->config.dataSetMetaData.fields[i].maxStringLength == 0) {
                UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                               "PubSub-RT configuration fail: "
                               "PDS contains String/ByteString with dynamic length.");
                return UA_STATUSCODE_BADNOTSUPPORTED;
            } else if(!UA_DataType_isNumeric(UA_findDataType(&dataSetReader->config.dataSetMetaData.fields[i].dataType))){
                UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                               "PubSub-RT configuration fail: "
                               "PDS contains variable with dynamic size.");
                return UA_STATUSCODE_BADNOTSUPPORTED;
            }
        }

        UA_DataSetMessage *dsm = (UA_DataSetMessage *) UA_calloc(1, sizeof(UA_DataSetMessage));
        if(!dsm) {
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                         "PubSub RT Offset calculation: DSM creation failed");
            return UA_STATUSCODE_BADOUTOFMEMORY;
        }

        /* Generate the DSM */
        UA_StatusCode res = UA_DataSetReader_generateDataSetMessage(server, dsm, dataSetReader);
        if(res != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "PubSub RT Offset calculation: DataSetMessage generation failed");
            UA_DataSetMessage_clear(dsm);
            UA_free(dsm);
            return UA_STATUSCODE_BADINTERNALERROR;
        }

        /* Generate data set messages - Considering 1 DSM as max */
        UA_UInt16 *dsWriterIds = (UA_UInt16 *)UA_calloc(1, sizeof(UA_UInt16));
        if(!dsWriterIds) {
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                         "PubSub RT Offset calculation: DataSetWriterId creation failed");
            UA_DataSetMessage_clear(dsm);
            UA_free(dsm);
            return UA_STATUSCODE_BADOUTOFMEMORY;
        }
        *dsWriterIds = dataSetReader->config.dataSetWriterId;

        UA_NetworkMessage *networkMessage = (UA_NetworkMessage *)UA_calloc(1, sizeof(UA_NetworkMessage));
        if(!networkMessage) {
            UA_free(dsWriterIds);
            UA_DataSetMessage_clear(dsm);
            UA_free(dsm);
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                         "PubSub RT Offset calculation: Network message creation failed");
            return UA_STATUSCODE_BADOUTOFMEMORY;
        }

        res = UA_DataSetReader_generateNetworkMessage(pubSubConnection, dataSetReader, dsm,
                                                      dsWriterIds, 1, networkMessage);
        if(res != UA_STATUSCODE_GOOD) {
            UA_free(networkMessage->payload.dataSetPayload.sizes);
            UA_free(networkMessage);
            UA_free(dsWriterIds);
            UA_DataSetMessage_clear(dsm);
            UA_free(dsm);
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "PubSub RT Offset calculation: NetworkMessage generation failed");
            return UA_STATUSCODE_BADINTERNALERROR;
        }

        memset(&dataSetReader->bufferedMessage, 0, sizeof(UA_NetworkMessageOffsetBuffer));
        dataSetReader->bufferedMessage.RTsubscriberEnabled = UA_TRUE;
        /* Fix the offsets necessary to decode */
        UA_NetworkMessage_calcSizeBinary(networkMessage, &dataSetReader->bufferedMessage);
        dataSetReader->bufferedMessage.nm = networkMessage;
    }

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_Server_unfreezeReaderGroupConfiguration(UA_Server *server, const UA_NodeId readerGroupId){
    UA_ReaderGroup *rg = UA_ReaderGroup_findRGbyId(server, readerGroupId);
    if(!rg)
        return UA_STATUSCODE_BADNOTFOUND;
    //PubSubConnection freezeCounter--
    UA_NodeId pubSubConnectionId =  rg->linkedConnection;
    UA_PubSubConnection *pubSubConnection = UA_PubSubConnection_findConnectionbyId(server, pubSubConnectionId);
    pubSubConnection->configurationFreezeCounter--;
    if(pubSubConnection->configurationFreezeCounter == 0){
        pubSubConnection->configurationFrozen = UA_FALSE;
    }
    //ReaderGroup unfreeze
    rg->configurationFrozen = UA_FALSE;
    //DataSetReader unfreeze
    UA_DataSetReader *dataSetReader;
    LIST_FOREACH(dataSetReader, &rg->readers, listEntry) {
        dataSetReader->configurationFrozen = UA_FALSE;
    }

    if(rg->config.rtLevel == UA_PUBSUB_RT_FIXED_SIZE) {
        dataSetReader = LIST_FIRST(&rg->readers);
        if(dataSetReader->bufferedMessage.offsetsSize > 0){
            for (size_t i = 0; i < dataSetReader->bufferedMessage.offsetsSize; i++) {
                if(dataSetReader->bufferedMessage.offsets[i].contentType == UA_PUBSUB_OFFSETTYPE_PAYLOAD_VARIANT){
                    UA_DataValue_delete(dataSetReader->bufferedMessage.offsets[i].offsetData.value.value);
                }
            }

            UA_free(dataSetReader->bufferedMessage.offsets);
        }

        if(dataSetReader->bufferedMessage.RTsubscriberEnabled) {
            if(dataSetReader->bufferedMessage.nm != NULL) {
                UA_NetworkMessage_delete(dataSetReader->bufferedMessage.nm);
                UA_free(dataSetReader->bufferedMessage.nm);
            }
        }
    }

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode UA_EXPORT
UA_Server_setReaderGroupOperational(UA_Server *server, const UA_NodeId readerGroupId){
    UA_ReaderGroup *rg = UA_ReaderGroup_findRGbyId(server, readerGroupId);
    if(!rg)
        return UA_STATUSCODE_BADNOTFOUND;
    return UA_ReaderGroup_setPubSubState(server, UA_PUBSUBSTATE_OPERATIONAL, rg);
}

UA_StatusCode UA_EXPORT
UA_Server_setReaderGroupDisabled(UA_Server *server, const UA_NodeId readerGroupId){
    UA_ReaderGroup *rg = UA_ReaderGroup_findRGbyId(server, readerGroupId);
    if(!rg)
        return UA_STATUSCODE_BADNOTFOUND;
    return UA_ReaderGroup_setPubSubState(server, UA_PUBSUBSTATE_DISABLED, rg);
}

static UA_StatusCode
checkReaderIdentifier(UA_Server *server, UA_NetworkMessage *pMsg, UA_DataSetReader *reader) {
    if(!pMsg->groupHeaderEnabled ||
       !pMsg->groupHeader.writerGroupIdEnabled ||
       !pMsg->payloadHeaderEnabled) {
        UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER,
                    "Cannot process DataSetReader without WriterGroup"
                    "and DataSetWriter identifiers");
        return UA_STATUSCODE_BADNOTIMPLEMENTED;
    }

    switch (pMsg->publisherIdType) {
    case UA_PUBLISHERDATATYPE_BYTE:
        if(reader->config.publisherId.type == &UA_TYPES[UA_TYPES_BYTE] &&
           pMsg->publisherIdType == UA_PUBLISHERDATATYPE_BYTE &&
           pMsg->publisherId.publisherIdByte == *(UA_Byte*)reader->config.publisherId.data)
            break;
        return UA_STATUSCODE_BADNOTFOUND;
    case UA_PUBLISHERDATATYPE_UINT16:
        if(reader->config.publisherId.type == &UA_TYPES[UA_TYPES_UINT16] &&
           pMsg->publisherIdType == UA_PUBLISHERDATATYPE_UINT16 &&
           pMsg->publisherId.publisherIdUInt16 == *(UA_UInt16*)reader->config.publisherId.data)
            break;
        return UA_STATUSCODE_BADNOTFOUND;
    case UA_PUBLISHERDATATYPE_UINT32:
        if(reader->config.publisherId.type == &UA_TYPES[UA_TYPES_UINT32] &&
           pMsg->publisherIdType == UA_PUBLISHERDATATYPE_UINT32 &&
           pMsg->publisherId.publisherIdUInt32 == *(UA_UInt32*)reader->config.publisherId.data)
            break;
        return UA_STATUSCODE_BADNOTFOUND;
    case UA_PUBLISHERDATATYPE_UINT64:
        if(reader->config.publisherId.type == &UA_TYPES[UA_TYPES_UINT64] &&
           pMsg->publisherIdType == UA_PUBLISHERDATATYPE_UINT64 &&
           pMsg->publisherId.publisherIdUInt64 == *(UA_UInt64*)reader->config.publisherId.data)
            break;
        return UA_STATUSCODE_BADNOTFOUND;
    case UA_PUBLISHERDATATYPE_STRING:
        if(reader->config.publisherId.type == &UA_TYPES[UA_TYPES_STRING] &&
           pMsg->publisherIdType == UA_PUBLISHERDATATYPE_STRING &&
           UA_String_equal(&pMsg->publisherId.publisherIdString,
                           (UA_String*)reader->config.publisherId.data))
            break;
        return UA_STATUSCODE_BADNOTFOUND;
    default:
        return UA_STATUSCODE_BADNOTFOUND;
    }

    if((reader->config.writerGroupId == pMsg->groupHeader.writerGroupId) &&
       (reader->config.dataSetWriterId == *pMsg->payloadHeader.dataSetPayloadHeader.dataSetWriterIds)) {
        UA_LOG_DEBUG(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "DataSetReader found. Process NetworkMessage");
        return UA_STATUSCODE_GOOD;
    }

    return UA_STATUSCODE_BADNOTFOUND;
}

UA_ReaderGroup *
UA_ReaderGroup_findRGbyId(UA_Server *server, UA_NodeId identifier) {
    UA_PubSubConnection *pubSubConnection;
    TAILQ_FOREACH(pubSubConnection, &server->pubSubManager.connections, listEntry){
        UA_ReaderGroup* readerGroup = NULL;
        LIST_FOREACH(readerGroup, &pubSubConnection->readerGroups, listEntry) {
            if(UA_NodeId_equal(&identifier, &readerGroup->identifier))
                return readerGroup;
        }
    }
    return NULL;
}

UA_DataSetReader *UA_ReaderGroup_findDSRbyId(UA_Server *server, UA_NodeId identifier) {
    UA_PubSubConnection *pubSubConnection;
    TAILQ_FOREACH(pubSubConnection, &server->pubSubManager.connections, listEntry){
        UA_ReaderGroup* readerGroup = NULL;
        LIST_FOREACH(readerGroup, &pubSubConnection->readerGroups, listEntry) {
            UA_DataSetReader *tmpReader;
            LIST_FOREACH(tmpReader, &readerGroup->readers, listEntry) {
                if(UA_NodeId_equal(&tmpReader->identifier, &identifier)) {
                    return tmpReader;
                }
            }
        }
    }
    return NULL;
}

/* This  triggers the collection and reception of NetworkMessages and the
 * contained DataSetMessages. */
void
UA_ReaderGroup_subscribeCallback(UA_Server *server, UA_ReaderGroup *readerGroup) {
    // TODO: feedback for debug-assert vs runtime-check
    UA_assert(server);
    UA_assert(readerGroup);

    UA_LOG_DEBUG(&server->config.logger, UA_LOGCATEGORY_SERVER, "PubSub subscribe callback");

    UA_PubSubConnection *connection =
        UA_PubSubConnection_findConnectionbyId(server, readerGroup->linkedConnection);
    if(!connection) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER, "SubscribeCallback(): "
            "Find linked connection failed");
        UA_ReaderGroup_setPubSubState(server, UA_PUBSUBSTATE_ERROR, readerGroup);
        return;
    }

    receiveBufferedNetworkMessage(server, readerGroup, connection);
}


/* Add new subscribeCallback. The first execution is triggered directly after
 * creation. */
UA_StatusCode
UA_ReaderGroup_addSubscribeCallback(UA_Server *server, UA_ReaderGroup *readerGroup) {
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    if(readerGroup->config.pubsubManagerCallback.addCustomCallback)
        retval |= readerGroup->config.pubsubManagerCallback.addCustomCallback(server, readerGroup->identifier,
                                                                              (UA_ServerCallback) UA_ReaderGroup_subscribeCallback,
                                                                              readerGroup,
                                                                              readerGroup->config.subscribingInterval,
                                                                              NULL,                                         // TODO: Send base time from reader group config
                                                                              UA_TIMER_HANDLE_CYCLEMISS_WITH_CURRENTTIME,   // TODO: Send timer policy from reader group config
                                                                              &readerGroup->subscribeCallbackId);
    else {
        if(readerGroup->config.enableBlockingSocket == UA_TRUE) {
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                           "addSubscribeCallback() failed, blocking socket functionality only supported in customcallback");
            return UA_STATUSCODE_BADNOTSUPPORTED;
        }

        retval |= UA_PubSubManager_addRepeatedCallback(server,
                                                       (UA_ServerCallback) UA_ReaderGroup_subscribeCallback,
                                                       readerGroup,
                                                       readerGroup->config.subscribingInterval,
                                                       NULL,                                        // TODO: Send base time from reader group config
                                                       UA_TIMER_HANDLE_CYCLEMISS_WITH_CURRENTTIME,  // TODO: Send timer policy from reader group config
                                                       &readerGroup->subscribeCallbackId);
    }

    if(retval == UA_STATUSCODE_GOOD)
        readerGroup->subscribeCallbackIsRegistered = true;

    /* Run once after creation */
    /* When using blocking socket functionality, the server mechanism might get blocked.
     * It is highly recommended to use custom callback when using blockingsocket. */
    if(readerGroup->config.enableBlockingSocket != UA_TRUE)
        UA_ReaderGroup_subscribeCallback(server, readerGroup);

    return retval;
}

/**********/
/* Reader */
/**********/

UA_StatusCode
UA_Server_addDataSetReader(UA_Server *server, UA_NodeId readerGroupIdentifier,
                           const UA_DataSetReaderConfig *dataSetReaderConfig,
                           UA_NodeId *readerIdentifier) {
    /* Search the reader group by the given readerGroupIdentifier */
    UA_ReaderGroup *readerGroup = UA_ReaderGroup_findRGbyId(server, readerGroupIdentifier);
    if(readerGroup == NULL)
        return UA_STATUSCODE_BADNOTFOUND;

    if(!dataSetReaderConfig)
        return UA_STATUSCODE_BADNOTFOUND;

    if(readerGroup->configurationFrozen){
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Add DataSetReader failed. Subscriber configuration is frozen.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

    /* Allocate memory for new DataSetReader */
    UA_DataSetReader *newDataSetReader = (UA_DataSetReader *)UA_calloc(1, sizeof(UA_DataSetReader));
    if(!newDataSetReader)
        return UA_STATUSCODE_BADOUTOFMEMORY;

    newDataSetReader->componentType = UA_PUBSUB_COMPONENT_DATASETREADER;
    if(readerGroup->state == UA_PUBSUBSTATE_OPERATIONAL) {
        UA_StatusCode retVal = UA_DataSetReader_setPubSubState(server, UA_PUBSUBSTATE_OPERATIONAL, newDataSetReader);
        if(retVal != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                            "Add DataSetReader failed. setPubSubState failed.");
            return retVal;
        }
    }

    /* Copy the config into the new dataSetReader */
    UA_DataSetReaderConfig_copy(dataSetReaderConfig, &newDataSetReader->config);
    newDataSetReader->linkedReaderGroup = readerGroup->identifier;

    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
#ifdef UA_ENABLE_PUBSUB_MONITORING
    /* create message receive timeout timer */
    retVal = server->config.pubSubConfig.monitoringInterface.
        createMonitoring(server, newDataSetReader->identifier,
                         UA_PUBSUB_COMPONENT_DATASETREADER,
                         UA_PUBSUB_MONITORING_MESSAGE_RECEIVE_TIMEOUT,
                         newDataSetReader, UA_DataSetReader_handleMessageReceiveTimeout);
    if(retVal != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
            "Add DataSetReader failed. Create message receive timeout timer failed.");
        UA_DataSetReaderConfig_clear(&newDataSetReader->config);
        UA_free(newDataSetReader);
        newDataSetReader = 0;
        return retVal;
    }
#endif /* UA_ENABLE_PUBSUB_MONITORING */

    /* Add the new reader to the group */
    LIST_INSERT_HEAD(&readerGroup->readers, newDataSetReader, listEntry);
    readerGroup->readersCount++;

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
    retVal = addDataSetReaderRepresentation(server, newDataSetReader);
#else
    UA_PubSubManager_generateUniqueNodeId(&server->pubSubManager,
                                          &newDataSetReader->identifier);
#endif
    if(readerIdentifier)
        UA_NodeId_copy(&newDataSetReader->identifier, readerIdentifier);

    return retVal;
}

UA_StatusCode
UA_Server_removeDataSetReader(UA_Server *server, UA_NodeId readerIdentifier) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    /* Remove datasetreader given by the identifier */
    UA_DataSetReader *dataSetReader = UA_ReaderGroup_findDSRbyId(server, readerIdentifier);
    if(!dataSetReader)
        return UA_STATUSCODE_BADNOTFOUND;

    if(dataSetReader->configurationFrozen){
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Remove DataSetReader failed. Subscriber configuration is frozen.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
    removeDataSetReaderRepresentation(server, dataSetReader);
#endif

#ifdef UA_ENABLE_PUBSUB_MONITORING
    /* stop and remove message receive timeout timer */
    retVal = UA_STATUSCODE_GOOD;
    if(dataSetReader->msgRcvTimeoutTimerRunning == UA_TRUE) {
        retVal = server->config.pubSubConfig.monitoringInterface.stopMonitoring(server, dataSetReader->identifier,
            UA_PUBSUB_COMPONENT_DATASETREADER, UA_PUBSUB_MONITORING_MESSAGE_RECEIVE_TIMEOUT, dataSetReader);
        if(retVal != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                "Remove DataSetReader failed. Stop message receive timeout timer of DataSetReader "
                    "'%.*s' failed.", (int) dataSetReader->config.name.length, dataSetReader->config.name.data);
        }
    }
    retVal |= server->config.pubSubConfig.monitoringInterface.deleteMonitoring(server, dataSetReader->identifier,
        UA_PUBSUB_COMPONENT_DATASETREADER, UA_PUBSUB_MONITORING_MESSAGE_RECEIVE_TIMEOUT, dataSetReader);
    if(retVal != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
            "Remove DataSetReader failed. Delete message receive timeout timer of DataSetReader "
                "'%.*s' failed.", (int) dataSetReader->config.name.length, dataSetReader->config.name.data);
    }
#endif /* UA_ENABLE_PUBSUB_MONITORING */

    UA_DataSetReader_clear(server, dataSetReader);
    return retVal;
}

UA_StatusCode
UA_Server_DataSetReader_updateConfig(UA_Server *server, UA_NodeId dataSetReaderIdentifier,
                                     UA_NodeId readerGroupIdentifier, const UA_DataSetReaderConfig *config) {
    if(config == NULL)
       return UA_STATUSCODE_BADINVALIDARGUMENT;

    UA_DataSetReader *currentDataSetReader = UA_ReaderGroup_findDSRbyId(server, dataSetReaderIdentifier);
    if(!currentDataSetReader)
       return UA_STATUSCODE_BADNOTFOUND;

    if(currentDataSetReader->configurationFrozen){
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Update DataSetReader config failed. Subscriber configuration is frozen.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

    UA_ReaderGroup *currentReaderGroup = UA_ReaderGroup_findRGbyId(server, readerGroupIdentifier);
    if(currentReaderGroup->configurationFrozen){
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Update DataSetReader config failed. Subscriber configuration is frozen.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

    /* The update functionality will be extended during the next PubSub batches.
     * Currently changes for writerGroupId, dataSetWriterId and TargetVariables are possible. */
    if(currentDataSetReader->config.writerGroupId != config->writerGroupId)
        currentDataSetReader->config.writerGroupId = config->writerGroupId;

    if(currentDataSetReader->config.dataSetWriterId != config->dataSetWriterId)
        currentDataSetReader->config.dataSetWriterId = config->dataSetWriterId;

    if(currentDataSetReader->config.subscribedDataSetType != UA_PUBSUB_SDS_TARGET) {
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Unsupported SubscribedDataSetType.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

    UA_TargetVariables *oldTV = &currentDataSetReader->config.subscribedDataSet.subscribedDataSetTarget;
    const UA_TargetVariables *newTV = &config->subscribedDataSet.subscribedDataSetTarget;
    if(oldTV->targetVariablesSize == newTV->targetVariablesSize) {
        for(size_t i = 0; i < config->subscribedDataSet.subscribedDataSetTarget.targetVariablesSize; i++) {
            if(!UA_NodeId_equal(&oldTV->targetVariables[i].targetVariable.targetNodeId,
                                &newTV->targetVariables[i].targetVariable.targetNodeId)) {
                UA_Server_DataSetReader_createTargetVariables(server, currentDataSetReader->identifier,
                                                              newTV->targetVariablesSize, newTV->targetVariables);
            }
        }
    } else {
        UA_Server_DataSetReader_createTargetVariables(server, currentDataSetReader->identifier,
                                                      newTV->targetVariablesSize, newTV->targetVariables);
    }

    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
#ifdef UA_ENABLE_PUBSUB_MONITORING
    if(currentDataSetReader->config.messageReceiveTimeout != config->messageReceiveTimeout) {
        /* update message receive timeout timer interval */
        currentDataSetReader->config.messageReceiveTimeout = config->messageReceiveTimeout;
        retVal = server->config.pubSubConfig.monitoringInterface.updateMonitoringInterval(server, currentDataSetReader->identifier,
            UA_PUBSUB_COMPONENT_DATASETREADER, UA_PUBSUB_MONITORING_MESSAGE_RECEIVE_TIMEOUT, currentDataSetReader);
        if(retVal != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                "Update DataSetReader message receive timeout timer failed.");
        }
    }
#endif /* UA_ENABLE_PUBSUB_MONITORING */
    return retVal;
}

UA_StatusCode
UA_Server_DataSetReader_getConfig(UA_Server *server, UA_NodeId dataSetReaderIdentifier,
                                 UA_DataSetReaderConfig *config) {
    if(!config)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    UA_DataSetReader *currentDataSetReader =
        UA_ReaderGroup_findDSRbyId(server, dataSetReaderIdentifier);
    if(!currentDataSetReader)
        return UA_STATUSCODE_BADNOTFOUND;

    UA_DataSetReaderConfig tmpReaderConfig;
    /* Deep copy of the actual config */
    UA_DataSetReaderConfig_copy(&currentDataSetReader->config, &tmpReaderConfig);
    *config = tmpReaderConfig;
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_DataSetReaderConfig_copy(const UA_DataSetReaderConfig *src,
                            UA_DataSetReaderConfig *dst) {
    memset(dst, 0, sizeof(UA_DataSetReaderConfig));
    UA_StatusCode retVal = UA_String_copy(&src->name, &dst->name);
    if(retVal != UA_STATUSCODE_GOOD)
        return retVal;

    retVal = UA_Variant_copy(&src->publisherId, &dst->publisherId);
    if(retVal != UA_STATUSCODE_GOOD)
        return retVal;

    dst->writerGroupId = src->writerGroupId;
    dst->dataSetWriterId = src->dataSetWriterId;
    retVal = UA_DataSetMetaDataType_copy(&src->dataSetMetaData, &dst->dataSetMetaData);
    if(retVal != UA_STATUSCODE_GOOD)
        return retVal;

    dst->dataSetFieldContentMask = src->dataSetFieldContentMask;
    dst->messageReceiveTimeout = src->messageReceiveTimeout;

    /* Currently memcpy is used to copy the securityParameters */
    memcpy(&dst->securityParameters, &src->securityParameters, sizeof(UA_PubSubSecurityParameters));
    retVal = UA_ExtensionObject_copy(&src->messageSettings, &dst->messageSettings);
    if(retVal != UA_STATUSCODE_GOOD)
        return retVal;

    retVal = UA_ExtensionObject_copy(&src->transportSettings, &dst->transportSettings);
    if(retVal != UA_STATUSCODE_GOOD)
        return retVal;

    if(src->subscribedDataSetType == UA_PUBSUB_SDS_TARGET) {
        retVal = UA_TargetVariables_copy(&src->subscribedDataSet.subscribedDataSetTarget,
                                         &dst->subscribedDataSet.subscribedDataSetTarget);
    }
    return retVal;
}

void
UA_DataSetReaderConfig_clear(UA_DataSetReaderConfig *cfg) {
    UA_String_clear(&cfg->name);
    UA_Variant_clear(&cfg->publisherId);
    UA_DataSetMetaDataType_clear(&cfg->dataSetMetaData);
    UA_ExtensionObject_clear(&cfg->messageSettings);
    UA_ExtensionObject_clear(&cfg->transportSettings);
    if(cfg->subscribedDataSetType == UA_PUBSUB_SDS_TARGET) {
        UA_TargetVariables_clear(&cfg->subscribedDataSet.subscribedDataSetTarget);
    }
}

UA_StatusCode
UA_Server_DataSetReader_getState(UA_Server *server, UA_NodeId dataSetReaderIdentifier,
                               UA_PubSubState *state) {

    if((server == NULL) || (state == NULL))
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    UA_DataSetReader *currentDataSetReader = UA_ReaderGroup_findDSRbyId(server, dataSetReaderIdentifier);
    if(currentDataSetReader == NULL)
        return UA_STATUSCODE_BADNOTFOUND;
    *state = currentDataSetReader->state;
    return UA_STATUSCODE_GOOD;
}

//state machine methods not part of the open62541 state machine API
UA_StatusCode
UA_DataSetReader_setPubSubState(UA_Server *server, UA_PubSubState state, UA_DataSetReader *dataSetReader) {
    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    switch(state){
        case UA_PUBSUBSTATE_DISABLED:
            switch(dataSetReader->state){
                case UA_PUBSUBSTATE_DISABLED:
                    return UA_STATUSCODE_GOOD;
                case UA_PUBSUBSTATE_PAUSED:
                    dataSetReader->state = UA_PUBSUBSTATE_DISABLED;
                    break;
                case UA_PUBSUBSTATE_OPERATIONAL:
#ifdef UA_ENABLE_PUBSUB_MONITORING
                    /* stop MessageReceiveTimeout timer */
                    if(dataSetReader->msgRcvTimeoutTimerRunning == UA_TRUE) {
                        ret = server->config.pubSubConfig.monitoringInterface.stopMonitoring(server, dataSetReader->identifier,
                            UA_PUBSUB_COMPONENT_DATASETREADER, UA_PUBSUB_MONITORING_MESSAGE_RECEIVE_TIMEOUT, dataSetReader);
                        if(ret == UA_STATUSCODE_GOOD) {
                            dataSetReader->msgRcvTimeoutTimerRunning = UA_FALSE;
                        } else {
                            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                            "Disable ReaderGroup failed. Stop message receive timeout timer of DataSetReader "
                            "'%.*s' failed.", (int) dataSetReader->config.name.length, dataSetReader->config.name.data);
                        }
                    }
#endif /* UA_ENABLE_PUBSUB_MONITORING */
                    if(ret == UA_STATUSCODE_GOOD)
                        dataSetReader->state = UA_PUBSUBSTATE_DISABLED;
                    break;
                case UA_PUBSUBSTATE_ERROR:
                    break;
                default:
                    UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                   "Received unknown PubSub state!");
            }
            break;
        case UA_PUBSUBSTATE_PAUSED:
            UA_LOG_DEBUG(&server->config.logger, UA_LOGCATEGORY_SERVER,
                            "PubSub state paused is unsupported at the moment!");
            switch(dataSetReader->state){
                case UA_PUBSUBSTATE_DISABLED:
                    break;
                case UA_PUBSUBSTATE_PAUSED:
                    return UA_STATUSCODE_GOOD;
                case UA_PUBSUBSTATE_OPERATIONAL:
                    break;
                case UA_PUBSUBSTATE_ERROR:
                    break;
                default:
                    UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                   "Received unknown PubSub state!");
            }
            break;
        case UA_PUBSUBSTATE_OPERATIONAL:
            switch(dataSetReader->state){
                case UA_PUBSUBSTATE_DISABLED:
                case UA_PUBSUBSTATE_PAUSED:
                case UA_PUBSUBSTATE_OPERATIONAL:
                case UA_PUBSUBSTATE_ERROR:  /* intended fall through */
                    dataSetReader->state = UA_PUBSUBSTATE_OPERATIONAL;
                    break;
                default:
                    UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                   "Received unknown PubSub state!");
            }
            break;
        case UA_PUBSUBSTATE_ERROR:
            switch(dataSetReader->state){
                case UA_PUBSUBSTATE_DISABLED:
                case UA_PUBSUBSTATE_PAUSED:
                case UA_PUBSUBSTATE_OPERATIONAL:
                case UA_PUBSUBSTATE_ERROR: /* intended fall through */
                    dataSetReader->state = state;
                    break;
                default:
                    UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                   "Received unknown PubSub state!");
            }
            dataSetReader->state = UA_PUBSUBSTATE_ERROR;
            break;
        default:
            UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                                           "Received unknown PubSub state!");
            ret = UA_STATUSCODE_BADINTERNALERROR;
    }
    return ret;
}

UA_StatusCode
UA_FieldTargetVariable_copy(const UA_FieldTargetVariable *src, UA_FieldTargetVariable *dst) {
    /* Do a simple memcpy */
    memcpy(dst, src, sizeof(UA_FieldTargetVariable));
    return UA_FieldTargetDataType_copy(&src->targetVariable, &dst->targetVariable);
}

UA_StatusCode
UA_TargetVariables_copy(const UA_TargetVariables *src, UA_TargetVariables *dst) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    memcpy(dst, src, sizeof(UA_TargetVariables));
    if(src->targetVariablesSize > 0) {
        dst->targetVariables = (UA_FieldTargetVariable*)
            UA_calloc(src->targetVariablesSize, sizeof(UA_FieldTargetVariable));
        if(!dst->targetVariables)
            return UA_STATUSCODE_BADOUTOFMEMORY;
        for(size_t i = 0; i < src->targetVariablesSize; i++)
            retVal |= UA_FieldTargetVariable_copy(&src->targetVariables[i], &dst->targetVariables[i]);
    }
    return retVal;
}

void
UA_TargetVariables_clear(UA_TargetVariables *subscribedDataSetTarget) {
    for(size_t i = 0; i < subscribedDataSetTarget->targetVariablesSize; i++) {
        UA_FieldTargetDataType_clear(&subscribedDataSetTarget->targetVariables[i].targetVariable);
    }
    if(subscribedDataSetTarget->targetVariablesSize > 0)
        UA_free(subscribedDataSetTarget->targetVariables);
    memset(subscribedDataSetTarget, 0, sizeof(UA_TargetVariables));
}

/* This Method is used to initially set the SubscribedDataSet to
 * TargetVariablesType and to create the list of target Variables of a
 * SubscribedDataSetType. */
UA_StatusCode
UA_Server_DataSetReader_createTargetVariables(UA_Server *server,
                                              UA_NodeId dataSetReaderIdentifier,
                                              size_t targetVariablesSize,
                                              const UA_FieldTargetVariable *targetVariables) {
    UA_DataSetReader *dataSetReader = UA_ReaderGroup_findDSRbyId(server, dataSetReaderIdentifier);
    if(!dataSetReader)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    if(dataSetReader->configurationFrozen) {
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Create Target Variables failed. Subscriber configuration is frozen.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

    if(dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariablesSize > 0)
        UA_TargetVariables_clear(&dataSetReader->config.subscribedDataSet.subscribedDataSetTarget);

    /* Set subscribed dataset to TargetVariableType */
    dataSetReader->config.subscribedDataSetType = UA_PUBSUB_SDS_TARGET;
    UA_TargetVariables tmp;
    tmp.targetVariablesSize = targetVariablesSize;
    tmp.targetVariables = (UA_FieldTargetVariable*)(uintptr_t)targetVariables;
    return UA_TargetVariables_copy(&tmp, &dataSetReader->config.subscribedDataSet.subscribedDataSetTarget);
}

/* This functionality of this API will be used in future to create mirror Variables - TODO */
/* UA_StatusCode
UA_Server_DataSetReader_createDataSetMirror(UA_Server *server, UA_String *parentObjectNodeName,
                                            UA_NodeId dataSetReaderIdentifier) {
    if((server == NULL) || (parentNode == NULL)) {
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    }

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_DataSetReader* pDataSetReader = UA_ReaderGroup_findDSRbyId(server, dataSetReaderIdentifier);
    if(pDataSetReader == NULL) {
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    }

    if(pDataSetReader->configurationFrozen) {
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Add Target Variables failed. Subscriber configuration is frozen.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    } // TODO: Frozen configuration variable in TargetVariable structure

    UA_TargetVariables targetVars;
    targetVars.targetVariablesSize = pDataSetReader->config.dataSetMetaData.fieldsSize;
    targetVars.targetVariables = (UA_FieldTargetVariable *)
        UA_calloc(targetVars.targetVariablesSize, sizeof(UA_FieldTargetVariable));

    for(size_t i = 0; i < pDataSetReader->config.dataSetMetaData.fieldsSize; i++) {
        UA_VariableAttributes vAttr = UA_VariableAttributes_default;
        vAttr.valueRank = pDataSetReader->config.dataSetMetaData.fields[i].valueRank;
        if(pDataSetReader->config.dataSetMetaData.fields[i].arrayDimensionsSize > 0) {
            retval = UA_Array_copy(pDataSetReader->config.dataSetMetaData.fields[i].arrayDimensions,
                                   pDataSetReader->config.dataSetMetaData.fields[i].arrayDimensionsSize,
                                   (void**)&vAttr.arrayDimensions, &UA_TYPES[UA_TYPES_UINT32]);
            if(retval == UA_STATUSCODE_GOOD) {
                vAttr.arrayDimensionsSize =
                    pDataSetReader->config.dataSetMetaData.fields[i].arrayDimensionsSize;
            }

        }

        vAttr.dataType = pDataSetReader->config.dataSetMetaData.fields[i].dataType;

        vAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
        UA_LocalizedText_copy(&pDataSetReader->config.dataSetMetaData.fields[i].description,
                              &vAttr.description);
        UA_QualifiedName qn;
        UA_QualifiedName_init(&qn);
        char szTmpName[UA_MAX_SIZENAME];
        if(pDataSetReader->config.dataSetMetaData.fields[i].name.length > 0) {
            UA_UInt16 slen = UA_MAX_SIZENAME -1;
            vAttr.displayName.locale = UA_STRING("en-US");
            vAttr.displayName.text = pDataSetReader->config.dataSetMetaData.fields[i].name;
            if(pDataSetReader->config.dataSetMetaData.fields[i].name.length < slen) {
                slen = (UA_UInt16)pDataSetReader->config.dataSetMetaData.fields[i].name.length;
                UA_snprintf(szTmpName, sizeof(szTmpName), "%.*s", (int)slen,
                            (const char*)pDataSetReader->config.dataSetMetaData.fields[i].name.data);
            }

            szTmpName[slen] = '\0';
            qn = UA_QUALIFIEDNAME(1, szTmpName);
        }
        else {
            strcpy(szTmpName, "SubscribedVariable");
            vAttr.displayName = UA_LOCALIZEDTEXT("en-US", szTmpName);
            qn = UA_QUALIFIEDNAME(1, "SubscribedVariable");
        }

        // Add variable to the given parent node
        UA_NodeId newNode;
        retval = UA_Server_addVariableNode(server, UA_NODEID_NULL, *parentNode,
                                           UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), qn,
                                           UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                           vAttr, NULL, &newNode);
        if(retval == UA_STATUSCODE_GOOD) {
            UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_USERLAND,
                        "addVariableNode %s succeeded", szTmpName);
        }
        else {
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_USERLAND,
                         "addVariableNode: error 0x%" PRIx32, retval);
        }

        targetVars.targetVariables[i].targetVariable.attributeId = UA_ATTRIBUTEID_VALUE;
        UA_NodeId_copy(&newNode, &targetVars.targetVariables[i].targetVariable.targetNodeId);
        UA_NodeId_clear(&newNode);
        if(vAttr.arrayDimensionsSize > 0) {
            UA_Array_delete(vAttr.arrayDimensions, vAttr.arrayDimensionsSize,
                            &UA_TYPES[UA_TYPES_UINT32]);
        }
    }

    UA_TargetVariables_clear(&targetVars);
    return retval;
}*/

void
UA_DataSetReader_process(UA_Server *server,
                         UA_ReaderGroup *readerGroup,
                         UA_DataSetReader *dataSetReader,
                         UA_DataSetMessage* dataSetMsg) {
    if(!dataSetReader || !readerGroup || !dataSetMsg || !server)
        return;

    if(!dataSetMsg->header.dataSetMessageValid) {
        UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER,
                    "DataSetMessage is discarded: message is not valid");
         /* To Do check ConfigurationVersion*/
         /*if(dataSetMsg->header.configVersionMajorVersionEnabled)
         * {
         * if(dataSetMsg->header.configVersionMajorVersion != dataSetReader->config.dataSetMetaData.configurationVersion.majorVersion)
         * {
         * UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_SERVER, "DataSetMessage is discarded: ConfigurationVersion MajorVersion does not match");
         * return;
         * }
         } */
        return;
    }

    if(dataSetMsg->header.dataSetMessageType == UA_DATASETMESSAGE_DATAKEYFRAME) {
        //Prepare the raw income
        if(dataSetMsg->header.fieldEncoding == UA_FIELDENCODING_RAWDATA) {
            UA_LOG_TRACE(&server->config.logger, UA_LOGCATEGORY_SERVER, "Received RAW Frame!");
            dataSetMsg->data.keyFrameData.fieldCount =
                (UA_UInt16) dataSetReader->config.dataSetMetaData.fieldsSize;

            size_t offset = 0;
            for(size_t i = 0; i < dataSetReader->config.dataSetMetaData.fieldsSize; i++){
                //TODO The datatype reference should be part of the internal pubsub configuration
                //to avoid the time-expensive lookup
                const UA_DataType *currentType =
                    UA_findDataTypeWithCustom(&dataSetReader->config.dataSetMetaData.fields[i].dataType,
                                              server->config.customDataTypes);
                dataSetMsg->data.keyFrameData.rawFields.length += currentType->memSize;
                UA_STACKARRAY(UA_Byte, decodedType, currentType->memSize);
                UA_StatusCode retVal;
                retVal = UA_decodeBinary(&dataSetMsg->data.keyFrameData.rawFields,
                                &offset, decodedType,
                                currentType, NULL);
                if(retVal != UA_STATUSCODE_GOOD) {
                    UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER, "Error during RAW-decode");
                }
                UA_Variant value;
                UA_Variant_setScalar(&value, decodedType, currentType);
                retVal = UA_Server_writeValue(server,
                                              dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariable.targetNodeId,
                                              value);
                if(retVal != UA_STATUSCODE_GOOD) {
                    UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER, "Error Write Value KF %s", UA_StatusCode_name(retVal));
                }
            }
        }

        if(dataSetMsg->header.fieldEncoding != UA_FIELDENCODING_RAWDATA) {
            size_t anzFields = dataSetMsg->data.keyFrameData.fieldCount;
            if(dataSetReader->config.dataSetMetaData.fieldsSize < anzFields) {
                anzFields = dataSetReader->config.dataSetMetaData.fieldsSize;
            }

            if(dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariablesSize < anzFields) {
                anzFields = dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariablesSize;
            }

            UA_StatusCode retVal = UA_STATUSCODE_GOOD;
            if(readerGroup->config.rtLevel == UA_PUBSUB_RT_FIXED_SIZE) {
                for(UA_UInt16 i = 0; i < anzFields; i++) {
                    if(dataSetMsg->data.keyFrameData.dataSetFields[i].hasValue) {
                        if(dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariable.attributeId == UA_ATTRIBUTEID_VALUE) {
                            memcpy((**(dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].externalDataValue)).value.data,
                                   dataSetMsg->data.keyFrameData.dataSetFields[i].value.data,
                                   dataSetMsg->data.keyFrameData.dataSetFields[i].value.type->memSize);
                            if(dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariableContext)
                                memcpy(dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariableContext,
                                       dataSetMsg->data.keyFrameData.dataSetFields[i].value.data,
                                       dataSetMsg->data.keyFrameData.dataSetFields[i].value.type->memSize);

                            if(dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].afterWrite)
                                dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].afterWrite(server,
                                                                                                                       &dataSetReader->identifier,
                                                                                                                       &dataSetReader->linkedReaderGroup,
                                                                                                                       &dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariable.targetNodeId,
                                                                                                                       dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariableContext,
                                                                                                                       dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].externalDataValue);

                        }
                    }
                }

                return;
            }

            for(UA_UInt16 i = 0; i < anzFields; i++) {
                if(dataSetMsg->data.keyFrameData.dataSetFields[i].hasValue) {
                    if(dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariable.attributeId == UA_ATTRIBUTEID_VALUE) {
                        retVal = UA_Server_writeValue(server,
                                                      dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariable.targetNodeId,
                                                      dataSetMsg->data.keyFrameData.dataSetFields[i].value);
                        if(retVal != UA_STATUSCODE_GOOD)
                            UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER, "Error Write Value KF %" PRIu16 ": 0x%"PRIx32, i, retVal);
                    }
                    else {
                        UA_WriteValue writeVal;
                        UA_WriteValue_init(&writeVal);
                        writeVal.attributeId = dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariable.attributeId;
                        writeVal.indexRange = dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariable.receiverIndexRange;
                        writeVal.nodeId = dataSetReader->config.subscribedDataSet.subscribedDataSetTarget.targetVariables[i].targetVariable.targetNodeId;
                        UA_DataValue_copy(&dataSetMsg->data.keyFrameData.dataSetFields[i], &writeVal.value);
                        retVal = UA_Server_write(server, &writeVal);
                        if(retVal != UA_STATUSCODE_GOOD)
                            UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER, "Error Write KF %" PRIu16 ": 0x%" PRIx32, i, retVal);
                    }
                }
            }

        }
    }

    UA_LOG_DEBUG(&server->config.logger, UA_LOGCATEGORY_SERVER, "DataSetReader '%.*s': received a network message",
        (int) dataSetReader->config.name.length, dataSetReader->config.name.data);
#ifdef UA_ENABLE_PUBSUB_MONITORING
    /* if previous reader state was error (because we haven't received messages and ran into timeout) we should set the state back to operational */
    if(dataSetReader->state == UA_PUBSUBSTATE_ERROR) {
        UA_DataSetReader_setPubSubState(server, UA_PUBSUBSTATE_OPERATIONAL, dataSetReader);
        if(server->config.pubSubConfig.stateChangeCallback != 0) {
            server->config.pubSubConfig.stateChangeCallback(&dataSetReader->identifier,
                                                                   UA_PUBSUBSTATE_OPERATIONAL,
                                                                   UA_STATUSCODE_GOOD);
        }
    }
    if(dataSetReader->msgRcvTimeoutTimerRunning == UA_TRUE) {
        /* stop message receive timeout timer */
        if(server->config.pubSubConfig.monitoringInterface.stopMonitoring(server, dataSetReader->identifier,
            UA_PUBSUB_COMPONENT_DATASETREADER, UA_PUBSUB_MONITORING_MESSAGE_RECEIVE_TIMEOUT, dataSetReader) == UA_STATUSCODE_GOOD) {
            dataSetReader->msgRcvTimeoutTimerRunning = UA_FALSE;
        } else {
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER, "DataSetReader '%.*s': stop receive timeout timer failed",
                (int) dataSetReader->config.name.length, dataSetReader->config.name.data);
            UA_DataSetReader_setPubSubState(server, UA_PUBSUBSTATE_ERROR, dataSetReader);
        }
    }
    /* start message receive timeout timer */
    if(server->config.pubSubConfig.monitoringInterface.startMonitoring(server, dataSetReader->identifier,
            UA_PUBSUB_COMPONENT_DATASETREADER, UA_PUBSUB_MONITORING_MESSAGE_RECEIVE_TIMEOUT, dataSetReader) == UA_STATUSCODE_GOOD) {
        UA_LOG_DEBUG(&server->config.logger, UA_LOGCATEGORY_SERVER, "Info: DataSetReader '%.*s': start receive timeout timer",
            (int) dataSetReader->config.name.length, dataSetReader->config.name.data);
        dataSetReader->msgRcvTimeoutTimerRunning = UA_TRUE;
    } else {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                        "Starting Message Receive Timeout timer failed.");
        UA_DataSetReader_setPubSubState(server, UA_PUBSUBSTATE_ERROR, dataSetReader);
    }
#endif /* UA_ENABLE_PUBSUB_MONITORING */
}

#ifdef UA_ENABLE_PUBSUB_MONITORING
/* Timeout callback for DataSetReader MessageReceiveTimeout handling */
void
UA_DataSetReader_handleMessageReceiveTimeout(UA_Server *server, void *dataSetReader) {
    if(!server || !dataSetReader) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
            "UA_DataSetReader_handleMessageReceiveTimeout(): null pointer param");
        return;
    }
    UA_DataSetReader *dsReader = (UA_DataSetReader*) dataSetReader;
    if(dsReader->componentType != UA_PUBSUB_COMPONENT_DATASETREADER) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
            "UA_DataSetReader_handleMessageReceiveTimeout(): input param is not of type DataSetReader");
        return;
    }
    UA_LOG_DEBUG(&server->config.logger, UA_LOGCATEGORY_SERVER, "UA_DataSetReader_handleMessageReceiveTimeout(): "
        "MessageReceiveTimeout occurred at DataSetReader '%.*s': MessageReceiveTimeout = %f Timer Id = %u ",
            (int) dsReader->config.name.length, dsReader->config.name.data, dsReader->config.messageReceiveTimeout, (UA_UInt32) dsReader->msgRcvTimeoutTimerId);

    UA_ServerConfig *pConfig = UA_Server_getConfig(server);
    if(pConfig->pubSubConfig.stateChangeCallback != 0) {
        pConfig->pubSubConfig.stateChangeCallback(&dsReader->identifier,
                                                  UA_PUBSUBSTATE_ERROR,
                                                  UA_STATUSCODE_BADTIMEOUT);
    }

    if(UA_DataSetReader_setPubSubState(server, UA_PUBSUBSTATE_ERROR, dsReader) != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
            "UA_DataSetReader_handleMessageReceiveTimeout(): setting pubsub state failed");
    }
}
#endif /* UA_ENABLE_PUBSUB_MONITORING */

static void
UA_DataSetReader_clear(UA_Server *server, UA_DataSetReader *dataSetReader) {

    /* Delete DataSetReader config */
    UA_DataSetReaderConfig_clear(&dataSetReader->config);

    /* Delete DataSetReader */
    UA_ReaderGroup* pGroup = UA_ReaderGroup_findRGbyId(server, dataSetReader->linkedReaderGroup);
    if(pGroup != NULL) {
        pGroup->readersCount--;
    }

    UA_NodeId_clear(&dataSetReader->identifier);
    UA_NodeId_clear(&dataSetReader->linkedReaderGroup);
    if(dataSetReader->config.subscribedDataSetType == UA_PUBSUB_SDS_TARGET) {
        UA_TargetVariables_clear(&dataSetReader->config.subscribedDataSet.subscribedDataSetTarget);
    } else {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
            "UA_DataSetReader_clear(): unsupported subscribed dataset enum type");
    }

    /* Remove DataSetReader from group */
    LIST_REMOVE(dataSetReader, listEntry);
    /* Free memory allocated for DataSetReader */
    UA_free(dataSetReader);
}

static void
processMessageWithReader(UA_Server *server, UA_ReaderGroup *readerGroup,
                         UA_DataSetReader *reader, UA_NetworkMessage *msg) {
    UA_Byte totalDataSets = 1;
    if(msg->payloadHeaderEnabled)
        totalDataSets = msg->payloadHeader.dataSetPayloadHeader.count;
    for(UA_Byte i = 0; i < totalDataSets; i++) {
        UA_LOG_DEBUG(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "Process Msg with DataSetReader!");
        UA_DataSetReader_process(server, readerGroup, reader,
                                 &msg->payload.dataSetPayload.dataSetMessages[i]);
    }
}

UA_StatusCode
UA_ReaderGroup_processNetworkMessage(UA_Server *server, UA_ReaderGroup *readerGroup,
                                     UA_NetworkMessage* msg) {
    if(!msg || !readerGroup)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    /* To Do The condition pMsg->dataSetClassIdEnabled
     * Here some filtering is possible */

    if(!msg->publisherIdEnabled) {
        UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER,
                    "Cannot process DataSetReader without PublisherId");
        return UA_STATUSCODE_BADNOTIMPLEMENTED; /* TODO: Handle DSR without PublisherId */
    }

    UA_Boolean processed = false;
    UA_DataSetReader *reader;
    /* There can be several readers listening for the same network message */
    LIST_FOREACH(reader, &readerGroup->readers, listEntry) {
        UA_StatusCode retval = checkReaderIdentifier(server, msg, reader);
        if(retval == UA_STATUSCODE_GOOD) {
            processed = true;
            processMessageWithReader(server, readerGroup, reader, msg);
        }
    }

    if(!processed) {
        UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER,
                    "Dataset reader not found. Check PublisherID, WriterGroupID "
                    "and DatasetWriterID");
    }

    return UA_STATUSCODE_GOOD;
}

#ifdef UA_ENABLE_PUBSUB_ENCRYPTION
UA_StatusCode
UA_Server_setReaderGroupEncryptionKeys(UA_Server *server, const UA_NodeId readerGroup,
                                       UA_UInt32 securityTokenId,
                                       const UA_ByteString signingKey,
                                       const UA_ByteString encryptingKey,
                                       const UA_ByteString keyNonce) {
    UA_ReaderGroup *rg = UA_ReaderGroup_findRGbyId(server, readerGroup);
    UA_CHECK_MEM(rg, return UA_STATUSCODE_BADNOTFOUND);

    UA_CHECK_MEM_WARN(rg->config.securityPolicy, return UA_STATUSCODE_BADINTERNALERROR,
                      &server->config.logger, UA_LOGCATEGORY_SERVER,
                      "No SecurityPolicy configured for the ReaderGroup");

    if(securityTokenId != rg->securityTokenId) {
        rg->securityTokenId = securityTokenId;
        rg->nonceSequenceNumber = 1;
    }

    /* Create a new context */
    if(!rg->securityPolicyContext) {
        return rg->config.securityPolicy->
            newContext(rg->config.securityPolicy->policyContext,
                       &signingKey, &encryptingKey, &keyNonce,
                       &rg->securityPolicyContext);
    }

    /* Update the context */
    return rg->config.securityPolicy->
        setSecurityKeys(rg->securityPolicyContext, &signingKey, &encryptingKey, &keyNonce);
}
#endif

/********************************************************************************
 * Functionality related to decoding, decrypting and processing network messages
 * as a subscriber
 ********************************************************************************/

#define MIN_PAYLOAD_SIZE_ETHERNET 46

#define RECEIVE_MSG_BUFFER_SIZE   4096
static UA_THREAD_LOCAL UA_Byte ReceiveMsgBuffer[RECEIVE_MSG_BUFFER_SIZE];

/* Delete the payload value of every decoded DataSet field */
static void UA_DataSetMessage_freeDecodedPayload(UA_DataSetMessage *dsm) {
    if(dsm->header.fieldEncoding == UA_FIELDENCODING_VARIANT) {
        for(UA_UInt16 i = 0; i < dsm->data.keyFrameData.fieldCount; i++) {
#ifdef UA_ENABLE_PUBSUB_BUFMALLOC
            UA_Variant_init(&dsm->data.keyFrameData.dataSetFields[i].value);
#else
            UA_Variant_clear(&dsm->data.keyFrameData.dataSetFields[i].value);
#endif
        }
    }
    else if(dsm->header.fieldEncoding == UA_FIELDENCODING_DATAVALUE) {
        for(UA_UInt16 i = 0; i < dsm->data.keyFrameData.fieldCount; i++) {
#ifdef UA_ENABLE_PUBSUB_BUFMALLOC
            UA_DataValue_init(&dsm->data.keyFrameData.dataSetFields[i]);
#else
            UA_DataValue_clear(&dsm->data.keyFrameData.dataSetFields[i]);
#endif
        }
    }
}

UA_StatusCode
decodeNetworkMessage(const UA_Logger *logger,
                     UA_ByteString *buffer, size_t *currentPosition,
                     UA_NetworkMessage *currentNetworkMessage,
                     UA_ReaderGroup *readerGroup) {

#ifdef UA_DEBUG_DUMP_PKGS
    UA_dump_hex_pkg(buffer->data, buffer->length);
#endif

    UA_StatusCode rv = UA_NetworkMessage_decodeHeaders(
        buffer, currentPosition, currentNetworkMessage);
    UA_CHECK_STATUS_ERROR(rv, return rv, logger, UA_LOGCATEGORY_SERVER,
                          "PubSub receive. decoding headers failed");

#ifdef UA_ENABLE_PUBSUB_ENCRYPTION
    rv = verifyAndDecryptNetworkMessage(logger,
                                        buffer,
                                        currentPosition,
                                        currentNetworkMessage,
                                        readerGroup);
    UA_CHECK_STATUS_WARN(rv, return rv, logger, UA_LOGCATEGORY_SERVER,
                         "Subscribe failed. verify and decrypt network message failed.");

    #ifdef UA_DEBUG_DUMP_PKGS
    UA_dump_hex_pkg(buffer->data, buffer->length);
    #endif

#endif

    rv = UA_NetworkMessage_decodePayload(buffer, currentPosition, currentNetworkMessage);
    UA_CHECK_STATUS(rv, return rv);

    rv = UA_NetworkMessage_decodeFooters(buffer, currentPosition, currentNetworkMessage);
    UA_CHECK_STATUS(rv, return rv);

    return UA_STATUSCODE_GOOD;
}

static
UA_StatusCode
decodeAndProcessNetworkMessage(UA_Server *server, UA_ReaderGroup *readerGroup,
                               UA_PubSubConnection *connection, size_t previousPosition,
                               UA_ByteString *buffer, size_t *currentPosition) {
    size_t paddingBytes = 0;
    UA_NetworkMessage currentNetworkMessage;
    memset(&currentNetworkMessage, 0, sizeof(UA_NetworkMessage));

    UA_StatusCode rv = UA_STATUSCODE_GOOD;
    rv = decodeNetworkMessage(&server->config.logger, buffer, currentPosition,
                              &currentNetworkMessage, readerGroup);
    UA_CHECK_STATUS_WARN(rv, goto cleanup, &server->config.logger, UA_LOGCATEGORY_SERVER,
                         "Subscribe failed. verify, decrypt and decode network message failed.");

    rv = UA_ReaderGroup_processNetworkMessage(server, readerGroup, &currentNetworkMessage);
    // TODO: check what action to perform on error (nothing?)
    UA_CHECK_STATUS_WARN(rv, (void)0, &server->config.logger, UA_LOGCATEGORY_SERVER,
                         "Subscribe failed. process network message failed.");

    /* Minimum ethernet packet size is 64 bytes where the header size is 14
     * bytes and FCS size is 4 bytes so remaining minimum payload size of
     * ethernet packet is 46 bytes */
    /* TODO: Need to handle padding bytes for UDP */
    if((((*currentPosition) - previousPosition) < MIN_PAYLOAD_SIZE_ETHERNET) &&
        (strncmp((const char *)connection->config->transportProfileUri.data,
                 "http://opcfoundation.org/UA-Profile/Transport/pubsub-eth-uadp",
                 (size_t)(connection->config->transportProfileUri.length)) == 0)) {
        paddingBytes = (MIN_PAYLOAD_SIZE_ETHERNET - ((*currentPosition) - previousPosition));
        (*currentPosition) += paddingBytes; /* During multiple receive, move the position to
                             handle padding bytes */
    }

cleanup:
    UA_NetworkMessage_clear(&currentNetworkMessage);
    return rv;
}

static
UA_StatusCode
decodeAndProcessNetworkMessageRT(UA_Server *server, UA_ReaderGroup *readerGroup,
                                 UA_PubSubConnection *connection, size_t previousPosition,
                                 UA_ByteString *buffer, size_t *currentPosition) {
#ifdef UA_ENABLE_PUBSUB_BUFMALLOC
    useMembufAlloc();
#endif /* UA_ENABLE_PUBSUB_BUFMALLOC */

    /* Considering max DSM as 1
* TODO:
* Process with the static value source
*/
    size_t paddingBytes = 0;
    UA_DataSetReader *dataSetReader = LIST_FIRST(&readerGroup->readers);
    /* Decode only the necessary offset and update the networkMessage */
    if(UA_NetworkMessage_updateBufferedNwMessage(&dataSetReader->bufferedMessage, buffer,
                                                 currentPosition) != UA_STATUSCODE_GOOD) {
        UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER,
                    "PubSub receive. Unknown field type.");
        UA_DataSetMessage_freeDecodedPayload(
            dataSetReader->bufferedMessage.nm->payload.dataSetPayload.dataSetMessages);
#ifdef UA_ENABLE_PUBSUB_BUFMALLOC
        useNormalAlloc();
#endif /* UA_ENABLE_PUBSUB_BUFMALLOC */
        return UA_STATUSCODE_UNCERTAIN;
    }

    /* Check the decoded message is the expected one
     * TODO: PublisherID check after modification in NM to support all datatypes */
    if((dataSetReader->bufferedMessage.nm->groupHeader.writerGroupId !=
        dataSetReader->config.writerGroupId) ||
       (*dataSetReader->bufferedMessage.nm->payloadHeader.dataSetPayloadHeader
           .dataSetWriterIds != dataSetReader->config.dataSetWriterId)) {
        UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER,
                    "PubSub receive. Unknown message received. Will not be processed.");
        UA_DataSetMessage_freeDecodedPayload(
            dataSetReader->bufferedMessage.nm->payload.dataSetPayload.dataSetMessages);
#ifdef UA_ENABLE_PUBSUB_BUFMALLOC
        useNormalAlloc();
#endif /* UA_ENABLE_PUBSUB_BUFMALLOC */
        return UA_STATUSCODE_UNCERTAIN;
    }

    UA_DataSetReader_process(server, readerGroup, dataSetReader,
                             dataSetReader->bufferedMessage.nm->payload.dataSetPayload.dataSetMessages);

    UA_DataSetMessage_freeDecodedPayload(dataSetReader->bufferedMessage.
                                         nm->payload.dataSetPayload.dataSetMessages);

#ifdef UA_ENABLE_PUBSUB_BUFMALLOC
    useNormalAlloc();
#endif /* UA_ENABLE_PUBSUB_BUFMALLOC */

    /* Minimum ethernet packet size is 64 bytes where the header size is 14
     * bytes and FCS size is 4 bytes so remaining minimum payload size of
     * ethernet packet is 46 bytes */
    /* TODO: Need to handle padding bytes for UDP */
    if(((*currentPosition - previousPosition) < MIN_PAYLOAD_SIZE_ETHERNET) &&
       (strncmp((const char *)connection->config->transportProfileUri.data,
                "http://opcfoundation.org/UA-Profile/Transport/pubsub-eth-uadp",
                (size_t)(connection->config->transportProfileUri.length)) == 0)) {
        paddingBytes =
            (MIN_PAYLOAD_SIZE_ETHERNET - ((*currentPosition) - previousPosition));
        (*currentPosition) += paddingBytes; /* During multiple receive, move the position
                                               to handle padding bytes */
    }
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
receiveBufferedNetworkMessage(UA_Server *server, UA_ReaderGroup *readerGroup,
                              UA_PubSubConnection *connection) {
    UA_ByteString buffer;
    buffer.length = RECEIVE_MSG_BUFFER_SIZE;
    buffer.data = ReceiveMsgBuffer;

    UA_StatusCode rv =
        connection->channel->receive(connection->channel, &buffer, NULL,
                                     readerGroup->config.timeout);

    // TODO attention: here rv is ok if UA_STATUSCODE_GOOD != rv
    UA_CHECK_WARN(!UA_StatusCode_isBad(rv), return rv,
                  &server->config.logger, UA_LOGCATEGORY_SERVER,
                  "SubscribeCallback(): Connection receive failed!");

    UA_StatusCode (*decodeAndProcessNetworkMessageFun)(UA_Server *server,
                                                       UA_ReaderGroup *readerGroup,
                                                       UA_PubSubConnection *connection,
                                                       size_t previousPosition,
                                                       UA_ByteString *buffer,
                                                       size_t *currentPosition) = NULL;

    if(readerGroup->config.rtLevel == UA_PUBSUB_RT_FIXED_SIZE) {
        decodeAndProcessNetworkMessageFun = decodeAndProcessNetworkMessageRT;
    } else {
        decodeAndProcessNetworkMessageFun = decodeAndProcessNetworkMessage;
    }

    size_t currentPosition = 0;
    size_t previousPosition = 0;
    while(buffer.length > currentPosition) {
        rv = decodeAndProcessNetworkMessageFun(server, readerGroup, connection,
                                               previousPosition, &buffer, &currentPosition);
        UA_CHECK_STATUS_WARN(rv, return rv, &server->config.logger, UA_LOGCATEGORY_SERVER,
                             "SubscribeCallback(): receive message failed");
        previousPosition = currentPosition;
    }
    return UA_STATUSCODE_GOOD;
}

#endif /* UA_ENABLE_PUBSUB */
