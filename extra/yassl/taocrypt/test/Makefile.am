INCLUDES = -I../include -I../mySTL
bin_PROGRAMS = test
test_SOURCES = test.cpp
test_LDADD    = $(top_builddir)/extra/yassl/taocrypt/src/libtaocrypt.la
test_CXXFLAGS = -DYASSL_PURE_C
EXTRA_DIST = make.bat
