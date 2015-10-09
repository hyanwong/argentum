#ifndef _TreeController_h_
#define _TreeController_h_
#include "default.h"
#include "Tree.h"
#include <vector>

/**
 * Controller to handle tree updates
 */
class TreeController
{
public:
    TreeController (PointerTree &t_, bool debug_)
        : t(t_), recombine(), debug(debug_)
    { }
    void process(InputColumn const &, unsigned);

    // Debug functionality
    unsigned countGhostBranches(PointerTree::PointerNode *);
    unsigned countUnaryGhosts(PointerTree::PointerNode *);
    unsigned countBranchingGhosts(PointerTree::PointerNode *);
        
protected:
    InputLabel reduce(PointerTree::PointerNode *, InputColumn const &);
    InputLabel reduceGhosts(PointerTree::PointerNode *);
    void resolveNonBinary(PointerTree::PointerNode *);
    void collectRecombine(PointerTree::PointerNode *);
    void recombineSubtrees(bool);
    PointerTree &t;
    std::vector<PointerTree::PointerNode *> recombine;
    bool debug;
private:
    TreeController();
    // No copy constructor or assignment
    TreeController(TreeController const&);
    TreeController& operator = (TreeController const&);
};
#endif
