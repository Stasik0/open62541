#include <stdio.h>
#include <stdlib.h>

#include "ua_types.h"
#include "ua_config_standard.h"
#include "check.h"

START_TEST(Instantiation_inheritance_masking_ShallWork)
{
    UA_ServerConfig config = UA_ServerConfig_standard;
    UA_Server *server = UA_Server_new(config);

    /* create information model */
    UA_ObjectTypeAttributes otAttr;
    UA_ObjectTypeAttributes_init(&otAttr);
    otAttr.description = UA_LOCALIZEDTEXT("en_US", "A field device");
    otAttr.displayName = UA_LOCALIZEDTEXT("en_US", "FieldDeviceType");
    UA_Server_addObjectTypeNode(server, UA_NODEID_NUMERIC(1, 10000),
                                UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE), UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                UA_QUALIFIEDNAME(1, "FieldDeviceType"), otAttr, NULL, NULL);

    UA_VariableAttributes   vAttr;
    UA_VariableAttributes_init(&vAttr);
    vAttr.description =  UA_LOCALIZEDTEXT("en_US", "Model name of the field device");
    UA_String defaultModelName = UA_STRING("");
    UA_Variant_setScalarCopy(&vAttr.value, &defaultModelName, &UA_TYPES[UA_TYPES_STRING]);
    vAttr.displayName =  UA_LOCALIZEDTEXT("en_US", "ModelName");
    UA_Server_addVariableNode(server, UA_NODEID_NUMERIC(1, 10001),
                              UA_NODEID_NUMERIC(1, 10000), UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                              UA_QUALIFIEDNAME(1, "ModelName"), UA_NODEID_NULL, vAttr, NULL, NULL);

    UA_VariableAttributes_init(&vAttr);
    vAttr.description =  UA_LOCALIZEDTEXT("en_US", "serial number of the field device");
    UA_String defaultSN = UA_STRING("");
    UA_Variant_setScalarCopy(&vAttr.value, &defaultSN, &UA_TYPES[UA_TYPES_STRING]);
    vAttr.displayName =  UA_LOCALIZEDTEXT("en_US", "SerialNumber");
    UA_Server_addVariableNode(server, UA_NODEID_NUMERIC(1, 10002),
                            UA_NODEID_NUMERIC(1, 10000), UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            UA_QUALIFIEDNAME(1, "SerialNumber"), UA_NODEID_NULL, vAttr, NULL, NULL);

    UA_ObjectTypeAttributes_init(&otAttr);
    otAttr.description = UA_LOCALIZEDTEXT("en_US", "A pump");
    otAttr.displayName = UA_LOCALIZEDTEXT("en_US", "PumpType");
    UA_Server_addObjectTypeNode(server, UA_NODEID_NUMERIC(1, 10003),
                                UA_NODEID_NUMERIC(1, 10000), UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                UA_QUALIFIEDNAME(1, "FieldDeviceType"), otAttr, NULL, NULL);

    UA_VariableAttributes_init(&vAttr);
    vAttr.description =  UA_LOCALIZEDTEXT("en_US", "Motor RPM");
    vAttr.displayName =  UA_LOCALIZEDTEXT("en_US", "MotorRPM");
    UA_Server_addVariableNode(server, UA_NODEID_NUMERIC(1, 10004),
                            UA_NODEID_NUMERIC(1, 10003), UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            UA_QUALIFIEDNAME(1, "MotorRPM"), UA_NODEID_NULL, vAttr, NULL, NULL);

    UA_Argument arg;
    UA_Argument_init(&arg);
    arg.arrayDimensionsSize = 0;
    arg.arrayDimensions = NULL;
    arg.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    arg.description = UA_LOCALIZEDTEXT("en_US", "Output Argument");
    arg.name = UA_STRING("OutputArgument");
    arg.valueRank = -1;

    UA_MethodAttributes mAttr;
    UA_MethodAttributes_init(&mAttr);
    mAttr.description = UA_LOCALIZEDTEXT("en_US","Start pump");
    mAttr.displayName = UA_LOCALIZEDTEXT("en_US","Start pump");
    mAttr.executable = true;
    mAttr.userExecutable = true;
    UA_Server_addMethodNode(server, UA_NODEID_NUMERIC(1,10005),
                            UA_NODEID_NUMERIC(1, 10003), UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            UA_QUALIFIEDNAME(1, "startPump"), mAttr, NULL, NULL, 0, NULL, 1, &arg, NULL);

    UA_Argument_init(&arg);
    arg.arrayDimensionsSize = 0;
    arg.arrayDimensions = NULL;
    arg.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    arg.description = UA_LOCALIZEDTEXT("en_US", "Output Argument");
    arg.name = UA_STRING("OutputArgument");
    arg.valueRank = -1;

    UA_MethodAttributes_init(&mAttr);
    mAttr.description = UA_LOCALIZEDTEXT("en_US","Stop pump");
    mAttr.displayName = UA_LOCALIZEDTEXT("en_US","Stop pump");
    mAttr.executable = true;
    mAttr.userExecutable = true;
    UA_Server_addMethodNode(server, UA_NODEID_NUMERIC(1,10006),
                            UA_NODEID_NUMERIC(1, 10003), UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            UA_QUALIFIEDNAME(1, "stopPump"), mAttr, NULL, NULL, 0, NULL, 1, &arg, NULL);


    UA_ObjectTypeAttributes_init(&otAttr);
    otAttr.description = UA_LOCALIZEDTEXT("en_US", "Pump AX-2500");
    otAttr.displayName = UA_LOCALIZEDTEXT("en_US", "PumpAX2500Type");
    UA_Server_addObjectTypeNode(server, UA_NODEID_NUMERIC(1, 10007),
                                UA_NODEID_NUMERIC(1, 10003), UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                UA_QUALIFIEDNAME(1, "PumpAX2500Type"), otAttr, NULL, NULL);

    UA_VariableAttributes_init(&vAttr);
    vAttr.description =  UA_LOCALIZEDTEXT("en_US", "Model name of the pump");
    vAttr.displayName =  UA_LOCALIZEDTEXT("en_US", "ModelName");
    UA_String defaultName = UA_STRING("AX-2500");
    UA_Variant_setScalarCopy(&vAttr.value, &defaultName, &UA_TYPES[UA_TYPES_STRING]);
    UA_Server_addVariableNode(server, UA_NODEID_NUMERIC(1, 10008),
                            UA_NODEID_NUMERIC(1, 10007), UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                            UA_QUALIFIEDNAME(1, "ModelName"), UA_NODEID_NULL, vAttr, NULL, NULL);



    // Instantiate objects
    UA_ObjectAttributes oAttr;
    UA_ObjectAttributes_init(&oAttr);
    oAttr.description = UA_LOCALIZEDTEXT("en_US","a specific field device");
    oAttr.displayName = UA_LOCALIZEDTEXT("en_US","FD314");
    UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 10009),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                            UA_QUALIFIEDNAME(1, "FD314"),
                            UA_NODEID_NUMERIC(1, 10000),
                            oAttr, NULL, NULL);

    UA_ObjectAttributes_init(&oAttr);
    oAttr.description = UA_LOCALIZEDTEXT("en_US","Pump T1.A3.P002");
    oAttr.displayName = UA_LOCALIZEDTEXT("en_US","T1.A3.P002");
    UA_StatusCode retval = UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 10010),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                            UA_QUALIFIEDNAME(1, "T1.A3.P002"),
                            UA_NODEID_NUMERIC(1, 10007),
                            oAttr, NULL, NULL);
    ck_assert_int_eq(UA_STATUSCODE_GOOD, retval);


    UA_LocalizedText outDisplayName;
    retval = UA_Server_readDisplayName(server, UA_NODEID_NUMERIC(1, 1315), &outDisplayName);
    ck_assert_int_eq(UA_STATUSCODE_GOOD, retval);
    retval = UA_Server_readDisplayName(server, UA_NODEID_NUMERIC(1, 1327), &outDisplayName);
    ck_assert_int_ne(UA_STATUSCODE_GOOD, retval);

    UA_Variant outValue;
    retval = UA_Server_readValue(server, UA_NODEID_NUMERIC(1, 1315), &outValue);
    ck_assert_int_eq(UA_STATUSCODE_GOOD, retval);

    // FIXME: I would like to test if the masking was succesful
}
END_TEST

static Suite* testSuite_Instantiation(void) {
    Suite *s = suite_create("Instantiation");
    TCase *tc_core = tcase_create("Inheritance");
    tcase_add_test(tc_core, Instantiation_inheritance_masking_ShallWork);

    suite_add_tcase(s,tc_core);
    return s;
}

int main(void) {
    int number_failed = 0;

    Suite *s;
    SRunner *sr;

    s = testSuite_Instantiation();
    sr = srunner_create(s);
    srunner_run_all(sr,CK_NORMAL);
    number_failed += srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
