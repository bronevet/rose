#include "partitions.h"
#include "abstract_object.h"
#include "compose.h"
using namespace std;

namespace dataflow {

/* ################
   ##### Part #####
   ################ */

PartPtr NULLPart;
PartEdgePtr NULLPartEdge;
  
// If the filter accepts (returns true) on any of the CFGNodes within this part, return true)
bool Part::filterAny(bool (*filter) (CFGNode cfgn))
{
  std::set<CFGNode> v=CFGNodes();
  for(std::set<CFGNode>::iterator i=v.begin(); i!=v.end(); i++) {
    if(filter(*i)) return true;
  }
  return false;
}

// If the filter accepts (returns true) on all of the CFGNodes within this part, return true)
bool Part::filterAll(bool (*filter) (CFGNode cfgn))
{
  std::set<CFGNode> v=CFGNodes();
  for(std::set<CFGNode>::iterator i=v.begin(); i!=v.end(); i++) {
    if(!filter(*i)) return false;
  }
  return true;
}

// If this and that come from the same analysis, call the type-specific equality test implemented
// in the derived class. Otherwise, these Parts are not equal.
bool Part::operator==(const PartPtr& that) const
{
  if(analysis == that->analysis) return equal(that);
  else                           return false;
}

// If this and that come from the same analysis, call the type-specific inequality test implemented
// in the derived class. Otherwise, determine inequality by comparing the analysis pointers.
bool Part::operator<(const PartPtr& that) const
{
  if(analysis == that->analysis) return less(that);
  else                           return analysis < that->analysis;
}

bool Part::operator!=(const PartPtr& that) const { return !(*this==that); }
bool Part::operator>=(const PartPtr& that) const { return !(*this<that); }
bool Part::operator<=(const PartPtr& that) const { return (*this<that) || (*this == that); }
bool Part::operator> (const PartPtr& that) const { return !(*this<=that); }

/* ####################
   ##### PartEdge #####
   #################### */


// If this and that come from the same analysis, call the type-specific equality test implemented
// in the derived class. Otherwise, these Parts are not equal.
bool PartEdge::operator==(const PartEdgePtr& that) const
{
  if(analysis == that->analysis) return equal(that);
  else                          return false;
}

// If this and that come from the same analysis, call the type-specific inequality test implemented
// in the derived class. Otherwise, determine inequality by comparing the analysis pointers.
bool PartEdge::operator<(const PartEdgePtr& that) const
{
  if(analysis == that->analysis) return less(that);
  else                           return analysis < that->analysis;
}

bool PartEdge::operator!=(const PartEdgePtr& that) const { return !(*this==that); }
bool PartEdge::operator>=(const PartEdgePtr& that) const { return !(*this<that); }
bool PartEdge::operator<=(const PartEdgePtr& that) const { return (*this<that) || (*this == that); }
bool PartEdge::operator> (const PartEdgePtr& that) const { return !(*this<=that); }

/* ############################
   ##### IntersectionPart #####
   ############################ */

// Recursive computation of the cross-product of the edges in the range of analysis2Edges. Hierarchically 
// builds a recursion tree that contains more and more combinations of PartEdgePtrs from the analysis2Edges, 
// which are associated with the partition implementations of different analyses. When the recursion 
// tree reaches its full depth (one level per part in parts), it creates an intersection the current combination of 
// parent - the common PartEdge that is the parent of all the edges in the range of analysis2Edges
// curA - current iterator into analysis2Edges
// outPartEdges - the list of outgoing edges of the current combination of analysis2Edges's sub-edges, upto curA->first
// edges - vector that contains all the edges in the intersection
// analysisOfIntersection - the analysis associated with the Intersection edge
void intersectEdges(PartEdgePtr parent,
                    map<ComposedAnalysis*, set<PartEdgePtr> >::iterator curA,
                    map<ComposedAnalysis*, set<PartEdgePtr> >& analysis2Edges,
                    map<ComposedAnalysis*, PartEdgePtr> outPartEdges, 
                    list<PartEdgePtr>& edges,
                    ComposedAnalysis* analysisOfIntersection)
{
  ComposedAnalysis* curAnalysis = curA->first;

  // If we've reached the last part in parts and outEdgeParts contains all the outgoing PartEdges
  if(curA == analysis2Edges.end()) {
    PartEdgePtr newEdge = makePart<IntersectionPartEdge>(outPartEdges, parent, analysisOfIntersection);
    //Dbg::dbg << "analysisOfIntersection="<<analysisOfIntersection<<" newEdge="<<newEdge->str()<<endl;
    edges.push_back(newEdge);
  // If we haven't yet reached the end, recurse on all the outgoing edges of the current part
  } else {
    // Set nextA to follow curA
    map<ComposedAnalysis*, set<PartEdgePtr> >::iterator nextA = curA;
    nextA++;
    
    // Recurse on the cross product of the outgoing edges of this part and the outgoing edges of subsequent parts
    for(set<PartEdgePtr>::iterator e=curA->second.begin(); e!=curA->second.end(); e++){
      outPartEdges[curAnalysis] = *e;
      intersectEdges(parent, nextA, analysis2Edges, outPartEdges, edges, analysisOfIntersection);
      outPartEdges.erase(curAnalysis);
    }
  }
}

/*IntersectionPart::IntersectionPart(PartPtr part, ComposedAnalysis* analysis) : 
    Part(analysis)
{ parts.push_back(part); }*/

IntersectionPart::IntersectionPart(const std::map<ComposedAnalysis*, PartPtr>& parts, PartPtr parent, ComposedAnalysis* analysis) : 
    Part(analysis, parent), parts(parts)
{}

// Returns the list of outgoing IntersectionPartEdge of this Part, which are the cross-product of the outEdges()
// of its sub-parts.
std::list<PartEdgePtr> IntersectionPart::outEdges()
{
  /*ostringstream label; label << "IntersectionPart::outEdges";
  Dbg::region reg(1, 1, Dbg::region::topLevel, label.str());*/
  // For each part in parts, maps the parent part of each outgoing part to the set of parts that share this parent
  map<PartEdgePtr, map<ComposedAnalysis*, set<PartEdgePtr> > > parent2Out;
  for(map<ComposedAnalysis*, PartPtr>::iterator part=parts.begin(); part!=parts.end(); part++) {
    // Get this part's outgoing edges
    list<PartEdgePtr> out = part->second->outEdges();
    
    // Group these edges according to their common parent edge
    for(list<PartEdgePtr>::iterator e=out.begin(); e!=out.end(); e++)
      parent2Out[(*e)->getParent()][part->first].insert(*e);
  }
  
  /*for(map<PartEdgePtr, map<ComposedAnalysis*, set<PartEdgePtr> > >::iterator p=parent2Out.begin(); p!=parent2Out.end(); p++) {
    PartEdgePtr pf = p->first;
    Dbg::dbg << "parent="<<pf->str()<<endl;
    Dbg::indent ind;
    for(map<ComposedAnalysis*, set<PartEdgePtr> >::iterator a=p->second.begin(); a!=p->second.end(); a++) {
      Dbg::indent ind;
      for(set<PartEdgePtr>::iterator e=a->second.begin(); e!=a->second.end(); e++) {
        PartEdgePtr ef = *e;
        Dbg::dbg << ef->str()<<endl;
   } } }*/
  
  // Create a cross-product of the edges in parent2Out, one parent edge at a time
  std::list<PartEdgePtr> edges;
  for(map<PartEdgePtr, map<ComposedAnalysis*, set<PartEdgePtr> > >::iterator par=parent2Out.begin(); 
      par!=parent2Out.end(); par++) {
    map<ComposedAnalysis*, PartEdgePtr> outPartEdges;
    ROSE_ASSERT(par->second.size()!=0);
    intersectEdges(par->first, par->second.begin(), par->second, outPartEdges, edges, analysis);
  }
  
  /*Dbg::dbg << "edges="<<endl;
  Dbg::indent ind;
  for(list<PartEdgePtr>::iterator e=edges.begin(); e!=edges.end(); e++)
    Dbg::dbg << (*e)->str()<<endl;*/
  
  return edges;
}

// Returns the list of incoming IntersectionPartEdge of this Part, which are the cross-product of the inEdges()
// of its sub-parts.
std::list<PartEdgePtr> IntersectionPart::inEdges()
{
  // For each part in parts, maps the parent part of each incoming part to the set of parts that share this parent
  map<PartEdgePtr, map<ComposedAnalysis*, set<PartEdgePtr> > > parent2In;
  for(map<ComposedAnalysis*, PartPtr>::iterator part=parts.begin(); part!=parts.end(); part++) {
    // Get this part's outgoing edges
    list<PartEdgePtr> in = part->second->inEdges();
    
    // Group these edges according to their common parent edge
    for(list<PartEdgePtr>::iterator e=in.begin(); e!=in.end(); e++)
      parent2In[(*e)->getParent()][part->first].insert(*e);
  }
  
  // Create a cross-product of the edges in parent2Out, one parent edge at a time
  std::list<PartEdgePtr> edges;
  for(map<PartEdgePtr, map<ComposedAnalysis*, set<PartEdgePtr> > >::iterator par=parent2In.begin(); 
      par!=parent2In.end(); par++) {
    map<ComposedAnalysis*, PartEdgePtr> inPartEdges;
    intersectEdges(par->first, par->second.begin(), par->second, inPartEdges, edges, analysis);
  }
  return edges;
}

/* // Recursive computation of the cross-product of the outEdges of all the sub-parts of this Intersection part.
// Hierarchically builds a recursion tree that contains more and more combinations of PartsPtr from the outEdges
// of different sub-parts. When the recursion tree reaches its full depth (one level per part in parts), it creates
// an intersection the current combination of 
// partI - refers to the current part in parts
// outPartEdges - the list of outgoing edges of the current combination of this IntersectionPart's sub-parts, 
//         upto partI
void IntersectionPart::outEdges_rec(std::map<ComposedAnalysis*, PartPtr>::iterator partI, 
                                    std::map<ComposedAnalysis*, PartPtr> outPartEdges, 
                                    std::vector<PartEdgePtr>& edges) {
  // If we've reached the last part in parts and outEdgeParts contains all the outgoing PartEdges
  if(partI == parts.end())
    edges.push_back(makePart<IntersectionPartEdge>(outPartEdges, analysis));
  // If we haven't yet reached the end, recurse on all the outgoing edges of the current part
  else {
    // Get this part's outgoing edges
    vector<PartEdgePtr> out = (*partI)->outEdges();
    // Maps the parent part of each outgoing part to the set of parts that share this parent
    map<PartPrt, set<PartEdgePtr> > parent2Out;
    for(vector<PartEdgePtr>::iterator e=out.begin(); e!=out.end(); e++)
      parent2Out[(*e)->getParent()].insert(*e);
    
    ComposedAnalysis* curAnalysis = partI->first;
    
    // Advance to the next part in parts
    partI++;
    
    // Recurse on the cross product of the outgoing edges of this part and the outgoing edges of subsequent parts
    for(vector<PartEdgePtr>::iterator e=partOut.begin(); e!=partOut.end(); e++){
      outPartEdges[curAnalysis] = *e;
      outEdges_rec(partI, outPartEdges, edges);
      outPartEdges.pop_back();
    }
  }
}*/

/*// Returns the list of incoming IntersectionPartEdge of this Part, which are the cross-product of the inEdges()
// of its sub-parts.
std::vector<PartEdgePtr> IntersectionPart::inEdges() {
  list<PartEdgePtr> inPartEdges;
  vector<PartEdgePtr> edges;
  inEdges_rec(parts.begin(), inPartEdges, edges);
  return edges;
}
// Recursive computation of the cross-product of the inEdges of all the sub-parts of this Intersection part.
// Hierarchically builds a recursion tree that contains more and more combinations of PartsPtr from the inEdges
// of different sub-parts. When the recursion tree reaches its full depth (one level per part in parts), it creates
// an intersection the current combination of 
// partI - refers to the current part in parts
// inPartEdges - the list of incoming edges of the current combination of this IntersectionPart's sub-parts, 
//         upto partI
void IntersectionPart::inEdges_rec(list<PartPtr>::iterator partI, list<PartEdgePtr> inPartEdges, 
                                   vector<PartEdgePtr>& edges) {
  // If we've reached the last part in parts and inEdgeParts contains all the incoming PartEdges
  if(partI == parts.end())
    edges.push_back(makePart<IntersectionPartEdge>(inPartEdges, analysis));
  // If we haven't yet reached the end, recurse on all the incoming edges of the current part
  else {
    // Get this part's incoming edges
    vector<PartEdgePtr> partIn = (*partI)->inEdges();
    
    // Advance to the next part in parts
    partI++;
    
    // Recurse on the cross product of the ingoing edges of this part and the incoming edges of subsequent parts
    for(vector<PartEdgePtr>::iterator e=partIn.begin(); e!=partIn.end(); e++){
      inPartEdges.push_back(*e);
      inEdges_rec(partI, inPartEdges, edges);
      inPartEdges.pop_back();
    }
  }
}*/

// Returns the intersection of the lists of CFGNodes returned by the Parts in parts
set<CFGNode> IntersectionPart::CFGNodes() {
  set<CFGNode> nodes;
  for(map<ComposedAnalysis*, PartPtr>::iterator part=parts.begin(); part!=parts.end(); part++) {
    // If this is the first part, simply copy its CFGNodes to nodes
    if(part==parts.begin()) { 
      // Make sure to only copy of part is not NULL (not a wildcard source or destination of an edge)
      if(part->second) nodes = part->second->CFGNodes();
    // Otherwise, remove any nodes from node that are not in (*part)->CFGNodes()
    } else {
      set<CFGNode> partNodes=part->second->CFGNodes();
      for(set<CFGNode>::iterator nI=nodes.begin(); nI!=nodes.end(); ) {
        bool found=false;
        for(set<CFGNode>::iterator pnI=partNodes.begin(); pnI!=partNodes.end(); pnI++) {  
          if(nI==pnI) { found=true; break; }
        }
        // If the current element in nodes was not found in partNodes, erase it and move on to the next one
        if(!found) { 
          set<CFGNode>::iterator nINext = nI; nINext++;
          nodes.erase(nI);
          nI = nINext;
        // If it was found, just move on to the next CFGNode in nodes
        } else
          nI++;
      }
    }
  }
  
  return nodes;
}

/*
// Let A={ set of execution prefixes that terminate at the given anchor SgNode }
// Let O={ set of execution prefixes that terminate at anchor's operand SgNode }
// Since to reach a given SgNode an execution must first execute all of its operands it must
//    be true that there is a 1-1 mapping m() : O->A such that o in O is a prefix of m(o).
// This function is the inverse of m: given the anchor node and operand as well as the
//    Part that denotes a subset of A (the function is called on this part), 
//    it returns a list of Parts that partition O.
std::list<PartPtr> IntersectionPart::getOperandPart(SgNode* anchor, SgNode* operand)
{
  list<PartPtr> accumOperandParts;
  list<PartPtr> allParts;
  getOperandPart_rec(anchor, operand, parts.begin(), accumOperandParts, allParts);
  return allParts;
}
// Recursive computation of the cross-product of the getOperandParts of all the sub-parts of this Intersection part.
// Hierarchically builds a recursion tree that contains more and more combinations of PartPtrs from the results of
// getOperandPart of different sub-parts. When the recursion tree reaches its full depth (one level per part in parts), 
// it creates an intersection the current combination of parts
// partI - refers to the current part in parts
// accumOperandParts - the list of incoming parts of the current combination of this IntersectionPart's sub-parts, 
//         upto partI
void IntersectionPart::getOperandPart_rec(SgNode* anchor, SgNode* operand,
                                          list<PartPtr>::iterator partI, list<PartPtr> accumOperandParts, 
                                          list<PartPtr>& allParts)
{
  // If we've reached the last part in parts and accumOperandParts contains all the parts for the current combination
  if(partI == parts.end())
    allParts.push_back(makePart<IntersectionPart>(accumOperandParts));
  // If we haven't yet reached the end, recurse on all the incoming edges of the current part
  else {
    // Get this part's incoming edges
    list<PartPtr> operandParts = (*partI)->getOperandPart(anchor, operand);
    
    // Advance to the next part in parts
    partI++;
    
    // Recurse on the cross product of the ingoing edges of this part and the incoming edges of subsequent parts
    for(list<PartPtr>::iterator opP=operandParts.begin(); opP!=operandParts.end(); opP++){
      accumOperandParts.push_back(*opP);
      getOperandPart_rec(anchor, operand, partI, accumOperandParts, allParts);
      accumOperandParts.pop_back();
    }
  }
}*/

// Returns a PartEdgePtr, where the source is a wild-card part (NULLPart) and the target is this Part
PartEdgePtr IntersectionPart::inEdgeFromAny()
{
  // Collect the incoming edges from each sub-part and intersect them
  map<ComposedAnalysis*, PartEdgePtr> edges;
  for(map<ComposedAnalysis*, PartPtr>::iterator part=parts.begin(); part!=parts.end(); part++) {
    PartPtr ps = part->second;
    edges[part->first] = part->second->inEdgeFromAny();
  }
  
  return makePart<IntersectionPartEdge>(edges, (getParent()? getParent()->inEdgeFromAny() : NULLPartEdge), analysis);
}

// Returns a PartEdgePtr, where the target is a wild-card part (NULLPart) and the source is this Part
PartEdgePtr IntersectionPart::outEdgeToAny()
{
  // Collect the outgoing edges from each sub-part and intersect them
  map<ComposedAnalysis*, PartEdgePtr> edges;
  for(map<ComposedAnalysis*, PartPtr>::iterator part=parts.begin(); part!=parts.end(); part++)
    edges[part->first] = part->second->outEdgeToAny();
  return makePart<IntersectionPartEdge>(edges, (getParent()? getParent()->outEdgeToAny() : NULLPartEdge), analysis);
}

// Two IntersectionParts are equal if their parents and all their constituent sub-parts are equal
bool IntersectionPart::equal(const PartPtr& that_arg) const
{
  IntersectionPartPtr that = dynamic_part_cast<IntersectionPart>(that_arg);
  /*IntersectionPart copy(parts, getParent(), analysis);
  Dbg::dbg << "IntersectionPart::equal("<<copy.str()<<", "<<that->str()<<")"<<endl;*/
  
  // Two intersection parts with different numbers of sub-parts are definitely not equal
  if(parts.size() != that->parts.size()) { /*Dbg::dbg << "NOT EQUAL: size\n"; */return false; }
  
  // Two intersections with different parents are definitely not equal
  if(getParent() != that->getParent()) { /*Dbg::dbg << "NOT EQUAL: parents\n"; */return false; }
  
  for(map<ComposedAnalysis*, PartPtr>::const_iterator thisIt=parts.begin(), thatIt=that->parts.begin();
      thisIt!=parts.end(); thisIt++) {
    ROSE_ASSERT(thisIt->first == thatIt->first);
    if(thisIt->second != thatIt->second) { /*Dbg::dbg << "NOT EQUAL\n"; */return false; }
  }
  
  //Dbg::dbg << "EQUAL: size\n";
  return true;
}

// Lexicographic ordering: This IntersectionPart is < that IntersectionPart if 
// - their parents are < ordered, OR
// - if this has fewer parts than that, OR
// - there exists an index i in this.parts and that.parts s.t. forall j<i. this.parts[j]==that.parts[j] and 
//   this.parts[i] < that.parts[i].
bool IntersectionPart::less(const PartPtr& that_arg) const
{
  IntersectionPartPtr that = dynamic_part_cast<IntersectionPart>(that_arg);
  /*IntersectionPart copy(parts, getParent(), analysis);
  Dbg::dbg << "IntersectionPart::less("<<copy.str()<<", "<<that->str()<<")"<<endl;*/
  
  // If parents are properly ordered, use their ordering
  if(getParent() < that->getParent()) { /*Dbg::dbg << "LESS-THAN: parent\n";*/ return true; }
  if(getParent() > that->getParent()) { /*Dbg::dbg << "GREATER-THAN: parent\n";*/ return false; }
  
  // If this has fewer parts than that, it is ordered before it
  if(parts.size() < that->parts.size()) { /*Dbg::dbg << "LESS-THAN: size\n";*/ return true; }
  // If greater number of parts, it is order afterwards
  if(parts.size() > that->parts.size()) { /*Dbg::dbg << "GREATER-THAN: size\n";*/ return false; }
  
  // Keep iterating for as long as the sub-parts are equal and declare this < that if we find
  // a sub-part in this < the corresponding sub-part in that
  for(map<ComposedAnalysis*, PartPtr>::const_iterator thisIt=parts.begin(), thatIt=that->parts.begin();
      thisIt!=parts.end(); thisIt++) {
    ROSE_ASSERT(thisIt->first == thatIt->first);
    if(thisIt->second < thatIt->second) { /*Dbg::dbg << "LESS-THAN\n";*/ return true; }
    else if(thisIt->second > thatIt->second) { /*Dbg::dbg << "GREATER-THAN\n"; */return false; }
  }
  
  // If the lexicographic < condition was not met then this is not < than that
  //Dbg::dbg << "NOT LESS THAN\n"; 
  return false;
}

std::string IntersectionPart::str(std::string indent)
{
  ostringstream oss;
  oss << "[IntersectionPart:";
  if(parts.size() > 1) oss << endl;
  for(map<ComposedAnalysis*, PartPtr>::iterator part=parts.begin(); part!=parts.end(); ) {
    if(parts.size() > 1) oss << indent << "&nbsp;&nbsp;&nbsp;&nbsp;";
    oss << (part->second? part->second->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;") : "NULL");
    part++;
    if(part!=parts.end()) oss << endl;
  }
  oss << "]"; //", parent="<<(getParent()? getParent()->str(): "NULL")<<", analysis="<<analysis<<"]";
  return oss.str();
}

/* ################################
   ##### IntersectionPartEdge #####
   ################################ */
/*IntersectionPartEdge::IntersectionPartEdge(PartEdgePtr edge, ComposedAnalysis* analysis) :
    PartEdge(analysis)
{ edges.push_back(edge); }*/

IntersectionPartEdge::IntersectionPartEdge(const map<ComposedAnalysis*, PartEdgePtr>& edges, PartEdgePtr parent, ComposedAnalysis* analysis) : 
    PartEdge(analysis, parent), edges(edges) 
{}

// Returns the PartEdge associated with this analysis. If the analysis does not implement the partition graph
// (is not among the keys of parts), returns the parent PartEdge.
PartEdgePtr IntersectionPartEdge::getPartEdge(ComposedAnalysis* analysis)
{
  //Dbg::dbg << "IntersectionPartEdge::getPartEdge(analysis="<<analysis<<" = "<<analysis->str()<<") #edges="<<edges.size()<<endl;
  for(map<ComposedAnalysis*, PartEdgePtr>::iterator e = edges.begin(); e!=edges.end(); e++)
  {
    PartEdgePtr es = e->second;
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<e->first<< " : " << e->first->str() << " : " << es->str() << endl;
  }
  
  //for(map<ComposedAnalysis*, PartEdgePtr>::iterator edge=edges.begin(); edge!=edges.end(); edge
  //map<ComposedAnalysis*, PartEdgePtr>::iterator edge;
  if(edges.find(analysis)!=edges.end()) { /*Dbg::dbg << "found edge "<<edges[analysis]->str()<<endl;*/ return edges[analysis]; }
  else { /*Dbg::dbg << "not found. parent="<<getParent()->str()<<endl; */return getParent(); }
}

// Return the part that intersects the sources of all the sub-edges of this IntersectionPartEdge
PartPtr IntersectionPartEdge::source() {
  map<ComposedAnalysis*, PartPtr> sourceParts;
  bool allNULL=true; // True if all the source parts of the sub-edges are NULL, false otherwise.
  PartPtr srcParent;
  for(map<ComposedAnalysis*, PartEdgePtr>::iterator e=edges.begin(); e!=edges.end(); e++) {
    PartPtr s = e->second->source();
    // Make sure that the parents of source parts are consistent
    if(e == edges.begin()) {
      if(s) {
        srcParent = s->getParent();
        allNULL=false;
      }
    } else {
      // Either all sources are NULL or none are
      ROSE_ASSERT((allNULL && !s) || (!allNULL && s));
      if(s)
        // All parents must be consistent
        ROSE_ASSERT(srcParent == s->getParent());
    }
    
    sourceParts[e->first] = s;
  }
  if(allNULL) return NULLPart;
  else        return makePart<IntersectionPart>(sourceParts, srcParent, analysis);
}

// Return the part that intersects the targets of all the sub-edges of this IntersectionPartEdge
PartPtr IntersectionPartEdge::target() {
  map<ComposedAnalysis*, PartPtr> targetParts;
  bool allNULL=true; // True if all the target parts of the sub-edges are NULL, false otherwise.
  PartPtr tgtParent;
  for(map<ComposedAnalysis*, PartEdgePtr>::iterator e=edges.begin(); e!=edges.end(); e++) {
    PartPtr t = e->second->target();
    // Make sure that the parents of source parts are consistent
    if(e == edges.begin()) {
      if(t) {
        tgtParent = t->getParent();
        allNULL=false;
      }
    } else {
      // Either all sources are NULL or none are
      ROSE_ASSERT((allNULL && !t) || (!allNULL && t));
      if(t)
        // All parents must be consistent
        ROSE_ASSERT(tgtParent == t->getParent());
    }
    
    targetParts[e->first] = t;
  }
  if(allNULL) return NULLPart;
  else        return makePart<IntersectionPart>(targetParts, tgtParent, analysis);
}


// Let A={ set of execution prefixes that terminate at the given anchor SgNode }
// Let O={ set of execution prefixes that terminate at anchor's operand SgNode }
// Since to reach a given SgNode an execution must first execute all of its operands it must
//    be true that there is a 1-1 mapping m() : O->A such that o in O is a prefix of m(o).
// This function is the inverse of m: given the anchor node and operand as well as the
//    PartEdge that denotes a subset of A (the function is called on this PartEdge), 
//    it returns a list of PartEdges that partition O.
std::list<PartEdgePtr> IntersectionPartEdge::getOperandPartEdge(SgNode* anchor, SgNode* operand)
{
  // For each part in parts, maps the parent part of each operand part edge to the set of parts that share this parent
  map<PartEdgePtr, map<ComposedAnalysis*, set<PartEdgePtr> > > parent2OPE;
  for(map<ComposedAnalysis*, PartEdgePtr>::iterator edge=edges.begin(); edge!=edges.end(); edge++) {
    // Get this part's outgoing edges
    list<PartEdgePtr> ope = edge->second->getOperandPartEdge(anchor, operand);
    
    // Group these edges according to their common parent edge
    for(list<PartEdgePtr>::iterator e=ope.begin(); e!=ope.end(); e++)
      parent2OPE[(*e)->getParent()][edge->first].insert(*e);
  }
  
  // Create a cross-product of the edges in parent2Out, one parent edge at a time
  std::list<PartEdgePtr> edges;
  for(map<PartEdgePtr, map<ComposedAnalysis*, set<PartEdgePtr> > >::iterator par=parent2OPE.begin(); 
      par!=parent2OPE.end(); par++) {
    map<ComposedAnalysis*, PartEdgePtr> opePartEdges;
    intersectEdges(par->first, par->second.begin(), par->second, opePartEdges, edges, analysis);
  }
  return edges;
}


/*std::list<PartEdgePtr> IntersectionPartEdge::getOperandPartEdge(SgNode* anchor, SgNode* operand)
{
  list<PartEdgePtr> accumOperandPartEdges;
  list<PartEdgePtr> allPartEdges;
  getOperandPartEdge_rec(anchor, operand, edges.begin(), accumOperandPartEdges, allPartEdges);
  return allPartEdges;
}

// Recursive computation of the cross-product of the getOperandParts of all the sub-part edges of this Intersection part edge.
// Hierarchically builds a recursion tree that contains more and more combinations of PartEdgePtrs from the results of
// getOperandPart of different sub-part edges. When the recursion tree reaches its full depth (one level per edge in edges), 
// it creates an intersection the current combination of edges.
// edgeI - refers to the current edge in edges
// accumOperandPartEdges - the list of incoming edgesof the current combination of this IntersectionPartEdges's sub-Edges, 
//         upto edgeI
void IntersectionPartEdge::getOperandPartEdge_rec(SgNode* anchor, SgNode* operand,
                                                  list<PartEdgePtr>::iterator edgeI, list<PartEdgePtr> accumOperandPartEdges, 
                                                  list<PartEdgePtr>& allPartEdges)
{
  // If we've reached the last edge in edges and accumOperandPartEdges contains all the edges for the current combination
  if(edgeI == edges.end())
    allPartEdges.push_back(makePart<IntersectionPartEdge>(accumOperandPartEdges, analysis));
  // If we haven't yet reached the end, recurse on all the incoming edges of the current edge
  else {
    // Get this edge's incoming edges
    list<PartEdgePtr> operandPartEdges = (*edgeI)->getOperandPartEdge(anchor, operand);
    
    // Advance to the next edge in edges
    edgeI++;
    
    // Recurse on the cross product of the ingoing edges of this edge and the incoming edges of subsequent edges
    for(list<PartEdgePtr>::iterator opP=operandPartEdges.begin(); opP!=operandPartEdges.end(); opP++){
      accumOperandPartEdges.push_back(*opP);
      getOperandPartEdge_rec(anchor, operand, edgeI, accumOperandPartEdges, allPartEdges);
      accumOperandPartEdges.pop_back();
    }
  }
}*/

// If the source Part corresponds to a conditional of some sort (if, switch, while test, etc.)
// it must evaluate some predicate and depending on its value continue, execution along one of the
// outgoing edges. The value associated with each outgoing edge is fixed and known statically.
// getPredicateValue() returns the value associated with this particular edge. Since a single 
// Part may correspond to multiple CFGNodes getPredicateValue() returns a map from each CFG node
// within its source part that corresponds to a conditional to the value of its predicate along 
// this edge. 
std::map<CFGNode, boost::shared_ptr<SgValueExp> > IntersectionPartEdge::getPredicateValue()
{
  if(source()) {
    // The set of CFGNodes for which we'll create a value mapping since these nodes exist
    // in all the sub-edges of this IntersectionPartEdge
    set<CFGNode> srcNodes = source()->CFGNodes();

    map<CFGNode, boost::shared_ptr<SgValueExp> > pv;
    // Consider the predicate->value mappings of all the sub-edges
    for(map<ComposedAnalysis*, PartEdgePtr>::iterator e=edges.begin(); e!=edges.end(); e++) {
      map<CFGNode, boost::shared_ptr<SgValueExp> > epv = e->second->getPredicateValue();
      // Consider the values mapped under all the CFG nodes of this sub-edge's source part
      for(map<CFGNode, boost::shared_ptr<SgValueExp> >::iterator v=epv.begin(); v!=epv.end(); v++) {
        // Skip CFGNodes that are not shared but all the sources of all the sub-edges
        if(srcNodes.find(v->first) == srcNodes.end()) { continue; }
        
        // If a value mapping for the current CFGNode of the current sub-edge has already 
        // been observed from another sub-edge, make sure that the mapped values are the same
        if(pv.find(v->first) != pv.end())
          ROSE_ASSERT(ValueObject::equalValueExp(pv[v->first].get(), v->second.get()));
        else
          pv[v->first] = v->second;
      }
    }
    
    return pv;
  } else {
    std::map<CFGNode, boost::shared_ptr<SgValueExp> > empty;
    return empty;
  }
}

// Two IntersectionPartEdges are equal of all their constituent sub-parts are equal
bool IntersectionPartEdge::equal(const PartEdgePtr& o) const
{
  IntersectionPartEdgePtr that = dynamic_part_cast<IntersectionPartEdge>(o);
  /*IntersectionPartEdge copy(edges, getParent(), analysis);
  Dbg::dbg << "IntersectionPartEdge::equal("<<copy.str()<<", "<<that->str()<<")"<<endl;*/
  // Two intersection parts with different numbers of sub-parts are definitely not equal
  if(edges.size() != that->edges.size()) { /*Dbg::dbg << "NOT EQUAL: size\n"; */return false; }
  
  // Two intersections with different parents are definitely not equal
  if(getParent() != that->getParent()) { /*Dbg::dbg << "NOT EQUAL: parents\n"; */return false; }
  
  for(map<ComposedAnalysis*, PartEdgePtr>::const_iterator thisIt=edges.begin(), thatIt=that->edges.begin();
      thisIt!=edges.end(); thisIt++) {
    if(*thisIt != *thatIt) { /*Dbg::dbg << "NOT EQUAL\n"; */return false; }
  }
  
  //Dbg::dbg << "EQUAL\n";
  return true;
}

// Lexicographic ordering: This IntersectionPartEdge is < that IntersectionPartEdge if this has fewer edges than that or
// there exists an index i in this.edges and that.edges s.t. forall j<i. this.edges[j]==that.edges[j] and 
// this.edges[i] < that.edges[i].
bool IntersectionPartEdge::less(const PartEdgePtr& o) const
{
  IntersectionPartEdgePtr that = dynamic_part_cast<IntersectionPartEdge>(o);
  /*IntersectionPartEdge copy(edges, getParent(), analysis);
  Dbg::dbg << "IntersectionPartEdge::less("<<copy.str()<<", "<<that->str()<<")"<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;getParent()="<<getParent()->str()<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;that->getParent()="<<that->getParent()->str()<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;< "<<(getParent() < that->getParent())<<", == "<<(getParent() == that->getParent())<<", > "<<(getParent() > that->getParent())<<endl;
  */
  // If parents are properly ordered, use their ordering
  if(getParent() < that->getParent()) { /*Dbg::dbg << "LESS-THAN: parent\n";*/ return true; }
  if(getParent() > that->getParent()) { /*Dbg::dbg << "GREATER-THAN: parent\n";*/ return false; }
  
  // If this has fewer edges than that, it is ordered before it
  if(edges.size() < that->edges.size()) { /*Dbg::dbg << "LESS-THAN: size\n";*/ return true; } 
  // If greater number of edges, it is order afterwards
  if(edges.size() > that->edges.size()) { /*Dbg::dbg << "GREATER-THAN: size\n";*/ return false; }
  
  // Keep iterating for as long as the sub-edges are equal and declare this < that if we find
  // a sub-edge in this < the corresponding sub-edge in that
  for(map<ComposedAnalysis*, PartEdgePtr>::const_iterator thisIt=edges.begin(), thatIt=that->edges.begin();
      thisIt!=edges.end(); thisIt++) {
    if(*thisIt < *thatIt) { /*Dbg::dbg << "LESS-THAN\n"; */return true; }
    else if(*thisIt > *thatIt) { /*Dbg::dbg << "GREATER-THAN\n"; */return false; }
  }
  
  // If the lexicographic < condition was not met then this is not < than that
  /*Dbg::dbg << "NOT LESS THAN\n"; */
  return false;
}

std::string IntersectionPartEdge::str(std::string indent)
{
  ostringstream oss;
  oss << "[IntersectionPartEdge:";
  if(edges.size() > 1) oss << endl;
  for(map<ComposedAnalysis*, PartEdgePtr>::iterator edge=edges.begin(); edge!=edges.end(); ) {
    if(edges.size() > 1) oss << indent << "&nbsp;&nbsp;&nbsp;&nbsp;";
    oss << edge->second->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    edge++;
    if(edge!=edges.end()) oss << endl;
  }
  oss << "]"; //", parent="<<(getParent()? getParent()->str(): "NULL")<<", analysis="<<analysis<<"]";
  return oss.str();
}

}; // namespace dataflow
