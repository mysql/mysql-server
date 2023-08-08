Getting started
==========================

Pre-built Linux packages are available in most mainstream distributions

**Ubuntu, Debian, etc.**:

.. code-block:: bash

    apt-get install libcbor-dev

**Fedora, openSUSE, etc.**:

.. code-block:: bash

    yum install libcbor-devel


**OS X** users can use `Homebrew <http://brew.sh/>`_:

.. code-block:: bash

    brew install libcbor

For other platforms, you will need to compile it from source.

Building & installing libcbor
------------------------------

Prerequisites:
 - C99 compiler
 - CMake_ 2.8 or newer (might also be called ``cmakesetup``, ``cmake-gui`` or ``ccmake`` depending on the installed version and system)
 - C build system CMake can target (make, Apple Xcode, MinGW, ...)

.. _CMake: http://cmake.org/

**Configuration options**

A handful of configuration flags can be passed to `cmake`. The following table lists libcbor compile-time directives and several important generic flags.

========================  =======================================================   ======================  =====================================================================================================================
Option                    Meaning                                                   Default                 Possible values
------------------------  -------------------------------------------------------   ----------------------  ---------------------------------------------------------------------------------------------------------------------
``CMAKE_C_COMPILER``      C compiler to use                                         ``cc``                   ``gcc``, ``clang``, ``clang-3.5``, ...
``CMAKE_INSTALL_PREFIX``  Installation prefix                                       System-dependent         ``/usr/local/lib``, ...
``BUILD_SHARED_LIBS``     Build as a shared library                                 ``OFF``                  ``ON``, ``OFF``
``HUGE_FUZZ``             :doc:`Fuzz test </tests>` with 8GB of data                ``OFF``                  ``ON``, ``OFF``
``SANE_MALLOC``           Assume ``malloc`` will refuse unreasonable allocations    ``OFF``                  ``ON``, ``OFF``
``COVERAGE``              Generate test coverage instrumentation                    ``OFF``                  ``ON``, ``OFF``
``WITH_TESTS``            Build unit tests (see :doc:`development`)                 ``OFF``                  ``ON``, ``OFF``
========================  =======================================================   ======================  =====================================================================================================================

The following configuration options will also be defined as macros [#]_ in ``<cbor/common.h>`` and can therefore be used in client code:

========================  =======================================================   ======================  =====================================================================================================================
Option                    Meaning                                                   Default                 Possible values
------------------------  -------------------------------------------------------   ----------------------  ---------------------------------------------------------------------------------------------------------------------
``CBOR_PRETTY_PRINTER``   Include a pretty-printing routine                         ``ON``                  ``ON``, ``OFF``
``CBOR_BUFFER_GROWTH``    Factor for buffer growth & shrinking                       ``2``                    Decimals > 1
========================  =======================================================   ======================  =====================================================================================================================

.. [#] ``ON`` & ``OFF`` will be translated to ``1`` and ``0`` using `cmakedefine <https://cmake.org/cmake/help/v3.2/command/configure_file.html?highlight=cmakedefine>`_.

If you want to pass other custom configuration options, please refer to `<http://www.cmake.org/Wiki/CMake_Useful_Variables>`_.

.. warning::
    ``CBOR_CUSTOM_ALLOC`` has been `removed <https://github.com/PJK/libcbor/pull/237>`_. 
    Custom allocators (historically a controlled by a build flag) are always enabled.

**Building using make**

CMake will generate a Makefile and other configuration files for the build. As a rule of thumb, you should configure the
build *outside of the source tree* in order to keep different configurations isolated. If you are unsure where to
execute the build, just use a temporary directory:

.. code-block:: bash

  cd $(mktemp -d /tmp/cbor_build.XXXX)

Now, assuming you are in the directory where you want to build, build libcbor as a **static library**:

.. code-block:: bash

  cmake -DCMAKE_BUILD_TYPE=Release path_to_libcbor_dir
  make cbor

... or as a **dynamic library**:

.. code-block:: bash

  cmake -DCMAKE_BUILD_TYPE=Release  -DBUILD_SHARED_LIBS=ON path_to_libcbor_dir
  make cbor

To install locally:

.. code-block:: bash

  make install

Root permissions are required on most systems when using the default installation prefix.


**Portability**

libcbor is highly portable and works on both little- and big-endian systems regardless of the operating system. After building
on an exotic platform, you might wish to verify the result by running the :doc:`test suite </tests>`. If you encounter any problems, please
report them to the `issue tracker <https://github.com/PJK/libcbor/issues>`_.

libcbor is known to successfully work on ARM Android devices. Cross-compilation is possible with ``arm-linux-gnueabi-gcc``.


Linking with libcbor
---------------------

If you include and linker paths include the directories to which libcbor has been installed, compiling programs that uses libcbor requires
no extra considerations.

You can verify that everything has been set up properly by creating a file with the following contents

.. code-block:: c

    #include <cbor.h>
    #include <stdio.h>

    int main(int argc, char * argv[])
    {
        printf("Hello from libcbor %s\n", CBOR_VERSION);
    }


and compiling it

.. code-block:: bash

    cc hello_cbor.c -lcbor -o hello_cbor


libcbor also comes with `pkg-config <https://wiki.freedesktop.org/www/Software/pkg-config/>`_ support. If you install libcbor with a custom prefix, you can use pkg-config to resolve the headers and objects:

.. code-block:: bash

    cc $(pkg-config --cflags libcbor) hello_cbor.c $(pkg-config --libs libcbor) -o hello_cbor


**A note on linkage**

libcbor is primarily intended to be linked statically. The shared library versioning scheme generally follows `SemVer <https://semver.org/>`_, but is irregular for the 0.X.Y development branch for historical reasons. The following version identifiers are used as a part of the SONAME (Linux) or the dylib `"Compatibility version" <https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/CreatingDynamicLibraries.html>`_ (OS X):

  - 0.Y for the 0.Y.Z branch. Patches are backwards compatible, minor releases are generally not and require re-compilation of any dependent code.
  - X for the X.Y.Z stable versions starting 1.X.Y. All minor release of the major version are backwards compatible.

.. warning:: Please note that releases up to and including v0.6.0 `may export misleading .so/.dylib version number <https://github.com/PJK/libcbor/issues/52>`_.


Troubleshooting
---------------------

**cbor.h not found**: The headers directory is probably not in your include path. First, verify the installation
location by checking the installation log. If you used make, it will look something like

.. code-block:: text

    ...
    -- Installing: /usr/local/include/cbor
    -- Installing: /usr/local/include/cbor/callbacks.h
    -- Installing: /usr/local/include/cbor/encoding.h
    ...

Make sure that ``CMAKE_INSTALL_PREFIX`` (if you provided it) was correct. Including the path path during compilation should suffice, e.g.:

.. code-block:: bash

    cc -I/usr/local/include hello_cbor.c -lcbor -o hello_cbor


**cannot find -lcbor during linking**: Most likely the same problem as before. Include the installation directory in the
linker shared path using ``-R``, e.g.:

.. code-block:: bash

    cc -Wl,-rpath,/usr/local/lib -lcbor -o hello_cbor

**shared library missing during execution**: Verify the linkage using ``ldd``, ``otool``, or similar and adjust the compilation directives accordingly:

.. code-block:: text

    ⇒  ldd hello_cbor
        linux-vdso.so.1 =>  (0x00007ffe85585000)
        libcbor.so => /usr/local/lib/libcbor.so (0x00007f9af69da000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f9af65eb000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f9af6be9000)

**compilation failed**: If your compiler supports C99 yet the compilation has failed, please report the issue to the `issue tracker <https://github.com/PJK/libcbor/issues>`_.
