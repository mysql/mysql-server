Encoding
=============================

The easiest way to encode data items is using the :func:`cbor_serialize` or :func:`cbor_serialize_alloc` functions:

.. doxygenfunction:: cbor_serialize
.. doxygenfunction:: cbor_serialize_alloc

To determine the number of bytes needed to serialize an item, use :func:`cbor_serialized_size`:

.. doxygenfunction:: cbor_serialized_size

Type-specific serializers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In case you know the type of the item you want to serialize beforehand, you can use one
of the type-specific serializers.

.. note:: Unless compiled in debug mode, these do not verify the type. Passing an incorrect item will result in an undefined behavior.

.. doxygenfunction:: cbor_serialize_uint
.. doxygenfunction:: cbor_serialize_negint
.. doxygenfunction:: cbor_serialize_bytestring
.. doxygenfunction:: cbor_serialize_string
.. doxygenfunction:: cbor_serialize_array
.. doxygenfunction:: cbor_serialize_map
.. doxygenfunction:: cbor_serialize_tag
.. doxygenfunction:: cbor_serialize_float_ctrl
