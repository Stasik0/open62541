/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef UA_SECURECHANNEL_H_
#define UA_SECURECHANNEL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "queue.h"
#include "ua_types.h"
#include "ua_transport_generated.h"
#include "ua_connection_internal.h"
#include "ua_plugin_securitypolicy.h"
#include "ua_util.h"

struct UA_Session;
typedef struct UA_Session UA_Session;

/* Definitions for fixed lengths taken from the standard */
#define UA_SECURE_MESSAGE_HEADER_LENGTH 24
#define UA_ASYMMETRIC_ALG_SECURITY_HEADER_FIXED_LENGTH 12
#define UA_SYMMETRIC_ALG_SECURITY_HEADER_LENGTH 4
#define UA_MESSAGE_HEADER_LENGTH 8
#define UA_SEQUENCE_HEADER_LENGTH 8
#define UA_SECURE_CONVERSATION_MESSAGE_HEADER_LENGTH 12
#define UA_SECUREMH_AND_SYMALGH_LENGTH \
    (UA_SECURE_CONVERSATION_MESSAGE_HEADER_LENGTH + \
    UA_SYMMETRIC_ALG_SECURITY_HEADER_LENGTH)

struct SessionEntry {
    LIST_ENTRY(SessionEntry) pointers;
    UA_Session *session; // Just a pointer. The session is held in the session manager or the client
};

/* For chunked requests */
struct ChunkEntry {
    LIST_ENTRY(ChunkEntry) pointers;
    UA_UInt32 requestId;
    UA_ByteString bytes;
};

struct UA_SecureChannel {
    UA_Boolean temporary; // this flag is set to false if the channel is fully opened.
    UA_MessageSecurityMode  securityMode;
    UA_ChannelSecurityToken securityToken; // the channelId is contained in the securityToken
    UA_ChannelSecurityToken nextSecurityToken; // the channelId is contained in the securityToken
    UA_AsymmetricAlgorithmSecurityHeader remoteAsymAlgSettings;
    UA_AsymmetricAlgorithmSecurityHeader localAsymAlgSettings;

    /* The endpoint and context of the channel */
    UA_Endpoint *endpoint;
    void *securityContext;

    /* The available endpoints */
    UA_Endpoints *endpoints;

    UA_ByteString  clientNonce;
    UA_ByteString  serverNonce;
    UA_UInt32      receiveSequenceNumber;
    UA_UInt32      sendSequenceNumber;
    UA_Connection *connection;

    UA_Logger logger;

    LIST_HEAD(session_pointerlist, SessionEntry) sessions;
    LIST_HEAD(chunk_pointerlist, ChunkEntry) chunks;
};

/**
 * \brief Initializes the secure channel.
 *
 * \param channel the channel to initialize.
 * \param endpoints the endpoints struct that contains all available endpoints
 *                         the channel will try to match when a channel is being established.
 * \param logger the logger the securechannel may use to log messages.
 */
void UA_SecureChannel_init(UA_SecureChannel *channel,
                           UA_Endpoints *endpoints,
                           UA_Logger logger);
void UA_SecureChannel_deleteMembersCleanup(UA_SecureChannel *channel);

/**
 * \brief Generates a nonce.
 *
 * Uses the random generator of the channels security policy
 *
 * \param nonce will contain the nonce after being successfully called.
 * \param securityPolicy the SecurityPolicy to use.
 */
UA_StatusCode UA_SecureChannel_generateNonce(const UA_SecureChannel *const channel,
                                             const size_t nonceLength,
                                             UA_ByteString *const nonce);

/**
 * Generates new keys and sets them in the channel context
 *
 * \param channel the channel to generate new keys for
 */
UA_StatusCode UA_SecureChannel_generateNewKeys(UA_SecureChannel* const channel);

void UA_SecureChannel_attachSession(UA_SecureChannel *channel, UA_Session *session);
void UA_SecureChannel_detachSession(UA_SecureChannel *channel, UA_Session *session);
UA_Session * UA_SecureChannel_getSession(UA_SecureChannel *channel, UA_NodeId *token);

UA_StatusCode UA_SecureChannel_sendBinaryMessage(UA_SecureChannel *channel, UA_UInt32 requestId,
                                                  const void *content, const UA_DataType *contentType);

void UA_SecureChannel_revolveTokens(UA_SecureChannel *channel);

/**
 * Chunking
 * -------- */
typedef void
(UA_ProcessMessageCallback)(void *application, UA_SecureChannel *channel,
                             UA_MessageType messageType, UA_UInt32 requestId,
                             const UA_ByteString *message);

/* For chunked responses */
typedef struct {
    UA_SecureChannel *channel;
    UA_UInt32 requestId;
    UA_UInt32 messageType;

    UA_UInt16 chunksSoFar;
    size_t messageSizeSoFar;

    UA_ByteString messageBuffer;
    UA_StatusCode errorCode;
    UA_Boolean final;
} UA_ChunkInfo;

/**
 * \brief Processes all chunks in the chunks ByteString.
 *
 * If a final chunk is processed, the callback function is called with the complete message body.
 *
 * \param channel the channel the chunks were recieved on.
 * \param chunks the memory region where the chunks are stored.
 * \param callback the callback function that gets called with the complete message body, once a final chunk is processed.
 * \param application data pointer to application specific data that gets passed on to the callback function.
 */
UA_StatusCode
UA_SecureChannel_processChunks(UA_SecureChannel *channel,
                               const UA_ByteString *chunks,
                               UA_ProcessMessageCallback callback,
                               void *application);

/**
 * Log Helper
 * ----------
 * C99 requires at least one element for the variadic argument. If the log
 * statement has no variable arguments, supply an additional NULL. It will be
 * ignored by printf.
 *
 * We have to jump through some hoops to enable the use of format strings
 * without arguments since (pedantic) C99 does not allow variadic macros with
 * zero arguments. So we add a dummy argument that is not printed (%.0s is
 * string of length zero). */

#define UA_LOG_TRACE_CHANNEL_INTERNAL(LOGGER, CHANNEL, MSG, ...)              \
    UA_LOG_TRACE(LOGGER, UA_LOGCATEGORY_SECURECHANNEL,                        \
                 "Connection %i | SecureChannel %i | " MSG "%.0s",            \
                 ((CHANNEL)->connection ? (CHANNEL)->connection->sockfd : 0), \
                 (CHANNEL)->securityToken.channelId, __VA_ARGS__)

#define UA_LOG_TRACE_CHANNEL(LOGGER, CHANNEL, ...)        \
    UA_MACRO_EXPAND(UA_LOG_TRACE_CHANNEL_INTERNAL(LOGGER, CHANNEL, __VA_ARGS__, ""))

#define UA_LOG_DEBUG_CHANNEL_INTERNAL(LOGGER, CHANNEL, MSG, ...)              \
    UA_LOG_DEBUG(LOGGER, UA_LOGCATEGORY_SECURECHANNEL,                        \
                 "Connection %i | SecureChannel %i | " MSG "%.0s",            \
                 ((CHANNEL)->connection ? (CHANNEL)->connection->sockfd : 0), \
                 (CHANNEL)->securityToken.channelId, __VA_ARGS__)

#define UA_LOG_DEBUG_CHANNEL(LOGGER, CHANNEL, ...)        \
    UA_MACRO_EXPAND(UA_LOG_DEBUG_CHANNEL_INTERNAL(LOGGER, CHANNEL, __VA_ARGS__, ""))

#define UA_LOG_INFO_CHANNEL_INTERNAL(LOGGER, CHANNEL, MSG, ...)               \
    UA_LOG_INFO(LOGGER, UA_LOGCATEGORY_SECURECHANNEL,                         \
                 "Connection %i | SecureChannel %i | " MSG "%.0s",            \
                 ((CHANNEL)->connection ? (CHANNEL)->connection->sockfd : 0), \
                 (CHANNEL)->securityToken.channelId, __VA_ARGS__)

#define UA_LOG_INFO_CHANNEL(LOGGER, CHANNEL, ...)        \
    UA_MACRO_EXPAND(UA_LOG_INFO_CHANNEL_INTERNAL(LOGGER, CHANNEL, __VA_ARGS__, ""))

#define UA_LOG_WARNING_CHANNEL_INTERNAL(LOGGER, CHANNEL, MSG, ...)            \
    UA_LOG_WARNING(LOGGER, UA_LOGCATEGORY_SECURECHANNEL,                      \
                 "Connection %i | SecureChannel %i | " MSG "%.0s",            \
                 ((CHANNEL)->connection ? (CHANNEL)->connection->sockfd : 0), \
                 (CHANNEL)->securityToken.channelId, __VA_ARGS__)

#define UA_LOG_WARNING_CHANNEL(LOGGER, CHANNEL, ...)        \
    UA_MACRO_EXPAND(UA_LOG_WARNING_CHANNEL_INTERNAL(LOGGER, CHANNEL, __VA_ARGS__, ""))

#define UA_LOG_ERROR_CHANNEL_INTERNAL(LOGGER, CHANNEL, MSG, ...)              \
    UA_LOG_ERROR(LOGGER, UA_LOGCATEGORY_SECURECHANNEL,                        \
                 "Connection %i | SecureChannel %i | " MSG "%.0s",            \
                 ((CHANNEL)->connection ? (CHANNEL)->connection->sockfd : 0), \
                 (CHANNEL)->securityToken.channelId, __VA_ARGS__)

#define UA_LOG_ERROR_CHANNEL(LOGGER, CHANNEL, ...)        \
    UA_MACRO_EXPAND(UA_LOG_ERROR_CHANNEL_INTERNAL(LOGGER, CHANNEL, __VA_ARGS__, ""))

#define UA_LOG_FATAL_CHANNEL_INTERNAL(LOGGER, CHANNEL, MSG, ...)              \
    UA_LOG_FATAL(LOGGER, UA_LOGCATEGORY_SECURECHANNEL,                        \
                 "Connection %i | SecureChannel %i | " MSG "%.0s",            \
                 ((CHANNEL)->connection ? (CHANNEL)->connection->sockfd : 0), \
                 (CHANNEL)->securityToken.channelId, __VA_ARGS__)

#define UA_LOG_FATAL_CHANNEL(LOGGER, CHANNEL, ...)        \
    UA_MACRO_EXPAND(UA_LOG_FATAL_CHANNEL_INTERNAL(LOGGER, CHANNEL, __VA_ARGS__, ""))

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* UA_SECURECHANNEL_H_ */
