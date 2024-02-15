#include <gtest/gtest.h>

#include <iostream>
#include <set>
#include <string>

#include "allpattgen.h"
#include "patterns.h"
#include "utils.h"

namespace
{
TEST(RotateAndCheckBottomEdge, worksCorrectly)
{
    pattern3_t p = 81766;
    std::array<pattern3_t, 8> rotations;
    std::array<std::string, 8> rot_str;
    rotations[0] = p;
    rotations[1] = Pattern3::reflect(p);
    for (int i = 2; i < 8; i += 2)
    {
        rotations[i] = Pattern3::rotate(rotations[i - 2]);
        rotations[i + 1] = Pattern3::rotate(rotations[i - 1]);
    }
    for (int i = 0; i < 8; ++i)
    {
        rot_str[i] = getPattStr(rotations[i]);
        EXPECT_TRUE(checkBottomEdge(rot_str[i]));
    }
}

TEST(CheckCommonFate, worksCorrectly)
{
    auto p = Pattern3::getCodeOfPattern(
        ".Y."
        "|-|"
        "YQY");
    auto q = Pattern3::rotate(Pattern3::rotate(Pattern3::rotate(p)));
    //  std::cout << "pattern:\n" << Pattern3::show(q) << " --> " <<
    //  getPattStr(q) <<std::endl;
    EXPECT_FALSE(checkCommonFate(getPattStr(q)));
}

TEST(PatternTest, isZeroWhenNoDotsInNeighb_andReadEqualsGet)
{
    const unsigned isometry = 0;
    auto sgf = constructSgfFromGameBoard(
        ".x....."
        "xo.o..."
        "....x.."
        "x......"
        "......."
        "xo.o..."
        ".x.....");
    Game game = constructGameFromSgfWithIsometry(sgf, isometry);
    std::set<std::string> pointsWithZeroPatt;
    for (pti i = coord.first; i <= coord.last; ++i)
        if (coord.dist[i] >= 0 and game.whoseDotMarginAt(i) == 0)
        {
            EXPECT_EQ(game.readPattern3_at(i), game.getPattern3_at(i));
            if (game.getPattern3_at(i) == 0)
                pointsWithZeroPatt.insert(coord.indToSgf(i));
        }
    std::set<std::string> expectedPoints{"cd", "fe", "ff"};
    EXPECT_EQ(expectedPoints, pointsWithZeroPatt);
}

}  // namespace
