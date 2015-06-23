#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "check.h"
#include "server/ua_nodestore.h"
#include "server/ua_services.h"
#include "ua_client.h"
#include "ua_nodeids.h"
#include "ua_statuscodes.h"
#include "ua_types.h"
#include "ua_util.h"
#include "server/ua_server_internal.h"


//#include "server/ua_services_attribute.c"

#ifdef UA_MULTITHREADING
#include <pthread.h>
#include <urcu.h>
#endif

static void copyNames(UA_Node *node, char *name) {
    node->browseName = UA_QUALIFIEDNAME_ALLOC(0, name);
    node->displayName = UA_LOCALIZEDTEXT_ALLOC("", name);
    node->description = UA_LOCALIZEDTEXT_ALLOC("", name);
}


static UA_Server* makeTestSequence(void) {
	UA_Server *server = UA_Server_new(UA_ServerConfig_standard);

	/* VariableNode */
	UA_Variant *myIntegerVariant = UA_Variant_new();
	UA_Int32 myInteger = 42;
	UA_Variant_setScalarCopy(myIntegerVariant, &myInteger,
			&UA_TYPES[UA_TYPES_INT32]);
	const UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, "the answer");
	const UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "the.answer");
	UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
	UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
	UA_Server_addVariableNode(server, myIntegerVariant, myIntegerName,
			myIntegerNodeId, parentNodeId, parentReferenceNodeId);

	/* ObjectNode */
	UA_Server_addObjectNode(server,UA_QUALIFIEDNAME(1, "Demo"), UA_NODEID_NUMERIC(1, 50), UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_NODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE));

	/* ReferenceTypeNode */
    UA_ReferenceTypeNode *hierarchicalreferences = UA_ReferenceTypeNode_new();
    copyNames((UA_Node*)hierarchicalreferences, "Hierarchicalreferences");
    hierarchicalreferences->nodeId.identifier.numeric = UA_NS0ID_HIERARCHICALREFERENCES;
    hierarchicalreferences->isAbstract = UA_TRUE;
    hierarchicalreferences->symmetric  = UA_FALSE;
    hierarchicalreferences->inverseName = UA_LOCALIZEDTEXT("", "test");

    UA_Server_addNode(server, (UA_Node*)hierarchicalreferences,
                      UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_REFERENCES),
                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE));

	/* ViewNode */
    UA_ViewNode *viewtest = UA_ViewNode_new();
    copyNames((UA_Node*)viewtest, "Viewtest");
    viewtest->nodeId.identifier.numeric = UA_NS0ID_VIEWNODE;

    UA_Server_addNode(server, (UA_Node*)viewtest,
                      UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_VIEWSFOLDER),
                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE));

	/* MethodNode */
    UA_MethodNode *methodtest = UA_MethodNode_new();
    copyNames((UA_Node*)methodtest, "Methodtest");
    methodtest->nodeId.identifier.numeric = UA_NS0ID_METHODNODE;

    UA_Server_addNode(server, (UA_Node*)methodtest,
                      UA_EXPANDEDNODEID_NUMERIC(0, 3),
                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE));


	return server;
}

static UA_VariableNode* makeCompareSequence(void) {
	UA_VariableNode *node = UA_VariableNode_new();
	UA_Variant *myIntegerVariant = UA_Variant_new();
	UA_Int32 myInteger = 42;
	UA_Variant_setScalarCopy(myIntegerVariant, &myInteger,
			&UA_TYPES[UA_TYPES_INT32]);
	const UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, "the answer");
	const UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "the.answer");
	UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
	//UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
	node->value.variant=*myIntegerVariant;
	UA_NodeId_copy(&myIntegerNodeId,&node->nodeId);
	UA_QualifiedName_copy(&myIntegerName,&node->browseName);
	UA_String_copy(&myIntegerName.name,&node->displayName.text);
	UA_ExpandedNodeId parentId;
	UA_ExpandedNodeId_init(&parentId);
	UA_NodeId_copy(&parentNodeId,&parentId.nodeId);

	return node;
}

START_TEST(ReadSingleAttributeValueWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_VALUE;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_INT32], resp.value.type);
		ck_assert_int_eq(42, *(UA_Int32* )resp.value.data);
	}END_TEST

START_TEST(ReadSingleAttributeNodeIdWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_NODEID;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);
		UA_NodeId* respval;
		respval = (UA_NodeId*) resp.value.data;
		const UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "the.answer");

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_NODEID], resp.value.type);
		ck_assert_int_eq(1, respval->namespaceIndex);
		for (int var = 0; var < respval->identifier.string.length; ++var) {
			ck_assert_int_eq(myIntegerNodeId.identifier.string.data[var],
					respval->identifier.string.data[var]);
		}
		UA_free(respval);
	}END_TEST

START_TEST(ReadSingleAttributeNodeClassWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_NODECLASS;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		//ck_assert_int_eq(&UA_TYPES[UA_TYPES_NODECLASS],resp.value.type);
	}END_TEST

START_TEST(ReadSingleAttributeBrowseNameWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_BROWSENAME;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		UA_QualifiedName* respval;
		respval = (UA_QualifiedName*) resp.value.data;
		const UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1,
				"the answer");

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_QUALIFIEDNAME], resp.value.type);
		ck_assert_int_eq(1, respval->namespaceIndex);
		ck_assert_int_eq(myIntegerName.name.length, respval->name.length);
		for (int var = 0; var < respval->name.length - 1; ++var) {
			ck_assert_int_eq(myIntegerName.name.data[var],
					respval->name.data[var]);
		}
		UA_free(respval);
	}END_TEST

START_TEST(ReadSingleAttributeDisplayNameWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DISPLAYNAME;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		UA_LocalizedText* respval;
		respval = (UA_LocalizedText*) resp.value.data;
		const UA_LocalizedText comp = UA_LOCALIZEDTEXT("locale", "the answer");
		UA_VariableNode* compNode = makeCompareSequence();

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_LOCALIZEDTEXT], resp.value.type);
		ck_assert_int_eq(comp.text.length, respval->text.length);
		for (int var = 0; var < respval->text.length - 1; ++var) {
			ck_assert_int_eq(comp.text.data[var], respval->text.data[var]);
		}
		ck_assert_int_eq(compNode->displayName.locale.length, respval->locale.length);
		for (int var = 0; var < respval->locale.length - 1; ++var) {
			ck_assert_int_eq(compNode->displayName.locale.data[var], respval->locale.data[var]);
		}
		UA_free(respval);
	}END_TEST

START_TEST(ReadSingleAttributeDescriptionWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DESCRIPTION;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);
		UA_LocalizedText* respval;
		respval = (UA_LocalizedText*) resp.value.data;
		UA_VariableNode* compNode = makeCompareSequence();

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_LOCALIZEDTEXT], resp.value.type);
		ck_assert_int_eq(compNode->description.locale.length, respval->locale.length);
				for (int var = 0; var < respval->locale.length - 1; ++var) {
					ck_assert_int_eq(compNode->description.locale.data[var], respval->locale.data[var]);
				}
		ck_assert_int_eq(compNode->description.text.length, respval->text.length);
		for (int var = 0; var < respval->text.length - 1; ++var) {
			ck_assert_int_eq(compNode->description.text.data[var], respval->text.data[var]);
		}
		UA_free(respval);
	}END_TEST

START_TEST(ReadSingleAttributeWriteMaskWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_WRITEMASK;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);
		//UA_UInt32* respval;
		//respval = (UA_UInt32*) resp.value.data;
		//UA_VariableNode* compNode = makeCompareSequence();

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_UINT32], resp.value.type);
		//ck_assert_int_eq(*(UA_UInt32* )compNode->writeMask,respval);
	}END_TEST

START_TEST(ReadSingleAttributeUserWriteMaskWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_USERWRITEMASK;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_UINT32], resp.value.type);
	}END_TEST

START_TEST(ReadSingleAttributeIsAbstractWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId.identifier.numeric = UA_NS0ID_HIERARCHICALREFERENCES;
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_ISABSTRACT;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_BOOLEAN], resp.value.type);
		ck_assert(*(UA_Boolean* )resp.value.data==UA_TRUE);
	}END_TEST

START_TEST(ReadSingleAttributeSymmetricWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId.identifier.numeric = UA_NS0ID_HIERARCHICALREFERENCES;
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_SYMMETRIC;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_BOOLEAN], resp.value.type);
		ck_assert(*(UA_Boolean* )resp.value.data==UA_FALSE);
	}END_TEST

START_TEST(ReadSingleAttributeInverseNameWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId.identifier.numeric = UA_NS0ID_HIERARCHICALREFERENCES;
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_INVERSENAME;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		/*UA_LocalizedText* respval;
		respval = (UA_LocalizedText*) resp.value.data;
		const UA_LocalizedText comp = UA_LOCALIZEDTEXT("", "test");
*/
		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_LOCALIZEDTEXT],resp.value.type);
		/*ck_assert_int_eq(comp.text.length, respval->text.length);
		for (int var = 0; var < respval->text.length - 1; ++var) {
			ck_assert_int_eq(comp.text.data[var], respval->text.data[var]);
		}
		ck_assert_int_eq(comp.locale.length, respval->locale.length);
		for (int var = 0; var < respval->locale.length - 1; ++var) {
			ck_assert_int_eq(comp.locale.data[var], respval->locale.data[var]);
		}
		UA_free(respval);*/
	}END_TEST

START_TEST(ReadSingleAttributeContainsNoLoopsWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId.identifier.numeric = UA_NS0ID_VIEWNODE;
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_CONTAINSNOLOOPS;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_BOOLEAN], resp.value.type);
		ck_assert(*(UA_Boolean* )resp.value.data==UA_FALSE);
	}END_TEST

START_TEST(ReadSingleAttributeEventNotifierWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_NUMERIC(1, 50);
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_EVENTNOTIFIER;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_BYTE],resp.value.type);
		ck_assert_int_eq(*(UA_Byte*)resp.value.data, 0);
	}END_TEST

START_TEST(ReadSingleAttributeDataTypeWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DATATYPE;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		//UA_NodeId* respval;
		//respval = (UA_NodeId*) resp.value.data;
		//const UA_VariableNode compNode = makeCompareSequence();
		//const UA_NodeId comp = compNode;

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_NODEID], resp.value.type);
	}END_TEST

START_TEST(ReadSingleAttributeValueRankWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_VALUERANK;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_INT32], resp.value.type);

		ck_assert_int_eq(-2, *(UA_Int32* )resp.value.data);
	}END_TEST

START_TEST(ReadSingleAttributeArrayDimensionsWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_ARRAYDIMENSIONS;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_INT32], resp.value.type);
	}END_TEST

START_TEST(ReadSingleAttributeAccessLevelWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_ACCESSLEVEL;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_BYTE], resp.value.type);
		ck_assert_int_eq(*(UA_Byte*)resp.value.data, 0);
	}END_TEST

START_TEST(ReadSingleAttributeUserAccessLevelWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_USERACCESSLEVEL;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);
		UA_VariableNode* compNode = makeCompareSequence();

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_BYTE], resp.value.type);
		ck_assert_int_eq(*(UA_Byte*)resp.value.data, compNode->userAccessLevel);
	}END_TEST

START_TEST(ReadSingleAttributeMinimumSamplingIntervalWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId =
				UA_ATTRIBUTEID_MINIMUMSAMPLINGINTERVAL;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		//UA_Double* respval;
		//respval = (UA_Double*) resp.value.data;
		//UA_VariableNode* compNode = makeCompareSequence();

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_DOUBLE], resp.value.type);
		//ck_assert_int_eq(compNode->minimumSamplingInterval,respval);
	}END_TEST

START_TEST(ReadSingleAttributeHistorizingWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_HISTORIZING;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_BOOLEAN], resp.value.type);
		ck_assert(*(UA_Boolean* )resp.value.data==UA_FALSE);
	}END_TEST

START_TEST(ReadSingleAttributeExecutableWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_EXECUTABLE;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0], &resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_BOOLEAN], resp.value.type);
	}END_TEST

START_TEST(ReadSingleAttributeUserExecutableWithoutTimestamp)
	{
		UA_Server *server = makeTestSequence();

		UA_DataValue resp;
		UA_DataValue_init(&resp);
		UA_ReadRequest rReq;
		UA_ReadRequest_init(&rReq);
		rReq.nodesToRead = UA_ReadValueId_new();
		rReq.nodesToReadSize = 1;
		rReq.nodesToRead[0].nodeId = UA_NODEID_STRING(1, "the.answer");
		rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_USEREXECUTABLE;

		readValue(server, UA_TIMESTAMPSTORETURN_NEITHER, &rReq.nodesToRead[0],
				&resp);

		ck_assert_int_eq(-1, resp.value.arrayLength);
		ck_assert_int_eq(&UA_TYPES[UA_TYPES_BOOLEAN], resp.value.type);
	}END_TEST

static Suite * testSuite_services_attributes(void) {
	Suite *s = suite_create("services_attributes_read");

	TCase *tc_readSingleAttributes = tcase_create("readSingleAttributes");
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeValueWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeNodeIdWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeNodeClassWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeBrowseNameWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeDisplayNameWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeDescriptionWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeWriteMaskWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeUserWriteMaskWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeIsAbstractWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeSymmetricWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeInverseNameWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeContainsNoLoopsWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeEventNotifierWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeDataTypeWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeValueRankWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeArrayDimensionsWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeAccessLevelWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeUserAccessLevelWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeMinimumSamplingIntervalWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeHistorizingWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeExecutableWithoutTimestamp);
	tcase_add_test(tc_readSingleAttributes,
			ReadSingleAttributeUserExecutableWithoutTimestamp);

	suite_add_tcase(s, tc_readSingleAttributes);
	return s;
}

int main(void) {

	int number_failed = 0;
	Suite *s;
	s = testSuite_services_attributes();
	SRunner *sr = srunner_create(s);
	srunner_set_log(sr, "test.log");
	void srunner_set_fork_status(SRunner * sr, enum fork_status CK_NOFORK);
	srunner_run_all(sr, CK_NORMAL);

	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
