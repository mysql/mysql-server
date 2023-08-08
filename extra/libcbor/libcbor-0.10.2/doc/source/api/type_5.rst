Type 5 – Maps
=============================

CBOR maps are the plain old associative maps similar JSON objects or Python dictionaries.

Definite maps have a fixed size which is stored in the header, whereas indefinite maps do not and are terminated by a special "break" byte instead.

Map are explicitly created or decoded as definite or indefinite and will be encoded using the corresponding wire representation, regardless of whether the actual size is known at the time of encoding.

.. note::

  Indefinite maps can be conveniently used with streaming :doc:`decoding <streaming_decoding>` and :doc:`encoding <streaming_encoding>`.
  Keys and values can simply be output one by one, alternating keys and values.

.. warning:: Any CBOR data item is a legal map key (not just strings).

==================================  =====================================================================================
Corresponding :type:`cbor_type`     ``CBOR_TYPE_MAP``
Number of allocations (definite)    Two plus any manipulations with the data
Number of allocations (indefinite)  Two plus logarithmically many
                                    reallocations relative to additions
Storage requirements (definite)     ``sizeof(cbor_pair) * size + sizeof(cbor_item_t)``
Storage requirements (indefinite)   ``<= sizeof(cbor_item_t) + sizeof(cbor_pair) * size * BUFFER_GROWTH``
==================================  =====================================================================================

Examples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    0xbf        Start indefinite map (represents {1: 2})
        0x01        Unsigned integer 1 (key)
        0x02        Unsigned integer 2 (value)
        0xff        "Break" control token

::

    0xa0        Map of size 0

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
