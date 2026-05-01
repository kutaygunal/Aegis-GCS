#include <gtest/gtest.h>
#include "utils/ring_buffer.hpp"

using namespace aegis::utils;

TEST(RingBuffer, BasicPushAndSnapshot) {
    RingBuffer<int> buf(4);
    buf.push(1);
    buf.push(2);
    buf.push(3);

    auto snap = buf.snapshot();
    EXPECT_EQ(snap.size(), 3);
    EXPECT_EQ(snap[0], 1);
    EXPECT_EQ(snap[2], 3);
}

TEST(RingBuffer, OverwriteOldest) {
    RingBuffer<int> buf(3);
    buf.push(1);
    buf.push(2);
    buf.push(3);
    buf.push(4);

    auto snap = buf.snapshot();
    EXPECT_EQ(snap.size(), 3);
    EXPECT_EQ(snap[0], 2);  // 1 overwritten
    EXPECT_EQ(snap[2], 4);
}
