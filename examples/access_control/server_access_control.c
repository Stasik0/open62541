/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information.
 *
 *  Copyright 2023 (c) Asish Ganesh, Eclatron Technologies Private Limited
 */

#include <open62541/plugin/accesscontrol_default.h>
#include <open62541/plugin/accesscontrol_custom.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <signal.h>
#include <stdlib.h>

static UA_Boolean
allowAddNode(UA_Server *server, UA_AccessControl *ac,
             const UA_NodeId *sessionId, void *sessionContext,
             const UA_AddNodesItem *item,
             UA_RolePermissionType *userRolePermission,
             size_t userRoleSize) {
    printf("Called allowAddNode\n");
    return UA_TRUE;
}

static UA_Boolean
allowAddReference(UA_Server *server, UA_AccessControl *ac,
                  const UA_NodeId *sessionId, void *sessionContext,
                  const UA_AddReferencesItem *item,
                  UA_RolePermissionType *userRolePermission,
                  size_t userRoleSize) {
    printf("Called allowAddReference\n");
    return UA_TRUE;
}

static UA_Boolean
allowDeleteNode(UA_Server *server, UA_AccessControl *ac,
                const UA_NodeId *sessionId, void *sessionContext,
                const UA_DeleteNodesItem *item, 
                UA_RolePermissionType *userRolePermission,
                size_t userRoleSize) {
    printf("Called allowDeleteNode\n");
    return UA_FALSE; // Do not allow deletion from client
}

static UA_Boolean
allowDeleteReference(UA_Server *server, UA_AccessControl *ac,
                     const UA_NodeId *sessionId, void *sessionContext,
                     const UA_DeleteReferencesItem *item,
                     UA_RolePermissionType *userRolePermission,
                     size_t userRoleSize) {
    printf("Called allowDeleteReference\n");
    return UA_TRUE;
}

UA_Boolean running = true;
static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = false;
}

static UA_UsernamePasswordLogin logins[2] = {
    {UA_STRING_STATIC("peter"), UA_STRING_STATIC("peter123")},
    {UA_STRING_STATIC("paula"), UA_STRING_STATIC("paula123")}
};

int main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);

    /* Disable anonymous logins, enable two user/password logins */
    config->accessControl.clear(&config->accessControl);
    UA_StatusCode retval;

#ifdef UA_ENABLE_ROLE_PERMISSIONS
    retval = UA_AccessControl_custom(config, true, NULL,
             &config->securityPolicies[config->securityPoliciesSize-1].policyUri, 2, logins);
#else
    retval = UA_AccessControl_default(config, false, NULL,
             &config->securityPolicies[config->securityPoliciesSize-1].policyUri, 2, logins);
#endif

    if(retval != UA_STATUSCODE_GOOD)
        goto cleanup;

    /* Set accessControl functions for nodeManagement */
    config->accessControl.allowAddNode = allowAddNode;
    config->accessControl.allowAddReference = allowAddReference;
    config->accessControl.allowDeleteNode = allowDeleteNode;
    config->accessControl.allowDeleteReference = allowDeleteReference;

    retval = UA_Server_run(server, &running);

 cleanup:
    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
