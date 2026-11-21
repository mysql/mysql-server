# Benchmark Results

To quantify the improvement of the in-memory relaylog, we benchmarked replication throughput for different stages (receiver, applier, and end-to-end) and compared the current CSA applier against CSA with the in-memory relaylog enabled. The test environment and results are below.

## Environment

| Setting          | Value           |
| ---------------- | --------------- |
| Testing platform | AWS             |
| Instance type    | m7a.16xlarge    |
| Disk type        | io2             |
| Benchmark tool   | Sysbench        |
| Workload         | oltp_write_only |
| Parallel workers | 128             |

## Results

Throughput in MB/s (higher is better).

| Stage      | CSA applier (MB/s) | CSA + In-memory relaylog (MB/s) |
| ---------- | -----------------: | ------------------------------: |
| Receiver   |                 80 |                             230 |
| Applier    |                 66 |                              67 |
| End-to-end |                 44 |                              65 |
