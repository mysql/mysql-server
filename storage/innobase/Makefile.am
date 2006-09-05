# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
# & Innobase Oy
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

# Process this file with automake to create Makefile.in

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/storage/innobase/include \
			-I$(top_srcdir)/sql \
                        -I$(srcdir)

AUTOMAKE_OPTIONS =	foreign
TAR =			gtar

noinst_HEADERS = 

SUBDIRS =		os ut btr buf data dict dyn eval fil fsp fut \
			ha ibuf lock log mach mem mtr page \
			pars que read rem row srv sync thr trx usr

EXTRA_DIST = 	include/btr0btr.h include/btr0btr.ic include/btr0cur.h include/btr0cur.ic \
		include/btr0pcur.h include/btr0pcur.ic include/btr0sea.h include/btr0sea.ic \
		include/btr0types.h \
		include/buf0buf.h include/buf0buf.ic include/buf0flu.h include/buf0flu.ic \
		include/buf0lru.h include/buf0lru.ic include/buf0rea.h include/buf0types.h \
		include/data0data.h include/data0data.ic include/data0type.h include/data0type.ic \
		include/data0types.h include/db0err.h \
		include/dict0boot.h include/dict0boot.ic include/dict0crea.h include/dict0crea.ic \
		include/dict0dict.h include/dict0dict.ic include/dict0load.h include/dict0load.ic \
		include/dict0mem.h include/dict0mem.ic include/dict0types.h \
		include/dyn0dyn.h include/dyn0dyn.ic \
		include/eval0eval.h include/eval0eval.ic include/eval0proc.h include/eval0proc.ic \
		include/fil0fil.h include/fsp0fsp.h include/fsp0fsp.ic \
		include/fut0fut.h include/fut0fut.ic include/fut0lst.h include/fut0lst.ic \
		include/ha0ha.h include/ha0ha.ic include/hash0hash.h include/hash0hash.ic \
		include/ibuf0ibuf.h include/ibuf0ibuf.ic include/ibuf0types.h \
		include/lock0lock.h include/lock0lock.ic include/lock0types.h \
		include/log0log.h include/log0log.ic include/log0recv.h include/log0recv.ic \
		include/mach0data.h include/mach0data.ic include/mem0dbg.h include/mem0dbg.ic \
		include/mem0mem.h include/mem0mem.ic include/mem0pool.h include/mem0pool.ic \
		include/mtr0log.h include/mtr0log.ic include/mtr0mtr.h include/mtr0mtr.ic \
		include/mtr0types.h include/os0file.h \
		include/os0proc.h include/os0proc.ic include/os0sync.h include/os0sync.ic \
		include/os0thread.h include/os0thread.ic \
		include/page0cur.h include/page0cur.ic include/page0page.h include/page0page.ic \
		include/page0types.h \
		include/pars0grm.h include/pars0opt.h include/pars0opt.ic \
		include/pars0pars.h include/pars0pars.ic include/pars0sym.h include/pars0sym.ic \
		include/pars0types.h \
		include/que0que.h include/que0que.ic include/que0types.h \
		include/read0read.h include/read0read.ic include/read0types.h \
		include/rem0cmp.h include/rem0cmp.ic include/rem0rec.h include/rem0rec.ic \
		include/rem0types.h \
		include/row0ins.h include/row0ins.ic include/row0mysql.h include/row0mysql.ic \
		include/row0purge.h include/row0purge.ic include/row0row.h include/row0row.ic \
		include/row0sel.h include/row0sel.ic include/row0types.h \
		include/row0uins.h include/row0uins.ic include/row0umod.h include/row0umod.ic \
		include/row0undo.h include/row0undo.ic include/row0upd.h include/row0upd.ic \
		include/row0vers.h include/row0vers.ic \
		include/srv0que.h include/srv0srv.h include/srv0srv.ic include/srv0start.h \
		include/sync0arr.h include/sync0arr.ic include/sync0rw.h include/sync0rw.ic \
		include/sync0sync.h include/sync0sync.ic include/sync0types.h \
		include/thr0loc.h include/thr0loc.ic \
		include/trx0purge.h include/trx0purge.ic include/trx0rec.h include/trx0rec.ic \
		include/trx0roll.h include/trx0roll.ic include/trx0rseg.h include/trx0rseg.ic \
		include/trx0sys.h include/trx0sys.ic include/trx0trx.h include/trx0trx.ic \
		include/trx0types.h include/trx0undo.h include/trx0undo.ic include/trx0xa.h \
		include/univ.i include/usr0sess.h include/usr0sess.ic include/usr0types.h \
		include/ut0byte.h include/ut0byte.ic include/ut0dbg.h include/ut0lst.h \
		include/ut0mem.h include/ut0mem.ic include/ut0rnd.h include/ut0rnd.ic \
		include/ut0sort.h include/ut0ut.h include/ut0ut.ic include/ut0vec.h include/ut0vec.ic include/ha_prototypes.h \
		include/ut0list.h include/ut0list.ic \
		include/ut0wqueue.h \
		CMakeLists.txt

noinst_LIBRARIES =	libinnobase.a
libinnobase_a_LIBADD =	usr/libusr.a srv/libsrv.a dict/libdict.a \
			que/libque.a srv/libsrv.a ibuf/libibuf.a \
			row/librow.a pars/libpars.a btr/libbtr.a \
			trx/libtrx.a read/libread.a usr/libusr.a \
			buf/libbuf.a ibuf/libibuf.a eval/libeval.a \
			log/liblog.a fsp/libfsp.a fut/libfut.a \
			fil/libfil.a lock/liblock.a mtr/libmtr.a \
			page/libpage.a rem/librem.a thr/libthr.a \
			sync/libsync.a data/libdata.a mach/libmach.a \
			ha/libha.a dyn/libdyn.a mem/libmem.a \
			ut/libut.a os/libos.a ut/libut.a
libinnobase_a_SOURCES =	


libinnobase.a:		$(libinnobase_a_LIBADD)
		-rm -f $@
		if test "$(host_os)" = "netware" ; \
		then \
		  $(libinnobase_a_AR) $@ $(libinnobase_a_LIBADD) ; \
		else \
		  for arc in $(libinnobase_a_LIBADD); do \
		     arpath=`echo $$arc|sed 's|[^/]*$$||'`; \
		     $(AR) t $$arc|sed "s|^|$$arpath|"; \
		  done | sort -u | xargs $(AR) cq $@ ; \
		  $(RANLIB) $@	; \
		fi

# Don't update the files from bitkeeper
%::SCCS/s.%
