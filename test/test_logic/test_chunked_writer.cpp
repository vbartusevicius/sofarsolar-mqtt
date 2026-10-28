#include <gtest/gtest.h>
#include <string>
#include "util/ChunkedWriter.h"

struct FakeSink {
    std::string data;
    unsigned    calls     = 0;
    unsigned    failAfter = 1000;   // calls that succeed before the socket stalls
    size_t write(const uint8_t* b, size_t n) {
        if (calls++ >= failAfter) return 0;
        data.append((const char*)b, n);
        return n;
    }
};

TEST(ChunkedWriter, BatchesSingleBytesIntoChunks) {
    FakeSink sink;
    ChunkedWriter<FakeSink, 16> w(sink);
    for (int i = 0; i < 40; i++) w.write((uint8_t)('a' + i % 26));
    EXPECT_TRUE(w.finish());
    EXPECT_EQ(sink.data.size(), 40u);
    EXPECT_EQ(sink.calls, 3u);            // 16 + 16 + 8, not 40 socket writes
}

TEST(ChunkedWriter, BulkWritesSpanChunks) {
    FakeSink sink;
    ChunkedWriter<FakeSink, 8> w(sink);
    const char* msg = "hello chunked world";
    EXPECT_EQ(w.write((const uint8_t*)msg, strlen(msg)), strlen(msg));
    EXPECT_TRUE(w.finish());
    EXPECT_EQ(sink.data, msg);
}

TEST(ChunkedWriter, FirstShortWriteStopsAllFurtherSocketCalls) {
    FakeSink sink;
    sink.failAfter = 1;
    ChunkedWriter<FakeSink, 4> w(sink);
    for (int i = 0; i < 1000; i++) w.write((uint8_t)'x');
    EXPECT_FALSE(w.finish());
    EXPECT_EQ(sink.calls, 2u);            // one success, one stall, then nothing
}

TEST(ChunkedWriter, EmptyFinishIsOk) {
    FakeSink sink;
    ChunkedWriter<FakeSink, 4> w(sink);
    EXPECT_TRUE(w.finish());
    EXPECT_EQ(sink.calls, 0u);
}
