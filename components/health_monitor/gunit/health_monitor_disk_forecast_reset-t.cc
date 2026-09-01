/* Copyright (c) 2026, Oracle and/or its affiliates.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include <mysql/components/services/mysql_mutex.h>
#include <mysql/components/services/mysql_rwlock.h>
#include <mysql/components/services/psi_memory.h>
#include <thr_rwlock.h>

#include "../health_monitor_disk_forecast_reset.h"
#include "../health_monitor_disk_forecaster.h"

REQUIRES_MYSQL_MUTEX_SERVICE_PLACEHOLDER;

namespace hm {
namespace {

std::vector<mysql_rwlock_t *> write_lock_order;

void register_rwlocks(const char *, PSI_rwlock_info *, int) {}

void register_memory(const char *, PSI_memory_info *, int) {}

PSI_memory_key memory_alloc(PSI_memory_key key, size_t, PSI_thread **owner) {
  *owner = nullptr;
  return key;
}

PSI_memory_key memory_realloc(PSI_memory_key key, size_t, size_t,
                              PSI_thread **owner) {
  *owner = nullptr;
  return key;
}

PSI_memory_key memory_claim(PSI_memory_key key, size_t, PSI_thread **owner,
                            bool) {
  *owner = nullptr;
  return key;
}

void memory_free(PSI_memory_key, size_t, PSI_thread *) {}

int tracked_rwlock_wrlock(mysql_rwlock_t *lock, const char *file,
                          unsigned int line) {
  (void)file;
  (void)line;
  write_lock_order.push_back(lock);
  return native_rw_wrlock(&lock->m_rwlock);
}

int rwlock_init(PSI_rwlock_key, mysql_rwlock_t *lock, const char *,
                unsigned int) {
  lock->m_psi = nullptr;
  return native_rw_init(&lock->m_rwlock);
}

int rwlock_destroy(mysql_rwlock_t *lock, const char *, unsigned int) {
  return native_rw_destroy(&lock->m_rwlock);
}

int rwlock_rdlock(mysql_rwlock_t *lock, const char *, unsigned int) {
  return native_rw_rdlock(&lock->m_rwlock);
}

int rwlock_tryrdlock(mysql_rwlock_t *lock, const char *, unsigned int) {
  return native_rw_tryrdlock(&lock->m_rwlock);
}

int rwlock_trywrlock(mysql_rwlock_t *lock, const char *, unsigned int) {
  return native_rw_trywrlock(&lock->m_rwlock);
}

int rwlock_unlock(mysql_rwlock_t *lock, const char *, unsigned int) {
  return native_rw_unlock(&lock->m_rwlock);
}

SERVICE_TYPE_NO_CONST(mysql_rwlock_v1)
rwlock_service{
    register_rwlocks, rwlock_init,      nullptr,          rwlock_destroy,
    nullptr,          rwlock_rdlock,    nullptr,          tracked_rwlock_wrlock,
    nullptr,          rwlock_tryrdlock, rwlock_trywrlock, rwlock_unlock,
    nullptr};

SERVICE_TYPE_NO_CONST(psi_memory_v2)
memory_service{register_memory, memory_alloc, memory_realloc, memory_claim,
               memory_free};

}  // namespace
}  // namespace hm

SERVICE_TYPE(mysql_rwlock_v1) *mysql_service_mysql_rwlock_v1 =
    &hm::rwlock_service;
SERVICE_TYPE(psi_memory_v2) *mysql_service_psi_memory_v2 = &hm::memory_service;

namespace hm {
namespace {

TEST(HealthMonitorDiskForecastReset, RestoresWriteableStateAfterReset) {
  bool writeable = true;
  bool reset_saw_paused_writes = false;

  reset_disk_series([&writeable]() { return writeable; },
                    [&writeable](bool value) { writeable = value; },
                    [&writeable, &reset_saw_paused_writes]() {
                      reset_saw_paused_writes = !writeable;
                    });

  EXPECT_TRUE(reset_saw_paused_writes);
  EXPECT_TRUE(writeable);
}

TEST(HealthMonitorDiskForecastReset, PreservesPausedWriteableState) {
  bool writeable = false;
  int reset_count = 0;

  reset_disk_series([&writeable]() { return writeable; },
                    [&writeable](bool value) { writeable = value; },
                    [&reset_count]() { ++reset_count; });

  EXPECT_EQ(1, reset_count);
  EXPECT_FALSE(writeable);
}

TEST(HealthMonitorDiskForecastReset, AlignsNextMetricWithMonotonicCount) {
  EXPECT_EQ(17U, disk_metric_position_after_reset(17));
}

TEST(HealthMonitorDiskForecastReset, ClearsRetainedReportingState) {
  size_t total_metric_count = 1;
  time_t last_forecast_time = 2;
  std::vector<Disk_Forecast> last_forecasts(1);
  Disk_Forecast_Summary last_summary;
  last_summary.timestamp = 3;
  uint64_t total_mb = 100;
  uint64_t last_total_mb = 100;
  uint64_t used_mb = 50;
  double used_pct = 50.0;
  bool volume_size_changed = true;

  clear_disk_forecast_retained_state(
      total_metric_count, last_forecast_time, last_forecasts, last_summary,
      total_mb, last_total_mb, used_mb, used_pct, volume_size_changed);

  EXPECT_EQ(0U, total_metric_count);
  EXPECT_EQ(0, last_forecast_time);
  EXPECT_TRUE(last_forecasts.empty());
  EXPECT_EQ(0, last_summary.timestamp);
  EXPECT_EQ(0U, total_mb);
  EXPECT_EQ(0U, last_total_mb);
  EXPECT_EQ(0U, used_mb);
  EXPECT_DOUBLE_EQ(0.0, used_pct);
  EXPECT_FALSE(volume_size_changed);
}

TEST(HealthMonitorDiskForecastReset, ProductionResetClearsSeriesInLockOrder) {
  auto metric_buffer = std::make_shared<Metric_Buffer<Disk_Metric>>(
      static_cast<PSI_rwlock_key>(0), static_cast<PSI_memory_key>(0));
  auto forecast_buffer = std::make_shared<Stat_Buffer<Disk_Forecast>>(
      static_cast<PSI_rwlock_key>(0), static_cast<PSI_memory_key>(0));
  auto summary_buffer = std::make_shared<Stat_Buffer<Disk_Forecast_Summary>>(
      static_cast<PSI_rwlock_key>(0), static_cast<PSI_memory_key>(0));
  metric_buffer->resize(4);
  forecast_buffer->resize(4);
  summary_buffer->resize(2);

  metric_buffer->set_writeable(true);
  Disk_Metric first_metric;
  first_metric.timestamp = 1;
  first_metric.total_mb = 100;
  first_metric.used_mb = 40;
  ASSERT_EQ(0, metric_buffer->write(first_metric));
  metric_buffer->set_writeable(true);
  Disk_Metric second_metric = first_metric;
  second_metric.timestamp = 2;
  second_metric.used_mb = 41;
  ASSERT_EQ(0, metric_buffer->write(second_metric));
  const size_t total_written_before_reset = metric_buffer->total_written();
  ASSERT_GT(total_written_before_reset, 0U);

  Disk_Forecast forecast;
  forecast.timestamp = 3;
  ASSERT_EQ(0, forecast_buffer->write(forecast));
  Disk_Forecast_Summary table_summary;
  table_summary.timestamp = 4;
  ASSERT_EQ(0, summary_buffer->write(table_summary));

  Holt_Winters_Forecaster forecaster;
  forecaster.state().initialized = true;
  forecaster.state().next_metric = 1;
  forecaster.state().season = {1.0, 2.0};
  mysql_rwlock_t model_lock;
  ASSERT_EQ(0, mysql_rwlock_init(0, &model_lock));

  size_t total_metric_count = total_written_before_reset;
  time_t last_forecast_time = 5;
  std::vector<Disk_Forecast> last_forecasts(1, forecast);
  Disk_Forecast_Summary last_summary = table_summary;
  uint64_t total_mb = 100;
  uint64_t last_total_mb = 100;
  uint64_t used_mb = 41;
  double used_pct = 41.0;
  bool volume_size_changed = true;
  std::mutex series_mutex;

  Disk_forecast_reset_context context{&series_mutex,
                                      metric_buffer,
                                      nullptr,
                                      forecast_buffer,
                                      summary_buffer,
                                      &forecaster,
                                      &model_lock,
                                      &total_metric_count,
                                      &last_forecast_time,
                                      &last_forecasts,
                                      &last_summary,
                                      &total_mb,
                                      &last_total_mb,
                                      &used_mb,
                                      &used_pct,
                                      &volume_size_changed};

  metric_buffer->set_writeable(true);
  write_lock_order.clear();
  Disk_Forecaster::reset_filesystem_series(context);

  ASSERT_GE(write_lock_order.size(), 2U);
  EXPECT_EQ(metric_buffer->get_rwlock(), write_lock_order[0]);
  EXPECT_EQ(&model_lock, write_lock_order[1]);
  EXPECT_TRUE(metric_buffer->writeable());
  EXPECT_EQ(0U, metric_buffer->count());
  EXPECT_EQ(total_written_before_reset, metric_buffer->total_written());
  EXPECT_FALSE(forecaster.state().initialized);
  EXPECT_EQ(total_written_before_reset, forecaster.state().next_metric);
  EXPECT_EQ(0U, forecast_buffer->count());
  EXPECT_EQ(0U, summary_buffer->count());
  EXPECT_EQ(0U, total_metric_count);
  EXPECT_EQ(0, last_forecast_time);
  EXPECT_TRUE(last_forecasts.empty());
  EXPECT_EQ(0, last_summary.timestamp);
  EXPECT_EQ(0U, total_mb);
  EXPECT_EQ(0U, last_total_mb);
  EXPECT_EQ(0U, used_mb);
  EXPECT_DOUBLE_EQ(0.0, used_pct);
  EXPECT_FALSE(volume_size_changed);

  Disk_Metric next_metric = second_metric;
  next_metric.timestamp = 6;
  ASSERT_EQ(0, metric_buffer->write(next_metric));
  EXPECT_EQ(1U, metric_buffer->count());
  EXPECT_EQ(total_written_before_reset + 1, metric_buffer->total_written());
  int row = -1;
  EXPECT_TRUE(
      metric_buffer->map_logical_to_row(total_written_before_reset, row));
  EXPECT_EQ(0, row);

  EXPECT_EQ(0, mysql_rwlock_destroy(&model_lock));
}

}  // namespace
}  // namespace hm
