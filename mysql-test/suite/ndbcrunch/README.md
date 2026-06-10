# ndbcrunch benchmark suite

This test suite is used for benchmarking a MySQL Cluster with all nodes colocated on a single machine. The sysbench program is used for generating load.

The configuration defines a cluster with a configurable number of MySQL Servers, two data nodes and one mgm node. The suite is primarily intended to be used for benchmarking the MySQL Server + ndbcluster plugin performance.

## Prerequisites: Sysbench

This suite requires `sysbench` to be installed and available in your `PATH`.

### Installation

`sysbench` must be installed or built from source. You can find the repository and instructions here: [akopytov/sysbench](https://github.com/akopytov/sysbench).

Ensure the MySQL libraries are in your `LD_LIBRARY_PATH` when running tests if you use a custom build:

```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/mysql/lib
```

## Walkthrough

### 1. Smoke Test

To verify that the cluster setup is functioning correctly before running benchmarks, use the `check.test` script. This confirms that the NDB cluster and MySQL servers can start up and communicate.

```bash
mtr ndbcrunch.check
```

### 2. Run a Testcase

**Short Run Verification:**
Run with only one thread for 5 seconds to not spend too much time. This usually takes around 10-20 seconds total on a developer machine.

```bash
CRUNCH_TIME=5 CRUNCH_ROWS=1000 CRUNCH_THREADS=8 mtr ndbcrunch.crunch_update
```

**Standard Benchmark Run:**
To run a standard benchmark test with default parameters (30 seconds, 10M rows and geometric series of threads):

```bash
mtr ndbcrunch.crunch_update
```

### Test Output

The benchmark execution log (including sysbench output) is saved to the following location:
`var/log/report_<testname>.log`

For example: `var/log/report_crunch_update.log`

### 3. Run Testcase with Profiling

You can profile specific MySQL servers during the run using the `--perf` option. This is useful for identifying performance bottlenecks.

**Example: Profile `mysqld.2`**

```bash
# Run with profiling enabled
CRUNCH_THREADS=8 mtr --suite=ndbcrunch --no-check-testcases --shutdown-timeout=120 --perf=mysqld.2 crunch_update
```

- **`CRUNCH_THREADS=8`**: Limit to 8 threads, which is sufficient to generate enough load for meaningful profiling.
- **`--perf=mysqld.2`**: Attach `perf` to the `mysqld.2` process (configured as the Applier).
- **`--no-check-testcases`**: Skip post-run verification logic. This prevents timeouts and ensures that the profiling data reflects the benchmark load, not the validation checks.
- **`--shutdown-timeout=120`**: Increase the timeout to ensure the server has enough time to shut down gracefully and flush the `perf.data` file to disk.

### 4. Analyze Report

The profiling run will generate a `perf.data` file in the `var/log` directory (e.g., `var/log/mysqld.2.crunch.perf.data`). Use `perf report` to analyze it:

```bash
perf report -i var/log/mysqld.2.crunch.perf.data
```

## Configuration

The default configuration in `my.cnf` is tuned for a standard **Developer Machine** (e.g., 32GB RAM).

> [!NOTE]
> `DataMemory` is set to **512M** by default to prevent desktop OOM. This is sufficient for small test runs with `CRUNCH_ROWS` up to ~1M. For the default benchmark of 10M rows, you may need to increase `DataMemory` to 4G-8G, or reduce `CRUNCH_ROWS` to 100k-1M for quicker local tests.

### Lab Server Configuration (e.g., 1TB RAM)

For high-performance benchmarking on powerful hardware, use the provided `large.cnf` or uncomment the relevant sections in `my.cnf`.

**Using `large.cnf` (Recommended):**

```bash
mtr --suite=ndbcrunch --parallel=1 --defaults-extra-file=suite/ndbcrunch/large.cnf <testname>
```

**High-resource settings included in `large.cnf`:**

- `DataMemory=128G`
- `ndb_batch_size=256M`
- `ndb_blob_write_batch_bytes=256M`
- `LockPagesInMainMemory=1`

### CPU Binding (`cpubind.cnf`)

For consistent results on large multi-core (NUMA) machines, it is critical to bind processes to specific CPUs. The `cpubind.cnf` file provides an example configuration for this.

**Usage:**

```bash
mtr --suite=ndbcrunch --defaults-extra-file=suite/ndbcrunch/cpubind.cnf ...
```

This configuration binds:

- `ndbd.1`: CPUs 0-15, 128-143
- `ndbd.2`: CPUs 64-79, 192-207
- `mysqld.1` (Binlog): CPUs 16-63, 144-191
- `mysqld.2` (Applier): CPUs 80-127, 208-255

### Cluster Roles & Binlogging

The cluster is configured with specific roles for the MySQL servers:

- **`mysqld.1.crunch`**: Configured as the **Binlogging Server** (`ndb-log-bin=1`). It is excluded from receiving Sysbench load to isolate the binlogging performance.
- **`mysqld.2.crunch`**: Configured as the **Replica/Applier** (`log_replica_updates=OFF`). It is also excluded from Sysbench load.
- **`mysqld.3+`**: Any additional MySQL servers will be used by Sysbench to drive traffic.

### Adding More Load Drivers

To increase the load on the cluster, you can enable more MySQL servers to act as Sysbench clients.

1.  Add `mysqld` sections to your configuration (or `cpubind.cnf`).
2.  Update the `mysqld=` string in `[cluster_config]`.

Sysbench will automatically detect and use these new servers to distribute the load.

## Environment Variables

Parameters for sizing and configuration can be provided via environment variables:

| Variable         | Description                                   | Default      |
| :--------------- | :-------------------------------------------- | :----------- |
| `CRUNCH_ROWS`    | Number of rows per table                      | 10M          |
| `CRUNCH_TIME`    | Duration of the benchmark run                 | 30 seconds   |
| `CRUNCH_EVENTS`  | Number of events (0 = run until time elapsed) | 0            |
| `CRUNCH_THREADS` | Number of threads (0 = geometric series)      | 0            |
| `CRUNCH_AUTOINC` | Use auto-increment column                     | 0            |
| `CRUNCH_ENGINE`  | Storage engine                                | `ndbcluster` |

### Geometric Thread Series

By default (`CRUNCH_THREADS=0`), the test runs in a loop with an increasing number of threads: **1, 2, 4, 8, ... 1024**. This allows capturing the performance profile across different concurrency levels in a single run.

## Technical Details

### `sb_run.pl` Logic

The `sb_run.pl` script orchestrates the benchmark execution. Key behaviors include:

1.  **Server Exclusion**: It parses `my.cnf` and explicitly **excludes** any `mysqld` configured with `ndb-log-bin` (the binlogging server) from the Sysbench connection pool.
2.  **Applier Exclusion**: It also hardcodes the exclusion of `mysqld.2.crunch` (the replica applier).
3.  **Load Distribution**: All other configured MySQL servers are passed to Sysbench, which load balances connections across them.

## Advanced: Custom Lua Script `cruncher.lua`

The suite uses a custom sysbench Lua script, `cruncher.lua`, to control load generation. It supports several NDB-specific and benchmark options:

- **`table_nologging`**: Use `NOLOGGING` when creating tables (avoids REDO logging).
- **`blob_length`**: Length of the blob column (default 0).
- **`mysql_storage_engine`**: Storage engine to use (default `ndbcluster`).
- **`auto_inc`**: Use `AUTO_INCREMENT` for primary keys (default `false`).
- **`primary_keys`**: Number of primary keys (default 1).
- **`create_secondary`**: Create a secondary index (default `true`).

These are controlled via environment variables passed to the test runner.
