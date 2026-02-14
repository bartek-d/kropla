#include "utils.h"

#include <ostream>
#include <vector>

#include "board.h"

std::string constructSgfFromGameBoard(const std::string &board)
{
    // Format of string: . or space = empty, o=blue, x=red; board is always
    // square
    //  "...o.."
    //  "..x.x."
    auto area = board.size();
    unsigned side = 1;
    while (side * side < area) ++side;
    std::ostringstream oss;
    oss << "(;GM[40]FF[4]CA[UTF-8]SZ[" << side << "]";
    std::vector<unsigned> blue{};
    std::vector<unsigned> red{};
    for (unsigned i = 0; i < board.size(); ++i)
    {
        switch (board[i])
        {
            case 'o':
                blue.push_back(i);
                break;
            case 'x':
                red.push_back(i);
                break;
        }
    }
    while (not(blue.empty() and red.empty()))
    {
        if (not blue.empty())
        {
            unsigned move = blue.back();
            blue.pop_back();
            oss << ";B[" << coord.numberToLetter(move % side)
                << coord.numberToLetter(move / side) << "]";
        }
        if (not red.empty())
        {
            unsigned move = red.back();
            red.pop_back();
            oss << ";W[" << coord.numberToLetter(move % side)
                << coord.numberToLetter(move / side) << "]";
        }
    }
    oss << ")";
    return oss.str();
}

std::string applyIsometry(const std::string &sgfCoord, unsigned isometry,
                          Coord &coord)
{
    std::string result{};
    auto reflect = [&coord](char c)
    {
        return coord.numberToLetter(coord.wlkx - 1 - coord.sgfCoordToInt(c))
            .front();
    };
    bool oddChar = true;
    // isometry & 1: reflect w/r to Y
    // isometry & 2: reflect w/r to X
    // isometry & 4: reflect w/r to x==y (swap x--y)
    for (auto c : sgfCoord)
    {
        if (c == '.')
        {
            result.push_back(c);
            continue;
        }
        if (oddChar)
        {
            result.push_back((isometry & 1) ? reflect(c) : c);
        }
        else
        {
            char newChar = (isometry & 2) ? reflect(c) : c;
            char lastChar = result.back();
            if (isometry & 4)
            {
                // switch last two
                std::swap(newChar, lastChar);
            }
            result.back() = lastChar;
            result.push_back(newChar);
        }
        oddChar = not oddChar;
    }
    return result;
}

Game constructGameFromSgfWithIsometry(const std::string &sgf, unsigned isometry)
{
    SgfParser parser(sgf);
    auto seq = parser.parseMainVar();
    Coord coord(10, 10);
    auto sz_pos = seq[0].findProp("SZ");
    std::string sz = (sz_pos != seq[0].props.end()) ? sz_pos->second[0] : "";
    if (sz.find(':') == std::string::npos)
    {
        if (sz != "")
        {
            int x = stoi(sz);
            coord.changeSize(x, x);
        }
    }
    else
    {
        std::string::size_type i = sz.find(':');
        int x = stoi(sz.substr(0, i));
        int y = stoi(sz.substr(i + 1));
        coord.changeSize(x, y);
    }
    for (SgfNode &node : seq)
    {
        for (SgfProperty &prop : node.props)
        {
            if (prop.first == "B" or prop.first == "W" or prop.first == "AB" or
                prop.first == "AW")
            {
                for (auto &value : prop.second)
                    value = applyIsometry(value, isometry, coord);
            }
        }
    }
    Game game(seq, 1000);
    return game;
}
