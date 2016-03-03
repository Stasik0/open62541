Creating and interacting with Nodes
===================================

The open62541 stack let's you access the guts of open62541 if you really feel the need for it. For simpler or not all that specific requirements, there is an extensive set of high-level abstractions available that let you easily and quickly use complex features in your programs. An example for a high-level abstraction is ''UA_Client_newSubscription()'', which spares you the internals of the subscription service (but also assumes sensible default values). High-Level abstractions make use of mid-level abstractions, which in turn use the low-level stack function like ''synchronousRequest()''.

This series of tutorials will stick to those high-level abstractions. Feel free to explore the API's source to see what they. Nodes are of course the foundation of OPC UA and we will start of at this point with creating, deleting and changing them.

API Concepts
------------

The following section will put you in contact with open62541 userspace API - that is functions available to you as a user. Functions in that API fall into three categories:

+-------------+-----------------------------------+
| Abstraction | Example                           |
+=============+===================================+
| High        | UA_Client_createSubscription()    |
+-------------+-----------------------------------+
| Medium      | UA_Client_read()                  |
+-------------+-----------------------------------+
| Low         | UA_decodeBinary()                 |
+-------------+-----------------------------------+

Low level abstractions presume that you are not only familiar with the inner workings OPC UA, but also with the precice implementation of these aspects in open62541.

Medium level abstractions allow you access to where the OPC UA Specification ends - you would for example have to fill out the contents of a read request before sending it and then parse the UA_ReadResponse structure returned by the call. This can be very powerful, but also includes a steep learning curve.

The High level abstraction concetrates on getting the job done in a simple manner for the user. This is the least flexible way of handling the stack, because at many places sensible defaults are presumed; at the same time using these functions is the easiest way of implementing an OPC UA application, as you will not have to consider how the stack or OPC UA actually gets things done. A concept of how nodes and datatypes are used are completely sufficient to handle OPC UA with this layer.

This tutorial will only introduce you to highlevel abstractions. Feel free to browse the example servers/clients and the doxygen documentation to find out more about the other two userspace layers.


Adding and deleting nodes
-------------------------

The current server example is very boring and the client doesn't do anything but connecting and disconnecting. We will fix that now by adding our own nodes at runtime (as opposed to loading them from an XML file) and we will do so once from our server and once from our client.

The user does not have direct access to the nodestore of the server; so even the server application cannot directly manipulate the memory nodes are stored in. Instead, a series of API calls allow the userspace to interact with the servers nodestore. How the functions do their job is hidden - especially for the client, who uses services to create/delete nodes. To the user however this provides a substantial simplifaction. Arguments passed to these functions may vary depending on the type of the node; but if they exist for both client and server, they can be used symmetrically. 

You can pick an appropriate function for adding and deleting nodes by sticking to the following regular expression::

  UA_(Client|Server)_(add|delete)<TYPE>Node();

The following table shows which of these functions are currently implemented.

+--------------+--------+--------+
| Node Type    | Server | Client |
+==============+========+========+
| Object       |  ✔,✔   |  ✔,✔   |
+--------------+--------+--------+
| Variable     |  ✔,✔   |  ✔,✔   |
+--------------+--------+--------+
| Method       |  ✔,✔   |  ✘,✔   |
+--------------+--------+--------+
| ReferenceType|  ✔,✔   |  ✔,✔   |
+--------------+--------+--------+
| ObjectType   |  ✔,✔   |  ✔,✔   |
+--------------+--------+--------+
| VariableType |  ✔,✔   |  ✘,✔   |
+--------------+--------+--------+
| DataType     |  ✔,✔   |  ✘,✔   |
+--------------+--------+--------+
| View         |  ✔,✔   |  ✘,✔   |
+--------------+--------+--------+

**FIXME**: The client should be able to do more than that. Please check back with us to see if we have come around to implement that feature.

Let us modify our current server to create a new object node (a folder) that will contain any objects and variables the clients wants to delete.::

    #include <stdio.h>
    #include <signal.h>

    # include "ua_types.h"
    # include "ua_server.h"
    # include "logger_stdout.h"
    # include "networklayer_tcp.h"

    UA_Boolean running;

    void stopHandler(int signal) {
      running = 0;
    }

    int main(void) {
      signal(SIGINT,  stopHandler);
      signal(SIGTERM, stopHandler);
      
      UA_Server *server = UA_Server_new(UA_ServerConfig_standard);
      UA_Server_addNetworkLayer(server, ServerNetworkLayerTCP_new(UA_ConnectionConfig_standard, 16664));
      running = true;
      
      UA_NodeId myObjectsId;
      UA_StatusCode retval = UA_Server_addObjectNode( 
        server, UA_NODEID_NUMERIC(1,1000), UA_QUALIFIEDNAME(1, "MyObjects"), 
        UA_LOCALIZEDTEXT("en_US", "MyObjects"),
        UA_LOCALIZEDTEXT("en_US", "A folder containing example objects and variables created by the client."),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), 0, 0,
        UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), &myObjectsId
      );
      if (retval)
        printf("Create node failed\n");
      else
        printf("Created objects folder with id ns=1;i=%d\n", myObjectsId.identifier.numeric);
    
      UA_Server_run(server, 1, &running);
      UA_Server_delete(server);
      
      printf("Bye\n");
      return 0;
    }

If you run the server now and check with UAExpert, you will find a new (empty) folder in /Objects. You may notice the numerous macros for simply creating OPC UA type variables in open62541. The ones used here create literals; we also provide ``UA_<type>_ALLOC`` macros for some of them that allow for storing the variable in a pointer.

Why was the NodeId myObjectsId passed? When creating dynamic node instances at runtime, chances are high that you will not care which id the node has, as long as you can reference it later. When passing numeric nodeids with a identifier 0 to open62541, the stack evaluates this as "any non allocated ID in that namespace" and assign the node a new one. To find out which ID was actually assigned to the new node, you *may* pass a pointer to a NodeId, which will (after a successfull node insertion) contain the nodeId of the new node. If you don't care about the ID of the node, you may also pass NULL as a pointer. The namespace index for nodes you create should never be 0, as that index is reserved for OPC UA's self-description (Namespace 0). So the following would have equally worked::

    UA_StatusCode retval = UA_Server_addObjectNode( 
      server, UA_NODEID_NUMERIC(1,1000), UA_QUALIFIEDNAME(1, "MyObjects"), 
      UA_LOCALIZEDTEXT("en_US", "MyObjects"),
      UA_LOCALIZEDTEXT("en_US", "A folder containing example objects and variables created by the client."),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), 0, 0,
      UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), NULL
    );
    if (retval)
      printf("Create node failed\n");

However, we will need that nodeId to actually have the client create a couple of nodes. To have the Client actually create a node, say an Object, we just need to pick the propper function and insert it into the example::

    #include <stdio.h>

    #include "ua_types.h"
    #include "ua_server.h"
    #include "logger_stdout.h"
    #include "networklayer_tcp.h"

    int main(void) {
      UA_Client *client = UA_Client_new(UA_ClientConfig_standard, Logger_Stdout_new());
      UA_StatusCode retval = UA_Client_connect(client, ClientNetworkLayerTCP_connect, "opc.tcp://localhost:16664");
      if(retval != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client);
        return retval;
      }
      
      // Add a new node with a server-picked nodeId
      UA_NodeId addedNodeId;
      UA_StatusCode retval = UA_Client_addObjectNode( 
        client, UA_NODEID_NUMERIC(1,0), UA_QUALIFIEDNAME(1, "ClientSideObject1"), 
        UA_LOCALIZEDTEXT("en_US", "ClientSideObject1"),
        UA_LOCALIZEDTEXT("en_US", "A dynamic object node added by the client."),
        UA_NODEID_NUMERIC(1, 1000), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), 0, 0,
        UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), &addedNodeId
      );
      if (retval)
        printf("Create node failed\n");
      
      UA_Client_disconnect(client);
      UA_Client_delete(client);
      return 0;
    } 

If you start the server, run the client and take a look at the server with UAExpert afterwards, you will see that the client has created a new node under the 'MyObjects' node created by the server. We are passing the NodeId (1,0), so the server will pick an appropriate ID for this new node when he creates it.

Supposing the client wants to clean up? All we need to do is to pass the nodeId returned by the server.::

    #include <stdio.h>

    #include "ua_types.h"
    #include "ua_server.h"
    #include "logger_stdout.h"
    #include "networklayer_tcp.h"

    int main(void) {
      UA_Client *client = UA_Client_new(UA_ClientConfig_standard, Logger_Stdout_new());
      UA_StatusCode retval = UA_Client_connect(client, ClientNetworkLayerTCP_connect, "opc.tcp://localhost:16664");
      if(retval != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client);
        return retval;
      }
      
      // Add a new node with a server-picked nodeId
      UA_NodeId addedNodeId;
      UA_StatusCode retval = UA_Client_addObjectNode( 
        client, UA_NODEID_NUMERIC(1,0), UA_QUALIFIEDNAME(1, "ClientSideObject1"), 
        UA_LOCALIZEDTEXT("en_US", "ClientSideObject1"),
        UA_LOCALIZEDTEXT("en_US", "A dynamic object node added by the client."),
        UA_NODEID_NUMERIC(1, 1000), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), 0, 0,
        UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE), &addedNodeId
      );
      if (retval)
        printf("Create node failed\n");
      
      // Cleanup the newly created node
      UA_Client_deleteObjectNode(client, addedNodeId);
      
      UA_Client_disconnect(client);
      UA_Client_delete(client);
      return 0;
    } 

Conclusion
----------

In this tutorial you learned how to create a client and create or delete nodes on the server and client side. In the next tutorial, we will cover how to manipulate node contents and how to use callbacks in datasources for ever changing variables (such as time or temperature data) on the serverside.

