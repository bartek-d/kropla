#include "influence.h"

/********************************************************************************************************
  Influence class
*********************************************************************************************************/

Influence::Influence(int n) { allocMem(n); }

void Influence::allocMem(int n)
{
    influence[0] = std::vector<float>(n, 0.0);
    influence[1] = std::vector<float>(n, 0.0);
    influence[2] = std::vector<float>(n, 0.0);
    working = std::vector<float>(n, 0.0);
    //  PointInfluence pi;
    influence_from = std::vector<PointInfluence>(n, PointInfluence());
    turned_off = false;
}

void Influence::changePointInfluence(PointInfluence new_pi, int ind)
{
    // the assumption is that new dots may only decrease influence, so if the
    // sum (and table_no) are still the same, then the new influence must be the
    // same as the old one
    if (new_pi.sum == influence_from[ind].sum &&
        new_pi.table_no == influence_from[ind].table_no)
        return;
    // first subtract old influence
    for (int i = 0; i < influence_from[ind].size; i++)
    {
        influence[influence_from[ind].table_no]
                 [influence_from[ind].list[i].p] -=
            influence_from[ind].list[i].v;
    }
    // save and add new influence
    influence_from[ind] = new_pi;
    for (int i = 0; i < influence_from[ind].size; i++)
    {
        influence[influence_from[ind].table_no]
                 [influence_from[ind].list[i].p] +=
            influence_from[ind].list[i].v;
    }
}

bool Influence::checkInfluenceFromAt(PointInfluence &other, int ind) const
{
    return (other.sum == influence_from[ind].sum &&
            (other.table_no == influence_from[ind].table_no || other.sum == 0));
}
