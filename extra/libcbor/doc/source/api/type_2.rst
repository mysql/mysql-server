Type 2 – Byte strings
=============================

CBOR byte strings are just (ordered) series of bytes without further interpretation (unless there is a :doc:`tag <type_6>`). Byte string's length may or may not be known during encoding. These two kinds of byte strings can be distinguished using :func:`cbor_bytestring_is_definite` and :func:`cbor_bytestring_is_indefinite` respectively.

In case a byte string is indefinite, it is encoded as a series of definite byte strings. These are called "chunks". For example, the encoded item

::

    0xf5	    Start indefinite byte string
	0x41	    Byte string (1B long)
	    0x00
	0x41	    Byte string (1B long)
	    0xff
	0xff	    "Break" control token

represents two bytes, ``0x00`` and ``0xff``. This on one hand enables streaming messages even before they are fully generated, but on the other hand it adds more complexity to the client code.


==================================  ======================================================
Corresponding :type:`cbor_type`     ``CBOR_TYPE_BYTESTRING``
Number of allocations (definite)    One plus any manipulations with the data
Number of allocations (indefinite)  One plus logarithmically many
                                    reallocations relative  to chunk count
Storage requirements (definite)     ``sizeof(cbor_item_t) + length(handle)``
Storage requirements (indefinite)   ``sizeof(cbor_item_t) * (1 + chunk_count) + chunks``
==================================  ======================================================


Streaming indefinite byte strings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Please refer to :doc:`/streaming`.

Getting metadata
~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_bytestring_length
.. doxygenfunction:: cbor_bytestring_is_definite
.. doxygenfunction:: cbor_bytestring_is_indefinite
.. doxygenfunction:: cbor_bytestring_chunk_count

Reading data
~~~~~~~~~~~~~

.. doxygenfunction:: cbor_bytestring_handle
.. doxygenfunction:: cbor_bytestring_chunks_handle

Creating new items
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_new_definite_bytestring
.. doxygenfunction:: cbor_new_indefinite_bytestring


Building items
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. doxygenfunction:: cbor_build_bytestring


Manipulating existing items
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_bytestring_set_handle
.. doxygenfunction:: cbor_bytestring_add_chunk

