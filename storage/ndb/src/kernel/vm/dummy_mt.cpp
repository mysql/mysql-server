/*
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "FastScheduler.hpp"
#include "RefConvert.hpp"

#include "Emulator.hpp"
#include "VMSignal.hpp"

#include <BlockNumbers.h>
#include <GlobalSignalNumbers.h>
#include <NdbTick.h>
#include <SignalLoggerManager.hpp>
#include <TimeQueue.hpp>
#include <signaldata/EventReport.hpp>
#include "LongSignal.hpp"

#define JAM_FILE_ID 229

FastScheduler::FastScheduler() {}

FastScheduler::~FastScheduler() {}

void FastScheduler::clear() {}

void bnr_error() {}

void jbuf_error() {}

void FastScheduler::prio_level_error() {}

APZJobBuffer::APZJobBuffer() {}

APZJobBuffer::~APZJobBuffer() {}

void APZJobBuffer::insert(const SignalHeader *const sh,
                          const Uint32 *const theData,
                          const Uint32 secPtrI[3]) {}

void APZJobBuffer::signal2buffer(Signal25 *signal, BufferEntry &buf) {}

TimeQueue::TimeQueue() {}

TimeQueue::~TimeQueue() {}

bool NdbIsMultiThreaded() { return true; }
