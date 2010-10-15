INCLUDES = -I$(srcdir)/../include -I$(srcdir)/../mySTL
noinst_PROGRAMS    = benchmark
benchmark_SOURCES  = benchmark.cpp
benchmark_LDADD    = $(top_builddir)/extra/yassl/taocrypt/src/libtaocrypt.la
benchmark_CXXFLAGS = -DYASSL_PURE_C @yassl_thread_cxxflags@
EXTRA_DIST = benchmark.dsp rsa1024.der dh1024.der dsa1024.der make.bat

# Don't update the files from bitkeeper
%::SCCS/s.%
