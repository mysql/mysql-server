Development
==========================

Vision and principles
---------------------------

Consistency and coherence are one of the key characteristics of good software.
While the reality is never black and white, it is important libcbor
contributors are working towards the same high-level goal. This document
attempts to set out the basic principles of libcbor and the rationale behind
them. If you are contributing to libcbor or looking to evaluate whether libcbor
is the right choice for your project, it might be worthwhile to skim through the
section below.

Mission statement
~~~~~~~~~~~~~~~~~~~~~~

*libcbor* is the compact, full-featured, and safe CBOR library that works
everywhere.


Goals
~~~~~~~~~~~~~~~~~~~~~~

RFC-conformance and full feature support
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Anything the standard allows, libcbor can do.

**Why?** Because conformance and interoperability is the point of defining
standards. Clients expect the support to be feature-complete and
there is no significant complexity reduction that can be achieved by slightly
cutting corners, which means that the incremental cost of full RFC support is
comparatively small over "almost-conformance" seen in many alternatives.


Safety
^^^^^^^^^^^^^^^^^^^^^^

Untrusted bytes from the network are the typical input.

**Why?** Because it is the client expectation. Vast majority of security
vulnerabilities are violations of contracts -- in other words, bugs -- anyway.


Self-containment
^^^^^^^^^^^^^^^^^^^^^^

libcbor has no runtime dependencies.

**Why?** Because any constraint imposed on libcbor has to be enforced
transitively, which is difficult and leads to incompatibilities and
distribution issues, especially in IoT applications.

Portability
^^^^^^^^^^^^^^^^^^^^^^

If you can compile C for it, libcbor will work there.

**Why?** Lowest-common-denominator solution for system-level and IoT software
was the original niche of libcbor. Users who rely on libcbor expect future
updates to work on their target platform.

Stable and predictable API
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

libcbor will not break without a warning.

**Why?** `Industry-standard <https://semver.org/>`_ versioning is a basic
requirement for production-quality software. This is especially relevant in IoT
environments where updates may be costly.

Performance
^^^^^^^^^^^^^^^^^^^^^^

libcbor is fast and resource-efficient by design


**Why?** Because the main maintainer is an avid hater of slow bloated software.
Who wouldn't want more bang per their electricity buck?


Non-goals
~~~~~~~~~~~~~~~~~~~~~~

 - Convenience -- libcbor only provides the minimum surface to make it usable
 - FFI/SWIG/interop support -- libcbor is primarily a C library for C clients
 - One-off usecases support -- although there are primitives to reuse, the
   basic
   assumption is that most clients want most of CBOR features


Development dependencies
---------------------------
- `CMocka <http://cmocka.org/>`_ (testing)
- `Python <https://www.python.org/>`_ and `pip <https://pypi.python.org/pypi/pip>`_ (Sphinx platform)
- `Doxygen <http://www.stack.nl/~dimitri/doxygen/>`_
- `Sphinx <http://sphinx-doc.org/>`_ (documentation)
- There are some `Ruby <https://www.ruby-lang.org/en/>`_ scripts in ``misc``
- `Valgrind <http://valgrind.org/>`_ (memory correctness & profiling)
- `GCOV/LCOV <http://ltp.sourceforge.net/coverage/lcov.php>`_ (test coverage)
- `clang-format`


Installing *sphinx*
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

  pip install sphinx
  pip install sphinx_rtd_theme
  pip install breathe
  pip install https://github.com/lepture/python-livereload/archive/master.zip
  pip install sphinx-autobuild

Further instructions on configuring advanced features can be found at `<http://read-the-docs.readthedocs.org/en/latest/install.html>`_.


Live preview of docs
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

  cd doc
  make livehtml


Set up git hooks
~~~~~~~~~~~~~~~~~

A catch-all git hook that runs clang-format and automatically refreshes the `GH
pages <https://pages.github.com/>`_  contents located in ``docs`` can be
symlinked:

.. code-block:: bash

  ln -sf $(pwd)/misc/hooks/pre-commit .git/hooks


Testing and code coverage
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Please refer to :doc:`tests`
