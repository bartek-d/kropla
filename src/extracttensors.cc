/********************************************************************************************************
 kropla -- a program to play Kropki; file extracttensors.cc -- main file for
extracting trainign data for NN. Copyright (C) 2021, 2022, 2023 Bartek Dyda,
email: bartekdyda (at) protonmail (dot) com

    Some parts are inspired by Pachi http://pachi.or.cz/
      by Petr Baudis and Jean-loup Gailly

    This file is part of Kropla.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************************************************/

#include <torch/torch.h>

#include <algorithm>
#include <array>
#include <boost/multi_array.hpp>
#include <cstring>
#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <string>

#include "allpattgen.h"
#include "game.h"
#include "gzip.hpp"
#include "patterns.h"
#include "sgf.h"
#include "string_utils.h"

constexpr int MOVES_USED = 3;
constexpr int PLANES = 20;
constexpr int BSIZEX = 20;
constexpr int BSIZEY = 20;

constexpr std::size_t size_of_boards =
    std::size_t(PLANES) * BSIZEX * BSIZEY * sizeof(float);
constexpr std::size_t size_of_labels = MOVES_USED * sizeof(int);

constexpr int MAX_FILE_SIZE = 2'100'000'000;

constexpr int getTabSize()
{
    int approximation = MAX_FILE_SIZE / (size_of_boards + size_of_labels);
    constexpr int block_size = 4096;
    return (approximation / block_size) * block_size;
}

constexpr int TAB_SIZE = getTabSize();
constexpr int THRESHOLD = TAB_SIZE * 200;

// using Board = float[PLANES][BSIZE][BSIZE];

struct Datum
{
    using Array3dim = boost::multi_array<float, 3>;
    Array3dim boards{boost::extents[PLANES][BSIZEX][BSIZEY]};
    std::array<int, MOVES_USED> labels;
    std::string serialise() const;
};

std::string Datum::serialise() const
{
    std::string res(static_cast<std::size_t>(size_of_boards + size_of_labels),
                    '\0');
    memcpy(&res[0], boards.origin(), size_of_boards);
    memcpy(&res[size_of_boards], &labels[0], size_of_labels);
    return zip(res);
}

template <typename BoardsArr, typename LabelsSaver>
void unserialise_to(BoardsArr board_arr, LabelsSaver saver,
                    const std::string& what)
{
    memcpy(board_arr.origin(), &what[0], size_of_boards);
    for (int i = 0; i < MOVES_USED; ++i)
    {
        int value;
        memcpy(&value, &what[size_of_boards + sizeof(int) * i], sizeof(int));
        saver(i, value);
    }
}

class DataCollector
{
    using Array4dim = boost::multi_array<float, 4>;
    using Array1dim = boost::multi_array<int, 1>;
    Array4dim data{boost::extents[TAB_SIZE][PLANES][BSIZEX][BSIZEY]};
    std::array<Array1dim, MOVES_USED> data_labels;
    int curr_size = 0;
    int file_no = 1;

   public:
    DataCollector()
    {
        for (auto& a : data_labels) a.resize(boost::extents[TAB_SIZE]);
    }
    auto getCurrentArray();
    void save(int move, int label);
    auto convertLabelsToArray(int move_no) const;
    void dump();
    ~DataCollector();
};

auto DataCollector::getCurrentArray() { return data[curr_size]; }

void DataCollector::save(int move, int label)
{
    if (label < 0 or label >= BSIZEX * BSIZEY)
        throw std::runtime_error("Niepoprawny label = " +
                                 std::to_string(label));
    data_labels[move][curr_size] = label;
    if (move == MOVES_USED - 1)
    {
        ++curr_size;
        if (curr_size == TAB_SIZE) dump();
    }
}

auto DataCollector::convertLabelsToArray(int move_no) const
{
    using Array3dim = boost::multi_array<float, 3>;
    Array3dim data{boost::extents[curr_size][BSIZEX][BSIZEY]};
    for (int i = 0; i < curr_size; ++i)
        for (int x = 0; x < BSIZEX; ++x)
            for (int y = 0; y < BSIZEY; ++y) data[i][x][y] = 0.0f;
    for (int i = 0; i < curr_size; ++i)
    {
        const unsigned x = data_labels[move_no][i] / BSIZEY;
        const unsigned y = data_labels[move_no][i] % BSIZEY;
        data[i][x][y] = 1.0f;
    }
    return data;
}

void DataCollector::dump()
{
    std::cout << "dump()\n";
    try
    {
        std::string filename{"board_" + std::to_string(file_no) + ".pt"};
        torch::Tensor output_tensor =
            torch::from_blob(data.origin(), {curr_size, PLANES, BSIZEX, BSIZEY})
                .clone();
        /*
        torch::Tensor output_tensor = torch::zeros({curr_size, PLANES, BSIZEX,
        BSIZEY}); for (int i=0; i<curr_size; ++i)
          {
            if (i%100==0) std::cout << "." << std::flush;
            for (int j=0; j<PLANES; ++j)
              for (int k=0; k<BSIZEX; ++k)
                for (int l=0; l<BSIZEY; ++l)
                  output_tensor[i][j][k][l] = data[i][j][k][l];
          }
        */
        torch::save(output_tensor, filename);
        std::cout << " data saved.\n";

        /*
        auto test_Tens = torch::from_blob(data.origin(), {curr_size, PLANES,
        BSIZEX, BSIZEY}).clone(); for (int i=0; i<curr_size; ++i) for (int j=0;
        j<PLANES; ++j) for (int k=0; k<BSIZEX; ++k) for (int l=0; l<BSIZEY; ++l)
                  if (output_tensor[i][j][k][l].item<float>() !=
        test_Tens[i][j][k][l].item<float>()) std::cout << i << " " << j << " "
        << k << " " << l << ": " <<output_tensor[i][j][k][l] << "!=" <<
        test_Tens[i][j][k][l] << std::endl;
        */

        torch::Tensor labels_tensor =
            torch::zeros({curr_size, MOVES_USED, BSIZEX, BSIZEY});
        for (int i = 0; i < curr_size; ++i)
        {
            if (i % 100 == 0) std::cout << "." << std::flush;
            for (int move_no = 0; move_no < MOVES_USED; ++move_no)
            {
                const unsigned x = data_labels[move_no][i] / BSIZEY;
                const unsigned y = data_labels[move_no][i] % BSIZEY;
                labels_tensor[i][move_no][x][y] = 1.0f;
            }
        }
        torch::save(labels_tensor, filename + ".labels");
        std::cout << " labels saved." << std::endl;
    }
    catch (...)
    {
        std::cerr << "Jakis blad w DataCollector::dump()" << std::endl;
        throw;
    }
    curr_size = 0;
    ++file_no;
}

DataCollector::~DataCollector()
{
    if (curr_size == 0) return;
    data.resize(boost::extents[curr_size][PLANES][BSIZEX][BSIZEY]);
    for (int m = 0; m < MOVES_USED; ++m)
        data_labels[m].resize(boost::extents[curr_size]);
    dump();
}

class CompressedData
{
    std::deque<std::string> cont;
    std::default_random_engine dre{};
    std::uniform_int_distribution<uint64_t> di;
    DataCollector collector{};
    void permute_last();
    void save_last();
    int64_t count = 0;

   public:
    CompressedData()
        : di{std::numeric_limits<uint64_t>::min(),
             std::numeric_limits<uint64_t>::max()}
    {
    }
    void save(std::string s);
    void dump();
    ~CompressedData();
} compressed_data;

void CompressedData::permute_last()
{
    int low =
        std::max(0, static_cast<int>(cont.size()) - static_cast<int>(TAB_SIZE));
    for (int i = cont.size() - 1; i >= low; --i)
    {
        int swap_with = di(dre) % (i + 1);
        if (i != swap_with)
        {
            std::swap(cont[i], cont[swap_with]);
        }
    }
}

void CompressedData::save_last()
{
    int count = std::min<int>(cont.size(), TAB_SIZE);

    for (int i = 0; i < count; ++i)
    {
        auto u = unzip(cont.back());
        auto curr_array = collector.getCurrentArray();
        unserialise_to(
            curr_array,
            [&](int move, int label) { collector.save(move, label); },
            std::move(u));
        cont.pop_back();
    }
}

void CompressedData::save(std::string s)
{
    if (++count % 10000 != 0) std::cout << "..." << count << "  " << s.size();
    cont.emplace_back(std::move(s));
    if (cont.size() >= THRESHOLD)
    {
        permute_last();
        save_last();
    }
}

CompressedData::~CompressedData() { dump(); }

void CompressedData::dump()
{
    while (not cont.empty())
    {
        permute_last();
        save_last();
    }
}

int applyIsometry(int p, unsigned isometry)
// isometry & 1: reflect w/r to Y
// isometry & 2: reflect w/r to X
// isometry & 4: reflect w/r to x==y (swap x--y)
{
    int x = coord.x[p];
    int y = coord.y[p];
    if (isometry & 1) x = coord.wlkx - 1 - x;
    if (isometry & 2) y = coord.wlky - 1 - y;
    if (isometry & 4) std::swap(x, y);
    if (x >= coord.wlkx or y >= coord.wlky or x < 0 or y < 0)
        throw std::runtime_error("applyIsom with x,y=" + std::to_string(x) +
                                 " " + std::to_string(y));
    return coord.ind(x, y);
}

int applyIsometryInverse(int p, unsigned isometry)
// isometry & 1: reflect w/r to Y
// isometry & 2: reflect w/r to X
// isometry & 4: reflect w/r to x==y (swap x--y)
{
    int x = coord.x[p];
    int y = coord.y[p];
    if (isometry & 4) std::swap(x, y);
    if (isometry & 2) y = coord.wlky - 1 - y;
    if (isometry & 1) x = coord.wlkx - 1 - x;
    if (x >= coord.wlkx or y >= coord.wlky or x < 0 or y < 0)
        throw std::runtime_error("applyIsomInv with x,y=" + std::to_string(x) +
                                 " " + std::to_string(y));
    return coord.ind(x, y);
}

void gatherDataFromPosition(Game& game, const stdb::vector<Move>& moves)
{
    /*
    std::cerr << "Gather data from position: " << std::endl;
    game.show();
    std::cerr << "Move " << move.show() << " was about to play." << std::endl;
    */
    const unsigned max_isometry = 1;  // do not apply isometries for tensors!
    for (unsigned isometry = 0; isometry < max_isometry; ++isometry)
    {
        Datum datum;
        auto& data = datum.boards;  // collector.getCurrentArray();
        for (int x = 0; x < BSIZEX; ++x)
            for (int y = 0; y < BSIZEY; ++y)
            {
                int p_orig = coord.ind(x, y);
                int p = applyIsometryInverse(p_orig, isometry);
                int on_move = game.whoNowMoves();
                int opponent = 3 - on_move;
                data[0][x][y] = (game.whoseDotMarginAt(p) == 0) ? 1.0f : 0.0f;
                data[1][x][y] =
                    (game.whoseDotMarginAt(p) == on_move) ? 1.0f : 0.0f;
                data[2][x][y] =
                    (game.whoseDotMarginAt(p) == opponent) ? 1.0f : 0.0f;
                data[3][x][y] = game.isInTerr(p, on_move) > 0 ? 1.0f : 0.0f;
                data[4][x][y] = game.isInTerr(p, opponent) > 0 ? 1.0f : 0.0f;
                data[5][x][y] = std::min(game.isInEncl(p, on_move), 2) * 0.5f;
                data[6][x][y] = std::min(game.isInEncl(p, opponent), 2) * 0.5f;
                if (PLANES > 7)
                {
                    data[7][x][y] =
                        std::min(game.isInBorder(p, on_move), 2) * 0.5f;
                    data[8][x][y] =
                        std::min(game.isInBorder(p, opponent), 2) * 0.5f;
                    data[9][x][y] =
                        std::min(game.getTotalSafetyOf(p), 2.0f) * 0.5f;
                    if (PLANES > 10)
                    {
                        data[10][x][y] = (coord.dist[p] == 1) ? 1 : 0;
                        //	data[11][x][y] = (coord.dist[p] == 4) ? 1 : 0;
                        data[11][x][y] = 1;
                        data[12][x][y] =
                            0;  // where for thr such that opp_dots>0
                        data[13][x][y] =
                            0;  // where for thr such that opp_dots>0
                        data[14][x][y] = 0;  // where0 for thr2 such that
                                             // minwin2 > 0 and isSafe
                        data[15][x][y] = 0;  // where0 for thr2 such that
                                             // minwin2 > 0 and isSafe
                        data[16][x][y] =
                            (game.threats[on_move - 1].is_in_2m_encl[p] > 0)
                                ? 1.0f
                                : 0.0f;
                        data[17][x][y] =
                            (game.threats[opponent - 1].is_in_2m_encl[p] > 0)
                                ? 1.0f
                                : 0.0f;
                        data[18][x][y] =
                            (game.threats[on_move - 1].is_in_2m_miai[p] > 1)
                                ? 1.0f
                                : 0.0f;
                        data[19][x][y] =
                            (game.threats[opponent - 1].is_in_2m_miai[p] > 1)
                                ? 1.0f
                                : 0.0f;
                    }
                }
            }
        for (int player = 0; player < 2; ++player)
        {
            int which2 = (player + 1 == game.whoNowMoves()) ? 14 : 15;
            if (which2 < PLANES)
            {
                for (auto& t : game.threats[player].threats2m)
                {
                    if (t.min_win2 && t.isSafe())
                    {
                        data[which2][coord.x[t.where0]][coord.y[t.where0]] =
                            1.0f - std::pow(0.75f, t.min_win2);
                    }
                }
            }
            int which = (player + 1 == game.whoNowMoves()) ? 12 : 13;
            if (which < PLANES)
            {
                for (auto& t : game.threats[player].threats)
                {
                    if (t.where && t.singular_dots)
                    {
                        data[which][coord.x[t.where]][coord.y[t.where]] =
                            1.0f - std::pow(0.75f, t.singular_dots);
                    }
                }
            }
        }
        for (int m = 0; m < MOVES_USED; ++m)
        {
            int move_isom = applyIsometry(moves.at(m).ind, isometry);
            int label = coord.x[move_isom] * BSIZEY + coord.y[move_isom];
            if (label < 0 or label >= BSIZEX * BSIZEY)
                throw std::runtime_error("label");
            // collector.save(m, label);
            datum.labels[m] = label;
        }

        auto s = datum.serialise();
        compressed_data.save(std::move(s));
    }
}

std::pair<Move, stdb::vector<std::string>> getMoveFromSgfNode(const Game& game,
                                                              SgfNode node)
{
    if (node.findProp("B") != node.props.end())
    {
        const auto& value = node.findProp("B")->second;
        if (not value.empty())
        {
            return game.extractSgfMove(value.front(), 1);
        }
    }
    if (node.findProp("W") != node.props.end())
    {
        const auto& value = node.findProp("W")->second;
        if (not value.empty())
        {
            return game.extractSgfMove(value.front(), 2);
        }
    }
    return {Move{}, {}};
}

std::pair<unsigned, unsigned> getSize(SgfNode& node)
{
    auto sz_pos = node.findProp("SZ");
    std::string sz = (sz_pos != node.props.end()) ? sz_pos->second[0] : "";
    if (sz.find(':') == std::string::npos)
    {
        if (sz != "")
        {
            int x = stoi(sz);
            return {x, x};
        }
    }
    else
    {
        std::string::size_type i = sz.find(':');
        int x = stoi(sz.substr(0, i));
        int y = stoi(sz.substr(i + 1));
        return {x, y};
    }
    return {0, 0};
}

void gatherDataFromSgfSequence(SgfSequence& seq,
                               const std::map<int, bool>& whichSide,
                               bool must_surround)
{
    const auto [x, y] = getSize(seq[0]);
    if (x != BSIZEX or y != BSIZEY)
    {
        std::cout << "  ... size: " << x << "x" << y << std::endl;
        return;
    }
    const unsigned start_from = 5;
    const unsigned go_to = std::min<unsigned>(
        (x * y * 3) / 5,  // use moves until 60% of board is full
        seq.size() - MOVES_USED);
    Game game(SgfSequence(seq.begin(), seq.begin() + start_from), go_to,
              must_surround);
    for (unsigned i = start_from; i < go_to; ++i)
    {
        if (whichSide.at(game.whoNowMoves()))
        {
            stdb::vector<Move> subsequentMoves;
            bool moves_are_correct = true;
            for (int j = 0; j < MOVES_USED; ++j)
            {
                auto [move, points_to_enclose] =
                    getMoveFromSgfNode(game, seq[i + j]);
                if (move.ind == 0 or
                    move.who != (j % 2 == 0 ? game.whoNowMoves()
                                            : 3 - game.whoNowMoves()))
                {
                    moves_are_correct = false;
                    break;
                }
                subsequentMoves.push_back(move);
            }
            if (moves_are_correct)
            {
                gatherDataFromPosition(game, subsequentMoves);
            }
        }
        // std::cerr << "Trying to play at: " << seq[i].toString() << std::endl;
        game.replaySgfSequence({seq[i]}, 1);
    }
}

std::set<std::string> readLines(const std::string& filename)
{
    std::ifstream t(filename);
    std::set<std::string> output;
    for (std::string line; std::getline(t, line);)
    {
        if (not line.empty()) output.emplace(line);
    }
    return output;
}

int main(int argc, char* argv[])
{
    std::cout << "TAB_SIZE = " << TAB_SIZE << ", THRESHOLD = " << THRESHOLD
              << std::endl;
    if (argc < 3)
    {
        std::cerr << "at least 3 parameters needed, players_file sgf_file(s)"
                  << std::endl;
        return 1;
    }

    auto players = readLines(argv[1]);
    std::set<std::string> allowedPlayers{};
    std::set<std::string> forbiddenPlayers{};
    int min_rank = 4000;
    bool all_allowed = false;
    for (auto& p : players)
    {
        switch (p[0])
        {
            case '!':
                std::cout << "PLAYER FORBIDDEN: " << p.substr(1) << std::endl;
                forbiddenPlayers.insert(p.substr(1));
                break;
            case '>':
                min_rank = std::stoi(p.substr(1));
                std::cout << "MIN RANK: " << min_rank << std::endl;
                break;
            case '$':
                all_allowed = true;
                break;
            default:
                std::cout << "PLAYER: " << p << std::endl;
                allowedPlayers.insert(p);
                break;
        }
    }

    for (int nfile = 2; nfile < argc; ++nfile)
    {
        std::string sgf_file{argv[nfile]};
        std::ifstream t(sgf_file);
        std::stringstream buffer;
        buffer << t.rdbuf();
        std::string s = buffer.str();

        SgfParser parser(s);
        auto seq = parser.parseMainVar();
        if (not all_allowed)
        {
            if (seq[0].findProp("PB") == seq[0].props.end() or
                seq[0].findProp("PW") == seq[0].props.end() or
                seq[0].findProp("BR") == seq[0].props.end() or
                seq[0].findProp("WR") == seq[0].props.end())
                continue;
        }
        bool must_surround = false;
        {
            auto res = seq[0].findProp("RU");
            if (res != seq[0].props.end() and res->second[0] == "russian")
                must_surround = true;
        }
        bool blueOk = true;
        bool redOk = true;
        if (not all_allowed)
        {
            auto blue = seq[0].findProp("PB")->second[0];
            auto red = seq[0].findProp("PW")->second[0];
            auto blueRank = robust_stoi(seq[0].findProp("BR")->second[0]);
            auto redRank = robust_stoi(seq[0].findProp("WR")->second[0]);
            blueOk = (allowedPlayers.find(blue) != allowedPlayers.end() or
                      blueRank >= min_rank) and
                     (forbiddenPlayers.find(blue) == forbiddenPlayers.end());
            redOk = (allowedPlayers.find(red) != allowedPlayers.end() or
                     redRank >= min_rank) and
                    (forbiddenPlayers.find(red) == forbiddenPlayers.end());
            std::cout << sgf_file << " -- game: " << blue << " [" << blueRank
                      << "] -- " << red << " [" << redRank << "]  ";
        }
        if (blueOk and redOk)
        {
            gatherDataFromSgfSequence(seq, {{1, blueOk}, {2, redOk}},
                                      must_surround);
        }
        else
        {
            std::cout << "omitted." << std::endl;
        }
    }
    compressed_data.dump();
}
