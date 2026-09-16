\page PageLibsMysqlScheduler MySQL Scheduler Library

<!---
Copyright (c) 2026, Oracle and/or its affiliates.
//
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.
//
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
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
-->

# MySQL Scheduler Library

## Overview

The MySQL Scheduler Library (`mysql_scheduler`) provides task scheduling,
dependency tracking, worker thread execution, task registries, and sharded
statistics used by MySQL server components such as Change Stream Apply (CSA).
Tasks become runnable when their schedule is ready and their dependency
tracker reports that predecessor work has completed.

The library supports standalone builds and MySQL server integration. Server
builds use MySQL synchronization wrappers and Performance Schema
Instrumentation (PSI); standalone builds use the STL-backed wrappers from
`mysql_concurrency`.

## Key Features

- **Task Scheduling**: Manages tasks with clock-based schedules and optional
  predecessor dependencies.
- **Dependency Management**: Supports end-to-start dependencies through the
  dependency tracker interface.
- **Phase-Based Execution**: Supports transaction schedules with an apply
  phase and a commit/order phase.
- **Thread Pool**: Provides a configurable worker pool for task
  execution.
- **Task Registries**: Provides concurrent task registries used by clocks,
  dependency trackers, and scheduler internals.
- **Statistics Monitoring**: Tracks sharded `long long` counters across
  threads and scheduler instances.
- **MySQL Integration**: Supports server-specific PSI keys and standalone
  compilation.

## Architecture

The MySQL Scheduler Library is organized into several key components:

### Scheduler

The main component responsible for managing and executing tasks. It coordinates
task execution based on schedule readiness, scheduler clocks, dependency
trackers, and thread pool capacity.

### Task

A callable unit of work. The scheduler invokes tasks with the worker thread id
as the first argument, followed by any arguments supplied to `enqueue()` or
`enqueue_after()`.

### Scheduler Clock

Provides the clock interface used by schedules. `Clock_lwm_registry` advances
using completed task identifiers and is suitable for low-water-mark ordering.
`Commit_order_clock` is used for commit sequencing in transaction schedules.

### Dependency Tracker

Monitors task dependencies and reports when a task can run. The library
includes the dependency tracker interface and `Dependency_tracker_stub` for
tests/no-op dependency checks. Components that need dependency-aware
scheduling should provide a tracker implementation for their dependency model.
`Dependency_tracker_single_predecessor` is an example implementation used by
scheduler gunit support code; it is not linked into the production scheduler
library.

### Thread Pool

Manages a pool of worker threads that execute scheduled tasks
asynchronously.

### Task Registries

Provides concurrent registries for task state. `Task_registry` stores a
single-entry-per-id mapping, while `Task_registry_multi` uses striped storage
to reduce contention when independent task ids are managed concurrently.

### Statistics Monitor

Tracks `Sharded_counter` values for named statistics across threads and
instances. Counters support per-thread values, coalesced totals, direct
increments, direct stores, and timer-style accumulation with `start_time()`,
`stop_time()`, and `get_timer()`.

## Core Components

### Scheduler Class

The central component that:

- Accepts tasks with defined schedules and dependencies
- Checks readiness based on timing and dependencies
- Dispatches ready tasks to the thread pool
- Handles callbacks on task completion to update clocks and dependencies

### Task Schedules

Define when and how tasks execute:

- **Delayed Schedule**: Single-phase execution after a delay
- **Transaction Order Schedule**: Multi-phase execution for commit order
  preservation

### Clocks

- **Clock LWM Registry**: Advances a low-water mark as registered task ids
  complete.
- **Commit Order Clock**: Coordinates ordered commit phases.

### Dependency Trackers

- **Dependency Tracker Stub**: Minimal no-op implementation for tests or
  dependency-free scheduling.
- **Base Dependency Tracker**: Interface for components that need to enforce
  dependency-aware scheduling.

### Task Registries

- **Task Registry**: Stores task state by task id.
- **Task Registry Multi**: Stores task state across striped buckets to reduce
  contention.

### Sharded Counter

- **Sharded Counter**: Stores one `long long` counter per worker thread and
  coalesces the values on demand. It can be used either as a plain counter or
  as a timer accumulator.

## Usage Examples

### Basic Task Scheduling

```cpp
#include <memory>
#include <utility>

#include "mysql/scheduler/clock_lwm_registry.h"
#include "mysql/scheduler/dependency_tracker_stub.h"
#include "mysql/scheduler/schedule_factory.h"
#include "mysql/scheduler/scheduler.h"
#include "mysql/scheduler/task_result.h"
#include "mysql/scheduler/task_sequencer.h"
#include "mysql/scheduler/thread_pool.h"

using namespace mysql::scheduler;

const unsigned int worker_count = 4;
const int instance_id = 0;

auto thread_pool = std::make_shared<Thread_pool<Task_result>>(worker_count,
                                                              instance_id);
if (thread_pool->init()) {
  // Handle worker thread creation failure.
}
auto clock = std::make_shared<Clock_lwm_registry>();
auto dependencies = std::make_unique<Dependency_tracker_stub>();
Scheduler scheduler(thread_pool, clock, std::move(dependencies), instance_id);

Task_sequencer sequencer;
Schedule_factory schedules(clock);

auto task_id = sequencer.next_id();
auto task = []([[maybe_unused]] unsigned int thread_id) {
  // Task logic here.
};

auto schedule = schedules.create(task_id, 0);
scheduler.enqueue(schedule, task);

scheduler.synchronize();
scheduler.deinit();
thread_pool->deinit();
```

### Task with Dependency Registration

```cpp
class My_dependency_tracker : public mysql::scheduler::Base_dependency_tracker {
  // Implement the dependency policy required by the component.
};

auto dependencies = std::make_unique<My_dependency_tracker>();
mysql::scheduler::Scheduler scheduler(
    thread_pool, clock, std::move(dependencies), instance_id);

auto task_a_id = sequencer.next_id();
auto task_b_id = sequencer.next_id();

scheduler.enqueue(schedules.create(task_a_id, 0), task_a);
scheduler.enqueue_after(task_a_id, schedules.create(task_b_id, 0), task_b);
```

### Multi-Phase Task Execution

```cpp
#include "mysql/scheduler/commit_order_clock.h"

auto apply_clock = std::make_shared<Clock_lwm_registry>();
auto commit_clock = std::make_shared<Commit_order_clock>();

Schedule_factory trx_schedules(apply_clock, commit_clock);
scheduler.register_phase(commit_clock);

auto task_id = sequencer.next_id();
auto trx_schedule = trx_schedules.create(task_id, 0, true);
scheduler.enqueue(trx_schedule, multi_phase_task);
```

### Statistics Monitoring

```cpp
#include <mysql/scheduler/statistics_monitor.h>

auto &monitor = mysql::scheduler::Statistics_monitor::get(instance_id);
monitor.register_stat("scheduled_transactions", worker_count);

monitor.get_stat("scheduled_transactions").add(1, thread_id);

auto scheduled_transactions =
    monitor.get_stat("scheduled_transactions").get();
```

### Timer Statistics

```cpp
#include <mysql/scheduler/statistics_monitor.h>

auto &monitor = mysql::scheduler::Statistics_monitor::get(instance_id);
monitor.register_stat("task_exec_time", worker_count);

auto &task_exec_time = monitor.get_stat("task_exec_time");
task_exec_time.start_time(thread_id);
// Timed task logic here.
task_exec_time.stop_time(thread_id);

auto total_exec_time_us = task_exec_time.get_timer();
```

## Performance Considerations

- **Atomic Operations**: Clocks, queues, counters, and registry paths use
  atomics where appropriate.
- **Striped Registries**: Task registries reduce contention across buckets.
- **Per-Slot Queue Synchronization**: Thread pool queues use bounded queues
  with atomic index allocation and per-slot locking.
- **Memory Alignment**: Proper alignment of atomic variables for optimal cache
  performance.
- **Thread Pool Sizing**: Configure worker thread count based on workload
  characteristics and hardware capabilities.
- **Clock Selection**: Choose appropriate clock implementations based on
  scheduling requirements.

## Integration with MySQL

When compiled within the MySQL server:

- Integrates with MySQL's PSI (Performance Schema Instrumentation).
- Supports server-specific threading models and resource management.

For standalone usage:

- Relies on STL implementations.
- Compatible with standard C++ threading libraries.

## Building and Installation

The library is typically built as part of the MySQL server compilation process.
For standalone usage:

1. Include the appropriate headers based on your compilation environment.
2. Link against the compiled library.
3. Define `STANDALONE_LIBS_MYSQL` for standalone builds.

### Dependencies

- C++11 or later
- Atomic operations support (built-in or std::atomic)
- MySQL headers (for server variant)

## Testing

The library includes comprehensive unit tests covering:

- Task scheduling correctness
- Dependency resolution validation
- Clock implementations
- Thread pool behavior
- Statistics monitoring accuracy
- Concurrent access thread safety

Run tests using the MySQL test suite or standalone test executables.

## Code Documentation

For detailed code documentation, please refer to @ref GroupLibsMysqlScheduler.
