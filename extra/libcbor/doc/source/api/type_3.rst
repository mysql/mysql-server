Type 3 – UTF-8 strings 
=============================

CBOR strings work in much the same ways as :doc:`type_2`.

==================================  ======================================================
Corresponding :type:`cbor_type`     ``CBOR_TYPE_STRING``
Number of allocations (definite)    One plus any manipulations with the data
Number of allocations (indefinite)  One plus logarithmically many
                                    reallocations relative  to chunk count
Storage requirements (definite)     ``sizeof(cbor_item_t) + length(handle)``
Storage requirements (indefinite)   ``sizeof(cbor_item_t) * (1 + chunk_count) + chunks``
==================================  ======================================================

Streaming indefinite strings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Please refer to :doc:`/streaming`.

UTF-8 encoding validation
~~~~~~~~~~~~~~~~~~~~~~~~~~~
*libcbor* considers UTF-8 encoding validity to be a part of the well-formedness notion of CBOR and therefore invalid UTF-8 strings will be rejected by the parser. Strings created by the user are not checked.


Getting metadata
~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_string_length
.. doxygenfunction:: cbor_string_is_definite
.. doxygenfunction:: cbor_string_is_indefinite
.. doxygenfunction:: cbor_string_chunk_count

Reading data
~~~~~~~~~~~~~

.. doxygenfunction:: cbor_string_handle
.. doxygenfunction:: cbor_string_chunks_handle

Creating new items
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_new_definite_string
.. doxygenfunction:: cbor_new_indefinite_string


Building items
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. doxygenfunction:: cbor_build_string


Manipulating existing items
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_string_set_handle
.. doxygenfunction:: cbor_string_add_chunk
