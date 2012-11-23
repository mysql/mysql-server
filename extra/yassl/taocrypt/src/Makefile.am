INCLUDES = -I$(srcdir)/../include -I$(srcdir)/../mySTL

noinst_LTLIBRARIES = libtaocrypt.la

libtaocrypt_la_SOURCES  = aes.cpp aestables.cpp algebra.cpp arc4.cpp \
        asn.cpp bftables.cpp blowfish.cpp coding.cpp des.cpp dh.cpp \
        dsa.cpp file.cpp hash.cpp integer.cpp md2.cpp md4.cpp md5.cpp misc.cpp \
        random.cpp ripemd.cpp rsa.cpp sha.cpp template_instnt.cpp \
        tftables.cpp twofish.cpp rabbit.cpp hc128.cpp

libtaocrypt_la_CXXFLAGS = @yassl_taocrypt_extra_cxxflags@ -DYASSL_PURE_C \
			  @yassl_thread_cxxflags@ \
			  @SYMBOL_VISIBILITY@ @SYMBOL_VISIBILITY_INLINES@
 
EXTRA_DIST = $(wildcard ../include/*.hpp)
