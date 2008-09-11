# Copyright (C) 2000-2006 MySQL AB
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
			-I$(top_srcdir)/sql
LDADD =			@CLIENT_EXTRA_LDFLAGS@ ../mysys/libmysys.a \
			../dbug/libdbug.a ../strings/libmystrings.a \
			$(ZLIB_LIBS)
BUILT_SOURCES=		$(top_builddir)/include/mysqld_error.h \
                        $(top_builddir)/include/sql_state.h \
                        $(top_builddir)/include/mysqld_ername.h
pkginclude_HEADERS=	$(BUILT_SOURCES)
DISTCLEANFILES =	$(BUILT_SOURCES)
SUBDIRS =		@yassl_dir@
DIST_SUBDIRS =		yassl

# This will build mysqld_error.h, mysqld_ername.h and sql_state.h
# NOTE Built files should depend on their sources to avoid
# the built files being rebuilt in source dist
$(top_builddir)/include/mysqld_error.h: comp_err.c \
			$(top_srcdir)/sql/share/errmsg.txt
	$(MAKE) $(AM_MAKEFLAGS) comp_err$(EXEEXT)
	$(top_builddir)/extra/comp_err$(EXEEXT) \
	--charset=$(top_srcdir)/sql/share/charsets \
	--out-dir=$(top_builddir)/sql/share/ \
	--header_file=$(top_builddir)/include/mysqld_error.h \
	--name_file=$(top_builddir)/include/mysqld_ername.h \
	--state_file=$(top_builddir)/include/sql_state.h \
	--in_file=$(top_srcdir)/sql/share/errmsg.txt
$(top_builddir)/include/mysqld_ername.h: $(top_builddir)/include/mysqld_error.h
$(top_builddir)/include/sql_state.h: $(top_builddir)/include/mysqld_error.h

bin_PROGRAMS =		replace perror resolveip my_print_defaults \
			resolve_stack_dump mysql_waitpid 
# "innochecksum" should be switched
if BUILD_INNODB_TOOLS
bin_PROGRAMS += innochecksum
endif

noinst_PROGRAMS =	charset2html
EXTRA_PROGRAMS =	comp_err
EXTRA_DIST =		CMakeLists.txt

perror.o: perror.c 
	$(COMPILE) @ndbcluster_includes@ $(LM_CFLAGS) -c $<

# Don't update the files from bitkeeper
%::SCCS/s.%
