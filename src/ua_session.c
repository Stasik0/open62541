#include "ua_util.h"
#include "ua_session.h"
#include "ua_statuscodes.h"

UA_Session adminSession = {
    .clientDescription =  {.applicationUri = {0, NULL}, .productUri = {0, NULL},
                           .applicationName = {.locale = {0, NULL}, .text = {0, NULL}},
                           .applicationType = UA_APPLICATIONTYPE_CLIENT,
                           .gatewayServerUri = {0, NULL}, .discoveryProfileUri = {0, NULL},
                           .discoveryUrlsSize = 0, .discoveryUrls = NULL},
    .sessionName = {sizeof("Administrator Session")-1, (UA_Byte*)"Administrator Session"},
    .authenticationToken = {.namespaceIndex = 0, .identifierType = UA_NODEIDTYPE_NUMERIC,
                            .identifier.numeric = 1},
    .sessionId = {.namespaceIndex = 0, .identifierType = UA_NODEIDTYPE_NUMERIC, .identifier.numeric = 1},
    .maxRequestMessageSize = UA_UINT32_MAX, .maxResponseMessageSize = UA_UINT32_MAX,
    .timeout = (UA_Double)UA_INT64_MAX, .validTill = UA_INT64_MAX, .channel = NULL,
    .continuationPoints = {NULL}};

void UA_Session_init(UA_Session *session) {
    UA_ApplicationDescription_init(&session->clientDescription);
    session->activated = UA_FALSE;
    UA_NodeId_init(&session->authenticationToken);
    UA_NodeId_init(&session->sessionId);
    UA_String_init(&session->sessionName);
    session->maxRequestMessageSize  = 0;
    session->maxResponseMessageSize = 0;
    session->timeout = 0;
    UA_DateTime_init(&session->validTill);
    session->channel = NULL;
#ifdef UA_ENABLE_SUBSCRIPTIONS
    SubscriptionManager_init(session);
#endif
    session->availableContinuationPoints = MAXCONTINUATIONPOINTS;
    LIST_INIT(&session->continuationPoints);
}

void UA_Session_deleteMembersCleanup(UA_Session *session, UA_Server* server) {
    UA_ApplicationDescription_deleteMembers(&session->clientDescription);
    UA_NodeId_deleteMembers(&session->authenticationToken);
    UA_NodeId_deleteMembers(&session->sessionId);
    UA_String_deleteMembers(&session->sessionName);
    struct ContinuationPointEntry *cp, *temp;
    LIST_FOREACH_SAFE(cp, &session->continuationPoints, pointers, temp) {
        LIST_REMOVE(cp, pointers);
        UA_ByteString_deleteMembers(&cp->identifier);
        UA_BrowseDescription_deleteMembers(&cp->browseDescription);
        UA_free(cp);
    }
    if(session->channel)
        UA_SecureChannel_detachSession(session->channel, session);
#ifdef UA_ENABLE_SUBSCRIPTIONS
    SubscriptionManager_deleteMembers(session, server);
#endif
}

void UA_Session_updateLifetime(UA_Session *session) {
    session->validTill = UA_DateTime_now() + (UA_DateTime)(session->timeout * UA_MSEC_TO_DATETIME);
}
