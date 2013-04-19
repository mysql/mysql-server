TokuKV
======

TokuKV is a high-performance, transactional key-value store, used in the
TokuDB storage engine for MySQL and MariaDB.

TokuKV is provided as a shared library with an interface similar to
Berkeley DB.

To build the full MySQL product, see the instructions for
[ft-engine][ft-engine].  This document covers TokuKV only.

[ft-engine]: http://github.com/Tokutek/ft-engine


Building
--------

TokuKV is built using CMake >= 2.8.8.  Out-of-source builds are
recommended.  You need a C++11 compiler, though only GCC >= 4.7 and
Apple's Clang are tested.  You also need zlib and valgrind development
packages (`yum install valgrind-devel zlib-devel` or `apt-get install
valgrind zlib1g-dev`).

You will also need the source code for jemalloc, checked out in
`third_party/`.

```sh
git clone git://github.com/Tokutek/ft-index.git ft-index
cd ft-index
git clone git://github.com/Tokutek/jemalloc.git third_party/jemalloc
mkdir build
cd build
CC=gcc47 CXX=g++47 cmake \
    -D CMAKE_BUILD_TYPE=Debug \
    -D USE_BDB=OFF \
    -D BUILD_TESTING=OFF \
    -D CMAKE_INSTALL_PREFIX=../prefix/ \
    ..
cmake --build . --target install
```

This will build `libtokudb.so` and `libtokuportability.so` and install it,
some header files, and some examples to `ft-index/prefix/`.  It will also
build jemalloc and install it alongside these libraries, you should link
to that if you are planning to run benchmarks or in production.

### Platforms

TokuKV is supported on 64-bit Centos, should work on other 64-bit linux
distributions, and may work on OSX 10.8 and FreeBSD.

[Transparent hugepages][transparent-hugepages] is a feature in newer linux
kernel versions that causes problems for the memory usage tracking
calculations in TokuKV and can lead to memory overcommit.  If you have
this feature enabled, TokuKV will not start, and you should turn it off.
If you want to run with transparent hugepages on, you can set an
environment variable `TOKU_HUGE_PAGES_OK=1`, but only do this for testing,
and only with a small cache size.

[transparent-hugepages]: https://access.redhat.com/site/documentation/en-US/Red_Hat_Enterprise_Linux/6/html/Performance_Tuning_Guide/s-memory-transhuge.html


Examples
--------

There are some sample programs that can use either TokuKV or Berkeley DB
in the `examples/` directory.  Follow the above instructions to build and
install TokuKV, and then look in the installed `examples/` directory for
instructions on building and running them.


Testing
-------

TokuKV uses CTest for testing.  The CDash testing dashboard is not
currently public, but you can run the tests without submitting them.

There are some large data files not stored in the git repository, that
will be made available soon.  For now, the tests that use these files will
not run.

Many of the tests are linked with both TokuKV and Berkeley DB, as a sanity
check on the tests themselves.  To build these tests, you will need
Berkeley DB and its header files installed.  If you do not have Berkeley
DB installed, just don't pass `USE_BDB=ON`.

In the build directory from above:

```sh
cmake -D BUILD_TESTING=ON [-D USE_BDB=ON] ..
ctest -D ExperimentalStart \
      -D ExperimentalConfigure \
      -D ExperimentalBuild \
      -D ExperimentalTest
```


Contributing
------------

Please report bugs in TokuKV here on github.

We have two publicly accessible mailing lists:

 - tokudb-user@googlegroups.com is for general and support related
   questions about the use of TokuDB.
 - tokudb-dev@googlegroups.com is for discussion of the development of
   TokuDB.

We are also available on IRC on freenode.net, in the #tokutek channel.


License
-------

TokuKV is available under the GPL version 2, with slight modifications.
See [README-TOKUDB][license].

[license]: http://github.com/Tokutek/ft-index/blob/master/README-TOKUDB
