#pragma once

#include <array>
#include <tuple>
#include <vector>
#include <queue>
#include "board.h"

const int POINT_INFLUENCE_SIZE = 40;

class Influence {
public:
  struct InfluenceAtPoint {
    float v;
    int32_t p;
  };
  struct PointInfluence {
    std::array<InfluenceAtPoint, POINT_INFLUENCE_SIZE> list;
    int size {0};
    int table_no {-1};
    float sum {0.0};
  };
  struct Tuple {
    pti p, dir;
    int32_t dist;
    bool operator<(const Tuple& other) const { return dist > other.dist; };  // warning: > to make closer points of higher priority!
  };
  std::priority_queue<Tuple> queue;
private:
  std::vector<PointInfluence> influence_from;   // to keep influence contribution from individual points
public:
  bool turned_off;
  std::vector<float> influence[3];
  Influence() { turned_off = false; };
  Influence(int n);
  void allocMem(int n);
  std::vector<float> working;
  void changePointInfluence(PointInfluence new_pi, int ind);
  bool checkInfluenceFromAt(PointInfluence &other, int ind) const;
};
