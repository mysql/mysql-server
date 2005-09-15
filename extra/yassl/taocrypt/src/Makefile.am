INCLUDES = -I../include -I../../mySTL

noinst_LTLIBRARIES = libtaocrypt.la libtaoint.la
libtaocrypt_la_SOURCES  = aes.cpp aestables.cpp algebra.cpp arc4.cpp asn.cpp \
	coding.cpp dh.cpp des.cpp dsa.cpp file.cpp hash.cpp \
	md2.cpp md5.cpp misc.cpp random.cpp ripemd.cpp rsa.cpp sha.cpp \
	template_instnt.cpp
libtaocrypt_la_LIBADD = libtaoint.la
libtaoint_la_SOURCES = integer.cpp
libtaoint_la_CXXFLAGS = @yassl_integer_extra_cxxflags@
EXTRA_DIST = ../include/*.hpp
AM_CXXFLAGS = -DYASSL_PURE_C
