include .defs.mk	

TYPE := kernel

ARCHIVE_TARGET := kernel

SOURCES = \
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

DIRS := testCopy testDataBuffer testSimplePropertiesSection
ifneq ($(USE_EDITLINE), N)
DIRS += testLongSig
endif


include $(NDB_TOP)/Epilogue.mk
