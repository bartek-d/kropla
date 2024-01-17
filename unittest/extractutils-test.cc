#include "extractutils.h"

#include <gtest/gtest.h>

#include <iostream>
#include <vector>

namespace
{

TEST(ExtractUtilsTest, extractsMove)
{
    const std::string s{
        "(;SZ[6];B[bc];W[ab];B[df];W[ac];B[ee];W[fc];B[ad];W[ce];B[ed])"};
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
        "..*..."};
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

TEST(ExtractUtilsTest, getMovesFromSgfNodePropertyLB)
{
    const std::string sgf{
        "(;SZ[20]PB[kropla:8000]PW[kropla:8000];B[jl];W[km];B[mk];W[gj];B[jj];"
        "W[ll]LB[bh:3][bi:1][bj:13][bl:30][bq:1][cb:5][cf:1][cn:1][cs:7][dh:13]"
        "[dj:2][dm:7][dn:11][ei:6][ek:1][es:3][fq:3][fs:14][gb:2][gh:4][gm:2]["
        "gs:1][hi:9][hm:1][hs:4][ib:11][ie:2][ih:13][ii:1][ik:6][il:1][im:2]["
        "io:1][jb:7][jk:1][jm:104][js:5][kk:2][kl:111][li:1][lj:1][lk:84][ll:"
        "379][ln:11][ml:4][mm:50][ob:1][oe:13][of:4][oi:10][pb:1][pq:3][pr:20]["
        "qq:1][qr:4][rb:25][ri:1][sb:12][sg:1][sj:7][sl:1][sm:4][sn:7][sr:1];B["
        "lk]LB[bb:12][bd:16][bf:21][bh:3][bi:5][bj:8][bk:3][bl:13][bm:2][bq:5]["
        "cb:2][cd:7][ci:1][ck:1][co:11][cp:26][cq:8][cr:1][dd:1][de:11][df:6]["
        "dn:5][dr:14][eb:3][ee:1][eq:1][fb:30][fs:1][gf:2][gs:2][hb:4][hc:27]["
        "he:5][hk:2][hl:1][ie:9][ih:2][im:7][is:5][jb:6][jg:1][jm:105][kb:6]["
        "kj:1][kk:3][kl:1][lc:8][le:2][lf:5][lk:440][ls:15][ml:10][ms:16][ng:5]"
        "[nj:1][nr:2][ns:1][ob:17][ok:7][os:3][pb:1][pp:4][ps:14][qs:6][rl:1]["
        "ro:1][sb:3][se:1][sf:13][si:35][sm:18][sq:2][sr:12][ss:8])"};
    coord.changeSize(20, 20);
    SgfParser parser(sgf);
    auto seq = parser.parseMainVar();
    {
        auto res = getMovesFromSgfNodePropertyLB(seq.back());
        ASSERT_EQ(2, res.size());
        EXPECT_EQ(coord.sgfToPti("lk"), res[0].first.ind);
        EXPECT_EQ(coord.sgfToPti("jm"), res[1].first.ind);
        const float eps = 1e-3;
        EXPECT_NEAR(0.807f, res[0].second, eps);
        EXPECT_NEAR(0.193f, res[1].second, eps);
    }
    {
        auto res = getMovesFromSgfNodePropertyLB(seq.at(seq.size() - 2));
        for (auto el : res)
            std::cout << el.first.show() << " --> " << el.second << '\n';
        ASSERT_EQ(4, res.size());
        EXPECT_EQ(coord.sgfToPti("ll"), res[0].first.ind);
        EXPECT_EQ(coord.sgfToPti("kl"), res[1].first.ind);
        EXPECT_EQ(coord.sgfToPti("jm"), res[2].first.ind);
        EXPECT_EQ(coord.sgfToPti("lk"), res[3].first.ind);
        const float eps = 1e-3;
        EXPECT_NEAR(0.559f, res[0].second, eps);
        EXPECT_NEAR(0.164f, res[1].second, eps);
        EXPECT_NEAR(0.153f, res[2].second, eps);
        EXPECT_NEAR(0.124f, res[3].second, eps);
    }
}

}  // namespace
