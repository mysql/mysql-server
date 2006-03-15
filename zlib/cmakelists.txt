SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")

ADD_DEFINITIONS(-DUSE_TLS -DMYSQL_CLIENT -D__WIN32__)
ADD_LIBRARY(zlib adler32.c compress.c crc32.c crc32.h deflate.c deflate.h gzio.c infback.c inffast.c inffast.h
			inffixed.h inflate.c inflate.h inftrees.c inftrees.h trees.c trees.h uncompr.c zconf.h zlib.h
			zutil.c zutil.h)
			