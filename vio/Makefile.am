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

INCLUDES =		-I$(srcdir)/../include -I../include \
			@OPENSSL_INCLUDES@
LDADD =			libvio.la
			

pkglib_LTLIBRARIES =	libvio.la
noinst_PROGRAMS	=	
noinst_HEADERS =	
libvio_la_SOURCES =			\
			Vio.cc			VioAcceptorFd.cc	\
			VioConnectorFd.cc	VioFd.cc		\
			VioHandle.cc		VioSSL.cc		\
			VioSSLFactoriesFd.cc	VioSocket.cc		\
			auto.cc			hostnamexx.cc		\
			vdbug.cc		version.cc		\
			vmem.cc			violitexx.cc

OMIT_DEPENDENCIES =	pthread.h stdio.h __stdio.h stdlib.h __stdlib.h math.h\
			__math.h time.h __time.h unistd.h __unistd.h types.h \
			xtypes.h ac-types.h posix.h string.h __string.h \
			errno.h socket.h inet.h dirent.h netdb.h \
			cleanup.h cond.h debug_out.h fd.h kernel.h mutex.h \
			prio_queue.h pthread_attr.h pthread_once.h queue.h\
			sleep.h specific.h version.h pwd.h timers.h uio.h \
			cdefs.h machdep.h signal.h __signal.h util.h	\
			openssl/x509 openssl/ssl.h openssl/err.h \
			openssl/pem.h openssl/asn1.h

