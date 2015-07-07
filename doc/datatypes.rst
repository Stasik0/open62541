Data Types
==========

Generic Data Type Handling
--------------------------

All data types are combinations of the 25 builtin data types show below. Types
are described in the UA_DataType structure.

.. doxygenstruct:: UA_DataType
   :members:

.. c:var:: const UA_DataType UA_TYPES[UA_TYPES_COUNT]

  The datatypes defined in the standard are stored in the ``UA_TYPES`` array.
  A typical function call is ``UA_Array_new(&data_ptr, 20, &UA_TYPES[UA_TYPES_STRING])``.

.. doxygenfunction:: UA_new
.. doxygenfunction:: UA_init
.. doxygenfunction:: UA_copy
.. doxygenfunction:: UA_deleteMembers
.. doxygenfunction:: UA_delete

For all datatypes, there are also macros with syntactic sugar over calling the
generic functions with a pointer into the ``UA_TYPES`` array.

.. c:function:: <typename>_new()

  Allocates the memory for the type and runs _init on the returned variable.
  Returns null if no memory could be allocated.

.. c:function:: <typename>_init(<typename> *value)

  Sets all members of the type to a default value, usually zero. Arrays (e.g.
  for strings) are set to a length of -1.

.. c:function:: <typename>_copy(<typename> *src, <typename> *dst)

  Copies a datatype. This performs a deep copy iterating over the members.
  Copying into variants with an external data source is not permitted. If
  copying fails, a deleteMembers is performed and an error code returned.

.. c:function:: <typename>_deleteMembers(<typename> *value)

   Frees the memory of dynamically sized members of a datatype (e.g. arrays).

.. c:function:: <typename>_delete(<typename> *value)

   Frees the memory of the datatype and its dynamically allocated members.

Array Handling
--------------
   
.. doxygenfunction:: UA_Array_new
.. doxygenfunction:: UA_Array_copy
.. doxygenfunction:: UA_Array_delete

Builtin Data Types
------------------

Number-Types
^^^^^^^^^^^^

.. doxygentypedef:: UA_Boolean
.. doxygentypedef:: UA_SByte
.. doxygentypedef:: UA_Byte
.. doxygentypedef:: UA_Int16
.. doxygentypedef:: UA_UInt16
.. doxygentypedef:: UA_Int32
.. doxygentypedef:: UA_UInt32
.. doxygentypedef:: UA_Int64
.. doxygentypedef:: UA_UInt64
.. doxygentypedef:: UA_Float
.. doxygentypedef:: UA_Double

UA_String
^^^^^^^^^
.. doxygenstruct:: UA_String
  :members:

.. c:macro:: UA_STRING_NULL

  The empty string

.. c:macro:: UA_STRING(CHARS)
     
  Creates an UA_String from an array of ``char``. The characters are not copied
  on the heap. Instead, the string points into the existing array.

.. c:macro:: UA_STRING_ALLOC(CHARS)
     
  Creates an UA_String from an array of ``char``. The characters are copied on
  the heap.

.. doxygenfunction:: UA_String_equal
.. doxygenfunction:: UA_String_copyprintf

UA_DateTime
^^^^^^^^^^^
.. doxygentypedef:: UA_DateTime
.. doxygenfunction:: UA_DateTime_now(void)
.. doxygenfunction:: UA_DateTime_toString
.. doxygenfunction:: UA_DateTime_toStruct

UA_Guid
^^^^^^^
.. doxygenstruct:: UA_Guid
.. doxygenfunction:: UA_Guid_equal
.. doxygenfunction:: UA_Guid_random
   
UA_ByteString
^^^^^^^^^^^^^
Bytestring are just a redefinition of strings. The semantic difference is that
ByteStrings may hold non-UTF8 data.

.. doxygentypedef:: UA_ByteString

.. c:macro:: UA_BYTESTRING_NULL

   The empty ByteString

.. c:function:: UA_Boolean UA_ByteString_equal(const UA_ByteString *s1, const UA_ByteString *s2)

   Compares two ByteStrings.
   
UA_XmlElement
^^^^^^^^^^^^^
XmlElements are just a redefinition of strings.

.. doxygentypedef:: UA_XmlElement

UA_NodeId
^^^^^^^^^
.. doxygenstruct:: UA_NodeId
  :members:

.. doxygenfunction:: UA_NodeId_equal
.. doxygenfunction:: UA_NodeId_isNull

.. c:macro:: UA_NODEID_NULL

  The null NodeId

.. c:macro:: UA_NODEID_NUMERIC(NSID, NUMERICID)
.. c:macro:: UA_NODEID_STRING(NSID, CHARS)
.. c:macro:: UA_NODEID_STRING_ALLOC(NSID, CHARS)
.. c:macro:: UA_NODEID_GUID(NSID, GUID)
.. c:macro:: UA_NODEID_BYTESTRING(NSID, CHARS)
.. c:macro:: UA_NODEID_BYTESTRING_ALLOC(NSID, CHARS)

UA_ExpandedNodeId
^^^^^^^^^^^^^^^^^
.. doxygenstruct:: UA_ExpandedNodeId
  :members:

.. doxygenfunction:: UA_ExpandedNodeId_isNull
.. c:macro:: UA_EXPANDEDNODEID_NUMERIC(NSID, NUMERICID)
  
UA_StatusCode
^^^^^^^^^^^^^
Many functions in open62541 return either ``UA_STATUSCODE_GOOD`` or an error code.

.. doxygentypedef:: UA_StatusCode

UA_QualifiedName
^^^^^^^^^^^^^^^^   
.. doxygenstruct:: UA_QualifiedName
  :members:

.. c:macro:: UA_QUALIFIEDNAME(NSID, CHARS)
.. c:macro:: UA_QUALIFIEDNAME_ALLOC(NSID, CHARS)
  
UA_LocalizedText
^^^^^^^^^^^^^^^^
.. doxygenstruct:: UA_LocalizedText
  :members:

.. c:macro:: UA_LOCALIZEDTEXT(LOCALE, TEXT)
   Takes two arrays of ``char`` as the input.

.. c:macro:: UA_LOCALIZEDTEXT_ALLOC(LOCALE, TEXT)
  
UA_ExtensionObject
^^^^^^^^^^^^^^^^^^

.. doxygenstruct:: UA_ExtensionObject
  :members:

UA_DataValue
^^^^^^^^^^^^

.. doxygenstruct:: UA_DataValue
  :members:
  :undoc-members:

UA_Variant
^^^^^^^^^^

.. doxygenstruct:: UA_Variant
  :members:

.. doxygenfunction:: UA_Variant_isScalar
.. doxygenfunction:: UA_Variant_setScalar
.. doxygenfunction:: UA_Variant_setScalarCopy
.. doxygenfunction:: UA_Variant_setArray
.. doxygenfunction:: UA_Variant_setArrayCopy

.. doxygenstruct:: UA_NumericRange
   :undoc-members:

.. doxygenfunction:: UA_Variant_setRange
.. doxygenfunction:: UA_Variant_setRangeCopy
        
UA_DiagnosticInfo
^^^^^^^^^^^^^^^^^

.. doxygenstruct:: UA_DiagnosticInfo
  :members:
  :undoc-members:
