# Copyright (c) 2004-2006 MySQL AB
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

noinst_LIBRARIES = libkernel.a libsched.a libtest.a
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
                      ArenaPool.cpp \
                      mt_thr_config.cpp

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

libtest_a_SOURCES = test_context.cpp \
                    ../blocks/dbtup/tuppage.cpp

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
ndbd_malloc_impl_test_LDFLAGS = @ndb_bin_am_ldflags@
ndbd_malloc_impl_test_LDADD = \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la

bench_pool_SOURCES = bench_pool.cpp
bench_pool_LDFLAGS = @ndb_bin_am_ldflags@
bench_pool_LDADD = libkernel.a ../error/liberror.a \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la

test_ldadd = libtest.a \
  libkernel.a libsched.a ../error/liberror.a \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la

testDynArr256_CXXFLAGS = -DUNIT_TEST
testDynArr256_SOURCES = DynArr256.cpp
testDynArr256_LDFLAGS = @ndb_bin_am_ldflags@
testDynArr256_LDADD = $(test_ldadd)

CountingPool_t_CXXFLAGS = -DTAP_TEST
CountingPool_t_SOURCES = CountingPool.cpp
CountingPool_t_LDFLAGS = @ndb_bin_am_ldflags@
CountingPool_t_LDADD = $(test_ldadd)

DynArr256_t_CXXFLAGS = -DTAP_TEST
DynArr256_t_SOURCES = DynArr256.cpp
DynArr256_t_LDFLAGS = @ndb_bin_am_ldflags@
DynArr256_t_LDADD = $(test_ldadd)

LHLevel_t_CXXFLAGS = -DTAP_TEST
LHLevel_t_SOURCES = LHLevel.cpp
LHLevel_t_LDFLAGS = @ndb_bin_am_ldflags@
LHLevel_t_LDADD = $(test_ldadd)

testSectionReader_CXXFLAGS = -DUNIT_TEST
testSectionReader_SOURCES = SectionReader.cpp
testSectionReader_LDFLAGS = @ndb_bin_am_ldflags@
testSectionReader_LDADD = $(test_ldadd)

testLongSignals_CXXFLAGS = -DUNIT_TEST
testLongSignals_SOURCES = testLongSig/testLongSig.cpp
testLongSignals_LDFLAGS = @ndb_bin_am_ldflags@
testLongSignals_LDADD = $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la \
  @readline_link@ @TERMCAP_LIB@

testSafeMutex_CXXFLAGS = -DUNIT_TEST
testSafeMutex_SOURCES = SafeMutex.cpp
testSafeMutex_LDFLAGS = @ndb_bin_am_ldflags@
testSafeMutex_LDADD = \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysyslt.la \
  $(top_builddir)/dbug/libdbuglt.la \
  $(top_builddir)/strings/libmystringslt.la \
  @readline_link@ @TERMCAP_LIB@

mt_thr_config_t_SOURCES = mt_thr_config.cpp
mt_thr_config_t_CXXFLAGS = -DTEST_MT_THR_CONFIG
mt_thr_config_t_LDADD = \
	$(top_builddir)/storage/ndb/src/common/util/libgeneral.la \
	$(top_builddir)/mysys/libmysyslt.la

noinst_PROGRAMS = mt_thr_config-t CountingPool-t DynArr256-t LHLevel-t

mt_send_t_CXXFLAGS = -DTAP_TEST
mt_send_t_SOURCES = mt-send-t.cpp
mt_send_t_LDFLAGS = @ndb_bin_am_ldflags@
mt_send_t_LDADD = $(test_ldadd)

if BUILD_NDBMTD
noinst_PROGRAMS += mt-send-t
endif
