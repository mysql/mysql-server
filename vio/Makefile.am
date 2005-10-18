# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

if HAVE_YASSL
  yassl_dummy_link_fix= $(top_srcdir)/extra/yassl/src/dummy.cpp
else
  yassl_dummy_link_fix=
endif
INCLUDES=		-I$(top_builddir)/include -I$(top_srcdir)/include \
			$(openssl_includes) $(yassl_includes)
LDADD=			@CLIENT_EXTRA_LDFLAGS@ $(openssl_libs) $(yassl_libs) 
pkglib_LIBRARIES=	libvio.a
noinst_PROGRAMS	=	test-ssl test-sslserver test-sslclient
noinst_HEADERS=	vio_priv.h
test_ssl_SOURCES=	test-ssl.c $(yassl_dummy_link_fix)
test_ssl_LDADD=   	@CLIENT_EXTRA_LDFLAGS@ ../dbug/libdbug.a libvio.a \
			../mysys/libmysys.a ../strings/libmystrings.a \
			$(openssl_libs) $(yassl_libs)
test_sslserver_SOURCES= test-sslserver.c $(yassl_dummy_link_fix)
test_sslserver_LDADD=   @CLIENT_EXTRA_LDFLAGS@	../dbug/libdbug.a libvio.a \
			../mysys/libmysys.a ../strings/libmystrings.a \
			$(openssl_libs) $(yassl_libs)
test_sslclient_SOURCES= test-sslclient.c $(yassl_dummy_link_fix)
test_sslclient_LDADD=   @CLIENT_EXTRA_LDFLAGS@	../dbug/libdbug.a libvio.a \
			../mysys/libmysys.a ../strings/libmystrings.a \
			$(openssl_libs) $(yassl_libs)
libvio_a_SOURCES=	vio.c viosocket.c viossl.c viosslfactories.c

# Don't update the files from bitkeeper
%::SCCS/s.%
