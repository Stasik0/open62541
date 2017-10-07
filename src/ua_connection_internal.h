/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef UA_CONNECTION_INTERNAL_H_
#define UA_CONNECTION_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_plugin_network.h"

/* The application can be the client or the server */
typedef void (*UA_Connection_processChunk)(void *application,
                                           UA_Connection *connection,
                                           const UA_ByteString *chunk);

/* The network layer may receive chopped up messages since TCP is a streaming
 * protocol. This method calls the processChunk callback on all full chunks that
 * were received. Dangling half-complete chunks are buffered in the connection
 * and considered for the next received packet.
 *
 * If an entire chunk is received, it is forwarded directly. But the memory
 * needs to be freed with the networklayer-specific mechanism. If a half message
 * is received, we copy it into a local buffer. Then, the stack-specific free
 * needs to be used.
 *
 * @param connection The connection
 * @param application The client or server application
 * @param processCallback The function pointer for processing each chunk
 * @param packet The received packet.
 * @return Returns UA_STATUSCODE_GOOD or an error code. When an error occurs,
 *         the ingoing message and the current buffer in the connection are
 *         freed. */
UA_StatusCode
UA_Connection_processChunks(UA_Connection *connection, void *application,
                            UA_Connection_processChunk processCallback,
                            const UA_ByteString *packet);

/* Try to receive at least one complete chunk on the connection. This blocks the
 * current thread up to the given timeout.
 *
 * @param connection The connection
 * @param application The client or server application
 * @param processCallback The function pointer for processing each chunk
 * @param timeout The timeout (in milliseconds) the method will block at most.
 * @return Returns UA_STATUSCODE_GOOD or an error code. When an timeout occurs,
 *         UA_STATUSCODE_GOODNONCRITICALTIMEOUT is returned. */
UA_StatusCode
UA_Connection_receiveChunksBlocking(UA_Connection *connection, void *application,
                                    UA_Connection_processChunk processCallback,
                                    UA_UInt32 timeout);

void UA_Connection_detachSecureChannel(UA_Connection *connection);
void UA_Connection_attachSecureChannel(UA_Connection *connection,
                                       UA_SecureChannel *channel);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* UA_CONNECTION_INTERNAL_H_ */
