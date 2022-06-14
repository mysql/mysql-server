Decoding
=============================

Another way to decode data using libcbor is to specify a callbacks that will be invoked when upon finding certain items in the input. This API is provided by

.. doxygenfunction:: cbor_stream_decode

Usage example: https://github.com/PJK/libcbor/blob/master/examples/streaming_parser.c

The callbacks are defined by

.. doxygenstruct:: cbor_callbacks
    :members:

When building custom sets of callbacks, feel free to start from

.. doxygenvariable:: cbor_empty_callbacks

Related structures
~~~~~~~~~~~~~~~~~~~~~

.. doxygenenum:: cbor_decoder_status
.. doxygenstruct:: cbor_decoder_result
    :members:


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
