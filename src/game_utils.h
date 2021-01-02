#pragma once

#include <cassert>

/********************************************************************************************************
  Cleanup class
*********************************************************************************************************/
template <typename Container, typename T>
class Cleanup {
  Container c;
  T andMask;
public:
  Cleanup(Container &cont, T am) : c(cont), andMask(am) { };
  ~Cleanup() { for (auto &e : c) e&=andMask; };
};

template <typename Container, typename T>
class CleanupUsingList {
  Container c;
  T andMask;
public:
  std::array<pti, Coord::maxSize> list;
  int count {0};
  CleanupUsingList(Container &cont, T am) : c(cont), andMask(am) {};
  void push(pti p) { list[count++] = p; };
  ~CleanupUsingList() { for (int i=0; i<count; i++) c[list[i]]&=andMask; };
};

template <typename Container, typename T>
class CleanupUsingListOfValues {
  Container c;
public:
  std::array<pti, Coord::maxSize> point;
  std::array<T, Coord::maxSize> values;
  int count {0};
  CleanupUsingListOfValues(Container &cont) : c(cont) {};
  void push(pti p, T val) { point[count] = p;  values[count] = val;  ++count;  };
  ~CleanupUsingListOfValues() { for (int i=count-1; i>=0; --i) c[point[i]] = values[i]; };
};

template <typename T>
class CleanupOneVar {
  T* ref_value;
  T saved_value;
public:
  CleanupOneVar(T* ref, T new_val) : ref_value(ref), saved_value(*ref) { *ref = new_val; }
  ~CleanupOneVar() { *ref_value = saved_value; }
};

/********************************************************************************************************
  SmallMultiset class
*********************************************************************************************************/
template <class T, int N>
class SmallMultiset {
  std::array<T, N> data;
  int count {0};
public:
  void insert(T x);
  int remove_one(T x);
  bool contains(T x) const;
  int size() { return count; };
  bool empty() { return (count==0); };
  void clear() { count=0; };
  std::string show();
};

template <class T, int N>
int getCapacity(const SmallMultiset<T, N>&)
{
  return N;
};

template <class T, int N>
void SmallMultiset<T, N>::insert(T x)
{
  assert(count < N);
  data[count++] = x;
}

template <class T, int N>
int SmallMultiset<T, N>::remove_one(T x)
{
  for (int i=0; i<count; ++i) {
    if (data[i] == x) {
      data[i] = data[--count];
      return 1;
    }
  }
  return 0;
}

template <class T, int N>
bool SmallMultiset<T, N>::contains(T x) const
{
  for (int i=0; i<count; ++i)
    if (data[i] == x) return true;
  return false;
}

template <class T, int N>
std::string SmallMultiset<T, N>::show()
{
  std::stringstream out;
  out << "{ ";
  std::string separator = "";
  for (auto &i : data) {
    out << separator;
    if (std::is_same<T, pti>::value) {
      out << coord.showPt(i);
    } else {
      out << i;
    }
    separator = ", ";
  }
  out << "}";
  return out.str();
}

template class SmallMultiset<pti, 4>;

