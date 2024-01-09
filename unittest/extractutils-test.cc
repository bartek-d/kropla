#include "extractutils.h"

#include <gtest/gtest.h>
#include <iostream>

namespace
{

TEST(ExtractUtilsTest, extractsMove)
{
  auto tensorSaver = [](float* ptr, const std::string& /*filename*/, int a, int b, int c, int d)
  {
    std::cout << a << " " << b << " " << c << " " << d << "\n";
  };

  const std::string s{"(;SZ[20];B[bc];W[ab];B[df];W[ac];B[ee];W[gg];B[hh];W[ii];B[jj])"};
  SgfParser parser(s);
  auto compressed_data = std::make_unique<CompressedData>(tensorSaver);
  auto seq = parser.parseMainVar();
  const bool must_surround = false;
  const bool blueOk = true;
  const bool redOk = true;
  gatherDataFromSgfSequence(*compressed_data, seq, {{1, blueOk}, {2, redOk}},
                            must_surround);
  compressed_data->dump();

}

}  // namespace
