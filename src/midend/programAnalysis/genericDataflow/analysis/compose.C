#include "compose.h"
#include "stx_analysis.h"
#include "const_prop_analysis.h"
#include <boost/enable_shared_from_this.hpp>
#include "printAnalysisStates.h"

namespace dataflow
{
int composerDebugLevel=1;

// -------------------------------------
// ----- Expression Interpretation -----
// -------------------------------------

// ----------------------
// --- Calling Expr2* ---
// ----------------------

// Contains the arguments needed by Expr2* calls
class FuncCallerArgs_Expr2Any
{
  public:
  SgNode* n;
  PartEdgePtr pedge;
  
  FuncCallerArgs_Expr2Any(SgNode* n, PartEdgePtr pedge) : n(n), pedge(pedge){}
  
  std::string str(std::string indent="") {
    ostringstream oss;
    oss << "[n="<<cfgUtils::SgNode2Str(n)<<", pedge="<<pedge->str()<<"]";
    return oss.str();
  }
};

// --- Calling Expr2Val ---

// These classes wrap the functionality of calling a specific function within an 
// Analysis or a Part
class Expr2ValCaller : public FuncCaller<ValueObjectPtr, const FuncCallerArgs_Expr2Any>
{
  public:
  // Calls the given analysis' implementation of Expr2Val within the given node
  ValueObjectPtr operator()(const FuncCallerArgs_Expr2Any& args, ComposedAnalysis* client)
  {
    try{
      //FuncCallerArgs_Expr2Any& a = dynamic_cast<FuncCallerArgs_Expr2Any&>(args);
      return client->Expr2Val(args.n, args.pedge);
    } catch (std::bad_cast bc) { ROSE_ASSERT(false); }
  }
  string funcName() const{ return "Expr2Val"; }
};

ValueObjectPtr ChainComposer::Expr2Val(SgNode* n, PartEdgePtr pedge, ComposedAnalysis* client) { 
  Expr2ValCaller c;
  FuncCallerArgs_Expr2Any args(n, pedge);
  return callServerAnalysisFunc<ValueObjectPtr, FuncCallerArgs_Expr2Any>(args, client, c);
}

// Variant of Expr2Val that inquires about the value of the memory location denoted by the operand of the 
// given node n, where the part denotes the set of prefixes that terminate at SgNode n.
ValueObjectPtr ChainComposer::OperandExpr2Val(SgNode* n, SgNode* operand, PartEdgePtr pedge, ComposedAnalysis* client) {
  // Get the part edges of the execution prefixes that terminate at the operand before continuing directly 
  // to SgNode n in the given part edge
  list<PartEdgePtr> opPartEdges = pedge->getOperandPartEdge(n, operand);
  Dbg::dbg << "opPartEdges(#"<<opPartEdges.size()<<")="<<endl;
  for(list<PartEdgePtr>::iterator opE=opPartEdges.begin(); opE!=opPartEdges.end(); opE++) {
    Dbg::indent ind;
    Dbg::dbg << (*opE)->str()<<endl;
  }
  
  // The ValuerObjects that represent the operand within different Parts in opParts
  list<ValueObjectPtr> partVs; 
  
  // Iterate over all the parts to get the expression and memory MemLocObjects for operand within those parts
  for(list<PartEdgePtr>::iterator opE=opPartEdges.begin(); opE!=opPartEdges.end(); opE++)
    partVs.push_back(Expr2Val(operand, *opE, client));

  return boost::static_pointer_cast<ValueObject>(boost::make_shared<UnionValueObject>(partVs));
}

// --- Calling Expr2MemLoc ---

class Expr2MemLocCaller : public FuncCaller<MemLocObjectPtr, const FuncCallerArgs_Expr2Any>
{
  public:
  // Calls the given analysis' implementation of Expr2MemLoc within the given node
  MemLocObjectPtr operator()(const FuncCallerArgs_Expr2Any& args, ComposedAnalysis* client)
  {
    try{
      return client->Expr2MemLoc(args.n, args.pedge);
    } catch (std::bad_cast bc) { ROSE_ASSERT(false); }
  }
  string funcName() const{ return "Expr2MemLoc"; }
};

MemLocObjectPtrPair ChainComposer::Expr2MemLoc(SgNode* n, PartEdgePtr pedge, ComposedAnalysis* client) {
  // Call Expr2MemLoc_ex() and wrap the results of the memory MemLocObject with a UnionMemLocObject
  MemLocObjectPtrPair p = Expr2MemLoc_ex(n, pedge, client);
  Dbg::dbg << "Expr2MemLoc() p="<<p.str("&npsb;&npsb;&npsb;&npsb;")<<endl;
  if(p.mem)  p.mem  = boost::static_pointer_cast<MemLocObject>(UnionMemLocObject::create(p.mem));
  //if(p.expr) p.expr = boost::static_pointer_cast<MemLocObject>(UnionMemLocObject::create(p.expr));
  return p;
}

MemLocObjectPtrPair ChainComposer::Expr2MemLoc_ex(SgNode* n, PartEdgePtr pedge, ComposedAnalysis* client) { 
  // Return the pair of <object that specifies the expression temporary of n, 
  //                     object that specifies the memory location that n corresponds to>
  Expr2MemLocCaller c;
  FuncCallerArgs_Expr2Any args(n, pedge);
  MemLocObjectPtr mem = callServerAnalysisFunc<MemLocObjectPtr, FuncCallerArgs_Expr2Any>(args, client, c);
  // If mem is an expression object returned by the syntactic analysis, there is no object that
  // specifies n's memory location
  if(boost::dynamic_pointer_cast<ExprObj>(mem))
    // Return mem as n's expression object and do not return an object for n's memory location
    return MemLocObjectPtrPair(mem, NULLMemLocObject);
  // If mem actually corresponds to a location in memory 
  else
    // Generate a fresh object for n's expression temporary and return it along with mem
    return MemLocObjectPtrPair(
              isSgExpression(n) && !isSgVarRefExp(n) ? 
                createExpressionMemLocObject(isSgExpression(n), isSgExpression(n)->get_type(), pedge) :
                NULLMemLocObject,
              mem);
}

// Variant of Expr2MemLoc that inquires about the memory location denoted by the operand of the given node n, where
// the part denotes the set of prefixes that terminate at SgNode n.
MemLocObjectPtrPair ChainComposer::OperandExpr2MemLoc(SgNode* n, SgNode* operand, PartEdgePtr pedge, ComposedAnalysis* client)
{
  Dbg::dbg << "ChainComposer::OperandExpr2MemLoc()"<<endl << "n="<<cfgUtils::SgNode2Str(n)<<endl << "operand("<<operand<<")="<<cfgUtils::SgNode2Str(operand)<<endl << "pedge="<<pedge->str()<<endl;
  
  // Get the parts of the execution prefixes that terminate at the operand before continuing directly 
  // to SgNode n in the given part
  list<PartEdgePtr> opPartEdges = pedge->getOperandPartEdge(n, operand);
  Dbg::dbg << "opPartEdges(#"<<opPartEdges.size()<<")="<<endl;
  for(list<PartEdgePtr>::iterator opE=opPartEdges.begin(); opE!=opPartEdges.end(); opE++) {
    Dbg::indent ind;
    Dbg::dbg << (*opE)->str()<<endl;
  }
  
  // The memory and expression MemLocObjects that represent the operand within different Parts in opParts
  //list<MemLocObjectPtr> partMLsExpr; 
  MemLocObjectPtr partMLsExpr = NULLMemLocObject;
  list<MemLocObjectPtr> partMLsMem;
  
  // Flags that indicate whether we have memory and expression objects from all/none of the sub-parts
  // Exactly One of these must be true
  bool expr4All=true, expr4None=true;
  bool mem4All=true,  mem4None=true;
  
  // Iterate over all the part edges to get the expression and memory MemLocObjects for operand within those parts
  for(list<PartEdgePtr>::iterator opE=opPartEdges.begin(); opE!=opPartEdges.end(); opE++) {
    MemLocObjectPtrPair p = Expr2MemLoc_ex(operand, *opE, client);
    if(!p.expr) expr4All=false;
    else        expr4None=false;
    
    if(!p.mem) mem4All=false;
    else       mem4None=false;
    
    //if(p.expr) partMLsExpr.push_back(p.expr);
    if(p.expr) {
      // All expression objects must be the same and we record the first one we see in partMLsExpr
      if(partMLsExpr) ROSE_ASSERT(partMLsExpr == p.expr);
      else partMLsExpr = p.expr;
    }
    if(p.mem)  partMLsMem.push_back(p.mem);
  }
  
  // Either we got expression/memory MemLocObjects from all parts or none of them
  ROSE_ASSERT((expr4All && !expr4None) || (!expr4All && expr4None));
  ROSE_ASSERT((mem4All  && !mem4None)  || (!mem4All  && mem4None));
  
  // Create a MemLocObjectPtrPair that includes UnionMemLocObjects that combine all the expression and memory
  // MemLocObjects from all the Parts that terminate at operand, using the Null MemLocObjectPtr if either
  // the expression or the memory MemLocObjects were not provided.
  if(expr4All && mem4All) 
    return MemLocObjectPtrPair(partMLsExpr, // boost::static_pointer_cast<MemLocObject>(UnionMemLocObject::create(partMLsExpr)),
                               boost::static_pointer_cast<MemLocObject>(UnionMemLocObject::create(partMLsMem)));
  else if(expr4All)
    return MemLocObjectPtrPair(partMLsExpr, // boost::static_pointer_cast<MemLocObject>(UnionMemLocObject::create(partMLsExpr)),
                               NULLMemLocObject);
  else if(mem4All)
    return MemLocObjectPtrPair(NULLMemLocObject,
                               boost::static_pointer_cast<MemLocObject>(UnionMemLocObject::create(partMLsMem)));
  // We must get either an expression or a memory object
  else
    ROSE_ASSERT(0);
  
  /*// Find the Partition(s) that correspond to the given operand of n
  std::vector<PartEdgePtr> in=part->inEdges();
  list<PartPtr> opParts;
  //for(std::vector<PartEdgePtr>::iterator e=in.begin(); e!=in.end(); e++) {
  // Walk backwards through the partition graph until we reach the Part that includes the operand
  // GB 2012-09-24: Note that there may be multiple such parts and right now we're only reaching the first one.
  //                To fully support this we need to integrate partitions into SgNodes to create a fixed mapping 
  //                between SgNodes and their containing Parts.
  back_partIterator curPart(part); curPart++;
  for(; curPart!=back_partIterator::end(); curPart++) {
    Dbg::indent ind;
    Dbg::dbg << "curPart="<<(*curPart)->str()<<endl;
    std::vector<CFGNode> nodes = (*curPart)->CFGNodes();
    Dbg::indent ind2;
    // Look to see if any of the CFGNodes within this source Part include this operand
    for(std::vector<CFGNode>::iterator node=nodes.begin(); node!=nodes.end(); node++) {
      Dbg::dbg << "node("<<node->getNode()<<")="<<cfgUtils::CFGNode2Str(*node)<<endl;
      // If so, record it in opParts
      if(node->getNode()==operand) opParts.push_back(*curPart);
    }
    // If we've reached the part that includes the operand, we're done with the search
    if(opParts.size()>0) { break; }
  }
  if(opParts.size()==0) { Dbg::dbg << "Empty opParts."<<endl; }
  ROSE_ASSERT(opParts.size()>=1);
  // We currently can only deal with the case where the operand appears in one source Part. To support the general
  // case we need to implement support for intersections of analyses, where any query to multiple analyses comes back
  // with the tightest result returned by any of them.
  ROSE_ASSERT(opParts.size()==1);
  PartPtr opPart = *opParts.begin();
  
  return Expr2MemLoc(operand, opPart, client);*/
}

// --- CallingExpr2CodeLoc ---

class Expr2CodeLocCaller : public FuncCaller<CodeLocObjectPtr, const FuncCallerArgs_Expr2Any>
{
  public:
  // Calls the given analysis' implementation of Expr2CodeLoc within the given node
  CodeLocObjectPtr operator()(const FuncCallerArgs_Expr2Any& args, ComposedAnalysis* client)
  {
    try{
      //FuncCallerArgs_Expr2Any& a = dynamic_cast<FuncCallerArgs_Expr2Any&>(args);
      return client->Expr2CodeLoc(args.n, args.pedge);
    } catch (std::bad_cast bc) { ROSE_ASSERT(false); }
  }
  
  string funcName() const{ return "Expr2CodeLoc"; }
};


CodeLocObjectPtrPair ChainComposer::Expr2CodeLoc(SgNode* n, PartEdgePtr pedge, ComposedAnalysis* client) { 
  // Return the pair of <object that specifies the expression temporary of n, 
  //                     object that specifies the memory location that n corresponds to>
  // GB: !!! Right now we don't have a firm idea of how to manage CodeLocObjects and have not yet implemented
  //     !!! an ExprObj for them. When we have done so, this code will likely mirror the code for Expr2MemLoc.
  Expr2CodeLocCaller c;
  FuncCallerArgs_Expr2Any args(n, pedge);
  return CodeLocObjectPtrPair(boost::make_shared<StxCodeLocObject>(n, pedge),
                              callServerAnalysisFunc<CodeLocObjectPtr, FuncCallerArgs_Expr2Any>(args, client, c));
}


// --------------------------------
// --- Calling GetFunction*Part ---
// --------------------------------

// Contains the arguments needed by GetFunction*Part calls
/*class FuncCallerArgs_GetFunctionAnyPart
{
  public:
  Function& func;
  
  FuncCallerArgs_GetFunctionAnyPart(Function& func) : func(func) {}
  
  std::string str(std::string indent="") {
    ostringstream oss;
    oss << func.get_name()<<"()";
    return oss.str();
  }
};*/

// --- Calling GetFunctionStartPart ---
class GetFunctionStartPartCaller : public FuncCaller<PartPtr, const Function>
{
  public:
  // Calls the given analysis' implementation of GetFunctionStartPart within the given node
  PartPtr operator()(const Function& func, ComposedAnalysis* client)
  {
    return client->GetFunctionStartPart(func);
  }
  string funcName() const{ return "GetFunctionStartPart"; }
};

PartPtr ChainComposer::GetFunctionStartPart(const Function& func, ComposedAnalysis* client) { 
  GetFunctionStartPartCaller c;
  return callServerAnalysisFunc<PartPtr, const Function>(func, client, c);
}


// --- Calling GetFunctionEndPart ---
class GetFunctionEndPartCaller : public FuncCaller<PartPtr, const Function>
{
  public:
  // Calls the given analysis' implementation of GetFunctionEndPart within the given node
  PartPtr operator()(const Function& func, ComposedAnalysis* client)
  {
    return client->GetFunctionEndPart(func);
  }
  string funcName() const{ return "GetFunctionEndPart"; }
};

PartPtr ChainComposer::GetFunctionEndPart(const Function& func, ComposedAnalysis* client) { 
  GetFunctionEndPartCaller c;
  return callServerAnalysisFunc<PartPtr, const Function>(func, client, c);
}



// Maps and Sets 
// (when analyses can implement these internally, these functions will also invoke 
//  callServerAnalysisFunc)
/*ValueSet* ChainComposer::NewValueSet()  { return new ValueSet(); }
ValueMap* ChainComposer::NewValueMap()  { return new ValueMap(); }

MemLocSet* ChainComposer::NewMemLocSet() { return new MemLocSet(); }
MemLocMap* ChainComposer::NewMemLocMap() { return new MemLocMap(); }

CodeLocSet* ChainComposer::NewCodeLocSet() { return new CodeLocSet(); }
CodeLocMap* ChainComposer::NewCodeLocMap() { return new CodeLocMap(); }*/

// ------------------------------
// ----- Composition Driver -----
// ------------------------------
  
ChainComposer::ChainComposer(int argc, char** argv, list<ComposedAnalysis*>& analyses, 
                             ComposedAnalysis* testAnalysis, bool verboseTest, SgProject* project) : 
    allAnalyses(analyses), testAnalysis(testAnalysis), verboseTest(verboseTest)
{
  //cout << "#allAnalyses="<<allAnalyses.size()<<endl;
  // Create an instance of the syntactic analysis and insert it at the front of the done list.
  // This analysis will be called last if matching functions do not exist in any other
  // analysis and does not need a round of fixed-point iteration to produce its results.
  doneAnalyses.push_front((ComposedAnalysis*)new SyntacticAnalysis());
  
  // Inform each analysis of the composer's identity
  //cout << "#allAnalyses="<<allAnalyses.size()<<endl;
  for(list<ComposedAnalysis*>::iterator a=allAnalyses.begin(); a!=allAnalyses.end(); a++)
    (*a)->setComposer(this);
  if(testAnalysis) testAnalysis->setComposer(this);

  // If a project object is provided, store it. Otherwise, run the front end now.  
  if(project) this->project = project;
  else        this->project = frontend(argc,argv);

  initAnalysis(project);
  Dbg::init("Composed Analysis", ".", "index.html");
  
  composerDebugLevel = 1;
}

void ChainComposer::runAnalysis()
//void ChainComposer::runAnalysis(int argc, char** argv)
{
  CallGraphBuilder cgb(project);
  cgb.buildCallGraph();
  SgIncidenceDirectedGraph* graph = cgb.getGraph();
  
  int i=1;
  for(list<ComposedAnalysis*>::iterator a=allAnalyses.begin(); a!=allAnalyses.end(); a++, i++) {
    ostringstream label; label << "Running Analysis "<<i<<": "<<(*a)->str("");
    Dbg::region reg(composerDebugLevel, 1, Dbg::region::topLevel, label.str());
    
    ContextInsensitiveInterProceduralDataflow inter_cc(*a, graph);
    inter_cc.runAnalysis();
    /*UnstructuredPassInterDataflow up_cc(*a);
    up_cc.runAnalysis();*/
    
    if(composerDebugLevel >= 3) {
      Dbg::region reg(composerDebugLevel, 3, Dbg::region::topLevel, label.str()+" Final State");
      std::vector<int> factNames;
      std::vector<int> latticeNames;
      latticeNames.push_back(0);
      printAnalysisStates paa(*a, factNames, latticeNames, printAnalysisStates::below);
      UnstructuredPassInterAnalysis up_paa(paa);
      up_paa.runAnalysis();
    }
    
    // Record that we've completed the given analysis
    doneAnalyses.push_back(*a);
  }
  
  if(testAnalysis) {
    //UnstructuredPassInterDataflow inter_up(testAnalysis);
    ContextInsensitiveInterProceduralDataflow inter_up(testAnalysis, graph);
    inter_up.runAnalysis();
  }
  
  return;
}
 
// Generic function that looks up the composition chain from the given client 
// analysis and returns the result produced by the first instance of the function 
// called by the caller object found along the way.
template<class RetObject, class FuncCallerArgs>
RetObject ChainComposer::callServerAnalysisFunc(FuncCallerArgs& args, ComposedAnalysis* client,
                                   FuncCaller<RetObject, const FuncCallerArgs>& caller) {
  if(composerDebugLevel>=2) Dbg::dbg << "ChainComposer::callServerAnalysisFunc() "<<caller.funcName()<<" #doneAnalyses="<<doneAnalyses.size()<<endl;
  ROSE_ASSERT(doneAnalyses.size()>0);
  /*for(list<ComposedAnalysis*>::reverse_iterator a=doneAnalyses.rbegin(); a!=doneAnalyses.rend(); a++) {
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<(*a)->str("")<<" : "<<(*a)<<endl;
  }*/
  list<ComposedAnalysis*> doneAnalyses_back;
  // Iterate backwards looking for an analysis that implements caller() behind in the chain of completed analyses
  list<ComposedAnalysis*>::reverse_iterator a=doneAnalyses.rbegin();
  while(doneAnalyses.size() >= 0) {
    std::ostringstream label; label << caller.funcName() << "  : " << (*a)->str("") << " " << args.str();
    Dbg::region reg(composerDebugLevel, 1, Dbg::region::topLevel, label.str());
    ComposedAnalysis* curAnalysis = *a;
    // Move the current analysis from doneAnalyses onto a backup list to ensure that in recursive calls
    // to callServerAnalysisFunc() doneAnalyses excludes the current analysis. doneAnalyses will be restored
    // at the end of this function.
    doneAnalyses_back.push_front(curAnalysis);
    doneAnalyses.pop_back();
    a=doneAnalyses.rbegin();
    
    /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;Post-pop doneAnalysis="<<endl;
    for(list<ComposedAnalysis*>::iterator a=doneAnalyses.begin(); a!=doneAnalyses.end(); a++)
    { Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<(*a)->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl; }
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;Post-pop doneAnalyses_back="<<endl;
    for(list<ComposedAnalysis*>::iterator a=doneAnalyses_back.begin(); a!=doneAnalyses_back.end(); a++)
    { Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<(*a)->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl; }
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;Post-pop a="<<(a!=doneAnalyses.rend()?(*a)->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"):"END")<<endl;*/
    
    try {
      //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;In TRY, caller="<<caller.funcName()<<endl;
      RetObject v(caller(args, curAnalysis));
      ROSE_ASSERT(v);
      // If control reaches here, we know that the current analysis does 
      // implement this method, so reconstruct doneAnalyses and return its reply

      /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;Pre-splice doneAnalysis="<<endl;
        for(list<ComposedAnalysis*>::iterator a=doneAnalyses.begin(); a!=doneAnalyses.end(); a++)
        Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<(*a)->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
        Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;Pre-splice doneAnalyses_back="<<endl;
        for(list<ComposedAnalysis*>::iterator a=doneAnalyses_back.begin(); a!=doneAnalyses_back.end(); a++)
        { Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<(*a)->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl; }
        Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;Pre-splice a="<<(a!=doneAnalyses.rend()?(*a)->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"):"END")<<endl;*/
      
      // Restore doneAnalyses by pushing back all the analyses that were removed for the sake of recursive
      // calls to callServerAnalysisFunc().
      doneAnalyses.splice(doneAnalyses.end(), doneAnalyses_back);
      
      /*Dbg::dbg << "Final State of doneAnalysis="<<endl;
      for(list<ComposedAnalysis*>::iterator a=doneAnalyses.begin(); a!=doneAnalyses.end(); a++)
        Dbg::dbg << "    "<<(*a)->str("        ")<<endl;*/
      if(composerDebugLevel>=1) {
        /*if(boost::dynamic_pointer_cast<AbstractObject>(v)) {
          ROSE_ASSERT(reinterpret_cast<const FuncCallerArgs_Expr2Any*>(&args));
          Dbg::dbg << "Returning "<<boost::dynamic_pointer_cast<AbstractObject>(v)->
                strp(reinterpret_cast<const FuncCallerArgs_Expr2Any*>(&args)->part, "")<<endl;
        } else*/ Dbg::dbg << "Returning "<<v.get()->str("")<<endl;
      }
      return v;
    } catch (NotImplementedException exc) {
      if(composerDebugLevel>=1) Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<caller.funcName()<<" Not Implemented. Advancing to "<<(*a)->str("")<<endl;
      // If control reaches here then the current analysis must not implement 
      // this method so we keep looking further back in the chain
      continue;
    }
  }
  
  // The first analysis in the chain must implement every optional method so 
  // control should never reach this point
  cerr << "ERROR: no analysis implements method "<<caller.funcName()<<"(SgExpression)";
  ROSE_ASSERT(0);
}

/****************************
 ***** ComposedAnalysis *****
 ****************************/

std::map<PartEdgePtr, std::vector<Lattice*> > IntraUndirDataflow::emptyMap;

// Generates the initial lattice state for the given dataflow node, in the given function. Implementations 
// fill in the lattices above and below this part, as well as the facts, as needed. Since in many cases
// the lattices above and below each node are the same, implementors can alternately implement the 
// genInitLattice and genInitFact functions, which are called by the default implementation of initializeState.
void ComposedAnalysis::initializeState(const Function& func, PartPtr part, NodeState& state)
{
  if(getDirection()==none) return;
  
  // Analyses associate all arriving information with a single NULL edge and all departing information
  // with the edge on which the information departs
  if(getDirection()==fw) {
    std::vector<Lattice*> lats;
    genInitLattice(func, part, part->inEdgeFromAny(), lats);
    state.setLatticeAbove(this, lats);
  } else if(getDirection()==bw) {
    std::vector<Lattice*> lats;
    genInitLattice(func, part, part->outEdgeToAny(), lats);
    state.setLatticeBelow(this, lats);
  }
  
  // Don't initialize the departing informaiton. This will be set by ComposedAnalysis::runAnalysis() when
  // it first touches the part
  /*vector<PartEdgePtr> edges = part->outEdges();
  for(vector<PartEdgePtr>::iterator e=edges.begin(); e!=edges.end(); e++) {
    std::vector<Lattice*> lats;
    genInitLattice(func, part, *e, lats);

    if(getDirection()==fw)      state.setLatticeBelow(this, *e, lats);
    else if(getDirection()==bw) state.setLatticeAbove(this, *e, lats);
  }*/
  
  vector<NodeFact*> initFacts;
  genInitFact(func, part, initFacts);
  state.setFacts(this, initFacts);
}


/******************************************************
 ***      printDataflowInfoPass         ***
 *** Prints out the dataflow information associated ***
 *** with a given analysis for every CFG node a     ***
 *** function.              ***
 ******************************************************/

// Initializes the state of analysis lattices at the given function, part and edge into our out of the part
// by setting initLattices to refer to freshly-allocated Lattice objects.
void printDataflowInfoPass::genInitLattice(const Function& func, PartPtr part, PartEdgePtr pedge, 
                                           std::vector<Lattice*>& initLattices)
{
  initLattices.push_back((Lattice*)(new BoolAndLattice(0, pedge)));
}
  
bool printDataflowInfoPass::transfer(const Function& func, PartPtr part, CFGNode cn, NodeState& state, 
                                     std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo)
{
  Dbg::dbg << "-----#############################--------\n";
  Dbg::dbg << "Node: ["<<part->str()<<"\n";
  Dbg::dbg << "State:\n";
  Dbg::indent ind(analysisDebugLevel, 1); 
  Dbg::dbg << state.str(analysis)<<endl;
  
  return dynamic_cast<BoolAndLattice*>(dfInfo[NULLPartEdge][0])->set(true);
}

/***************************************************
 ***            checkDataflowInfoPass            ***
 *** Checks the results of the composed analysis ***
 *** chain at special assert calls.              ***
 ***************************************************/

// Initializes the state of analysis lattices at the given function, part and edge into our out of the part
// by setting initLattices to refer to freshly-allocated Lattice objects.
void checkDataflowInfoPass::genInitLattice(const Function& func, PartPtr part, PartEdgePtr pedge, 
                                           std::vector<Lattice*>& initLattices)
{
  initLattices.push_back((Lattice*)(new BoolAndLattice(0, pedge)));
}
  
bool checkDataflowInfoPass::transfer(const Function& func, PartPtr part, CFGNode cn, NodeState& state, 
                                     std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo)
{
  set<CFGNode> nodes = part->CFGNodes();
  for(set<CFGNode>::iterator n=nodes.begin(); n!=nodes.end(); n++) {
    if(SgFunctionCallExp* call = isSgFunctionCallExp(n->getNode())) {
      Function func(call);
      if(func.get_name().getString() == "CompDebugAssert") {
        SgExpressionPtrList args = call->get_args()->get_expressions();
        for(SgExpressionPtrList::iterator a=args.begin(); a!=args.end(); a++) {
          ValueObjectPtr v = getComposer()->OperandExpr2Val(call, *a, part->inEdgeFromAny(), this);
          ostringstream errorMesg;
          if(!v->isConcrete())
            errorMesg << "Debug assertion at "<<call->get_file_info()->get_filenameString()<<":"<<call->get_file_info()->get_line()<<" failed: concrete interpretation not available! test="<<(*a)->unparseToString()<<" v="<<v->str();
          else if(!ValueObject::isValueBoolCompatible(v->getConcreteValue()))
            errorMesg << "Debug assertion at "<<call->get_file_info()->get_filenameString()<<":"<<call->get_file_info()->get_line()<<" failed: interpretation not convertible to a boolean! test="<<(*a)->unparseToString()<<" v="<<v->str()<<" v->getConcreteValue()="<<cfgUtils::SgNode2Str(v->getConcreteValue().get());
          else if(!ValueObject::SgValue2Bool(v->getConcreteValue())) 
            errorMesg << "Debug assertion at "<<call->get_file_info()->get_filenameString()<<":"<<call->get_file_info()->get_line()<<" failed: test evaluates to false! test="<<(*a)->unparseToString()<<" v="<<v->str()<<" v->getConcreteValue()="<<cfgUtils::SgNode2Str(v->getConcreteValue().get());
          
          if(errorMesg.str() != "") {
            cerr << errorMesg.str() << endl;
            Dbg::dbg << "<h1><font color=\"#ff0000\">"<<errorMesg.str()<<"</font></h1>"<<endl;
            numErrors++;
          }
        }
      }
    }
  }
  
  return dynamic_cast<BoolAndLattice*>(dfInfo[NULLPartEdge][0])->set(true);
}

}; //namespace dataflow;
