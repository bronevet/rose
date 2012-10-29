#define ANALYSIS_C
#include "genericDataflowCommon.h"
#include "VirtualCFGIterator.h"
#include "cfgUtils.h"
#include "CallGraphTraverse.h"
#include "analysisCommon.h"
#include "analysis.h"
#include "dataflow.h"
#include "latticeFull.h"
#include "compose.h"
#include "functionState.h"
#include "abstract_object.h"
#include "partitionIterator.h"
#include <vector>
#include <set>
#include <map>
#include <boost/make_shared.hpp>

/* GB 2012-10-23: DESIGN NOTE
 * At the start of an intra-procedural analysis of a given function the function's initial dataflow state is copied 
 * from the special NodeState from inside the function's FunctionState object to the function's starting/ending Part.
 * To make our intra analyses simple we want to analyze each Part by propagating information from before it to after it
 * (fw: from incoming to outgoing edges; bw: from outgoing to incoming edges). This means that we need to ensure that
 * it is possible to create Lattices on the incoming edge of the starting Part and the outgoing edge of the ending Part
 * of a function. This is problematic because intra analyses are run upto the starting/ending Part but do not propagate
 * information to the other side (that would mean effectively falling off the edge of the function). This makes it 
 * impossible to create Lattices on these "other-side" edges. We overcome this problem in 
 * ContextInsensitiveInterProceduralDataflow (and should do the same in other analyses) by propagating the information
 * on the outgoing edges of the starting Parts / incoming edges of the ending Part one extra step to the incoming
 * edges of the starting Parts / outgoing edges of the ending Parts.
 */

using namespace std;
namespace dataflow {
  
int analysisDebugLevel=1;

/*******************************
 *** IntraProceduralAnalysis ***
 *******************************/

void IntraProceduralAnalysis::runAnalysis(const Function& func, NodeState* state) { 
  /* GB: For some reason the compiler complains that SyntacticAnalysis doesn't implement this
         when it is implemented in its ancestor IntraProceduralDataflow. As such, 
         a dummy implementation is provided here. */
  ROSE_ASSERT(0); 
}

IntraProceduralAnalysis::~IntraProceduralAnalysis() {}

/*******************************
 *** InterProceduralAnalysis ***
 *******************************/
InterProceduralAnalysis::~InterProceduralAnalysis() {}

/*************************************
 *** UnstructuredPassIntraAnalysis ***
 *************************************/

// runs the intra-procedural analysis on the given function, returns true if 
// the function's NodeState gets modified as a result and false otherwise
// state - the function's NodeState
void UnstructuredPassIntraAnalysis::runAnalysis(const Function& func, NodeState* state)
{
  if(analysisDebugLevel>=2)
    Dbg::dbg << "UnstructuredPassIntraAnalysis::runAnalysis() function "<<func.get_name().getString()<<"()\n";
  
  // Iterate over all the nodes in this function
  for(partIterator it(analysis->getComposer()->GetFunctionStartPart(func, analysis)); it!=partIterator::end(); it++)
  {
    PartPtr p = *it;
    NodeState* state = NodeState::getNodeState(analysis, p);
    visit(func, p, *state);
  }
}

/*************************************
 *** UnstructuredPassInterAnalysis ***
 *************************************/
void UnstructuredPassInterAnalysis::runAnalysis()
{
  set<FunctionState*> allFuncs = FunctionState::getAllDefinedFuncs();
  // Go through functions one by one, call an intra-procedural analysis on each of them
  // iterate over all functions with bodies
  for(set<FunctionState*>::iterator it=allFuncs.begin(); it!=allFuncs.end(); it++)
  {
    FunctionState* fState = *it;
    intraAnalysis->runAnalysis(fState->func, &(fState->state));
  }
}

/*class Dataflow : virtual public Analysis
{

};*/

/*******************************
 *** InterProceduralDataflow ***
 *******************************/

InterProceduralDataflow::InterProceduralDataflow(ComposedAnalysis* intraDataflowAnalysis) : 
  InterProceduralAnalysis((IntraProceduralAnalysis*)intraDataflowAnalysis)
{
  set<FunctionState*> allFuncs = FunctionState::getAllDefinedFuncs();
  
  // Initialize the dataflow state of all functions with bodies
  for(set<FunctionState*>::iterator it=allFuncs.begin(); it!=allFuncs.end(); it++)  
  {
    FunctionState* funcS = *it;
    Function func = funcS->func;
    //Dbg::dbg << "func="<<func.get_name().getString()<<"() func.get_definition()="<<func.get_definition()<<endl;
    if(func.get_definition())
    {      
      if(analysisDebugLevel>=1){
        Dbg::dbg << "InterProceduralAnalysis initLats at Beginning "<<func.get_name().getString()<<"()"<<endl;
        Dbg::dbg << funcS->state.str(intraDataflowAnalysis)<<endl;
      }
      
      // Initialize the dataflow state of the current function
      PartPtr startN = getIntraComposedAnalysis()->getComposer()->GetFunctionStartPart(func, getIntraComposedAnalysis());
      intraDataflowAnalysis->initializeState(func, startN, funcS->state);
      
      // Make sure that the starting lattices are initialized
      /*for(vector<Lattice*>::iterator it=initLats.begin(); it!=initLats.end(); it++)
        (*it)->initialize();*/
      
      Dbg::dbg << "Initialized state of function "<<func.get_name().getString()<<"(), state="<<(&(funcS->state))<<endl;
      Dbg::dbg << "    "<<funcS->state.str(intraDataflowAnalysis, "    ")<<endl;
      
      // Initialize funcS->retState with initLats. 
      // !!!Need to make a copy of initLats first
      //funcS->retState.setLatticeBelow(intraAnalysis, initLats);
      
      // We do not initialize retState since we don't know the type of the lattice that will capture
      // the projection of the normal lattice type on the return variables. However, there is no
      // need to provide default lattices since DFStateAtReturns and MergeAllReturnStates can
      // deal with Lattice* vectors that start out empty.
      
      // We do not need to deallocate facts in initFacts and lattices in initLats since setFacts() and
      // setLattices() have taken the original facts in initFacts and did not make private copies of them
      
      /*const vector<Lattice*>* funcLatticesBefore = &(funcS->state.getLatticeAbove(intraAnalysis));
      Dbg::dbg << "  funcLatticesBefore->size()="<<funcLatticesBefore->size()<<", intraAnalysis="<<intraAnalysis<<endl;*/
    }
  }
}

/*************************
 *** InitDataflowState ***
 *************************/

void InitDataflowState::visit(const Function& func, PartPtr p, NodeState& state)
{
  /*ostringstream label; label << "InitDataflowState::visit() p="<<p->str()<<", analysis="<<analysis<<"="<<analysis->str()<<" state="<<&state<<endl;
  Dbg::region reg(analysisDebugLevel, 1, Dbg::region::topLevel, label.str());*/
  
  // generate a new initial state for this node
  analysis->initializeState(func, p, state);
  
  /*if(analysisDebugLevel>=2){
    int i=0;
    for(vector<Lattice*>::iterator l=initLats.begin(); l!=initLats.end(); l++, i++)
      Dbg::dbg << "Lattice "<<i<<": "<<(*l)->str("      ")<<endl;
    
    i=0;
    for(vector<NodeFact*>::iterator f=initFacts.begin(); f!=initFacts.end(); f++, i++)
      Dbg::dbg << "Lattice "<<i<<": "<<(*f)->str("      ")<<endl;
  }*/
                
  //if(analysisDebugLevel>=1) Dbg::dbg << "    state="<<state.str(analysis, "    ")<<endl;
  
  /*vector<Lattice*> initState = analysis->genInitState(func, n, state);
  Dbg::dbg << "InitDataflowState::visit() 1"<<endl;
      
  for(int i=0; i<initState.size(); i++)
  {
    Dbg::dbg << "       i="<<i<<", initState[i]="<<initState[i]->str("")<<endl;
    state.addLattice(analysis, i, initState[i]);
    Dbg::dbg << "       state->getLatticeAbove(analysis).size()="<<state.getLatticeAbove(analysis).size()<<endl, );
    //Dbg::dbg << printf("       state->getLatticeBelow(analysis).size()="<<state.getLatticeBelow(analysis).size()<<endl;
  }*/
  
  //const vector<Lattice*>& masterLatBel = state.getLatticeBelow(analysis);
  //printf("    creator=%p, state=%p, masterLatBel.size()=%d\n", analysis, &state, masterLatBel.size());
}


/****************************
 *** FindAllFunctionCalls ***
 ****************************/
 
void FindAllFunctionCalls::visit(const Function& func, PartPtr part, NodeState& state)
{
  if(analysisDebugLevel>=2) Dbg::dbg << "FindAllFunctionCalls::visit() part="<<part->str()<<endl;
  
  // If this is a function call, find the function that is being called and if it is 
  // in funcsToFind, record the call in funcCalls
  set<CFGNode> v=part->CFGNodes();
  for(set<CFGNode>::iterator c=v.begin(); c!=v.end(); c++) {
    SgNode* sgn = c->getNode();
    if(isSgFunctionCallExp(sgn)) {
      for(set<Function>::const_iterator func=funcsToFind.begin(); func!=funcsToFind.end(); func++) {
        if((*func).get_declaration() == isSgFunctionCallExp(sgn)->getAssociatedFunctionDeclaration()) {
          funcCalls[*func].insert(part);
          break;
        }
      }
    }
  }
}

/************************
 *** ComposedAnalysis ***
 ************************/


// Propagates the dataflow info from the current node's NodeState (curNodeState) to the next node's 
//     NodeState (nextNodeState).
// Returns true if the next node's meet state is modified and false otherwise.
bool ComposedAnalysis::propagateStateToNextNode(
                map<PartEdgePtr, vector<Lattice*> >& curNodeState, PartPtr curNode, 
                map<PartEdgePtr, vector<Lattice*> >& nextNodeState, PartPtr nextNode)
{
  Dbg::region reg(analysisDebugLevel, 1, Dbg::region::topLevel, "propagateStateToNextNode");
  bool modified = false;
  
  // curNodeState should have a single mapping to the NULLPartEdge
  ROSE_ASSERT(curNodeState.begin()->first == NULLPartEdge);
  
  vector<Lattice*>::const_iterator itC, itN;
  if(analysisDebugLevel>=1) {
    Dbg::dbg << endl << "Propagating to Next Node: "<<nextNode->str()<<endl;
    Dbg::dbg << "Cur Node Lattice "<<endl;
    { Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg<<NodeState::str(curNodeState); }
    
    Dbg::dbg << "Next Node Lattice "<<endl;
    { Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg<<NodeState::str(nextNodeState); }
  }

  // Update forward info above nextNode from the forward info below curNode.
  
  // Compute the meet of the dataflow information along the curNode->nextNode edge with the 
  // next node's current state one Lattice at a time and save the result above the next node.
  
  // If nextNodeState is non-empty, we union curNodeState into it
  Dbg::dbg << "----------------------"<<endl;
  if(nextNodeState.size()>0)
    modified = NodeState::unionLatticeMaps(nextNodeState, curNodeState) || modified;
  // Otherwise, we copy curNodeState[NULLPartEdge] over it
  else {
    NodeState::copyLatticesOW(nextNodeState, NULLPartEdge, curNodeState, NULLPartEdge);
    modified = true;
  }

  Dbg::dbg << "Result:"<<endl;  
  { Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg<<NodeState::str(nextNodeState); }

  if(analysisDebugLevel>=1) {
    Dbg::indent ind(analysisDebugLevel, 1);
    if(modified) {
      Dbg::dbg << "  Next node's in-data modified. Adding..."<<endl;
      Dbg::dbg << "Propagated: Lattice "<<endl;
      { Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg<<NodeState::str(nextNodeState); }
    }
    else
      Dbg::dbg << "  No modification on this node"<<endl;
  }

  return modified;
}

/*************************************
 *** UnstructuredPassInterDataflow ***
 *************************************/
void UnstructuredPassInterDataflow::runAnalysis()
{
  set<FunctionState*> allFuncs = FunctionState::getAllDefinedFuncs();
  IntraProceduralDataflow *intraDataflow = dynamic_cast<IntraProceduralDataflow*>(intraAnalysis);
  ROSE_ASSERT(intraDataflow);
  
  // iterate over all functions with bodies
  for(set<FunctionState*>::iterator it=allFuncs.begin(); it!=allFuncs.end(); it++)
  {
    const Function& func = (*it)->func;
    FunctionState* fState = FunctionState::getDefinedFuncState(func);
    
    // Initialize the lattices used by this analysis, if this is the first time the analysis visits this function
    if(visited.find(func) == visited.end()) {
      InitDataflowState ids(getIntraComposedAnalysis(), ComposedAnalysis::none);
      ids.runAnalysis(func, &(fState->state));
      visited.insert(func);
    }
    
    // Call the current intra-procedural dataflow as if it were a generic analysi
    intraAnalysis->runAnalysis(func, &(fState->state));
  }
}

/****************************
 *** MergeAllReturnStates ***
 ****************************/

MergeAllReturnStates::MergeAllReturnStates(ComposedAnalysis* analysis): 
          UnstructuredPassIntraAnalysis(analysis)
{ modified=false; }

MergeAllReturnStates::MergeAllReturnStates(ComposedAnalysis* analysis, const std::vector<Lattice*>& initLats):
          UnstructuredPassIntraAnalysis(analysis)
{ 
  modified=false;

  // Copy initLats into mergedLatsRetVal
  for(vector<Lattice*>::const_iterator l=initLats.begin(); l!=initLats.end(); l++)
    mergedLatsRetVal.push_back((*l)->copy());
}

void MergeAllReturnStates::visit(const Function& func, PartPtr part, NodeState& state)
{
  Dbg::region reg(1, 1, Dbg::region::topLevel, "MergeAllReturnStates::visit()");
  Dbg::dbg << "part="<<part->str()<<endl;
  //if(analysisDebugLevel>=1) Dbg::dbg << "MergeAllReturnStates::visit() func="<<func.get_name().getString()<<"()"<<endl; // sgn="<<sgn<<"["<<Dbg::escape(sgn->unparseToString())<<" | "<<sgn->class_name()<<"]\n";
  //Dbg::dbg << "visit {{{: modified="<<modified<<endl;
  
  Dbg::indent ind(analysisDebugLevel, 1);
  
  // Consider all the CFGNodes in the part to see if any are return statements or the end of the function
  // and merge their dataflow state
  set<CFGNode> v=part->CFGNodes();
  for(set<CFGNode>::iterator c=v.begin(); c!=v.end(); c++) {
    SgNode* sgn=c->getNode();

    // If this is an explicit return statement
    if(isSgReturnStmt(sgn)) {
      if(analysisDebugLevel>=1)
        Dbg::dbg << "MergeAllReturnStates::visit() return expr="<<isSgReturnStmt(sgn)->get_expression()<<"["<<Dbg::escape(isSgReturnStmt(sgn)->get_expression()->unparseToString())<<" | "<<isSgReturnStmt(sgn)->get_expression()->class_name()<<"]\n";

      NodeState* state = NodeState::getNodeState(analysis, part);
      if(analysisDebugLevel>=1)
      { Dbg::dbg << "state="<<endl; Dbg::indent ind; Dbg::dbg<<state->str(analysis)<<endl; }
      // Incorporate the entire dataflow state at the return statement
      //modified = mergeLats(mergedLatsRetStmt, state->getLatticeAbove(analysis)) || modified;

      // Incorporate just the portion of the dataflow state that corresponds to the value being returned,
      // assuming that any information is available
      vector<Lattice*> exprLats;
      std::set<pair<MemLocObjectPtr, MemLocObjectPtr> > retVal2Decl;
      MemLocObjectPtrPair retP = analysis->getComposer()->Expr2MemLoc(isSgReturnStmt(sgn)->get_expression(), part->inEdgeFromAny(), analysis);
      retVal2Decl.insert(make_pair(retP.expr ? retP.expr: retP.mem,
                                   analysis->getComposer()->Expr2MemLoc(func.get_declaration()->search_for_symbol_from_symbol_table(), part->inEdgeFromAny(), analysis).mem));
      
      // Propagate the state above the SgReturnStmt to below it. This is to ensure that there is dataflow state
      // immediately before and immediately after the first/last part of each function, just like in the FunctionState.
      NodeState::copyLatticesOW(state->getLatticeBelowAllMod(analysis), part->outEdgeToAny(),
                                state->getLatticeAboveAll(analysis),    NULLPartEdge, true);
      
      for(vector<Lattice*>::const_iterator l=state->getLatticeBelow(analysis, part->outEdgeToAny()).begin(); 
          l!=state->getLatticeBelow(analysis, part->outEdgeToAny()).end(); l++) {
        exprLats.push_back((*l)->remapML(retVal2Decl, part->outEdgeToAny()));
      }
      if(analysisDebugLevel>=1) Dbg::dbg << "    Merging dataflow state of return value\n";
      modified = mergeLats(mergedLatsRetVal, exprLats) || modified; 
      // Deallocate the newly-created renamed lattices
      for(vector<Lattice*>::iterator l=exprLats.begin(); l!=exprLats.end(); l++) 
        delete *l;
    }
    // If this is the end of a function, which is an implicit return that has no return value
    else if(isSgFunctionDefinition(sgn)) {
      if(analysisDebugLevel>=1)
        Dbg::dbg << "MergeAllReturnStates::visit() isSgFunctionDefinition\n";
    
      NodeState* state = NodeState::getNodeState(analysis, part);
      
      // Propagate the state above the SgFunctionDefinition to below it. This is to ensure that there is dataflow state
      // immediately before and immediately after the first/last part of each function, just like in the FunctionState.
      NodeState::copyLatticesOW(state->getLatticeBelowAllMod(analysis), part->outEdgeToAny(),
                                state->getLatticeAboveAll(analysis),    NULLPartEdge, true);
    
      // Incorporate the entire dataflow state at the implicit return statement
      modified = mergeLats(mergedLatsRetVal, state->getLatticeBelow(analysis, part->outEdgeToAny())) || modified;
    }
  }
  
  //Dbg::dbg << "visit >>>: modified="<<modified<<endl;
}

// Merges the lattices in the given vector into mergedLats. 
// Returns true of mergeLats changes as a result and false otherwise.
bool MergeAllReturnStates::mergeLats(vector<Lattice*>& mergedLat, const vector<Lattice*>& lats) {
  Dbg::indent(analysisDebugLevel, 1);
  // If this is the first return statement we've observed, initialize mergedLat with its lattices
  if(mergedLat.size()==0) {
    if(analysisDebugLevel>=1) Dbg::dbg << "Fresh lattice: "<<endl;
    for(vector<Lattice*>::const_iterator l=lats.begin(); l!=lats.end(); l++) {
      mergedLat.push_back((*l)->copy());
      if(analysisDebugLevel>=1) Dbg::dbg <<(*l)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    }
    return true;
  // Otherwise, merge lats into mergedLats
  } else {
    if(analysisDebugLevel>=1) {
      if(mergedLat.size()!=lats.size()) {
        Dbg::dbg << "#mergedLat="<<mergedLat.size()<<endl;
        for(vector<Lattice*>::iterator ml=mergedLat.begin(); ml!=mergedLat.end(); ml++)
          Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<(*ml)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
        Dbg::dbg << "#lats="<<lats.size()<<endl;
        for(vector<Lattice*>::const_iterator l=lats.begin(); l!=lats.end(); l++)
          Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<(*l)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
      }
    }
    ROSE_ASSERT(mergedLat.size()==lats.size());
    vector<Lattice*>::const_iterator l;
    vector<Lattice*>::iterator ml;
    bool modified = false;
    if(analysisDebugLevel>=1) Dbg::dbg << "    Updating lattice: \n";
    for(l=lats.begin(), ml=mergedLat.begin(); l!=lats.end(); l++, ml++) {
      if(analysisDebugLevel>=1) {
        Dbg::dbg << "Update: "<<(*l)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
        Dbg::dbg << "+ Orig: "<<(*ml)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
      }
      modified = (*ml)->meetUpdate(*l) || modified;
      if(analysisDebugLevel>=1) {
        Dbg::dbg << "=> New: "<<(*ml)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
        Dbg::dbg << "modified="<<modified<<endl;
      }
    }
    return modified;
  }
}

// Returns the merged dataflow information at the end of the analyzed function
map<PartEdgePtr, vector<Lattice*> > MergeAllReturnStates::getMergedDFInfo()
{
  map<PartEdgePtr, vector<Lattice*> > dfInfo;
  dfInfo[NULLPartEdge] = mergedLatsRetVal;
  return dfInfo;
}

// Deallocates all the merged lattices
MergeAllReturnStates::~MergeAllReturnStates()
{
  for(vector<Lattice*>::iterator ml=mergedLatsRetVal.begin(); ml!=mergedLatsRetVal.end(); ml++)
    delete *ml;
}


/****************************
 *** SetAllReturnStates ***
 ****************************/

void SetAllReturnStates::visit(const Function& func, PartPtr part, NodeState& state)
{
  //if(analysisDebugLevel>=1) Dbg::dbg << "SetAllReturnStates::visit() func="<<func.get_name().getString()<<"() p="<<p->str()<<", #state.getLatticeAbove="<<state.getLatticeAboveMod(analysis).size()<<", #lats="<<lats.size()<<endl;
  //Dbg::dbg << "visit {{{: modified="<<modified<<endl;
  
  Dbg::indent ind(analysisDebugLevel, 1);
  
  set<CFGNode> v=part->CFGNodes();
  for(set<CFGNode>::iterator c=v.begin(); c!=v.end(); c++) {
    SgNode* sgn = c->getNode(); 
  
    if(isSgFunctionParameterList(sgn)) { 
      paramList = isSgFunctionParameterList(sgn);
      paramsState = &state;
      //paramsPart = part;
    }

    // If this is an explicit return statement
    if(isSgReturnStmt(sgn)) {
      Dbg::region r(analysisDebugLevel, 1, Dbg::region::topLevel, "Setting dataflow at Return Statement");

      if(analysisDebugLevel>=1)
        Dbg::dbg << "SetAllReturnStates::visit() isSgReturnStmt(sgn)->get_expression()="<<isSgReturnStmt(sgn)->get_expression()<<"["<<Dbg::escape(isSgReturnStmt(sgn)->get_expression()->unparseToString())<<" | "<<isSgReturnStmt(sgn)->get_expression()->class_name()<<"]\n";
      
      // Incorporate the entire dataflow state at the return statement
      // Incorporate just the portion of the dataflow state that corresponds to the value being returned,
      // assuming that any information is available
      std::set<pair<MemLocObjectPtr, MemLocObjectPtr> > decl2RetVal;
      MemLocObjectPtrPair returnExpr = analysis->getComposer()->Expr2MemLoc(isSgReturnStmt(sgn)->get_expression(), part->inEdgeFromAny(), analysis);
      decl2RetVal.insert(make_pair(analysis->getComposer()->Expr2MemLoc(func.get_declaration()->search_for_symbol_from_symbol_table(), part->inEdgeFromAny(), analysis).mem,
                                   (returnExpr.expr? returnExpr.expr : returnExpr.mem)));

      Dbg::dbg << "decl2RetVal="<<endl;
      for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::const_iterator m=decl2RetVal.begin(); m!=decl2RetVal.end(); m++) {
        Dbg::indent ind(analysisDebugLevel, 1);
        Dbg::dbg << m->first.get()->str("")<<" => "<<m->second.get()->str("")<<endl;
      }

      vector<Lattice*> exprLats;
      for(vector<Lattice*>::const_iterator l=lats.begin(); l!=lats.end(); l++)
        exprLats.push_back((*l)->remapML(decl2RetVal, part->inEdgeFromAny()));

      if(analysisDebugLevel>=1) Dbg::dbg << "    Setting dataflow state of return value analysis="<<analysis<<" state="<<&state<<endl;
      modified = mergeLats(state.getLatticeBelowMod(analysis), exprLats) || modified; 

      // Deallocate the newly-created renamed lattices
      for(vector<Lattice*>::iterator l=exprLats.begin(); l!=exprLats.end(); l++) 
        delete *l;
    }
    // If this is the end of a function, which is an implicit return that has no return value
    else if(isSgFunctionDefinition(sgn)) {
      Dbg::region r(analysisDebugLevel, 1, Dbg::region::topLevel, "Setting dataflow at isSgFunctionDefinition");

      std::set<pair<MemLocObjectPtr, MemLocObjectPtr> > decl2NULL;
      MemLocObjectPtr NULLMemLoc;
      decl2NULL.insert(make_pair(analysis->getComposer()->Expr2MemLoc(func.get_declaration()->search_for_symbol_from_symbol_table(), part->inEdgeFromAny(), analysis).mem,
                                   NULLMemLoc));

      vector<Lattice*> exprLats;
      for(vector<Lattice*>::const_iterator l=lats.begin(); l!=lats.end(); l++)
        exprLats.push_back((*l)->remapML(decl2NULL, part->inEdgeFromAny()));

      // Incorporate the entire dataflow state at the implicit return statement
      //modified = mergeLats(mergedLatsRetStmt, state->getLatticeAbove(analysis)) || modified;
      modified = mergeLats(state.getLatticeBelowMod(analysis), exprLats) || modified;
    }
 }
  
  //Dbg::dbg << "visit >>>: modified="<<modified<<endl;
}

// Merges the lattices in the given vector into mergedLats. 
// Returns true of mergeLats changes as a result and false otherwise.
bool SetAllReturnStates::mergeLats(vector<Lattice*>& mergedLat, const vector<Lattice*>& lats) {
  Dbg::indent(analysisDebugLevel, 1);
  
  if(analysisDebugLevel>=1) {
    if(mergedLat.size()!=lats.size()) {
      Dbg::dbg << "#mergedLat="<<mergedLat.size()<<endl;
      for(vector<Lattice*>::iterator ml=mergedLat.begin(); ml!=mergedLat.end(); ml++)
        Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<(*ml)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
      Dbg::dbg << "#lats="<<lats.size()<<endl;
      for(vector<Lattice*>::const_iterator l=lats.begin(); l!=lats.end(); l++)
        Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<(*l)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    }
  }
  ROSE_ASSERT(mergedLat.size()==lats.size());
  vector<Lattice*>::const_iterator l;
  vector<Lattice*>::iterator ml;
  bool modified = false;
  if(analysisDebugLevel>=1) Dbg::dbg << "    Updating lattice: \n";
  for(l=lats.begin(), ml=mergedLat.begin(); l!=lats.end(); l++, ml++) {
    if(analysisDebugLevel>=1) {
      Dbg::dbg << "Update: "<<(*l)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
      Dbg::dbg << "+ Orig: "<<(*ml)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    }
    modified = (*ml)->meetUpdate(*l) || modified;
    if(analysisDebugLevel>=1) {
      Dbg::dbg << "=> New: "<<(*ml)->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
      Dbg::dbg << "modified="<<modified<<endl;
    }
  }
  return modified;
}

/*************************************************
 *** ContextInsensitiveInterProceduralDataflow ***
 *************************************************/

ContextInsensitiveInterProceduralDataflow::ContextInsensitiveInterProceduralDataflow
        (ComposedAnalysis* intraDataflow, SgIncidenceDirectedGraph* graph) :
             InterProceduralAnalysis((IntraProceduralAnalysis*)intraDataflow),
             InterProceduralDataflow(intraDataflow), 
             TraverseCallGraphDataflow(graph)
{
  // Record that the functions that have no callers are being analyzed because the data flow at their
  // callers (the environment) has changed. This is done to jump-start the analysis.
  /*for(set<const CGFunction*>::iterator func=noPred.begin(); func!=noPred.end(); func++) {
    remainingDueToCallers.insert(**func);
  }*/
  for(set<CGFunction>::iterator func=functions.begin(); func!=functions.end(); func++) {
    //if(SageInterface::isMain(func->get_declaration()))
      remainingDueToCallers.insert(*func);
  }
  
  if(intraDataflow->getDirection()==ComposedAnalysis::none) return;
  
  // Record as part of each FunctionState the merged lattice states above the function's return statements
  set<FunctionState*> allFuncs = FunctionState::getAllDefinedFuncs();
  for(set<FunctionState*>::iterator it=allFuncs.begin(); it!=allFuncs.end(); it++)
  {
    FunctionState* funcS = *it;
    if(funcS->func.get_definition()) {
      if(intraDataflow->getDirection()==ComposedAnalysis::fw) {
        PartPtr startPart = getIntraComposedAnalysis()->getComposer()->GetFunctionStartPart(funcS->getFunc(), getIntraComposedAnalysis());
        intraDataflow->initializeState(funcS->getFunc(), startPart, 
                                       funcS->state);
      /*
        // Make sure that the lattices above the function are associated with the outgoing edges of startN
        // i.e. they correspond to the side of the function's start that goes towards the function
        vector<Lattice*> lats = funcS->state.getLatticeAboveMod(intraDataflow);
        for(vector<Lattice*>::iterator l=lats.begin(); l!=lats.end(); l++)
        { (*l)->setPartEdge(startPart->outEdgeToAny()); }*/
      }
      else if(intraDataflow->getDirection()==ComposedAnalysis::bw) {
        PartPtr endPart = getIntraComposedAnalysis()->getComposer()->GetFunctionEndPart(funcS->getFunc(), getIntraComposedAnalysis());
        intraDataflow->initializeState(funcS->getFunc(), endPart, funcS->state);
        
        /* // Make sure that the lattices below the function are associated with the incoming edges of endN 
        // i.e. they correspond to the side of the function's end that comes from the function
        vector<Lattice*> lats = funcS->state.getLatticeAboveMod(intraDataflow);
        for(vector<Lattice*>::iterator l=lats.begin(); l!=lats.end(); l++)
        { (*l)->setPartEdge(endPart->inEdgeFromAny()); }*/
      }
      
      Dbg::dbg << "Return state for function " << funcS << " " << funcS->func.get_name().getString() << endl
               << "funcS->state" << funcS->state.str(intraAnalysis) << endl;
      //         << "funcS->retState="<<  funcS->retState.str(intraDataflow) << endl;
    }
  }
  
/*      set<FunctionState*> allFuncs = FunctionState::getAllDefinedFuncs();
  //Function main;
  
  // initialize the dataflow state of all functions with bodies
  for(set<FunctionState*>::iterator it=allFuncs.begin(); it!=allFuncs.end(); it++)  
  {
    FunctionState* funcS = *it;
    Function func = funcS->func;
    vector<Lattice*> initState = intraDataflow->genInitState(
             func, func.get_definition()->cfgForBeginning(), funcS->state);

printf("ContextInsensitiveInterProceduralDataflow initState %s():\n", (*it)->func.get_name().getString());
for(vector<Lattice*>::iterator it = initState.begin(); 
    it!=initState.end(); it++)
{       
  Dbg::dbg << *it << ": " << (*it)->str("    ") << endl;
}
  
    funcS->state.setLattices(this, initState);
    
     // remember the main() function
    if(strcmp(func.get_name().getString(), "main")==0)
      main = func;
  }
  
  for(set<FunctionState*>::iterator it=allFuncs.begin(); it!=allFuncs.end(); it++)  
  {
    FunctionState* funcS = *it;
  }*/
}
  
// The transfer function that is applied to SgFunctionCallExp nodes to perform the appropriate state transfers
// fw - =true if this is a forward analysis and =false if this is a backward analysis
// n - the dataflow node that is being processed
// state - The NodeState object that describes the dataflow state immediately before (if fw=true) or immediately after 
//   (if fw=false) the SgFunctionCallExp node, as established by earlier analysis passes
// dfInfo - The Lattices that this transfer function operates on. The function propagates them 
//    to the callee function and overwrites them with the dataflow result of calling this function.
/*// retState - Pointer reference to a Lattice* vector that will be assigned to point to the lattices of
//    the function call's return value. The caller may not modify these lattices.*/
// Returns true if any of the input lattices changed as a result of the transfer function and
//    false otherwise.  
bool ContextInsensitiveInterProceduralDataflow::transfer(
           const Function& caller, PartPtr callPart, CFGNode callCFG, NodeState& state, 
           vector<Lattice*>& dfInfo)
{
  // First pass information from the caller to the callee, placing the callee on the worklist if it is modified.
  // Then, we do not wait for the callee to be processed but rather optimistically guess that the new pass will not 
  // change the info after it and simply take the current info and pass it back to the caller. This is simpler and
  // ensures that we get debug output for the caller as one contiguous sequence that is not interspersed by debug 
  // info of analysis of other functions.
  // Caller -> Callee
  //   Remap lattices before the call from caller's to callee's scope
  //      fw: all arguments mapped to their parameters
  //      bw: all arguments passed by reference mapped to their parameters
  //          Function call expression mapped to the function's SgDefinition
  //   Merge resulting lattice to the lattice before the function in its FunctionState
  //   If this lattice is modified, merge the results 
  // Callee -> Caller
  //   Remap lattices after the callee function to the caller's scope. It is assumed that the state after the function 
  //     is the merge of all the states at possible end points. For fw this means the function exit and the
  //     return statements and for bw it is just the function's entry. For fw it is also assumed that the function's 
  //     symbol is mapped to the merge of all return values in all the return statements.
  //     fw: all parameters passed by reference are mapped to their callers (remaining parameters are ignored but 
  //         if a live-dead analysis is used then they'll automatically be mapped to DEAD, meaning that in practice 
  //         it won't be necessary to remove them as part of remap). The function's symbol is mapped to the function
  //         call expression in the caller.
  //     bw: all parameters are mapped back to their arguments
   
  bool modified = false;
  SgFunctionCallExp* call = isSgFunctionCallExp(callCFG.getNode());
  Function callee(call);
  ROSE_ASSERT(call);
  
  ROSE_ASSERT(getIntraComposedAnalysis()->getDirection()!=ComposedAnalysis::none);
  
  if(analysisDebugLevel > 0)
    Dbg::dbg << "ContextInsensitiveInterProceduralDataflow::transfer "<<caller.get_name().getString()<<"()=>"<<callee.get_name().getString()<<"()\n";
  if(callee.get_definition())
  {
    FunctionState* funcS = FunctionState::getDefinedFuncState(callee);
    Dbg::dbg << "Function nodeState="<<endl;
    { Dbg::indent ind; Dbg::dbg << funcS->state.str(getIntraComposedAnalysis())<<endl; }
    
    
    Dbg::dbg << "funcS->state="<<endl;
    {Dbg::indent ind; Dbg::dbg << funcS->state.str(intraAnalysis)<<endl; }
    
    // Caller -> Callee
    // ----------------
    {
      Dbg::region reg(analysisDebugLevel, 1, Dbg::region::topLevel, "Inter::transfer Caller -> Callee");
      
      // The lattices before the function (forward: before=above, after=below; backward: before=below, after=above)
      const vector<Lattice*>* funcLatticesBefore;
      if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw)
        funcLatticesBefore = &(funcS->state.getLatticeAbove(intraAnalysis));
      else if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::bw)
        funcLatticesBefore = &(funcS->state.getLatticeBelow(intraAnalysis));
   
      // Create a MemLocObject map to remap the lattices from the caller's to the callee's context
      std::set<pair<MemLocObjectPtr, MemLocObjectPtr> > ml2ml;
      if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw)
        FunctionState::setArgParamMap(callPart, call, ml2ml, 
                                      getIntraComposedAnalysis()->getComposer(),
                                      getIntraComposedAnalysis());
      else if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::bw)
        FunctionState::setArgByRef2ParamMap(callPart, call, ml2ml, 
                                      getIntraComposedAnalysis()->getComposer(),
                                      getIntraComposedAnalysis());
      
      PartPtr beforePart = 
              (getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw ? 
                  getIntraComposedAnalysis()->getComposer()->GetFunctionStartPart(callee, getIntraComposedAnalysis()) : 
                  getIntraComposedAnalysis()->getComposer()->GetFunctionEndPart(callee,   getIntraComposedAnalysis()));
      if(analysisDebugLevel>=1) {
        Dbg::dbg << "ml2ml="<<endl;
        for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::const_iterator m=ml2ml.begin(); m!=ml2ml.end(); m++) {
          Dbg::indent ind(analysisDebugLevel, 1);
          Dbg::dbg << m->first.get()->str("")<<" => "<<m->second.get()->str("")<<endl;
        }
      }

      // Update the function's entry/exit state with the caller's state at the call site
      vector<Lattice*>::const_iterator itCalleeBefore, itCallerBefore;
      for(itCallerBefore = dfInfo.begin(), itCalleeBefore = funcLatticesBefore->begin(); 
          itCallerBefore!=dfInfo.end() && itCalleeBefore!=funcLatticesBefore->end(); 
          itCallerBefore++, itCalleeBefore++)
      {
        Lattice* calleeL = *itCalleeBefore;
        Lattice* callerL = *itCallerBefore;

        if(analysisDebugLevel>=1) {
          Dbg::indent(analysisDebugLevel, 1);
          Dbg::dbg << "callerL="<<endl;
          {Dbg::indent indt; Dbg::dbg<<callerL->str()<<endl;}
          Dbg::dbg << "Before calleeL="<<endl;
          {Dbg::indent indt; Dbg::dbg<<calleeL->str()<<endl; }
        }

        Lattice* remappedL;
        if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw)
          remappedL = callerL->remapML(ml2ml, beforePart->inEdgeFromAny());
        else if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::bw)
          remappedL = callerL->remapML(ml2ml, beforePart->outEdgeToAny());
        
        if(analysisDebugLevel>=1) {
          Dbg::dbg << "remappedCallerL=["<<remappedL<<"]="<<endl;
          { Dbg::indent ind; Dbg::dbg<<remappedL->str()<<endl; }
        }

        // Update the coutEdgeToAnyallee's Lattice with the new information at the call site
        modified = calleeL->meetUpdate(remappedL) || modified;
        
        if(analysisDebugLevel>=1) {
          Dbg::dbg << "After modified = "<<modified << endl;
          Dbg::dbg << "calleeL=["<<calleeL<<"]="<<endl;
          { Dbg::indent ind; Dbg::dbg<<calleeL->str()<<endl; }
        }
  //!!!     delete remappedL;
      }
      
      // If this resulted in the dataflow information before the callee changing or the calle has not yet been
      // analyzed, add it to the remaining list.
      if(modified || (visited.find(callee) == visited.end())) {
        if(analysisDebugLevel > 0) Dbg::dbg << "ContextInsensitiveInterProceduralDataflow::transfer Incoming Dataflow info modified\n";
        // Record that the callee function needs to be re-analyzed because of new information from the caller
        TraverseCallGraphDataflow::addToRemaining(getFunc(callee));
        remainingDueToCallers.insert(getFunc(callee));
      }
    } // Caller -> Callee

    // Callee -> Caller
    // ----------------
    {
      Dbg::region reg(analysisDebugLevel, 1, Dbg::region::topLevel, "Inter::transfer Callee -> Caller");
      // The lattices after the function (forward: before=above, after=below; backward: before=below, after=above).
      
      PartPtr afterPart = 
              (getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw ? 
                  getIntraComposedAnalysis()->getComposer()->GetFunctionEndPart(callee,   getIntraComposedAnalysis()) : 
                  getIntraComposedAnalysis()->getComposer()->GetFunctionStartPart(callee, getIntraComposedAnalysis()));
      
      const vector<Lattice*>* funcLatticesAfter;
      if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw) 
        funcLatticesAfter = &(funcS->state.getLatticeBelow(intraAnalysis, afterPart->outEdgeToAny()));
      else if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::bw)
        funcLatticesAfter = &(funcS->state.getLatticeAbove(intraAnalysis, afterPart->inEdgeFromAny()));

      // Create a MemLocObject map to remap the lattices from the callee's to the caller's context
      std::set<pair<MemLocObjectPtr, MemLocObjectPtr> > ml2ml;
      if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw) {
        FunctionState::setArgByRef2ParamMap(callPart, call, ml2ml, 
                                      getIntraComposedAnalysis()->getComposer(),
                                      getIntraComposedAnalysis());
      } else if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::bw) {
        FunctionState::setArgParamMap(callPart, call, ml2ml, 
                                      getIntraComposedAnalysis()->getComposer(),
                                      getIntraComposedAnalysis());
      }
      // Invert the above mappings to map parameters to arguments
      ml2ml = FunctionState::invertArg2ParamMap(ml2ml);
      if(analysisDebugLevel>=1) {
        Dbg::dbg << "ml2ml="<<endl;
        for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::const_iterator m=ml2ml.begin(); m!=ml2ml.end(); m++) {
          Dbg::indent ind(analysisDebugLevel, 1);
          Dbg::dbg << m->first.get()->str("")<<" => "<<m->second.get()->str("")<<endl;
        }
      }
      
      // Transfer the result of the function call into the dfInfo Lattices.
      vector<Lattice*>::const_iterator itCalleeAfter, itCallerAfter;
      for(itCallerAfter = dfInfo.begin(), itCalleeAfter = funcLatticesAfter->begin(); 
          itCallerAfter!=dfInfo.end() && itCalleeAfter!=funcLatticesAfter->end(); 
          itCallerAfter++, itCalleeAfter++)
      {
        Dbg::indent ind(analysisDebugLevel, 1);
        Lattice* callerL = *itCallerAfter;
        Lattice* calleeL = *itCalleeAfter;
        //Dbg::dbg << "      calleeL-after=["<<calleeL<<"] "<<calleeL->str()<<endl;

        if(analysisDebugLevel>=1) {
          Dbg::dbg << "callerL-before=["<<callerL<<"]"<<endl;
          {Dbg::indent ind; Dbg::dbg<<callerL->str()<<endl; }
          Dbg::dbg << "+calleeL=["<<calleeL<<"]"<<endl;
          {Dbg::indent ind; Dbg::dbg<<calleeL->str()<<endl; }
        }

        // Update the caller's Lattice with the new information at the call site
        Lattice* calleeRemapped;
        if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw)
          calleeRemapped = calleeL->remapML(ml2ml, callPart->outEdgeToAny());
        else if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::bw)
          calleeRemapped = calleeL->remapML(ml2ml, callPart->inEdgeFromAny());
        
        if(analysisDebugLevel>=1) {
          Dbg::dbg << "~calleeRemapped=["<<calleeRemapped<<"]"<<endl; 
          {Dbg::indent ind; Dbg::dbg<<calleeRemapped->str()<<endl; }
        }
        modified = callerL->replaceML(calleeRemapped) || modified;
        delete calleeRemapped;
        
        if(analysisDebugLevel>=1) {
          Dbg::dbg << "==> callerL-after=["<<callerL<<"] "<<endl;
          {Dbg::indent ind; Dbg::dbg<<callerL->str()<<endl; }
        }
      }
    } // Callee -> Caller
    // Point retState to the lattices of the function's return values
    //*retState = &(funcS->retState.getLatticeBelowMod(intraAnalysis));
    
    /*Dbg::dbg << "      retState="<<retState<<endl;
    for(vector<Lattice*>::iterator l=(*retState)->begin(); l!=(*retState)->end(); l++)
      Dbg::dbg << "    "<<(*l)->str("      ")<<endl;*/
  }
  // Don't do anything for functions with no definitions
  else
  {
    
  }
  
  return modified;
}

// Uses TraverseCallGraphDataflow to traverse the call graph.
void ContextInsensitiveInterProceduralDataflow::runAnalysis()
{
  traverse();
}

// Runs the intra-procedural analysis every time TraverseCallGraphDataflow passes a function.
void ContextInsensitiveInterProceduralDataflow::visit(const CGFunction* funcCG)
{
  Function func = *funcCG;
  Dbg::region r(analysisDebugLevel, 1, Dbg::region::topLevel, "Inter-analysis Function "+func.get_name().getString());
  
  // 1. Invoke the intra-procedural analysis on the given function. It is assumed that currently the state before this 
  // function is initialized to the dataflow state at one or more calls to func, remapped to the func's context. 
  //   fw: Call arguments remapped to function parameters
  //   bw: For pass-by-reference parameters the call arguments remapped to parameters, and
  //       the function's return value mapped to the function's symbol.
  // 2. propagate the state before func in its FunctionState to the intra analysis on the func.
  //   fw: Copy from before function to function start
  //   bw: Copy from before function to function end (all return statements and the end of the function) and within
  //       these lattices, maps the state associated with the function's symbol to be the state of the returned values 
  //       in all the return statements (done by SetAllReturnStates).
  // 3. Propagate the state produced by the intra analysis to the function's FunctionState after the function
  //   fw: Merge the state at all return statements and function end, mapping the state of the returned values to 
  //       the function's symbol (done by MergeAllReturnStates).
  //   bw: Copy from the function's start in the intra analysis to the FunctionState after it.
  
  if(func.get_definition())
  {
    FunctionState* fState = FunctionState::getDefinedFuncState(func);
      
    if(analysisDebugLevel>=1)
    {
      if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw) {
        Dbg::dbg<<"Function LatticeAboveMod:"<<endl;
        for(vector<Lattice*>::const_iterator it = fState->state.getLatticeAbove(intraAnalysis).begin();
            it!=fState->state.getLatticeAbove(intraAnalysis).end(); it++)
        { Dbg::indent ind; Dbg::dbg << (*it)->str("") << endl; }
      } else if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::bw) {
        Dbg::dbg<<"Function LatticeBelowMod:"<<endl;
        for(vector<Lattice*>::const_iterator it = fState->state.getLatticeBelow(intraAnalysis).begin();
            it!=fState->state.getLatticeBelow(intraAnalysis).end(); it++)
        { Dbg::indent ind; Dbg::dbg << (*it)->str("") << endl; }
      }
    }
    
    // The reason why the visit function was called was because this is the first time this function was analyzed
    // or because its incoming information was changed. As such, by default we assume that the intra analysis
    // must be re-run.
    
    /*Dbg::dbg << "Visited functions (visited.find(func) == visited.end())="<<(visited.find(func) == visited.end())<<endl;
    for(std::set<Function>::iterator f=visited.begin(); f!=visited.end(); f++)
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<f->get_name().getString();*/
    
    // Initialize the lattices used by this analysis, if this is the first time the analysis visits this function
    // Flag that indicates whether this function is being analyzed for the first time
    bool firstVisit = visited.find(func) == visited.end(); 
    if(firstVisit) {
      visited.insert(func);
    }
    if(getIntraComposedAnalysis()->getDirection()!=ComposedAnalysis::none) {
      // Initialize the dataflow states of the nodes within this function using the intra analysis
      InitDataflowState ids(getIntraComposedAnalysis(), getIntraComposedAnalysis()->getDirection());
      ids.runAnalysis(func, &(fState->state));
    }
    
    SgFunctionParameterList* paramListNode = NULL;
    NodeState* paramsListState = NULL;
    //PartPtr paramsPart;
    PartPtr entryPart = getIntraComposedAnalysis()->getComposer()->GetFunctionStartPart(func, getIntraComposedAnalysis());
    PartPtr exitPart = getIntraComposedAnalysis()->getComposer()->GetFunctionEndPart(func, getIntraComposedAnalysis());
            
    if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw) {
      NodeState* entryState = NodeState::getNodeState(getIntraComposedAnalysis(), entryPart);
      NodeState::copyLattices_aEQa(intraAnalysis, *entryState, fState->state);
    } else if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::bw) {
      Dbg::dbg << "fState->state="<<endl;
      {Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg << fState->state.str(intraAnalysis, "") <<endl; }
      
      SetAllReturnStates sars(getIntraComposedAnalysis(), fState->state.getLatticeBelowMod(intraAnalysis));
      sars.runAnalysis(func, &(fState->state)); 
      ROSE_ASSERT(sars.paramList); ROSE_ASSERT(sars.paramsState); //ROSE_ASSERT(sars.paramsPart);
      paramListNode   = sars.paramList;
      paramsListState = sars.paramsState;
      //paramsPart      = sars.paramsPart;
      //ROSE_ASSERT(paramsPart == entryPart);
      // Here we can be more precisely when deciding whether to rerun the intra analysis because it may be the case 
      // that the overall analysis state gets changed but the state at the function end and the returns to not change. 
      // However, this shouldn't happen except in buggy situations so we'll take the conservative route and 
      // always re-run the intra analysis if the visit method gets called.
      //mustRerunIntra = sars.getModified() || mustRerunIntra;
    }
    
    // Run the intra-procedural dataflow analysis on the current function
    
    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    getIntraComposedAnalysis()->runAnalysis(func, &(fState->state), 
                      firstVisit || (remainingDueToCallers.find(func)!=remainingDueToCallers.end()),
                      remainingDueToCalls[func]);
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    vector<Lattice*>* finalLatsToPrint=NULL;
    // Flag that indicates whether the dataflow information after this function was modified, in which case
    // its callers need to be re-analyzed.
    bool afterFuncModified=false;

    if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::fw) {
      // For forward analyses merge the dataflow states above all the return statements in the function, storing
      // the results in the lattices after the function
      //DFStateAtReturns* dfsar = dynamic_cast<DFStateAtReturns*>(fState->state.getFact(this, 0));
      //bool modified = dfsar->mergeReturnStates(func, fState, dynamic_cast<ComposedAnalysis*>(intraAnalysis));  

      MergeAllReturnStates mars(getIntraComposedAnalysis(), fState->state.getLatticeBelowMod(intraAnalysis));
      mars.runAnalysis(func, &(fState->state));
      afterFuncModified = mars.getModified();
      // Overwrite the state below the function with the merged state at its end
      NodeState::copyLatticesOW(fState->state.getLatticeBelowAllMod(intraAnalysis), exitPart->outEdgeToAny(),
                                mars.getMergedDFInfo(),                             NULLPartEdge, true);
      //NodeState::copyLatticesOW(fState->state.getLatticeBelowAllMod(intraAnalysis), mars.getMergedDFInfo());
      
      finalLatsToPrint = &(fState->state.getLatticeBelowMod(intraAnalysis));
    } else if(getIntraComposedAnalysis()->getDirection()==ComposedAnalysis::bw) {
      // For backward analyses get the dataflow state at the start of the function and copy it to the dataflow
      // state above the function
      /*Dbg::dbg << "funcCG->get_definition()->get_declaration()="<<funcCG->get_definition()->get_declaration()<<" funcCG->get_declaration()="<<funcCG->get_declaration()<<endl;
      Dbg::dbg << "paramListNode="<<paramListNode<<" funcCG->get_definition()->get_declaration()->get_parameterList()="<<funcCG->get_definition()->get_declaration()->get_parameterList()<<" funcCG->get_declaration()->get_parameterList()="<<funcCG->get_declaration()->get_parameterList()<<endl;*/

      //PartPtr paramList = DataflowNode(funcCG->get_definition()->get_declaration()->get_parameterList(), getIntraComposedAnalysis()->filter);
      //PartPtr paramListPart = DataflowNode(paramListNode, getIntraComposedAnalysis()->filter);
      //NodeState* paramsState = NodeState::getNodeState(paramListPart);
      ROSE_ASSERT(paramsListState);
      NodeState* paramsState = paramsListState;
      
      // Propagate the state below the first part of the function to above it. This is to ensure that there is dataflow state
      // immediately before and immediately after the first/last part of each function, just like in the FunctionState.
      NodeState::copyLatticesOW(paramsState->getLatticeAboveAllMod(intraAnalysis), entryPart->inEdgeFromAny(),
                                paramsState->getLatticeBelowAll(intraAnalysis),    NULLPartEdge, true);
      
      ROSE_ASSERT(fState->state.getLatticeBelowMod(intraAnalysis).size() == 
                  paramsState->getLatticeAboveMod(intraAnalysis, entryPart->inEdgeFromAny()).size());
      //afterFuncModified = NodeState::equivLattices(fState->state.getLatticeAboveMod(intraAnalysis), paramsState->getLatticeBelowMod(intraAnalysis));
      Dbg::dbg << "initial fState->state="<<fState->state.str(intraAnalysis)<<endl;
      Dbg::dbg << "paramsState="<<paramsState->str(intraAnalysis)<<endl;
      // If nextNodeState is non-empty, we union curNodeState into it
      Dbg::dbg << "----------------------"<<endl;
      if(fState->state.getLatticeAboveAllMod(intraAnalysis).size()>0)
        afterFuncModified = NodeState::unionLatticeMaps(fState->state.getLatticeAboveAllMod(intraAnalysis), 
                                               paramsState->getLatticeAboveAll(intraAnalysis)) || afterFuncModified;
      // Otherwise, we copy curNodeState[NULLPartEdge] over it
      else {
        NodeState::copyLatticesOW(fState->state.getLatticeAboveAllMod(intraAnalysis), 
                                  paramsState->getLatticeAboveAll(intraAnalysis));
        afterFuncModified = true;
      }
      Dbg::dbg << "final afterFuncModified="<<afterFuncModified<<" fState->state="<<fState->state.str(intraAnalysis)<<endl;
      /*for(vector<Lattice*>::iterator lA=fState->state.getLatticeAboveMod(intraAnalysis).begin(), lP=paramsState->getLatticeBelowMod(intraAnalysis).begin();
          lA!=fState->state.getLatticeAboveMod(intraAnalysis).end(); lA++, lP++)
        (*lA)->copy(*lP);*/
      finalLatsToPrint = &(fState->state.getLatticeAboveMod(intraAnalysis));
    }
    
    if(getIntraComposedAnalysis()->getDirection()!=ComposedAnalysis::none && analysisDebugLevel>=1) {
      Dbg::dbg << "final #lattice="<<finalLatsToPrint->size()<<": "<<endl;
      {
        Dbg::indent ind();
        for(vector<Lattice*>::iterator l=finalLatsToPrint->begin(); l!=finalLatsToPrint->end(); l++)
            Dbg::dbg << (*l)->str("&nbsp;&nbsp;")<<endl;
      }

      Dbg::dbg << "function "<<func.get_name().getString()<<" <b><u>"<<(afterFuncModified? "modified": "not modified")<<"</u></b>"<<endl;


      /*Dbg::dbg << "State below:\n";
      for(vector<Lattice*>::const_iterator it = fState->state.getLatticeBelow((Analysis*)this).begin();
          it!=fState->state.getLatticeBelow((Analysis*)this).end(); it++)
      {
        Dbg::dbg << (*it)->str("    ") << endl; 
      }*/
      /*Dbg::dbg << "States at Return Statements:\n";
      {
        Dbg::indent ind;
        for(vector<Lattice*>::iterator it = dfsar->getLatsAtFuncReturn().begin();
            it!=dfsar->getLatsAtFuncReturn().end(); it++)
        {
          Dbg::dbg << (*it)->str("&nbsp;&nbsp;") << endl; 
        }
      }

      vector<Lattice*> retState = fState->retState.getLatticeBelow(intraAnalysis);
      Dbg::dbg << "retState: \n";
      {
        Dbg::indent ind;
        for(vector<Lattice*>::iterator it = retState.begin(); it!=retState.end(); it++)
        Dbg::dbg << (*it)->str("&nbsp;&nbsp;") << endl;
      }

      Dbg::dbg << "States of Return Values: "<<&(dfsar->getLatsRetVal())<<endl;
      {
        Dbg::indent ind;
        for(vector<Lattice*>::iterator it = dfsar->getLatsRetVal().begin();
            it!=dfsar->getLatsRetVal().end(); it++)
        {
          Dbg::dbg << (*it)->str("&nbsp;&nbsp;") << endl; 
        }
      }*/
    }
    
    // Remove the record for this function from remainingDueToCallers and remainingDueToCalls 
    // because it has now been analyzed
    remainingDueToCallers.erase(func);
    remainingDueToCalls.erase(func);

    // If this function's final dataflow state was modified, its callers must be 
    // placed back onto the remaining list, recording that they're on the list
    // because of their calls to this function
    if(afterFuncModified)
    {
      Dbg::dbg << "Inserting Callers\n";
      for(CGFunction::iterator it = funcCG->predecessors(); it!=funcCG->end(); it++)
      {
        const CGFunction* caller = it.getTarget(functions);

        //Dbg::dbg << "Caller of "<<funcCG->get_name().getString()<<": "<<caller->get_name().getString()<<endl;
        addToRemaining(caller);
        remainingDueToCalls[caller].insert(func);
      }
      Dbg::dbg << "remaining = ";
      for(list<const CGFunction*>::iterator f=remaining.begin(); f!=remaining.end(); f++)
        Dbg::dbg << (*f)->get_name().getString() << ", ";
      Dbg::dbg << endl;
    }
  } // if(modified)
} // ContextInsensitiveInterProceduralDataflow::visit(const CGFunction* funcCG)
} // namespace dataflow;
