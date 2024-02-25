/********************************************************************************************************
 kropla -- a program to play Kropki; file extractutils.h -- utils for
extracting trainign data for NN. Copyright (C) 2021, 2022, 2023, 2024 Bartek
Dyda, email: bartekdyda (at) protonmail (dot) com

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

/*
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
*/

// using Board = float[PLANES][BSIZE][BSIZE];

using MoveValue = std::pair<Move, float>;

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
struct Datum
{
    constexpr static std::size_t size_of_boards =
        std::size_t(PLANES) * BSIZEX * BSIZEY * sizeof(float);
    constexpr static std::size_t size_of_labels =
        MOVES_USED * BSIZEX * BSIZEY * sizeof(float);
    using Array3dim = boost::multi_array<float, 3>;
    Array3dim boards{boost::extents[PLANES][BSIZEX][BSIZEY]};
    Array3dim labels{boost::extents[MOVES_USED][BSIZEX][BSIZEY]};
    std::string serialise() const;
};

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
std::string Datum<MOVES_USED, PLANES, BSIZEX, BSIZEY>::serialise() const
{
    std::string res(static_cast<std::size_t>(size_of_boards + size_of_labels),
                    '\0');
    memcpy(&res[0], boards.origin(), size_of_boards);
    memcpy(&res[size_of_boards], labels.origin(), size_of_labels);
    return zip(res);
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY,
          typename BoardsArr, typename LabelsArr>
void unserialise_to(BoardsArr board_arr, LabelsArr labels_arr,
                    const std::string& what)
{
    memcpy(board_arr.origin(), &what[0],
           Datum<MOVES_USED, PLANES, BSIZEX, BSIZEY>::size_of_boards);
    memcpy(labels_arr.origin(),
           &what[Datum<MOVES_USED, PLANES, BSIZEX, BSIZEY>::size_of_boards],
           Datum<MOVES_USED, PLANES, BSIZEX, BSIZEY>::size_of_labels);
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
class DataCollector
{
   public:
    using DataSaver =
        std::function<void(float*, const std::string&, int, int, int, int)>;
    constexpr static int MAX_FILE_SIZE = 2'100'000'000;
    static constexpr int getTabSize()
    {
        int approximation =
            MAX_FILE_SIZE /
            (Datum<MOVES_USED, PLANES, BSIZEX, BSIZEY>::size_of_boards +
             Datum<MOVES_USED, PLANES, BSIZEX, BSIZEY>::size_of_labels);
        constexpr int block_size = 4096;
        return (approximation / block_size) * block_size;
    }

    constexpr static int TAB_SIZE = getTabSize();
    constexpr static int THRESHOLD = TAB_SIZE * 200;

   private:
    using Array4dim = boost::multi_array<float, 4>;
    using Array1dim = boost::multi_array<int, 1>;
    Array4dim data{boost::extents[TAB_SIZE][PLANES][BSIZEX][BSIZEY]};
    Array4dim data_labels{boost::extents[TAB_SIZE][MOVES_USED][BSIZEX][BSIZEY]};
    int curr_size = 0;
    int file_no = 1;
    DataSaver dataSaver;

   public:
    DataCollector(DataSaver dataSaver) : dataSaver{std::move(dataSaver)} {}
    auto getCurrentArray();
    auto getCurrentLabelsArray();
    void registerSavedPosition();
    void dump();
    ~DataCollector();
};

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
auto DataCollector<MOVES_USED, PLANES, BSIZEX, BSIZEY>::getCurrentArray()
{
    return data[curr_size];
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
auto DataCollector<MOVES_USED, PLANES, BSIZEX, BSIZEY>::getCurrentLabelsArray()
{
    return data_labels[curr_size];
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
void DataCollector<MOVES_USED, PLANES, BSIZEX, BSIZEY>::registerSavedPosition()
{
    ++curr_size;
    if (curr_size == TAB_SIZE) dump();
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
void DataCollector<MOVES_USED, PLANES, BSIZEX, BSIZEY>::dump()
{
    try
    {
        std::string filename{"board_" + std::to_string(file_no) + ".pt"};
        dataSaver(data.origin(), filename, curr_size, PLANES, BSIZEX, BSIZEY);
        /*
        torch::Tensor output_tensor =
            torch::from_blob(data.origin(), {curr_size, PLANES, BSIZEX, BSIZEY})
                .clone();
        */

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

        /*
        torch::save(output_tensor, filename);
        std::cout << " data saved.\n";
        */

        /*
        auto test_Tens = torch::from_blob(data.origin(), {curr_size, PLANES,
        BSIZEX, BSIZEY}).clone(); for (int i=0; i<curr_size; ++i) for (int j=0;
        j<PLANES; ++j) for (int k=0; k<BSIZEX; ++k) for (int l=0; l<BSIZEY; ++l)
                  if (output_tensor[i][j][k][l].item<float>() !=
        test_Tens[i][j][k][l].item<float>()) std::cout << i << " " << j << " "
        << k << " " << l << ": " <<output_tensor[i][j][k][l] << "!=" <<
        test_Tens[i][j][k][l] << std::endl;
        */

        /*
        Array4dim labels{boost::extents[curr_size][MOVES_USED][BSIZEX][BSIZEY]};
        for (auto el = labels.origin();
             el < (labels.origin() + labels.num_elements()); ++el)
        {
            *el = 0.0;
        }

        for (int i = 0; i < curr_size; ++i)
        {
            if (i % 100 == 0) std::cout << "." << std::flush;
            for (int move_no = 0; move_no < MOVES_USED; ++move_no)
            {
                const unsigned x = data_labels[move_no][i] / BSIZEY;
                const unsigned y = data_labels[move_no][i] % BSIZEY;
                labels[i][move_no][x][y] = 1.0f;
            }
        }
        */
        dataSaver(data_labels.origin(), filename + ".labels", curr_size,
                  MOVES_USED, BSIZEX, BSIZEY);

        /*
        torch::save(labels_tensor, filename + ".labels");
        std::cout << " labels saved." << std::endl;
        */
    }
    catch (...)
    {
        std::cerr << "Jakis blad w DataCollector::dump()" << std::endl;
        throw;
    }
    curr_size = 0;
    ++file_no;
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
DataCollector<MOVES_USED, PLANES, BSIZEX, BSIZEY>::~DataCollector()
{
    if (curr_size == 0) return;
    data.resize(boost::extents[curr_size][PLANES][BSIZEX][BSIZEY]);
    data_labels.resize(boost::extents[curr_size][MOVES_USED][BSIZEX][BSIZEY]);
    dump();
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
class CompressedData
{
    std::deque<std::string> cont;
    std::default_random_engine dre{};
    std::uniform_int_distribution<uint64_t> di;
    DataCollector<MOVES_USED, PLANES, BSIZEX, BSIZEY> collector;
    void permute_last();
    void save_last();
    int64_t count = 0;

   public:
    constexpr static int moves_used_v = MOVES_USED;
    constexpr static int planes_v = PLANES;
    constexpr static int bsizex_v = BSIZEX;
    constexpr static int bsizey_v = BSIZEY;
    CompressedData(typename DataCollector<MOVES_USED, PLANES, BSIZEX,
                                          BSIZEY>::DataSaver dataSaver)
        : di{std::numeric_limits<uint64_t>::min(),
             std::numeric_limits<uint64_t>::max()},
          collector{std::move(dataSaver)}
    {
    }
    void save(std::string s);
    void dump();
    ~CompressedData();
};

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
void CompressedData<MOVES_USED, PLANES, BSIZEX, BSIZEY>::permute_last()
{
    int low = std::max(
        0,
        static_cast<int>(cont.size()) -
            static_cast<int>(
                DataCollector<MOVES_USED, PLANES, BSIZEX, BSIZEY>::TAB_SIZE));
    for (int i = cont.size() - 1; i >= low; --i)
    {
        int swap_with = di(dre) % (i + 1);
        if (i != swap_with)
        {
            std::swap(cont[i], cont[swap_with]);
        }
    }
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
void CompressedData<MOVES_USED, PLANES, BSIZEX, BSIZEY>::save_last()
{
    int count = std::min<int>(
        cont.size(),
        DataCollector<MOVES_USED, PLANES, BSIZEX, BSIZEY>::TAB_SIZE);

    for (int i = 0; i < count; ++i)
    {
        auto u = unzip(cont.back());
        auto curr_array = collector.getCurrentArray();
        auto curr_labels_array = collector.getCurrentLabelsArray();
        unserialise_to<MOVES_USED, PLANES, BSIZEX, BSIZEY>(
            curr_array, curr_labels_array, std::move(u));
        collector.registerSavedPosition();
        cont.pop_back();
    }
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
void CompressedData<MOVES_USED, PLANES, BSIZEX, BSIZEY>::save(std::string s)
{
    if (++count % 10000 != 0) std::cout << "..." << count << "  " << s.size();
    cont.emplace_back(std::move(s));
    if (cont.size() >=
        DataCollector<MOVES_USED, PLANES, BSIZEX, BSIZEY>::THRESHOLD)
    {
        permute_last();
        save_last();
    }
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
CompressedData<MOVES_USED, PLANES, BSIZEX, BSIZEY>::~CompressedData()
{
    dump();
}

template <int MOVES_USED, int PLANES, int BSIZEX, int BSIZEY>
void CompressedData<MOVES_USED, PLANES, BSIZEX, BSIZEY>::dump()
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

template <typename CompressedDataCont>
void gatherDataFromPosition(CompressedDataCont& compressed_data, const Game& game,
                            const std::vector<std::vector<MoveValue>>& moves)
{
    /*
    std::cerr << "Gather data from position: " << std::endl;
    game.show();
    std::cerr << "Move " << moves[0][0].first.show() << " was one of the moves
    considered." << std::endl;
    */
    const unsigned max_isometry = 1;  // do not apply isometries for tensors!
    for (unsigned isometry = 0; isometry < max_isometry; ++isometry)
    {
        Datum<compressed_data.moves_used_v, compressed_data.planes_v,
              compressed_data.bsizex_v, compressed_data.bsizey_v>
            datum;
        auto& data = datum.boards;  // collector.getCurrentArray();
        for (int x = 0; x < compressed_data.bsizex_v; ++x)
            for (int y = 0; y < compressed_data.bsizey_v; ++y)
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
                data[5][x][y] = std::min<pti>(game.isInEncl(p, on_move), 2) * 0.5f;
                data[6][x][y] = std::min<pti>(game.isInEncl(p, opponent), 2) * 0.5f;
                if (compressed_data.planes_v > 7)
                {
                    data[7][x][y] =
                        std::min<pti>(game.isInBorder(p, on_move), 2) * 0.5f;
                    data[8][x][y] =
                        std::min<pti>(game.isInBorder(p, opponent), 2) * 0.5f;
                    data[9][x][y] =
                        std::min(game.getTotalSafetyOf(p), 2.0f) * 0.5f;
                    if (compressed_data.planes_v > 10)
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
            if (which2 < compressed_data.planes_v)
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
            if (which < compressed_data.planes_v)
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

        for (int m = 0; m < compressed_data.moves_used_v; ++m)
        {
            for (int x = 0; x < compressed_data.bsizex_v; ++x)
                for (int y = 0; y < compressed_data.bsizey_v; ++y)
                    datum.labels[m][x][y] = 0.0f;
            for (auto& [move, value] : moves.at(m))
            {
                int move_isom = applyIsometry(move.ind, isometry);
                datum.labels[m][coord.x[move_isom]][coord.y[move_isom]] = value;
            }
            /*
            int move_isom = applyIsometry(moves.at(m).ind, isometry);
            int label = coord.x[move_isom] * compressed_data.bsizey_v +
                        coord.y[move_isom];
            if (label < 0 or
                label >= compressed_data.bsizex_v * compressed_data.bsizey_v)
                throw std::runtime_error("label");
            // collector.save(m, label);
            datum.labels[m] = label;
            */
        }

        auto s = datum.serialise();
        compressed_data.save(std::move(s));
    }
}

std::vector<MoveValue> getMovesFromSgfNodePropertyLB(const SgfNode& node)
{
    int who = -1;
    if (node.findProp("B") != node.props.end())
        who = 1;
    else if (node.findProp("W") != node.props.end())
        who = 2;
    else
        return {};
    const auto iter = node.findProp("LB");
    if (iter == node.props.end()) return {};
    const auto& values = iter->second;
    std::vector<MoveValue> result;
    if (values.empty()) return result;
    float total_sims = 0.0f;
    for (const auto& moveStr : values)
    {
        constexpr auto index_of_sims = 3u;
        const int sims = atoi(moveStr.c_str() + index_of_sims);
        total_sims += sims;
        Move move;
        move.who = who;
        move.ind = coord.sgfToPti(moveStr);
        result.push_back({move, sims});
    }
    std::sort(result.begin(), result.end(),
              [](auto p, auto q) { return p.second > q.second; });
    const int threshold_N = 15;
    const float threshold = 0.05 * total_sims;
    const int to = std::min<int>(threshold_N, result.size());
    const auto stop_at =
        std::find_if(std::next(result.begin()), result.begin() + to,
                     [threshold](auto p) { return p.second < threshold; });
    result.erase(stop_at, result.end());
    const float sum_of_elems_left =
        std::accumulate(result.begin(), result.end(), 0.0f,
                        [](float sum, auto p) { return sum + p.second; });
    for (auto& el : result)
    {
        el.second /= sum_of_elems_left;
    }
    return result;
}

std::pair<Move, std::vector<std::string>> getMoveFromSgfNode(
    const Game& game, const SgfNode& node)
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

std::vector<MoveValue> getMovesFromSgfNodeUsingBW_or_LB(const Game& game,
                                                        const SgfNode& node)
{
    std::vector<MoveValue> res = getMovesFromSgfNodePropertyLB(node);
    if (not res.empty()) return res;
    auto [move, points_to_enclose] = getMoveFromSgfNode(game, node);
    if (move.ind == 0) return {};
    res.push_back(MoveValue{move, 1.0f});
    return res;
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

bool isNumberOfEmpty3x3SquaresAtLeast(const Game& game, int threshold)
{
    int count = 0;
    for (pti i = coord.first; i <= coord.last; ++i)
        if (coord.dist[i] >= 1 and game.whoseDotMarginAt(i) == 0 and
            game.getPattern3_at(i) == 0)
        {
            count++;
            if (count >= threshold) return true;
        }
    return false;
}

template <typename CompressedDataCont>
void gatherDataFromSgfSequence(CompressedDataCont& compressed_data,
                               SgfSequence& seq,
                               const std::map<int, bool>& whichSide,
                               bool must_surround)
{
    const auto [x, y] = getSize(seq[0]);
    if (x != compressed_data.bsizex_v or y != compressed_data.bsizey_v)
    {
        std::cout << "  ... size: " << x << "x" << y << std::endl;
        return;
    }
    const unsigned start_from = 5;
    const unsigned go_to = std::min<unsigned>(
        (x * y * 3) / 5,  // use moves until 60% of board is full
        seq.size() - compressed_data.moves_used_v);
    Game game(SgfSequence(seq.begin(), seq.begin() + start_from), go_to,
              must_surround);
    for (unsigned i = start_from; i < go_to; ++i)
    {
        if (whichSide.at(game.whoNowMoves()))
        {
            std::vector<std::vector<MoveValue>> subsequentMoves;
            bool moves_are_correct = true;
            for (int j = 0; j < compressed_data.moves_used_v; ++j)
            {
                auto moves_with_values =
                    getMovesFromSgfNodeUsingBW_or_LB(game, seq[i + j]);
                if (moves_with_values.empty() or
                    moves_with_values[0].first.ind == 0 or
                    moves_with_values[0].first.who !=
                        (j % 2 == 0 ? game.whoNowMoves()
                                    : 3 - game.whoNowMoves()))
                {
                    moves_are_correct = false;
                    break;
                }
                subsequentMoves.push_back(std::move(moves_with_values));
            }
            if (moves_are_correct)
            {
                gatherDataFromPosition(compressed_data, game, subsequentMoves);
            }
        }
        if (not isNumberOfEmpty3x3SquaresAtLeast(game,
                                                 2))  // is board almost full?
            break;
        // std::cerr << "Trying to play at: " << seq[i].toString() << std::endl;
        game.replaySgfSequence({seq[i]}, 1);
    }
}

std::vector<std::string> split(const std::string& buf, const std::string& key)
{
    std::vector<std::string> res;
    std::size_t pos1 = 0;
    for (;;)
    {
        std::size_t pos2 = buf.find(key, pos1);
        auto subs = buf.substr(pos1, pos2 - pos1);
        if (not subs.empty()) res.push_back(std::move(subs));
        pos1 = pos2 + key.size();
        if (pos2 == std::string::npos) break;
    }
    return res;
}
