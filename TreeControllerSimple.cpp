#include "TreeControllerSimple.h"
#include "TreeController.h"
#include <iostream>
#include <utility>
#include <set>
#include <map>
#include <cassert>
#include <cstdlib> // std::abort()
using namespace std;

void TreeControllerSimple::process(InputColumn const &ic, unsigned step_)
{
    step = step_;
    PointerTree::PointerNode *root = t.root();
    reduce(root, ic);
    if (debug && !dotfile.empty())
        t.outputDOT(dotfile + "_reduced", step);

    deTagAll(t.root());
    resolveNonBinary(root);
    t.clearNonBranchingInternalNodes();
    if (debug && !dotfile.empty())
        t.outputDOT(dotfile + "_resolvednonbinaryr", step);
    if (debug)
        t.validate();

    recombine.clear();
    findReduced(root, 1);
    updatedThisStep = recombine;
    
    deTagAll(t.root());
    recombine.clear();
    recombine.reserve(2 * t.size());
    findReduced(t.root(), 1);
    nOnesCut += recombine.size()-1;
    recombineSubtrees(t.root(), true, false);
    
    for (vector<PointerTree::PointerNode *>::iterator it = updatedThisStep.begin(); it != updatedThisStep.end(); ++it)
        if (!(*it)->leaf())
            (*it)->previousUpdate(step);
    t.clearNonBranchingInternalNodes();
}


void TreeControllerSimple::process(InputColumn const &ic, unsigned step_, TreeDistance &dist)
{
    step = step_;
    PointerTree::PointerNode *root = t.root();
    reduce(root, ic);
    t.updateMaxDists();
    
    if (debug && !dotfile.empty())
        t.outputDOT(dotfile + "_reduced", step);

    deTagAll(t.root());
    resolveNonBinary(root);
    t.clearNonBranchingInternalNodes();
    if (debug && !dotfile.empty())
        t.outputDOT(dotfile + "_resolvednonbinaryr", step);
    if (debug)
        t.validate();

    recombine.clear();
    findReduced(root, 1);
    updatedThisStep = recombine;

    dist.recomputeDistances(ic);
    
    deTagAll(t.root());
    recombine.clear();
    recombine.reserve(2 * t.size());
    findReduced(t.root(), 1);
    nOnesCut += recombine.size()-1;
    recombineSubtrees(t.root(), true, false, dist);
    
    for (vector<PointerTree::PointerNode *>::iterator it = updatedThisStep.begin(); it != updatedThisStep.end(); ++it)
        if (!(*it)->leaf())
            (*it)->previousUpdate(step);
    t.clearNonBranchingInternalNodes();
}

void TreeControllerSimple::rewind(InputColumn const &ic, unsigned step_, TreeEnumerator *te)
{
    step = step_;
    PointerTree::PointerNode *root = t.root();
    reduce(root, ic);

    if (te && te->initialize())
        te->initialize(t, step+1);

    if (!dotfile.empty() && debug)
        t.outputDOT(dotfile + "_prerewind", step);
    t.prerewind(step, te);

    recombine.clear();
    findReduced(root, 1);
    unsigned nones = recombine.size();
    if (te && nones == 1)
    {
        PointerTree::PointerNode *pn = recombine[0];
        if (!pn->root())
            te->insertMutation(pn->parentPtr(), pn->uniqueId(), step + 1);
    }

    recombine.clear();
    findReduced(root, 0);
    unsigned nzeros = recombine.size();

    if (nones > 1 && nzeros > 1)
    {
        unsigned rootzeros = 0;
        for (vector<PointerTree::PointerNode *>::iterator it = recombine.begin(); it != recombine.end(); ++it)
            if ((*it)->parentPtr() == t.root())
                ++rootzeros;
        recombine.clear();
        findReduced(root, 1);
        unsigned rootones = 0;
        PointerTree::PointerNode *nonrootparent = 0;
        for (vector<PointerTree::PointerNode *>::iterator it = recombine.begin(); it != recombine.end(); ++it)
            if ((*it)->parentPtr() == t.root())
                ++rootones;
            else
                nonrootparent = (*it)->parentPtr();

        unsigned nonrparentones = 0;
        for (vector<PointerTree::PointerNode *>::iterator it = recombine.begin(); it != recombine.end(); ++it)
            if ((*it)->parentPtr() == nonrootparent)
                ++nonrparentones;
        
        cerr << "assert failed at TreeControllerSimple::rewind(): step = " << step
             << "    nzeros = " << nzeros << ", nones = " << nones << endl
             << " root zeros = " << rootzeros << ", root ones = " << rootones << endl
             << " nonroot ones = " << nonrparentones << endl;
        
        abort();
    }

    t.rewind(step, te);
    t.clearNonBranchingInternalNodes();
    if (!dotfile.empty() && debug)
        t.outputDOT(dotfile + "_rewind", step);

    if (debug)
        t.validate();
}

/**
 * Reduce the tree
 *
 * Updates all the 
 *     0-1-count values in the given tree, and
 *     removes tags from binary nodes' children
 */
pair<int, int> TreeControllerSimple::reduce(PointerTree::PointerNode *pn, InputColumn const &ic)
{
    assert(!pn->leaf() || pn->leafId() != ~0u);
    if (pn->ghostbranch())
        return make_pair(0,0);

    if (pn->leaf())
    {
        // Sets the new column leaf label
        pn->setLabel(ic[pn->leafId()]);
        return make_pair(pn->nZeros(), pn->nOnes());
    }
    
    if (pn->size() == 0)
    {
        // New ghost branch
        assert(pn->numberOfRefs() > 0);
        assert(pn->nZeros() > 0 || pn->nOnes() > 0);
        pn->nZeros(0); // Becomes a ghost
        pn->nOnes(0);
        return make_pair(0,0);
    }
    
    int nzeros = 0;
    int nones = 0;
    unsigned tagged = 0;
    unsigned nonghosts = 0;
    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
    {
        pair<int, int> tmp = reduce(*it, ic);
        nzeros += tmp.first;
        nones += tmp.second;
        if (tmp.first + tmp.second > 0)
        {
            ++ nonghosts;
            if ((*it)->tagged())
                ++ tagged;
        }
    }
    pn->nZeros(nzeros);
    pn->nOnes(nones);
    if (!pn->root() && nonghosts <= 2 && tagged > 0)
        for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
            (*it)->tagged(false);

    return make_pair(nzeros, nones);
}

/**
 * Resolve non-binary nodes locally
 *
 * These modifications are not tracked in history events.
 * Only non-tagged subtrees are considered.
 */
void TreeControllerSimple::resolveNonBinary(PointerTree::PointerNode *pn)
{
    if (pn->leaf() || pn->reduced() || pn->ghostbranch())
        return;

    // Depth first
    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
        resolveNonBinary(*it);

    // Modify local structure
    recombine.clear();
    int nones = 0;
    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
        if (!(*it)->tagged() && (*it)->reduced() && (*it)->reducedLabel() == 1)
        {
            nones += (*it)->nOnes(); // FIXME If *it is a unary ghost, gets merged with other reduced subtrees
            recombine.push_back(*it);
        }
    if (recombine.size() > 1)
        recombineNonBinarySubtrees(nones, false, true); // Not included in history events; does keep parent counter 
}

void TreeControllerSimple::findReduced(PointerTree::PointerNode *pn, InputLabel il)
{
    if (pn->ghostbranch())
        return;
    if (pn->leaf())
    {
        if (pn->reducedLabel() == il)
            recombine.push_back(pn);
        return;
    }
    
    bool unarypath = false;
    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
    {
        if ((*it)->nZeros() == pn->nZeros() && (*it)->nOnes() == pn->nOnes())
            unarypath = true;
    }
    if (!unarypath && pn->reduced() && !pn->root())
    {
        if (pn->reducedLabel() == il)
            recombine.push_back(pn);
        return;
    }

    // Assert: 'pn' is an unary ghost node; or a nonreducible node; or the root node
    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
        findReduced(*it, il);
}

// Relocates all selected subtrees under new internal node
void TreeControllerSimple::recombineNonBinarySubtrees(unsigned nones, bool keephistory, bool keepparentcounts)
{
    if (recombine.size() < 2)
        return; // One subtree cannot be relocated to itself

    // Choose the largest, non-tagged subtree as destination
    int msize = 0;
    PointerTree::PointerNode *mpn = 0;
    for (vector<PointerTree::PointerNode *>::iterator it = recombine.begin(); it != recombine.end(); ++it)
        if (!(*it)->tagged() && (*it)->nZeros() + (*it)->nOnes() > msize)
        {
            msize = (*it)->nZeros()+(*it)->nOnes();
            mpn = *it;
        }
    assert (mpn != 0);
    
    // Count the number of tags younger than the target subtree
    unsigned dest_updated = mpn->previousUpdate();
    if (mpn->previousUpdate() == PointerTree::nohistory)
        dest_updated = 0;    
    unsigned ntags = 0;
    for (vector<PointerTree::PointerNode *>::iterator it = recombine.begin(); it != recombine.end(); ++it)
        if ((*it)->tagged() && *it != mpn)
        {
            unsigned flt_created = t.getPreviousEventStep(*it);
            if (flt_created >= dest_updated)
                ntags++;
        }
       
    // Old version PointerTree::PointerNode * dest = mpn;
    //if (mpn->leaf() || (ntags < recombine.size()-1 && !mpn->tagged()))
        //    dest = t.createDest(mpn, step); // Create new internal node if leaf, or not all siblings are tags and destination is not tagged
    //if (dest->tagged())
    //    keephistory = true;

    PointerTree::PointerNode * dest = mpn;
    dest = t.createDest(mpn, step);

    for (vector<PointerTree::PointerNode *>::iterator it = recombine.begin(); it != recombine.end(); ++it)
        if (*it != mpn)
            t.relocate(*it, dest, step, step, keephistory, keepparentcounts); // Relocate all selected 'recombine' subtrees to destination
    
    dest->nOnes(nones);
    dest->nZeros(0);
    if (!dest->parentPtr()->root() && dest->parentPtr()->size() <= 2)
        for (PointerTree::PointerNode::iterator it = dest->parentPtr()->begin(); it != dest->parentPtr()->end(); ++it)
            (*it)->tagged(false);
    if (dest->size() <= 2)
        for (PointerTree::PointerNode::iterator it = dest->begin(); it != dest->end(); ++it)
            (*it)->tagged(false);
    if (dest!=mpn && mpn->size() <= 2)
        for (PointerTree::PointerNode::iterator it = mpn->begin(); it != mpn->end(); ++it)
            (*it)->tagged(false);
}

void TreeControllerSimple::assignLabels(InputColumn const &ic)
{
    reduce(t.root(), ic);
}   

// Relocates all selected subtrees to the largest selected subtree
void TreeControllerSimple::recombineSubtrees(PointerTree::PointerNode *subtree_root, bool keephistory, bool keepparentcounts)
{
    if (recombine.size() < 2)
        return; // One subtree cannot be relocated to itself

    // Choose the largest, non-tagged subtree as destination
    int msize = 0; 
    PointerTree::PointerNode *mpn = 0;
    for (vector<PointerTree::PointerNode *>::iterator it = recombine.begin(); it != recombine.end(); ++it)
	if (!(*it)->tagged() && (*it)->nZeros()+(*it)->nOnes() > msize)
	{
            msize = (*it)->nZeros()+(*it)->nOnes();
            mpn = *it;
	}
	  
    assert (mpn != 0);
    
    map<PointerTree::PointerNode *, unsigned> upward_path; // Lowest common ancestor candidates between mpn and rest of the subtrees
    PointerTree::PointerNode *tmp = mpn;
    unsigned largest_age = mpn->previousUpdate();
    if (largest_age == PointerTree::nohistory)
        largest_age = 0;
    while (tmp != subtree_root)
    {
        tmp = tmp->parentPtr();
        if (tmp->previousUpdate() > largest_age && tmp->previousUpdate() != PointerTree::nohistory)
            largest_age = tmp->previousUpdate();
        upward_path[tmp] = largest_age;
    }
    
    PointerTree::PointerNode * dest = mpn;
    if (mpn->leaf())
        dest = t.createDest(mpn, PointerTree::nohistory);
    for (vector<PointerTree::PointerNode *>::iterator it = recombine.begin(); it != recombine.end(); ++it)
        if (*it != mpn)
        {
            unsigned lca_age = 0;
            if (upward_path.count((*it)->parentPtr()) == 0)
                (*it)->tagged(false); // Not tagged for this dest.
            else
                lca_age = upward_path[(*it)->parentPtr()];

            t.relocate(*it, dest, lca_age, step, keephistory, keepparentcounts); // Relocate all selected 'recombine' subtrees to destination
        }
    if (mpn->leaf())
        dest->previousUpdate(step);
}


// Relocates all selected subtrees to the largest selected subtree
void TreeControllerSimple::recombineSubtrees(PointerTree::PointerNode *subtree_root, bool keephistory, bool keepparentcounts, TreeDistance &dist)
{
    if (recombine.size() < 2)
        return; // One subtree cannot be relocated to itself

    // Choose the destination
    double eval = -1;
    int msize = 0;
    PointerTree::PointerNode *mpn = 0;
    for (vector<PointerTree::PointerNode *>::iterator it = recombine.begin(); it != recombine.end(); ++it)
        if (!(*it)->tagged())
        {
            double e = dist.evaluateDistance(recombine, subtree_root, *it);
            if (printEval)
                cerr << "eval = " << e << " for dest = " << (*it)->nodeId() << ", isleaf = " << (*it)->leaf() << endl;
            if (eval == -1 || e < eval || (e == eval && msize < (*it)->nZeros()+(*it)->nOnes()))
            {
                eval = e;
                msize = (*it)->nZeros()+(*it)->nOnes();
                mpn = *it;
            }
        }

    
    assert(mpn!= 0);
    
    map<PointerTree::PointerNode *, unsigned> upward_path; // Lowest common ancestor candidates between mpn and rest of the subtrees
    PointerTree::PointerNode *tmp = mpn;
    unsigned largest_age = mpn->previousUpdate();
    if (largest_age == PointerTree::nohistory)
        largest_age = 0;
    while (tmp != subtree_root)
    {
        tmp = tmp->parentPtr();
        if (tmp->previousUpdate() > largest_age  && tmp->previousUpdate() != PointerTree::nohistory)
            largest_age = tmp->previousUpdate();
        upward_path[tmp] = largest_age;
    }
    
    PointerTree::PointerNode * dest = mpn;
    if (mpn->leaf())
        dest = t.createDest(mpn, PointerTree::nohistory);
    for (vector<PointerTree::PointerNode *>::iterator it = recombine.begin(); it != recombine.end(); ++it)
        if (*it != mpn)
        {
            unsigned lca_age = 0;
            if (upward_path.count((*it)->parentPtr()) == 0)
                (*it)->tagged(false); // Not tagged for this dest.
            else
                lca_age = upward_path[(*it)->parentPtr()];

            t.relocate(*it, dest, lca_age, step, keephistory, keepparentcounts); // Relocate all selected 'recombine' subtrees to destination
        }
    if (mpn->leaf())
        dest->previousUpdate(step);
}


// Update the tags in the whole tree
void TreeControllerSimple::deTagAll(PointerTree::PointerNode *pn)
{
    pn->tagged(false);
    if (pn->leaf() || pn->ghostbranch())
        return;

    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
        deTagAll(*it);
}

// Counts the number of active internal nodes
unsigned countInternals(PointerTree::PointerNode *pn)
{
    if (pn->leaf())
        return 0;
    if (pn->ghostbranch())
        return 0;
    
    bool unarypath = false;
    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
        if ((*it)->nZeros() == pn->nZeros() && (*it)->nOnes() == pn->nOnes())
            unarypath = true;
    
    unsigned g = 0;
    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
        g += countInternals(*it);
    if (!unarypath)
        g++;
    return g;
}
unsigned TreeControllerSimple::countInternalNodes()
{
    return ::countInternals(t.root());
}

// Counts the number of active nodes
unsigned TreeControllerSimple::countActive(PointerTree::PointerNode *pn)
{
    unsigned g = 0;
    if (pn->leaf())
        return 0;
    if (pn->ghostbranch())
        return 0;

    g++;    
    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
        g += countActive(*it);
    return g;
}


// Counts the number of ghostbranch nodes (1 or 0 children)
unsigned TreeControllerSimple::countGhostBranches(PointerTree::PointerNode *pn)
{
    unsigned g = 0;
    if (pn->leaf())
        return 0;
    if (pn->ghostbranch())
        return 1;

    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
        g += countGhostBranches(*it);
    return g;
}

// Counts the number of non-ghostbranch unary-nodes (1 or 0 children)
unsigned TreeControllerSimple::countUnaryGhosts(PointerTree::PointerNode *pn)
{
    unsigned g = 0;
    if (pn->leaf())
        return 0;
    if (pn->ghostbranch())
        return 0;

    if (pn->size() == 1)
        g++;

    for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
        g += countUnaryGhosts(*it);
    return g;
}

// Counts the number of non-ghostbranch unary-nodes (1 or 0 children)
unsigned TreeControllerSimple::countBranchingGhosts(PointerTree::PointerNode *pn)
{
    unsigned g = 0;
    if (pn->leaf())
        return 0;
    if (pn->ghostbranch())
        return 0;

    if (pn->size() == 1)
        for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
            g += countBranchingGhosts(*it);
    else
    {
        unsigned nonghosts = 0;
        for (PointerTree::PointerNode::iterator it = pn->begin(); it != pn->end(); ++it)
        {
            if (!(*it)->ghostbranch())
                nonghosts++;
            g += countBranchingGhosts(*it);
        }
        if (nonghosts==1)
            g++;
    }
    return g;
}

