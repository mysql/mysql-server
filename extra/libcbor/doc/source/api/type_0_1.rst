Types 0 & 1 – Positive and negative integers
===============================================

*CBOR* has two types of integers – positive (which may be effectively regarded as unsigned), and negative. There are four possible widths for an integer – 1, 2, 4, or 8 bytes. These are represented by

.. doxygenenum:: cbor_int_width


Type 0 - positive integers
--------------------------
==================================  =========================================
Corresponding :type:`cbor_type`     ``CBOR_TYPE_UINT``
Number of allocations               One per lifetime
Storage requirements                ``sizeof(cbor_item_t) + sizeof(uint*_t)``
==================================  =========================================

**Note:** once a positive integer has been created, its width *cannot* be changed.

Type 1 - negative integers
--------------------------
==================================  =========================================
Corresponding :type:`cbor_type`     ``CBOR_TYPE_NEGINT``
Number of allocations               One per lifetime
Storage requirements                ``sizeof(cbor_item_t) + sizeof(uint*_t)``
==================================  =========================================

**Note:** once a positive integer has been created, its width *cannot* be changed.

Type 0 & 1
-------------
Due to their largely similar semantics, the following functions can be used for both Type 0 and Type 1 items. One can convert between them freely using `the conversion functions <#dealing-with-signedness>`_.

Actual Type of the integer can be checked using :doc:`item types API <item_types>`.



An integer item is created with one of the four widths. Because integers' `storage is bundled together with the handle </internal#c.cbor_item_t.data>`_, the width cannot be changed over its lifetime.

.. warning::

    Due to the fact that CBOR negative integers represent integers in the range :math:`[-1, -2^N]`, ``cbor_set_uint`` API is somewhat counter-intuitive as the resulting logical value is 1 less. This behavior is necessary in order to permit uniform manipulation with the full range of permitted values. For example, the following snippet

    .. code-block:: c

        cbor_item_t * item = cbor_new_int8();
        cbor_mark_negint(item);
        cbor_set_uint8(0);

    will produce an item with the logical value of :math:`-1`. There is, however, an upside to this as well: There is only one representation of zero.


Building new items
------------------------
.. doxygenfunction:: cbor_build_uint8
.. doxygenfunction:: cbor_build_uint16
.. doxygenfunction:: cbor_build_uint32
.. doxygenfunction:: cbor_build_uint64


Retrieving values
------------------------
.. doxygenfunction:: cbor_get_uint8
.. doxygenfunction:: cbor_get_uint16
.. doxygenfunction:: cbor_get_uint32
.. doxygenfunction:: cbor_get_uint64

Setting values
------------------------

.. doxygenfunction:: cbor_set_uint8
.. doxygenfunction:: cbor_set_uint16
.. doxygenfunction:: cbor_set_uint32
.. doxygenfunction:: cbor_set_uint64

Dealing with width
---------------------
.. doxygenfunction:: cbor_int_get_width

Dealing with signedness
--------------------------

.. doxygenfunction:: cbor_mark_uint
.. doxygenfunction:: cbor_mark_negint

Creating new items
------------------------

.. doxygenfunction:: cbor_new_int8
.. doxygenfunction:: cbor_new_int16
.. doxygenfunction:: cbor_new_int32
.. doxygenfunction:: cbor_new_int64
