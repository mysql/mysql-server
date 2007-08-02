# Copyright (C) 2006 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# Note that this library is not using any "Thread Local Storage" (TLS),
# i.e. no data declared "__declspec(thread)" or allocated with TlsAlloc().
# Not directly and indirectly using any of the macros for creating and
# using the storage, pthread_key*(), {,my_}{set,get}_specific*() ....

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")

ADD_LIBRARY(zlib adler32.c compress.c crc32.c crc32.h deflate.c deflate.h gzio.c infback.c inffast.c inffast.h
			inffixed.h inflate.c inflate.h inftrees.c inftrees.h trees.c trees.h uncompr.c zconf.h zlib.h
			zutil.c zutil.h)
			