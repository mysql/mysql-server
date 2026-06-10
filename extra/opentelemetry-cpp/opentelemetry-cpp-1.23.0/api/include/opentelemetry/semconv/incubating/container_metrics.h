/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DO NOT EDIT, this is an Auto-generated file from:
 * buildscripts/semantic-convention/templates/registry/semantic_metrics-h.j2
 */

#pragma once

#include "opentelemetry/common/macros.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv
{
namespace container
{

/**
  Total CPU time consumed.
  <p>
  Total CPU time consumed by the specific container on all available CPU cores
  <p>
  counter
 */
static constexpr const char *kMetricContainerCpuTime     = "container.cpu.time";
static constexpr const char *descrMetricContainerCpuTime = "Total CPU time consumed.";
static constexpr const char *unitMetricContainerCpuTime  = "s";

static inline nostd::unique_ptr<metrics::Counter<uint64_t>> CreateSyncInt64MetricContainerCpuTime(
    metrics::Meter *meter)
{
  return meter->CreateUInt64Counter(kMetricContainerCpuTime, descrMetricContainerCpuTime,
                                    unitMetricContainerCpuTime);
}

static inline nostd::unique_ptr<metrics::Counter<double>> CreateSyncDoubleMetricContainerCpuTime(
    metrics::Meter *meter)
{
  return meter->CreateDoubleCounter(kMetricContainerCpuTime, descrMetricContainerCpuTime,
                                    unitMetricContainerCpuTime);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncInt64MetricContainerCpuTime(metrics::Meter *meter)
{
  return meter->CreateInt64ObservableCounter(kMetricContainerCpuTime, descrMetricContainerCpuTime,
                                             unitMetricContainerCpuTime);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncDoubleMetricContainerCpuTime(metrics::Meter *meter)
{
  return meter->CreateDoubleObservableCounter(kMetricContainerCpuTime, descrMetricContainerCpuTime,
                                              unitMetricContainerCpuTime);
}

/**
  Container's CPU usage, measured in cpus. Range from 0 to the number of allocatable CPUs.
  <p>
  CPU usage of the specific container on all available CPU cores, averaged over the sample window
  <p>
  gauge
 */
static constexpr const char *kMetricContainerCpuUsage = "container.cpu.usage";
static constexpr const char *descrMetricContainerCpuUsage =
    "Container's CPU usage, measured in cpus. Range from 0 to the number of allocatable CPUs.";
static constexpr const char *unitMetricContainerCpuUsage = "{cpu}";

#if OPENTELEMETRY_ABI_VERSION_NO >= 2

static inline nostd::unique_ptr<metrics::Gauge<int64_t>> CreateSyncInt64MetricContainerCpuUsage(
    metrics::Meter *meter)
{
  return meter->CreateInt64Gauge(kMetricContainerCpuUsage, descrMetricContainerCpuUsage,
                                 unitMetricContainerCpuUsage);
}

static inline nostd::unique_ptr<metrics::Gauge<double>> CreateSyncDoubleMetricContainerCpuUsage(
    metrics::Meter *meter)
{
  return meter->CreateDoubleGauge(kMetricContainerCpuUsage, descrMetricContainerCpuUsage,
                                  unitMetricContainerCpuUsage);
}
#endif /* OPENTELEMETRY_ABI_VERSION_NO */

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncInt64MetricContainerCpuUsage(metrics::Meter *meter)
{
  return meter->CreateInt64ObservableGauge(kMetricContainerCpuUsage, descrMetricContainerCpuUsage,
                                           unitMetricContainerCpuUsage);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncDoubleMetricContainerCpuUsage(metrics::Meter *meter)
{
  return meter->CreateDoubleObservableGauge(kMetricContainerCpuUsage, descrMetricContainerCpuUsage,
                                            unitMetricContainerCpuUsage);
}

/**
  Disk bytes for the container.
  <p>
  The total number of bytes read/written successfully (aggregated from all disks).
  <p>
  counter
 */
static constexpr const char *kMetricContainerDiskIo     = "container.disk.io";
static constexpr const char *descrMetricContainerDiskIo = "Disk bytes for the container.";
static constexpr const char *unitMetricContainerDiskIo  = "By";

static inline nostd::unique_ptr<metrics::Counter<uint64_t>> CreateSyncInt64MetricContainerDiskIo(
    metrics::Meter *meter)
{
  return meter->CreateUInt64Counter(kMetricContainerDiskIo, descrMetricContainerDiskIo,
                                    unitMetricContainerDiskIo);
}

static inline nostd::unique_ptr<metrics::Counter<double>> CreateSyncDoubleMetricContainerDiskIo(
    metrics::Meter *meter)
{
  return meter->CreateDoubleCounter(kMetricContainerDiskIo, descrMetricContainerDiskIo,
                                    unitMetricContainerDiskIo);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncInt64MetricContainerDiskIo(metrics::Meter *meter)
{
  return meter->CreateInt64ObservableCounter(kMetricContainerDiskIo, descrMetricContainerDiskIo,
                                             unitMetricContainerDiskIo);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncDoubleMetricContainerDiskIo(metrics::Meter *meter)
{
  return meter->CreateDoubleObservableCounter(kMetricContainerDiskIo, descrMetricContainerDiskIo,
                                              unitMetricContainerDiskIo);
}

/**
  Container filesystem available bytes.
  <p>
  In K8s, this metric is derived from the
  <a
  href="https://pkg.go.dev/k8s.io/kubelet@v0.33.0/pkg/apis/stats/v1alpha1#FsStats">FsStats.AvailableBytes</a>
  field of the <a
  href="https://pkg.go.dev/k8s.io/kubelet@v0.33.0/pkg/apis/stats/v1alpha1#ContainerStats">ContainerStats.Rootfs</a>
  of the Kubelet's stats API.
  <p>
  updowncounter
 */
static constexpr const char *kMetricContainerFilesystemAvailable = "container.filesystem.available";
static constexpr const char *descrMetricContainerFilesystemAvailable =
    "Container filesystem available bytes.";
static constexpr const char *unitMetricContainerFilesystemAvailable = "By";

static inline nostd::unique_ptr<metrics::UpDownCounter<int64_t>>
CreateSyncInt64MetricContainerFilesystemAvailable(metrics::Meter *meter)
{
  return meter->CreateInt64UpDownCounter(kMetricContainerFilesystemAvailable,
                                         descrMetricContainerFilesystemAvailable,
                                         unitMetricContainerFilesystemAvailable);
}

static inline nostd::unique_ptr<metrics::UpDownCounter<double>>
CreateSyncDoubleMetricContainerFilesystemAvailable(metrics::Meter *meter)
{
  return meter->CreateDoubleUpDownCounter(kMetricContainerFilesystemAvailable,
                                          descrMetricContainerFilesystemAvailable,
                                          unitMetricContainerFilesystemAvailable);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncInt64MetricContainerFilesystemAvailable(metrics::Meter *meter)
{
  return meter->CreateInt64ObservableUpDownCounter(kMetricContainerFilesystemAvailable,
                                                   descrMetricContainerFilesystemAvailable,
                                                   unitMetricContainerFilesystemAvailable);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncDoubleMetricContainerFilesystemAvailable(metrics::Meter *meter)
{
  return meter->CreateDoubleObservableUpDownCounter(kMetricContainerFilesystemAvailable,
                                                    descrMetricContainerFilesystemAvailable,
                                                    unitMetricContainerFilesystemAvailable);
}

/**
  Container filesystem capacity.
  <p>
  In K8s, this metric is derived from the
  <a
  href="https://pkg.go.dev/k8s.io/kubelet@v0.33.0/pkg/apis/stats/v1alpha1#FsStats">FsStats.CapacityBytes</a>
  field of the <a
  href="https://pkg.go.dev/k8s.io/kubelet@v0.33.0/pkg/apis/stats/v1alpha1#ContainerStats">ContainerStats.Rootfs</a>
  of the Kubelet's stats API.
  <p>
  updowncounter
 */
static constexpr const char *kMetricContainerFilesystemCapacity = "container.filesystem.capacity";
static constexpr const char *descrMetricContainerFilesystemCapacity =
    "Container filesystem capacity.";
static constexpr const char *unitMetricContainerFilesystemCapacity = "By";

static inline nostd::unique_ptr<metrics::UpDownCounter<int64_t>>
CreateSyncInt64MetricContainerFilesystemCapacity(metrics::Meter *meter)
{
  return meter->CreateInt64UpDownCounter(kMetricContainerFilesystemCapacity,
                                         descrMetricContainerFilesystemCapacity,
                                         unitMetricContainerFilesystemCapacity);
}

static inline nostd::unique_ptr<metrics::UpDownCounter<double>>
CreateSyncDoubleMetricContainerFilesystemCapacity(metrics::Meter *meter)
{
  return meter->CreateDoubleUpDownCounter(kMetricContainerFilesystemCapacity,
                                          descrMetricContainerFilesystemCapacity,
                                          unitMetricContainerFilesystemCapacity);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncInt64MetricContainerFilesystemCapacity(metrics::Meter *meter)
{
  return meter->CreateInt64ObservableUpDownCounter(kMetricContainerFilesystemCapacity,
                                                   descrMetricContainerFilesystemCapacity,
                                                   unitMetricContainerFilesystemCapacity);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncDoubleMetricContainerFilesystemCapacity(metrics::Meter *meter)
{
  return meter->CreateDoubleObservableUpDownCounter(kMetricContainerFilesystemCapacity,
                                                    descrMetricContainerFilesystemCapacity,
                                                    unitMetricContainerFilesystemCapacity);
}

/**
  Container filesystem usage.
  <p>
  This may not equal capacity - available.
  <p>
  In K8s, this metric is derived from the
  <a
  href="https://pkg.go.dev/k8s.io/kubelet@v0.33.0/pkg/apis/stats/v1alpha1#FsStats">FsStats.UsedBytes</a>
  field of the <a
  href="https://pkg.go.dev/k8s.io/kubelet@v0.33.0/pkg/apis/stats/v1alpha1#ContainerStats">ContainerStats.Rootfs</a>
  of the Kubelet's stats API.
  <p>
  updowncounter
 */
static constexpr const char *kMetricContainerFilesystemUsage     = "container.filesystem.usage";
static constexpr const char *descrMetricContainerFilesystemUsage = "Container filesystem usage.";
static constexpr const char *unitMetricContainerFilesystemUsage  = "By";

static inline nostd::unique_ptr<metrics::UpDownCounter<int64_t>>
CreateSyncInt64MetricContainerFilesystemUsage(metrics::Meter *meter)
{
  return meter->CreateInt64UpDownCounter(kMetricContainerFilesystemUsage,
                                         descrMetricContainerFilesystemUsage,
                                         unitMetricContainerFilesystemUsage);
}

static inline nostd::unique_ptr<metrics::UpDownCounter<double>>
CreateSyncDoubleMetricContainerFilesystemUsage(metrics::Meter *meter)
{
  return meter->CreateDoubleUpDownCounter(kMetricContainerFilesystemUsage,
                                          descrMetricContainerFilesystemUsage,
                                          unitMetricContainerFilesystemUsage);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncInt64MetricContainerFilesystemUsage(metrics::Meter *meter)
{
  return meter->CreateInt64ObservableUpDownCounter(kMetricContainerFilesystemUsage,
                                                   descrMetricContainerFilesystemUsage,
                                                   unitMetricContainerFilesystemUsage);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncDoubleMetricContainerFilesystemUsage(metrics::Meter *meter)
{
  return meter->CreateDoubleObservableUpDownCounter(kMetricContainerFilesystemUsage,
                                                    descrMetricContainerFilesystemUsage,
                                                    unitMetricContainerFilesystemUsage);
}

/**
  Memory usage of the container.
  <p>
  Memory usage of the container.
  <p>
  counter
 */
static constexpr const char *kMetricContainerMemoryUsage     = "container.memory.usage";
static constexpr const char *descrMetricContainerMemoryUsage = "Memory usage of the container.";
static constexpr const char *unitMetricContainerMemoryUsage  = "By";

static inline nostd::unique_ptr<metrics::Counter<uint64_t>>
CreateSyncInt64MetricContainerMemoryUsage(metrics::Meter *meter)
{
  return meter->CreateUInt64Counter(kMetricContainerMemoryUsage, descrMetricContainerMemoryUsage,
                                    unitMetricContainerMemoryUsage);
}

static inline nostd::unique_ptr<metrics::Counter<double>>
CreateSyncDoubleMetricContainerMemoryUsage(metrics::Meter *meter)
{
  return meter->CreateDoubleCounter(kMetricContainerMemoryUsage, descrMetricContainerMemoryUsage,
                                    unitMetricContainerMemoryUsage);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncInt64MetricContainerMemoryUsage(metrics::Meter *meter)
{
  return meter->CreateInt64ObservableCounter(
      kMetricContainerMemoryUsage, descrMetricContainerMemoryUsage, unitMetricContainerMemoryUsage);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncDoubleMetricContainerMemoryUsage(metrics::Meter *meter)
{
  return meter->CreateDoubleObservableCounter(
      kMetricContainerMemoryUsage, descrMetricContainerMemoryUsage, unitMetricContainerMemoryUsage);
}

/**
  Network bytes for the container.
  <p>
  The number of bytes sent/received on all network interfaces by the container.
  <p>
  counter
 */
static constexpr const char *kMetricContainerNetworkIo     = "container.network.io";
static constexpr const char *descrMetricContainerNetworkIo = "Network bytes for the container.";
static constexpr const char *unitMetricContainerNetworkIo  = "By";

static inline nostd::unique_ptr<metrics::Counter<uint64_t>> CreateSyncInt64MetricContainerNetworkIo(
    metrics::Meter *meter)
{
  return meter->CreateUInt64Counter(kMetricContainerNetworkIo, descrMetricContainerNetworkIo,
                                    unitMetricContainerNetworkIo);
}

static inline nostd::unique_ptr<metrics::Counter<double>> CreateSyncDoubleMetricContainerNetworkIo(
    metrics::Meter *meter)
{
  return meter->CreateDoubleCounter(kMetricContainerNetworkIo, descrMetricContainerNetworkIo,
                                    unitMetricContainerNetworkIo);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncInt64MetricContainerNetworkIo(metrics::Meter *meter)
{
  return meter->CreateInt64ObservableCounter(
      kMetricContainerNetworkIo, descrMetricContainerNetworkIo, unitMetricContainerNetworkIo);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncDoubleMetricContainerNetworkIo(metrics::Meter *meter)
{
  return meter->CreateDoubleObservableCounter(
      kMetricContainerNetworkIo, descrMetricContainerNetworkIo, unitMetricContainerNetworkIo);
}

/**
  The time the container has been running.
  <p>
  Instrumentations SHOULD use a gauge with type @code double @endcode and measure uptime in seconds
  as a floating point number with the highest precision available. The actual accuracy would depend
  on the instrumentation and operating system. <p> gauge
 */
static constexpr const char *kMetricContainerUptime = "container.uptime";
static constexpr const char *descrMetricContainerUptime =
    "The time the container has been running.";
static constexpr const char *unitMetricContainerUptime = "s";

#if OPENTELEMETRY_ABI_VERSION_NO >= 2

static inline nostd::unique_ptr<metrics::Gauge<int64_t>> CreateSyncInt64MetricContainerUptime(
    metrics::Meter *meter)
{
  return meter->CreateInt64Gauge(kMetricContainerUptime, descrMetricContainerUptime,
                                 unitMetricContainerUptime);
}

static inline nostd::unique_ptr<metrics::Gauge<double>> CreateSyncDoubleMetricContainerUptime(
    metrics::Meter *meter)
{
  return meter->CreateDoubleGauge(kMetricContainerUptime, descrMetricContainerUptime,
                                  unitMetricContainerUptime);
}
#endif /* OPENTELEMETRY_ABI_VERSION_NO */

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncInt64MetricContainerUptime(metrics::Meter *meter)
{
  return meter->CreateInt64ObservableGauge(kMetricContainerUptime, descrMetricContainerUptime,
                                           unitMetricContainerUptime);
}

static inline nostd::shared_ptr<metrics::ObservableInstrument>
CreateAsyncDoubleMetricContainerUptime(metrics::Meter *meter)
{
  return meter->CreateDoubleObservableGauge(kMetricContainerUptime, descrMetricContainerUptime,
                                            unitMetricContainerUptime);
}

}  // namespace container
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
