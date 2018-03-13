
#if !defined(__SGF_H)
#define __SGF_H

/********************************************************************************************************
  SgfParser class and classes for keeping the sgf.
*********************************************************************************************************/
typedef std::pair<std::string, std::vector<std::string> >  SgfProperty;
  
struct SgfNode {
  std::vector<SgfProperty> props;
  std::list<std::shared_ptr<SgfNode> > children;
  std::weak_ptr<SgfNode> parent;
  SgfNode() : props(), children(), parent() {};
  std::string toString(bool mainVar=false) const;
  std::vector<SgfProperty>::iterator findProp(const std::string pr);
};

typedef std::vector<SgfNode>  SgfSequence;

class SgfParser {
  std::string input;
  int pos;
  const static int eof = -1;
  int eatChar();
  int checkChar() const;
  void eatWS();
  std::string propValue();
  std::string propIdent();
  SgfProperty property();
  SgfNode node();
public:
  SgfParser(std::string s) { input=s;  pos=0;  };
  SgfSequence parseMainVar();
};

/********************************************************************************************************
  Sgf tree
*********************************************************************************************************/
class SgfTree {
  std::shared_ptr<SgfNode> root;
  std::weak_ptr<SgfNode> cursor;
  std::weak_ptr<SgfNode> saved_cursor;
  SgfProperty partial_move;
  std::string comment;
public:
  SgfTree();
  void changeBoardSize(int x, int y);
  void addChild(std::vector<SgfProperty> &&prs, bool as_first=false);
  void addComment(std::string cmt);
  void addProperty(SgfProperty prop);
  void makeMove(SgfProperty move);
  void makePartialMove(SgfProperty move);
  void makePartialMove_addEncl(std::string sgf_encl);
  void finishPartialMove();
  void saveCursor();
  void restoreCursor();
  std::string toString(bool mainVar=false);
  std::string toString_debug(bool mainVar=false);
};

#endif
