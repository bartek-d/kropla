#include "patterns.h"
#include "allpattgen.h"

#include <gtest/gtest.h>

#include <string>
#include <set>
#include <iostream>

namespace {

TEST(RotateAndCheckBottomEdge, worksCorrectly)
{
  pattern3_t p = 81766;
  std::array<pattern3_t, 8> rotations;
  std::array<std::string, 8> rot_str;
  rotations[0] = p;
  rotations[1] = Pattern3::reflect(p);
  for (int i=2; i<8; i+=2) {
    rotations[i] = Pattern3::rotate(rotations[i-2]);
    rotations[i+1] = Pattern3::rotate(rotations[i-1]);
  }
  for (int i=0; i<8; ++i) {
    rot_str[i] = getPattStr(rotations[i]);
    EXPECT_TRUE(checkBottomEdge(rot_str[i]));
  }
}

TEST(CheckCommonFate, worksCorrectly)
{
  auto p = Pattern3::getCodeOfPattern(".Y."
				      "|-|"
				      "YQY");
  auto q = Pattern3::rotate(Pattern3::rotate(Pattern3::rotate(p)));
  //  std::cout << "pattern:\n" << Pattern3::show(q) << " --> " << getPattStr(q) <<std::endl;
  EXPECT_FALSE(checkCommonFate(getPattStr(q)));

}
  
}
