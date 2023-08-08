API
=======

The data API is centered around :type:`cbor_item_t`, a generic handle for any CBOR item. There are functions to

 - create items,
 - set items' data,
 - parse serialized data into items,
 - manage, move, and links item together.

The single most important thing to keep in mind is: :type:`cbor_item_t` **is an opaque type and should only be manipulated using the appropriate functions!** Think of it as an object.

The *libcbor* API closely follows the semantics outlined by `CBOR standard <https://tools.ietf.org/html/rfc7049>`_. This part of the documentation provides a short overview of the CBOR constructs, as well as a general introduction to the *libcbor* API. Remaining reference can be found in the following files structured by data types.

The API is designed to allow both very tight control & flexibility and general convenience with sane defaults. [#]_ For example, client with very specific requirements (constrained environment, custom application protocol built on top of CBOR, etc.) may choose to take full control (and responsibility) of memory and data structures management by interacting directly with the decoder. Other clients might want to take control of specific aspects (streamed collections, hash maps storage), but leave other responsibilities to *libcbor*. More general clients might prefer to be abstracted away from all aforementioned details and only be presented complete data structures.


*libcbor* provides
 - stateless encoders and decoders
 - encoding and decoding *drivers*, routines that coordinate encoding and decoding of complex structures
 - data structures to represent and transform CBOR structures
 - routines for building and manipulating these structures
 - utilities for inspection and debugging

.. toctree::

   api/item_types
   api/item_reference_counting
   api/decoding
   api/encoding
   api/streaming_decoding
   api/streaming_encoding
   api/type_0_1
   api/type_2
   api/type_3
   api/type_4
   api/type_5
   api/type_6
   api/type_7

.. [#] http://softwareengineering.vazexqi.com/files/pattern.html
