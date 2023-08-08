Streaming Encoding
=============================

`cbor/encoding.h <https://github.com/PJK/libcbor/blob/master/src/cbor/encoding.h>`_
exposes a low-level encoding API to encode CBOR objects on the fly. Unlike
:func:`cbor_serialize`, these functions take logical values (integers, floats,
strings, etc.) instead of :type:`cbor_item_t`. The client is responsible for
constructing the compound types correctly (e.g. terminating arrays).

Streaming encoding is typically used to create an streaming (indefinite length) CBOR :doc:`strings <type_2>`, :doc:`byte strings <type_3>`, :doc:`arrays <type_4>`, and :doc:`maps <type_5>`. Complete example: `examples/streaming_array.c <https://github.com/PJK/libcbor/blob/master/examples/streaming_array.c>`_

.. doxygenfunction:: cbor_encode_uint8

.. doxygenfunction:: cbor_encode_uint16

.. doxygenfunction:: cbor_encode_uint32

.. doxygenfunction:: cbor_encode_uint64

.. doxygenfunction:: cbor_encode_uint

.. doxygenfunction:: cbor_encode_negint8

.. doxygenfunction:: cbor_encode_negint16

.. doxygenfunction:: cbor_encode_negint32

.. doxygenfunction:: cbor_encode_negint64

.. doxygenfunction:: cbor_encode_negint

.. doxygenfunction:: cbor_encode_bytestring_start

.. doxygenfunction:: cbor_encode_indef_bytestring_start

.. doxygenfunction:: cbor_encode_string_start

.. doxygenfunction:: cbor_encode_indef_string_start

.. doxygenfunction:: cbor_encode_array_start

.. doxygenfunction:: cbor_encode_indef_array_start

.. doxygenfunction:: cbor_encode_map_start

.. doxygenfunction:: cbor_encode_indef_map_start

.. doxygenfunction:: cbor_encode_tag

.. doxygenfunction:: cbor_encode_bool

.. doxygenfunction:: cbor_encode_null

.. doxygenfunction:: cbor_encode_undef

.. doxygenfunction:: cbor_encode_half

.. doxygenfunction:: cbor_encode_single

.. doxygenfunction:: cbor_encode_double

.. doxygenfunction:: cbor_encode_break

.. doxygenfunction:: cbor_encode_ctrl

