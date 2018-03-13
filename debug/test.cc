#include <utility>   // std::pair
#include <vector>
#include <string>
#include <memory>   // unique pointer
#include <list> 
#include <sstream>
#include <algorithm>
#include <cassert>
#include <iostream>

typedef std::pair<std::string, std::vector<std::string> >  SgfProperty;

struct SgfNodeE {
  std::vector<SgfProperty> props;
  //std::list<std::shared_ptr<SgfNode> > children;
  //std::weak_ptr<SgfNode> parent;
  //SgfNodeE() : props() {}; //, children(), parent() {};
};

typedef std::vector<SgfNodeE>  SgfSequence;

int show(SgfSequence s)
{
  for (auto p : s) {
    std::cout << p.props.size();
  }
}

SgfNodeE dajN() {
  SgfNodeE n;
  //SgfProperty p = SgfProperty("FF", {"4"});
  //n.props.push_back(p);
  return n;
}

void bad(SgfSequence p)
{
  std::cout << p.size();
}

int main() {
  SgfSequence s;
  SgfNodeE n = dajN();

  for (int i=0; i<10; ++i) {
    SgfNodeE p;
    p.props.push_back(SgfProperty("aa", {"111"}));
    s.push_back(p);
  }
  s.push_back(n);
  
  show(s);

}
