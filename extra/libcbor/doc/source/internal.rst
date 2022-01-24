Internal mechanics
==========================

Internal workings of *libcbor* are mostly derived from the specification. The purpose of this document is to describe technical choices made during design & implementation and to explicate the reasoning behind those choices.

Terminology
---------------
===  ======================  ========================================================================================================================================
MTB  Major Type Byte         http://tools.ietf.org/html/rfc7049#section-2.1
---  ----------------------  ----------------------------------------------------------------------------------------------------------------------------------------
DST  Dynamically Sized Type  Type whose storage requirements cannot be determined

                             during compilation (originated in the `Rust <http://www.rust-lang.org/>`_ community)
===  ======================  ========================================================================================================================================

Conventions
--------------
API symbols start with ``cbor_`` or ``CBOR_`` prefix, internal symbols have ``_cbor_`` or ``_CBOR_`` prefix.

Inspiration & related projects
-------------------------------
Most of the API is largely modelled after existing JSON libraries, including

 - `Jansson <http://www.digip.org/jansson/>`_
 - `json-c <https://github.com/json-c/json-c>`_
 - Gnome's `JsonGlib <https://wiki.gnome.org/action/show/Projects/JsonGlib?action=show&redirect=JsonGlib>`_

and also borrowing from

 - `msgpack-c <https://github.com/msgpack/msgpack-c>`_
 - `Google Protocol Buffers <http://code.google.com/p/protobuf/>`_.

General notes on the API design
--------------------------------
The API design has two main driving priciples:

 1. Let the client manage the memory as much as possible
 2. Behave exactly as specified by the standard

Combining these two principles in practice turns out to be quite difficult. Indefinite-length strings, arrays, and maps require client to handle every fixed-size chunk explicitly in order to

 - ensure the client never runs out of memory due to *libcbor*
 - use :func:`realloc` sparsely and predictably [#]_

    - provide strong guarantees about its usage (to prevent latency spikes)
    - provide APIs to avoid :func:`realloc` altogether
 - allow proper handling of (streamed) data bigger than available memory

 .. [#] Reasonable handling of DSTs requires reallocation if the API is to remain sane.


Coding style
-------------
This code loosely follows the `Linux kernel coding style <https://www.kernel.org/doc/Documentation/CodingStyle>`_. Tabs are tabs, and they are 4 characters wide.


Memory layout
---------------
CBOR is very dynamic in the sense that it contains many data elements of variable length, sometimes even indefinite length. This section describes internal representation of all CBOR data types.

Generally speaking, data items consist of three parts:

 - a generic :type:`handle <cbor_item_t>`,
 - the associated :type:`metadata <cbor_item_metadata>`,
 - and the actual data

.. type:: cbor_item_t

    Represents the item. Used as an opaque type

    .. member:: cbor_type type

        Type discriminator

    .. member:: size_t refcount

        Reference counter. Used by :func:`cbor_decref`, :func:`cbor_incref`

    .. member:: union cbor_item_metadata metadata

        Union discriminated by :member:`type`. Contains type-specific metadata

    .. member:: unsigned char * data

        Contains pointer to the actual data. Small, fixed size items (:doc:`api/type_0_1`, :doc:`api/type_6`, :doc:`api/type_7`) are allocated as a single memory block.

        Consider the following snippet

        .. code-block:: c

            cbor_item_t * item = cbor_new_int8();

        then the memory is laid out as follows

        ::

            +-----------+---------------+---------------+-----------------------------------++-----------+
            |           |               |               |                                   ||           |
            |   type    |   refcount    |   metadata    |              data                 ||  uint8_t  |
            |           |               |               |   (= item + sizeof(cbor_item_t))  ||           |
            +-----------+---------------+---------------+-----------------------------------++-----------+
            ^                                                                                ^
            |                                                                                |
            +--- item                                                                        +--- item->data

        Dynamically sized types (:doc:`api/type_2`, :doc:`api/type_3`, :doc:`api/type_4`, :doc:`api/type_5`) may store handle and data in separate locations. This enables creating large items (e.g :doc:`byte strings <api/type_2>`) without :func:`realloc` or copying large blocks of memory. One simply attaches the correct pointer to the handle.


.. type:: cbor_item_metadata

    Union type of the following members, based on the item type:

    .. member:: struct _cbor_int_metadata int_metadata

        Used both by both :doc:`api/type_0_1`

    .. member:: struct _cbor_bytestring_metadata bytestring_metadata
    .. member:: struct _cbor_string_metadata string_metadata
    .. member:: struct _cbor_array_metadata array_metadata
    .. member:: struct _cbor_map_metadata map_metadata
    .. member:: struct _cbor_tag_metadata tag_metadata
    .. member:: struct _cbor_float_ctrl_metadata float_ctrl_metadata

Decoding
---------

As outlined in :doc:`api`, there decoding is based on the streaming decoder Essentially, the decoder is a custom set of callbacks for the streaming decoder.

