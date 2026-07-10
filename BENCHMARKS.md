# Benchmarks

## Setup

- 100,000 messages, 70% Add / 30% Cancel, prices ~N(ref, 5 ticks), fixed seed (42)
- Target rate: 20,000 msg/sec — achieved: ~19,999.8 msg/sec (consistent across runs)
- WSL2 Ubuntu, loopback UDP, `-O3 -march=native`
- Latency = wire arrival → applied to `OrderBook`, across FeedHandler → SPSC queue → MatchingThread
- 8-buffer overlapped io_uring receive (see note below)
- 5 runs, two-core, unpinned (production config)

## Results

0% message loss in all 5 runs (100,000 / 100,000 measured each time).

| Stage | p50 | p99 | p99.9 |
|---|---|---|---|
| Wire-to-book (full pipeline) | 489–507 ns | 1,260–1,750 ns | 7,991–11,411 ns |
| Decode | 89–93 ns | 148–177 ns | — |
| Queue transit | 228–245 ns | 463–668 ns | — |
| Match | 150–160 ns | 638–731 ns | — |

Representative single run: wire-to-book p50 = 501 ns, p99 = 1,260 ns, p99.9 = 7,991 ns.

## Receive path: overlapped io_uring reads

Initial implementation used one buffer with a single outstanding
`io_uring` read at a time (submit → wait → process → resubmit,
serialized). At 20,000 msg/sec sustained, this dropped up to 47% of
sent messages — the kernel's UDP receive buffer absorbed packets while
the single-buffered loop was still processing the previous message, and
overflowed under load.

Fixed by keeping 8 buffers with 8 reads continuously in flight
(`io_uring_prep_recv` + `io_uring_sqe_set_data` to tag each buffer,
resubmitting immediately on each completion). This eliminated
measured loss entirely (0/100,000 across 5 runs) and also removed a
clock-skew artifact that had been showing up as small negative p50
values under the old serialized receive path — plausibly because
processing is no longer blocked in a single synchronous wait-then-process
cycle. Not confirmed as the root mechanism, but the negative p50 issue
did not recur once switched to the overlapped design.

## Reproducing

```bash
cmake -B build -G Ninja
cmake --build build --target benchmark_runner
./build/benchmark_runner <port> <message_count> [rate_per_sec]
```