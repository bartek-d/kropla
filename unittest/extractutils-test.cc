#include "extractutils.h"

#include <gtest/gtest.h>

#include <iostream>
#include <vector>

namespace
{

TEST(ExtractUtilsTest, extractsMove)
{
    const std::string s{
        "(;SZ[6];B[bc];W[ab];B[df];W[ac];B[ee];W[ff];B[ad];W[ce];B[ed])"};
    const std::string expected_data{
        ".22..."
        "..1..."
        "......"
        ".....1"
        "....*."
        "......"

        ".11..."
        "..2..."
        "......"
        ".....2"
        "....2."
        ".....*"};
    constexpr int MOVES_USED = 3;
    constexpr int PLANES = 20;
    constexpr int BSIZEX = 6;
    constexpr int BSIZEY = 6;
    const std::vector<int> expected1{2, PLANES, BSIZEX, BSIZEY};
    const std::vector<int> expected2{2, MOVES_USED, BSIZEX, BSIZEY};
    int run = 0;
    std::string actual;
    auto tensorSaver = [&run, &expected1, &expected2, &actual](
                           float* ptr, const std::string& /*filename*/, int a,
                           int b, int c, int d)
    {
        if (run == 0)
        {
            EXPECT_EQ(expected1, (std::vector<int>{a, b, c, d}));
            actual = std::string(a * c * d, '.');
            for (int sample = 0; sample < a; ++sample)
                for (int plane : {1, 2})
                    for (int i = 0; i < c; ++i)
                        for (int j = 0; j < d; ++j)
                            if (ptr[sample * b * c * d + plane * d * c + i * d +
                                    j] >= 0.99f)
                                actual[sample * c * d + i * d + j] =
                                    plane == 1 ? '1' : '2';
            run = 1;
        }
        else
        {
            EXPECT_EQ(expected2, (std::vector<int>{a, b, c, d}));
            for (int sample = 0; sample < a; ++sample)
                for (int i = 0; i < c; ++i)
                {
                    for (int j = 0; j < d; ++j)
                    {
                        if (ptr[sample * b * c * d + 0 * d * c + i * d + j] >=
                            0.99f)
                            actual[sample * c * d + i * d + j] = '*';
                        else if (ptr[sample * b * c * d + 0 * d * c + i * d +
                                     j] >= 0.01f)
                            actual[sample * c * d + i * d + j] = '_';
                    }
                }
        }
    };
    SgfParser parser(s);
    {
        auto compressed_data = std::make_unique<
            CompressedData<MOVES_USED, PLANES, BSIZEX, BSIZEX>>(tensorSaver);
        auto seq = parser.parseMainVar();
        const bool must_surround = false;
        const bool blueOk = true;
        const bool redOk = true;
        gatherDataFromSgfSequence(*compressed_data, seq,
                                  {{1, blueOk}, {2, redOk}}, must_surround);
        compressed_data->dump();
    }
    EXPECT_EQ(expected_data, actual)
        << "actual: " << actual << "\nexpected: " << expected_data << std::endl;
}

}  // namespace
