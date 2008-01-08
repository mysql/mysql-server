#include "FastScheduler.hpp"
#include "RefConvert.hpp"

#include "Emulator.hpp"
#include "VMSignal.hpp"

#include <SignalLoggerManager.hpp>
#include <BlockNumbers.h>
#include <GlobalSignalNumbers.h>
#include <signaldata/EventReport.hpp>
#include "LongSignal.hpp"
#include <NdbTick.h>

FastScheduler::FastScheduler()
{
}

FastScheduler::~FastScheduler()
{
}

void
FastScheduler::clear()
{
}


void bnr_error()
{
}

void jbuf_error()
{
}

void
FastScheduler::prio_level_error()
{
}

APZJobBuffer::APZJobBuffer()
{
}

APZJobBuffer::~APZJobBuffer()
{
}

void
APZJobBuffer::insert(const SignalHeader * const sh,
		     const Uint32 * const theData, const Uint32 secPtrI[3]){
}

void 
APZJobBuffer::signal2buffer(Signal* signal,
			    BlockNumber bnr, GlobalSignalNumber gsn,
			    BufferEntry& buf)
{
}

#include <TimeQueue.hpp>

TimeQueue::TimeQueue()
{
}

TimeQueue::~TimeQueue()
{
}

