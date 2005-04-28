INCLUDES = -I../include -I../../mySTL

noinst_LIBRARIES = libtaocrypt.a
libtaocrypt_a_SOURCES  = aes.cpp aestables.cpp algebra.cpp arc4.cpp asn.cpp \
	coding.cpp dh.cpp des.cpp dsa.cpp file.cpp hash.cpp integer.cpp \
	md2.cpp md5.cpp misc.cpp random.cpp ripemd.cpp rsa.cpp sha.cpp
EXTRA_DIST = ../include/*.hpp
CXXFLAGS=`echo "@CXXFLAGS@" | sed 's/-fno-implicit-templates//'`
