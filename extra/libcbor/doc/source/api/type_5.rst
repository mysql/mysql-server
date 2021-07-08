Type 5 – Maps
=============================

CBOR maps are the plain old associate hash maps known from JSON and many other formats and languages, with one exception: any CBOR data item can be a key, not just strings. This is somewhat unusual and you, as an application developer, should keep that in mind.

Maps can be either definite or indefinite, in much the same way as :doc:`type_4`.

==================================  =====================================================================================
Corresponding :type:`cbor_type`     ``CBOR_TYPE_MAP``
Number of allocations (definite)    Two plus any manipulations with the data
Number of allocations (indefinite)  Two plus logarithmically many
                                    reallocations relative to additions
Storage requirements (definite)     ``sizeof(cbor_pair) * size + sizeof(cbor_item_t)``
Storage requirements (indefinite)   ``<= sizeof(cbor_item_t) + sizeof(cbor_pair) * size * BUFFER_GROWTH``
==================================  =====================================================================================

Streaming maps
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Please refer to :doc:`/streaming`.

Getting metadata
~~~~~~~~~~~~~~~~~
.. doxygenfunction:: cbor_map_size
.. doxygenfunction:: cbor_map_allocated
.. doxygenfunction:: cbor_map_is_definite
.. doxygenfunction:: cbor_map_is_indefinite

Reading data
~~~~~~~~~~~~~

.. doxygenfunction:: cbor_map_handle

Creating new items
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_new_definite_map
.. doxygenfunction:: cbor_new_indefinite_map


Modifying items
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_map_add
