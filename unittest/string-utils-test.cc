#include "string_utils.h"
#include <gtest/gtest.h>
#include <string>

TEST(robust_stoi, withNoDigitsReturnsMinus1)
{
  EXPECT_EQ(-1, robust_stoi(""));
  EXPECT_EQ(-1, robust_stoi("acjdkf rf jor"));
  EXPECT_EQ(-1, robust_stoi("--dAhc - -"));
  EXPECT_EQ(-1, robust_stoi("     "));
  EXPECT_EQ(-1, robust_stoi("\n\n\n"));
}

TEST(robust_stoi, withDigitsReturnsFirstIntWithAbsValue)
{
  EXPECT_EQ(123, robust_stoi("123"));
  EXPECT_EQ(123, robust_stoi("-123"));
  EXPECT_EQ(1234, robust_stoi("acjd--1234---899"));
  EXPECT_EQ(5677, robust_stoi("5677--dAhc - -"));
  EXPECT_EQ(5677, robust_stoi("5677 1234 -dAhc - -"));
}


