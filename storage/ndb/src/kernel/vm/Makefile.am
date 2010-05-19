# Copyright (C) 2004-2005 MySQL AB
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

#SUBDIRS = testCopy testDataBuffer testSimplePropertiesSection
#ifneq ($(USE_EDITLINE), N)
#DIRS += testLongSig
#endif

noinst_LIBRARIES = libkernel.a

libkernel_a_SOURCES = \
	SimulatedBlock.cpp	\
	FastScheduler.cpp		\
	TimeQueue.cpp		\
	VMSignal.cpp		\
	ThreadConfig.cpp          \
	TransporterCallback.cpp \
	Emulator.cpp		\
	Configuration.cpp		\
	WatchDog.cpp \
        SimplePropertiesSection.cpp \
        SectionReader.cpp \
        Mutex.cpp SafeCounter.cpp \
        Rope.cpp \
	ndbd_malloc.cpp ndbd_malloc_impl.cpp \
        Pool.cpp WOPool.cpp RWPool.cpp \
        DynArr256.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/mgmapi

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

windoze-dsp: libkernel.dsp

libkernel.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libkernel_a_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)

EXTRA_PROGRAMS = ndbd_malloc_impl_test bench_pool testDynArr256
ndbd_malloc_impl_test_CXXFLAGS = -DUNIT_TEST
ndbd_malloc_impl_test_SOURCES = ndbd_malloc_impl.cpp
ndbd_malloc_impl_test_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysys.a \
  $(top_builddir)/dbug/libdbug.a \
  $(top_builddir)/strings/libmystrings.a

bench_pool_SOURCES = bench_pool.cpp
bench_pool_LDFLAGS = @ndb_bin_am_ldflags@\
  libkernel.a ../error/liberror.a \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysys.a \
  $(top_builddir)/dbug/libdbug.a \
  $(top_builddir)/strings/libmystrings.a

testDynArr256_CXXFLAGS = -DUNIT_TEST
testDynArr256_SOURCES = DynArr256.cpp
testDynArr256_LDFLAGS = @ndb_bin_am_ldflags@ \
  libkernel.a ../error/liberror.a \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
  $(top_builddir)/mysys/libmysys.a \
  $(top_builddir)/dbug/libdbug.a \
  $(top_builddir)/strings/libmystrings.a

