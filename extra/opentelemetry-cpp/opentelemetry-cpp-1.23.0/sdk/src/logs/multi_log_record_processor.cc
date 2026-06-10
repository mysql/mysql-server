// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <memory>
#include <utility>
#include <vector>

#include "opentelemetry/sdk/logs/multi_log_record_processor.h"
#include "opentelemetry/sdk/logs/multi_recordable.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

MultiLogRecordProcessor::MultiLogRecordProcessor(
    std::vector<std::unique_ptr<LogRecordProcessor>> &&processors)
{
  auto log_record_processors = std::move(processors);
  for (auto &processor : log_record_processors)
  {
    AddProcessor(std::move(processor));
  }
}

MultiLogRecordProcessor::~MultiLogRecordProcessor()
{
  InternalForceFlush();
  InternalShutdown();
}

void MultiLogRecordProcessor::AddProcessor(std::unique_ptr<LogRecordProcessor> &&processor)
{
  // Add preocessor to end of the list.
  if (processor)
  {
    processors_.emplace_back(std::move(processor));
  }
}

std::unique_ptr<Recordable> MultiLogRecordProcessor::MakeRecordable() noexcept
{
  auto recordable       = std::unique_ptr<Recordable>(new MultiRecordable());
  auto multi_recordable = static_cast<MultiRecordable *>(recordable.get());
  for (auto &processor : processors_)
  {
    multi_recordable->AddRecordable(*processor, processor->MakeRecordable());
  }
  return recordable;
}

void MultiLogRecordProcessor::OnEmit(std::unique_ptr<Recordable> &&record) noexcept
{
  auto log_record = std::move(record);
  if (!log_record)
  {
    return;
  }
  auto multi_recordable = static_cast<MultiRecordable *>(log_record.get());

  for (auto &processor : processors_)
  {
    auto recordable = multi_recordable->ReleaseRecordable(*processor);
    if (recordable)
    {
      processor->OnEmit(std::move(recordable));
    }
  }
}

bool MultiLogRecordProcessor::ForceFlush(std::chrono::microseconds timeout) noexcept
{
  return InternalForceFlush(timeout);
}

bool MultiLogRecordProcessor::Shutdown(std::chrono::microseconds timeout) noexcept
{
  return InternalShutdown(timeout);
}

bool MultiLogRecordProcessor::InternalForceFlush(std::chrono::microseconds timeout) noexcept
{
  bool result           = true;
  auto start_time       = std::chrono::system_clock::now();
  auto overflow_checker = std::chrono::system_clock::time_point::max();
  std::chrono::system_clock::time_point expire_time;
  if (std::chrono::duration_cast<std::chrono::microseconds>(overflow_checker - start_time) <=
      timeout)
  {
    expire_time = overflow_checker;
  }
  else
  {
    expire_time =
        start_time + std::chrono::duration_cast<std::chrono::system_clock::duration>(timeout);
  }
  for (auto &processor : processors_)
  {
    if (!processor->ForceFlush(timeout))
    {
      result = false;
    }
    start_time = std::chrono::system_clock::now();
    if (expire_time > start_time)
    {
      timeout = std::chrono::duration_cast<std::chrono::microseconds>(expire_time - start_time);
    }
    else
    {
      timeout = std::chrono::microseconds::zero();
    }
  }
  return result;
}

bool MultiLogRecordProcessor::InternalShutdown(std::chrono::microseconds timeout) noexcept
{
  bool result           = true;
  auto start_time       = std::chrono::system_clock::now();
  auto overflow_checker = std::chrono::system_clock::time_point::max();
  std::chrono::system_clock::time_point expire_time;
  if (std::chrono::duration_cast<std::chrono::microseconds>(overflow_checker - start_time) <=
      timeout)
  {
    expire_time = overflow_checker;
  }
  else
  {
    expire_time =
        start_time + std::chrono::duration_cast<std::chrono::system_clock::duration>(timeout);
  }
  for (auto &processor : processors_)
  {
    result |= processor->Shutdown(timeout);
    start_time = std::chrono::system_clock::now();
    if (expire_time > start_time)
    {
      timeout = std::chrono::duration_cast<std::chrono::microseconds>(expire_time - start_time);
    }
    else
    {
      timeout = std::chrono::microseconds::zero();
    }
  }
  return result;
}

}  // namespace logs
}  // namespace sdk

OPENTELEMETRY_END_NAMESPACE
