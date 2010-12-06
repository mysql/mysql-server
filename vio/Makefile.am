# Copyright (C) 2000-2003, 2005, 2006 MySQL AB
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

INCLUDES =		-I$(top_builddir)/include -I$(top_srcdir)/include \
			$(openssl_includes)
LDADD =			@CLIENT_EXTRA_LDFLAGS@ $(openssl_libs) $(yassl_libs)
pkglib_LIBRARIES =	libvio.a
noinst_LTLIBRARIES =	libvio.la

noinst_HEADERS =	vio_priv.h

libvio_la_SOURCES =	vio.c viosocket.c viossl.c viosslfactories.c

EXTRA_DIST=		CMakeLists.txt

libvio_a_SOURCES=
libvio.a: libvio.la
	$(CP) .libs/libvio.a $@

