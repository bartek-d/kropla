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

#include "board.h"
#include "game_utils.h"
#include "enclosure.h"
#include "threats.h"
#include "patterns.h"
#include "safety.h"

#include <vector>
#include <map>
#include <string>
#include <atomic>
#include <set>
#include <list>
#include <random>
#include <chrono>

typedef std::set<pti, std::greater<pti>> PointsSet;

/********************************************************************************************************
  Worm description class
*********************************************************************************************************/

struct WormDescr {
  pti dots[2];  // number of dots of players
  pti leftmost; // the dot of the worm with the smallest index (the top one from the leftmost)
  pti group_id; // some positive number which is common for worms in the same group (i.e., connected) and different otherwise
  int32_t safety;  // safety info, ==0 not safe, ==1 partially safe, >=2 safe
  bool isSafe() const { return safety >= 2; };
  const static int32_t SAFE_VALUE = 20000;        // safety := SAFE_VALUE when the worm touches the edge
  const static int32_t SAFE_THRESHOLD = 10000;
  std::vector<pti> neighb;   // numbers of other worms that touch this one
  std::vector<uint64_t> opp_threats;  // zobrist keys of opp's threats that include this worm
  //  void addOppThreatZobrist(uint64_t z);
  //void removeOppThreatZobrist(uint64_t z);
  std::string show() const;
};

/********************************************************************************************************
  Connections class
*********************************************************************************************************/
struct OneConnection {
  std::array<pti,4> groups_id {0,0,0,0};    // id's of connected groups, the same may appear more than once,
                                  // 0-filled at the end if necessary
  int code {0};       // code of the neighbourhood used by coord.connections_tab
  // int() and != are mainly for debugging, to print and check connections
  operator int() const { return (groups_id[0]!=0) + (groups_id[1]!=0) + (groups_id[2]!=0) + (groups_id[3]!=0); };
  bool operator!=(const OneConnection& other) const;
  // returns the number of groups in the neighbourhood
  int count() { int count=0; while (count<4 && groups_id[count]!=0) count++; return count; };
  int getUniqueGroups(std::array<pti,4> &ug) const;
};

/********************************************************************************************************
  Movestats class for handling Monte Carlo tree.
*********************************************************************************************************/

struct Movestats {
  std::atomic<int32_t> playouts;
  std::atomic<real_t> value_sum;
  Movestats() { playouts = 0;   value_sum = 0; };
  const Movestats& operator+=(const Movestats& other);
  Movestats& operator=(const Movestats&);
  bool operator<(const Movestats& other) const;
  std::string show() const;
};

/********************************************************************************************************
  Treenode class for handling Monte Carlo tree.
*********************************************************************************************************/
struct Treenode {
  Treenode* parent;
  //std::vector<Treenode> children;
  std::atomic<Treenode*> children;
  Movestats t;
  Movestats amaf;
  Movestats prior;
  Move move;
  uint32_t flags;
  static const uint32_t LAST_CHILD = 1;
  real_t getValue() const;
  bool operator<(const Treenode& other) const;
  void markAsLast() { flags |= LAST_CHILD; };
  void markAsNotLast() { flags &= ~LAST_CHILD; };
  bool isLast() { return (flags & LAST_CHILD) !=0; };
  const Treenode* getBestChild() const;
  std::string show() const;
  std::string getMoveSgf() const;
  Treenode() { flags=0;   children=nullptr;  parent=nullptr; };
  Treenode& operator=(const Treenode&);
  Treenode(const Treenode& other) { *this = other; };
};

/********************************************************************************************************
  TreenodeAllocator class for thread-safe memory allocation in Monte Carlo.
*********************************************************************************************************/
class TreenodeAllocator {
  std::list< Treenode* > pools;
  const int pool_size = 100000;
  int min_block_size;
  int last_block_start;
  int cursor;
public:
  TreenodeAllocator();
  ~TreenodeAllocator();
  Treenode* getNext();
  Treenode* getLastBlock();
  void copyPrevious();
  static int getSize(Treenode *ch);
};

/********************************************************************************************************
  PossibleMoves class for keeping track of possible moves
*********************************************************************************************************/
namespace PossibleMovesConsts
{
  constexpr int LIST_NEUTRAL{0};
  constexpr int LIST_DAME    = 1;
  constexpr int LIST_TERRM   = 2;
  constexpr int LIST_REMOVED = 3;  // although there's no list for that
  constexpr int MASK_SHIFT  = 12;
  constexpr int INDEX_MASK = (1 << MASK_SHIFT) - 1;  // 0xfff
  //
  constexpr int NEUTRAL = LIST_NEUTRAL << MASK_SHIFT;  // 0x0000;
  constexpr int DAME    = LIST_DAME << MASK_SHIFT;     // 0x1000;
  constexpr int TERRM   = LIST_TERRM << MASK_SHIFT;      // 0x2000;
  constexpr int REMOVED = LIST_REMOVED << MASK_SHIFT;  // 0x3000;
  constexpr int TYPE_MASK = (NEUTRAL | DAME | TERRM | REMOVED);   // 0x3000;
};

class PossibleMoves {
  std::vector<pti> mtype;   // type of move OR-ed with its index on the list (neutral, dame or bad)
  void removeFromList(pti p);
  enum class EdgeType { LEFT, TOP, RIGHT, BOTTOM };
  void newDotOnEdge(pti p, EdgeType edge);
public:
  std::vector<pti> lists[3];  // neutral, dame, bad;
  bool left, top, right, bottom;  // are margins empty?

  void generate();
  void changeMove(pti p, int new_type);
};

/********************************************************************************************************
  InterestingMoves -- class for keeping track of 3 lists of interesting moves, very similar to PossibleMoves
*********************************************************************************************************/
namespace InterestingMovesConsts
{
  constexpr int LIST_0 = 0;
  constexpr int LIST_1 = 1;
  constexpr int LIST_2 = 2;
  constexpr int LIST_REMOVED = 3;  // although there's no list for that
  constexpr int MASK_SHIFT  = 12;
  constexpr int INDEX_MASK = (1 << MASK_SHIFT) - 1;  // 0xfff
  //
  constexpr int MOVE_0 = LIST_0 << MASK_SHIFT;  // 0x0000;
  constexpr int MOVE_1 = LIST_1 << MASK_SHIFT;     // 0x1000;
  constexpr int MOVE_2 = LIST_2 << MASK_SHIFT;      // 0x2000;
  constexpr int REMOVED = LIST_REMOVED << MASK_SHIFT;  // 0x3000;
  constexpr int TYPE_MASK = (MOVE_0 | MOVE_1 | MOVE_2 | REMOVED);   // 0x3000;
};

class InterestingMoves {
  std::vector<pti> mtype;   // type of move OR-ed with its index on the list (0, 1 or 2)
  void removeFromList(pti p);
public:
  std::vector<pti> lists[3];  // list0, list1, list2

  void generate();
  void changeMove(pti p, int new_type);
  int classOfMove(pti p) const;
};


class Game {
  std::vector<pti> worm;
  std::vector<pti> nextDot;
  std::map<pti, WormDescr> descr;
  AllThreats threats[2];
  std::vector<OneConnection> connects[2];
  Score score[2];
  int lastWormNo[2];  // lastWormNo used in worm, for players 1,2
  int nowMoves; // =1 or 2
  std::vector<pti> history;
  static const pti HISTORY_TERR = 0x4000;  // this is OR-ed with history[...] to denote that someone played inside own terr or encl
  static const pti HISTORY_ENCL_BORDER = 0x2000; // this is OR-ed with history[...] to denote that someone played on the border of their encl, possibly enclosing sth
  static const pti history_move_MASK = ~(HISTORY_ENCL_BORDER | HISTORY_TERR);
  //
  std::vector<pti> recalculate_list;
  PossibleMoves possible_moves;
  InterestingMoves interesting_moves;
  std::vector<pattern3_val> pattern3_value[2];
  std::vector<pattern3_t> pattern3_at;
  static thread_local std::default_random_engine engine;
  // fields for functions generating list of moves (ml prefix, 'move list')
  std::vector<ThrInfo> ml_priorities;
  std::vector<uint64_t> ml_deleted_opp_thr;
  std::vector<ThrInfo> ml_priority_vect;
  std::vector<pti> ml_special_moves;
  std::vector<std::shared_ptr<Enclosure> > ml_encl_moves;
  std::vector<std::shared_ptr<Enclosure> > ml_opt_encl_moves;
  std::vector<uint64_t> ml_encl_zobrists;
  int update_soft_safety{0};   // if safety_soft needs to be recalculated after dot+(enclosure), and which margins
  Safety safety_soft;
  int dame_moves_so_far {0};
  // worm[] has worm-id if it is >= 4 && <= MASK_WORM_NO,
  // worm[] & MASK_DOT can have 4 values: 0=empty, 1,2 = dots, 3=point outside the board
  static const int MASK_DOT = 3;
  static const int CONST_WORM_INCR = 4;  // =MASK_DOT+1
  static const int MASK_WORM_NO = 0xfff;
  static const int MASK_MARK = 0x2000;   // used by some functions
  static const int MASK_BORDER = 0x4000; //
  //
  static const int COEFF_URGENT = 4;
  static const int COEFF_NONURGENT = 1;
  //
  static const int MC_EXPAND_THRESHOLD = 8;
  static const int VIRTUAL_LOSS = 1;
#ifdef DEBUG_SGF
public:
  static SgfTree sgf_tree;
  std::string getSgf() { return sgf_tree.toString(); };
  std::string getSgf_debug() { return sgf_tree.toString_debug(true); };
private:
#endif
  //
  void initWorm();
  void wormMergeAny(pti dst, pti src);
  void wormMergeOther(pti dst, pti src);
  void wormMergeSame(pti dst, pti src);
  void wormMerge_common(pti dst, pti src);
  int floodFillExterior(std::vector<pti> &tab, pti mark_by, pti stop_at) const;
  static const constexpr float COST_INFTY = 1000.0;
  std::vector<pti> findImportantMoves(pti who);
  float costOfPoint(pti p, int who) const;
  float floodFillCost(int who) const;
  std::vector<pti> findThreats_preDot(pti ind, int who);
  std::array<int,2> findThreats2moves_preDot__getRange(pti ind, pti nb, int i, int who) const;
  std::array<int,5> findClosableNeighbours(pti ind, pti forbidden1, pti forbidden2, int who) const;
  void addClosableNeighbours(std::vector<pti> &tab, pti p0, pti p1, pti p2, int who) const;
  bool haveConnection(pti p1, pti p2, int who) const;
  std::vector<pti> findThreats2moves_preDot(pti ind, int who);
  void checkThreat_encl(Threat* thr, int who);
  std::shared_ptr<Enclosure> checkThreat_terr(Threat* thr, pti p, int who);
  void checkThreats_postDot(std::vector<pti> &newthr, pti ind, int who);
  void checkThreat2moves_encl(Threat* thr, pti where0, int who);
  int addThreat2moves(pti ind0, pti ind1, int who, std::shared_ptr<Enclosure> &encl);
  void checkThreats2moves_postDot(std::vector<pti> &newthr, pti ind, int who);
  void addThreat(Threat&& t, int who);
  void removeMarked(int who);
  void removeMarkedAndAtPoint(pti ind, int who);
  bool removeAtPoint(pti ind, int who);
  void subtractThreat(const Threat& t, int who);
  void pointNowInDanger2moves(pti ind, int who);
  void pointNowSafe2moves(pti ind, int who);
  bool isSafeFor(pti ind, int who) const;
  void connectionsRecalculateCode(pti ind, int who);
  void connectionsRecalculateConnect(pti ind, int who);
  void connectionsRecalculatePoint(pti ind, int who);
  void connectionsRecalculateNeighb(pti ind, int who);
  void connectionsRenameGroup(pti dst, pti src);
  pattern3_t getPattern3_at(pti ind) const;
  bool isEmptyInDirection(pti ind, int direction) const;
  bool isEmptyInNeighbourhood(pti ind) const;
  pattern3_val getPattern3Value(pti ind, int who) const;
  void showPattern3Values(int who) const;
  real_t getPattern52Value(pti ind, int who) const;
  void showPattern52Values(int who) const;
  void pattern3recalculatePoint(pti ind);
  void recalculatePatt3Values();
  int checkLadderStep(pti x, PointsSet &ladder_breakers, pti v1, pti v2, pti escaping_group, bool ladder_ext, int escapes, int iteration=0);
  void getEnclMoves(std::vector<std::shared_ptr<Enclosure> > &encl_moves, std::vector<std::shared_ptr<Enclosure> > &opt_encl_moves,
		    std::vector<uint64_t> &encl_zobrists,
		    pti move, int who);
  bool appendSimplifyingEncl(std::vector<std::shared_ptr<Enclosure>> &encl_moves, uint64_t &zobrists, int who);
  void getSimplifyingEnclAndPriorities(int who);
  int checkBorderMove(pti ind, int who) const;
  int checkBorderOneSide(pti ind, pti viter, pti vnorm, int who) const;
  void possibleMoves_updateSafety(pti p);
  void possibleMoves_updateSafetyDame();
  int checkInterestingMove(pti p) const;
  int checkDame(pti p) const;
  std::vector<pti> getPatt3extraValues() const;
public:
  static const constexpr real_t increase_komi_threshhold = 0.75;
  static const constexpr real_t decrease_komi_threshhold = 0.15;
  static const int start_increasing = 200;
  Game() = delete;
  Game(SgfSequence seq, int max_moves);
  int whoNowMoves() { return nowMoves; };
  void replaySgfSequence(SgfSequence seq, int max_moves);
  void placeDot(int x, int y, int who);
  Enclosure findNonSimpleEnclosure(std::vector<pti> &tab, pti point, pti mask, pti value) const;
  Enclosure findNonSimpleEnclosure(pti point, pti mask, pti value);
  Enclosure findSimpleEnclosure(std::vector<pti> &tab, pti point, pti mask, pti value) const;
  Enclosure findSimpleEnclosure(pti point, pti mask, pti value);
  Enclosure findEnclosure(std::vector<pti> &tab, pti point, pti mask, pti value) const;
  Enclosure findEnclosure(pti point, pti mask, pti value);
  Enclosure findEnclosure_notOptimised(std::vector<pti> &tab, pti point, pti mask, pti value) const;
  Enclosure findEnclosure_notOptimised(pti point, pti mask, pti value);
  Enclosure findInterior(std::vector<pti> border) const;
  void makeEnclosure(const Enclosure& encl, bool remove_it_from_threats);
  std::pair<int, int> countTerritory(int now_moves) const;
  std::pair<int, int> countTerritory_simple(int now_moves) const;
  std::pair<int16_t, int16_t> countDotsTerrInEncl(const Enclosure& encl, int who, bool optimise = true) const;
  std::pair<Move, std::vector<std::string>> extractSgfMove(std::string m, int who) const;
  void makeSgfMove(std::string m, int who);
  void makeMove(Move &m);
  void makeMoveWithPointsToEnclose(Move &m, std::vector<std::string> to_enclose);

  bool isDotAt(pti ind) const { assert(worm[ind] >= 0 && worm[ind] <= MASK_WORM_NO);  return (worm[ind] >= CONST_WORM_INCR); }
  int whoseDotMarginAt(pti ind) const { return (worm[ind] & MASK_DOT); }
  int whoseDotAt(pti ind) const { int v[4]={0,1,2,0};  return v[worm[ind] & MASK_DOT]; }
  int getSafetyOf(pti ind) const { return descr.at(worm[ind]).safety; }
  pattern3_t readPattern3_at(pti ind) const { return pattern3_at[ind]; }
  void generateListOfMoves(TreenodeAllocator &alloc, Treenode *parent, int depth, int who);
  Move getRandomEncl(Move &m);
  Move chooseAtariMove(int who);
  Move chooseAtariResponse(pti lastMove, int who);
  Move chooseSoftSafetyResponse(int who);
  Move chooseSoftSafetyContinuation(int who);
  Move selectMoveRandomlyFrom(const std::vector<pti>& moves, int who);
  Move choosePattern3Move(pti move0, pti move1, int who);
  Move chooseSafetyMove(int who);
  Move chooseAnyMove(int who);
  std::vector<pti> getGoodTerrMoves(int who) const;
  Move chooseAnyMove_pm(int who);
  Move chooseInterestingMove(int who);
  Move choosePatt3extraMove(int who);
  Move getLastMove() const;
  Move getLastButOneMove() const;
  real_t randomPlayout();
  void descend(TreenodeAllocator &alloc, Treenode *node, int depth, bool expand);

  bool isDame_directCheck(pti p, int who) const;
  bool isDame_directCheck_symm(pti p) const;
  bool checkRootListOfMovesCorrectness(Treenode *children) const;
  bool checkWormCorrectness() const;
  bool checkSoftSafetyCorrectness();
  bool checkThreatCorrectness();
  bool checkThreat2movesCorrectness();
  bool checkConnectionsCorrectness();
  bool checkPossibleMovesCorrectness(int who) const;
  bool checkCorrectness(SgfSequence seq);
  bool checkPattern3valuesCorrectness() const;

  void seedRandomEngine(int newseed) { engine.seed(newseed); };
  void findConnections();

  void Test();
  void show() const;
  void showSvg();
  void showConnections();
  void showGroupsId();
  void showThreats2m();
  void showPattern3extra();

  std::string showDescr(pti p) const { return descr.at(p).show();  };

  friend class Safety;
};

namespace global {
extern Pattern3 patt3;
extern Pattern3 patt3_symm;
extern Pattern3 patt3_cost;
extern  Pattern3extra_array patt3_extra;
extern Pattern52 patt52_edge;
extern  Pattern52 patt52_inner;
extern int komi;     // added to terr points of white (i.e. > 0 -> good for white), komi=2 -> 1 dot
extern int komi_ratchet;
}

extern int debug_allt2m, debug_sing_smallt2m, debug_sing_larget2m, debug_skippedt2m;
extern int debug_n, debug_N;
extern long long debug_nanos;
extern long long debug_nanos2;
extern long long debug_nanos3;
extern int debug_previous_count;

extern std::chrono::high_resolution_clock::time_point start_time;
