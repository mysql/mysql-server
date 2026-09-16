\page PageLibsMysqlConcurrency MySQL Concurrency Library

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

# MySQL Concurrency Library

## Overview

The MySQL Concurrency Library (`mysql_concurrency`) is a C++ library designed
to provide synchronization wrappers and concurrent queue primitives for
building MySQL components that can be compiled either inside the server or as
standalone libraries. It is part of the MySQL server codebase and supports
both standalone usage and integration within the MySQL ecosystem.

The library provides wrappers that abstract synchronization APIs, offering
a consistent interface for maintainability across different
environments (STL or MySQL-specific). It includes mutexes, condition
variables, threads, spin locks, stage instrumentation helpers, and
producer-consumer queues.

## Key Features

- **Synchronization Primitives**: Thread-safe wrappers for mutexes, condition
  variables, threads, and short critical-section spin locks.
- **Concurrent Queues**: Bounded and locking FIFO queues for
  producer-consumer scenarios.
- **Performance Monitoring**: Integration with MySQL's Performance Schema for
  server builds.
- **Cross-Platform Compatibility**: Supports both standalone compilation and
  MySQL server integration.
- **Thread Safety**: Components provide explicit synchronization semantics for
  concurrent access.

## Architecture

The library is organized into two main variants:

- **`mysql_concurrency`**: Standalone library for use outside the MySQL server.
- **`mysql_concurrency_server`**: Server-specific variant with additional MySQL
  server integrations and underlying mysys library.

If `STANDALONE_LIBS_MYSQL` is not defined when compiling the headers, link
with `mysql_concurrency_server`, which uses `mysql_mutex_t`, `mysql_cond_t`,
and MySQL thread wrappers internally and provides Performance Schema
instrumentation. If `STANDALONE_LIBS_MYSQL` is defined, link with
`mysql_concurrency`, which uses STL synchronization primitives and does not
provide instrumentation.

### Core Components

#### Synchronization Wrappers

- **Thread Wrappers**: Abstractions around threading APIs.
- **Mutex Wrappers**: Thread-safe locking mechanisms with MySQL mutex
  integration.
- **Condition Variable Wrappers**: Signaling primitives for thread coordination.
- **Spin Lock Mutex**: Low-latency spin-based locking for short critical
  sections.
- **Stage Helpers**: Lightweight wrappers for server-side stage
  instrumentation.

#### Data Structures

- **Sync Bounded Queue**: Fixed-capacity, thread-safe FIFO queue. It allocates
  producer and consumer indexes atomically and protects element slots with
  per-slot synchronization.
- **Locking Queue**: Unbounded FIFO queue protected by one mutex and one
  condition variable.
- **Padded Slot**: Cache-line-aware storage helper used by bounded queues.

## Usage Examples

### Basic Mutex Usage

```cpp
#include <mutex>

#include <mysql/concurrency/mutex.h>

mysql::concurrency::Mutex mutex{MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(0)};

// In thread 1
{
  std::unique_lock<decltype(mutex)> lock(mutex);
  // Critical section.
}

// In thread 2
{
  std::unique_lock<decltype(mutex)> lock(mutex);
  // Critical section.
}
```

### Bounded Queue

```cpp
#include <mysql/concurrency/sync_bounded_queue.h>

mysql::concurrency::Sync_bounded_queue<int, 128> queue;

// Producer thread
queue.enqueue(42);

// Consumer thread
auto [value, ok] = queue.dequeue([] { return false; });
if (ok) {
  // Use value.
}
```

### Locking Queue

```cpp
#include <mysql/concurrency/locking_queue.h>

mysql::concurrency::Locking_queue<int> queue;

queue.enqueue(7);

auto [value, ok] = queue.dequeue([] { return false; });
```

## Performance Considerations

- **Atomic Index Allocation**: `Sync_bounded_queue` uses atomic head and tail
  index allocation before synchronizing individual slots.
- **Per-Slot Synchronization**: Bounded queues reduce contention by locking
  only the affected slot.
- **Memory Alignment**: Proper alignment of atomic variables for optimal
  performance.
- **Spin Locks**: Suitable for short critical sections; avoid for long
  operations.
- **Queue Sizing**: Choose appropriate queue capacities based on
  producer-consumer patterns.

## Integration with MySQL

When compiled within the MySQL server (`mysql_concurrency_server`):

- Integrates with MySQL's PSI (Performance Schema Instrumentation).
- Uses MySQL-specific synchronization primitives (`mysql_mutex_t`,
  `mysql_cond_t`).
- Supports server-specific threading models and resource management.

For standalone usage (`mysql_concurrency`):

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

- Synchronization primitive correctness
- Concurrent data structure thread safety
- Queue behavior under producer-consumer workloads
- Spin lock behavior

Run tests using the MySQL test suite or standalone test executables.

## Code documentation

For code documentation, please refer to @ref GroupLibsMysqlConcurrency.
