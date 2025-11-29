/********************************************************************************************************
 kropla -- a program to play Kropki; file game.h.
    Copyright (C) 2015,2016,2017,2018 Bartek Dyda,
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

#pragma once

#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "../3rdparty/short_alloc.h"
#include "board.h"
#include "enclosure.h"
#include "game_utils.h"
#include "patterns.h"
#include "safety.h"
#include "simplegame.h"
#include "threats.h"

/********************************************************************************************************
  Movestats class for handling Monte Carlo tree.
*********************************************************************************************************/

struct NonatomicMovestats
{
    int32_t playouts{0};
    real_t value_sum{0.0f};
    const NonatomicMovestats& operator+=(const NonatomicMovestats& other);
    NonatomicMovestats& operator=(const NonatomicMovestats&) = default;
    bool operator==(const NonatomicMovestats&) const = default;
    void normaliseTo(int32_t N);
    std::string show() const;
};

struct Movestats
{
    std::atomic<int32_t> playouts{0};
    std::atomic<real_t> value_sum{0.0f};
    const Movestats& operator+=(const Movestats& other);
    const Movestats& operator+=(const NonatomicMovestats& other);
    Movestats& operator=(const Movestats&);
    Movestats& operator=(const NonatomicMovestats&);
    bool operator<(const Movestats& other) const;
    std::string show() const;
};

/********************************************************************************************************
  DebugInfo class for tree nodes.
*********************************************************************************************************/
struct DebugInfo
{
    std::unordered_map<uint64_t, std::string> zobrist2priors_info;
};

/********************************************************************************************************
  Treenode class for handling Monte Carlo tree.
*********************************************************************************************************/
class Game;
struct Treenode
{
    Treenode* parent{nullptr};
    // std::vector<Treenode> children;
    std::atomic<Treenode*> children{nullptr};
    std::shared_ptr<Game> game_ptr{nullptr};
    Movestats t;
    Movestats amaf;
    Movestats prior;
    Move move;
    uint32_t flags{0};
    float cnn_prob{-1.0};
    std::mutex children_mutex;
    static const uint32_t LAST_CHILD = 0x10000;
    static const uint32_t IS_DAME = 0x20000;
    static const uint32_t IS_INSIDE_TERR_NO_ATARI =
        0x40000;  // move is inside someone's terr, but does not save
                  // from/create atari
    static const uint32_t DEPTH_MASK = 0xffff;
    real_t getValue() const;
    bool operator<(const Treenode& other) const;
    void markAsLast() { flags |= LAST_CHILD; }
    void markAsNotLast() { flags &= ~LAST_CHILD; }
    bool isLast() const { return (flags & LAST_CHILD) != 0; }
    void markAsDame() { flags |= IS_DAME; }
    bool isDame() const { return (flags & IS_DAME) != 0; }
    void markAsInsideTerrNoAtari() { flags |= IS_INSIDE_TERR_NO_ATARI; }
    bool isInsideTerrNoAtari() const
    {
        return (flags & IS_INSIDE_TERR_NO_ATARI) != 0;
    }
    bool isInsideTerrNoAtariOrDame() const
    {
        return isInsideTerrNoAtari() or isDame();
    }
    void setDepth(uint32_t depth) { flags |= (depth & DEPTH_MASK); }
    uint32_t getDepth() const { return (flags & DEPTH_MASK); }
    uint32_t getVirtualLoss() const
    {
        const auto depth = getDepth();
        if (depth == 0) return 0;
        return 2;
    }
    const Treenode* getBestChild() const;
    std::string show() const;
    std::string showParents() const;
    std::string getMoveSgf() const;
    Treenode() = default;
    Treenode& operator=(const Treenode&);
    Treenode(const Treenode& other) { *this = other; }
};

/********************************************************************************************************
  TreenodeAllocator class for thread-safe memory allocation in Monte Carlo.
*********************************************************************************************************/
class TreenodeAllocator
{
    std::list<Treenode*> pools;
    const int pool_size = 100000;
    int min_block_size;
    int last_block_start;
    int cursor;

   public:
    TreenodeAllocator();
    ~TreenodeAllocator();
    Treenode* getNext();
    Treenode* getLastBlock();
    Treenode* getLastBlockWithoutResetting() const;
    void copyPrevious();
    static int getSize(Treenode* ch);
};

/********************************************************************************************************
  PossibleMoves class for keeping track of possible moves
*********************************************************************************************************/
namespace PossibleMovesConsts
{
constexpr int LIST_NEUTRAL{0};
constexpr int LIST_DAME = 1;
constexpr int LIST_TERRM = 2;
constexpr int LIST_REMOVED = 3;  // although there's no list for that
constexpr int MASK_SHIFT = 12;
constexpr int INDEX_MASK = (1 << MASK_SHIFT) - 1;  // 0xfff
//
constexpr int NEUTRAL = LIST_NEUTRAL << MASK_SHIFT;            // 0x0000;
constexpr int DAME = LIST_DAME << MASK_SHIFT;                  // 0x1000;
constexpr int TERRM = LIST_TERRM << MASK_SHIFT;                // 0x2000;
constexpr int REMOVED = LIST_REMOVED << MASK_SHIFT;            // 0x3000;
constexpr int TYPE_MASK = (NEUTRAL | DAME | TERRM | REMOVED);  // 0x3000;
}  // namespace PossibleMovesConsts

class PossibleMoves
{
    std::vector<pti> mtype;  // type of move OR-ed with its index on the list
                             // (neutral, dame or bad)
    void removeFromList(pti p);
    enum class EdgeType
    {
        LEFT,
        TOP,
        RIGHT,
        BOTTOM
    };
    void newDotOnEdge(pti p, EdgeType edge);

   public:
    std::vector<pti> lists[3];      // neutral, dame, bad;
    bool left, top, right, bottom;  // are margins empty?

    void generate();
    void changeMove(pti p, int new_type);
};

/********************************************************************************************************
  InterestingMoves -- class for keeping track of 3 lists of interesting moves,
very similar to PossibleMoves
*********************************************************************************************************/
namespace InterestingMovesConsts
{
constexpr int LIST_0 = 0;
constexpr int LIST_1 = 1;
constexpr int LIST_2 = 2;
constexpr int LIST_REMOVED = 3;  // although there's no list for that
constexpr int MASK_SHIFT = 12;
constexpr int INDEX_MASK = (1 << MASK_SHIFT) - 1;  // 0xfff
//
constexpr int MOVE_0 = LIST_0 << MASK_SHIFT;                     // 0x0000;
constexpr int MOVE_1 = LIST_1 << MASK_SHIFT;                     // 0x1000;
constexpr int MOVE_2 = LIST_2 << MASK_SHIFT;                     // 0x2000;
constexpr int REMOVED = LIST_REMOVED << MASK_SHIFT;              // 0x3000;
constexpr int TYPE_MASK = (MOVE_0 | MOVE_1 | MOVE_2 | REMOVED);  // 0x3000;
}  // namespace InterestingMovesConsts

class InterestingMoves
{
    std::vector<pti>
        mtype;  // type of move OR-ed with its index on the list (0, 1 or 2)
    void removeFromList(pti p);

   public:
    std::vector<pti> lists[3];  // list0, list1, list2

    void generate();
    void changeMove(pti p, int new_type);
    int classOfMove(pti p) const;
};

class Game
{
    SimpleGame sg;

   public:
    AllThreats threats[2];

   private:
    //
    std::vector<pti> recalculate_list;
    PossibleMoves possible_moves;
    InterestingMoves interesting_moves;
    std::vector<pattern3_val> pattern3_value[2];
    std::vector<pattern3_t> pattern3_at;
    static thread_local std::default_random_engine engine;
    // fields for functions generating list of moves (ml prefix, 'move list')
    std::vector<ThrInfo> ml_priorities;
    std::vector<ThrInfo> ml_priority_vect;
    std::vector<pti> ml_special_moves;
    std::vector<std::shared_ptr<Enclosure>> ml_encl_moves;
    std::vector<std::shared_ptr<Enclosure>> ml_opt_encl_moves;
    std::vector<uint64_t> ml_encl_zobrists;
    int update_soft_safety{0};
    int dame_moves_so_far{0};
    static const int COEFF_URGENT = 4;
    static const int COEFF_NONURGENT = 1;
    bool must_surround{false};
    uint64_t zobrist{0};
    // for debugging:
    static thread_local std::stringstream out;

#ifdef DEBUG_SGF
   public:
    static SgfTree sgf_tree;
    std::string getSgf() { return sgf_tree.toString(); };
    std::string getSgf_debug() { return sgf_tree.toString_debug(true); };

   private:
#endif
    //
    int floodFillExterior(std::vector<pti>& tab, pti mark_by,
                          pti stop_at) const;
    static const constexpr float COST_INFTY = 1000.0;
    std::vector<pti> findImportantMoves(pti who);
    float costOfPoint(pti p, int who) const;
    float floodFillCost(int who) const;
    std::vector<pti> findThreats_preDot(pti ind, int who);
    std::array<int, 2> findThreats2moves_preDot__getRange(pti ind, pti nb,
                                                          int i, int who) const;
    std::array<int, 5> findClosableNeighbours(pti ind, pti forbidden1,
                                              pti forbidden2, int who) const;
    void addClosableNeighbours(std::vector<pti>& tab, pti p0, pti p1, pti p2,
                               int who) const;
    bool haveConnection(pti p1, pti p2, int who) const
    {
        return sg.haveConnection(p1, p2, who);
    }
    const std::vector<OneConnection>& getConnects(int ind) const
    {
        return sg.getConnects(ind);
    }

    std::vector<pti> findThreats2moves_preDot(pti ind, int who);
    SmallMultimap<7, 7> getEmptyPointsCloseToIndTouchingSomeOtherGroup(
        const SmallMultiset<pti, 4>& connected_groups, pti ind, int who) const;
    void checkThreat_encl(Threat* thr, int who);
    bool checkIfThreat_encl_isUnnecessary(Threat* thr, pti ind, int who) const;
    void checkThreat_terr(Threat* thr, pti p, int who,
                          std::vector<int8_t>* done = nullptr);
    void checkThreats_postDot(std::vector<pti>& newthr, pti ind, int who);
    void checkThreat2moves_encl(Threat* thr, pti where0, int who);
    int addThreat2moves(pti ind0, pti ind1, int who, Enclosure&& encl);
    void checkThreats2moves_postDot(std::vector<pti>& newthr, pti ind, int who);
    void addThreat(Threat&& t, int who);
    void removeMarked(int who);
    void removeMarkedAndAtPoint(pti ind, int who);
    bool removeAtPoint(pti ind, int who);
    void subtractThreat(const Threat& t, int who);
    void pointNowInDanger2moves(pti ind, int who);
    void pointNowSafe2moves(pti ind, int who);
    bool isSafeFor(pti ind, int who) const;
    bool isEmptyInDirection(pti ind, int direction) const;
    bool isEmptyInNeighbourhood(pti ind) const;
    pattern3_val getPattern3Value(pti ind, int who) const;
    void showPattern3Values(int who) const;
    real_t getPattern52Value(pti ind, int who) const;
    void showPattern52Values(int who) const;
    void pattern3recalculatePoint(pti ind);
    void recalculatePatt3Values();
    int checkLadderStep(pti x, krb::PointsSet& ladder_breakers, pti v1, pti v2,
                        pti escaping_group, bool ladder_ext, int escapes,
                        int iteration = 0) const;
    std::pair<pti, pti> checkLadderToFindBadOrGoodMoves() const;
    void getEnclMoves(std::vector<std::shared_ptr<Enclosure>>& encl_moves,
                      std::vector<std::shared_ptr<Enclosure>>& opt_encl_moves,
                      std::vector<uint64_t>& encl_zobrists, pti move, int who);
    bool appendSimplifyingEncl(
        std::vector<std::shared_ptr<Enclosure>>& encl_moves, uint64_t& zobrists,
        int who);
    void getSimplifyingEnclAndPriorities(int who);
    int checkBorderMove(pti ind, int who) const;
    int checkBorderOneSide(pti ind, pti viter, pti vnorm, int who) const;
    void possibleMoves_updateSafety(pti p);
    void possibleMoves_updateSafetyDame();
    int checkInterestingMove(pti p) const;
    int checkDame(pti p) const;

   public:
    Game() = delete;
    Game(SgfSequence seq, int max_moves, bool must_surround = false);
    const SimpleGame& getSimpleGame() const { return sg; }
    int whoNowMoves() const { return sg.whoNowMoves(); };
    void replaySgfSequence(SgfSequence seq, int max_moves);
    void placeDot(int x, int y, int who);
    Enclosure findNonSimpleEnclosure(std::vector<pti>& tab, pti point, pti mask,
                                     pti value) const;
    Enclosure findNonSimpleEnclosure(pti point, pti mask, pti value);
    Enclosure findSimpleEnclosure(std::vector<pti>& tab, pti point, pti mask,
                                  pti value) const;
    Enclosure findSimpleEnclosure(pti point, pti mask, pti value);
    Enclosure findEnclosure(std::vector<pti>& tab, pti point, pti mask,
                            pti value) const;
    Enclosure findEnclosure(pti point, pti mask, pti value);
    Enclosure findEnclosure_notOptimised(std::vector<pti>& tab, pti point,
                                         pti mask, pti value) const;
    Enclosure findEnclosure_notOptimised(pti point, pti mask, pti value);
    Enclosure findInterior(std::vector<pti> border) const;
    void makeEnclosure(const Enclosure& encl, bool remove_it_from_threats);
    std::pair<int, int> countTerritory(int now_moves) const;
    std::pair<int, int> countTerritory_simple(int now_moves) const;
    std::pair<int16_t, int16_t> countDotsTerrInEncl(const Enclosure& encl,
                                                    int who,
                                                    bool optimise = true) const;
    std::pair<Move, std::vector<std::string>> extractSgfMove(std::string m,
                                                             int who) const;
    void makeSgfMove(const std::string& m, int who);
    void makeMove(const Move& m);
    void makeMoveWithPointsToEnclose(
        const Move& m, const std::vector<std::string>& to_enclose);
    bool isDotAt(pti ind) const { return sg.isDotAt(ind); }
    int whoseDotMarginAt(pti ind) const { return sg.whoseDotMarginAt(ind); }
    int whoseDotAt(pti ind) const { return whoseDotAt(ind); }

    pti isInTerr(pti ind, int who) const;
    pti isInEncl(pti ind, int who) const;
    pti isInBorder(pti ind, int who) const;
    std::tuple<int, pti, pti> checkLadder(int who_defends, pti where) const;

    using Edge = std::pair<pti, pti>;
    struct EdgeInfo
    {
        int capacity;
        int flow;
    };
    std::map<Edge, EdgeInfo> findCapAndFlow(pti source, int who, pti sink,
                                            pti outerMask, int infty) const;
    int findNumberOfDotsToEncloseBy(pti ind, int who, int infty) const;

   private:
    pti& isInTerr(pti ind, int who);
    pti& isInEncl(pti ind, int who);
    pti& isInBorder(pti ind, int who);

   public:
    bool isDameOnEdge(pti i, int who) const;
    int getSafetyOf(pti ind) const { return sg.getSafetyOf(ind); }
    float getTotalSafetyOf(pti ind) const { return sg.getTotalSafetyOf(ind); }
    pattern3_t readPattern3_at(pti ind) const { return pattern3_at[ind]; }
    pattern3_t getPattern3_at(pti ind) const;
    uint64_t getZobrist() const { return zobrist; }
    const History& getHistory() const { return sg.getHistory(); }
    NonatomicMovestats priorsAndDameForPattern3(bool& is_dame, bool is_root,
                                                bool is_in_our_te,
                                                bool is_in_opp_te, int i,
                                                int who) const;
    NonatomicMovestats priorsAndDameForEdgeMoves(bool& is_dame, bool is_root,
                                                 int i, int who) const;
    NonatomicMovestats priorsForInterestingMoves_cut_or_connect(bool is_root,
                                                                int i) const;
    NonatomicMovestats priorsForDistanceFromLastMoves(bool is_root,
                                                      int i) const;
    NonatomicMovestats priorsForThreats(bool is_root, bool is_in_opp_te, int i,
                                        int who) const;
    NonatomicMovestats priorsForLadderExtension(bool is_root, int i,
                                                int who) const;

    DebugInfo generateListOfMoves(TreenodeAllocator& alloc, Treenode* parent,
                                  int depth, int who);
    Move getRandomEncl(Move& m);
    Move chooseAtariMove(int who, pti forbidden_place);
    Move chooseAtariResponse(pti lastMove, int who, pti forbidden_place);
    Move chooseSoftSafetyResponse(int who, pti forbidden_place);
    Move chooseSoftSafetyContinuation(int who, pti forbidden_place);
    Move selectMoveRandomlyFrom(const std::vector<pti>& moves, int who,
                                pti forbidden_place);
    Move choosePattern3Move(pti move0, pti move1, int who, pti forbidden_place);
    std::vector<pti> getSafetyMoves(int who, pti forbidden_place);
    Move chooseSafetyMove(int who, pti forbidden_place);
    Move chooseAnyMove(int who, pti forbidden_place);
    std::vector<pti> getGoodTerrMoves(int who) const;
    Move chooseAnyMove_pm(int who, pti forbidden_place);
    Move chooseInterestingMove(int who, pti forbidden_place);
    Move chooseLastGoodReply(int who, pti forbidden_place);
    Move getLastMove() const;
    Move getLastButOneMove() const;
    real_t randomPlayout();
    void rollout(Treenode* node, int depth);

    std::default_random_engine& getRandomEngine();

    bool isDame_directCheck(pti p, int who) const;
    bool isDame_directCheck_symm(pti p) const;
    bool checkRootListOfMovesCorrectness(Treenode* children) const;
    bool checkMarginsCorrectness() const;
    bool checkWormCorrectness() const;
    bool checkSoftSafetyCorrectness();
    bool checkThreatCorrectness();
    bool checkThreat2movesCorrectness();
    bool checkConnectionsCorrectness();
    bool checkPossibleMovesCorrectness() const;
    bool checkCorrectness(SgfSequence seq);
    bool checkPattern3valuesCorrectness() const;

    void seedRandomEngine(int newseed) { engine.seed(newseed); }
    const AllThreats& getAllThreatsForPlayer(int who) const
    {
        return threats[who];
    }
    void show() const;
    void show(const std::vector<pti>& moves) const;
    void showSvg(const std::string& filename,
                 const std::vector<pti>& tab) const;
    void showConnections();
    void showGroupsId();
    void showThreats2m();

    std::string showDescr(pti p) const { return sg.descr.at(p).show(); }

    friend class Safety;
    friend class GroupNeighbours;
};

namespace global
{
extern const Pattern3 patt3;
extern const Pattern3 patt3_symm;
extern const Pattern3 patt3_cost;
extern Pattern52 patt52_edge;
extern Pattern52 patt52_inner;
extern int komi;  // added to terr points of white (i.e. > 0 -> good for white),
                  // komi=2 -> 1 dot
extern int komi_ratchet;
extern std::string program_path;
}  // namespace global

extern int debug_allt2m, debug_sing_smallt2m, debug_sing_larget2m,
    debug_skippedt2m;
extern int debug_n, debug_N;
extern long long debug_nanos;
extern long long debug_nanos2;
extern long long debug_nanos3;
extern int debug_previous_count;

extern std::chrono::high_resolution_clock::time_point start_time;
