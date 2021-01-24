#include "board.h"
#include <gtest/gtest.h>

namespace {


TEST(CoordTest, distBetweenPts_infty) {
  constexpr int xsize = 12;
  constexpr int ysize = 18;
  Coord coord(xsize, ysize);
  auto ind = coord.ind(xsize/2, ysize/2);
  auto indNEE = ind + coord.NE + coord.E;
  EXPECT_EQ(2, coord.distBetweenPts_infty(ind, indNEE));

}

}
