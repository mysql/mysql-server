#SUBDIRS = testCopy testDataBuffer testSimplePropertiesSection
#ifneq ($(USE_EDITLINE), N)
#DIRS += testLongSig
#endif

pkglib_LIBRARIES = libkernel.a

libkernel_a_SOURCES = \
	SimulatedBlock.cpp	\
	FastScheduler.cpp		\
	TimeQueue.cpp		\
	VMSignal.cpp		\
	ThreadConfig.cpp          \
	TransporterCallback.cpp \
	Emulator.cpp		\
	Configuration.cpp		\
	ClusterConfiguration.cpp	\
	WatchDog.cpp \
        SimplePropertiesSection.cpp \
        SectionReader.cpp \
	MetaData.cpp \
        Mutex.cpp SafeCounter.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
