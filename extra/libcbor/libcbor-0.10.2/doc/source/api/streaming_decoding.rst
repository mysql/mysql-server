Streaming Decoding
=============================

*libcbor* exposes a stateless decoder that reads a stream of input bytes from a buffer and invokes user-provided callbacks as it decodes the input:

.. doxygenfunction:: cbor_stream_decode

For example, when :func:`cbor_stream_decode` encounters a 1B unsigned integer, it will invoke the function pointer stored in ``cbor_callbacks.uint8``.
Complete usage example: `examples/streaming_parser.c <https://github.com/PJK/libcbor/blob/master/examples/streaming_parser.c>`_

The callbacks are defined by

.. doxygenstruct:: cbor_callbacks
    :members:

When building custom sets of callbacks, feel free to start from

.. doxygenvariable:: cbor_empty_callbacks


Callback types definition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. doxygentypedef:: cbor_int8_callback
.. doxygentypedef:: cbor_int16_callback
.. doxygentypedef:: cbor_int32_callback
.. doxygentypedef:: cbor_int64_callback
.. doxygentypedef:: cbor_simple_callback
.. doxygentypedef:: cbor_string_callback
.. doxygentypedef:: cbor_collection_callback
.. doxygentypedef:: cbor_float_callback
.. doxygentypedef:: cbor_double_callback
.. doxygentypedef:: cbor_bool_callback
