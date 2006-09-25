INCLUDES = -I../include -I../mySTL
bin_PROGRAMS = test
test_SOURCES = test.cpp
test_LDFLAGS  = -L../src
test_LDADD    = -ltaocrypt
test_DEPENDENCIES = ../src/libtaocrypt.la
test_CXXFLAGS = -DYASSL_PURE_C
EXTRA_DIST = make.bat
