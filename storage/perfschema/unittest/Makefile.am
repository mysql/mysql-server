# Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

INCLUDES = -I$(top_builddir)/include \
 -I$(top_srcdir)/include \
 -I$(top_srcdir)/include/mysql \
 -I$(top_srcdir)/regex \
 -I$(top_srcdir)/unittest/mytap \
 -I$(top_srcdir)/sql \
 -I$(top_srcdir)/storage/perfschema

DEFS = -DMYSQL_SERVER @DEFS@

TEST_CODE = $(top_builddir)/unittest/mytap/libmytap.a

$(TEST_CODE) :
	(cd $(top_builddir)/unittest/mytap; $(MAKE))

PROD_CODE = $(top_builddir)/storage/perfschema/libperfschema.a \
  $(top_builddir)/mysys/libmysys.a \
  $(top_builddir)/dbug/libdbug.a \
  $(top_builddir)/strings/libmystrings.a

noinst_PROGRAMS = pfs_instr_class-t pfs_instr_class-oom-t \
	pfs_instr-t pfs_instr-oom-t pfs_timer-t pfs-t

pfs_t_SOURCES = pfs-t.cc stub_print_error.h
pfs_t_LDADD = $(TEST_CODE) $(PROD_CODE)

pfs_instr_class_t_SOURCES = pfs_instr_class-t.cc
pfs_instr_class_t_LDADD = $(TEST_CODE) $(PROD_CODE)

pfs_instr_class_oom_t_SOURCES = pfs_instr_class-oom-t.cc stub_pfs_global.h
pfs_instr_class_oom_t_LDADD = $(TEST_CODE) $(PROD_CODE)

pfs_instr_t_SOURCES = pfs_instr-t.cc
pfs_instr_t_LDADD = $(TEST_CODE) $(PROD_CODE)

pfs_instr_oom_t_SOURCES = pfs_instr-oom-t.cc stub_pfs_global.h
pfs_instr_oom_t_LDADD = $(TEST_CODE) $(PROD_CODE)

pfs_timer_t_SOURCES = pfs_timer-t.cc
pfs_timer_t_LDADD = $(TEST_CODE) $(PROD_CODE)

EXTRA_DIST = conf.txt CMakeLists.txt

