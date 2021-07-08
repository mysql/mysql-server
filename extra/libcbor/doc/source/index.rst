libcbor
===================================

Documentation for version |release|, updated on |today|.

Overview
--------
*libcbor* is a C library for parsing and generating CBOR_, the general-purpose schema-less binary data format.


Main features
 - Complete RFC conformance [#]_
 - Robust C99 implementation
 - Layered architecture offers both control and convenience
 - Flexible memory management
 - No shared global state - threading friendly [#]_
 - Proper handling of UTF-8
 - Full support for streams & incremental processing
 - Extensive documentation and test suite
 - No runtime dependencies, small footprint

.. [#] See :doc:`rfc_conformance`

.. [#] With the exception of custom memory allocators (see :doc:`api/item_reference_counting`)

Contents
----------
.. toctree::

   getting_started
   using
   api
   streaming
   tests
   rfc_conformance
   internal
   changelog
   development

.. _CBOR: http://tools.ietf.org/html/rfc7049
