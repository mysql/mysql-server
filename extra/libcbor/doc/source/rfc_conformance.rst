RFC conformance
=========================

*libcbor* is, generally speaking, very faithful implementation of `RFC 7049 <http://tools.ietf.org/html/rfc7049>`_. There are, however, some limitations imposed by technical constraints.

Bytestring length
-------------------
There is no explicit limitation of indefinite length byte strings. [#]_ *libcbor* will not handle byte strings with more chunks than the maximum value of :type:`size_t`. On any sane platform, such string would not fit in the memory anyway. It is, however, possible to process arbitrarily long strings and byte strings using the streaming decoder.

.. [#] http://tools.ietf.org/html/rfc7049#section-2.2.2

"Half-precision" IEEE 754 floats
---------------------------------
As of C99 and even C11, there is no standard implementation for 2 bytes floats. *libcbor* packs them as a `float <https://en.cppreference.com/w/c/language/type>`. When encoding, *libcbor* selects the appropriate wire representation based on metadata and the actual value. This applies both to canonical and normal mode.

For more information on half-float serialization, please refer to the section on :ref:`api_type_7_hard_floats`.

