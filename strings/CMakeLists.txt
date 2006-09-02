SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG -DSAFEMALLOC -DSAFE_MUTEX")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)
ADD_LIBRARY(strings bchange.c bcmp.c bfill.c bmove512.c bmove_upp.c ctype-big5.c ctype-bin.c ctype-cp932.c
                ctype-czech.c ctype-euc_kr.c ctype-eucjpms.c ctype-extra.c ctype-gb2312.c ctype-gbk.c
                ctype-latin1.c ctype-mb.c ctype-simple.c ctype-sjis.c ctype-tis620.c ctype-uca.c
                ctype-ucs2.c ctype-ujis.c ctype-utf8.c ctype-win1250ch.c ctype.c decimal.c int2str.c
                is_prefix.c llstr.c longlong2str.c my_strtoll10.c my_vsnprintf.c r_strinstr.c
                str2int.c str_alloc.c strcend.c strend.c strfill.c strmake.c strmov.c strnmov.c 
                strtod.c strtol.c strtoll.c strtoul.c strtoull.c strxmov.c strxnmov.c xml.c
		strcont.c strinstr.c)
