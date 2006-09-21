INCLUDES = -I../include -I../../mySTL
bin_PROGRAMS       = benchmark
benchmark_SOURCES  = benchmark.cpp
benchmark_LDADD    = $(top_builddir)/extra/yassl/taocrypt/src/libtaocrypt.la
benchmark_CXXFLAGS = -DYASSL_PURE_C
EXTRA_DIST = benchmark.dsp rsa1024.der dh1024.der dsa1024.der make.bat
