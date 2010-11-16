# Copyright (C) 2004-2005 MySQL AB
#  All rights reserved. Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

noinst_LIBRARIES = libkernel.a libsched.a
EXTRA_LIBRARIES= libsched_mt.a

if BUILD_NDBMTD
noinst_LIBRARIES += libsched_mt.a
endif

libkernel_a_SOURCES = VMSignal.cpp \
                      Emulator.cpp \
                      Configuration.cpp \
                      WatchDog.cpp \
                      SectionReader.cpp \
                      Mutex.cpp \
                      SafeCounter.cpp \
                      Rope.cpp \
                      ndbd_malloc.cpp \
                      ndbd_malloc_impl.cpp \
                      Pool.cpp \
                      WOPool.cpp \
                      RWPool.cpp \
                      DynArr256.cpp \
                      LockQueue.cpp \
                      GlobalData.cpp \
                      SafeMutex.cpp \
                      Ndbinfo.cpp \
                      NdbinfoTables.cpp \
                      ArenaPool.cpp

libsched_a_SOURCES = TimeQueue.cpp \
                     ThreadConfig.cpp \
                     FastScheduler.cpp \
                     TransporterCallback_nonmt.cpp \
                     SimulatedBlock_nonmt.cpp \
                     LongSignal_nonmt.cpp \
                     SimplePropertiesSection_nonmt.cpp \
                     dummy_nonmt.cpp

libsched_mt_a_SOURCES = SimulatedBlock_mt.cpp \
                        TransporterCallback_mt.cpp \
                        LongSignal_mt.cpp \
                        SimplePropertiesSection_mt.cpp \
                        mt.cpp \
                        dummy_mt.cpp

EXTRA_DIST=SimulatedBlock.cpp TransporterCallback.cpp CMakeLists.txt LongSignal.cpp SimplePropertiesSection.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/mgmapi

LDADD_LOC = $(top_builddir)/storage/ndb/src/common/util/libgeneral.la

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

EXTRA_PROGRAMS = ndbd_malloc_impl_test bench_pool testDynArr256 \
                 testSectionReader testLongSignals \
		 testSafeMutex

ndbd_malloc_impl_test_CXXFLAGS = -DUNIT_TEST
ndbd_malloc_impl_test_SOURCES = ndbd_malloc_impl.cpp
ndbd_malloc_impl_test_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la

bench_pool_SOURCES = bench_pool.cpp
bench_pool_LDFLAGS = @ndb_bin_am_ldflags@\
  libkernel.a ../error/liberror.a \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la

testDynArr256_CXXFLAGS = -DUNIT_TEST
testDynArr256_SOURCES = DynArr256.cpp
testDynArr256_LDFLAGS = @ndb_bin_am_ldflags@ \
  libkernel.a ../error/liberror.a \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la

testSectionReader_CXXFLAGS = -DUNIT_TEST
testSectionReader_SOURCES = SectionReader.cpp
testSectionReader_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la

testLongSignals_CXXFLAGS = -DUNIT_TEST
testLongSignals_SOURCES = testLongSig/testLongSig.cpp
testLongSignals_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la \
  @readline_link@ @TERMCAP_LIB@

testSafeMutex_CXXFLAGS = -DUNIT_TEST
testSafeMutex_SOURCES = SafeMutex.cpp
testSafeMutex_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la \
  @readline_link@ @TERMCAP_LIB@


