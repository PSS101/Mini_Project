Design a comprehensive benchmark suite for a Redis-like key-value database implemented in C and communicating over TCP.

Requirements:

* Compare Mini Redis against SQLite using the same client-server architecture and network transport.
* Reuse TCP connections for large batches of operations instead of reconnecting per command.
* Measure throughput (operations/second), average latency, p95 latency, and total execution time.
* Test the following workloads:

  1. Write-heavy: 100,000 SET operations.
  2. Read-heavy: 100,000 GET operations on preloaded keys.
  3. Mixed workload: 50% GET, 50% SET.
  4. Read-after-write: SET followed immediately by GET.
  5. Random-access workload using at least 100,000 unique keys.
  6. Concurrent workload with 1, 10, 50, and 100 client threads.

Data sizes:

* Small values: 32 bytes.
* Medium values: 1 KB.
* Large values: 10 KB.

Dataset sizes:

* 10,000 keys.
* 100,000 keys.
* 1,000,000 keys if memory permits.

Benchmark rules:

* Warm up both systems before measurement.
* Run each test at least 5 times and report average, minimum, maximum, and standard deviation.
* Ensure SQLite uses prepared statements and WAL mode.
* Ensure Mini Redis uses persistent connections.
* Exclude server startup time from measurements.
* Report memory usage of both systems.

Output:

* Generate a table comparing throughput, latency, and memory consumption.
* Identify whether performance is limited by network overhead, protocol parsing, storage engine performance, locking, or memory access.
* Provide charts showing scaling behavior as key count, value size, and client concurrency increase.

Goal:
Determine whether the Mini Redis implementation demonstrates the expected advantages of an in-memory key-value store under realistic workloads rather than microbenchmark conditions.


python3 benchmark_suite.py --repeat 5