Types of items
===============================================

Every :type:`cbor_item_t` has a :type:`cbor_type` associated with it - these constants correspond to the types specified by the `CBOR standard <http://tools.ietf.org/html/rfc7049>`_:

.. doxygenenum:: cbor_type

To find out the type of an item, one can use

.. doxygenfunction:: cbor_typeof

Please note the distinction between functions like :func:`cbor_isa_uint()` and :func:`cbor_is_int()`. The following functions work solely with the major type value.


Binary queries
------------------------

Alternatively, there are functions to query each particular type.

.. warning:: Passing an invalid :type:`cbor_item_t` reference to any of these functions results in undefined behavior.

.. doxygenfunction:: cbor_isa_uint
.. doxygenfunction:: cbor_isa_negint
.. doxygenfunction:: cbor_isa_bytestring
.. doxygenfunction:: cbor_isa_string
.. doxygenfunction:: cbor_isa_array
.. doxygenfunction:: cbor_isa_map
.. doxygenfunction:: cbor_isa_tag
.. doxygenfunction:: cbor_isa_float_ctrl


Logical queries
------------------------

These functions provide information about the item type from a more high-level perspective

.. doxygenfunction:: cbor_is_int
.. doxygenfunction:: cbor_is_float
.. doxygenfunction:: cbor_is_bool
.. doxygenfunction:: cbor_is_null
.. doxygenfunction:: cbor_is_undef
