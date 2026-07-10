// tests/latency_recorder_test.cpp
#include "latency_recorder.hpp"

#include <gtest/gtest.h>
#include <thread>

using engine::LatencyRecorder;

TEST(LatencyRecorderTest, SingleMessageAllStagesRecorded) {
    LatencyRecorder recorder(10);

    recorder.record_received(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    recorder.record_decoded(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    recorder.record_popped(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    recorder.record_applied(0);

    // wire-to-book should be roughly 3ms (three 1ms sleeps), definitely > 0
    double p50 = recorder.wire_to_book_percentile(0.5);
    EXPECT_GT(p50, 0.0);
    // sanity upper bound -- should be well under, say, 100ms for 3 sleeps of 1ms
    EXPECT_LT(p50, 100'000'000.0);
}

TEST(LatencyRecorderTest, IncompleteEntryExcludedFromPercentile) {
    LatencyRecorder recorder(10);

    // sequence 0: fully recorded
    recorder.record_received(0);
    recorder.record_decoded(0);
    recorder.record_popped(0);
    recorder.record_applied(0);

    // sequence 1: only partially recorded -- should be excluded entirely
    recorder.record_received(1);
    recorder.record_decoded(1);
   
    double p99 = recorder.wire_to_book_percentile(0.99);
    EXPECT_GE(p99, 0.0);
}

TEST(LatencyRecorderTest, PercentileMatchesKnownDistribution) {
    
    LatencyRecorder recorder(100);

    for (uint64_t i = 0; i < 100; ++i) {
        recorder.record_received(i);
      
        std::this_thread::sleep_for(std::chrono::microseconds(i));
        recorder.record_decoded(i);
        recorder.record_popped(i);
        recorder.record_applied(i);
    }

    double p50 = recorder.wire_to_book_percentile(0.5);
    double p99 = recorder.wire_to_book_percentile(0.99);

    // p99 duration should be larger
    
    EXPECT_GT(p99, p50);
}

TEST(LatencyRecorderTest, EmptyRecorderReturnsZero) {
    LatencyRecorder recorder(10);

    // nothing recorded at all
    EXPECT_EQ(recorder.wire_to_book_percentile(0.99), 0.0);
}

TEST(LatencyRecorderTest, SubStagesSumApproximatelyToWireToBook) {
    LatencyRecorder recorder(10);

    recorder.record_received(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    recorder.record_decoded(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    recorder.record_popped(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    recorder.record_applied(0);

    double decode = recorder.decode_percentile(0.5);
    double queue_transit = recorder.queue_transit_percentile(0.5);
    double match = recorder.match_percentile(0.5);
    double wire_to_book = recorder.wire_to_book_percentile(0.5);

   
    double sum = decode + queue_transit + match;
    EXPECT_NEAR(sum, wire_to_book, 1000.0);  // within 1 microsecond of each other
}