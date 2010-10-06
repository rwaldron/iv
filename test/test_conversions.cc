#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include <cmath>
#include "conversions-inl.h"

TEST(ConversionsCase, UStringToDoubleTest) {
  using iv::core::StringToDouble;
  ASSERT_TRUE(std::isnan(StringToDouble("TEST")));
  ASSERT_TRUE(std::isnan(StringToDouble(" T")));
  ASSERT_TRUE(std::isnan(StringToDouble(" T ")));
  ASSERT_TRUE(std::isnan(StringToDouble("T ")));
  ASSERT_TRUE(std::isnan(StringToDouble("T")));
  ASSERT_EQ(StringToDouble(" "), 0);
  ASSERT_EQ(StringToDouble("    "), 0);
  ASSERT_EQ(StringToDouble("0   "), 0);
  ASSERT_EQ(StringToDouble(" 0  "), 0);
  ASSERT_EQ(StringToDouble("0000"), 0);
  ASSERT_EQ(StringToDouble("00  "), 0);
  ASSERT_EQ(StringToDouble("01  "), 1);
  ASSERT_EQ(StringToDouble("08  "), 8);
  ASSERT_EQ(StringToDouble("  08  "), 8);
  ASSERT_EQ(StringToDouble("  8"), 8);
  ASSERT_EQ(StringToDouble("8"), 8);
  ASSERT_EQ(StringToDouble("0x01"), 1);
  ASSERT_EQ(StringToDouble("0x0f"), 15);
  ASSERT_EQ(StringToDouble("0x1f"), 31);
  ASSERT_EQ(StringToDouble("0x1f   "), 31);
  ASSERT_EQ(StringToDouble("    0x1f   "), 31);
  ASSERT_EQ(StringToDouble("    0x1f"), 31);
  ASSERT_TRUE(std::isnan(StringToDouble("    0x   1f")));
  ASSERT_TRUE(std::isnan(StringToDouble("    0 x   1f")));
  ASSERT_TRUE(std::isnan(StringToDouble("    0x1 f")));
  ASSERT_TRUE(std::isnan(StringToDouble("    0 x1f  ")));
  ASSERT_TRUE(std::isnan(StringToDouble("    0X   1f")));
  ASSERT_TRUE(std::isnan(StringToDouble("    0 X   1f")));
  ASSERT_TRUE(std::isnan(StringToDouble("    0X1 f")));
  ASSERT_TRUE(std::isnan(StringToDouble("    00X1f")));
  ASSERT_TRUE(std::isnan(StringToDouble("00X1f  ")));
  ASSERT_TRUE(std::isnan(StringToDouble("    00X1f  ")));
  ASSERT_TRUE(std::isnan(StringToDouble("00X1f")));
  ASSERT_EQ(StringToDouble("100"), 100);
  ASSERT_EQ(StringToDouble(" 100 "), 100);
  ASSERT_EQ(StringToDouble("100   "), 100);
  ASSERT_EQ(StringToDouble("    100"), 100);
  ASSERT_TRUE(std::isnan(StringToDouble("100T")));
  ASSERT_TRUE(std::isnan(StringToDouble("T100")));
  ASSERT_TRUE(std::isnan(StringToDouble("100     T")));
  ASSERT_TRUE(std::isnan(StringToDouble("         100     T")));
  ASSERT_EQ(StringToDouble("0"), 0);
  ASSERT_EQ(StringToDouble(""), 0);
  ASSERT_TRUE(std::isnan(StringToDouble("E0")));
  ASSERT_TRUE(std::isnan(StringToDouble("e0")));
  ASSERT_EQ(StringToDouble("1e0"), 1);
  ASSERT_EQ(StringToDouble("-10"), -10);
  ASSERT_EQ(StringToDouble("+10"), 10);
  ASSERT_EQ(StringToDouble(" -10 "), -10);
  ASSERT_EQ(StringToDouble(" +10 "), 10);
  ASSERT_FALSE(std::isfinite(StringToDouble(" +Infinity ")));
  ASSERT_FALSE(std::isfinite(StringToDouble(" -Infinity ")));
  ASSERT_FALSE(std::isfinite(StringToDouble("+Infinity ")));
  ASSERT_FALSE(std::isfinite(StringToDouble("-Infinity ")));
  ASSERT_FALSE(std::isfinite(StringToDouble("  +Infinity")));
  ASSERT_FALSE(std::isfinite(StringToDouble("  -Infinity")));
  ASSERT_TRUE(std::isnan(StringToDouble("Infinity  ty")));
  ASSERT_TRUE(std::isnan(StringToDouble("+Infinity t")));
  ASSERT_TRUE(std::isnan(StringToDouble("-Infinity t")));
  ASSERT_GT(StringToDouble(" +Infinity "), 0);
  ASSERT_LT(StringToDouble(" -Infinity "), 0);
}

TEST(ConversionsCase, BigNumberTest) {
  using iv::core::StringToDouble;
  ASSERT_FALSE(std::isfinite(StringToDouble("1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111")));
  ASSERT_LT(StringToDouble("1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111e-20000000000"), 1);
  ASSERT_LT(StringToDouble("-11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111.111111111111111111111111111111111111111111111111111111111111111111e-999"), 1);
  ASSERT_LT(StringToDouble("-0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), 0);
  ASSERT_LT(StringToDouble("-0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000ff"), 0);
}