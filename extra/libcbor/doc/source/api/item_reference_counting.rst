Memory management and reference counting
===============================================

Due to the nature of its domain, *libcbor* will need to work with heap memory. The stateless decoder and encoder don't allocate any memory.

If you have specific requirements, you should consider rolling your own driver for the stateless API.

Using custom allocator
^^^^^^^^^^^^^^^^^^^^^^^^

*libcbor* gives you with the ability to provide your own implementations of ``malloc``, ``realloc``, and ``free``. This can be useful if you are using a custom allocator throughout your application, or if you want to implement custom policies (e.g. tighter restrictions on the amount of allocated memory).

In order to use this feature, *libcbor* has to be compiled with the :doc:`appropriate flags </getting_started>`. You can verify the configuration using the ``CBOR_CUSTOM_ALLOC`` macro. A simple usage might be as follows:

.. code-block:: c

	#if CBOR_CUSTOM_ALLOC
		cbor_set_allocs(malloc, realloc, free);
	#else
	   #error "libcbor built with support for custom allocation is required"
	#endif

.. doxygenfunction:: cbor_set_allocs


Reference counting
^^^^^^^^^^^^^^^^^^^^^

As CBOR items may require complex cleanups at the end of their lifetime, there is a reference counting mechanism in place. This also enables very simple GC when integrating *libcbor* into managed environment. Every item starts its life (by either explicit creation, or as a result of parsing) with reference count set to 1. When the refcount reaches zero, it will be destroyed.

Items containing nested items will be destroyed recursively - refcount of every nested item will be decreased by one.

The destruction is synchronous and renders any pointers to items with refcount zero invalid immediately after calling the :func:`cbor_decref`.


.. doxygenfunction:: cbor_incref
.. doxygenfunction:: cbor_decref
.. doxygenfunction:: cbor_intermediate_decref
.. doxygenfunction:: cbor_refcount
.. doxygenfunction:: cbor_move
.. doxygenfunction:: cbor_copy
