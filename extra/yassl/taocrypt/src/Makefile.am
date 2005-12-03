INCLUDES = -I../include -I../../mySTL

noinst_LTLIBRARIES = libtaocrypt.la

libtaocrypt_la_SOURCES  = aes.cpp aestables.cpp algebra.cpp arc4.cpp asn.cpp \
	coding.cpp dh.cpp des.cpp dsa.cpp file.cpp hash.cpp \
	md2.cpp md5.cpp misc.cpp random.cpp ripemd.cpp rsa.cpp sha.cpp \
	template_instnt.cpp integer.cpp
libtaocrypt_la_CXXFLAGS = @yassl_taocrypt_extra_cxxflags@ -DYASSL_PURE_C

EXTRA_DIST = $(wildcard ../include/*.hpp)
