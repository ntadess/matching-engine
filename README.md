# matching-engine

A low-latency order matching engine in C++20, built as a resume-centerpiece project
for HFT/quant dev roles. Focus: correct price-time-priority matching, a real binary
wire protocol, a lock-free concurrent pipeline, and measured (not assumed) latency.

## Architecture

```
UDP packet
    │
    ▼
FeedHandler (producer thread)
  - io_uring receive (8 buffers, overlapped reads)
  - decode binary protocol → AddMessage / CancelMessage
  - sequence gap/duplicate detection
  - push onto SPSC queue
    │
    ▼
SpscQueue<FeedMessage>
  - lock-free, single-producer/single-consumer ring buffer
  - std::variant<AddMessage, CancelMessage> as the element type
  - monotonic counters + bitmask indexing, acquire/release ordering
    │
    ▼
MatchingThread (consumer thread)
  - pop, std::visit dispatch on message type
  - apply to OrderBook
    │
    ▼
OrderBook
  - price-time-priority matching, partial fills, multi-level walks
  - array-indexed price levels with dynamic recentering
  - intrusive linked list per level (O(1) FIFO)
  - O(1) cancel via id → order lookup
```

`Engine` owns and wires together the queue, the book, `FeedHandler`, and
`MatchingThread`, and manages their lifecycle (`start()`/`stop()`).

## Design decisions worth knowing

- **SPSC lock-free queue over a mutex-protected queue** — avoids lock
  contention/OS scheduling involvement on the hot path between the network
  thread and the matching thread. Monotonic read/write counters (not raw
  wrapped indices) avoid the classic empty/full ambiguity; slot index is
  computed via bitmask (`index & (capacity - 1)`, capacity is a compile-time
  power of two).
- **`std::variant<AddMessage, CancelMessage>` over a raw union or virtual
  dispatch** — type-safe, no heap allocation, no vtable/indirect call on
  the hot path.
- **Manual byte-offset encode/decode for the wire protocol** — not
  `memcpy`-ing structs directly, since struct padding isn't guaranteed
  consistent across compilation, and the wire format needs a fixed,
  documented layout.
- **8-buffer overlapped io_uring receive** — a single-buffer,
  submit-wait-process-resubmit loop dropped up to 47% of messages under
  load (see BENCHMARKS.md); keeping multiple reads continuously in flight
  fixed this.
- **`FeedHandler` and `MatchingThread` know nothing about each other** —
  `FeedHandler` only knows the wire protocol and the queue; `MatchingThread`
  only knows the queue and `OrderBook`. Neither knows about sockets,
  io_uring, or byte-level decoding on the other side.

## Status

All planned phases complete:

- **Phase 0** — environment (CMake + Ninja, GoogleTest, liburing)
- **Phase 1** — order book core: add/cancel, price-time-priority matching,
  partial fills, dynamic recentering
- **Phase 2** — binary protocol: fixed-width messages, manual encode/decode
- **Phase 3** — feed handler: real UDP + io_uring, sequence gap/duplicate detection
- **Phase 4** — concurrency: SPSC queue, separate matching thread, `Engine` orchestrator
- **Phase 5** — instrumentation: per-stage latency recording, synthetic load
  generator, benchmark runner, real measured p50/p99/p99.9 — see [BENCHMARKS.md](BENCHMARKS.md)
- **Phase 6** — profiling pass with `perf`; confirmed the order book and
  concurrency layer are not the bottleneck at current load levels — see
  [BENCHMARKS.md](BENCHMARKS.md)

Passing GoogleTest suite across order book, messages, feed handler,
SPSC queue, matching thread, and full end-to-end engine tests (run
`./build/engine_tests` for the current count).

## Building

Requires: CMake, Ninja, a C++20 compiler, liburing (dev headers).

```bash
cmake -B build -G Ninja
cmake --build build
```

## Running tests

```bash
./build/engine_tests
```

## Running the benchmark

```bash
./build/benchmark_runner <port> <message_count> [rate_per_sec]
# e.g.
./build/benchmark_runner 47000 100000 20000
```

See [BENCHMARKS.md](BENCHMARKS.md) for methodology, measured latency
figures, a real throughput bug found and fixed via benchmarking, and
profiling results.

## Known limitations

- Sequence gap detection is detection-only — no retransmission or
  snapshot recovery is implemented on gap.
- Synthetic benchmark traffic is a simplified model (70/30 Add/Cancel,
  normally-distributed prices), not a replay of real market data.
- Benchmarked on WSL2, not bare-metal Linux — p50 latency figures for
  cross-thread intervals showed measurement artifacts from hypervisor
  clock virtualization in earlier testing; see BENCHMARKS.md for the
  full investigation and how p99/p99.9 remain trustworthy despite this.

## Project layout

```
include/        — headers (order_types, order_book, messages, spsc_queue,
                   feed_handler, matching_thread, latency_recorder, engine)
src/             — implementations
tests/           — GoogleTest suites, one per component + end-to-end engine tests
tools/           — benchmark_runner (synthetic load + latency reporting)
BENCHMARKS.md    — methodology, results, bug found/fixed, profiling findings
```
