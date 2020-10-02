#include <vector>
#include <array>
#include <map>
#include <cassert>
#include <cstdint>    // intXX_t types
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <algorithm>
//#include <exception>
#include <stdexcept>
#include <cctype>  // iswhite()
//#include <typeinfo>

typedef int16_t  pti;  // point int, i.e., int suitable for coordinates

/*
 Produces an array temp[j] of ints with the following entries:
   j encodes the position, each 2 bits: 0=empty, 1,2=players dot, 3=outside; the order of bits: NE, E, SE, S, SW, W, NW, N.
 Then temp[j] == 0, if the centre point does not connect 2 groups of player 1,
 otherwise, temp[j] encodes numbers of points (0-7) in four subsequent 3-bit-groups (always the last, so the next point does
 not belong to player 1 and might be enclosed); so it uses 12 bits.
 Bit 13 says if this is an edge pattern.
 If there are less then 4 groups connected, then the last number is repeated.
 If the pattern touches the edge, then only the point after the first might be enclosed.

 And also array simplified[j][4] with the entries similar as above, but:
  j encodes the position, each bit: 0=empty, 1=player dot;
 temp[j] == {n0, n1, n2, n3},  n--positions of four groups (or -1, if less than 4 groups).
 Also possible that n0>=0 but n1==n2==n3==-1, unlike temp[j].
*/



/********************************************************************************************************
  Colour constants.
*********************************************************************************************************/
const std::string colour_off = "\033[0m";
const std::string black = "\033[30m";
const std::string red = "\033[31m";
const std::string green = "\033[32m";
const std::string yellow = "\033[33m";
const std::string blue = "\033[34m";
const std::string magenta = "\033[35m";
const std::string cyan = "\033[36m";
const std::string white = "\033[37m";
// background colours:
const std::string black_b = "\033[40m";
const std::string red_b = "\033[41m";
const std::string green_b = "\033[42m";
const std::string yellow_b = "\033[43m";
const std::string blue_b = "\033[44m";
const std::string magenta_b = "\033[45m";
const std::string cyan_b = "\033[46m";
const std::string white_b = "\033[47m";

/********************************************************************************************************
  Coord class for manipulating coordinates.
*********************************************************************************************************/

class Coord {
public:
  int wlkx=15;
  int wlky=15;
  const static int maxx = 40;
  const static int maxy = 40;
  const static int maxSize = (maxx+2)*(maxy+1) +1;
  static int N;
  static int S;
  static int W;
  static int E;
  static int NE, NW, SE, SW;
  static pti nb4[4];  // neighbours (N, E, S, W)
  static pti nb8[8];  // 8 neighbours, order is important for some functions (NE, E, ... -- clockwise)
  static pti first, last;  // smallest and largest coordinate possible for a point inside the board
  Coord(int x, int y);
  pti ind(int x, int y) const { return (x+1)*(wlky+1) + y+1; };
  pti findNextOnRight(pti x0, pti y) const;
  std::array<int8_t, maxSize> x;
  std::array<int8_t, maxSize> y;
  std::array<int8_t, maxSize> dist;
  void initPtTabs();
  int getSize() const { return (wlkx+2)*(wlky+1) +1; };
  void changeSize(int x, int y);
  template <typename Container> std::string showBoard(Container const &b);
  template <typename Container> std::string showFullBoard(Container const &b);
  std::string showPt(pti p) const;
  int sgfToX(std::string s) const;
  int sgfToY(std::string s) const;
  int sgfCoordToInt(char s) const;
} coord(15,15);

int Coord::N;
int Coord::S;
int Coord::W;
int Coord::E;
int Coord::NE, Coord::NW, Coord::SE, Coord::SW;
pti Coord::nb4[4];
pti Coord::nb8[8];
pti Coord::first, Coord::last;

Coord::Coord(int x, int y)
{
  changeSize(x, y);
}

void
Coord::changeSize(int x, int y)
{
  wlkx = x; wlky = y;
  initPtTabs();
  N = -1;
  S = 1;
  W = -wlky-1;
  E = wlky+1;
  NE = N+E;  NW = N+W;  SE = S+E;  SW = S+W;
  nb4[0] = N;  nb4[1] = E;  nb4[2] = S;  nb4[3] = W;
  nb8[0] = NE;  nb8[1] = E;  nb8[2] = SE;  nb8[3] = S;  nb8[4] = SW;  nb8[5] = W;  nb8[6] = NW;  nb8[7] = N;
  first = ind(0,0);
  last = ind(wlkx-1,wlky-1);
}


void
Coord::initPtTabs()
{
  int ind = 0;
  for (int i=0; i< wlkx+2; i++)
    for (int j=0; j< wlky+1; j++) {
      if (i==0 || i==wlkx+1 || j==0) {
	x[ind] = -1;
	y[ind] = -1;
	dist[ind] = -1;
      } else {
	x[ind] = i-1;
	y[ind] = j-1;
	dist[ind] = std::min( std::min(i-1, wlkx-i), std::min(j-1, wlky-j) );
      }
      ind++;
    }
  x[ind] = -1;
  y[ind] = -1;
  dist[ind] = -1;
  assert(ind == getSize()-1);
}

pti
Coord::findNextOnRight(pti x0, pti y) const
// in: x0 = centre, y = a neighbour of x0
// returns point z next to right to y (as seen from x0)
{
  auto v = y - x0;
  for (int i=0; i<8; i++) {
    if (nb8[i] == v) {
      return x0 + nb8[(i+1) & 7];
    }
  }
  std::cout << coord.showPt(x0) << "  " << coord.showPt(y) << std::endl;
  assert(0);
}

template <typename Container>
std::string
Coord::showBoard(Container const &b)
{
  std::stringstream out;
  for (int y=0; y<wlky; y++) {
    for (int x=0; x<wlkx; x++) {
      if (b[ind(x,y)]==0) out << colour_off;
      else if (b[ind(x,y)] & 1) out << blue;
      else out << red;
      out << std::setw(4) << int(b[ind(x,y)]) << colour_off;
    }
    out << std::endl;
  }
  return out.str();
}

template <typename Container>
std::string
Coord::showFullBoard(Container const &b)
{
  std::stringstream out;
  for (int y=-1; y<=wlky; y++) {
    for (int x=-1; x<=wlkx; x++) {
      if (y<wlky || x==wlkx) {
	out << std::setw(4) << int(b[ind(x,y)]);
      } else {
	out << std::setw(4) << "";
      }
    }
    out << std::endl;
  }
  return out.str();
}

std::string
Coord::showPt(pti p) const
{
  std::stringstream out;
  out << "(" << int(coord.x[p]) << ", " << int(coord.y[p]) << ")";
  return out.str();
}

int
Coord::sgfToX(std::string s) const
{
  return sgfCoordToInt(s.at(0));
}

int
Coord::sgfToY(std::string s) const
{
  return sgfCoordToInt(s.at(1));
}

int
Coord::sgfCoordToInt(char s) const
{
  if (s >= 'a' && s <= 'z') {
    return s - 'a';
  }
  if (s >= 'A' && s <= 'Z') {
    return s - 'A' + 26;
  }
  throw std::runtime_error("sgfCoordToInt: Unexpected character");
}



int UstawOtoczenie(unsigned char otoczenie[9])
// uklad pol w tablicy otoczenie:
//   0 1 2
//   3 4 5
//   6 7 8.
{
 unsigned char czyja=otoczenie[4];
 // ponumeruj nasze kropki z sasiedztwa
 for (int i=0; i<4; i++)
   if (otoczenie[i]==czyja)
     otoczenie[i]=i+1;
   else
     otoczenie[i]=0;
 for (int i=5; i<9; i++)
   if (otoczenie[i]==czyja)
     otoczenie[i]=i;
   else
     otoczenie[i]=0;
 // teraz znajdz skladowe spojnosci
 int ost=0;
 if (otoczenie[1])
   {
   //if (otoczenie[0]) otoczenie[0]=1;  // -nie trzeba, bo to i tak =0 lub 1
   otoczenie[1]=1;
   if (otoczenie[2]) otoczenie[2]=1;
   if (otoczenie[3]) otoczenie[3]=1;
   if (otoczenie[5]) otoczenie[5]=1;
   ost=1;
   }
 if (otoczenie[3])
   {
   // [0] i [1] nie trzeba zmieniac
   otoczenie[3]=1;
   if (otoczenie[6]) otoczenie[6]=1;
   if (otoczenie[7]) otoczenie[7]=1;
   ost=1;
   }
 if (otoczenie[5])
   {
   if (!otoczenie[1] && (otoczenie[7]!=1)) ost++;
   // [1] nie trzeba zmieniac
   if (otoczenie[2]) otoczenie[2]=ost;
   otoczenie[5]=ost;
   if (otoczenie[7]) otoczenie[7]=ost;
   if (otoczenie[8]) otoczenie[8]=ost;
   }
 if (otoczenie[7]==7)
   {
   ost++;
   // [3] i [5] nie trzeba zmieniac - one sa w tym przypadku =0
   if (otoczenie[6]) otoczenie[6]=ost;
   otoczenie[7]=ost;
   if (otoczenie[8]) otoczenie[8]=ost;
   }
 else
   if (otoczenie[7])
     {
     if (otoczenie[6]) otoczenie[6]=otoczenie[7];
     if (otoczenie[8]) otoczenie[8]=otoczenie[7];
     }
 // ustaw ew. narozniki
 if (otoczenie[0] && !otoczenie[1] && !otoczenie[3])
   otoczenie[0]=++ost;
 if (otoczenie[2]==3)
   otoczenie[2]=++ost;
 if (otoczenie[6]==6)
   otoczenie[6]=++ost;
 if (otoczenie[8]==8)
   otoczenie[8]=++ost;
 return ost;
}

int DwaBity(int x, int odkad)
{
  int maska = (1 << odkad);
  maska |= (maska <<1);
  return (x & maska) >> odkad;
}

int pokazSzabl(int p, int wart)
{
  int kol[9] = {6, 7, 0, 5, -1, 1, 4, 3, 2};
  for (int w=0; w<9; w+=3) {
    for (int k=0; k<3; k++) {
      std::cout << colour_off;
      for (int m=0; m<=9; m+=3) {
	if ((wart & (7 << m)) == (kol[w+k] << m)) {
	  std::cout << ((m==0) ? cyan : red);
	  break;
	}
      }
      if (w+k == 4) {
	std::cout << std::setw(4) << ".";
      } else {
	std::cout << std::setw(4) << (( p & (3 << (2*kol[w+k])) ) >> (2*kol[w+k]));
      }
    }
    std::cout << std::endl << colour_off;
  }
  std::cout << std::endl;

}


int main()
{
  unsigned char ot[9];
  unsigned int buf[256*256];
  int simple_mask = 0xaaaa;
  int simple[256][4];
  int simple_count = 0;
  for (int p=0; p<256*256; p++) {
    buf[p] = 0;
    ot[0] = DwaBity(p, 12);
    ot[1] = DwaBity(p, 14);
    ot[2] = DwaBity(p, 0);
    ot[3] = DwaBity(p, 10);
    ot[4] = 1;
    ot[5] = DwaBity(p, 2);
    ot[6] = DwaBity(p, 8);
    ot[7] = DwaBity(p, 6);
    ot[8] = DwaBity(p, 4);
    int maska = 1, wynik=0;
    for (int j=0; j<=8; j++) {
      if (j==4) j++;
      if (ot[j] == 3) {
	wynik |= maska;
	ot[j] = 0;
      }
      maska <<= 1;
    }
    int banda;
    switch (wynik) {
    case 7:
      banda =1 ;  //gorna
      break;
    case 0x94:
      banda =2;   // prawa
      break;
    case 0xe0:
      banda =3;   // dolna
      break;
    case 0x29:
      banda=4;  // lewa
      break;
    case 0x2f:  case 0x97:  case 0xf4:  case 0xe9:
      banda = 5;  // naroznik
      break;
    case 0:
      banda = 0;
      break;
    default:
      banda = -1;
      break;
    }
    if (banda >=0)  {
      int ile = UstawOtoczenie(ot);
      int kolejn[] = {2, 5, 8, 7, 6, 3, 0, 1, 2};
      int numery[9] = {6, 7, 0, 5, -1, 1, 4, 3, 2};
      // wez jakis niezerowy element, zanim poprawimy narozniki
      int nonzero = -1;
      for (int i=0; i<8; i++) {
	if (ot[kolejn[i]]) { nonzero = numery[kolejn[i]]; break; }
      }
      // popraw narozniki: jesli naroznik jest pusty, a pola obok niego nie, to wypelniamy naroznik
      if (ot[0]==0 && ot[1] && ot[3]) ot[0]=ot[1];
      if (ot[2]==0 && ot[1] && ot[5]) ot[2]=ot[1];
      if (ot[8]==0 && ot[5] && ot[7]) ot[8]=ot[7];
      if (ot[6]==0 && ot[3] && ot[7]) ot[6]=ot[7];
      // ustaw odpowiednie
      bool bylo = false;
      int ost;
      int maska = 0, przes = 0, ost_numer = -1;
      for (int i=0; i<sizeof(kolejn)/sizeof(kolejn[0]); i++) {
	if (ot[kolejn[i]]) {
	  bylo = true;
	  ost = kolejn[i];
	}
	else {
	  if (bylo) {
	    maska |= (numery[ost] << przes);
	    ost_numer = numery[ost];
	    przes+=3;
	    bylo = false;
	  }
	}
      }
      if (ile >= 2) {
	// popraw, gdy na bandzie
	switch (banda) {
	case 1:    // gorna jest OK
	case 2:  // prawa tez
	  break;
	case 3:  // dolna
	  // zamien miejscami
	  {
	    int tmp = (maska &= 7);
	    maska <<= 3;
	    maska |= ost_numer;
	    ost_numer = tmp;
	  }
	  break;
	case 4:    // lewa
	  if (ost_numer == 7 || ost_numer == 0) {
	    // zamien miejscami
	    int tmp = (maska &= 7);
	    maska <<= 3;
	    maska |= ost_numer;
	    ost_numer = tmp;
	  }
	  break;
	}

	while (przes < 12) {
	  maska |= (ost_numer << przes);
	  przes += 3;
	}
	if (banda) maska |= 0x1000;
	buf[p] = maska;
	// pokaz
	/*
	if (banda>=1)	{
	  std::cout << p << " " << std::hex << buf[p] << " banda=" << banda << std::endl;
	  pokazSzabl(p, buf[p]);
	  } */
	//
      }
      if ((p & simple_mask) == 0) {
	if (ile <= 1) {
	  simple[simple_count][0] = nonzero;
	  simple[simple_count][1] = -1;
	  simple[simple_count][2] = -1;
	  simple[simple_count][3] = -1;
	  simple_count++;
	} else {
	  for (int i=0; i<4; i++) {
	    if (i<ile) {
	      simple[simple_count][i] = maska & 7;
	      maska >>= 3;
	    } else {
	      simple[simple_count][i] = -1;
	    }
	  }
	  simple_count++;
	}
      }
    }
  }
  assert(simple_count==256);
  //
  std::cout << "={" << std::endl;
  for (int i=0; i<256*256; i+=16) {
    std::cout << "   ";
    for (int j=0; j<16; j++) {
      std::cout << "0x" << std::hex << buf[i+j];
      std::cout << ", ";
    }
    std::cout << std::endl;
  }
  std::cout << "};" << std::endl;
  std::cout << std::endl;
  std::cout << "={" << std::endl;
  for (int i=0; i<256; i+=4) {
    std::cout << "   ";
    for (int j=0; j<4; j++) {
      std::cout << "{";
      for (int k=0; k<4; k++) {
	std::cout << std::dec << std::setw(2) << simple[i+j][k];
	if (k<3) std::cout << ", ";
      }
      std::cout << "}, ";
    }
    std::cout << std::endl;
  }
}

