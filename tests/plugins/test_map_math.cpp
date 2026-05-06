#include <gtest/gtest.h>
#include "plugins/map_view/map_math.hpp"

using namespace aegis::plugins::map_math;

TEST(MapMath, LatLonToWorldPixel_SanFrancisco) {
    // San Francisco ~ (37.7749, -122.4194) at zoom 15
    QPointF world = latLonToWorldPixel(37.7749, -122.4194, 15, 256);
    EXPECT_GT(world.x(), 0.0);
    EXPECT_GT(world.y(), 0.0);

    // Tile x should be approximately 5240, y approximately 12667
    QPoint tile = worldPixelToTile(world.x(), world.y(), 256);
    EXPECT_EQ(tile.x(), 5240);
    EXPECT_EQ(tile.y(), 12667);
}

TEST(MapMath, WorldPixelToTile_Boundaries) {
    EXPECT_EQ(worldPixelToTile(0.0, 0.0, 256), QPoint(0, 0));
    EXPECT_EQ(worldPixelToTile(255.9, 255.9, 256), QPoint(0, 0));
    EXPECT_EQ(worldPixelToTile(256.0, 256.0, 256), QPoint(1, 1));
    EXPECT_EQ(worldPixelToTile(512.0, 512.0, 256), QPoint(2, 2));
    EXPECT_EQ(worldPixelToTile(-1.0, -1.0, 256), QPoint(-1, -1));
}

TEST(MapMath, VisibleTileRange_SimpleViewport) {
    // Zoom 1: mapSize = 512. Scene rect (0,0)-(512,512), center at (0,0)
    QRectF scene(0, 0, 512, 512);
    QRect range = visibleTileRange(scene, QPointF(0, 0), 1, 256);
    EXPECT_EQ(range.left(), 0);
    EXPECT_EQ(range.top(), 0);
    EXPECT_EQ(range.right(), 1);
    EXPECT_EQ(range.bottom(), 1);
}

TEST(MapMath, VisibleTileRange_ClampedAtBounds) {
    // Large negative scene rect at zoom 1 should clamp to tile 0
    QRectF scene(-1000, -1000, 100, 100);
    QRect range = visibleTileRange(scene, QPointF(0, 0), 1, 256);
    EXPECT_EQ(range.left(), 0);
    EXPECT_EQ(range.top(), 0);
    // max tile at zoom 1 is 1; even if scene extends far right, clamped
    EXPECT_LE(range.right(), 1);
    EXPECT_LE(range.bottom(), 1);
}

TEST(MapMath, VisibleTileRange_OffsetCenter) {
    // Scene at (0,0)-(512,512) but centerWorld is (256,256)
    // world pixels = scene + centerWorld -> (256..768, 256..768)
    // tiles: floor(256/256)=1 to floor(768/256)=2 (but at zoom 1 max is 1)
    QRectF scene(0, 0, 512, 512);
    QRect range = visibleTileRange(scene, QPointF(256, 256), 1, 256);
    EXPECT_EQ(range.left(), 1);
    EXPECT_EQ(range.top(), 1);
    EXPECT_EQ(range.right(), 1);  // clamped by zoom-1 max
    EXPECT_EQ(range.bottom(), 1);
}

TEST(MapMath, LatLonToWorldPixel_EquatorMeridian) {
    // (0, 0) at zoom 0: mapSize = 256, x = 128, y = 128
    QPointF world = latLonToWorldPixel(0.0, 0.0, 0, 256);
    EXPECT_DOUBLE_EQ(world.x(), 128.0);
    EXPECT_DOUBLE_EQ(world.y(), 128.0);
}
