/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2018 (c) Stefan Profanter, fortiss GmbH
 */

#ifndef UA_CLIENT_CONFIG_H
#define UA_CLIENT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_plugin_network.h"

/**
 * .. _client-config:
 *
 * Client Configuration
 * --------------------
 *
 * The client configuration is used for setting connection parameters and
 * additional settings used by the client.
 * The configuration should not be modified after it is passed to a client.
 * Currently, only one client can use a configuration at a time.
 *
 * Examples for configurations are provided in the ``/plugins`` folder.
 * The usual usage is as follows:
 *
 * 1. Create a client configuration with default settings as a starting point
 * 2. Modifiy the configuration, e.g. modifying the timeout
 * 3. Instantiate a client with it
 * 4. After shutdown of the client, clean up the configuration (free memory)
 *
 * The :ref:`tutorials` provide a good starting point for this. */

typedef enum {
    UA_CLIENTSTATE_DISCONNECTED,        /* The client is disconnected */
    UA_CLIENTSTATE_WAITING_FOR_ACK,     /* The Client has sent HEL and waiting*/
    UA_CLIENTSTATE_CONNECTED,           /* A TCP connection to the server is open */
    UA_CLIENTSTATE_SECURECHANNEL,       /* A SecureChannel to the server is open */
    UA_CLIENTSTATE_SESSION,             /* A session with the server is open */
    UA_CLIENTSTATE_SESSION_DISCONNECTED,  /*disconnected vs renewed?*/
    UA_CLIENTSTATE_SESSION_RENEWED      /* A session with the server is open (renewed) */
} UA_ClientState;


struct UA_Client;
typedef struct UA_Client UA_Client;

typedef void (*UA_ClientAsyncServiceCallback)(UA_Client *client, void *userdata,
        UA_UInt32 requestId, void *response);
/*
 * Repeated Callbacks
 * ------------------ */
typedef UA_StatusCode (*UA_ClientCallback)(UA_Client *client, void *data);

UA_StatusCode
UA_Client_addRepeatedCallback(UA_Client *Client, UA_ClientCallback callback,
        void *data, UA_UInt32 interval, UA_UInt64 *callbackId);

UA_StatusCode
UA_Client_changeRepeatedCallbackInterval(UA_Client *Client,
        UA_UInt64 callbackId, UA_UInt32 interval);

UA_StatusCode UA_Client_removeRepeatedCallback(UA_Client *Client,
        UA_UInt64 callbackId);

/**
 * Client Lifecycle callback
 * ^^^^^^^^^^^^^^^^^^^^^^^^^ */

typedef void (*UA_ClientStateCallback)(UA_Client *client, UA_ClientState clientState);

/**
 * Subscription Inactivity callback
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */

#ifdef UA_ENABLE_SUBSCRIPTIONS
typedef void (*UA_SubscriptionInactivityCallback)(UA_Client *client, UA_UInt32 subscriptionId, void *subContext);
#endif

/**
 * Client Configuration Data
 * ^^^^^^^^^^^^^^^^^^^^^^^^^ */

typedef struct UA_ClientConfig {
    UA_UInt32 timeout;               /* Sync response timeout in ms */
    UA_UInt32 secureChannelLifeTime; /* Lifetime in ms (then the channel needs
                                        to be renewed) */
    UA_Logger logger;
    UA_ConnectionConfig localConnectionConfig;
    UA_ConnectClientConnection connectionFunc;
    UA_ConnectClientConnection initConnectionFunc;
    UA_ClientCallback pollConnectionFunc;

    /* Custom DataTypes */
    size_t customDataTypesSize;
    const UA_DataType *customDataTypes;

    /* Callback function */
    UA_ClientStateCallback stateCallback;
#ifdef UA_ENABLE_SUBSCRIPTIONS
    UA_SubscriptionInactivityCallback subscriptionInactivityCallback;
#endif

    void *clientContext;

    /* number of PublishResponse standing in the sever */
    /* 0 = background task disabled                    */
    UA_UInt16 outStandingPublishRequests;
} UA_ClientConfig;

#ifdef __cplusplus
}
#endif


#endif /* UA_CLIENT_CONFIG_H */
