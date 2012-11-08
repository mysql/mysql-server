# Copyright (C) 2001, 2004, 2006 MySQL AB & Innobase Oy
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

# Process this file with automake to create Makefile.in

MYSQLDATAdir=		$(localstatedir)
MYSQLSHAREdir=		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)
MYSQLLIBdir=		$(pkglibdir)
pkgplugindir=		$(pkglibdir)/plugin
INCLUDES=		-I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(srcdir)/include \
			-I$(top_srcdir)/sql \
			-I$(srcdir) @ZLIB_INCLUDES@

DEFS=			@DEFS@


noinst_HEADERS=		\
			handler/ha_innodb.h	\
			handler/i_s.h		\
			include/btr0btr.h	\
			include/btr0btr.ic	\
			include/btr0cur.h	\
			include/btr0cur.ic	\
			include/btr0pcur.h	\
			include/btr0pcur.ic	\
			include/btr0sea.h	\
			include/btr0sea.ic	\
			include/btr0types.h	\
			include/buf0buddy.h	\
			include/buf0buddy.ic	\
			include/buf0buf.h	\
			include/buf0buf.ic	\
			include/buf0flu.h	\
			include/buf0flu.ic	\
			include/buf0lru.h	\
			include/buf0lru.ic	\
			include/buf0rea.h	\
			include/buf0types.h	\
			include/data0data.h	\
			include/data0data.ic	\
			include/data0type.h	\
			include/data0type.ic	\
			include/data0types.h	\
			include/db0err.h	\
			include/dict0boot.h	\
			include/dict0boot.ic	\
			include/dict0crea.h	\
			include/dict0crea.ic	\
			include/dict0dict.h	\
			include/dict0dict.ic	\
			include/dict0load.h	\
			include/dict0load.ic	\
			include/dict0mem.h	\
			include/dict0mem.ic	\
			include/dict0types.h	\
			include/dyn0dyn.h	\
			include/dyn0dyn.ic	\
			include/eval0eval.h	\
			include/eval0eval.ic	\
			include/eval0proc.h	\
			include/eval0proc.ic	\
			include/fil0fil.h	\
			include/fsp0fsp.h	\
			include/fsp0fsp.ic	\
			include/fsp0types.h	\
			include/fut0fut.h	\
			include/fut0fut.ic	\
			include/fut0lst.h	\
			include/fut0lst.ic	\
			include/ha0ha.h		\
			include/ha0ha.ic	\
			include/ha0storage.h	\
			include/ha0storage.ic	\
			include/ha_prototypes.h	\
			include/handler0alter.h	\
			include/hash0hash.h	\
			include/hash0hash.ic	\
			include/ibuf0ibuf.h	\
			include/ibuf0ibuf.ic	\
			include/ibuf0types.h	\
			include/lock0iter.h	\
			include/lock0lock.h	\
			include/lock0lock.ic	\
			include/lock0priv.h	\
			include/lock0priv.ic	\
			include/lock0types.h	\
			include/log0log.h	\
			include/log0log.ic	\
			include/log0online.h	\
			include/log0recv.h	\
			include/log0recv.ic	\
			include/mach0data.h	\
			include/mach0data.ic	\
			include/mem0dbg.h	\
			include/mem0dbg.ic	\
			include/mem0mem.h	\
			include/mem0mem.ic	\
			include/mem0pool.h	\
			include/mem0pool.ic	\
			include/mtr0log.h	\
			include/mtr0log.ic	\
			include/mtr0mtr.h	\
			include/mtr0mtr.ic	\
			include/mtr0types.h	\
			include/mysql_addons.h	\
			include/os0file.h	\
			include/os0proc.h	\
			include/os0proc.ic	\
			include/os0sync.h	\
			include/os0sync.ic	\
			include/os0thread.h	\
			include/os0thread.ic	\
			include/page0cur.h	\
			include/page0cur.ic	\
			include/page0page.h	\
			include/page0page.ic	\
			include/page0types.h	\
			include/page0zip.h	\
			include/page0zip.ic	\
			include/pars0grm.h	\
			include/pars0opt.h	\
			include/pars0opt.ic	\
			include/pars0pars.h	\
			include/pars0pars.ic	\
			include/pars0sym.h	\
			include/pars0sym.ic	\
			include/pars0types.h	\
			include/que0que.h	\
			include/que0que.ic	\
			include/que0types.h	\
			include/read0read.h	\
			include/read0read.ic	\
			include/read0types.h	\
			include/rem0cmp.h	\
			include/rem0cmp.ic	\
			include/rem0rec.h	\
			include/rem0rec.ic	\
			include/rem0types.h	\
			include/row0ext.h	\
			include/row0ext.ic	\
			include/row0ins.h	\
			include/row0ins.ic	\
			include/row0merge.h	\
			include/row0mysql.h	\
			include/row0mysql.ic	\
			include/row0purge.h	\
			include/row0purge.ic	\
			include/row0row.h	\
			include/row0row.ic	\
			include/row0sel.h	\
			include/row0sel.ic	\
			include/row0types.h	\
			include/row0uins.h	\
			include/row0uins.ic	\
			include/row0umod.h	\
			include/row0umod.ic	\
			include/row0undo.h	\
			include/row0undo.ic	\
			include/row0upd.h	\
			include/row0upd.ic	\
			include/row0vers.h	\
			include/row0vers.ic	\
			include/srv0que.h	\
			include/srv0srv.h	\
			include/srv0srv.ic	\
			include/srv0start.h	\
			include/sync0arr.h	\
			include/sync0arr.ic	\
			include/sync0rw.h	\
			include/sync0rw.ic	\
			include/sync0sync.h	\
			include/sync0sync.ic	\
			include/sync0types.h	\
			include/thr0loc.h	\
			include/thr0loc.ic	\
			include/trx0i_s.h	\
			include/trx0purge.h	\
			include/trx0purge.ic	\
			include/trx0rec.h	\
			include/trx0rec.ic	\
			include/trx0roll.h	\
			include/trx0roll.ic	\
			include/trx0rseg.h	\
			include/trx0rseg.ic	\
			include/trx0sys.h	\
			include/trx0sys.ic	\
			include/trx0trx.h	\
			include/trx0trx.ic	\
			include/trx0types.h	\
			include/trx0undo.h	\
			include/trx0undo.ic	\
			include/trx0xa.h	\
			include/univ.i		\
			include/usr0sess.h	\
			include/usr0sess.ic	\
			include/usr0types.h	\
			include/ut0auxconf.h	\
			include/ut0byte.h	\
			include/ut0byte.ic	\
			include/ut0dbg.h	\
			include/ut0list.h	\
			include/ut0list.ic	\
			include/ut0lst.h	\
			include/ut0mem.h	\
			include/ut0mem.ic	\
			include/ut0rbt.h	\
			include/ut0rnd.h	\
			include/ut0rnd.ic	\
			include/ut0sort.h	\
			include/ut0ut.h		\
			include/ut0ut.ic	\
			include/ut0vec.h	\
			include/ut0vec.ic	\
			include/ut0wqueue.h	\
			mem/mem0dbg.c

noinst_LTLIBRARIES=	@plugin_xtradb_static_target@
libxtradb_la_SOURCES=	\
			btr/btr0btr.c			\
			btr/btr0cur.c			\
			btr/btr0pcur.c			\
			btr/btr0sea.c			\
			buf/buf0buddy.c			\
			buf/buf0buf.c			\
			buf/buf0flu.c			\
			buf/buf0lru.c			\
			buf/buf0rea.c			\
			data/data0data.c		\
			data/data0type.c		\
			dict/dict0boot.c		\
			dict/dict0crea.c		\
			dict/dict0dict.c		\
			dict/dict0load.c		\
			dict/dict0mem.c			\
			dyn/dyn0dyn.c			\
			eval/eval0eval.c		\
			eval/eval0proc.c		\
			fil/fil0fil.c			\
			fsp/fsp0fsp.c			\
			fut/fut0fut.c			\
			fut/fut0lst.c			\
			ha/ha0ha.c			\
			ha/ha0storage.c			\
			ha/hash0hash.c			\
			handler/ha_innodb.cc		\
			handler/handler0alter.cc	\
			handler/i_s.cc			\
			handler/mysql_addons.cc		\
			ibuf/ibuf0ibuf.c		\
			lock/lock0iter.c		\
			lock/lock0lock.c		\
			log/log0log.c			\
			log/log0online.c		\
			log/log0recv.c			\
			mach/mach0data.c		\
			mem/mem0mem.c			\
			mem/mem0pool.c			\
			mtr/mtr0log.c			\
			mtr/mtr0mtr.c			\
			os/os0file.c			\
			os/os0proc.c			\
			os/os0sync.c			\
			os/os0thread.c			\
			page/page0cur.c			\
			page/page0page.c		\
			page/page0zip.c			\
			pars/lexyy.c			\
			pars/pars0grm.c			\
			pars/pars0opt.c			\
			pars/pars0pars.c		\
			pars/pars0sym.c			\
			que/que0que.c			\
			read/read0read.c		\
			rem/rem0cmp.c			\
			rem/rem0rec.c			\
			row/row0ext.c			\
			row/row0ins.c			\
			row/row0merge.c			\
			row/row0mysql.c			\
			row/row0purge.c			\
			row/row0row.c			\
			row/row0sel.c			\
			row/row0uins.c			\
			row/row0umod.c			\
			row/row0undo.c			\
			row/row0upd.c			\
			row/row0vers.c			\
			srv/srv0que.c			\
			srv/srv0srv.c			\
			srv/srv0start.c			\
			sync/sync0arr.c			\
			sync/sync0rw.c			\
			sync/sync0sync.c		\
			thr/thr0loc.c			\
			trx/trx0i_s.c			\
			trx/trx0purge.c			\
			trx/trx0rec.c			\
			trx/trx0roll.c			\
			trx/trx0rseg.c			\
			trx/trx0sys.c			\
			trx/trx0trx.c			\
			trx/trx0undo.c			\
			usr/usr0sess.c			\
			ut/ut0byte.c			\
			ut/ut0dbg.c			\
			ut/ut0list.c			\
			ut/ut0mem.c			\
			ut/ut0rbt.c			\
			ut/ut0rnd.c			\
			ut/ut0ut.c			\
			ut/ut0vec.c			\
			ut/ut0wqueue.c

libxtradb_la_CXXFLAGS=	$(AM_CXXFLAGS)
libxtradb_la_CFLAGS=	$(AM_CFLAGS)

EXTRA_LTLIBRARIES=	libxtradb.la ha_xtradb.la
pkgplugin_LTLIBRARIES=	@plugin_xtradb_shared_target@

ha_xtradb_la_LDFLAGS=	-module -rpath $(pkgplugindir) -L$(top_builddir)/libservices -lmysqlservices
ha_xtradb_la_CXXFLAGS=	-shared $(AM_CXXFLAGS) $(INNODB_DYNAMIC_CFLAGS)
ha_xtradb_la_CFLAGS=	-shared $(AM_CFLAGS) $(INNODB_DYNAMIC_CFLAGS)
ha_xtradb_la_SOURCES=	$(libxtradb_la_SOURCES)

EXTRA_DIST=		CMakeLists.txt plug.in \
			pars/make_bison.sh pars/make_flex.sh \
			pars/pars0grm.y pars/pars0lex.l
