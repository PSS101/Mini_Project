# Benchmark Results

The benchmark compares Mini Redis over TCP against a networked SQLite-backed baseline using repeated read/write batches.

## Summary

- Write-only (SET): Mini Redis was about 1.1x slower on average.
- Read-after-write (SET + GET): Mini Redis was about 0.84x slower on average.
- Mixed read/write: Mini Redis was about 1.16x slower on average.

## Notes

- Both systems are now exercised over a networked request path, so the comparison is much closer to the architecture of a real server-style database.
- The results show that Mini Redis is competitive with SQLite for these simple key/value workloads when the same transport overhead is included.
- Results are stored in tests/benchmark_results.txt.
