#ifndef COMPOSE_H
#define COMPOSE_H

#include "rose.h"
#include "dataflow.h"
#include "abstract_object.h"
#include "partitions.h"
#include "partitionIterator.h"
/* GB 2012-09-02: DESIGN NOTE
   Analyses and abstract objects written for the compositional framework must support a wide range of functionality,
   both mandatory (e.g. mayEqual/mustEqual in AbstractObjects) and optional (e.g. Expr2MemLoc() in ComposedAnalysis).
   In general optional functionality should go into the composer, which will then find an implementor for this 
 * functionality. Mandatory functionality should be placed as body-less virtual methods that are required for specific
 * instances of AbstractObject and ComposedAnalysis. Note that although we do currently have a way for ComposedAnalyses
 * to provide optional functionality and have the Composer find it, the same is not true for AbstractObjects because
 * thus far we have not found a use-case where we wanted AbstractObject functionality to be optional. This issue
 * may need to be revisited.
*/

// ------------------------------
// ----- Composition Driver -----
// ------------------------------

namespace dataflow {
extern int composerDebugLevel;
class Composer;

// --------------------
// ----- Analyses -----
// --------------------
class FunctionState;
  
class NotImplementedException
{};

// Represents the state of our knowledge about some fact
typedef enum {Unknown=-1, False=0, True=1} knowledgeT;

// #####################################
// ##### INTRA-PROCEDURAL ANALYSES #####
// #####################################

class ComposedAnalysis : public virtual IntraUnitDataflow, public printable
{
  public:
  Composer* composer;
  
  // Informs this analysis about the identity of the Composer object that composes
  // this analysis with others
  void setComposer(Composer* composer)
  {
    this->composer = composer;
  }
  
  Composer* getComposer()
  {
    return composer;
  }
  
  public:
    
  // Runs the analysis, combining the intra-analysis with the inter-analysis of its choice
  virtual void runAnalysis();
  
  // The transfer function for this analysis
  //GOAL: virtual void transfer(SgNode &n, Part& p)=0;
  //LEGACY: virtual bool transfer(const Function& func, const PartPtr p, NodeState& state, const std::vector<Lattice*>& dfInfo)=0;
  
  // Abstract interpretation functions that return this analysis' abstractions that 
  // represent the outcome of the given SgExpression. The default implementations of 
  // these throw NotImplementedException so that if a derived class does not implement 
  // any of these functions, the Composer is informed.
  //
  // The objects returned by these functions are expected to be deallocated by their callers.
  virtual ValueObjectPtr   Expr2Val    (SgNode* n, PartEdgePtr pedge)  { throw NotImplementedException(); }
  virtual MemLocObjectPtr  Expr2MemLoc (SgNode* n, PartEdgePtr pedge) { throw NotImplementedException(); }
  virtual CodeLocObjectPtr Expr2CodeLoc(SgNode* n, PartEdgePtr pedge)  { throw NotImplementedException(); }
  
  private:
  // Cached copies of the results of GetFunctionStartPart and GetFunctionEndPart
  map<Function, PartPtr> func2StartPart;
  map<Function, PartPtr> func2EndPart;  
  
  public:
  // Return the anchor Parts of a given function
  PartPtr GetFunctionStartPart(const Function& func);
  PartPtr GetFunctionEndPart(const Function& func);
  
  // Specific Composers implement these two functions
  virtual PartPtr GetFunctionStartPart_Spec(const Function& func) { throw NotImplementedException(); }
  virtual PartPtr GetFunctionEndPart_Spec(const Function& func)   { throw NotImplementedException(); }
  
  // In the long term we will want analyses to return their own implementations of 
  // maps and sets. This is not strictly required to produce correct code and is 
  // therefore not supported.
  // Maps and Sets 
  /*virtual ValueSet* NewValueSet()  { throw NotImplementedException; }
  virtual ValueMap* NewValueMap()  { throw NotImplementedException; }
  
  virtual MemLocSet* NewMemLocSet() { throw NotImplementedException; }
  virtual MemLocMap* NewMemLocMap() { throw NotImplementedException; }
  
  virtual CodeLocSet* NewCodeLocSet() { throw NotImplementedException; }
  virtual CodeLocMap* NewCodeLocMap() { throw NotImplementedException; }*/

  // GB 2012-09-19 : As far as I can tell, all ComposedAnalyses will be either forward, backward or undirected, so
  //                 ComposedAnalysis and IntraUniDirectionalDataflow have been fused.
/*};

// Base class of Uni-directional (Forward or Backward) Intra-Procedural Dataflow Analyses
class IntraUniDirectionalDataflow : public ComposedAnalysis
{*/
  public:
  typedef enum {fw=0, bw=1, none=2} direction;
  
  // Runs the intra-procedural analysis on the given function and returns true if
  // the function's NodeState gets modified as a result and false otherwise
  // state - the function's NodeState
  // analyzeFromDirectionStart - If true the function should be analyzed from its starting point from the analysis' 
  //    perspective (fw: entry point, bw: exit point)
  void runAnalysis(const Function& func, NodeState* state, bool analyzeFromDirectionStart, std::set<Function> calleesUpdated);
  
  // Generates the initial lattice state for the given dataflow node, in the given function. Implementations 
  // fill in the lattices above and below this part, as well as the facts, as needed. Since in many cases
  // the lattices above and below each node are the same, implementors can alternately implement the 
  // genInitLattice and genInitFact functions, which are called by the default implementation of initializeState.
  virtual void initializeState(const Function& func, PartPtr part, NodeState& state);
  
  // Initializes the state of analysis lattices at the given function, part and edge into our out of the part
  // by setting initLattices to refer to freshly-allocated Lattice objects.
  virtual void genInitLattice(const Function& func, PartPtr part, PartEdgePtr pedge, 
                             std::vector<Lattice*>& initLattices) {}
  
  // Initializes the state of analysis facts at the given function and part by setting initFacts to 
  // freshly-allocated Fact objects.
  virtual void genInitFact(const Function& func, PartPtr part, std::vector<NodeFact*>& initFacts) {}
  
  // propagates the dataflow info from the current node's NodeState (curNodeState) to the next node's
  // NodeState (nextNodeState)
  bool propagateStateToNextNode(
              std::map<PartEdgePtr, std::vector<Lattice*> >& curNodeState, PartPtr curDFNode,
              std::map<PartEdgePtr, std::vector<Lattice*> >& nextNodeState, PartPtr nextDFNode);

  virtual NodeState*initializeFunctionNodeState(const Function &func, NodeState *fState) = 0;
  virtual std::list<PartPtr>
    getInitialWorklist(const Function &func, bool analyzeFromDirectionStart, const set<Function> &calleesUpdated, NodeState *fState) = 0;
  virtual std::map<PartEdgePtr, std::vector<Lattice*> >& getLatticeAnte(NodeState *state) = 0;
  virtual std::map<PartEdgePtr, std::vector<Lattice*> >& getLatticePost(NodeState *state) = 0;
  virtual void setLatticeAnte(NodeState *state, std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo, bool overwrite) = 0;
  virtual void setLatticePost(NodeState *state, std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo, bool overwrite) = 0;

  // If we're currently at a function call, use the associated inter-procedural
  // analysis to determine the effect of this function call on the dataflow state.
  //virtual void transferFunctionCall(const Function &caller, PartPtr callPart, CFGNode callCFG, NodeState *state) = 0;

  virtual list<PartPtr> getDescendants(PartPtr p) = 0;
  virtual list<PartEdgePtr> getEdgesToDescendants(PartPtr part) = 0;
  virtual PartPtr getUltimate(const Function &func) = 0;
  virtual dataflowPartIterator* getIterator(const Function &func) = 0;
  
  virtual direction getDirection() = 0;
};

/* Forward Intra-Procedural Dataflow Analysis */
class IntraFWDataflow  : public ComposedAnalysis
{
  public:
  
  IntraFWDataflow()
  {}
  
  NodeState* initializeFunctionNodeState(const Function &func, NodeState *fState);
  std::list<PartPtr>
    getInitialWorklist(const Function &func/*, bool firstVisit*/, bool analyzeDueToCallers, const set<Function> &calleesUpdated, NodeState *fState);
  std::map<PartEdgePtr, std::vector<Lattice*> >& getLatticeAnte(NodeState *state);
  std::map<PartEdgePtr, std::vector<Lattice*> >& getLatticePost(NodeState *state);
  void setLatticeAnte(NodeState *state, std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo, bool overwrite);
  void setLatticePost(NodeState *state, std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo, bool overwrite);
  
  //void transferFunctionCall(const Function &func, PartPtr callPart, CFGNode callCFG, NodeState *state);
  list<PartPtr> getDescendants(PartPtr p);
  list<PartEdgePtr> getEdgesToDescendants(PartPtr part);
  PartPtr getUltimate(const Function &func);
  dataflowPartIterator* getIterator(const Function &func);
  
  direction getDirection() { return fw; }
};

/* Backward Intra-Procedural Dataflow Analysis */
class IntraBWDataflow  : public ComposedAnalysis
{
  public:
  
  IntraBWDataflow()
  {}
  
  NodeState* initializeFunctionNodeState(const Function &func, NodeState *fState);
  std::list<PartPtr>
    getInitialWorklist(const Function &func/*, bool firstVisit*/, bool analyzeDueToCallers, const set<Function> &calleesUpdated, NodeState *fState);
  std::map<PartEdgePtr, std::vector<Lattice*> >& getLatticeAnte(NodeState *state);
  std::map<PartEdgePtr, std::vector<Lattice*> >& getLatticePost(NodeState *state);
  void setLatticeAnte(NodeState *state, std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo, bool overwrite);
  void setLatticePost(NodeState *state, std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo, bool overwrite);
  //void transferFunctionCall(const Function &func, PartPtr callPart, CFGNode callCFG, NodeState *state);
  list<PartPtr> getDescendants(PartPtr p);
  list<PartEdgePtr> getEdgesToDescendants(PartPtr part);
  PartPtr getUltimate(const Function &func);
  dataflowPartIterator* getIterator(const Function &func);
  
  direction getDirection() { return bw; }
};


/* Dummy Intra-Procedural Dataflow Analysis that doesn't have a direction but is still a compositional
   analysis (e.g. Syntactic analysis or orthogonal array analysis)*/
class IntraUndirDataflow  : public ComposedAnalysis
{
  public:
  
  IntraUndirDataflow()
  {}
  
  NodeState* initializeFunctionNodeState(const Function &func, NodeState *fState) { return NULL; }
  std::list<PartPtr>
    getInitialWorklist(const Function &func/*, bool firstVisit*/, bool analyzeDueToCallers, const set<Function> &calleesUpdated, NodeState *fState) { std::list<PartPtr> empty; return empty; } //return NULL; }
  static std::map<PartEdgePtr, std::vector<Lattice*> > emptyMap;
  std::map<PartEdgePtr, std::vector<Lattice*> >& getLatticeAnte(NodeState *state) { return emptyMap; }
  std::map<PartEdgePtr, std::vector<Lattice*> >& getLatticePost(NodeState *state) { return emptyMap; }
  void setLatticeAnte(NodeState *state, std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo, bool overwrite) { }
  void setLatticePost(NodeState *state, std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo, bool overwrite) { }
  //void transferFunctionCall(const Function &func, PartPtr callPart, CFGNode callCFG, NodeState *state) {};
  list<PartPtr> getDescendants(PartPtr p) { list<PartPtr> empty; return empty; }
  list<PartEdgePtr> getEdgesToDescendants(PartPtr part) { list<PartEdgePtr> empty; return empty; }
  PartPtr getUltimate(const Function &func) { return NULLPart; } 
  dataflowPartIterator* getIterator(const Function &func) { return NULL; }
  
  direction getDirection() { return none; }
  
  // Dummy transfer function since undirected analyses does not propagate flow information
  bool transfer(const Function& func, PartPtr p, CFGNode cn, NodeState& state, std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo) {
    return true;
  }
};

// #####################################
// ##### INTER-PROCEDURAL ANALYSES #####
// #####################################

class InterProceduralDataflow : virtual public InterProceduralAnalysis
{
  public:
  InterProceduralDataflow(ComposedAnalysis* intraDataflowAnalysis);

  // the transfer function that is applied to SgFunctionCallExp nodes to perform the appropriate state transfers
  // fw - =true if this is a forward analysis and =false if this is a backward analysis
  // n - the dataflow node that is being processed
  // state - the NodeState object that describes the dataflow state immediately before (if fw=true) or immediately after
  //         (if fw=false) the SgFunctionCallExp node, as established by earlier analysis passes
  // dfInfo - The Lattices that this transfer function operates on. The function propagates them
  //          to the calling function and overwrites them with the dataflow result of calling this function.
  //          At the time of the call the dfInfo map has only one key: PartEdgePtr(), which is the NULL edge.
  //          If the transfer function needs to provide different information to different successor nodes,
  //          it can replace this single key with multiple keys, one for each successor edge, that map to different
  //          Lattice* vectors. If such differentiation is not required the map's key can be left alone.
  // retState - Pointer reference to a Lattice* vector that will be assigned to point to the lattices of
  //          the function call's return value. The callee may not modify these lattices.
  // Returns true if any of the input lattices changed as a result of the transfer function and
  //    false otherwise.
  virtual bool transfer(const Function& func, PartPtr callPart, CFGNode callCFG, NodeState& state,
                        std::vector<Lattice*>& dfInfo)=0;

  // Since InterProceduralDataflow takes in only Composed intra analyses,
  // this method is a simple way to get access to the intra analysis with the right type.
  ComposedAnalysis* getIntraComposedAnalysis() { 
    ComposedAnalysis* ca = dynamic_cast<ComposedAnalysis*>(intraAnalysis);
    ROSE_ASSERT(ca);
    return ca;
  }
};

/**********************************************************************
 ***                 UnstructuredPassInterDataflow                  ***
 *** The trivial inter-procedural dataflow where a intra-procedural ***
 *** dataflow analysis is executed once on every function in the    ***
 *** application, with no guarantees about how dataflow information ***
 *** is transmitted across function calls.                          ***
 **********************************************************************/
class UnstructuredPassInterDataflow : virtual public InterProceduralDataflow
{
  protected:
  // Keeps track of the functions that have already been visited and thus initialized by this inter analysis
  std::set<Function> visited;

  public:
  UnstructuredPassInterDataflow(ComposedAnalysis* intraDataflowAnalysis)
                       : InterProceduralAnalysis((IntraProceduralAnalysis*)intraDataflowAnalysis), InterProceduralDataflow(intraDataflowAnalysis)
  {}

  // the transfer function that is applied to SgFunctionCallExp nodes to perform the appropriate state transfers
  // fw - =true if this is a forward analysis and =false if this is a backward analysis
  // n - the dataflow node that is being processed
  // state - the NodeState object that describes the dataflow state immediately before (if fw=true) or immediately after
  //         (if fw=false) the SgFunctionCallExp node, as established by earlier analysis passes
  // dfInfo - the Lattices that this transfer function operates on. The function propagates them
  //          to the calling function and overwrites them with the dataflow result of calling this function.
  //          At the time of the call the dfInfo map has only one key: PartEdgePtr(), which is the NULL edge.
  //          If the transfer function needs to provide different information to different successor nodes,
  //          it can replace this single key with multiple keys, one for each successor edge, that map to different
  //          Lattice* vectors. If such differentiation is not required the map's key can be left alone.
  // retState - Pointer reference to a Lattice* vector that will be assigned to point to the lattices of
  //          the function call's return value. The callee may not modify these lattices.
  // Returns true if any of the input lattices changed as a result of the transfer function and
  //    false otherwise.
  bool transfer(const Function& func, PartPtr callPart, CFGNode callCFG, NodeState& state,
                std::vector<Lattice*>& dfInfo)
  {
    return false;
  }

  void runAnalysis();
};

class ContextInsensitiveInterProceduralDataflow : virtual public InterProceduralDataflow, public TraverseCallGraphDataflow
{
  // list of functions that still remain to be processed
  //list<Function> remaining;

  // The functions that still remain to be processed.

  // These functions need to be processed because they are called by functions that have been processed
  // or are called at startup such as main() and the constructors of static objects.
  std::set<Function> remainingDueToCallers;

  // Each function F in this map needs to be processed because it has called other functions and those functions
  // have now been analyzed and the dataflow information at their exit points has changed since the last time
  // F was analyzed. remainingDueToCalls maps each F to all such functions. As such, F needs to be re-analyzed,
  // starting at the calls to these functions.
  std::map<Function, std::set<Function> > remainingDueToCalls;
  
  // Keeps track of the functions that have already been visited and thus initialized by this inter analysis
  std::set<Function> visited;

  public:
  ContextInsensitiveInterProceduralDataflow(ComposedAnalysis* intraDataflowAnalysis, SgIncidenceDirectedGraph* graph) ;
    
  public:

  // the transfer function that is applied to SgFunctionCallExp nodes to perform the appropriate state transfers
  // fw - =true if this is a forward analysis and =false if this is a backward analysis
  // n - the dataflow node that is being processed
  // state - the NodeState object that describes the dataflow state immediately before (if fw=true) or immediately after
  //         (if fw=false) the SgFunctionCallExp node, as established by earlier analysis passes
  // dfInfo - the Lattices that this transfer function operates on. The function propagates them
  //          to the calling function and overwrites them with the dataflow result of calling this function.
  // retState - Pointer reference to a Lattice* vector that will be assigned to point to the lattices of
  //          the function call's return value. The callee may not modify these lattices.
  // Returns true if any of the input lattices changed as a result of the transfer function and
  //    false otherwise.
  bool transfer(const Function& func, PartPtr callPart, CFGNode callCFG, NodeState& state,
                std::vector<Lattice*>& dfInfo);

  // Uses TraverseCallGraphDataflow to traverse the call graph.
  void runAnalysis();

  // Runs the intra-procedural analysis every time TraverseCallGraphDataflow passes a function.
  void visit(const CGFunction* func);
};

// #######################################
// ##### UTILITY PASSES and ANALYSES #####
// #######################################

/******************************************************
 ***            printDataflowInfoPass               ***
 *** Prints out the dataflow information associated ***
 *** with a given analysis for every CFG node a     ***
 *** function.                                      ***
 ******************************************************/
class printDataflowInfoPass : public IntraFWDataflow
{
  Analysis* analysis;

  public:
  printDataflowInfoPass(Analysis *analysis)
  {
          this->analysis = analysis;
  }

  // Initializes the state of analysis lattices, for analyses that produce the same lattices above and below each node
  void genInitLattice(const Function& func, PartPtr part, PartEdgePtr pedge, std::vector<Lattice*>& initLattices);

  bool transfer(const Function& func, PartPtr p, CFGNode cn, NodeState& state, 
                std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo);

  // pretty print for the object
  std::string str(std::string indent="")
  { return "printDataflowInfoPass"; }
};

/***************************************************
 ***            checkDataflowInfoPass            ***
 *** Checks the results of the composed analysis ***
 *** chain at special assert calls.              ***
 ***************************************************/
class checkDataflowInfoPass : public IntraFWDataflow
{
  private:
  int numErrors;

  public:
  checkDataflowInfoPass() : numErrors(0) { }

  int getNumErrors() const { return numErrors; }

  // Initializes the state of analysis lattices, for analyses that produce the same lattices above and below each node
  void genInitLattice(const Function& func, PartPtr part, PartEdgePtr pedge, std::vector<Lattice*>& initLattices);

  bool transfer(const Function& func, PartPtr p, CFGNode cn, NodeState& state, 
                std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo);

  // pretty print for the object
  std::string str(std::string indent="")
  { return "checkDataflowInfoPass"; }
};


class InitDataflowState : public UnstructuredPassIntraAnalysis
{
  ComposedAnalysis::direction dir;
  
  public:
  InitDataflowState(ComposedAnalysis* analysis, ComposedAnalysis::direction dir) : UnstructuredPassIntraAnalysis(analysis), dir(dir)
  { }

  void visit(const Function& func, PartPtr p, NodeState& state);
};

// Analysis that finds the Parts that corresponds to calls to a given set of functions
class FindAllFunctionCalls : public UnstructuredPassIntraAnalysis
{
  // The set of functions that we wish to find the calls to
  const std::set<Function>& funcsToFind;

  // Maps each function in funcsToFind to a set of Parts that hold calls to this function
  std::map<Function, std::set<PartPtr> > funcCalls;

  public:
  FindAllFunctionCalls(ComposedAnalysis* analysis, const std::set<Function>& funcsToFind) : 
    UnstructuredPassIntraAnalysis(analysis), funcsToFind(funcsToFind)
  { }

  void visit(const Function& func, PartPtr p, NodeState& state);

  // Returns a reference to funcCalls
  std::map<Function, std::set<PartPtr> >& getFuncCalls() { return funcCalls; }
};

// Analysis that merges the dataflow states belonging to the given Analysis at all the return statements in the given function
// and produces a list of merged lattices (same number of lattices as were maintained by the nodes in the function).
// NOTE: The callers of this analysis are responsible for deallocating the lattices stored in mergedLatsRetVal at the 
//       end of the analysis pass.
class MergeAllReturnStates : public UnstructuredPassIntraAnalysis
{
  // List of merged lattices of all the return statements and the returned values
  std::vector<Lattice*> mergedLatsRetVal;

  protected:
  // After the pass is complete, records true if the state of the mergedLattices changed
  // during the analysis and false otherwise
  bool modified;

  public:
  MergeAllReturnStates(ComposedAnalysis* analysis);

  MergeAllReturnStates(ComposedAnalysis* analysis, const std::vector<Lattice*>& mergedLatsRetVal);

  void visit(const Function& func, PartPtr p, NodeState& state);

  // Merges the lattices in the given vector into mergedLat, which may be mergedLatsRetStmt or mergedLatsRetVal
  // Returns true of mergedLatsStmt changes as a result and false otherwise.
  static bool mergeLats(std::vector<Lattice*>& mergedLat, const std::vector<Lattice*>& lats);

  // Returns the value of modified
  bool getModified() { return modified; }
  
  // Returns the merged dataflow information at the end of the analyzed function
  std::map<PartEdgePtr, std::vector<Lattice*> > getMergedDFInfo();

  // Deallocates all the merged lattices
  ~MergeAllReturnStates();
};

// Analysis that takes the dataflow state belonging to the given Analysis and initializes to this state all the 
//   return statements in the given function and its end. The return value at each return statement is associated
//   with the portion of the lattice mapped to the function's SgSymbol
// NOTE: This pass does not modify the Lattices in lats.
class SetAllReturnStates : public UnstructuredPassIntraAnalysis
{
  // List of lattices to assign the function's returns to 
  std::vector<Lattice*> lats;

  protected:
  // After the pass is complete, records true if the state of the lattices at function end or return 
  // statements and false otherwise
  bool modified;
  
  public:
  // Information about the function's starting node
  SgFunctionParameterList* paramList;
  NodeState* paramsState;
  //PartPtr paramsPart;
  
  public:
  SetAllReturnStates(ComposedAnalysis* analysis): UnstructuredPassIntraAnalysis(analysis) { 
    modified=false;
    paramList=NULL;
  }

  SetAllReturnStates(ComposedAnalysis* analysis, const std::vector<Lattice*>& lats) : 
          UnstructuredPassIntraAnalysis(analysis), lats(lats)
  { modified=false; }

  void visit(const Function& func, PartPtr p, NodeState& state);

  // Merges the lattices in the given vector into mergedLat, which may be mergedLatsRetStmt or mergedLatsRetVal
  // Returns true of mergedLatsStmt changes as a result and false otherwise.
  static bool mergeLats(std::vector<Lattice*>& mergedLat, const std::vector<Lattice*>& lats);

  // Returns the value of modified
  bool getModified() { return modified; }
};

// #####################
// ##### COMPOSERS #####
// #####################

// Composer that invokes multiple analyses in parallel (they do not interact) and runs them to completion independently.
// It also implements the ComposedAnalysis interface and can be used by another Composer as an analysis. Thus, when this
// Composer's constituent analyses ask a query on the composer, it merely forwards this query to its parent Composer.
// Further, when its parent Composer makes queries of it, this Composer forwards those queries to its constituent 
// analyses and returns an Intersection object that contains their responses.
class LooseParallelComposer : public Composer, public IntraUndirDataflow
{
  list<ComposedAnalysis*> allAnalyses;
  
  
  // Indicates whether at least one sub-analysis implements a partition
  knowledgeT subAnalysesImplementPartitions;
  
  public:
  LooseParallelComposer(const list<ComposedAnalysis*>& analyses);

  // ---------------------------------
  // ----- Methods from Composer -----
  // ---------------------------------
  
  // The Expr2* and GetFunction*Part functions are implemented by calling the same functions in the parent composer
  
  // Abstract interpretation functions that return this analysis' abstractions that 
  // represent the outcome of the given SgExpression. 
  // The objects returned by these functions are expected to be deallocated by their callers.
  ValueObjectPtr Expr2Val(SgNode* n, PartEdgePtr pedge, ComposedAnalysis* client);
  // Variant of Expr2Val that inquires about the value of the memory location denoted by the operand of the 
  // given node n, where the part denotes the set of prefixes that terminate at SgNode n.
  ValueObjectPtr OperandExpr2Val(SgNode* n, SgNode* operand, PartEdgePtr pedge, ComposedAnalysis* client);
  
  MemLocObjectPtrPair Expr2MemLoc(SgNode* n, PartEdgePtr pedge, ComposedAnalysis* client);
  // Variant of Expr2MemLoc that inquires about the memory location denoted by the operand of the given node n, where
  // the part denotes the set of prefixes that terminate at SgNode n.
  MemLocObjectPtrPair OperandExpr2MemLoc(SgNode* n, SgNode* operand, PartEdgePtr pedge, ComposedAnalysis* client);
  
  CodeLocObjectPtrPair Expr2CodeLoc(SgNode* n, PartEdgePtr pedge, ComposedAnalysis* client);
  
  // Return the anchor Parts of a given function
  PartPtr GetFunctionStartPart(const Function& func, ComposedAnalysis* client);
  PartPtr GetFunctionEndPart(const Function& func, ComposedAnalysis* client);
  
  // -----------------------------------------
  // ----- Methods from ComposedAnalysis -----
  // -----------------------------------------
  
  // Runs the analysis, combining the intra-analysis with the inter-analysis of its choice
  // LooseParallelComposer invokes the runAnalysis methods of all its constituent analyses in sequence
  void runAnalysis();
  
  // The Expr2* and GetFunction*Part functions are implemented by calling the same functions in each of the 
  // constituent analyses and returning an Intersection object that includes their responses
  
  // Abstract interpretation functions that return this analysis' abstractions that 
  // represent the outcome of the given SgExpression. The default implementations of 
  // these throw NotImplementedException so that if a derived class does not implement 
  // any of these functions, the Composer is informed.
  //
  // The objects returned by these functions are expected to be deallocated by their callers.
  ValueObjectPtr   Expr2Val    (SgNode* n, PartEdgePtr pedge);
  MemLocObjectPtr  Expr2MemLoc (SgNode* n, PartEdgePtr pedge);
  CodeLocObjectPtr Expr2CodeLoc(SgNode* n, PartEdgePtr pedge);
  
  // Return the anchor Parts of a given function
  PartPtr GetFunctionStartPart_Spec(const Function& func);
  PartPtr GetFunctionEndPart_Spec(const Function& func);
  
  // When Expr2* is queried for a particular analysis on edge pedge, exported by this LooseParallelComposer 
  // this function translates from the pedge that the LooseParallelComposer::Expr2* is given to the PartEdge 
  // that this particular sub-analysis runs on. If some of the analyses that were composed in parallel with 
  // this analysis (may include this analysis) implement partition graphs, we know that 
  // GetFunctionStartPart/GetFunctionEndPart wrapped them in IntersectionPartEdges. In this case this function
  // converts pedge into an IntersectionPartEdge and queries its getPartEdge method. Otherwise, 
  // GetFunctionStartPart/GetFunctionEndPart do no wrapping and thus, we can return pedge directly.
  PartEdgePtr getEdgeForAnalysis(PartEdgePtr pedge, ComposedAnalysis* analysis);
  
  std::string str(std::string indent="");
};

}; // namespace dataflow

#endif
