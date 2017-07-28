/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */

/**
 * Working with Variable Types
 * ---------------------------
 *
 * Variable types have three functions:
 *
 * - Constrain the possible data type, value rank and array dimensions of the
 *   variables of that type. This allows interface code to be written against
 *   the generic type definition, so it is applicable for all instances.
 * - Provide a sensible default value
 * - Enable a semantic interpretation of the variable based on its type
 *
 * In the example of this tutorial, we represent a point in 2D space by an array
 * of double values. The following function adds the corresponding
 * VariableTypeNode to the hierarchy of variable types.
 */

#include <signal.h>
#include "open62541.h"

static UA_NodeId pointTypeId;

static void
addVariableType2DPoint(UA_Server *server) {
    UA_VariableTypeAttributes vtAttr = UA_VariableTypeAttributes_default;
    vtAttr.dataType = UA_TYPES[UA_TYPES_DOUBLE].typeId;
    vtAttr.valueRank = 1; /* array with one dimension */
    UA_UInt32 arrayDims[1] = {2};
    vtAttr.arrayDimensions = arrayDims;
    vtAttr.arrayDimensionsSize = 1;
    vtAttr.displayName = UA_LOCALIZEDTEXT("en_US", "2DPoint Type");

    /* a matching default value is required */
    UA_Double zero[2] = {0.0, 0.0};
    UA_Variant_setArray(&vtAttr.value, zero, 2, &UA_TYPES[UA_TYPES_DOUBLE]);

    UA_Server_addVariableTypeNode(server, UA_NODEID_NULL,
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_BASEVARIABLETYPE),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                  UA_QUALIFIEDNAME(1, "2DPoint Type"), UA_NODEID_NULL,
                                  vtAttr, NULL, &pointTypeId);
}

/**
 * Now the new variable type for *2DPoint* can be referenced during the creation
 * of a new variable. If no value is given, the default from the variable type
 * is copied during instantiation.
 */

static UA_NodeId pointVariableId;

static void
addVariable(UA_Server *server) {
    /* Prepare the node attributes */
    UA_VariableAttributes vAttr = UA_VariableAttributes_default;
    vAttr.dataType = UA_TYPES[UA_TYPES_DOUBLE].typeId;
    vAttr.valueRank = 1; /* array with one dimension */
    UA_UInt32 arrayDims[1] = {2};
    vAttr.arrayDimensions = arrayDims;
    vAttr.arrayDimensionsSize = 1;
    vAttr.displayName = UA_LOCALIZEDTEXT("en_US", "2DPoint Variable");
    /* vAttr.value is left empty, the server instantiates with the default value */

    /* Add the node */
    UA_Server_addVariableNode(server, UA_NODEID_NULL,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "2DPoint Type"), pointTypeId,
                              vAttr, NULL, &pointVariableId);
}

/**
 * The constraints of the variable type are enforced when creating new variable
 * instances of the type. In the following function, adding a variable of
 * *2DPoint* type with a string value fails because The value does not match the
 * variable type constraints. */

static void
addVariableFail(UA_Server *server) {
    /* Prepare the node attributes */
    UA_VariableAttributes vAttr = UA_VariableAttributes_default;
    vAttr.dataType = UA_TYPES[UA_TYPES_DOUBLE].typeId;
    vAttr.valueRank = -1; /* a scalar. this is not allowed per the variable type */
    vAttr.displayName = UA_LOCALIZEDTEXT("en_US", "2DPoint Variable (fail)");
    UA_String s = UA_STRING("2dpoint?");
    UA_Variant_setScalar(&vAttr.value, &s, &UA_TYPES[UA_TYPES_STRING]);

    /* Add the node */
    UA_Server_addVariableNode(server, UA_NODEID_NULL,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "2DPoint Type (fail)"), pointTypeId,
                              vAttr, NULL, NULL);
}

/**
 * The constraints of the variable type are enforced when writing the datatype,
 * valuerank and arraydimensions attributes of the variable. This, in turn,
 * constrains the value attribute of the variable. */

static void
writeVariable(UA_Server *server) {
    UA_StatusCode retval = UA_Server_writeValueRank(server, pointVariableId, 0);
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Setting the Value Rank failed with Status Code %s\n",
                UA_StatusCode_name(retval));

}

/** It follows the main server code, making use of the above definitions. */

UA_Boolean running = true;
static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = false;
}

int main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_ServerConfig config = UA_ServerConfig_standard;
    UA_ServerNetworkLayer nl =
        UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, 16664);
    config.networkLayers = &nl;
    config.networkLayersSize = 1;
    UA_Server *server = UA_Server_new(config);

    addVariableType2DPoint(server);
    addVariable(server);
    addVariableFail(server);
    writeVariable(server);

    UA_Server_run(server, &running);
    UA_Server_delete(server);
    nl.deleteMembers(&nl);
    return 0;
}
