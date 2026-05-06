#include <gtest/gtest.h>
#include <QCoreApplication>
#include "map/tile_cache.hpp"
#include <QImage>

using namespace aegis::map;

class TileCacheTest : public ::testing::Test {
protected:
    static std::unique_ptr<QCoreApplication> app;

    static void SetUpTestSuite() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {const_cast<char*>("test")};
            app = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
};

std::unique_ptr<QCoreApplication> TileCacheTest::app;

TEST(TileCacheTest, InsertAndGet) {
    TileCache cache(1024 * 1024); // 1 MB
    TileCoord c{10, 5, 5};
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::red);

    cache.put(c, img);
    QImage out;
    EXPECT_TRUE(cache.get(c, out));
    EXPECT_FALSE(out.isNull());
}

TEST(TileCacheTest, MissReturnsFalse) {
    TileCache cache(1024 * 1024);
    TileCoord c{10, 99, 99};
    QImage out;
    EXPECT_FALSE(cache.get(c, out));
    EXPECT_EQ(cache.missCount(), 1);
}

TEST(TileCacheTest, EvictionAtCapacity) {
    TileCache cache(1024); // Very small: ~4 tiles of 64x64x4 bytes
    std::vector<TileCoord> coords = {
        {10, 0, 0}, {10, 1, 0}, {10, 2, 0}, {10, 3, 0}, {10, 4, 0}
    };
    for (const auto& c : coords) {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(Qt::blue);
        cache.put(c, img);
    }
    // At least one eviction should have happened (total > 1024 bytes)
    EXPECT_LT(cache.currentBytes(), 1024 + 64 * 64 * 4);
}

TEST(TileCacheTest, LRUOrderAfterAccess) {
    // Cache big enough for 3 tiles (32*32*4 = 4096 each), small enough to evict on 4th
    TileCache cache(13000);
    TileCoord a{10, 0, 0}, b{10, 1, 0}, c{10, 2, 0};
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::green);

    cache.put(a, img);
    cache.put(b, img);
    cache.put(c, img);

    // Access a to make it most-recently used
    QImage out;
    cache.get(a, out);

    // Now insert d; b (LRU) should be evicted
    TileCoord d{10, 3, 0};
    cache.put(d, img);

    EXPECT_TRUE(cache.get(a, out));  // a still present
    EXPECT_FALSE(cache.get(b, out)); // b evicted
}

TEST(TileCacheTest, HitRate) {
    TileCache cache(1024 * 1024);
    TileCoord a{10, 0, 0}, b{10, 1, 0};
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::yellow);

    cache.put(a, img);
    QImage out;
    cache.get(a, out); // hit
    cache.get(a, out); // hit
    cache.get(b, out); // miss

    EXPECT_DOUBLE_EQ(cache.hitRate(), 2.0 / 3.0);
}

TEST(TileCacheTest, Clear) {
    TileCache cache(1024 * 1024);
    TileCoord c{10, 0, 0};
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::white);
    cache.put(c, img);

    cache.clear();
    EXPECT_EQ(cache.currentBytes(), 0);
    QImage out;
    EXPECT_FALSE(cache.get(c, out));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
