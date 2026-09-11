# Public Benchmark Results

We benchmarked BOLT and validated its performance across commit latency, throughput, and recovery.

## Experiment 1: Commit Latency

This experiment validates BOLT's impact on large transaction commit latency.

### EC2 Setup

- HDD-backed host: r7i.xlarge with a 1 TiB st1 HDD EBS volume for MySQL data and binary logs. The InnoDB buffer pool is 12 GiB.
- SSD-backed host: r7i.xlarge with a 1 TiB gp3 SSD EBS volume for MySQL data and binary logs. The InnoDB buffer pool is 12 GiB.

### MySQL Params Setup

- `log_bin=ON`
- `binlog_format=ROW`
- `sync_binlog=1`
- `binlog_transaction_compression=OFF`
- `binlog_large_transaction_optimization_enabled` is set to ON or OFF before each test.

### Workload Setup

- Large transaction: one transaction inserts 8 MiB `LONGBLOB` chunks into a dedicated table and commits once.
- Payload sizes are 1 GiB, 5 GiB, 10 GiB, and 50 GiB.
- Concurrent Sysbench workload: 1,500 threads against 500 tables with 100,000 rows per table (50 million rows total).

### Results

BOLT also affects overall system throughput, but here we share only the commit-latency results for the large transactions. The results are in seconds and are averaged over three runs.

| Storage | BOLT | 1 GiB | 5 GiB | 10 GiB | 50 GiB |
| --- | --- | --- | --- | --- | --- |
| HDD | ON | 3.325s | 3.081s | 3.204s | 2.741s |
| HDD | OFF | 17.473s | 92.322s | 202.495s | 1091.222s |
| SSD | ON | 2.253s | 2.351s | 2.215s | 2.419s |
| SSD | OFF | 9.940s | 44.067s | 88.983s | 435.713s |

The table shows that BOLT makes commit latency effectively independent of transaction size. With BOLT ON, latency stays in the 2s to 3.5s range across all payload sizes. With BOLT OFF, latency grows roughly linearly with the transaction. The improvement widens as transactions grow. On SSD it goes from about 4x at 1 GiB to about 180x at 50 GiB. On HDD it goes from about 5x at 1 GiB to about 400x at 50 GiB.

## Experiment 2: Binlog Recovery

This experiment validates BOLT's impact on binlog crash recovery time for large transactions. The setup is the same as Experiment 1, but here we force recovery of a large transaction on restart.

BOLT writes a padding event (`Large_transaction_header_event`) that records the offset of the end of a large transaction. On restart, the fast recovery path reads this offset and jumps directly to the end of the transaction instead of scanning the entire transaction body. Recovery time therefore becomes nearly independent of transaction size. With BOLT OFF, recovery falls back to the old path and scans the full transaction body.

Note that we measure how long the binlog recovery stage takes. We do not measure how long it takes to roll back or roll forward prepared transactions in InnoDB.

| Storage | BOLT | 1 GiB | 5 GiB | 10 GiB | 50 GiB |
| --- | --- | --- | --- | --- | --- |
| HDD | ON | 0.22s | 0.21s | 0.19s | 0.23s |
| HDD | OFF | 14.6s | 76.9s | 168.7s | 909.4s |
| SSD | ON | 0.09s | 0.11s | 0.12s | 0.11s |
| SSD | OFF | 5.68s | 25.2s | 50.8s | 249s |

BOLT recovery is consistent across all transaction sizes. It does not read the transaction body and only reads the first and last event, which makes recovery constant time. This yields large recovery improvements and saves tens of minutes in availability time.

## Experiment 3: Write Throughput

This experiment validates BOLT's impact on write throughput when large transactions are used.

### EC2 Setup

Single host: r7i.16xlarge with a 1 TiB gp3 EBS volume (3,000 IOPS baseline, 125 MiB/s baseline throughput). `innodb_dedicated_server=ON`.

### MySQL Params Setup

- `log_bin=ON`
- `binlog_format=ROW`
- `sync_binlog=1`
- `binlog_transaction_compression=OFF`
- `binlog_large_transaction_optimization_enabled` is set to ON or OFF before each test.

### Workload Setup

A custom Sysbench workload inserts `LONGBLOB` payloads in 8 MiB chunks (matching Experiment 1) into one of 500 shard tables per transaction, then commits. Spreading writes across 500 tables avoids the lock contention that a single table would create among writer threads. We modified the scripts to control both the transaction size and the mix of sizes. For example, a run can be 100% 128 MB transactions, or a mix such as 20% 5 GB, 30% 128 MB, and 50% 20 KB.

Across all experiments, we use 50 threads and run for 30 minutes.

### Results

#### Sub-Experiment 1: Large-Sized Transaction Workload

We measure the performance improvement for a workload made up only of large transactions.

| BOLT | Elapsed (s) | 5 GB Trxs Count | Throughput (MB/s) |
| --- | --- | --- | --- |
| ON | 1810 | 22 | 60.8 |
| OFF | 1810 | 7 | 19.3 |

When BOLT is off, the system is limited by latency. As described in Experiment 1, during the transaction commit the transaction blocks all other transactions from committing. BOLT does not have this bottleneck, so it is instead limited by IO throughput. This yields roughly a 3x throughput improvement.

#### Sub-Experiment 2: Medium-Sized Transactions Workload

We measure the performance penalty that BOLT may add for transactions that spill over but are not promoted through BOLT. This is an edge case for BOLT because it computes the checksum for the same transaction twice: once when spilling over (introduced by BOLT) and once when copying back to the binlog file. CRC computation is highly optimized, but we still measure BOLT's impact in this scenario. Here the BOLT optimization is not enabled, since it only works when the threshold is above 128 MB.

| BOLT | Elapsed (s) | 64 MB Trxs Count | Throughput (MB/s) |
| --- | --- | --- | --- |
| ON | 1810 | 482 | 17.0 |
| OFF | 1810 | 468 | 16.5 |

As expected, there is no meaningful difference between ON and OFF. The CRC computation adds negligible overhead.

#### Sub-Experiment 3: Mixed-Sized Transactions Workload

We measure the performance improvement for a workload made up of different transaction sizes. This workload is 10% huge (5 GB), 30% medium (256 MB), 40% small (12 MB), and 20% tiny (5 KB).

| BOLT | Elapsed (s) | 5 GB Trxs Count | 256 MB Trxs Count | 12 MB Trxs Count | 5 KB Trxs Count | Throughput (MB/s) |
| --- | --- | --- | --- | --- | --- | --- |
| ON | 1810 | 18 | 42 | 68 | 42 | 56.1 |
| OFF | 1810 | 6 | 22 | 38 | 16 | 19.9 |

BOLT commits the 5 GB and 256 MB transactions faster and with very low commit latency, which allows more concurrency and higher parallelization. As a result, system throughput improves by about 2.8x.

#### Sub-Experiment 4: Small-Sized Transactions Workload

We measure the performance penalty that BOLT may add for computing the CRC checksum for events written into the binlog `IO_cache`. With BOLT enabled, events are written into memory with checksum ON, while if BOLT is disabled, events are written into cache without computing a checksum; the checksum is then computed again when copied into the binlog file. We don't anticipate this to introduce any performance penalty, as CRC computation is fast and the bottleneck on the binlog lies in the commit codepath, not the write. This experiment runs Sysbench without any modifications, using normal-sized binlog transactions.

| BOLT | Elapsed (s) | Throughput (MB/s) |
| --- | --- | --- |
| ON | 1810 | 23.6 |
| OFF | 1810 | 23.7 |

As expected, there is no meaningful difference between ON and OFF. The CRC computation adds no overhead.

#### Sub-Experiment 5: Medium-Sized Transactions Workload

BOLT also writes the CRC when the file is spilled to disk. This experiment is designed to validate the overhead for transactions that spill over to disk but are not committed through the BOLT codepath. This experiment runs a modified Sysbench workload with 12 MB transaction sizes.

| BOLT | Elapsed (s) | 12 MB Trxs Count | Throughput (MB/s) |
| --- | --- | --- | --- |
| ON | 1810 | 472 | 16.6 |
| OFF | 1810 | 468 | 16.5 |

Similar to Sub-Experiment 4, there is no overhead.

---