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

## Phase 6: Profiling

Profiled a full benchmark run (`perf record -g`, 100,000 messages,
20,000 msg/sec) to identify optimization targets before making any
changes.

**Finding: no single function dominates CPU time.** Highest self-time
entries were all under 0.5%: `main` (0.43%), `syscall` (0.35%),
`__vdso_clock_gettime` (0.35%), `std::sort`'s introsort loop (0.20%,
attributable to `LatencyRecorder`'s percentile calculation, not the hot
path), `OrderBook::add_order` (0.09%), `OrderBook::cancel_order` (0.08%).
`OrderBook::recenter` did not register at all — either infrequent enough
or cheap enough to not surface in a system-wide profile at this sample
rate.

**What the time actually goes to, in aggregate:**
- **Clock reads** (`clock_gettime`, `__vdso_clock_gettime`,
  `std::chrono::steady_clock::now`) appear repeatedly across the profile
  and collectively represent a real, attributable cost — this is
  `LatencyRecorder` itself: 4 timestamp calls per message at 20,000
  msg/sec is 80,000 clock reads/sec. Self-inflicted measurement
  overhead, not a flaw in the matching logic.
- **io_uring wait functions and syscalls** (`io_uring_wait_cqe`,
  `io_uring_wait_cqe_timeout`, `clock_nanosleep`) — expected: these are
  intentional blocking waits (receive loop idling, generator pacing),
  not wasted CPU.

**Conclusion:** the order book and concurrency layer (`add_order`,
`cancel_order`, `match_incoming_order`, the SPSC queue) are not the
bottleneck — none of it shows up as a meaningful hotspot under profiling.
Remaining latency is dominated by OS/syscall overhead inherent to the
I/O path and the cost of the instrumentation itself, not by an
inefficiency in the matching engine's core data structures or
algorithms. No further optimization was pursued on this basis — chasing
a fix for a bottleneck the data doesn't show would be optimizing without
evidence.

## Reproducing

```bash
cmake -B build -G Ninja
cmake --build build --target benchmark_runner
./build/benchmark_runner <port> <message_count> [rate_per_sec]
```

Profiling:
```bash
perf record -g -o benchmark.perf.data -- ./build/benchmark_runner <port> <message_count> [rate_per_sec]
perf report -i benchmark.perf.data
```