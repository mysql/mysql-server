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
	MetaData.cpp \
        Mutex.cpp SafeCounter.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/mgmapi

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libkernel.dsp

libkernel.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libkernel_a_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LIB $(LDADD)
