/*
  Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "tm_mysql_metric.h"
#include "tm_psi.h"

namespace telemetry {

class MySingleKeyValueIterable
    : public opentelemetry::common::KeyValueIterable {
 public:
  MySingleKeyValueIterable(const char *name, const char *value);
  ~MySingleKeyValueIterable() override = default;

  bool ForEachKeyValue(opentelemetry::nostd::function_ref<
                       bool(opentelemetry::nostd::string_view,
                            opentelemetry::common::AttributeValue)>
                           callback) const noexcept override;

  size_t size() const noexcept override { return 1; }

 private:
  const char *m_name;
  const char *m_value;
};

MySingleKeyValueIterable::MySingleKeyValueIterable(const char *name,
                                                   const char *value)
    : m_name(name), m_value(value) {}

bool MySingleKeyValueIterable::ForEachKeyValue(
    opentelemetry::nostd::function_ref<
        bool(opentelemetry::nostd::string_view,
             opentelemetry::common::AttributeValue)>
        callback) const noexcept {
  const opentelemetry::nostd::string_view name(m_name);
  const opentelemetry::common::AttributeValue value(m_value);

  return callback(name, value);
}

class MyMultiKeyValueIterable : public opentelemetry::common::KeyValueIterable {
 public:
  MyMultiKeyValueIterable(const char **name_array, const char **value_array,
                          size_t size);
  ~MyMultiKeyValueIterable() override = default;

  bool ForEachKeyValue(opentelemetry::nostd::function_ref<
                       bool(opentelemetry::nostd::string_view,
                            opentelemetry::common::AttributeValue)>
                           callback) const noexcept override;

  size_t size() const noexcept override { return m_size; }

 private:
  const char **m_name_array;
  const char **m_value_array;
  size_t m_size;
};

MyMultiKeyValueIterable::MyMultiKeyValueIterable(const char **name_array,
                                                 const char **value_array,
                                                 size_t size)
    : m_name_array(name_array), m_value_array(value_array), m_size(size) {}

bool MyMultiKeyValueIterable::ForEachKeyValue(
    opentelemetry::nostd::function_ref<
        bool(opentelemetry::nostd::string_view,
             opentelemetry::common::AttributeValue)>
        callback) const noexcept {
  bool rc;
  for (size_t i = 0; i < m_size; i++) {
    const opentelemetry::nostd::string_view name(m_name_array[i]);
    const opentelemetry::common::AttributeValue value(m_value_array[i]);

    rc = callback(name, value);

    if (!rc) {
      return false;
    }
  }

  return true;
}

void my_delivery_int64_0_callback(void *context, int64_t value) {
  auto *access =
      reinterpret_cast<opentelemetry::metrics::ObserverResultT<int64_t> *>(
          context);

  assert(access != nullptr);
  access->Observe(value);
}

void my_delivery_int64_1_callback(void *context, int64_t value,
                                  const char *attr_name,
                                  const char *attr_value) {
  auto *access =
      reinterpret_cast<opentelemetry::metrics::ObserverResultT<int64_t> *>(
          context);

  assert(access != nullptr);
  const MySingleKeyValueIterable kv(attr_name, attr_value);
  access->Observe(value, kv);
}

void my_delivery_int64_n_callback(void *context, int64_t value,
                                  const char **attr_name_array,
                                  const char **attr_value_array, size_t size) {
  auto *access =
      reinterpret_cast<opentelemetry::metrics::ObserverResultT<int64_t> *>(
          context);

  assert(access != nullptr);
  const MyMultiKeyValueIterable kv(attr_name_array, attr_value_array, size);
  access->Observe(value, kv);
}

void my_delivery_double_0_callback(void *context, double value) {
  auto *access =
      reinterpret_cast<opentelemetry::metrics::ObserverResultT<double> *>(
          context);

  assert(access != nullptr);
  access->Observe(value);
}

void my_delivery_double_1_callback(void *context, double value,
                                   const char *attr_name,
                                   const char *attr_value) {
  auto *access =
      reinterpret_cast<opentelemetry::metrics::ObserverResultT<double> *>(
          context);

  assert(access != nullptr);
  const MySingleKeyValueIterable kv(attr_name, attr_value);
  access->Observe(value, kv);
}

void my_delivery_double_n_callback(void *context, double value,
                                   const char **attr_name_array,
                                   const char **attr_value_array, size_t size) {
  auto *access =
      reinterpret_cast<opentelemetry::metrics::ObserverResultT<double> *>(
          context);

  assert(access != nullptr);
  const MyMultiKeyValueIterable kv(attr_name_array, attr_value_array, size);
  access->Observe(value, kv);
}

struct measurement_delivery_callback my_delivery_callback = {
    my_delivery_int64_0_callback,  my_delivery_int64_1_callback,
    my_delivery_int64_n_callback,  my_delivery_double_0_callback,
    my_delivery_double_1_callback, my_delivery_double_n_callback};

void MySQLMetric::callback(opentelemetry::metrics::ObserverResult result,
                           void *state) {
  auto *that = reinterpret_cast<MySQLMetric *>(state);
  assert(that != nullptr);

  that->observe(std::move(result));
}

MySQLMetric::MySQLMetric(opentelemetry::nostd::shared_ptr<
                             opentelemetry::metrics::ObservableInstrument>
                             otel_instrument,
                         measurement_callback_t metric_cb,
                         void *metric_cb_context)
    : m_otel_instrument(std::move(otel_instrument)),
      m_metric_cb(metric_cb),
      m_metric_cb_context(metric_cb_context),
      m_collecting(false) {
  mutex_srv->init(g_metric_mutex_key, &m_lock, nullptr, __FILE__, __LINE__);
}

MySQLMetric::~MySQLMetric() {
  removeCallback();
  mutex_srv->destroy(&m_lock, __FILE__, __LINE__);
}

void MySQLMetric::addCallback() {
  mutex_srv->lock(&m_lock, __FILE__, __LINE__);

  if (!m_collecting) {
    /*
      WARNING:

      As soon as the callback function is added to the otel instrument,
      otel can start collection and invoke the MySQLMetric::callback.

      MySQLMetric::callback will expect a fully constructed C++ instance
      in state.

      For this reason, the callback not added in MySQLMetric constructor,
      but added explicitly after with MySQLMetric::addCallback().
    */

    m_otel_instrument->AddCallback(MySQLMetric::callback, this);
    m_collecting = true;
  }

  mutex_srv->unlock(&m_lock, __FILE__, __LINE__);
}

void MySQLMetric::removeCallback() {
  mutex_srv->lock(&m_lock, __FILE__, __LINE__);

  if (m_collecting) {
    m_otel_instrument->RemoveCallback(MySQLMetric::callback, this);
    m_collecting = false;
  }

  mutex_srv->unlock(&m_lock, __FILE__, __LINE__);
}

void MySQLMetric::observe(opentelemetry::metrics::ObserverResult result) {
  mutex_srv->lock(&m_lock, __FILE__, __LINE__);

  if (m_collecting) {
    if (opentelemetry::nostd::holds_alternative<
            opentelemetry::nostd::shared_ptr<
                opentelemetry::metrics::ObserverResultT<int64_t>>>(result)) {
      auto access = opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::ObserverResultT<int64_t>>>(result);
      opentelemetry::metrics::ObserverResultT<int64_t> *access_ptr =
          access.get();

      m_metric_cb(m_metric_cb_context, &my_delivery_callback, access_ptr);
    } else if (opentelemetry::nostd::holds_alternative<
                   opentelemetry::nostd::shared_ptr<
                       opentelemetry::metrics::ObserverResultT<double>>>(
                   result)) {
      auto access = opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::ObserverResultT<double>>>(result);
      opentelemetry::metrics::ObserverResultT<double> *access_ptr =
          access.get();

      m_metric_cb(m_metric_cb_context, &my_delivery_callback, access_ptr);
    } else {
      assert(false);
    }
  }

  mutex_srv->unlock(&m_lock, __FILE__, __LINE__);
}

void MySQLMeter::createInt64ObservableCounter(const char *metric_name,
                                              const char *metric_desc,
                                              const char *metric_unit,
                                              measurement_callback_t metric_cb,
                                              void *metric_cb_context) {
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      instrument;

  instrument = m_otel_meter->CreateInt64ObservableCounter(
      metric_name, metric_desc, metric_unit);

  std::unique_ptr<MySQLMetric> metric(
      new MySQLMetric(instrument, metric_cb, metric_cb_context));

  metric->addCallback();

  m_metrics.push_back(std::move(metric));
}

void MySQLMeter::createInt64ObservableUpDownCounter(
    const char *metric_name, const char *metric_desc, const char *metric_unit,
    measurement_callback_t metric_cb, void *metric_cb_context) {
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      instrument;

  instrument = m_otel_meter->CreateInt64ObservableUpDownCounter(
      metric_name, metric_desc, metric_unit);

  std::unique_ptr<MySQLMetric> metric(
      new MySQLMetric(instrument, metric_cb, metric_cb_context));

  metric->addCallback();

  m_metrics.push_back(std::move(metric));
}

void MySQLMeter::createInt64ObservableGauge(const char *metric_name,
                                            const char *metric_desc,
                                            const char *metric_unit,
                                            measurement_callback_t metric_cb,
                                            void *metric_cb_context) {
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      instrument;

  instrument = m_otel_meter->CreateInt64ObservableGauge(
      metric_name, metric_desc, metric_unit);

  std::unique_ptr<MySQLMetric> metric(
      new MySQLMetric(instrument, metric_cb, metric_cb_context));

  metric->addCallback();

  m_metrics.push_back(std::move(metric));
}

void MySQLMeter::createDoubleObservableCounter(const char *metric_name,
                                               const char *metric_desc,
                                               const char *metric_unit,
                                               measurement_callback_t metric_cb,
                                               void *metric_cb_context) {
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      instrument;

  instrument = m_otel_meter->CreateDoubleObservableCounter(
      metric_name, metric_desc, metric_unit);

  std::unique_ptr<MySQLMetric> metric(
      new MySQLMetric(instrument, metric_cb, metric_cb_context));

  metric->addCallback();

  m_metrics.push_back(std::move(metric));
}

void MySQLMeter::createDoubleObservableUpDownCounter(
    const char *metric_name, const char *metric_desc, const char *metric_unit,
    measurement_callback_t metric_cb, void *metric_cb_context) {
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      instrument;

  instrument = m_otel_meter->CreateDoubleObservableUpDownCounter(
      metric_name, metric_desc, metric_unit);

  std::unique_ptr<MySQLMetric> metric(
      new MySQLMetric(instrument, metric_cb, metric_cb_context));

  metric->addCallback();

  m_metrics.push_back(std::move(metric));
}

void MySQLMeter::createDoubleObservableGauge(const char *metric_name,
                                             const char *metric_desc,
                                             const char *metric_unit,
                                             measurement_callback_t metric_cb,
                                             void *metric_cb_context) {
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      instrument;

  instrument = m_otel_meter->CreateDoubleObservableGauge(
      metric_name, metric_desc, metric_unit);

  std::unique_ptr<MySQLMetric> metric(
      new MySQLMetric(instrument, metric_cb, metric_cb_context));

  metric->addCallback();

  m_metrics.push_back(std::move(metric));
}

opentelemetry::metrics::MeterProvider *MySQLMeterProviders::get(
    size_t frequency) {
  /* Search in increasing frequency order */
  for (auto &e : m_providers) {
    if (frequency <= e.m_frequency) {
      return e.m_provider.get();
    }
  }

  return m_providers.back().m_provider.get();
}

void MySQLMeterProviders::add(
    size_t frequency,
    std::unique_ptr<opentelemetry::metrics::MeterProvider> provider) {
  m_providers.emplace_back(frequency, std::move(provider));
}

void MySQLMeterProviders::remove_meter(
    opentelemetry::nostd::string_view name,
    opentelemetry::nostd::string_view version,
    opentelemetry::nostd::string_view url) {
  /* We do not know the meter frequency, so remove in every partition. */
  for (auto &e : m_providers) {
    e.m_provider->RemoveMeter(name, version, url);
  }
}

void MySQLMeterProviders::reset() { m_providers.clear(); }

}  // namespace telemetry
