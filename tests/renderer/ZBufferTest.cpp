#include "renderer/ZBuffer.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>

namespace {
constexpr float kFarthest = std::numeric_limits<float>::lowest();
} // namespace

// ---------- size and initial state ----------

TEST(ZBufferTest, DefaultConstructedIsEmpty) {
    const ZBuffer buffer;
    EXPECT_EQ(buffer.width(), 0);
    EXPECT_EQ(buffer.height(), 0);
}

TEST(ZBufferTest, NewBufferIsFarthestEverywhere) {
    const ZBuffer buffer(5, 3);
    EXPECT_EQ(buffer.width(), 5);
    EXPECT_EQ(buffer.height(), 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 5; ++x)
            EXPECT_EQ(buffer.at(x, y), kFarthest) << "pixel " << x << ", " << y;
}

TEST(ZBufferTest, NewBufferAcceptsAnyDepth) {
    // Bigger z = closer to the viewer, so even a very far point is drawn on an empty buffer
    ZBuffer buffer(2, 2);
    EXPECT_TRUE(buffer.testAndSet(0, 0, -1e30f));
    EXPECT_TRUE(buffer.testAndSet(1, 1, 0.0f));
}

// ---------- depth test ----------

TEST(ZBufferTest, CloserPointOverwritesFartherOne) {
    ZBuffer buffer(4, 4);
    ASSERT_TRUE(buffer.testAndSet(2, 1, -1.0f));
    EXPECT_TRUE(buffer.testAndSet(2, 1, 0.5f));
    EXPECT_FLOAT_EQ(buffer.at(2, 1), 0.5f);
}

TEST(ZBufferTest, FartherPointIsRejectedAndKeepsDepth) {
    ZBuffer buffer(4, 4);
    ASSERT_TRUE(buffer.testAndSet(2, 1, 0.5f));
    EXPECT_FALSE(buffer.testAndSet(2, 1, -1.0f));
    EXPECT_FLOAT_EQ(buffer.at(2, 1), 0.5f);
}

TEST(ZBufferTest, EqualDepthIsRejected) {
    // Strict test: the first point drawn at a given depth stays
    ZBuffer buffer(4, 4);
    ASSERT_TRUE(buffer.testAndSet(3, 3, 1.25f));
    EXPECT_FALSE(buffer.testAndSet(3, 3, 1.25f));
}

TEST(ZBufferTest, OrderOfDrawingDoesNotMatter) {
    // Whatever order the points come in, the closest one ends up in the buffer
    for (const auto &depths : {std::array{1.0f, 2.0f, 3.0f}, std::array{3.0f, 2.0f, 1.0f}, std::array{2.0f, 3.0f, 1.0f}}) {
        ZBuffer buffer(1, 1);
        for (const float z : depths)
            (void)buffer.testAndSet(0, 0, z);
        EXPECT_FLOAT_EQ(buffer.at(0, 0), 3.0f);
    }
}

TEST(ZBufferTest, PixelsAreIndependent) {
    // Non-square on purpose: catches x * height + y and similar index mix-ups
    constexpr int w = 7, h = 3;
    ZBuffer buffer(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            ASSERT_TRUE(buffer.testAndSet(x, y, static_cast<float>(10 * y + x)));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            EXPECT_FLOAT_EQ(buffer.at(x, y), static_cast<float>(10 * y + x)) << "pixel " << x << ", " << y;
}

TEST(ZBufferTest, BorderPixelsAreValid) {
    // Column 0, row 0 and the last column/row are all inside the buffer
    constexpr int w = 6, h = 4;
    ZBuffer buffer(w, h);
    EXPECT_TRUE(buffer.testAndSet(0, 0, 1.0f));
    EXPECT_TRUE(buffer.testAndSet(w - 1, 0, 1.0f));
    EXPECT_TRUE(buffer.testAndSet(0, h - 1, 1.0f));
    EXPECT_TRUE(buffer.testAndSet(w - 1, h - 1, 1.0f));
    EXPECT_FLOAT_EQ(buffer.at(w - 1, h - 1), 1.0f);
}

// ---------- clear and resize ----------

TEST(ZBufferTest, ClearResetsEveryPixel) {
    ZBuffer buffer(3, 2);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 3; ++x)
            ASSERT_TRUE(buffer.testAndSet(x, y, 5.0f));
    buffer.clear();
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 3; ++x)
            EXPECT_EQ(buffer.at(x, y), kFarthest);
    // and a farther point than before is accepted again
    EXPECT_TRUE(buffer.testAndSet(1, 1, -5.0f));
}

TEST(ZBufferTest, ResizeChangesSizeAndClears) {
    ZBuffer buffer(2, 2);
    ASSERT_TRUE(buffer.testAndSet(1, 1, 5.0f));
    buffer.resize(8, 5);
    EXPECT_EQ(buffer.width(), 8);
    EXPECT_EQ(buffer.height(), 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 8; ++x)
            EXPECT_EQ(buffer.at(x, y), kFarthest);
    EXPECT_TRUE(buffer.testAndSet(7, 4, 0.0f));
}

TEST(ZBufferTest, ResizeOfDefaultConstructedBuffer) {
    ZBuffer buffer;
    buffer.resize(3, 3);
    EXPECT_EQ(buffer.width(), 3);
    EXPECT_EQ(buffer.at(2, 2), kFarthest);
}

// ---------- invalid arguments ----------

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(ZBufferDeathTest, RejectsEmptySize) {
    EXPECT_DEATH(ZBuffer(0, 5), "must be > 0");
    EXPECT_DEATH(ZBuffer(5, 0), "must be > 0");
    ZBuffer buffer(2, 2);
    EXPECT_DEATH(buffer.resize(-1, 2), "must be > 0");
}

TEST(ZBufferDeathTest, RejectsPixelsOutOfRange) {
    ZBuffer buffer(4, 3);
    EXPECT_DEATH((void)buffer.testAndSet(-1, 0, 0.0f), "out of range");
    EXPECT_DEATH((void)buffer.testAndSet(0, -1, 0.0f), "out of range");
    EXPECT_DEATH((void)buffer.testAndSet(4, 0, 0.0f), "out of range");
    EXPECT_DEATH((void)buffer.testAndSet(0, 3, 0.0f), "out of range");
    EXPECT_DEATH((void)buffer.at(4, 2), "out of range");
}
#endif
