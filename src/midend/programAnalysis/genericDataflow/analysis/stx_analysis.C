#include "stx_analysis.h"
#include <map>
#include <typeinfo>
#include "sageInterface.h"
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include "VirtualCFGIterator.h"


#include <boost/function.hpp>
#include <boost/bind.hpp>


using namespace std;
using namespace cfgUtils;
//namespace bll = boost::lambda;

namespace dataflow {

int stxAnalysisDebugLevel=1;
  
/*****************************
 ***** SyntacticAnalysis *****
 *****************************/

// the top level builder for MemLocObject from any SgNode

SyntacticAnalysis* SyntacticAnalysis::_instance = 0;

SyntacticAnalysis* SyntacticAnalysis::instance()
{
  if(!_instance)
    _instance = new SyntacticAnalysis();
  return _instance;
}

MemLocObjectPtr SyntacticAnalysis::Expr2MemLoc(SgNode* n, PartEdgePtr pedge)
{ return SyntacticAnalysis::Expr2MemLocStatic(n, pedge); }

// Returns if r is an indirect lexical variable reference a->b or a.b.c where a is a reference type
/*bool isIndirectDotVarRef(SgExpression* r)
{
  // a is not a reference type
  while(isSgDotExp(r) && 
        !isSgReferenceType(isSgDotExp(r)->get_rhs_operand()) &&
        isSgVarRefExp((isSgDotExp(r)->get_lhs_operand()))) {
    if(!isSgExpression(r->get_parent())) break;
    r = isSgExpression(r->get_parent());
  }

  // If the root of the expression is a->b or a.b where a is a reference type, then VarRefExp r
  // was accessed through an indirection
  return 
    // a->b.c.d
    !isSgArrowExp(r) &&
    // a.b.c.d where a is a reference type
    !(isSgDotExp(r) && isSgReferenceType(isSgDotExp(r)->get_rhs_operand()));
}*/

// If this a reference to a static variable (e.g., a, a.b), return the SgSymbol for this variable and NULL otherwise.
SgSymbol* isDirectVarRef(SgNode* r)
{
  //Dbg::dbg << "isDirectVarRef("<<cfgUtils::SgNode2Str(r)<<")"<<endl;
  // a
  if(isSgVarRefExp(r)) {
    //Dbg::dbg << "isDirectVarRef("<<cfgUtils::SgNode2Str(r)<<") => varref "<<cfgUtils::SgNode2Str(isSgVarRefExp(r)->get_symbol())<<endl;
    return isSgVarRefExp(r)->get_symbol();
  // a.b
  } else if(isSgDotExp(r)) {
    /*{
      SgDotExp* cur = isSgDotExp(r);
      Dbg::dbg << "r="<<endl;
      while(cur) {
        Dbg::dbg << "    "<<cfgUtils::SgNode2Str(cur)<<endl;
        Dbg::dbg << "    left="<<cfgUtils::SgNode2Str(cur->get_lhs_operand())<< " | type="<<cfgUtils::SgNode2Str(cur->get_lhs_operand()->get_type())<<endl;
        Dbg::dbg << "    right="<<cfgUtils::SgNode2Str(cur->get_rhs_operand())<< " | type="<<cfgUtils::SgNode2Str(cur->get_rhs_operand()->get_type())<<endl;
        cur = isSgDotExp(cur->get_lhs_operand());
      }
    }*/
    
    // Check that this is an expression of form a.b.c.d where each component is a VarRefExp that does not 
    // have a reference type
    SgDotExp *cur = isSgDotExp(r);
    while(cur) {
      if(isSgReferenceType(cur->get_rhs_operand()->get_type())) return NULL;
      if(isSgReferenceType(cur->get_lhs_operand()->get_type())) return NULL;
      if(!isSgVarRefExp(cur->get_rhs_operand())) return NULL;
      
      cur = isSgDotExp(cur->get_lhs_operand());
    }
    
    //Dbg::dbg << "isDirectVarRef("<<cfgUtils::SgNode2Str(r)<<") => dotexp "<<cfgUtils::SgNode2Str(isSgVarRefExp(isSgDotExp(r)->get_rhs_operand())->get_symbol())<<endl;
    return isSgVarRefExp(isSgDotExp(r)->get_rhs_operand())->get_symbol();
  }

  return NULL;
}

// If this is a memory reference that works through indirection (*a, a->b, a.b.c where a is a reference type)
bool isIndirectVarRef(SgNode* r)
{
  return isSgPointerDerefExp(r) ||
         isSgArrowExp(r) ||
         (isSgDotExp(r) && !isDirectVarRef(r));
}

MemLocObjectPtr SyntacticAnalysis::Expr2MemLocStatic(SgNode* n, PartEdgePtr pedge)
{
  MemLocObjectPtr rt;

  ROSE_ASSERT(n);
  /*ostringstream label; if(stxAnalysisDebugLevel>=1) label << "Expr2MemLocStatic("<<cfgUtils::SgNode2Str(n)<<")"<<endl;
  Dbg::region reg(stxAnalysisDebugLevel, 1, Dbg::region::midLevel, label.str());*/
  
  //Dbg::dbg << "isSgPntrArrRefExp (n)="<<isSgPntrArrRefExp (n)<<endl;//" isSgPntrArrRefExp (n->get_parent())="<<isSgPntrArrRefExp (n->get_parent())<<endl;
  /*if(isSgPntrArrRefExp (n->get_parent())) {
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;parent->lhs=["<<Dbg::escape(isSgPntrArrRefExp (n->get_parent())->get_lhs_operand()->unparseToString())<<" | "<<isSgPntrArrRefExp (n->get_parent())->get_lhs_operand()->class_name()<<"]"<<endl;
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;parent->rhs=["<<Dbg::escape(isSgPntrArrRefExp (n->get_parent())->get_rhs_operand()->unparseToString())<<" | "<<isSgPntrArrRefExp (n->get_parent())->get_rhs_operand()->class_name()<<"]"<<endl;
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;isSgPntrArrRefExp (n->get_parent())->get_rhs_operand()==n)="<<(isSgPntrArrRefExp (n->get_parent())->get_lhs_operand()==n)<<endl;
  }
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;isSgExpression(n)="<<isSgExpression(n)<<endl;*/
  
  // Only create Named objects for top-level SgPntrArrRefExps
  // Or for SgPntrArrRefExps that denote the indexes inside SgPntrArrRefExps (e.g. array[array[i]])
  if (isSgPntrArrRefExp (n) && 
      !(isSgPntrArrRefExp (n->get_parent()) && isSgPntrArrRefExp(isSgPntrArrRefExp (n->get_parent())->get_lhs_operand()) &&
        isSgPntrArrRefExp (n->get_parent())->get_lhs_operand()==n))
  {
    //Dbg::dbg<< "NamedML"<<endl;
    SgPntrArrRefExp* r = isSgPntrArrRefExp(n);
    assert (r != NULL);
    rt = createNamedMemLocObject_PntrArrRef(n, r, pedge);
  }
  else if (isDirectVarRef(n))
  {
    // Create a NamedMemLocObject from a static variable reference of form a and a.b.c where a is not a reference type
    ROSE_ASSERT(isSgExpression(n));
    rt = createNamedMemLocObject_DirectVarRef(n, isSgExpression(n), pedge);
  }
  else if (SgExpression* sgexp=isSgExpression(n)) // the order matters !! Must put after V_SgVarRefExp, SgPntrArrRefExp etc.
  {
    if(isIndirectVarRef(sgexp)) {
      // create the aliased object based on its type
      ROSE_ASSERT(sgexp->get_type());
      rt = createAliasedMemLocObject(sgexp, sgexp->get_type(), pedge);
    }
    //TODO: handle array expression objects
    else{
      rt = createExpressionMemLocObject(sgexp, pedge);
    }
  }
  else if (SgType* t = isSgType(n))
  {
    rt = createAliasedMemLocObject(n, t, pedge);
  }
  else if (isSgSymbol(n) || isSgInitializedName(n)) // skip SgFunctionSymbol etc
  {
    SgSymbol* s;
    cout << "n="<<cfgUtils::SgNode2Str(n)<<endl;
    if(isSgSymbol(n)) s = isSgSymbol (n);
    else              s = isSgInitializedName(n)->search_for_symbol_from_symbol_table();
    assert (s != NULL);

/*    if(SgClassDefinition* classDef = isMemberVariableDeclarationSymbol(s))
    {
/ *      // This symbol is part of an aggregate object
      // We cannot create an MemLocObject based on this symbol alone since it can be instantiated to multiple instances, based on the parent obj, and optional index value
      // We should create something like a.b when this field (b) is referenced in the AST
      Dbg::dbg << "n->get_parent()->get_parent()="<<cfgUtils::SgNode2Str(n->get_parent()->get_parent())<<endl;
      Dbg::dbg << "n->get_parent()="<<cfgUtils::SgNode2Str(n->get_parent())<<endl;
      Dbg::dbg << "n="<<cfgUtils::SgNode2Str(n)<<endl;
      Dbg::dbg << "s="<<cfgUtils::SgNode2Str(s)<<endl;
      if(SgVariableSymbol* vs = isSgVariableSymbol (s)) {
        Dbg::dbg << "vs->get_declaration()->get_scope()="<<cfgUtils::SgNode2Str(vs->get_declaration()->get_scope())<<endl;
      }
      ROSE_ASSERT(0);
* /
        // The parent is an aliased object because we don't know the actual class instance that we're operating on
        rt  = createNamedMemLocObject(n, s, s->get_type(), pedge, 
                                   //createAliasedMemLocObject(classDef, classDef->get_type(), pedge),
                                   MemLocObjectPtr(),
                                   IndexVectorPtr());
    } else {*/
    if(isSgVariableSymbol (s)) {
      // parent should be NULL since it is not a member variable symbol
      // TODO handle array of arrays ?? , then last IndexVectorPtr should not be NULL   
      rt  = createNamedMemLocObject(n, s, s->get_type(), pedge, MemLocObjectPtr(), IndexVectorPtr()); 
    } else {
      ROSE_ASSERT(0);
    }
  } else ROSE_ASSERT(0);
  //Dbg::dbg << "SyntacticAnalysis::Expr2MemLocStatic(n"<<cfgUtils::SgNode2Str(n)<<", pedge="<<pedge->str()<<")"<<endl;
  //Dbg::dbg << "rt="<<(rt? rt->str(): "NULLML")<<endl;
  
  return rt;
}

ValueObjectPtr SyntacticAnalysis::Expr2Val(SgNode* n, PartEdgePtr pedge)
{ return SyntacticAnalysis::Expr2ValStatic(n, pedge); }

ValueObjectPtr SyntacticAnalysis::Expr2ValStatic(SgNode* n, PartEdgePtr pedge)
{ return boost::make_shared<StxValueObject>(n); }

CodeLocObjectPtr SyntacticAnalysis::Expr2CodeLoc(SgNode* n, PartEdgePtr pedge)
{ return SyntacticAnalysis::Expr2CodeLocStatic(n, pedge); }

CodeLocObjectPtr SyntacticAnalysis::Expr2CodeLocStatic(SgNode* n, PartEdgePtr pedge)
{ return boost::make_shared<StxCodeLocObject>(n, pedge); }

// Return the anchor Parts of a given function
PartPtr SyntacticAnalysis::GetFunctionStartPart_Spec(const Function& func)
{
  // Find the SgFunctionParameterList node by walking the CFG forwards from the function's start
  /*Dbg::dbg << "SyntacticAnalysis::GetFunctionStartPart()"<<endl;
  Dbg::indent ind;*/
  //cout << "func.get_definition()="<<cfgUtils::SgNode2Str(func.get_definition())<<endl;
  for(VirtualCFG::back_iterator it(cfgUtils::getFuncEndCFG(func.get_definition())); it!=VirtualCFG::back_iterator::end(); it++) {
    //cout << "it="<<cfgUtils::CFGNode2Str(*it)<<" isSgFunctionParameterList((*it).getNode()="<<isSgFunctionParameterList((*it).getNode())<<endl;
    if(isSgFunctionParameterList((*it).getNode())/* && (*it).getIndex()==1*/)
      return makePart<StxPart>(*it, this, filter);//boost::make_shared<StxPart>(*it, filter);
  }
  // We should never get here
  ROSE_ASSERT(0);
}

PartPtr SyntacticAnalysis::GetFunctionEndPart_Spec(const Function& func)
{
  return makePart<StxPart>(cfgUtils::getFuncEndCFG(func.get_definition()), this, filter);//boost::make_shared<StxPart>(cfgUtils::getFuncEndCFG(func.get_definition()), filter);
}

/**********************
 ***** PARTITIONS *****
 **********************/

// A NULL CFGNode that is used as a wild-card for termination points of edges to/from anywhere
CFGNode NULLCFGNode;

/*******************
 ***** StxPart *****
 *******************/

// XXX: This code is duplicated from frontend/SageIII/virtualCFG/virtualCFG.C
// Make a set of raw CFG edges closure. Raw edges may have src and dest CFG nodes which are to be filtered out. 
// The method used is to connect them into CFG paths so src and dest nodes of each path are interesting, skipping intermediate filtered nodes)
map<StxPartEdgePtr, bool> makeClosureDF(const vector<CFGEdge>& orig, // raw in or out edges to be processed
                                     vector<CFGEdge> (CFGNode::*closure)() const, // find successor edges from a node, CFGNode::outEdges() for example
                                     CFGNode (CFGPath::*otherSide)() const, // node from the other side of the path: CFGPath::target()
                                     CFGPath (*merge)(const CFGPath&, const CFGPath&),  // merge two paths into one
                                     bool (*filter) (CFGNode),   // filter function 
                                     ComposedAnalysis* analysis)
{
  // a filter function here
  // A set of CFG paths, each of them is made from a raw CFG edge initially
  vector<CFGPath> currentPaths(orig.begin(), orig.end());
  /*vector<CFGPath> currentPaths;
  for(vector<StxPartEdgePtr>::const_iterator o=orig.begin(); o!=orig.end(); o++)
    currentPaths.push_back((*o)->getPath());*/
  
  /*{
  Dbg::dbg << "currentPaths="<<endl;
  Dbg::indent ind;
  for(vector<CFGPath>::iterator p=currentPaths.begin(); p!=currentPaths.end(); p++)
    Dbg::dbg << SgNode2Str(p->source().getNode())<<" ==> "<<SgNode2Str(p->target().getNode())<<endl;
  }*/
  while (true) {
top:
    Dbg::indent ind;
    for (size_t i = 0; i < currentPaths.size(); ++i) { // check each of the current paths
      //Dbg::dbg << "currentPaths["<<i<<"]="<<SgNode2Str(currentPaths[i].source().getNode())<<" ==> "<<SgNode2Str(currentPaths[i].target().getNode())<<endl;
      
      // if a path has a node from the other side which is not interesting, do the path merge
      if (!filter((currentPaths[i].*otherSide)())) {
        unsigned int oldSize = currentPaths.size(); // the number of unique paths before merge
        //get all other successor edges from the non-interesting dest node
        vector<CFGEdge> currentPaths2 = ((currentPaths[i].*otherSide)().*closure)(); 
        
        // merge the successor edges one by one
        for (unsigned int j = 0; j < currentPaths2.size(); ++j) {
          //Dbg::dbg << "currentPaths["<<i<<"]="<<SgNode2Str(currentPaths[i].source().getNode())<<" ==> "<<SgNode2Str(currentPaths[i].target().getNode())<<endl;
          //Dbg::dbg << "currentPaths2["<<j<<"]="<<SgNode2Str(currentPaths2[j].source().getNode())<<" ==> "<<SgNode2Str(currentPaths2[j].target().getNode())<<endl;
          CFGPath merged = (*merge)(currentPaths[i], currentPaths2[j]);
          if (std::find(currentPaths.begin(), currentPaths.end(), merged) == currentPaths.end()) { // find a new path? push it to the working set of initial edges
            currentPaths.push_back(merged); // a new path will be inserted. Old path ending with non-interesting node still exists
          }
        }
        if (currentPaths.size() != oldSize) 
          goto top; // TODO go through all paths again? not very efficient!!
      }
    }
    break; // If the iteration got all of the way through: all dest nodes of all paths are interesting or no new merges are made.
  }
  // cerr << "makeClosure loop done: " << currentPaths.size() << endl;

  // Now convert the set of CFG paths with interesting src and dest nodes into a set of DataflowEdge 
  //list<StxPartEdgePtr> edges;
  // Maps edges to bools. A map is used to enable efficient lookups to avoid inserting duplicate edges, 
  // which may happen in situations like an if statement with empty true and false bodies.
  map<StxPartEdgePtr, bool> edges;
  for (vector<CFGPath>::iterator i = currentPaths.begin(); i != currentPaths.end(); ++i) {
    // Only if the end node of the path is interesting
    //if (((*i).*otherSide)().isInteresting())
    if (filter(((*i).*otherSide)())) {
      //edges.push_back(/*boost::static_pointer_cast<PartEdge>(*/boost::make_shared<StxPartEdge>(*i, filter)/*)*/);
      //edges.push_back(makePart<StxPartEdge>(*i, analysis, filter));
      StxPartEdgePtr newEdge = makePart<StxPartEdge>(*i, analysis, filter);
      if(edges.find(newEdge) == edges.end()) edges[newEdge] = true;
    }
  }
  //Dbg::dbg << "makeClosure done: #edges=" << edges.size() << endl;
  //for(vector<DataflowEdge>::iterator e=edges.begin(); e!=edges.end(); e++)
  //    printf("Current Node %p<%s | %s>\n", e.target().getNode(), e.target().getNode()->unparseToString().c_str(), e.target().getNode()->class_name().c_str());
  //for (list<StxPartEdgePtr>::iterator i = edges.begin(); i != edges.end(); ++i) {
  for (map<StxPartEdgePtr, bool>::iterator i = edges.begin(); i != edges.end(); ++i) {
    StxPartEdgePtr edge = i->first;
    ROSE_ASSERT(edge->source()->filterAny(filter)  || 
                edge->target()->filterAny(filter)); // at least one node is interesting
  }
  return edges;
}

list<PartEdgePtr> StxPart::outEdges() {
  map<StxPartEdgePtr, bool> vStx = makeClosureDF(n.outEdges(), &CFGNode::outEdges, &CFGPath::target, &mergePaths, filter, analysis);
  list<PartEdgePtr> v;
  for(map<StxPartEdgePtr, bool>::iterator i=vStx.begin(); i!=vStx.end(); i++) {
    v.push_back(dynamic_part_cast<PartEdge>(i->first));
  }
  return v;
}

list<StxPartEdgePtr> StxPart::outStxEdges() {
  map<StxPartEdgePtr, bool> vStx = makeClosureDF(n.outEdges(), &CFGNode::outEdges, &CFGPath::target, &mergePaths, filter, analysis);
  list<StxPartEdgePtr> v;
  for(map<StxPartEdgePtr, bool>::iterator i=vStx.begin(); i!=vStx.end(); i++)
    v.push_back(i->first);
  return v;
}

list<PartEdgePtr> StxPart::inEdges() {
  map<StxPartEdgePtr, bool> vStx = makeClosureDF(n.inEdges(), &CFGNode::inEdges, &CFGPath::source, &mergePathsReversed, filter, analysis);
  list<PartEdgePtr> v;
  for(map<StxPartEdgePtr, bool>::iterator i=vStx.begin(); i!=vStx.end(); i++)
    v.push_back(dynamic_part_cast<PartEdge>(i->first));
  return v;
}

list<StxPartEdgePtr> StxPart::inStxEdges() {
  map<StxPartEdgePtr, bool> vStx = makeClosureDF(n.inEdges(), &CFGNode::inEdges, &CFGPath::source, &mergePathsReversed, filter, analysis);
  list<StxPartEdgePtr> v;
  for(map<StxPartEdgePtr, bool>::iterator i=vStx.begin(); i!=vStx.end(); i++)
    v.push_back(i->first);
  return v;
}

set<CFGNode> StxPart::CFGNodes()
{
  set<CFGNode> v;
  v.insert(n);
  return v;
}

/*// Let A={ set of execution prefixes that terminate at the given anchor SgNode }
// Let O={ set of execution prefixes that terminate at anchor's operand SgNode }
// Since to reach a given SgNode an execution must first execute all of its operands it must
//    be true that there is a 1-1 mapping m() : O->A such that o in O is a prefix of m(o).
// This function is the inverse of m: given the anchor node and operand as well as the
//    Part that denotes a subset of A (the function is called on this part), 
//    it returns a list of Parts that partition O.
std::list<PartPtr> StxPart::getOperandPart(SgNode* anchor, SgNode* operand)
{
  list<PartPtr> l;
  l.push_back(makePart<StxPart>(operand->cfgForEnd(), analysis));
  return l;
}*/

/*class NULLCFGNode : public CFGNode {
  public:
  NULLCFGNode() : CFGNode(SageInterface::getProject(), -1) { }
};
*/
CFGNode getCFGNode() {
  static SgNode* sgn = NULL;
  if(!sgn) sgn = SageBuilder::buildNullStatement();
  CFGNode n(sgn, 0);
  return n;
}
bool isNULLCFGNode(CFGNode n) { return isSgNullStatement(n.getNode()); }



// Returns a PartEdgePtr, where the source is a wild-card part (NULLPart) and the target is this Part
PartEdgePtr StxPart::inEdgeFromAny()
{ return makePart<StxPartEdge>(getCFGNode(), n, analysis); } ///*NULLCFGNode*/SageInterface::getGlobalScope(n.getNode())->cfgForBeginning(), n); }

// Returns a PartEdgePtr, where the target is a wild-card part (NULLPart) and the source is this Part
PartEdgePtr StxPart::outEdgeToAny()
{ return makePart<StxPartEdge>(n, getCFGNode(), analysis); } ///*NULLCFGNode*/SageInterface::getGlobalScope(n.getNode())->cfgForEnd()); }

bool StxPart::equal(const PartPtr& o) const
{
  /*ROSE_ASSERT(boost::dynamic_pointer_cast<StxPart>(o));
  return n == boost::dynamic_pointer_cast<StxPart>(o)->n;*/
  ROSE_ASSERT(dynamic_part_cast<StxPart>(o).get());
  return n == dynamic_part_cast<StxPart>(o)->n;
}

bool StxPart::less(const PartPtr& o) const
{
  /*ROSE_ASSERT(boost::dynamic_pointer_cast<StxPart>(o));
  return n < boost::dynamic_pointer_cast<StxPart>(o)->n;*/
  ROSE_ASSERT(dynamic_part_cast<StxPart>(o).get());
  return n < dynamic_part_cast<StxPart>(o)->n;
}

std::string StxPart::str(std::string indent)
{
  ostringstream oss;
  if(isNULLCFGNode(n.getNode())) oss << "[*]";
  else oss << cfgUtils::CFGNode2Str(n);//", analysis="<<analysis<<"]";
  return oss.str();
}

/***********************
 ***** StxPartEdge *****
 ***********************/

PartPtr StxPartEdge::source() {
  if(isNULLCFGNode(p.source().getNode())) return NULLPart;
  else return makePart<StxPart>(p.source(), analysis, filter);
}

PartPtr StxPartEdge::target() { 
  if(isNULLCFGNode(p.target().getNode())) return NULLPart;
  else return makePart<StxPart>(p.target(), analysis, filter);
}

// Let A={ set of execution prefixes that terminate at the given anchor SgNode }
// Let O={ set of execution prefixes that terminate at anchor's operand SgNode }
// Since to reach a given SgNode an execution must first execute all of its operands it must
//    be true that there is a 1-1 mapping m() : O->A such that o in O is a prefix of m(o).
// This function is the inverse of m: given the anchor node and operand as well as the
//    PartEdge that denotes a subset of A (the function is called on this PartEdge), 
//    it returns a list of PartEdges that partition O.
std::list<PartEdgePtr> StxPartEdge::getOperandPartEdge(SgNode* anchor, SgNode* operand)
{
  // operand precedes anchor in the CFG, either immediately or at some distance. As such, the edge
  // we're looking for is not necessarily the edge from operand to anchor but rather the first
  // edge along the path from operand to anchor. Since operand is part of anchor's expression
  // tree we're guaranteed that there is only one such path.
  CFGNode opCFG = operand->cfgForEnd();
  //Dbg::dbg << "opCFG="<<cfgUtils::CFGNode2Str(opCFG)<<endl;
  StxPart opPart(opCFG, analysis);
  ROSE_ASSERT(opPart.outEdges().size()==1);
  list<PartEdgePtr> l;
  StxPartPtr partTarget = (*(opPart.outStxEdges().begin()))->target();
  ROSE_ASSERT(partTarget);
  ROSE_ASSERT(partTarget->n.getNode());
  l.push_back(makePart<StxPartEdge>(opCFG, partTarget->n, analysis));
  return l;
}

// If the source Part corresponds to a conditional of some sort (if, switch, while test, etc.)
// it must evaluate some predicate and depending on its value continue, execution along one of the
// outgoing edges. The value associated with each outgoing edge is fixed and known statically.
// getPredicateValue() returns the value associated with this particular edge. Since a single 
// Part may correspond to multiple CFGNodes getPredicateValue() returns a map from each CFG node
// within its source part that corresponds to a conditional to the value of its predicate along 
// this edge.
map<CFGNode, boost::shared_ptr<SgValueExp> > StxPartEdge::getPredicateValue()
{
  CFGNode cn = p.source();
  
  map<CFGNode, boost::shared_ptr<SgValueExp> > pv;
       if(p.condition() == eckTrue)  pv[cn] = boost::shared_ptr<SgValueExp>(SageBuilder::buildBoolValExp(true));
  else if(p.condition() == eckFalse) pv[cn] = boost::shared_ptr<SgValueExp>(SageBuilder::buildBoolValExp(false));
  else if(p.condition() == eckCaseLabel) {
    ROSE_ASSERT(isSgValueExp(p.caseLabel()));
    pv[cn] = boost::shared_ptr<SgValueExp>(isSgValueExp(p.caseLabel()));
  }
  
  return pv;
}

bool StxPartEdge::equal(const PartEdgePtr& o) const
{
  ROSE_ASSERT(dynamic_part_cast<StxPartEdge>(o).get());
  /*Dbg::dbg << "StxPartEdge::operator<("<<(p.source() == dynamic_part_cast<StxPartEdge>(o)->p.source() &&
         p.target() == dynamic_part_cast<StxPartEdge>(o)->p.target())<<endl; //(p == dynamic_part_cast<StxPartEdge>(o)->p)<<endl;
  Dbg::dbg << "---- p="<<cfgUtils::CFGPath2Str(p)<<endl;
  Dbg::dbg << "---- dynamic_part_cast<StxPartEdge>(o)->p"<<cfgUtils::CFGPath2Str(dynamic_part_cast<StxPartEdge>(o)->p)<<endl;*/
  //return p == dynamic_part_cast<StxPartEdge>(o)->p;
  // Since is the possible to create p either from makeClosureDF() or from its source/target CFGNode pair, we compare
  // paths in terms of just their source/target CFGNodes
  return p.source() == dynamic_part_cast<StxPartEdge>(o)->p.source() &&
         p.target() == dynamic_part_cast<StxPartEdge>(o)->p.target();
}

bool StxPartEdge::less(const PartEdgePtr& o) const
{
  ROSE_ASSERT(dynamic_part_cast<StxPartEdge>(o).get());
  /*Dbg::dbg << "StxPartEdge::operator<(source="<<cfgUtils::CFGNode2Str(p.source())<<
                                 ", o.source="<<cfgUtils::CFGNode2Str(dynamic_part_cast<StxPartEdge>(o)->p.source())<<",\n"<<
                                     "target="<<cfgUtils::CFGNode2Str(p.target())<<
                                 ", o.target="<<cfgUtils::CFGNode2Str(dynamic_part_cast<StxPartEdge>(o)->p.target())<<",\n"<<
          ", source: < "<<(p.source() < dynamic_part_cast<StxPartEdge>(o)->p.source())<<" == "<<(p.source() == dynamic_part_cast<StxPartEdge>(o)->p.source())<<"\n"<<
          ", target: < "<<(p.target() < dynamic_part_cast<StxPartEdge>(o)->p.target())<<" == "<<(p.target() == dynamic_part_cast<StxPartEdge>(o)->p.target())<<"\n";*/
  /*Dbg::dbg << "StxPartEdge::operator<("<<((p.source() < dynamic_part_cast<StxPartEdge>(o)->p.source()) ||
         (p.source() == dynamic_part_cast<StxPartEdge>(o)->p.source() &&
          p.target() < dynamic_part_cast<StxPartEdge>(o)->p.target()))<<endl; //(p < dynamic_part_cast<StxPartEdge>(o)->p)<<endl;
  Dbg::dbg << "---- p="<<cfgUtils::CFGPath2Str(p)<<endl;
  Dbg::dbg << "---- dynamic_part_cast<StxPartEdge>(o)->p"<<cfgUtils::CFGPath2Str(dynamic_part_cast<StxPartEdge>(o)->p)<<endl;*/
  //return p < dynamic_part_cast<StxPartEdge>(o)->p;
  // Since is the possible to create p either from makeClosureDF() or from its source/target CFGNode pair, we compare
  // paths in terms of just their source/target CFGNodes
  return (p.source() < dynamic_part_cast<StxPartEdge>(o)->p.source()) ||
         (p.source() == dynamic_part_cast<StxPartEdge>(o)->p.source() &&
          p.target() < dynamic_part_cast<StxPartEdge>(o)->p.target());
}

std::string StxPartEdge::str(std::string indent)
{
  ostringstream oss;
  oss << (isNULLCFGNode(p.source().getNode())? "*" : source()->str()) << 
         Dbg::escape(" ==> ") << 
         (isNULLCFGNode(p.target().getNode())? "*" : target()->str());// << ", analysis="<<analysis
  return oss.str();
}

/*******************************
 ***** StxMemLocObjectKind *****
 *******************************/

// Casts this object to a NamedObj, ExprObj or AliasedObj, returning 
// the pointer if it is one of these kinds or NULL otherwise
NamedObj*   StxMemLocObjectKind::isNamedObj()   { return dynamic_cast<NamedObj*>(this); }
ExprObj*    StxMemLocObjectKind::isExprObj()    { return dynamic_cast<ExprObj*>(this); }
AliasedObj* StxMemLocObjectKind::isAliasedObj() { return dynamic_cast<AliasedObj*>(this); }

/***************************
 ***** StxMemLocObject *****
 ***************************/
StxMemLocObject::StxMemLocObject(SgNode* n, SgType* t, StxMemLocObjectKindPtr kind) : 
  MemLocObject(n), type(t), kind(kind)
{
  ROSE_ASSERT(kind);
}

StxMemLocObject::eqType StxMemLocObject::equal(StxMemLocObjectPtr that_arg, PartEdgePtr pedge) 
{
  StxMemLocObjectPtr that = boost::dynamic_pointer_cast <StxMemLocObject> (that_arg);
  
  //Dbg::dbg << "StxMemLocObject::equal()  isLiveML(pedge)="<<isLiveML(pedge)<<" that->isLiveML(pedge)="<<that->isLiveML(pedge)<<endl;
  if(isLiveML(pedge)) {
    // One is in-scope but the other is out-of-scope: different classes
    if(!that->isLiveML(pedge)) return defNotEqual;
    // Both are in-scope: need more refined processing
    else               return unknown;
  } else {
    // Both are out-of-scope: same class
    if(!that->isLiveML(pedge)) return defEqual;
    // One is in-scope but the other is out-of-scope: different classes
    else               return defNotEqual;
  }
}

// Returns whether this object may/must be equal to o within the given Part p
// These methods are called by composers and should not be called by analyses.
bool StxMemLocObject::mayEqualML(MemLocObjectPtr that_arg, PartEdgePtr pedge)
{
  StxMemLocObjectPtr that = boost::dynamic_pointer_cast <StxMemLocObject> (that_arg);
  ROSE_ASSERT(that);
  
  // If StxMemLocObject says they're definitely equal/not equal, return true/false
  switch(StxMemLocObject::equal(that, pedge)) {
    case StxMemLocObject::defEqual:    return true;
    case StxMemLocObject::defNotEqual: return false;
    case StxMemLocObject::unknown:     
    default:                           return kind->mayEqualML(that->kind, pedge);
  }
}

bool StxMemLocObject::mustEqualML(MemLocObjectPtr that_arg, PartEdgePtr pedge)
{
  StxMemLocObjectPtr that = boost::dynamic_pointer_cast <StxMemLocObject> (that_arg);
  ROSE_ASSERT(that);
  
  // If StxMemLocObject says they're definitely equal/not equal, return true/false
  switch(StxMemLocObject::equal(that, pedge)) {
    case StxMemLocObject::defEqual:    return true;
    case StxMemLocObject::defNotEqual: return false;
    case StxMemLocObject::unknown:     
    default:                           return kind->mustEqualML(that->kind, pedge);
  }
}

// Returns whether the two abstract objects denote the same set of concrete objects
bool StxMemLocObject::equalSet(AbstractObjectPtr that_arg, PartEdgePtr pedge)
{
  StxMemLocObjectPtr that = boost::dynamic_pointer_cast <StxMemLocObject> (that_arg);
  ROSE_ASSERT(that);
  
  // If StxMemLocObject says they're definitely equal/not equal, return true/false
  switch(StxMemLocObject::equal(that, pedge)) {
    case StxMemLocObject::defEqual:    return true;
    case StxMemLocObject::defNotEqual: return false;
    case StxMemLocObject::unknown:     
    default:                           return kind->equalSet(that->kind, pedge);
  }
}

// Returns true if this object is live at the given part and false otherwise.
// This method is called by composers and should not be called by analyses.
bool StxMemLocObject::isLiveML(PartEdgePtr pedge)
{ return kind->isLiveML(pedge); }

// Computes the meet of this and that and saves the result in this
// returns true if this causes this to change and false otherwise
bool StxMemLocObject::meetUpdateML(MemLocObjectPtr that_arg, PartEdgePtr pedge)
{
  StxMemLocObjectPtr that = boost::dynamic_pointer_cast <StxMemLocObject> (that_arg);
  ROSE_ASSERT(that);
  
  // Make sure that the two objects have the same type
  ROSE_ASSERT(type == that->type);
  
  // If the MemLocs are both NamedObjects or both ExprObjects
  if((kind->isNamedObj() && that->kind->isNamedObj()) ||
     (kind->isExprObj()  && that->kind->isExprObj())) {
    // If they're not the same named object, replace this one with an aliased object
    if(!kind->mustEqualML(that->kind, pedge)) {
      kind = createAliasedMemLocObjectKind(getBase(), type, pedge);
      ROSE_ASSERT(kind);
      return true;
    }
  // The MemLocs are different kinds of objects (named vs expr) and this is not already an aliased object,
  // make it into an aliased object
  } else if(!kind->isAliasedObj()) {
    kind = createAliasedMemLocObjectKind(getBase(), type, pedge);
    ROSE_ASSERT(kind);
    return true;
  }
  
  return false;
}

// Returns whether this AbstractObject denotes the set of all possible execution prefixes.
bool StxMemLocObject::isFull(PartEdgePtr pedge)
{ return kind->isFull(pedge); }

// Returns whether this AbstractObject denotes the empty set.
bool StxMemLocObject::isEmpty(PartEdgePtr pedge)
{ return kind->isEmpty(pedge); }

// Allocates a copy of this object and returns a pointer to it
MemLocObjectPtr StxMemLocObject::copyML() const
{
  return boost::make_shared<StxMemLocObject>(getBase(), type, kind->copyMLK());
}

// pretty print for the object
std::string StxMemLocObject::str(std::string indent)
{
  string out = "[StxML: ";
  /*if (type != NULL )
    out += "  type:" + type->unparseToString()/ * + " @ " + StringUtility::numberToString(type)* /;
  else
    out += "  type: NULL";*/
  
  out += " | " + kind->str(indent) + "]";
  
  return out;
}

std::string StxMemLocObject::strp(PartEdgePtr pedge, std::string indent) // pretty print for the object
{
  string out = "[StxML: ";
  if (type != NULL )
    out += "  type:" + type->unparseToString()/* + " @ " + StringUtility::numberToString(type)*/;
  else
    out += "  type: NULL";
  
  out += " | " + kind->strp(pedge, indent) + "]";
  
  return out;
}

/**************************
 ***** StxValueObject *****
 **************************/

StxValueObject::StxValueObject(SgNode* n) : ValueObject(n)
{
  // If a valid node is passed, check if it is an SgValue
  if(n) {
    if(stxAnalysisDebugLevel>=1) {
      Dbg::dbg << "StxValueObject::StxValueObject("<<cfgUtils::SgNode2Str(n)<<")";
      Dbg::dbg << " isSgCastExp(n)="<<isSgCastExp(n)<<" cfgUtils::unwrapCasts(isSgCastExp(n))="<<(isSgCastExp(n) ? cfgUtils::SgNode2Str(cfgUtils::unwrapCasts(isSgCastExp(n))) : "NULL")<<" iscast="<<(isSgCastExp(n) ? isSgValueExp(cfgUtils::unwrapCasts(isSgCastExp(n))) : 0)<<endl;
    }
    if(isSgValueExp(n)) 
      val = isSgValueExp(n);
    // If this is a value that has been wrapped in many casts
    // GB 2012-10-09 - NOTE: in the future we'll need to refine this code to accurately capture the effect of these casts!
    else if(isSgCastExp(n) && isSgValueExp(cfgUtils::unwrapCasts(isSgCastExp(n))))
      val = isSgValueExp(cfgUtils::unwrapCasts(isSgCastExp(n)));
    else
      val = NULL;
  // Otherwise, default this ValueObject to an unknown 
  } else 
    val = NULL;
}

StxValueObject::StxValueObject(const StxValueObject& that) : ValueObject((const ValueObject&)that), val(that.val)
{ }

bool StxValueObject::mayEqualV(ValueObjectPtr other, PartEdgePtr pedge)
{
  StxValueObjectPtr svo = boost::dynamic_pointer_cast <StxValueObject> (other);
  // ValueObject abstractions of different types may be equal to each other (can't tell either way)
  if(!svo) { return true; }
  
  // If either object is not an SgValue, they may be equal to each other
  if(val==NULL || svo->val==NULL) { return true; }
  
  // If both are SgValues, equalValExp makes a definitive precise comparison
  return equalValExp(val, svo->val);
}

bool StxValueObject::mustEqualV(ValueObjectPtr other, PartEdgePtr pedge)
{
  //const StxValueObject & svo = dynamic_cast <const StxValueObject&> (other);
  StxValueObjectPtr svo = boost::dynamic_pointer_cast <StxValueObject> (other);
  // ValueObject abstractions of different types can't be proven to be definitely equal to each other (can't tell either way)
  if(!svo) { return false; }
  
  // If either object is not an SgValue, we can't prove that must be equal to each other
  if(val==NULL || svo->val==NULL) { return false; }
  
  // If both are SgValues, equalValExp makes a definitive precise comparison
  //if(stxAnalysisDebugLevel>=1) Dbg::dbg << "StxValueObject::mustEqualV calling equalValExp("<<cfgUtils::SgNode2Str(val)<<", "<<cfgUtils::SgNode2Str(svo->val)<<")"<<endl;
  return equalValExp(val, svo->val);
}

// Returns whether the two abstract objects denote the same set of concrete objects
bool StxValueObject::equalSet(AbstractObjectPtr other, PartEdgePtr pedge)
{
  //const StxValueObject & svo = dynamic_cast <const StxValueObject&> (other);
  StxValueObjectPtr svo = boost::dynamic_pointer_cast <StxValueObject> (other);
  // ValueObject abstractions of different types can't be proven to be definitely equal to each other (can't tell either way)
  if(!svo) { return false; }
  
  // If neither object is not a known SgValue, they both denote the set of all Values, 
  if(val==NULL && svo->val==NULL) { return true; }
  // If only one of the objects is not a known SgValue, they denote different sets
  if(val==NULL || svo->val==NULL) { return false; }
  
  // If both are SgValues, equalValExp makes a definitive precise comparison
  return equalValExp(val, svo->val);
}

// Returns true if the given pair of SgValueExps represent the same value and false otherwise
bool StxValueObject::equalValExp(SgValueExp* a, SgValueExp* b)
{
  if(isSgBoolValExp(a) && isSgBoolValExp(b)) 
    return isSgBoolValExp(a)->get_value() == isSgBoolValExp(b)->get_value();
  else if(isSgCharVal(a) && isSgCharVal(a)) 
    return isSgCharVal(a)->get_value() == isSgCharVal(b)->get_value();
  else if(isSgComplexVal(a) && isSgComplexVal(b))
    return equalValExp(isSgComplexVal(a)->get_real_value(), isSgComplexVal(b)->get_real_value()) &&
           equalValExp(isSgComplexVal(a)->get_imaginary_value(), isSgComplexVal(b)->get_imaginary_value());
  else if(isSgDoubleVal(a) && isSgDoubleVal(b))
    return isSgDoubleVal(a)->get_value() == isSgDoubleVal(b)->get_value();
  else if(isSgEnumVal(a) && isSgEnumVal(b))
    return isSgEnumVal(a)->get_value() == isSgEnumVal(b)->get_value();
  else if(isSgFloatVal(a) && isSgFloatVal(b))
    return isSgFloatVal(a)->get_value() == isSgFloatVal(b)->get_value();
  else if(isSgIntVal(a) && isSgIntVal(b))
    return isSgIntVal(a)->get_value() == isSgIntVal(b)->get_value();
  else if(isSgLongDoubleVal(a) && isSgLongDoubleVal(b))
    return isSgLongDoubleVal(a)->get_value() == isSgLongDoubleVal(b)->get_value();
  else if(isSgLongIntVal(a) && isSgLongIntVal(b))
    return isSgLongIntVal(a)->get_value() == isSgLongIntVal(b)->get_value();
  else if(isSgLongLongIntVal(a) && isSgLongLongIntVal(b))
    return isSgLongLongIntVal(a)->get_value() == isSgLongLongIntVal(b)->get_value();
  else if(isSgShortVal(a) && isSgShortVal(b))
    return isSgShortVal(a)->get_value() == isSgShortVal(b)->get_value();
  else if(isSgStringVal(a) && isSgStringVal(b))
    return isSgStringVal(a)->get_value() == isSgStringVal(b)->get_value();
  else if(isSgUnsignedCharVal(a) && isSgUnsignedCharVal(b))
    return isSgUnsignedCharVal(a)->get_value() == isSgUnsignedCharVal(b)->get_value();
  else if(isSgUnsignedIntVal(a) && isSgUnsignedIntVal(b))
    return isSgUnsignedIntVal(a)->get_value() == isSgUnsignedIntVal(b)->get_value();
  /*else if(isSgUnsigedLongLongIntVal(a) && isSgUnsigedLongLongIntVal(b))
    return isSgUnsigedLongLongIntVal(a)->get_value() == isSgUnsigedLongLongIntVal(b)->get_value();*/
  else if(isSgUnsignedLongVal(a) && isSgUnsignedLongVal(b))
    return isSgUnsignedLongVal(a)->get_value() == isSgUnsignedLongVal(b)->get_value();
  else if(isSgUnsignedShortVal(a) && isSgUnsignedShortVal(b))
    return isSgUnsignedShortVal(a)->get_value() == isSgUnsignedShortVal(b)->get_value();
  /*else if(isSgUpcMythreadVal(a) && isSgUpcMythreadVal(b))
    return isSgUpcMythreadVal(a)->get_value() == isSgUpcMythreadVal(b)->get_value();
  else if(isSgUpcThreadsVal(a) && isSgUpcThreadsVal(b))
    return isSgUpcThreadsVal(a)->get_value() == isSgUpcThreadsVal(b)->get_value();*/
  else if(isSgWcharVal(a) && isSgWcharVal(b))
    return isSgWcharVal(a)->get_value() == isSgWcharVal(b)->get_value();
  else
    return false;
}

// Computes the meet of this and that and saves the result in this.
// Returns true if this causes this to change and false otherwise.
bool StxValueObject::meetUpdateV(ValueObjectPtr that, PartEdgePtr pedge)
{
  StxValueObjectPtr svo = boost::dynamic_pointer_cast <StxValueObject> (that);
  ROSE_ASSERT(svo);
  
  // If the value objects denote different values
  if(!mustEqualV(that, pedge)) {
    // Set the value pointer of this object to NULL since we cannot represent their union with a single value
    val = NULL;
    return true;
  }
  return false;
}

bool StxValueObject::isFull(PartEdgePtr pedge)
{ return val == NULL; }

bool StxValueObject::isEmpty(PartEdgePtr pedge)
{ return false; }


// Returns true if this ValueObject corresponds to a concrete value that is statically-known
bool StxValueObject::isConcrete()
{
  return val;
}

// Returns the type of the concrete value (if there is one)
SgType* StxValueObject::getConcreteType()
{
  ROSE_ASSERT(val);
  SgTreeCopy copyHelp;
  return (SgType*)(val->get_type()->copy(copyHelp));
}

// Returns the concrete value (if there is one) as an SgValueExp, which allows callers to use
// the normal ROSE mechanisms to decode it
boost::shared_ptr<SgValueExp> StxValueObject::getConcreteValue()
{
  ROSE_ASSERT(val);
  SgTreeCopy copyHelp;
  return boost::shared_ptr<SgValueExp>((SgValueExp*)val->copy(copyHelp));
}
 
//std::string StxValueObject::str(const string& indent) {
std::string StxValueObject::str(std::string indent) { // pretty print for the object
  return "[StxValueObject: "+(val? val->unparseToString() : "NULL")+"]";
}

// Allocates a copy of this object and returns a pointer to it
ValueObjectPtr StxValueObject::copyV() const 
{ return boost::make_shared<StxValueObject>(*this); }

/****************************
 ***** StxCodeLocObject *****
 ****************************/

StxCodeLocObject::StxCodeLocObject(SgNode* n, PartEdgePtr pedge) : pedge(pedge)
{
  code = isSgExpression(n);
}    

StxCodeLocObject::StxCodeLocObject(const StxCodeLocObject& that) : pedge(that.pedge), code(that.code)
{ }

bool StxCodeLocObject::mayEqualCL(CodeLocObjectPtr other, PartEdgePtr pedge)
{
  StxCodeLocObjectPtr svo = boost::dynamic_pointer_cast <StxCodeLocObject> (other);
  if(!svo) { return false; }
  // Return true if either CodeLocObject is a wildcard
  if(code==NULL || svo->code==NULL) return true;
  else                              return mustEqualCL(other, pedge);
}

bool StxCodeLocObject::mustEqualCL(CodeLocObjectPtr other, PartEdgePtr pedge)
{
  StxCodeLocObjectPtr svo = boost::dynamic_pointer_cast <StxCodeLocObject> (other);
  if(!svo) { return false; }
  if(isSgFunctionCallExp(code) && isSgFunctionCallExp(svo->code) &&
     isSgFunctionCallExp(code)->getAssociatedFunctionSymbol() && 
     isSgFunctionCallExp(svo->code)->getAssociatedFunctionSymbol())
    return isSgFunctionCallExp(code)->getAssociatedFunctionSymbol()->get_name() == 
           isSgFunctionCallExp(svo->code)->getAssociatedFunctionSymbol()->get_name();
  else
    return false;
}

// Returns whether the two abstract objects denote the same set of concrete objects
bool StxCodeLocObject::equalSet(AbstractObjectPtr other, PartEdgePtr pedge)
{
  StxCodeLocObjectPtr svo = boost::dynamic_pointer_cast <StxCodeLocObject> (other);
  if(!svo) { return false; }
  // If both objects denote a concrete function, they denote the same set of those functions are equal
  if(isSgFunctionCallExp(code) && isSgFunctionCallExp(svo->code) &&
     isSgFunctionCallExp(code)->getAssociatedFunctionSymbol() && 
     isSgFunctionCallExp(svo->code)->getAssociatedFunctionSymbol())
    return isSgFunctionCallExp(code)->getAssociatedFunctionSymbol()->get_name() == 
           isSgFunctionCallExp(svo->code)->getAssociatedFunctionSymbol()->get_name();
  // If both objects denote the set of all CodeLocs, they're equal
  else if(code==NULL && svo->code==NULL)
    return true;
  else
    return false;
}

// Computes the meet of this and that and saves the result in this
// returns true if this causes this to change and false otherwise
bool StxCodeLocObject::meetUpdateCL(CodeLocObjectPtr that, PartEdgePtr pedge)
{
   StxCodeLocObjectPtr svo = boost::dynamic_pointer_cast <StxCodeLocObject> (that);
   ROSE_ASSERT(svo);
   
   // If the objects denote different code location expressions, 
   // make this into a wildcard location
   if(code != svo->code) {
     code = NULL;
     return true;
   }
   
   return false;
}

bool StxCodeLocObject::isFull(PartEdgePtr pedge)
{ return code == NULL; }

bool StxCodeLocObject::isEmpty(PartEdgePtr pedge)
{ return false; }

std::string StxCodeLocObject::str(std::string indent) { // pretty print for the object
  return "[StxCodeLocObject: "+(code? code->unparseToString() : "NULL")+"]";
}

// Allocates a copy of this object and returns a pointer to it
CodeLocObjectPtr StxCodeLocObject::copyCL() const 
{ return boost::make_shared<StxCodeLocObject>(*this); }

/***************************
 ***** StxMemLocObject *****
 ***************************/

  // Only useful to provide compare operator of std::map.
  // This operator is not semantically consistent: if a mustEqual b and a < c 
  // then it is not guaranteed that b < c.
  bool Scalar_Impl::operator < (const MemLocObject& other) const
  {
    return (this < &other);
  }

  bool Function_Impl::operator < (const MemLocObject& other) const
  {
    return (this < &other);
  }

  bool LabeledAggregate_Impl::operator < (const MemLocObject& other) const
  {
    return (this < &other);
  }

  bool Array_Impl::operator < (const MemLocObject& other) const
  {
    return (this < &other);
  }

  bool Pointer_Impl::operator < (const MemLocObject& other) const
  {
    return (this < &other);
  }

  /*GB: Deprecating IndexSets and replacing them with ValueObjects.
  IndexSet::~IndexSet()
  {
    cerr<<"Error. Calling the base destructor of IndexSet is not allowed. "<<endl;
    assert (false);
  }

  std::map <size_t, ConstIndexSet * >  ConstIndexSet::constIndexMap;
  UnknownIndexSet* UnknownIndexSet::inst = NULL;

  bool IndexSet::operator==(const IndexSet & other) const
  {
    cerr<<"Error. Calling the base operator=() of IndexSet is not allowed. "<<endl;
    assert (false);
    return false;
  }

  bool IndexSet::mayEqual(const IndexSet & other, const Part& p) const
  {
    cerr<<"Error. Calling the base mayEqual() of IndexSet is not allowed. "<<endl;
    assert (false);
    return false;
  }

  bool IndexSet::mustEqual(const IndexSet & other, const Part& p) const
  {
    cerr<<"Error. Calling the base mustEqual() of IndexSet is not allowed. "<<endl;
    assert (false);
    return false;
  }

  bool IndexSet::operator!=(const IndexSet & other) const
  {
    cerr<<"Error. Calling the base operator!=() of IndexSet is not allowed. "<<endl;
    assert (false);
    return false;
  }

  bool ConstIndexSet::operator!= (const IndexSet & other) const
  {
    return !(*this == other);
  }

  bool ConstIndexSet::operator == (const IndexSet & other) const
  {
    bool rt = false;
    try
    {
      const ConstIndexSet & cis = dynamic_cast <const ConstIndexSet&> (other);
      return (cis.value == this->value);
    }
    catch (bad_cast & bc)
    {
      try
      {
        const UnknownIndexSet & uis = dynamic_cast <const UnknownIndexSet&> (other);
        return (uis un *this);
      }
      catch (bad_cast & bc)
      {
        cerr<<"Error: unreachable branch reached ."<<endl;
        assert (false);
      }
    }
    return rt;
  }

  bool ConstIndexSet::mayEqual(const IndexSet & other, const Part& p) const
  {
    bool rt = false;
    try
    {
      const ConstIndexSet & cis = dynamic_cast <const ConstIndexSet&> (other);
      return (cis.value == this->value);
    }
    catch (bad_cast & bc)
    {
      try
      {
        const UnknownIndexSet & uis = dynamic_cast <const UnknownIndexSet&> (other);
        ROSE_ASSERT(&uis); // this is a use of uis to avoid a warning
        return true; // may equal to an unknown index set
      }
      catch (bad_cast & bc)
      {
        cerr<<"Error: unreachable branch reached ."<<endl;
        assert (false);
      }

    }
   return rt;
  }

  bool ConstIndexSet::mustEqual(const IndexSet & other, const Part& p) const
  {
    bool rt = false;
    try
    {
      const ConstIndexSet & cis = dynamic_cast <const ConstIndexSet&> (other);
      return (cis.value == this->value);
    }
    catch (bad_cast & bc)
    {
      try
      {
        const UnknownIndexSet & uis = dynamic_cast <const UnknownIndexSet&> (other);
        ROSE_ASSERT(&uis); // this is a use of uis to avoid a warning
        return false; // cannot decide if it is a must relation
      }
      catch (bad_cast & bc)
      {
        cerr<<"Error: unreachable branch reached ."<<endl;
        assert (false);
      }

    }
   return rt;
  }
 
  ConstIndexSet* ConstIndexSet::get_inst(SgValueExp * v_exp){
    size_t v; 
    assert (v_exp != NULL);
    SgType* t = v_exp->get_type();
    assert (t!= NULL);
    assert (SageInterface::isStrictIntegerType (t) == true);
    switch (v_exp->variantT())
    { 
      //Take advantage of the fact that the value expression is always SgUnsignedLongVal in AST
      case V_SgUnsignedLongVal:
      { 
        SgUnsignedLongVal* ul_exp = isSgUnsignedLongVal (v_exp); 
        v =  ul_exp->get_value() ; 
        break;
      }
      case V_SgIntVal:
      { 
        SgIntVal* i_exp = isSgIntVal (v_exp); 
        assert (i_exp->get_value() >=0);
        v =  i_exp->get_value() ; 
        break;
      }
 
      //TODO Handle other integer value nodes
      default:
      {
        cerr<<"Error in ConstIndexSet::ConstIndexSet(), unexpected integer valued SgValueExp type: " << v_exp->class_name()<<endl;
        assert(false);
      }
    }
    return get_inst (v);
  }

  ConstIndexSet* ConstIndexSet::get_inst(size_t v)
  {
    ConstIndexSet* rt = constIndexMap[v];
    if ( rt == NULL)
    {
      rt = new ConstIndexSet (v);
      constIndexMap[v] = rt;
    }
    assert (rt);
    return rt;   
  }

  UnknownIndexSet* UnknownIndexSet::get_inst()
  {
    if (inst == NULL)
      inst = new UnknownIndexSet();
    return inst;  
  }*/

  // convert std::vector<SgExpression*>* subscripts to IndexVectorPtr array_index_vector
  // We only generate two kinds of IndexSet : ConstIndexSet or UnkownIndexSet
  IndexVectorPtr generateIndexVector (std::vector<SgExpression*>& subscripts)
  {
    assert (subscripts.size() >0 );
    IndexVector_ImplPtr rt = boost::make_shared<IndexVector_Impl>();  // TODO how to avoid duplicated creation here, or we don't care
    std::vector<SgExpression*>::iterator iter;
    
    for (iter = subscripts.begin(); iter != subscripts.end(); iter++)
    {
      rt->index_vector.push_back(boost::make_shared<StxValueObject>(*iter));
      /*GB: Deprecating IndexSets and replacing them with ValueObjects.
      SgExpression* exp = *iter;
      if (isSgValueExp (exp))
      {
        rt->index_vector.push_back(ConstIndexSet::get_inst(isSgValueExp (exp)));
      }
      else
      {
        rt->index_vector.push_back(UnknownIndexSet::get_inst()); 
      }*/
    }  
    return rt; 
  }
     
  //there are at least three levels resolution for modeling memory for labeled aggregates (structures, classes, etc)
  //
  //Think the following example:
  //-------------------
  //struct A { int i; int j;} ;
  //struct A a1;
  //struct A a2;
  //
  //int x = a1.i + a2.i + a2.j + a2.j;
  //-------------------
  //
  //1. most coarse: treat any references to struct A's instance as the same to struct A.
  //    This means to treat a1.i, a2.i, a2.j, a2.j as the same access to struct A.
  //    There is only one memory object for all a1.i, a2.i, a2.j and a2.j.
  //
  //2. finer: distinguish between different fields of A. But the same field to all instances will be treated as the same.
  //
  //  Now a1.i and a2.i will be modeled as A.i
  //      a2.j and a2.j will be modeled as A.j
  //  There are two memory objects for all a1.i, a2.i, a2.j and a2.j.
  //
  //3. Finest : distinguish between not only the fields, but also the instances.
  //
  //    a1.i, a2.i, a2.j, a2.j will be represented as four different objects.
  
  // We decide the resolution of this implementation is to distinguish between both fields and instances of aggregated objects.
  // This means type declarations (e.g. struct A{}) without declared instances (struct A a1; )will not have the corresponding memory objects.
  //
  // Naively creating NamedObj from symbol won't work since a single symbol can be shared by different NamedObj,
  // e.g.,  a1.i, and a2.i are different. But the field i will share the same symbol
  // So we have to traverse the SgVarRef to generate NamedObj
  // three level case: a1.mag.size  the AST will look like
  //    (a1 <-- SgDotExp --> mag ) <-- SgDotExp --> size
  //
  // To create all NamedObj, one has to search both symbols and SgVarRef and skip the overlapped objects.
  //
  // For all symbols, there are two categories
  // 1. symbols corresponding to real top level instances of types. Create NamedObj as we see each of them, NULL as parent
  //    They are the symbols with declarations not under SgClassDefinition
  // 2. symbols within type declarations: does not correspond NamedObj by itself. We should ignore them until we see the instance  
  //    They are the symbols with declarations under SgClassDefinition
  //   
  // For all SgVarRef, find the corresponding symbol 
  // 1. if is a instance symbol. skip since we already created it
  // 2. if it is a symbol within a declaration, two cases
  //    a. the symbol has a pointer type, we don't track pointer aliasing, so we create AliasedObj for it
  //    b. other types: a child of an instance, check if is the rhs of SgDotExp/SgArrowExp, if not assert
  //        use lhs of SgDotExp/SgArrowExp as parent
  //            lhs could be SgVarRefExp: find the corresponding NamedObj as parent (top level object, labeled aggregate)
  //            lhs could be another SgDotExp: find its rhs's NamedObj as parent
  //  
  //  To avoid duplicated NamedObj, map[parent][symbol] -> NamedObj
  //
  // TODO: build the connection between SgVarRefExp and the created NamedObj and support fast lookup!

  std::string LabeledAggregateField_Impl::getName(PartEdgePtr pedge)
  {
    MemLocObjectPtr f = getField(pedge);
    boost::shared_ptr<NamedObj> nn = boost::dynamic_pointer_cast<NamedObj>(f);
    assert(nn);
    return nn->getName(); 
  }

  size_t LabeledAggregateField_Impl::getIndex(PartEdgePtr pedge)
  {
    LabeledAggregatePtr parent = getParent(pedge);
    list<LabeledAggregateFieldPtr > elements = parent->getElements(pedge);
    int i=0;
    for(list<LabeledAggregateFieldPtr >::iterator e=elements.begin(); e!=elements.end(); e++) {
      if(this == (*e).get())
        return i;
      i++;
    }
    ROSE_ASSERT(0); // must find it! 
    return i;
  }

  //std::string LabeledAggregateField_Impl::str(const string& indent)
  std::string LabeledAggregateField_Impl::str(std::string indent) // pretty print for the object
  {
    string rt;
    rt = "LabeledAggregateField_Impl: parent "/*@ " + StringUtility::numberToString(parent.get()) */ + string(" field ") 
      + field->str(indent+"    ");
    return rt;
  }

  // --------------------- Expression Object --------------------
  // concern about the ExprObj itself , not the value it contains/stores
  bool ExprObj::mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    //const ExprObj & expr_o2 = dynamic_cast <const ExprObj&> (o2);
    ExprObjPtr expr_o2 = boost::dynamic_pointer_cast <ExprObj> (o2);
    //Dbg::dbg << "ExprObj::mayEqualML this="<<str("")<<endl;
    //Dbg::dbg << "                   o2="<<(o2? o2->str("") : "NULL")<<endl;
    //Dbg::dbg << "    ==>"<<(!expr_o2 ? false: ( this -> anchor_exp  == expr_o2->anchor_exp))<<endl;
    
    if(!expr_o2) { return false; }
    return ( this -> anchor_exp  == expr_o2->anchor_exp);
  }

  // reuse the equal operator, which is must equal for ExprObj
  bool ExprObj::mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    //const ExprObj & expr_o2 = dynamic_cast <const ExprObj&> (o2);
    ExprObjPtr expr_o2 = boost::dynamic_pointer_cast <ExprObj> (o2);
    /*Dbg::dbg << "ExprObj::mustEqualML this="<<str("")<<endl;
    Dbg::dbg << "                   expr_o2="<<(o2? o2->str("") : "NULL")<<endl;
    Dbg::dbg << "    ==>"<<(!expr_o2 ? false: ( this -> anchor_exp  == expr_o2->anchor_exp))<<endl;*/
    
    if(!expr_o2) { return false; }
    return (  this -> anchor_exp  == expr_o2->anchor_exp);
  }
  
  // Returns whether the two abstract objects denote the same set of concrete objects
  bool ExprObj::equalSet(AbstractObjectPtr o2, PartEdgePtr pedge)
  {
    // Since ExprObj always denote a single concrete MemLocs its set is only equal to that of o
    // if o is also a ExprObj that denotes the same concrete MemLoc
    ExprObjPtr expr_o2 = boost::dynamic_pointer_cast <ExprObj> (o2);
    if(!expr_o2) return false;
    else         return (this -> anchor_exp  == expr_o2->anchor_exp);
  }
  
  // Return the list of this node's ancestors, upto and including the nearest enclosing 
  // statement as well as the node itself, with the deeper ancestors placed towards the front of the list
  list<SgNode*> getAncestorToStmt(SgNode* n) {
    list<SgNode*> ancestors;
    /*Dbg::region reg(1,1, Dbg::region::midLevel, "getAncestorToStmt");
    Dbg::dbg << "n=["<<n->unparseToString()<<" | "<<n->class_name()<<"]"<<endl;
    Dbg::indent ind(1, 1);*/

    SgNode* a = n;
    //Dbg::dbg << "a=["<<a->unparseToString()<<" | "<<a->class_name()<<"]"<<endl;
    while(a!=NULL && !isSgStatement(a)) {
      ancestors.push_front(a);
      a = a->get_parent();
      /*if(a) Dbg::dbg << "#ancestors="<<ancestors.size()<<" a=["<<a->unparseToString()<<" | "<<a->class_name()<<"]"<<endl;
      else  Dbg::dbg << "#ancestors="<<ancestors.size()<<" a=NULL"<<endl;*/
    }
    if(a!=NULL) ancestors.push_front(a);
    return ancestors;
  }
  
  bool enc (SgExpression* anchor_exp, const CFGNode& n) {
    // anchor_expr is in-scope at n if they're inside the same statement or n is an SgIfStmt, SgForStatement, SgWhileStmt 
    // or SgDoWhileStmt and anchor_expr is inside its sub-statements
    return (SageInterface::getEnclosingStatement(n.getNode()) == 
            SageInterface::getEnclosingStatement(anchor_exp)) ||
           (isSgIfStmt(n.getNode()) && 
            isSgIfStmt(n.getNode())->get_conditional()==
            SageInterface::getEnclosingStatement(anchor_exp)) ||
           (isSgWhileStmt(n.getNode()) && 
            isSgWhileStmt(n.getNode())->get_condition()==
            SageInterface::getEnclosingStatement(anchor_exp)) ||
           (isSgDoWhileStmt(n.getNode()) && 
            isSgDoWhileStmt(n.getNode())->get_condition()==
            SageInterface::getEnclosingStatement(anchor_exp)) ||
           (isSgForStatement(n.getNode()) && 
            (isSgForStatement(n.getNode())->get_for_init_stmt()==SageInterface::getEnclosingStatement(anchor_exp) ||
             isSgForStatement(n.getNode())->get_test()         ==SageInterface::getEnclosingStatement(anchor_exp)));
  }
    
  // Returns true if this MemLocObject is in-scope at the given part and false otherwise
  bool ExprObj::isLiveML(PartEdgePtr pedge) 
  {
    //RULE 1: Fails because it doesn't account for the fact that between an operand and its parent
    //        there may be several more nodes from another sub-branch of the expression tree
    // The anchor expression is in scope if it is equal to the current SgNode or is its operand
    //return (anchor_exp==part.getNode() || isOperand(part.getNode(), anchor_exp));
    
    //RULE 2: The expression is in-scope at a Part if they're inside the same statement
    //        This rule is fairly loose but at least it is easy to compute. The right rule
    //        would have been that the part is on some path between the expression and its
    //        parent but this would require an expensive graph search
    /*return SageInterface::getEnclosingStatement(anchor_exp) == 
           SageInterface::getEnclosingStatement(part.getNode());*/    
    //boost::function<bool (SgExpression*, const CFGNode&)> enc1 = &enc;

    // GB 2012-10-18 - I'm not sure what to do here about edges with wildcard sources or targets.
    //                 It seems like to be fully general we need to say that something is live if it is live at
    //                 any source and any destination, meaning that we need consider all the outcomes of a wildcard.
    //                 For example, what happens when an edge may cross a scope boundary for one but not all
    //                 of the wildcard outcomes?
    return (pedge->source() ? pedge->source()->mapCFGNodeANY<bool>(boost::bind(enc, anchor_exp, _1)): false) ||
           (pedge->target() ? pedge->target()->mapCFGNodeANY<bool>(boost::bind(enc, anchor_exp, _1)): false);
    
    /*struct enc { public: bool op(SgExpression* anchor_exp, const CFGNode& n) {
      return SageInterface::getEnclosingStatement(anchor_exp) == 
             SageInterface::getEnclosingStatement(n.getNode());
    } }; enc e;
    return part->mapCFGNodeANY<bool>(boost::bind(&enc::op, anchor_exp, _1));*/
    
    //RULE 3: look for a common ancestor between anchor_exp and part.getNode(). If this ancestor134/dix
    //        is part.getNode(), below part.getNode() or anchor_expr is an operand of part.getNode() (it is
    //        one level above part.getNode()) then it is in-scope.
    // anchor_exp         a     b
    //     |                \ /
    //     c                 d
    //      \------- e -----/
    //               |
    //               f
    // anchor_exp is in-scope at anchor_exp, a, b, c, d but not e or f.
    //Dbg::region reg(1, 1, Dbg::region::midLevel, string("ExprObj::isLive[")+anchor_exp->unparseToString()+string(" | ")+anchor_exp->class_name()+string(">"));
    
    // If part.getNode() is equal to anchor_expr or uses it as an operand, then anchor_exp is in-scope
    /*if(anchor_exp==part.getNode() || isOperand(part.getNode(), anchor_exp)) { //anchor_exp->get_parent()==part.getNode()) {
      //Dbg::dbg << "IN-SCOPE"<<endl;
      return true;
    // Otherwise, anchor_exp is only in-scope if shares an ancestor with part.getNode() but part.getNode() 
    // is not that ancestor.
  } else {
      //Dbg::dbg << "anchor_exp->get_parent()=["<<anchor_exp->get_parent()->unparseToString()<<" | "<<anchor_exp->get_parent()->class_name()<<"]"<<endl;
      //Dbg::dbg << "part.getNode()=["<<part.getNode()->unparseToString()<<" | "<<part.getNode()->class_name()<<"]"<<endl;
      //Dbg::dbg << "isOperand(part.getNode(), anchor_exp)="<<isOperand(part.getNode(), anchor_exp)<<endl;
      // Get the ancestor lists of both nodes
      //Dbg::dbg << "getAncestorToStmt(anchor_exp)"<<endl;
      list<SgNode*> anchorAncestors = getAncestorToStmt(anchor_exp);
      //Dbg::dbg << "#anchorAncestors="<<anchorAncestors.size()<<endl;
      //Dbg::dbg << "getAncestorToStmt(part.getNode())"<<endl;
      list<SgNode*> partAncestors = getAncestorToStmt(part.getNode());
      //Dbg::dbg << "#partAncestors="<<partAncestors.size()<<endl;
      ROSE_ASSERT(isSgStatement(*anchorAncestors.begin()));
      
      // If the roots of the ancestor trees are mismatched, anchor_exp is not in-scope
      if(!isSgStatement(*partAncestors.begin()) || *(anchorAncestors.begin())!=*(partAncestors.begin())) {
        //Dbg::dbg << "OUT-OF-SCOPE partStmt="<<isSgStatement(*partAncestors.begin())<<", sameStmt="<<(*(anchorAncestors.begin())!=*(partAncestors.begin()))<<endl;
        return false;
      }
      
      // Iterate through the ancestor lists from the deepest point to the shallowest, looking for a deviation
      list<SgNode*>::iterator a, p;
      for(a = anchorAncestors.begin(), p = partAncestors.begin(); 
          a!=anchorAncestors.end() && p!=partAncestors.end(); a++, p++) {
        if(*a != *p) break;
      }
      
      // If we stopped at the end of either ancestor list then one of the nodes is an ancestor of the other: not in-scope
      if(a==anchorAncestors.end() || p==partAncestors.end()) {
        //Dbg::dbg << "OUT-OF-SCOPE (anchor end="<<(a==anchorAncestors.end())<<", part end="<<(p==partAncestors.end())<<endl;
        return false;
      }
      
      // Otherwise, if there are more nodes left on both ancestor lists, then anchor_exp is in-scope
      //Dbg::dbg << "IN-SCOPE"<<endl;
      return true;
    }*/
  }
  
  bool ExprObj::isFull(PartEdgePtr pedge)
  {
    // Expressions cannot denote the set of all MemLocs
    return false;
  }
  
  bool ExprObj::isEmpty(PartEdgePtr pedge)
  {
    // If an expression is known, the corresponding StxMemLocObject cannot denote the empty set
    return false;
  }
  
  // Returns the type of the MemLoc this object denotes
  SgType* ExprObj::getType() const
  { return anchor_exp->get_type(); }
 
  //std::string ExprObj::str(const string& indent)
  std::string ExprObj::str(std::string indent) // pretty print for the object
  {
    string rt;
    
    if (anchor_exp!= NULL)
      rt += anchor_exp->class_name()+ ": " + anchor_exp->unparseToString()/* + " @ " + StringUtility::numberToString (anchor_exp)*/;
    else
      rt += "expression: NULL";

    return rt;
  }

  std::string ExprObj::strp(PartEdgePtr pedge, std::string indent) // pretty print for the object
  {
    string rt;
    
    if(!isLiveML(pedge)) return "OUT-OF-SCOPE";
    else                 return str(indent);
  }
  
  //------------------
  /*std::set<SgType*> ScalarExprObj::getType() const
  {
    std::set<SgType*> rt;
    rt.insert (ExprObj::getType());
    return rt;
  }*/

  // -----------------------------
  // ----- Expression object -----
  // -----------------------------
  
  /*bool ScalarExprObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (ExprObj::mayEqualML(o2, pedge));
  } 

  bool ScalarExprObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (ExprObj::mustEqualML(o2, pedge));
  }*/
  
  //std::string ScalarExprObj::str(const string& indent)
  std::string ScalarExprObj::str(std::string indent)  // pretty print for the object
  {
    string rt = /*"<u>ScalarExprObj:str()</u> @" + StringUtility::numberToString(this)+ " "+*/ExprObj::str(indent+"    ");
    return rt;
  }
  
  std::string ScalarExprObj::strp(PartEdgePtr pedge, std::string indent) // pretty print for the object
  { return /*"<u>ScalarExprObj:strp()</u> "+*/ (ExprObj::isLiveML(pedge) ? ExprObj::strp(pedge, indent+"    "): "OUT-OF-SCOPE "+ExprObj::str(indent+"    ")); }
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr ScalarExprObj::copyMLK() const
  { return boost::make_shared<ScalarExprObj>(*this); }
  
  //------------------
  /*std::set<SgType*> FunctionExprObj::getType() const
  {
    std::set<SgType*> rt;
    rt.insert (ExprObj::getType());
    return rt;
  }*/

  /*bool FunctionExprObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
   return (ExprObj::mayEqualML(o2, pedge));
  } 

  bool FunctionExprObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
   return (ExprObj::mustEqualML(o2, pedge));
  }*/

  //std::string FunctionExprObj::str(const string& indent)
  std::string FunctionExprObj::str(std::string indent)  // pretty print for the object  
  {
    string rt = "<u>FunctionExprObj</u> "/*@" + StringUtility::numberToString(this)+ " "*/+ ExprObj::str(indent+"    ");
    return rt;
  }
  std::string FunctionExprObj::strp(PartEdgePtr pedge, std::string indent)  // pretty print for the object
  { return "<u>FunctionExprObj:strp()</u> "+ (ExprObj::isLiveML(pedge) ? ExprObj::strp(pedge, indent+"    "): "OUT-OF-SCOPE "+ExprObj::str(indent+"    ")); }
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr FunctionExprObj::copyMLK() const
  { return boost::make_shared<FunctionExprObj>(*this); }

  //------------------
  /*std::set<SgType*> ArrayExprObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (ExprObj::getType());
    return rt;
  }*/

  /*
  bool ArrayExprObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (ExprObj::mayEqualML(o2, pedge));
  } 

  bool ArrayExprObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (ExprObj::mustEqualML(o2, pedge));
  }*/

  //std::string ArrayExprObj::str(const string& indent)
  std::string ArrayExprObj::str(std::string indent)  // pretty print for the object  
  {
    string rt = "<u>ArrayExprObj</u> "/*@" + StringUtility::numberToString(this)+ " "*/+ ExprObj::str(indent+"    ");
    return rt;
  }
  
  std::string ArrayExprObj::strp(PartEdgePtr pedge, std::string indent)  // pretty print for the object
  { return "<u>ArrayExprObj</u> "+ (ExprObj::isLiveML(pedge) ? ExprObj::str(indent+"    "): "OUT-OF-SCOPE "+ExprObj::str(indent+"    ")); }
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr ArrayExprObj::copyMLK() const
  { return boost::make_shared<ArrayExprObj>(*this); }

  // GB: 2012-08-27: should be implementing the following functions here:
  //                 Array::getElements(), getElements(IndexVectorPtr ai), getNumDims(), getDereference()
  MemLocObjectPtr ArrayExprObj::getElements(PartEdgePtr pedge) { ROSE_ASSERT(false); /*Need to implement based on type*/ };
  MemLocObjectPtr ArrayExprObj::getElements(IndexVectorPtr ai, PartEdgePtr pedge) { ROSE_ASSERT(false); /*Need to implement based on type*/ };
  size_t ArrayExprObj::getNumDims(PartEdgePtr pedge) { ROSE_ASSERT(false); /*Need to implement based on type*/ };
  MemLocObjectPtr ArrayExprObj::getDereference(PartEdgePtr pedge) { ROSE_ASSERT(false); /*Need to implement based on type*/ };
  
  //------------------
  /*std::set<SgType*> PointerExprObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (ExprObj::getType());
    return rt;
  }*/

  MemLocObjectPtr PointerExprObj::getDereference(PartEdgePtr pedge) 
  {
    // simplest type-based implementation
    SgType* t = getType();
    SgPointerType* p_t = isSgPointerType(t);
    assert (p_t != NULL);
    return createAliasedMemLocObject (NULL, p_t->get_base_type(), pedge);
  }

  MemLocObjectPtr PointerExprObj::getElements(PartEdgePtr pedge) // in case it is a pointer to array
  {
    MemLocObjectPtr rt;
    //TODO
    assert (false);
    return rt;
  }

  /*// We are concerned about the PointerExprObj itself, not the mem location it points to!!
  bool PointerExprObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (ExprObj::mayEqualML(o2, pedge));
  } 

  // identical pointers, must equal for now
  bool PointerExprObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (ExprObj::mustEqualML(o2, pedge));
  }*/

  //std::string PointerExprObj::str(const string& indent)
  std::string PointerExprObj::str(std::string indent)  // pretty print for the object  
  {
    string rt = "<u>PointerExprObj</u> "/*@" + StringUtility::numberToString(this)+ " "*/+ ExprObj::str(indent+"    ");
    return rt;
  }
  
  std::string PointerExprObj::strp(PartEdgePtr pedge, std::string indent)  // pretty print for the object
  { return "<u>PointerExprObj</u> "+ (ExprObj::isLiveML(pedge) ? ExprObj::str(indent+"    "): "OUT-OF-SCOPE "+ExprObj::str(indent+"    ")); }
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr PointerExprObj::copyMLK() const
  { return boost::make_shared<PointerExprObj>(*this); }

  //---------------------
  LabeledAggregateExprObj::LabeledAggregateExprObj(SgExpression* e, PartEdgePtr pedge): 
    MemLocObject(e), LabeledAggregate_Impl(e), ExprObj (e) 
  {
      // init(e, t, pedge);  //#SA init() should be stripped to avoid double deletion when using shared_ptr
  }
  
  LabeledAggregateExprObj::LabeledAggregateExprObj(const LabeledAggregateExprObj& that):
    MemLocObject((const MemLocObject &)that), LabeledAggregate_Impl((const LabeledAggregate_Impl&) that), ExprObj(that.anchor_exp)
  {
    //init(that.anchor_exp, that.type);
    assert (that.anchor_exp != NULL);
    assert (getType() == that.getType());
    
    elements = that.elements;
  }
  
  void LabeledAggregateExprObj::init(SgExpression* e, PartEdgePtr pedge)
  {
    assert (e != NULL);
    SgClassType * c_t = isSgClassType(e->get_type()->findBaseType());
    assert (c_t != NULL);
    fillUpElements(boost::dynamic_pointer_cast<LabeledAggregate>(shared_from_this()), LabeledAggregate_Impl::getElements(pedge), c_t, pedge);
  }
  
  /*std::set<SgType*> LabeledAggregateExprObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (ExprObj::getType());
    return rt;
  }*/
  
  /*bool LabeledAggregateExprObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (ExprObj::mayEqualML(o2, pedge));
  } 

  bool LabeledAggregateExprObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (ExprObj::mustEqualML(o2, pedge));
  }*/

  //std::string LabeledAggregateExprObj::str(const string& indent)
  std::string LabeledAggregateExprObj::str(std::string indent) // pretty print for the object  
  {
    std::string rt = "<u>LabeledAggregateExprObj</u> "/*@ " + StringUtility::numberToString (this)*/;
    rt += " "+ ExprObj::str(indent+"    ");
    //rt += "   with " + StringUtility::numberToString(fieldCount()) + " fields:\n";
    rt += indent;
    /*for (size_t i =0; i< fieldCount(); i++)
    {
      rt += "&nbsp;&nbsp;&nbsp;&nbsp;" + (getElements())[i]->str(indent+"    ") + "\n";
    }*/
    return rt; 
  }
  
  std::string LabeledAggregateExprObj::strp(PartEdgePtr pedge, std::string indent) // pretty print for the object  
  {
    std::string rt = "<u>LabeledAggregateExprObj</u>";
    if(ExprObj::isLiveML(pedge)) {
      rt += " "+ ExprObj::str(indent+"    ");
      rt += "   with " + StringUtility::numberToString(fieldCount(pedge)) + " fields:\n";
      rt += indent;
      list<LabeledAggregateFieldPtr> elements = getElements(pedge);
      int i=0;
      for(list<LabeledAggregateFieldPtr>::iterator e=elements.begin(); e!=elements.end(); e++, i++)
        rt += "&nbsp;&nbsp;&nbsp;&nbsp;" + (*e)->strp(pedge, indent+"    ") + "\n";
    } else {
      rt += "OUT-OF-SCOPE";
    }
    return rt; 
  }
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr LabeledAggregateExprObj::copyMLK() const
  { return boost::make_shared<LabeledAggregateExprObj>(*this); }
  
  // --------------------- Named Object --------------------

  bool NamedObj::mayEqualML(NamedObjPtr o2, PartEdgePtr pedge)
  {
    bool rt = false;
        
    /*Dbg::indent ind(1, 1);
    Dbg::dbg << "o2="<<(o2? o2->str() : "NULL")<<endl;*/
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(anchor_symbol == o2->anchor_symbol)="<<(anchor_symbol == o2->anchor_symbol)<<endl;*/
    
    if (anchor_symbol == o2->anchor_symbol) { // same symbol
/*      Dbg::dbg << "(!parent && !o2->parent)="<<(!parent && !o2->parent)<<endl;
      Dbg::dbg << "(parent && o2->parent)="<<(parent && o2->parent)<<endl;
      if(parent) Dbg::dbg << "parent="<<parent->str()<<endl;
      if(o2->parent) Dbg::dbg << "o2->parent="<<parent->str()<<endl;
      if(parent && o2->parent) Dbg::dbg << "parent->mayEqual(o2->parent, p)="<<parent->mayEqual(o2->parent, pedge)<<endl;*/
      
      // GB: Do we need to be more relaxed with mayEqual?
      if((!parent && !o2->parent) || 
        (parent && o2->parent && parent->mayEqualML(o2->parent, pedge)))   // same parent
        {
//          Dbg::dbg << "(array_index_vector && o2->array_index_vector)="<<(array_index_vector && o2->array_index_vector)<<endl;
//          Dbg::dbg << "dynamic_cast<ArrayNamedObj*>(this)="<<dynamic_cast<ArrayNamedObj*>(this)<<" dynamic_cast<ArrayNamedObj*>(o2.get())="<<dynamic_cast<ArrayNamedObj*>(o2.get())<<endl;
          if(array_index_vector && o2->array_index_vector)
          {
            // same array index, must use *pointer == *pointer to get the right comparison!!
            //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;array_index_vector->mayEqual(o2->array_index_vector, p)="<<array_index_vector->mayEqual(o2->array_index_vector, p)<<endl;
            if(array_index_vector->mayEqual(o2->array_index_vector, pedge, SyntacticAnalysis::instance()->getComposer(), SyntacticAnalysis::instance()))
              rt = true; // semantically equivalent index vectors
          }
          // Array objects denote an array's entire contents. As such, array objects mayEqual to any element in them
          else if((array_index_vector     && dynamic_cast<ArrayNamedObj*>(o2.get())) || 
                  (o2->array_index_vector && dynamic_cast<ArrayNamedObj*>(this))) {
            return true;
          }
          else {
            //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(!array_index_vector && !o2->array_index_vector)="<<(!array_index_vector && !o2->array_index_vector)<<endl;
            if (!array_index_vector && !o2->array_index_vector) // both are NULL
              rt = true;
          }
        }
    }
    return rt;
  }

  bool NamedObj::mustEqualML(NamedObjPtr o2, PartEdgePtr pedge)
  {
    bool rt = false;
    /*Dbg::dbg << "NamedObj::mustEqualML this="<<str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;o2="<<(o2? o2->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;") : "NULL")<<endl;
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(anchor_symbol == o2->anchor_symbol)="<<(anchor_symbol == o2->anchor_symbol)<<endl;*/
    
    if (anchor_symbol == o2->anchor_symbol) { // same symbol
      /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(!parent && !o2->parent)="<<(!parent && !o2->parent)<<endl;
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(parent && o2->parent)="<<(parent && o2->parent)<<endl;
      if(parent && o2->parent) Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;parent->mustEqual(o2->parent, pedge)="<<parent->mustEqualML(o2->parent, pedge)<<endl;*/
      
      if((!parent && !o2->parent) || 
         (parent && o2->parent && parent->mustEqualML(o2->parent, pedge)))   // same parent
      {
        //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(array_index_vector && o2->array_index_vector)="<<(array_index_vector && o2->array_index_vector)<<endl;
        if(array_index_vector && o2->array_index_vector)
        {
          //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;array_index_vector->mustEqual(o2->array_index_vector, pedge)="<<array_index_vector->mayEqual(o2->array_index_vector, pedge, SyntacticAnalysis::instance()->getComposer(), SyntacticAnalysis::instance())<<endl;
          
          // same array index, must use *pointer == *pointer to get the right comparison!!
          if (array_index_vector->mustEqual(o2->array_index_vector, pedge, SyntacticAnalysis::instance()->getComposer(), SyntacticAnalysis::instance()))
            rt = true; // semantically equivalent index vectors
        }
        else {
          //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(!array_index_vector && !o2->array_index_vector)="<<(!array_index_vector && !o2->array_index_vector)<<endl;
          if(!array_index_vector && !o2->array_index_vector) // both are NULL
            rt = true;
        }
      }
    }
    return rt;
  }

  bool NamedObj::mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    // three cases:
    AliasedObjPtr aliased_o2 = boost::dynamic_pointer_cast <AliasedObj> (o2);
    if(aliased_o2) {
        // case 1:
      return isAliased(getType(), aliased_o2->getType());
    } else { 
      NamedObjPtr named_o2 = boost::dynamic_pointer_cast <NamedObj> (o2);
      if(named_o2) {
        // case 2:
        return NamedObj::mayEqualML(named_o2, pedge);
      } else {
        //case 3:
        // Only Expression Obj is left, always return false 
        return false;
      }
    }
  }

  bool NamedObj::mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    // three cases:
    AliasedObjPtr aliased_o2 = boost::dynamic_pointer_cast <AliasedObj> (o2);
    if(aliased_o2) {
        // case 1:
      //Dbg::dbg << "NamedObj::mustEqualML o2="<<o2->str("")<<", o2.get()"<<o2.get()<<", aliased_o2.get()="<<aliased_o2.get()<<endl;
      ROSE_ASSERT(aliased_o2 || !aliased_o2); // Using aliased_o2 to avoid a warning.
      return false; //TODO accurate alias analysis can answer this question better. For now, we cannot decide. 
    }
    else { 
        // case 2:
      NamedObjPtr named_o2 = boost::dynamic_pointer_cast <NamedObj> (o2);
      if(named_o2) {
        return NamedObj::mustEqualML(named_o2, pedge);
      } else {
        //case 3:
        // Only Expression Obj is left, always return false 
        return false;
      }
    }
  }
  
  // Returns whether the two abstract objects denote the same set of concrete objects
  bool NamedObj::equalSet(AbstractObjectPtr o2, PartEdgePtr pedge)
  {
    // Since NamedObjects always denote a single concrete MemLocs its set is only equal to that of o
    // if o is also a NamedObject that denotes the same concrete MemLoc
    NamedObjPtr named_o2 = boost::dynamic_pointer_cast <NamedObj> (o2);
    if(!named_o2) return false;

    bool rt = false;
    /*Dbg::dbg << "NamedObj::equalSet this="<<str()<<endl;
    Dbg::dbg << "named_o2="<<(named_o2? named_o2->str() : "NULL")<<endl;*/
    //Dbg::dbg << "(anchor_symbol == named_o2->anchor_symbol)="<<(anchor_symbol == named_o2->anchor_symbol)<<endl;*/
    
    if(anchor_symbol == named_o2->anchor_symbol) { // same symbol
      /*Dbg::indent(1,1);
      Dbg::dbg << "(!parent && !named_o2->parent)="<<(!parent && !named_o2->parent)<<endl;
      Dbg::dbg << "(parent && named_o2->parent)="<<(parent && named_o2->parent)<<endl;
      if(parent && named_o2->parent) Dbg::dbg << "parent->equalSet(named_o2->parent, p)="<<parent->equalSet(named_o2->parent, pedge)<<endl;*/
      if((!parent && !named_o2->parent) || 
         (parent && named_o2->parent && parent->equalSet(named_o2->parent, pedge)))   // same parent
      {
        /*Dbg::indent(1,1);
        Dbg::dbg << "array_index_vector="<<array_index_vector<<", named_o2->array_index_vector="<<named_o2->array_index_vector<<endl;*/
        if(array_index_vector && named_o2->array_index_vector)
        {
          /*Dbg::dbg << "array_index_vector="<<array_index_vector->str()<<", named_o2->array_index_vector="<<named_o2->array_index_vector->str()<<endl;
          Dbg::dbg << "array_index_vector->equalSet(named_o2->array_index_vector, p)="<<array_index_vector->equalSet(named_o2->array_index_vector, pedge, SyntacticAnalysis::instance()->getComposer(), SyntacticAnalysis::instance())<<endl;*/
          
          // same array index, must use *pointer == *pointer to get the right comparison!!
          if (array_index_vector->equalSet(named_o2->array_index_vector, pedge, SyntacticAnalysis::instance()->getComposer(), SyntacticAnalysis::instance()))
            rt = true; // semantically equivalent index vectors
        }
        else {
          //Dbg::dbg << "(array_index_vector && !named_o2->array_index_vector)="<<(!array_index_vector && !named_o2->array_index_vector)<<endl;
          if(!array_index_vector && !named_o2->array_index_vector) // both are NULL
            rt = true;
        }
      }
    }
    return rt;
  }
  
  // Return whether there exists a CFGNode within this part that is inside the function in which the anchor symbol
  // is defined.
  bool matchAnchorPart(SgScopeStatement* anchor_scope, const CFGNode& n) {
      SgScopeStatement* part_scope = SageInterface::getScope(n.getNode());
      ROSE_ASSERT(part_scope);
      if(anchor_scope == part_scope) 
          return true;
      else
          return SageInterface::isAncestor(anchor_scope, part_scope);
  }
  
  // Returns true if this MemLocObject is in-scope at the given part and false otherwise
  bool NamedObj::isLiveML(PartEdgePtr pedge)
  {
      return true;
    // This variable is in-scope if part.getNode() is inside the scope that contains its declaration
    SgScopeStatement* anchor_scope;
    ROSE_ASSERT(isSgVariableSymbol(anchor_symbol) || isSgFunctionSymbol(anchor_symbol));
    if(isSgVariableSymbol(anchor_symbol))
      anchor_scope = isSgVariableSymbol(anchor_symbol)->get_declaration()->get_declaration()->get_scope();
    else if(isSgFunctionSymbol(anchor_symbol))
      anchor_scope = isSgFunctionSymbol(anchor_symbol)->get_declaration()->get_scope();

    ROSE_ASSERT(anchor_scope);
      
    if(isSgFunctionSymbol(anchor_symbol)) return true;
    else if(isSgVariableSymbol(anchor_symbol)) {
      //Dbg::dbg << "anchor_symbol="<<cfgUtils::SgNode2Str(anchor_symbol)<<" pedge="<<pedge->str()<<endl;
      // GB 2012-10-18 - I'm not sure what to do here about edges with wildcard sources or targets.
      //                 It seems like to be fully general we need to say that something is live if it is live at
      //                 any source and any destination, meaning that we need consider all the outcomes of a wildcard.
      //                 For example, what happens when an edge may cross a scope boundary for one but not all
      //                 of the wildcard outcomes?
      return (pedge->source() ? pedge->source()->mapCFGNodeANY<bool>(boost::bind(&matchAnchorPart, anchor_scope, _1)) : false) ||
             (pedge->target() ? pedge->target()->mapCFGNodeANY<bool>(boost::bind(&matchAnchorPart, anchor_scope, _1)) : false);
    } else
      return false;
 
    /*Dbg::region reg(1,1, Dbg::region::midLevel, string("NamedObj::isLiveML(")+anchor_symbol->get_name().getString()+string(")")+string(isSgFunctionSymbol(anchor_symbol) || (isSgVariableSymbol(anchor_symbol) && (anchorFD == partFD)) ?  "IN-SCOPE" : "OUT-OF-SCOPE"));
    Dbg::dbg << "anchorFD=";
    if(anchorFD) Dbg::dbg << "["<<anchorFD->unparseToString()<<" | "<<anchorFD->class_name()<<"]"<<endl;
    else         Dbg::dbg << "SgFunctionSymbol"<<endl;
    Dbg::dbg << "partFD=["<<partFD->unparseToString()<<" | "<<partFD->class_name()<<"]"<<endl;
    Dbg::dbg << "part=["<<part.getNode()->unparseToString()<<" | "<<part.getNode()->class_name()<<"]"<<endl;*/
  }
  
  bool NamedObj::isFull(PartEdgePtr pedge)
  {
    // Named objects cannot denote the set of all MemLocs since they do not denote heap memory or expressions
    return false;
  }
  
  bool NamedObj::isEmpty(PartEdgePtr pedge)
  {
    // If a named object is known, the corresponding StxMemLocObject cannot denote the empty set
    return false;
  }
  
  // Returns the type of the MemLoc this object denotes
  SgType* NamedObj::getType() const
  { return anchor_symbol->get_type(); }
  
  //std::string IndexVector_Impl::str(const string& indent)
  std::string IndexVector_Impl::str(std::string indent) // pretty print for the object  
  {
    string rt;
    std::vector<ValueObjectPtr>::const_iterator iter;
    for (iter = index_vector.begin(); iter != index_vector.end(); iter++)
    {
      ValueObjectPtr current_index_field = *iter;
      rt += current_index_field->str(indent+"    ");
      if(iter != index_vector.begin()) rt += ", ";
    }
    return rt;
  }
  
  // Allocates a copy of this object and returns a pointer to it
  IndexVectorPtr IndexVector_Impl::copyIV() const
  {
    IndexVector_ImplPtr newIV = boost::make_shared<IndexVector_Impl>();
    for (std::vector<ValueObjectPtr>::const_iterator iter = index_vector.begin(); iter != index_vector.end(); iter++)
      newIV->index_vector.push_back((*iter)->copyV());
    return newIV;
  }

  bool IndexVector_Impl::mayEqual(IndexVectorPtr other, PartEdgePtr pedge, Composer* comp, ComposedAnalysis* analysis)
  {
    IndexVector_ImplPtr other_impl = boost::dynamic_pointer_cast<IndexVector_Impl>(other);
    // If other is not of a compatible type
    if(!other_impl) {
      // Cannot be sure that objects are not equal, so conservatively state they may be equal
      return true;
    }
    bool rt = false;

    bool has_diff_element = false;
    if (this->getSize(pedge) == other_impl->getSize(pedge)) 
    { // same size, no different element
      for (size_t i =0; i< other_impl->getSize(pedge); i++)
      {
        if (!(this->index_vector[i]->mayEqualV(other_impl->index_vector[i], pedge)))
        {
          has_diff_element = true;
            break;
        }
      }
      if (!has_diff_element )
        rt = true;
    }
    
    return rt;
  }

  bool IndexVector_Impl::mustEqual(IndexVectorPtr other, PartEdgePtr pedge, Composer* comp, ComposedAnalysis* analysis)
  {
    IndexVector_ImplPtr other_impl = boost::dynamic_pointer_cast<IndexVector_Impl>(other);
    // If other is not of a compatible type
    if(!other_impl) {
      // Cannot be sure that objects must be equal, so conservatively don't claim this
      return false;
    }
    bool rt = false;
      
    bool has_diff_element = false;
    if (this->getSize(pedge) == other_impl->getSize(pedge)) 
    { // same size, no different element
      for (size_t i =0; i< other_impl->getSize(pedge); i++)
      {
        if (!(this->index_vector[i]->mustEqualV(other_impl->index_vector[i], pedge)))
        {
          has_diff_element = true;
            break;
        }
      }
      if (!has_diff_element )
        rt = true;
    }
    
    return rt;
  }
  
  bool IndexVector_Impl::equalSet(IndexVectorPtr other, PartEdgePtr pedge, Composer* comp, ComposedAnalysis* analysis)
  {
    IndexVector_ImplPtr other_impl = boost::dynamic_pointer_cast<IndexVector_Impl>(other);
    // If other is not of a compatible type, cannot be sure that objects denote the same set, so conservatively don't claim this
    if(!other_impl) return false;
      
    if (this->getSize(pedge) != other_impl->getSize(pedge)) return false;
    
    for (size_t i =0; i< other_impl->getSize(pedge); i++) {
      if (!(this->index_vector[i]->equalSet(other_impl->index_vector[i], pedge))) return false;
    }
    return true;
  }
  
  bool IndexVector_Impl::meetUpdate(IndexVectorPtr other, PartEdgePtr pedge, Composer* comp, ComposedAnalysis* analysis)
  {
    IndexVector_ImplPtr other_impl = boost::dynamic_pointer_cast<IndexVector_Impl>(other);
    ROSE_ASSERT(other_impl);
    ROSE_ASSERT(index_vector.size() == other_impl->index_vector.size());
    
    bool modified = false;
    vector<ValueObjectPtr>::iterator thisI = index_vector.begin();
    vector<ValueObjectPtr>::iterator otherI = other_impl->index_vector.begin();
    
    for(; thisI!=index_vector.end(); thisI++, otherI++)
      modified = (*thisI)->meetUpdateV(*otherI, pedge) || modified;
    return modified;
  }
  
  bool IndexVector_Impl::isFull(PartEdgePtr pedge, Composer* comp, ComposedAnalysis* analysis)
  {
    // Return false if any sub-index is not full
    for(vector<ValueObjectPtr>::iterator i=index_vector.begin(); i!=index_vector.end(); i++)
      if(!(*i)->isFull(pedge)) return false;
    // Return true if all sub-indexes are full
    return true;
  }
  
  bool IndexVector_Impl::isEmpty(PartEdgePtr pedge, Composer* comp, ComposedAnalysis* analysis)
  {
    // Return false if any sub-index is not empty
    for(vector<ValueObjectPtr>::iterator i=index_vector.begin(); i!=index_vector.end(); i++)
      if(!(*i)->isEmpty(pedge)) return false;
    // Return true if all sub-indexes are empty
    return true;
  }
  
  std::string NamedObj::str(std::string indent)// pretty print for the object  
  {
    string rt;

    if (anchor_symbol != NULL)
      rt += "symbol: " + anchor_symbol->get_name().getString()/* + " @ " + StringUtility::numberToString (anchor_symbol)*/;
    /*else
      rt += "symbol: NULL";*/

    if (parent)
      rt += "  parent: " + parent->str();//StringUtility::numberToString(parent.get()); // use address is sufficient
    /*else
      rt += "  parent: NULL";*/

    if (array_index_vector != NULL)
      rt += "  array_index_vector: "/*@ " + StringUtility::numberToString(array_index_vector.get()) */+ array_index_vector->str(indent+"    "); // use address is sufficient
    /*else
      rt += "  array_index_vector: NULL";*/

    return rt;
  }

  std::string NamedObj::strp(PartEdgePtr pedge, std::string indent) // pretty print for the object
  {
    string rt;
    
    if(!isLiveML(pedge)) return "OUT-OF-SCOPE";
    else              return str(indent);
  }


  //------------------
  /*std::set<SgType*> ScalarNamedObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (NamedObj::getType());
    return rt;
  }*/

  /*bool ScalarNamedObj::mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (NamedObj::mayEqualML(o2, pedge));
  } 

  bool ScalarNamedObj::mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (NamedObj::mustEqualML(o2, pedge));
  }*/

  //std::string ScalarNamedObj::str(const string& indent)
  std::string ScalarNamedObj::str(std::string indent) // pretty print for the object  
  {
    string rt = "<u>ScalarNamedObj</u> "/*@" + StringUtility::numberToString(this)+ " "*/+ NamedObj::str(indent);
    return rt;
  }

  std::string ScalarNamedObj::strp(PartEdgePtr pedge, std::string indent) // pretty print for the object
  { return "<u>ScalarNamedObj</u> "+ (NamedObj::isLiveML(pedge) ? NamedObj::str(indent+"    "): "OUT-OF-SCOPE "+NamedObj::str(indent+"    ")); }
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr ScalarNamedObj::copyMLK() const
  { return boost::make_shared<ScalarNamedObj>(*this); }

  //------------------
  /*std::set<SgType*> FunctionNamedObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (NamedObj::getType());
    return rt;
  }*/

  // This is a confusing part:  operator == of AbstractObject side is implemented through the operator== () of the NamedObj
  /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
  bool FunctionNamedObj::operator == (const MemLocObject& o2) const
  {
    const NamedObj& o1 = dynamic_cast<const NamedObj&> (*this);
    return (o1==o2);
  } */

  /*bool FunctionNamedObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (NamedObj::mayEqualML(o2, pedge));
  } 

  bool FunctionNamedObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (NamedObj::mustEqualML(o2, pedge));
  }*/

  //std::string FunctionNamedObj::str(const string& indent)
  std::string FunctionNamedObj::str(std::string indent) // pretty print for the object  
  {
    string rt = "<u>FunctionNamedObj</u> @" + StringUtility::numberToString(this)+ " "+ NamedObj::str(indent);
    return rt;
  }
  
  std::string FunctionNamedObj::strp(PartEdgePtr pedge, std::string indent) // pretty print for the object
  { return "<u>FunctionNamedObj</u> "+ (NamedObj::isLiveML(pedge) ? NamedObj::str(indent+"    "): "OUT-OF-SCOPE "+NamedObj::str(indent+"    ")); }
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr FunctionNamedObj::copyMLK() const
  { return boost::make_shared<FunctionNamedObj>(*this); }

  //------------------
  /*std::set<SgType*> PointerNamedObj::getType() const
  {
    std::set<SgType*> rt;
    rt.insert (NamedObj::getType());
    return rt;
  }*/

  MemLocObjectPtr PointerNamedObj::getDereference(PartEdgePtr pedge) 
  {
    // simplest type-based implementation
    SgType* t = getType();
    SgPointerType* p_t = isSgPointerType(t);
    assert (p_t != NULL);
    return createAliasedMemLocObject (NULL, p_t->get_base_type(), pedge);
  }

  MemLocObjectPtr PointerNamedObj::getElements(PartEdgePtr pedge) // in case it is a pointer to array
  {
    MemLocObjectPtr rt;
    //TODO
    assert (false);

    return rt;
    
  }

  /*bool PointerNamedObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (NamedObj::mayEqualML(o2, pedge));
  } 

  bool PointerNamedObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (NamedObj::mustEqualML(o2, pedge));
  }*/

  //std::string PointerNamedObj::str(const string& indent)
  std::string PointerNamedObj::str(std::string indent) // pretty print for the object  
  {
    string rt = "<u>PointerNamedObj</u> "/*@" + StringUtility::numberToString(this)+ " "*/+ NamedObj::str(indent);
    return rt;
  }
  
  std::string PointerNamedObj::strp(PartEdgePtr pedge, std::string indent) // pretty print for the object
  { return "<u>PointerNamedObj</u> "+ (NamedObj::isLiveML(pedge) ? NamedObj::str(indent+"    "): "OUT-OF-SCOPE "+NamedObj::str(indent+"    ")); }
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr PointerNamedObj::copyMLK() const
  { return boost::make_shared<PointerNamedObj>(*this); }

  
  
  // a helper function to fill up std::vector<LabeledAggregateField*>  from a class/structure type
  // TODO handle static members,they should be treated as global variables , not instances
  void fillUpElements(MemLocObjectPtr p, std::list<LabeledAggregateFieldPtr > & elements, SgClassType* c_t, PartEdgePtr pedge)
  {
    ROSE_ASSERT(p != NULL);
    LabeledAggregatePtr lp = boost::dynamic_pointer_cast<LabeledAggregate>(p);
    ROSE_ASSERT(lp != NULL);
    assert (c_t != NULL);
    SgDeclarationStatement * decl = c_t->get_declaration();
    assert (decl != NULL);
    SgClassDeclaration* c_decl = isSgClassDeclaration(decl);
    assert (c_decl != NULL);
    SgClassDeclaration* def_decl = isSgClassDeclaration(c_decl->get_definingDeclaration()); 
    if (def_decl != NULL )
    {   
      SgClassDefinition * c_def = def_decl->get_definition();
      assert (c_def != NULL);
      // get members and insert LabeledAggregateField_Impl
      SgDeclarationStatementPtrList stmt_list = c_def->get_members();
      SgDeclarationStatementPtrList::iterator iter;
      for (iter = stmt_list.begin(); iter != stmt_list.end(); iter ++)
      {
        SgDeclarationStatement * decl_stmt = *iter;
        SgVariableDeclaration * var_decl = isSgVariableDeclaration (decl_stmt);
        if (var_decl)
        {
          SgVariableSymbol * s = SageInterface::getFirstVarSym(var_decl);
          MemLocObjectPtr field_obj = createNamedMemLocObject(NULL, s, s->get_type(), pedge, lp, IndexVectorPtr()); // we don't store explicit index  for elements for now
          boost::shared_ptr<LabeledAggregateField_Impl> f(new LabeledAggregateField_Impl (field_obj, lp));
          elements.push_back(f);
        }  
      }
    }
  }

  //----------------------
  LabeledAggregateNamedObj::LabeledAggregateNamedObj(SgNode* n, SgSymbol* s, MemLocObjectPtr p, IndexVectorPtr iv, PartEdgePtr pedge): 
    MemLocObject(n), LabeledAggregate_Impl(n), NamedObj(n, s, p, iv)
  {
      // init(s, t, p, iv, pedge); //#SA init should be stripped to avoid double deletion when using shared_ptr
  }
  
  LabeledAggregateNamedObj::LabeledAggregateNamedObj(const LabeledAggregateNamedObj& that): 
    MemLocObject((const MemLocObject &)that), LabeledAggregate_Impl(that), NamedObj((const NamedObj&)that)
  {
    assert (that.anchor_symbol != NULL);
    
    //init(that.anchor_symbol, that.type, that.parent, that.array_index_vector);
    elements = that.elements;
  }
  
  void LabeledAggregateNamedObj::init(SgSymbol* s, MemLocObjectPtr p, IndexVectorPtr iv, PartEdgePtr pedge)
  {
    assert (s != NULL);
    
    // typedef objects are created with base types
    SgClassType * c_t;
    if(isSgTypedefType(s->get_type()->findBaseType()))
      c_t = isSgClassType((s->get_type())->findBaseType());
    else      
      c_t = isSgClassType(s->get_type()->findBaseType());
    
    fillUpElements(boost::dynamic_pointer_cast<LabeledAggregate>(shared_from_this()), LabeledAggregate_Impl::getElements(pedge), c_t, pedge);
  }

  /*std::set<SgType*> LabeledAggregateNamedObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (NamedObj::getType());
    return rt;
  }*/

  //std::string LabeledAggregateNamedObj::str(const string& indent)
  std::string LabeledAggregateNamedObj::str(std::string indent) // pretty print for the object  
  {
    std::string rt = "<u>LabeledAggregateNamedObj</u> "/*@ " + StringUtility::numberToString (this)*/;
    rt += " "+ NamedObj::str(indent);
    //rt += "   with " + StringUtility::numberToString(fieldCount()) + " fields:\n";
    /*for (size_t i =0; i< fieldCount(); i++)
    {
      rt += indent + "&nbsp;&nbsp;&nbsp;&nbsp;" + (getElements())[i]->str(indent+"    ") + "\n";
    }*/
    return rt; 
  }
  
  std::string LabeledAggregateNamedObj::strp(PartEdgePtr pedge, std::string indent) // pretty print for the object  
  {
    std::string rt = "<u>LabeledAggregateNamedObj</u>";
    if(NamedObj::isLiveML(pedge)) {
      rt += " "+ NamedObj::str(indent);
      rt += "   with " + StringUtility::numberToString(fieldCount(pedge)) + " fields:\n";
      list<LabeledAggregateFieldPtr> elements = getElements(pedge);
      int i=0;
      for(list<LabeledAggregateFieldPtr>::iterator e=elements.begin(); e!=elements.end(); e++, i++)
        rt += indent + "&nbsp;&nbsp;&nbsp;&nbsp;" + (*e)->strp(pedge, indent+"    ") + "\n";
    } else {
      rt += "OUT-OF-SCOPE";
    }
    return rt; 
  }
  
  /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
  bool LabeledAggregateNamedObj::operator == (const MemLocObject& o2) const
  { 
    const NamedObj& o1 = dynamic_cast<const NamedObj&> (*this);
    return (o1==o2);
  } */

  /*bool LabeledAggregateNamedObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  { 
    return (NamedObj::mayEqualML(o2, pedge));
  }

  bool LabeledAggregateNamedObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  { 
    return (NamedObj::mustEqualML(o2, pedge));
  }*/
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr LabeledAggregateNamedObj::copyMLK() const
  { return boost::make_shared<LabeledAggregateNamedObj>(*this); }
  
  //---------------------
  ArrayNamedObj::ArrayNamedObj(SgNode* n, SgSymbol* s, MemLocObjectPtr p, IndexVectorPtr iv, PartEdgePtr pedge): 
    MemLocObject(n), Array_Impl(n), NamedObj (n, s, p, iv)
  {
    init(s,p,iv);
  }
    
  ArrayNamedObj::ArrayNamedObj(const ArrayNamedObj& that): 
     MemLocObject((const MemLocObject &)that), Array_Impl(that), NamedObj((const NamedObj&)that)
  {
    init(that.anchor_symbol, that.parent, that.array_index_vector);
  }
  
  void ArrayNamedObj::init(SgSymbol* s, MemLocObjectPtr p, IndexVectorPtr iv)
  {
    assert (s != NULL);

    assert (isSgVariableSymbol (s) != NULL);
    // typedef objects are created with base types
    SgArrayType * a_t;
    if(isSgTypedefType(s->get_type()))
      a_t = isSgArrayType((s->get_type())->findBaseType());
    else      
      a_t = isSgArrayType(s->get_type());
    
    assert (a_t != NULL);
  }

  /*std::set<SgType*> ArrayNamedObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (NamedObj::getType());
    return rt;
  }*/

  size_t ArrayNamedObj::getNumDims(PartEdgePtr pedge) const
  {
    SgType * a_type = getType();
    assert (a_type != NULL);
    assert (isSgArrayType(a_type) != NULL);
    return SageInterface::getDimensionCount (a_type);
  }

  //std::string ArrayNamedObj::str(const string& indent)
  std::string ArrayNamedObj::str(std::string indent) // pretty print for the object  
  {
    std::string rt = "<u>ArrayNamedObj</u> "/*@ " + StringUtility::numberToString (this)*/;
    rt += " "+ NamedObj::str(indent);
    //rt += "   with " + StringUtility::numberToString(getNumDims(part)) + " dimensions";
/*     for (size_t i =0; i< fieldCount(); i++)
     {
       rt += "&nbsp;&nbsp;&nbsp;&nbsp;" + (getElements())[i]->str(indent+"    ") + "\n";
     }
*/
    return rt; 
  }

  std::string ArrayNamedObj::strp(PartEdgePtr pedge, std::string indent) // pretty print for the object  
  {
    std::string rt = "<u>ArrayNamedObj</u>";
    if(NamedObj::isLiveML(pedge)) {
      rt += " "+ NamedObj::str(indent);
      rt += "   with " + StringUtility::numberToString(getNumDims(pedge)) + " dimensions";
   } else {
      rt += "OUT-OF-SCOPE";
    }
    return rt; 
  }


  // Returns the memory object that corresponds to the elements described by the given abstract index, 
  MemLocObjectPtr ArrayNamedObj::getElements(IndexVectorPtr ai, PartEdgePtr pedge) 
  {
    MemLocObjectPtr mem_obj;
    
    SgVariableSymbol* s = isSgVariableSymbol(this->getSymbol());
    assert (s != NULL);
    assert (ai != NULL);
    assert (isSgArrayType(s->get_type()) != NULL);
    SgType* element_type = SageInterface::getArrayElementType (s->get_type());
    assert (element_type != NULL);

    return createNamedMemLocObject(NULL, s, element_type, pedge, 
                                   boost::dynamic_pointer_cast<MemLocObject>(shared_from_this()), ai);
  }

  MemLocObjectPtr ArrayNamedObj::getDereference(PartEdgePtr pedge) 
  {
    // return array[0][*]..[*]
    IndexVector_ImplPtr myindexv = boost::make_shared<IndexVector_Impl>();
    /*GB: Deprecating IndexSets and replacing them with ValueObjects.
    myindexv ->index_vector.push_back(ConstIndexSet::get_inst((size_t)0));*/
    myindexv->index_vector.push_back(boost::make_shared<StxValueObject>(SageBuilder::buildIntVal(0)));
    
    // we use unknown index to represent the ALL element concept of a dimension
    // TODO, we may want to generate an Array object which is the N-1 dimension array type to be accurate.
    for (size_t i =0; i< getNumDims(pedge) -1; i++)
    {
      /*GB: Deprecating IndexSets and replacing them with ValueObjects.
      myindexv ->index_vector.push_back(UnknownIndexSet::get_inst());*/
      myindexv->index_vector.push_back(boost::make_shared<StxValueObject>((SgNode*)NULL));
    }

    return getElements(myindexv, pedge);
  }
  //use the [Named|Expr|Aliased]Obj side of 
  /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
  bool ArrayNamedObj::operator == (const MemLocObject& o2) const
  { 
   const NamedObj& o1 = dynamic_cast<const NamedObj&> (*this);
    return (o1==o2);
  } */
  
  /*bool ArrayNamedObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (NamedObj::mayEqualML(o2, pedge));
  } 
  bool ArrayNamedObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  { 
    return (NamedObj::mustEqualML(o2, pedge));
  }*/
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr ArrayNamedObj::copyMLK() const
  { return boost::make_shared<ArrayNamedObj>(*this); }

  // --------------------- Aliased Object --------------------
  //std::string AliasedObj::str(const string& indent)
  std::string AliasedObj::str(std::string indent) // pretty print for the object  
  {
    string rt;
    if (type != NULL )
      rt += type->unparseToString()/* + " @ " + StringUtility::numberToString(type)*/;
    return rt;
  }

  // Simplest alias analysis: same type ==> aliased
  bool isAliased (const SgType *t1, const SgType* t2) 
  {
    // TODO : consider subtype, if type1 is a subtype of type2, they are aliased to each other also
    // TODO : consider relationship between pointer and reference types (should be equivalent)
    if (t1 == t2)
      return true;
    else if (isSgFunctionType(t1) && isSgFunctionType(t2)) // function type, check return and argument types
    {
      const SgFunctionType * ft1 = isSgFunctionType(t1);
      const SgFunctionType * ft2 = isSgFunctionType(t2);
      if (isAliased (ft1->get_return_type(), ft2->get_return_type())) // CHECK return type
      {
        SgFunctionParameterTypeList* ptl1 = ft1->get_argument_list();
        SgFunctionParameterTypeList* ptl2 = ft2->get_argument_list();
        SgTypePtrList tpl1 = ptl1->get_arguments();
        SgTypePtrList tpl2 = ptl2->get_arguments();
        if (tpl1.size() == tpl2.size())
        {
          size_t equal_count = 0;
          for (size_t i =0; i< tpl1.size(); i++) // check each argument type
          {
            if ( isAliased (tpl1[i], tpl2[i]) )
              equal_count ++;
          }
          if (equal_count == tpl1.size())
            return true;
        }
      }
    }

    return false;
  }

  /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
  bool AliasedObj::operator == ( const AliasedObj & o2)  const
  {
    AliasedObj o1 = *this;
    SgType* own_type = o1.getType();
    SgType* other_type = o2.getType();
    return isAliased (own_type, other_type);
  } */

  // if type may alias to each other, may equal
  bool AliasedObj::mayEqualML(AliasedObjPtr o2, PartEdgePtr pedge)
  {
    return isAliased(getType(), o2->getType());
  }

  //identical type means must equal 
  bool AliasedObj::mustEqualML(AliasedObjPtr o2, PartEdgePtr pedge)
  { 
    if (getType() == o2->getType())
      return true;
    else if (isSgFunctionType(getType()) && isSgFunctionType(o2->getType())) // function type, check return and argument types
    {
      const SgFunctionType * ft1 = isSgFunctionType(getType());
      const SgFunctionType * ft2 = isSgFunctionType(o2->getType());
      if (isAliased (ft1->get_return_type(), ft2->get_return_type())) // CHECK return type
      {
        SgFunctionParameterTypeList* ptl1 = ft1->get_argument_list();
        SgFunctionParameterTypeList* ptl2 = ft2->get_argument_list();
        SgTypePtrList tpl1 = ptl1->get_arguments();
        SgTypePtrList tpl2 = ptl2->get_arguments();
        if (tpl1.size() == tpl2.size())
        {
          size_t equal_count = 0;
          for (size_t i =0; i< tpl1.size(); i++) // check each argument type
          {
            if ( isAliased (tpl1[i], tpl2[i]) )
              equal_count ++;
          }
          if (equal_count == tpl1.size())
            return true;
        }
      }
    }
    return false;
  }
  
  bool AliasedObj::mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    AliasedObjPtr aliased_o2 = boost::dynamic_pointer_cast <AliasedObj> (o2);
    if(aliased_o2) {
        // 1. o2 is AliasedObj:
        return mayEqualML(aliased_o2, pedge);
    } else {
      NamedObjPtr named_o2 = boost::dynamic_pointer_cast <NamedObj> (o2);
      if(named_o2) {
        // 2. o2 is Named Obj: return operator == (AliasedObj&o1, NamedObj & o2)
        return isAliased (getType(), named_o2->getType());
      } else {
        // 3. o2 is  ExpressionObj: always return false
        return false;
      }
    }
  }

  bool AliasedObj::mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    AliasedObjPtr aliased_o2 = boost::dynamic_pointer_cast <AliasedObj> (o2);
    if(aliased_o2) {
      // 1. o2 is AliasedObj:
      return mustEqualML(aliased_o2, pedge);
    } else {
      NamedObjPtr named_o2 = boost::dynamic_pointer_cast <NamedObj> (o2);
      if(named_o2) {
        // 2. o2 is  NamedObj: no way they can be equal mem object
        return false;
      } else {
        // 3. o2 is  ExpressionObj: always return false
        return false;
      }
    }
  }
  
  // Returns whether the two abstract objects denote the same set of concrete objects
  bool AliasedObj::equalSet(AbstractObjectPtr o2, PartEdgePtr pedge)
  {
    // Since ExprObjs and NamedObjs always denote a single concrete MemLoc, their sets are only equal to 
    // to identical ExprObjs and NamedObjs. Since AliasedObjs denote equivalence classes of MemLocs,
    // they cannot denote the same set as anly ExprObj or NamedObj and can only denote the same set
    // as another AliasedObj that corresponds to the same type.
    AliasedObjPtr aliased_o2 = boost::dynamic_pointer_cast <AliasedObj> (o2);
    if(!aliased_o2) return false;
    else            return isAliased(getType(), aliased_o2->getType());
  }
  
  bool AliasedObj::isLiveML(PartEdgePtr pedge)
  { return true; }

  bool AliasedObj::isFull(PartEdgePtr pedge)
  {
    // Aliased objects cannot denote the set of all MemLocs since they are type-specific
    return false;
  }
  
  bool AliasedObj::isEmpty(PartEdgePtr pedge)
  {
    // Since an aliased object denotes the set of all MemLocs of a given type, the corresponding 
    // StxMemLocObject cannot denote the empty set
    return false;
  }
  
  // Returns the type of the MemLoc this object denotes
  SgType* AliasedObj::getType() const
  { return type; }
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr ScalarAliasedObj::copyMLK() const
  { return boost::make_shared<ScalarAliasedObj>(*this); }
 
  /*bool FunctionAliasedObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (AliasedObj::mayEqualML(o2, pedge));
  }

  bool FunctionAliasedObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (AliasedObj::mustEqualML(o2, pedge));
  }*/
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr FunctionAliasedObj::copyMLK() const
  { return boost::make_shared<FunctionAliasedObj>(*this); }

  /*bool LabeledAggregateAliasedObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (AliasedObj::mayEqualML(o2, pedge));
  }

  bool LabeledAggregateAliasedObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (AliasedObj::mustEqualML(o2, pedge));
  }*/
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr LabeledAggregateAliasedObj::copyMLK() const
  { return boost::make_shared<LabeledAggregateAliasedObj>(*this); }
  
  /*bool ArrayAliasedObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (AliasedObj::mayEqualML(o2, pedge));
  }

  bool ArrayAliasedObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (AliasedObj::mustEqualML(o2, pedge));
  }*/
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr ArrayAliasedObj::copyMLK() const
  { return boost::make_shared<ArrayAliasedObj>(*this); }

  // GB: 2012-08-27: should be implementing the following functions here:
  //                 Array::getElements(), getElements(IndexVectorPtr ai), getNumDims(), getDereference()
  MemLocObjectPtr ArrayAliasedObj::getElements(PartEdgePtr pedge) { ROSE_ASSERT(false); /*Need to implement based on type*/ };
  MemLocObjectPtr ArrayAliasedObj::getElements(IndexVectorPtr ai, PartEdgePtr pedge) { ROSE_ASSERT(false); /*Need to implement based on type*/ };
  size_t ArrayAliasedObj::getNumDims(PartEdgePtr pedge) { ROSE_ASSERT(false); /*Need to implement based on type*/ };
  boost::shared_ptr<MemLocObject> ArrayAliasedObj::getDereference(PartEdgePtr pedge) { ROSE_ASSERT(false); /*Need to implement based on type*/ };

  MemLocObjectPtr PointerAliasedObj::getDereference(PartEdgePtr pedge)
  {
    // simplest type-based implementation
    SgPointerType* p_t = isSgPointerType(type);
    assert (p_t != NULL);
    return createAliasedMemLocObject (NULL, p_t->get_base_type(), pedge);
  }

  /*bool PointerAliasedObj::mayEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (AliasedObj::mayEqualML(o2, pedge));
  }

  bool PointerAliasedObj::mustEqualML(const MemLocObjectPtr o2, PartEdgePtr pedge)
  {
    return (AliasedObj::mustEqualML(o2, pedge));
  }*/
  
  // Allocates a copy of this object and returns a pointer to it
  StxMemLocObjectKindPtr PointerAliasedObj::copyMLK() const
  { return boost::make_shared<PointerAliasedObj>(*this); }

  /*std::set<SgType*> ScalarAliasedObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (AliasedObj::getType());
    return rt;
  }

  std::set<SgType*> FunctionAliasedObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (AliasedObj::getType());
    return rt;
  }

  std::set<SgType*> LabeledAggregateAliasedObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (AliasedObj::getType());
    return rt;
  }

  std::set<SgType*> ArrayAliasedObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (AliasedObj::getType());
    return rt;
  }

  std::set<SgType*> PointerAliasedObj::getType()
  {
    std::set<SgType*> rt;
    rt.insert (AliasedObj::getType());
    return rt;
  }*/

  //std::string ScalarAliasedObj::str(const string& indent)
  std::string ScalarAliasedObj::str(std::string indent) // pretty print for the object  
  {
    string rt = "<u>ScalarAliasedObj</u>"/*  @ " + StringUtility::numberToString(this)+ " "*/+ AliasedObj::str(indent);
    return rt;
  }

  //std::string FunctionAliasedObj::str(const string& indent)
  std::string FunctionAliasedObj::str(std::string indent) // pretty print for the object  
  {
    string rt = "<u>FunctionAliasedObj</u>"/*  @ " + StringUtility::numberToString(this)+ " "*/+ AliasedObj::str(indent);
    return rt;
  }

  //std::string LabeledAggregateAliasedObj::str(const string& indent)
  std::string LabeledAggregateAliasedObj::str(std::string indent) // pretty print for the object  
  {
    string rt = "<u>LabeledAggregateAliasedObj</u>"/*  @ " + StringUtility::numberToString(this)+ " "*/+ AliasedObj::str(indent);
    return rt;
  }
  
  //std::string ArrayAliasedObj::str(const string& indent)
  std::string ArrayAliasedObj::str(std::string indent) // pretty print for the object  
  {
    string rt = "<u>ArrayAliasedObj</u>"/*  @ " + StringUtility::numberToString(this)+ " "*/+ AliasedObj::str(indent);
    return rt;
  }
  
  //std::string PointerAliasedObj::str(const string& indent)
  std::string PointerAliasedObj::str(std::string indent) // pretty print for the object  
  {
    string rt = "<u>PointerAliasedObj</u>"/*  @ " + StringUtility::numberToString(this)+ " "*/+ AliasedObj::str(indent);
    return rt;
  }

  /*
     bool operator == ( ScalarAliasedObj & o1, AbstractObject& o2) 
     {
     return dynamic_cast<AbstractObject>
     }
     */

  // A map to store aliased obj set
  // This can provide quick lookup for existing aliased objset to avoid duplicated creation
  map<SgType*, StxMemLocObjectKindPtr > aliased_objset_map; 

  void dump_aliased_objset_map ()
  {
    cout<<"Not yet implemented."<<endl;
    assert (false);

  }
  // creator for different objects
  // ------------------------------------------------------------------
  // Creates an AliasedMemLocObject and an StxMemLocObject that contains it. It can return NULL since not all types are supported.
  // One object per type, Type based alias analysis. A type of the object pointed to by a pointer.
  MemLocObjectPtr createAliasedMemLocObject(SgNode* n, SgType* t, PartEdgePtr pedge)
  {
    StxMemLocObjectKindPtr kind = createAliasedMemLocObjectKind(n, t, pedge);
    MemLocObjectPtr newML;
    if(kind) newML = boost::make_shared<StxMemLocObject>(n, t, kind);
    return newML;
  }
  
  // Creates an AliasedMemLocObject, which is a MemLocObjectKind. It can return NULL since not all types are supported.
  // One object per type, Type based alias analysis. A type of the object pointed to by a pointer.
  StxMemLocObjectKindPtr createAliasedMemLocObjectKind(SgNode* n, SgType* t, PartEdgePtr pedge)
  {
    bool assert_flag = true; 
    assert (t!= NULL);
    StxMemLocObjectKindPtr rt;
    map<SgType*, StxMemLocObjectKindPtr >::const_iterator iter;
    SgType* baseT = t->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_TYPEDEF_TYPE);
    
    iter = aliased_objset_map.find(baseT);
    if (iter == aliased_objset_map.end())
    { // None found, create a new one and update the map
      if (SageInterface::isScalarType(baseT) || isSgEnumType(baseT))
        // We define the following SgType as scalar types: 
        // char, short, int, long , void, Wchar, Float, double, long long, string, bool, complex, imaginary 
        // any type of enum
      { rt = boost::make_shared<ScalarAliasedObj>(n, baseT, pedge); }
      else if (isSgPointerType(baseT) || isSgReferenceType(t))
      { rt = boost::make_shared<PointerAliasedObj>(n, baseT, pedge); }
      else if (isSgArrayType(baseT))
      {
        // TODO: We may wan to only generate a single array aliased obj for a multi-dimensional array
        // which will have multiple SgArrayType nodes , each per dimension
        rt = boost::make_shared<ArrayAliasedObj>(n, baseT, pedge);
      }
      else if (isSgClassType(baseT))
      { rt = boost::make_shared<LabeledAggregateAliasedObj>(n, baseT, pedge); }
      else if (isSgFunctionType(baseT))
      { rt = boost::make_shared<FunctionAliasedObj>(n, baseT, pedge); }  
      // Unwrap typedefs
      /*else if (isSgTypedefType(t))
      { rt = createAliasedMemLocObjectKind(n, baseT, pedge); }*/
      else
      {
        cerr<<"Warning: createAliasedMemLocObject(): unhandled type: \""<<t->class_name()<<"\" base type: \""<<baseT->class_name()<<"\" n="<<(n? cfgUtils::SgNode2Str(n): "NULL")<<endl;
        assert_flag = false;
      }
      
      // Update the map only if something has been created
      if (rt) 
        aliased_objset_map[baseT]= rt;
    }
    else // Found one, return it directly
    {
      rt = (*iter).second; 
    }  

    if (assert_flag) assert (rt); // we cannot always assert this since not all SgType are supported now
    return rt;
  } 
  
  // variables that are explicitly declared/named in the source code
  // local, global, static variables,
  // formal and actual function parameters
  //  Scalar
  //  Labeled aggregate
  //  Pointer
  //  Array
  // anchor_symbol - the anchor lexical symbol from which the created object is based. For array reference expressions
  //    array[i] this is the symbol of array. For dot expressions a.b it is the symbol of b. anchor_symbol serves
  //    as a first filter for determining whether two NamedObjs denote the same memory location
  // type - the type of the actual expression for which the NamedObj is being created (e.g. for array[i] it is the
  //    type of expression array[i], not the anchor_symbol array. The type determines whether we create a 
  //    ScalarNamedObj, PointerNamedObj, etc.
  // ------------------------------------------------------------------
  MemLocObjectPtr createNamedMemLocObject(SgNode* n, SgSymbol* anchor_symbol, SgType* t, PartEdgePtr pedge, MemLocObjectPtr parent, IndexVectorPtr iv)
  {
    StxMemLocObjectKindPtr rt;

    SgType* baseT = t->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_TYPEDEF_TYPE);
    
    if (!isSgVariableSymbol(anchor_symbol) && !isSgFunctionSymbol(anchor_symbol))
    {
      cerr<<"Warning. createNamedMemLocObject() skips non-variable and non-function symbol:"<< anchor_symbol->class_name() <<endl;
      StxMemLocObjectPtr NULL_ML;
      return NULL_ML;
    }
    
    // check parameters
    assert (anchor_symbol != NULL);
    // ! (isArray || isPointer) ==> !isArray && !isPointer
    if (! isSgArrayType(anchor_symbol->get_type())  && ! isSgPointerType(anchor_symbol->get_type()))
    { // only array elements can have different type from its anchor (parent) symbol
      // pointer type can also have array-like subscripting
      // typedef elements can have different type from its anchor symbol
      assert (anchor_symbol->get_type() == t);
    }
    bool assert_flag = true; 
    
    if(isSgPointerType(baseT) || isSgReferenceType(baseT))
    { 
        rt = boost::make_shared<PointerNamedObj>(n, anchor_symbol, parent, iv, pedge); 
    }
    else if (SageInterface::isScalarType(baseT) || isSgEnumType(baseT))
    // We define the following SgType as scalar types: 
    // char, short, int, long , void, Wchar, Float, double, long long, string, bool, complex, imaginary 
    { 
        rt = boost::make_shared<ScalarNamedObj>(n, anchor_symbol, parent, iv, pedge); 
    }
    else if (isSgFunctionType(baseT))
    { 
        rt = boost::make_shared<FunctionNamedObj>(n, anchor_symbol, pedge); 
    }
    else if (isSgClassType(baseT))
    {
        // #SA 10/15/12
        // Stripping init() from constructor
        // 
        rt = boost::make_shared<LabeledAggregateNamedObj>(n, anchor_symbol, parent, iv, pedge);
        boost::dynamic_pointer_cast<LabeledAggregateNamedObj>(rt)->init(anchor_symbol, parent, iv, pedge);
    }
    else if(isSgArrayType(baseT)) // This is for the entire array variable
    { 
        rt = boost::make_shared<ArrayNamedObj>(n, anchor_symbol, parent, iv, pedge); 
    }
    // #SA 11/28/12
    // to handle typedef memory objects
    // #GB 3/12/13: Don't need it if we always consider base types of each type
    /*else if(isSgTypedefType(t))
    {
      // make a recursive call to create the object with typedef base type
      return createNamedMemLocObject(n, anchor_symbol, t->findBaseType(), pedge, parent, iv);
    }*/
    else
    {
      cerr<<"Warning: createNamedMemLocObject(): unhandled symbol: \""<<anchor_symbol->class_name() << "\"" << 
          " name: \"" <<  anchor_symbol->get_name().getString() << "\" type: \""<< t->class_name()<< "\", base type: \""<<t->findBaseType()->class_name()<<"\" @ "<<StringUtility::numberToString(anchor_symbol) <<endl;
      SgType* myT = t;
      while(isSgReferenceType(myT) || isSgModifierType(myT)) {
        cerr << "base type: "<<cfgUtils::SgNode2Str(isSgReferenceType(t)->get_base_type())<<""<<endl;
        if(isSgReferenceType(myT)) myT = isSgReferenceType(myT)->get_base_type();
        else if(isSgModifierType(myT)) myT = isSgModifierType(myT)->get_base_type();
      }
      cerr << "final base type: "<<cfgUtils::SgNode2Str(myT)<<""<<endl;
        
      //assert_flag = false;
    }
    
    if (assert_flag) assert (rt); // we cannot always assert this since not all SgType are supported now
    
    return boost::make_shared<StxMemLocObject>(n, t, rt);
  }
  
  // Create a NamedMemLocObject from a static variable reference of form a and a.b.c where a is not a reference type
  MemLocObjectPtr createNamedMemLocObject_DirectVarRef(SgNode* n, SgExpression* ref, PartEdgePtr pedge) 
  {
    // If this is a plain VarRef
    if(isSgVarRefExp(ref)) 
      return createNamedMemLocObject(n, isSgVarRefExp(ref)->get_symbol(), isSgVarRefExp(ref)->get_symbol()->get_type(), 
                                     pedge, MemLocObjectPtr(), IndexVectorPtr()); // parent should be NULL since it is not a member variable symbol)
    // Otherwise, if this is a dot expression
    else if(isSgDotExp(n)) {
      // Create the MemLocObject for the parent dot expression
      MemLocObjectPtr parent;
      if(isSgDotExp(isSgDotExp(n)->get_lhs_operand()))
        parent = createNamedMemLocObject_DirectVarRefDotExp(isSgDotExp(n)->get_lhs_operand(), isSgDotExp(n)->get_lhs_operand(), pedge);
      ROSE_ASSERT(isSgVarRefExp(isSgDotExp(n)->get_rhs_operand()));
      SgSymbol* symbol = isSgVarRefExp(isSgDotExp(n)->get_rhs_operand())->get_symbol();
      
      // Create the MemLocObject for the overall expression
      return createNamedMemLocObject(n, symbol, symbol->get_type(), pedge, parent, IndexVectorPtr()); // parent should be NULL since it is not a member variable symbol)
    }
    ROSE_ASSERT(0);
  }
  
  // Create a NamedMemLocObject from a static variable reference of form a.b.c where a is not a reference type
  MemLocObjectPtr createNamedMemLocObject_DirectVarRefDotExp(SgNode* n, SgExpression* ref, PartEdgePtr pedge) 
  {
    // If this is a SgDotExp in the middle of a larger dot expression tree
    if(isSgDotExp(ref)) {
      MemLocObjectPtr parentML = 
              createNamedMemLocObject_DirectVarRefDotExp(
                     isSgDotExp(ref)->get_lhs_operand(), 
                     isSgDotExp(ref)->get_lhs_operand(),
                     pedge);
      ROSE_ASSERT(isSgVarRefExp(isSgDotExp(ref)->get_rhs_operand()));
      return boost::make_shared<LabeledAggregateNamedObj>(isSgDotExp(ref)->get_rhs_operand(), 
              isSgVarRefExp(isSgDotExp(ref)->get_rhs_operand())->get_symbol(), parentML, IndexVectorPtr(), pedge);
    }
    // If this is the top-most dot expression
    else if(isSgVarRefExp(ref))
      return boost::make_shared<LabeledAggregateNamedObj>(n, isSgVarRefExp(ref)->get_symbol(), MemLocObjectPtr(), IndexVectorPtr(), pedge);
    
    cerr << "Unhandled case in createNamedMemLocObject_DirectVarRefDotExp(n="<<cfgUtils::SgNode2Str(n)<<", ref="<<cfgUtils::SgNode2Str(ref)<<endl;
    ROSE_ASSERT(0);
    /*
    assert (r!=NULL);
    SgVariableSymbol * s = r->get_symbol();
    assert (s != NULL);
    SgType * t = s->get_type();
    assert (t != NULL);
    
    // If ref is a simple reference to a static variable
    if(isSgVarRefExp(ref))
      return createNamedMemLocObject(n, symbol, symbol->get_type(), pedge, MemLocObjectPtr(), IndexVectorPtr()); // parent should be NULL since it is not a member variable symbol
    // If ref is part of a dot expression and its parent is also a dot expression
    else if(isSgDotExp(ref)) {
      // We know that all the parents of this reference are direct SgDotExpressions, so call createNamedMemLocObject_DirectVarRef()
      // recursively to create a MemLocObject of ref's parent and then call the generic createNamedMemLocObject()
      // to construct the full MemLocObject for ref that refers to this parent.
      SgInitializedName* parentName = SageInterface::convertRefToInitializedName(ref->get_parent());
      SgSymbol* parenSymbol = isSgVariableSymbol(parentName->get_symbol_from_symbol_table());
      MemLocObjectPtr parent = createNamedMemLocObject_DirectVarRef(n, ref->get_parent(), parentSymbol, pedge);
      return createNamedMemLocObject(n, symbol, symbol->get_type(), pedge, parent, IndexVectorPtr()); // parent should be NULL since it is not a member variable symbol
    }*/
  }
  
  // For a SgVarRef, find the corresponding symbol first
  // 1. if is a instance symbol. It corresponding to real top level instances of types. Create NamedObj as we see each of them, NULL as parent
  //     They are the symbols with declarations not under SgClassDefinition
  // 2. if it is a symbol within a class definition, it is a child of an instance, 
  //     so check if is the rhs of SgDotExp/SgArrowExp, if not assert
  //     use lhs of SgDotExp/SgArrowExp as parent
  //         lhs could be SgVarRefExp: find the corresponding NamedObj as parent (top level object, labeled aggregate)
  //         lhs could be another SgDotExp: find its rhs's NamedObj as parent
/*  MemLocObjectPtr createNamedMemLocObject(SgNode* n, SgExpression* ref, PartEdgePtr pedge) // create NamedMemLocObject or aliased object from a variable reference 
  {
    assert (r!=NULL);
    SgVariableSymbol * s = r->get_symbol();
    assert (s != NULL);
    SgType * t = s->get_type();
    assert (t != NULL);
    
    // symbol within SgClassDefinition
    //if(isMemberVariableDeclarationSymbol(s))
    // If this is an indirection via a pointer (reference a->b or a.b where a is a reference)
    if(!isSgExpression(r->get_parent()) || isIndirectDotVarRef(isSgExpression(r->get_parent())))
    { 
      // We model referents of pointers as AliasedMemLocObjects
      return createAliasedMemLocObject(n, t, pedge);
    // Else, if this is a dot expression a.b with no pointer access
    } else if(isSgDotExp (r->get_parent())) {
      / *
Dbg::dbg << "createNamedMemLocObject()"<<endl;
Dbg::dbg << "n = "<<cfgUtils::SgNode2Str(parent)<<endl;
Dbg::dbg << "r = "<<cfgUtils::SgNode2Str(r)<<endl;
Dbg::dbg << "s = "<<cfgUtils::SgNode2Str(s)<<endl;
Dbg::dbg << "t = "<<cfgUtils::SgNode2Str(t)<<endl;
Dbg::dbg << "t2 = "<<cfgUtils::SgNode2Str(t)<<endl;
Dbg::dbg << "parent = "<<cfgUtils::SgNode2Str(parent)<<endl;* /
      SgBinaryOp* b_e = isSgBinaryOp (r->get_parent());
      ROSE_ASSERT(b_e);
      ROSE_ASSERT(b_e->get_rhs_operand_i() == r);

      // First, get MemLocObject for its parent part
      MemLocObjectPtr p_obj;
      SgExpression * lhs = b_e ->get_lhs_operand_i();
      ROSE_ASSERT(lhs != NULL);
      if (isSgVarRefExp(lhs))
        p_obj = createNamedMemLocObject(NULL, isSgVarRefExp(lhs), pedge); // recursion here
      else if (isSgDotExp (lhs)) // another SgDotExp 
      { // find its rhs's NamedObj as parent
        SgExpression* rhs = isSgBinaryOp (lhs) -> get_rhs_operand_i();
        assert (isSgVarRefExp (rhs) != NULL); // there might be some more cases!!
        p_obj = createNamedMemLocObject(NULL, isSgVarRefExp(rhs), pedge);
      } else
        ROSE_ASSERT(false);
      // now create the child mem obj
      MemLocObjectPtr mem_obj = createNamedMemLocObject(n, s, s->get_type(), pedge, p_obj, IndexVectorPtr()); // we don't explicitly store index for elements of labeled aggregates for now 
      // assert (mem_obj != NULL); // we may return NULL for cases not yet handled
      return mem_obj;
    }
    else // other symbols
    {
      MemLocObjectPtr mem_obj = createNamedMemLocObject(n, s, s->get_type(), pedge, MemLocObjectPtr(), IndexVectorPtr());
      // assert (mem_obj != NULL); // We may return NULL for cases not yet handled
      return mem_obj;
    }
  }*/

 // create NamedObj from an array element access 
 /* The AST for a 2-D array element access:  
  * Two SgPntrArrRefExp will be found. But we only need to create one element. 
  * We choose to trigger the creation when we see the top level SgPntrArrRefExp
       a[4][6]    SgPntrArrRefExp  (a[4], 6)  // We focus on this level
                          lhs: SgPntrArrRefExp (a, 4) // inner level, we skip this SgPntrArrRefExp
                                 lhs: SgVarRefExp = a   //  find symbol for a, the go back wards to find rhs operands such as 4 and 6
                                 rhs: SgIntVal = 4
                          rhs: SgIntVal =6    
    Two things should happen when we see an array element access like: a[4][6]
    1. Create ArrayNamedObj for the entire array a. It has two further cases:
       a. The array is a standalone object, not a field of another aggregates or array (TODO)
          create it based on symbol is sufficient
       b. The array is part of other objects, such as structure/class/array
          We have to create it based on both its symbol and parent, and optionally the index 
       The creation interface should take care of avoiding duplicated creation of the entire array object.    
    2. Create the array element NamedMemLocObject for  a[4][6], based on parent a, and indexVector <4, 6>
  */
  MemLocObjectPtr createNamedMemLocObject_PntrArrRef(SgNode* n, SgPntrArrRefExp* r, PartEdgePtr pedge) 
  {
    MemLocObjectPtr mem_obj;
    assert (r!=NULL);
    //MemLocObjectPtr whole_array_obj;

    SgPntrArrRefExp* arr_ref_parent = isSgPntrArrRefExp(r->get_parent());
    // Only create Named objects for top-level SgPntrArrRefExps
    // Or for SgPntrArrRefExps that denote the indexes inside SgPntrArrRefExps (e.g. array[array[i]])
    if(!(arr_ref_parent && isSgPntrArrRefExp (arr_ref_parent->get_lhs_operand()) && arr_ref_parent->get_lhs_operand()==r))
    {
      // try to create the Obj for the whole array first
      SgExpression* arrayNameExp = NULL;
      std::vector<SgExpression*>* subscripts = new std::vector<SgExpression*>;

      SageInterface::isArrayReference(r, &arrayNameExp, &subscripts);
      /*Dbg::dbg << "createNamedMemLocObject()"<<endl;
      Dbg::dbg << "    n="<<cfgUtils::SgNode2Str(n)<<endl;
      Dbg::dbg << "    r="<<cfgUtils::SgNode2Str(r)<<endl;
      Dbg::dbg << "    arrayNameExp="<<cfgUtils::SgNode2Str(arrayNameExp)<<endl;*/
      
      // array[i] or a.b.c[i]
      if(SgSymbol* symbol = isDirectVarRef(arrayNameExp)) {
        MemLocObjectPtr whole_array_obj;
        // If this is a plain VarRef
        if(isSgVarRefExp(arrayNameExp)) 
          whole_array_obj = createNamedMemLocObject(arrayNameExp, symbol, symbol->get_type(), pedge, MemLocObjectPtr(), IndexVectorPtr()); // parent should be NULL since it is not a member variable symbol)
        // Otherwise, if this is a dot expression
        else if(isSgDotExp(arrayNameExp))
          whole_array_obj = createNamedMemLocObject_DirectVarRefDotExp(n, isSgDotExp(arrayNameExp), pedge);
        
        // create the element access then, using symbol, parent, and index
        IndexVectorPtr iv = generateIndexVector(*subscripts);
        assert (iv != 0);
        return createNamedMemLocObject(n, symbol, r->get_type(), pedge, whole_array_obj, iv);
      }
      // Otherwise, the array reference denotes an aliased object
      else
        return createAliasedMemLocObject(n, r->get_type(), pedge);
      
      /* GB 2013-03-12: original code tried to capture simple array reference expressions but allowed 
       *      odd thigs like (a->q)[10], which are clearly aliased objects because of the dereference
      SgInitializedName* array_name = SageInterface::convertRefToInitializedName(arrayNameExp);
      SgVariableSymbol * s = NULL; 
      if(array_name != NULL)
      {
        s = isSgVariableSymbol(array_name->get_symbol_from_symbol_table());
        assert (s != NULL);
        SgType * t = s->get_type();
        // both array type and pointer type can have subscripts like p[10]
        assert (isSgArrayType(t) != NULL || isSgPointerType(t) != NULL);
        whole_array_obj = SyntacticAnalysis::Expr2MemLocStatic(s, pedge);
        if (!whole_array_obj)
        {
           cerr<<"Warning. Unhandled case in createNamedMemLocObject(SgPntrArrRefExp*) where the array is part of other aggregate objects."<<endl;
        }
      }
      else
      {
        // We only handle canonical array like a[1], not (pointer+10)[1] for now TODO
        cerr<<"Warning. Unhandled case in createNamedMemLocObject(SgPntrArrRefExp*) where the array name is not a single variable."<<endl;
      }

      // create the element access then, using symbol, parent, and index
      IndexVectorPtr iv = generateIndexVector(*subscripts);
      assert (iv != 0);
      mem_obj = createNamedMemLocObject(n, s, r->get_type(), pedge, whole_array_obj, iv);*/
      
      // GB: Do we need to deallocate subscripts???
    }
    else
    {
      // This is isSgPntrArrRefExp in the middle
      // we should not generate any MemLocObject for it.
      // GB: !!! This appears to be broken for arrays of type int*** array, where we do want to have objects for the internal SgPntrArrRefExp!
    }
    // assert (mem_obj != NULL); // we may return NULL 
    return mem_obj;
  }

  
  // A map to avoid duplicated creation of ExprObj
  // SgExpression here excludes SgVarRef, which should be associated with a named memory object
  map<SgExpression*, MemLocObjectPtr> expr_objset_map; 

  // ------------------------------------------------------------------
  // Creator for expression MemLocObject
  MemLocObjectPtr createExpressionMemLocObject(SgExpression* anchor_exp, PartEdgePtr pedge)
  {
    StxMemLocObjectKindPtr rt;
    assert (anchor_exp != NULL);

    bool assert_flag = true; 
    SgVarRefExp* var_exp = isSgVarRefExp (anchor_exp);
    if (var_exp)
    {
      cerr<<"Error. Trying to create an expression object when anchor_exp is a SgVarRefExp, which should be associated with a named object. "<<endl;
      assert (false);
    }
    
    if (expr_objset_map[anchor_exp] == NULL)
    { 
      SgType* t = anchor_exp->get_type();
      // None found, create a new one depending on its type and update the map
      if (SageInterface::isScalarType(t) || (isSgReferenceType(t) && SageInterface::isScalarType(isSgReferenceType(t)->get_base_type())))
        // We define the following SgType as scalar types: 
        // char, short, int, long , void, Wchar, Float, double, long long, string, bool, complex, imaginary 
      { 
        // An array element access could also have a scalar type, but we want to record it as a named object, instead of an expression object
        rt = boost::make_shared<ScalarExprObj>(anchor_exp, pedge);
      }
      else if (isSgFunctionType(t) || (isSgReferenceType(t) && isSgFunctionType(isSgReferenceType(t)->get_base_type())))
      { 
          rt = boost::make_shared<FunctionExprObj>(anchor_exp, pedge); 
      }
      else if (isSgPointerType(t) || (isSgReferenceType(t) && isSgPointerType(isSgReferenceType(t)->get_base_type())))
      { 
          rt = boost::make_shared<PointerExprObj>(anchor_exp, pedge); 
      }
      else if (isSgClassType(t) || (isSgReferenceType(t) && isSgClassType(isSgReferenceType(t)->get_base_type())))
      {
          // #SA 10/15/12
          // stripping the init(...) function from the constructor to avoid double deletion of object
          // 
          rt = boost::make_shared<LabeledAggregateExprObj>(anchor_exp, pedge); 
          boost::dynamic_pointer_cast<LabeledAggregateExprObj>(rt)->init(anchor_exp, pedge);          
      }
      else if (isSgArrayType(t) || (isSgReferenceType(t) && isSgArrayType(isSgReferenceType(t)->get_base_type())))
      { 
          rt = boost::make_shared<ArrayExprObj>(anchor_exp, pedge); 
      }
      else
      {
        // By default make it a scalar object
        rt = boost::make_shared<ScalarExprObj>(anchor_exp, pedge);
/*        cerr<<"Warning: createExprMemLocObject(): unhandled expression:\""<<anchor_exp->class_name() << 
          "\" string : \"" <<  anchor_exp->unparseToString() << "\" type: \""<< t->class_name()<< "\" @ "<<StringUtility::numberToString(anchor_exp) <<endl;*/
        assert_flag = false;
      }

      if (assert_flag) assert (rt); // we cannot always assert this since not all SgType are supported now

      // update the map  only if something has been created
      MemLocObjectPtr newML;
      if (rt) {
        newML = boost::make_shared<StxMemLocObject>(anchor_exp, anchor_exp->get_type(), rt);
        expr_objset_map[anchor_exp] = newML;
      }
      return newML;
    }
    else // Found one, return it directly
    {
      ROSE_ASSERT(expr_objset_map[anchor_exp]);
      return expr_objset_map[anchor_exp];
    }
  }
  
  // If a symbol corresponds to a member variable declaration within SgClassDefinition, returns a pointer
  // to the SgClassDefinition. Otherwise, returns NULL.
  SgClassDefinition* isMemberVariableDeclarationSymbol(SgSymbol * s)
  {
    assert (s!=NULL);
    // Only relevant for SgVariableSymbol for now
    SgVariableSymbol* vs = isSgVariableSymbol (s);
    if (vs != NULL)
    {
      SgInitializedName* i_name = vs->get_declaration();
      assert  (i_name != NULL);
      if (SgClassDefinition* def = isSgClassDefinition(i_name->get_scope()))
        return def;
    }
    return NULL;
  }
  
  /// Visits live expressions to determine whether the given SgExpression is an operand of the visited Sgxpression
  class IsOperandVisitor : public ROSE_VisitorPatternDefaultBase
  {
    public:
    bool isOperand;
    SgExpression* op;

    IsOperandVisitor(SgExpression* op) : isOperand(false), op(op) {}

    // Should only be called on expressions
    void visit(SgNode *) { assert(0); }

    // Catch up any other expressions that are not yet handled
    void visit(SgExpression *)
    {
        // Function Reference
        // !!! CURRENTLY WE HAVE NO NOTION OF VARIABLES THAT IDENTIFY FUNCTIONS, SO THIS CASE IS EXCLUDED FOR NOW
        //} else if(isSgFunctionRefExp(sgn)) {
        //} else if(isSgMemberFunctionRefExp(sgn)) { 

        // !!! DON'T KNOW HOW TO HANDLE THESE
        //} else if(isSgStatementExpression(sgn)) {(

        // Typeid
        // !!! DON'T KNOW WHAT TO DO HERE SINCE THE RETURN VALUE IS A TYPE AND THE ARGUMENT'S VALUE IS NOT USED
        //} else if(isSgTypeIdOp(sgn)) {
        // Var Args
        // !!! DON'T HANDLE THESE RIGHT NOW. WILL HAVE TO IN THE FUTURE
        /*  SgVarArgOp 
            SgExpression *  get_operand_expr () const 
            SgVarArgCopyOp
            SgExpression *  get_lhs_operand () const
            SgExpression *  get_rhs_operand () const  
            SgVarArgEndOp 
            SgExpression *  get_operand_expr 
            SgVarArgStartOneOperandOp 
            SgExpression *  get_operand_expr () const 
            SgVarArgStartOp 
            SgExpression *  get_lhs_operand () const
            SgExpression *  get_rhs_operand () const */
        // !!! WHAT IS THIS?
        // SgVariantExpression


        // TODO: Make this assert(0), because unhandled expression types are likely to give wrong results
    }
    // Initializer for a variable
    void visit(SgAssignInitializer *sgn) {
      if(op == sgn->get_operand()) isOperand = true;
    }
    // Initializer for a function arguments
    void visit(SgConstructorInitializer *sgn) {
        SgExprListExp* exprList = sgn->get_args();
        for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
            expr!=exprList->get_expressions().end(); expr++)
          if(op == *expr) {
            isOperand = true;
            return;
          }
    }
    // Initializer that captures internal stucture of structs or arrays ("int x[2] = {1,2};", it is the "1,2")
    // NOTE: Should this use abstractMemory interface ?
    void visit(SgAggregateInitializer *sgn) {
        SgExprListExp* exprList = sgn->get_initializers();
        for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
            expr!=exprList->get_expressions().end(); expr++)
          if(op == *expr) {
            isOperand = true;
            return;
          }
    }
    // Designated Initializer 
    void visit(SgDesignatedInitializer *sgn) {
        SgExprListExp* exprList = sgn->get_designatorList();
        for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
            expr!=exprList->get_expressions().end(); expr++)
          if(op == *expr) {
            isOperand = true;
            return;
          }
    }
    // Array References
    void visit(SgPntrArrRefExp *sgn) {
      SgExpression* arrayNameExp = NULL;
      std::vector<SgExpression*>* subscripts = new std::vector<SgExpression*>;
      SageInterface::isArrayReference(sgn, &arrayNameExp, &subscripts);
      
      for (std::vector<SgExpression*>::iterator i = subscripts->begin(); i != subscripts->end(); i++) {
        if(op==*i) { isOperand = true; return; }
      }
    }
    // Binary Operations
    void visit(SgBinaryOp *sgn) {
      if(op == sgn->get_lhs_operand()) { isOperand = true; return; }
      if(op == sgn->get_rhs_operand()) { isOperand = true; return; }
    }
    // Unary Operations
    void visit(SgUnaryOp *sgn) {
      if(op == sgn->get_operand()) isOperand = true;
    }
    // Conditionals (condE ? trueE : falseE)
    void visit(SgConditionalExp *sgn) {
      if(op == sgn->get_conditional_exp()) { isOperand = true; return; }
      if(op == sgn->get_true_exp())        { isOperand = true; return; }
      if(op == sgn->get_false_exp())       { isOperand = true; return; }
    }
    // Delete
    void visit(SgDeleteExp *sgn) {
        if(op == sgn->get_variable()) isOperand = true;
    }
    // New
    void visit(SgNewExp *sgn) {
        // The placement arguments are used
        SgExprListExp* exprList = sgn->get_placement_args();
        // NOTE: placement args are optional
        // exprList could be NULL
        // check for NULL before adding to used set
        if(exprList) {
            for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
                expr!=exprList->get_expressions().end(); expr++)
              if(op == *expr) {
                isOperand = true;
                return;
              }
        }

        // The placement arguments are used
        // check for NULL before adding to used set
        // not sure if this check is required for get_constructor_args()
        exprList = sgn->get_constructor_args()->get_args();
        if(exprList) {
            for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
                expr!=exprList->get_expressions().end(); expr++)
              if(op == *expr) {
                isOperand = true;
                return;
              }
        }

        // The built-in arguments are used (DON'T KNOW WHAT THESE ARE!)
        // check for NULL before adding to used set
        // not sure if this check is required for get_builtin_args()
        if(sgn->get_builtin_args()) {
            if(op == sgn->get_builtin_args()) { isOperand = true; return; }
        }
    }
    // Function Calls
    void visit(SgFunctionCallExp *sgn) {
      SgExprListExp* exprList = sgn->get_args();
      for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
            expr!=exprList->get_expressions().end(); expr++)
        if(op == *expr) {
          isOperand = true;
          break;
        }
    }
    // Sizeof
    void visit(SgSizeOfOp *sgn) {
        // XXX: The argument is NOT used, but its type is
        // NOTE: get_operand_expr() returns NULL when sizeof(type)
        // FIX: use get_operand_expr() only when sizeof() involves expr
        if(sgn->get_operand_expr()) {
          if(op == sgn->get_operand_expr()) { isOperand = true; return; }
        }
    }
    // This
    void visit(SgThisExp *sgn) {
    }
    // Variable Reference (we know this expression is live)
    void visit(SgVarRefExp *sgn) {
    }

    void visit(SgReturnStmt *sgn) {
      if(op == sgn->get_expression()) { isOperand = true; return; }
    }
  }; // class IsOperandVisitor

  // Return true if op is an operand of the given SgNode n and false otherwise.
  bool isOperand(SgNode* n, SgExpression* op) {
    if(isSgExpression(n)) {
      IsOperandVisitor helper(op);
      n->accept(helper);
      return helper.isOperand;
    } else if(isSgInitializedName(n)) {
      if(op==isSgInitializedName(n)->get_initializer()) return true;
    } else if(isSgReturnStmt(n)) {
      if(op==isSgReturnStmt(n)->get_expression()) return true;
    } else if(isSgExprStatement(n)) {
      if(op==isSgExprStatement(n)->get_expression()) return true;
    } else if(isSgCaseOptionStmt(n)) {
      if(op==isSgCaseOptionStmt(n)->get_key()) return true;
      if(op==isSgCaseOptionStmt(n)->get_key_range_end()) return true;
    } else if(isSgIfStmt(n)) {
      ROSE_ASSERT(isSgExprStatement(isSgIfStmt(n)->get_conditional()));
      if(op==isSgExprStatement(isSgIfStmt(n)->get_conditional())->get_expression()) return true;
    } else if(isSgForStatement(n)) {
      ROSE_ASSERT(isSgExprStatement(isSgForStatement(n)->get_test()));
      if(op==isSgExprStatement(isSgForStatement(n)->get_test())->get_expression()) return true;
      if(op==isSgForStatement(n)->get_increment()) return true;
    } else if(isSgWhileStmt(n)) {
      ROSE_ASSERT(isSgExprStatement(isSgWhileStmt(n)->get_condition()));
      if(op==isSgExprStatement(isSgWhileStmt(n)->get_condition())->get_expression()) return true;
    } else if(isSgDoWhileStmt(n)) {
      ROSE_ASSERT(isSgExprStatement(isSgDoWhileStmt(n)->get_condition()));
      if(op==isSgExprStatement(isSgDoWhileStmt(n)->get_condition())->get_expression()) return true;
    } else if(isSgInitializedName(n)) {
      if(op==isSgInitializedName(n)->get_initializer()) return true;
    } else if(isSgInitializedName(n)) {
      if(op==isSgInitializedName(n)->get_initializer()) return true;
    } else {
       // For now we ignore the other cases but should make sure to cover them all in the future
    }
    
    return false;
  }
}; // namespace dataflow
