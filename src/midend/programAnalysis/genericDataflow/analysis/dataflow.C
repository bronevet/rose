#include "analysis.h"
#include "dataflow.h"
#include "compose.h"
#include "printAnalysisStates.h"

#include <memory>
using std::auto_ptr;

#include <utility>
using std::pair;
using std::make_pair;

#include <boost/mem_fn.hpp>
using boost::mem_fn;
#include <boost/make_shared.hpp>

namespace dataflow
{
 
NodeState* IntraBWDataflow::initializeFunctionNodeState(const Function &func, NodeState *fState)
{
  // Initialize the function's entry NodeState 
  NodeState* entryState = NodeState::getNodeState(this, getComposer()->GetFunctionEndPart(func, this));
  //printf("before copyLattices on [%s | %s]\n", funcCFGStart.getNode()->class_name().c_str(), funcCFGStart.getNode()->unparseToString().c_str());
  NodeState::copyLattices_bEQb(this, *entryState, /*interAnalysis*//*this, */*fState);
  
  return entryState;
}

NodeState* IntraFWDataflow::initializeFunctionNodeState(const Function &func, NodeState *fState)
{
  // Initialize the function's entry NodeState 
  NodeState* entryState = NodeState::getNodeState(this, getComposer()->GetFunctionStartPart(func, this));
                
  //printf("before copyLattices on [%s | %s] %d\n", funcCFGStart.getNode()->unparseToString().c_str(), funcCFGStart.getNode()->class_name().c_str(), funcCFGStart.getIndex());
        
  //int i=0;
  //Dbg::dbg << "Before: entryState-above="<<entryState<<endl;
  //for(vector<Lattice*>::const_iterator l=entryState->getLatticeAbove(this).begin(); l!=entryState->getLatticeAbove(this).end(); l++, i++)
  //    Dbg::dbg << "Lattice "<<i<<": "<<*l<<" = "<<(*l)->str("            ")<<endl;
  //i=0;
  //Dbg::dbg << "Before: fState-above="<<fState<<endl;
  //for(vector<Lattice*>::const_iterator l=fState->getLatticeAbove(this).begin(); l!=fState->getLatticeAbove(this).end(); l++, i++)
  ///   Dbg::dbg << "Lattice "<<i<<": "<<*l<<" = "<<(*l)->str("            ")<<endl;
  //Dbg::dbg << "this="<<this<<endl;
        
  NodeState::copyLattices_aEQa((Analysis*)this, *entryState, *fState);

  return entryState;
}

list<PartPtr>
IntraFWDataflow::getInitialWorklist(const Function &func, bool analyzeFromDirectionStart, const set<Function> &calleesUpdated, NodeState *fState)
{
  // Initialize the set of nodes that this dataflow will iterate over
  list<PartPtr> dfIt;

  // If we're analyzing this function for the first time or because the dataflow information coming in from its
  // callers has changed, add the function's entry point
/* GB 2012-09-18 : We currently do not support incremental inter-procedural analysis 
  if(analyzeFromDirectionStart)*/
    //dfIt->add(funcCFGStart);
    dfIt.push_back(getComposer()->GetFunctionStartPart(func, this));
/* GB 2012-09-18 : We currently do not support incremental inter-procedural analysis   
  else {
    / *
     * // If we're analyzing this function because of a change in the exit dataflow states of some of the 
    // functions called by this function (these functions are recorded in calleesUpdated), add the calls
    // by this function to those functions to the dataflow iterator
    FindAllFunctionCalls fafc(calleesUpdated);
    fafc.runAnalysis(func, fState);
    for(map<Function, set<PartPtr> >::iterator callee=fafc.getFuncCalls().begin(); callee!=fafc.getFuncCalls().end(); callee++)
      for(set<PartPtr>::iterator callNode=callee->second.begin(); callNode!=callee->second.end(); callNode++)
        {
          //dfIt->add(*callNode);
          dfIt.push_back(*callNode);
        }* /
    
      / * This doesn't work because it is possible to process a single call that appears early in the function,
         get no updates from it and stop, without processing any subsequent calls.* /
      
    // Iterate over the function in the forward direction, identifying the calls to functions in calleesUpdated.
    // Calls are added to the dfIt in the order they were discovered. This is done to ensure that when the intra
    // analysis is run it is likely to take all effects of the function calls into account with the calls
    // that appear early in the list (earlier in the CFG) and for the later ones will terminate quickly.
    PartPtr funcCFGStart = getComposer()->GetFunctionStartPart(func, this);
    PartPtr funcCFGEnd   = getComposer()->GetFunctionEndPart(func, this);
    
    // iterate over all the nodes in this function
    for(partIterator it(funcCFGStart); it!=partIterator::end(); it++)
    {
      PartPtr p = *it;
      SgNode* n = p.getNode();
      
      // Flag that indicates whether this node is tained.
      // A node is tainted if a predecessor has tainted it
      //bool pTaint = (tainted.find(p)!=tainted.end());
      
      // If this is a call to one of the functions that have had their information updated that is
      // not reachable from another call to such function
      if(/ *!pTaint &&* / isSgFunctionCallExp(n)) {
        for(set<Function>::const_iterator func=calleesUpdated.begin(); func!=calleesUpdated.end(); func++) {
          if((*func).get_declaration() == isSgFunctionCallExp(n)->getAssociatedFunctionDeclaration()) {
            // Add it to the start set of the dataflow iterator
            //dfIt->add(*callNode); 
              dfIt.push_back(p); 
            
            // A node is also tainted if it is a member of the start set
            //pTaint = true;
            break;
        }}
      }
      
      // If the current node is tainted, taint its successors
      / *if(pTaint)
      list<PartPtr> desc = IntraFWDataflow::getDescendants(p);
      for(list<PartPtr>::iterator d=desc.begin(); d!=desc.end(); d++)
        tainted.insert(*d);* /
    }
  }*/
  //Dbg::dbg << "analyzeFromDirectionStart="<<analyzeFromDirectionStart<<" #calleesUpdated="<<calleesUpdated.size()<<" it="<<it.str()<<endl;

  return dfIt;
}

class GetReturnStmts : public UnstructuredPassIntraAnalysis
{
  public:
  set<PartPtr> returns;
  
  GetReturnStmts(ComposedAnalysis* analysis) : UnstructuredPassIntraAnalysis(analysis) {}
  
  void visit(const Function& func, PartPtr part, NodeState& state) {
    if(part->maySgNodeAny<SgReturnStmt>()) {
      returns.insert(part);
    }
  }
};
//

list<PartPtr>
IntraBWDataflow::getInitialWorklist(const Function &func, bool analyzeFromDirectionStart, const set<Function> &calleesUpdated, NodeState *fState)
{
  list<PartPtr> dfIt;
  dfIt.push_back(getComposer()->GetFunctionEndPart(func, this));
  Dbg::dbg << "IntraBWDataflow::getInitialWorklist() end="<<getComposer()->GetFunctionEndPart(func, this)->str()<<endl;
  
  //RoseSTLContainer<SgNode*> rets=NodeQuery::querySubTree(project,VSgReturnStmt);
  //for(RoseSTLContainer<SgNode*>::iterator r=rets.begin(); r!=rets.end(); r++)
  GetReturnStmts grs(this);
  grs.runAnalysis(func, fState);
  for(set<PartPtr>::iterator r=grs.returns.begin(); r!=grs.returns.end(); r++)
    dfIt.push_back(*r);
  return dfIt;
}

map<PartEdgePtr, vector<Lattice*> >& IntraFWDataflow::getLatticeAnte(NodeState *state) { return state->getLatticeAboveAllMod(this); }
map<PartEdgePtr, vector<Lattice*> >& IntraFWDataflow::getLatticePost(NodeState *state) { return state->getLatticeBelowAllMod(this); }
void IntraFWDataflow::setLatticeAnte(NodeState *state, map<PartEdgePtr, vector<Lattice*> >& dfInfo, bool overwrite) {
  if(overwrite) state->copyLatticesOW(state->getLatticeAboveAllMod(this), dfInfo);
  else          state->copyLattices  (state->getLatticeAboveAllMod(this), dfInfo);
}
void IntraFWDataflow::setLatticePost(NodeState *state, map<PartEdgePtr, vector<Lattice*> >& dfInfo, bool overwrite) { 
  if(overwrite) state->copyLatticesOW(state->getLatticeBelowAllMod(this), dfInfo);
  else          state->copyLattices  (state->getLatticeBelowAllMod(this), dfInfo);
}
map<PartEdgePtr, vector<Lattice*> >& IntraBWDataflow::getLatticeAnte(NodeState *state) { return state->getLatticeBelowAllMod(this); }
map<PartEdgePtr, vector<Lattice*> >& IntraBWDataflow::getLatticePost(NodeState *state) { return state->getLatticeAboveAllMod(this); }
void IntraBWDataflow::setLatticeAnte(NodeState *state, map<PartEdgePtr, vector<Lattice*> >& dfInfo, bool overwrite) {
  if(overwrite) state->copyLatticesOW(state->getLatticeBelowAllMod(this), dfInfo);
  else          state->copyLattices  (state->getLatticeBelowAllMod(this), dfInfo);
}
void IntraBWDataflow::setLatticePost(NodeState *state, map<PartEdgePtr, vector<Lattice*> >& dfInfo, bool overwrite) { 
  if(overwrite) state->copyLatticesOW(state->getLatticeAboveAllMod(this), dfInfo);
  else          state->copyLattices  (state->getLatticeAboveAllMod(this), dfInfo);
}

/*void IntraFWDataflow::transferFunctionCall(const Function &caller, PartPtr callPart, CFGNode callCFG, NodeState *state)
{
  vector<Lattice*> dfInfoBelow = state->getLatticeBelow(this);

  //vector<Lattice*>* retState = NULL;
  dynamic_cast<InterProceduralDataflow*>(interAnalysis)->
    transfer(caller, callPart, callCFG, *state, dfInfoBelow);
}

void IntraBWDataflow::transferFunctionCall(const Function &caller, PartPtr callPart, CFGNode callCFG, NodeState *state)
{
  //vector<Lattice*>* retState;
  dynamic_cast<InterProceduralDataflow*>(interAnalysis)->
    transfer(caller, callPart, callCFG, *state, state->getLatticeAboveMod(this));
                                
  // NEED TO INCORPORATE INFORMATION ABOUT RETURN INTO DATAFLOW SOMEHOW
}*/

list<PartPtr> IntraFWDataflow::getDescendants(PartPtr part)
{ 
  list<PartPtr> descendants;
  list<PartEdgePtr> outEdges = part->outEdges();
  for(list<PartEdgePtr>::iterator ei=outEdges.begin(); ei!=outEdges.end(); ei++)
    descendants.push_back((*ei)->target());
  return descendants;
}

list<PartPtr> IntraBWDataflow::getDescendants(PartPtr part)
{ 
  list<PartPtr> descendants;
  list<PartEdgePtr> inEdges = part->inEdges();
  for(list<PartEdgePtr>::iterator ei=inEdges.begin(); ei!=inEdges.end(); ei++)
    descendants.push_back((*ei)->source());
  return descendants;
}

list<PartEdgePtr> IntraFWDataflow::getEdgesToDescendants(PartPtr part)
{ 
  return part->outEdges();
}

list<PartEdgePtr> IntraBWDataflow::getEdgesToDescendants(PartPtr part)
{ 
  return part->inEdges();
}

PartPtr IntraFWDataflow::getUltimate(const Function &func)
{ return getComposer()->GetFunctionEndPart(func, this); }
PartPtr IntraBWDataflow::getUltimate(const Function &func)
{ return getComposer()->GetFunctionStartPart(func, this); }

dataflowPartIterator* IntraFWDataflow::getIterator(const Function &func)
{ return new dataflowPartIterator(getComposer()->GetFunctionEndPart(func, this)); }
dataflowPartIterator* IntraBWDataflow::getIterator(const Function &func)
{ return new back_dataflowPartIterator(getComposer()->GetFunctionStartPart(func, this)); }

// Runs the intra-procedural analysis on the given function. Returns true if 
// the function's NodeState gets modified as a result and false otherwise.
// state - the function's NodeState
// analyzeFromDirectionStart - If true the function should be analyzed from its starting point from the analysis' 
//    perspective (fw: entry point, bw: exit point)
void ComposedAnalysis::runAnalysis(const Function& func, NodeState* fState, bool analyzeFromDirectionStart, 
                                   set<Function> calleesUpdated)
{
  // Make sure that we've been paired with a valid inter-procedural dataflow analysis
  ROSE_ASSERT(dynamic_cast<InterProceduralDataflow*>(interAnalysis));

  ostringstream funcNameStr; funcNameStr << "Intra-analysis Function "<<func.get_name().getString()<<"()";
  Dbg::region reg(analysisDebugLevel, 1, Dbg::region::topLevel, funcNameStr.str());
  if(analysisDebugLevel>=1) {
      Dbg::dbg << "analyzeFromDirectionStart="<<analyzeFromDirectionStart<<" calleesUpdated=";
      for(set<Function>::iterator f=calleesUpdated.begin(); f!=calleesUpdated.end(); f++)
          Dbg::dbg << f->get_name().getString()<<", ";
      Dbg::dbg << endl;
  }

  // Set of all the Parts that have already been visited by the analysis
  set<PartPtr> visited;
  
  // Re-analyze it from scratch
  list<PartPtr> startingParts = getInitialWorklist(func, true, calleesUpdated, fState);
  PartPtr ultimatePart = getUltimate(func);
  for(list<PartPtr>::iterator start=startingParts.begin(); start!=startingParts.end(); start++) {
    ostringstream startStr; startStr << "Starting from "<<(*start)->str()<<endl;
    Dbg::region reg(analysisDebugLevel, 1, Dbg::region::topLevel, startStr.str());
    
    // Iterate over the nodes in this function that are downstream from the nodes added above
    dataflowPartIterator* curNodeIt = getIterator(func);
    Dbg::dbg << "Initial curNodeIt="<<curNodeIt->str()<<endl;
    curNodeIt->add(*start);
    while(*curNodeIt!=dataflowPartIterator::end())
    {
      PartPtr part = **curNodeIt;
      
      ostringstream nodeNameStr;
      nodeNameStr << "Current Part "<<part->str()<<endl; Dbg::region reg(analysisDebugLevel, 1, Dbg::region::topLevel, nodeNameStr.str());
      
      bool modified = false;
      bool firstVisit;
      if((firstVisit = (visited.find(part) == visited.end())))
        visited.insert(part);
      
      // The NodeState associated with this part
      NodeState* state = NodeState::getNodeState(this, part);
      //Dbg::dbg << "analysis="<<this<<"="<<((ComposedAnalysis*)this)<<"="<<str()<<" state="<<state<<"="<<state->str(this)<<endl;
      Dbg::dbg << "state="<<endl;
      Dbg::dbg << state->str()<<endl;
        
      map<PartEdgePtr, vector<Lattice*> >& dfInfoAnte = getLatticeAnte(state);
      // Create a local map for the post dataflow information. It will be deallocated 
      // at the end of the transfer function.
      map<PartEdgePtr, vector<Lattice*> > dfInfoPost;
      
      // Iterate over all the CFGNodes associated with this part and merge the result of applying to transfer function
      // to all of them
      set<CFGNode> v=part->CFGNodes();
      for(set<CFGNode>::iterator c=v.begin(); c!=v.end(); c++) {
        SgNode* sgn = c->getNode();
        
        ostringstream nodeNameStr; nodeNameStr << "Current CFGNode "<<part->str()<<endl;
        Dbg::region reg(analysisDebugLevel, (v.size()==1 ? 10: 1), Dbg::region::topLevel, nodeNameStr.str());
        
        // =================== Copy incoming lattices to outgoing lattices ===================
        // For the case where dfInfoPost needs to be created fresh, this shared pointer dfInfoPostPtr will ensure that 
        // the map is deallocated when dfInfoPostPtr goes out of scope.
        boost::shared_ptr<map<PartEdgePtr, vector<Lattice*> > > dfInfoPostPtr;
        // Overwrite the Lattices below this node with the lattices above this node.
        // The transfer function will then operate on these Lattices to produce the
        // correct state below this node.

        //printf("                 dfInfoAnte.size()=%d, dfInfoPost.size()=%d, this=%p\n", dfInfoAnte.size(), dfInfoPost.size(), this);
        if(c==v.begin()) {
          if(analysisDebugLevel>=1) {
             Dbg::dbg << "==================================  "<<endl;
             Dbg::dbg << "  Copying incoming Lattice :"<<endl;
             {Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg <<NodeState::str(dfInfoAnte); }
             Dbg::dbg << "  To outgoing Lattice: "<<endl;
             {Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg <<NodeState::str(dfInfoPost); }
          }
          
          // Over-write the post information with the ante information, creating it if it doesn't exist yet
          /*if(dfInfoPost.size()==0) */NodeState::copyLatticesOW(dfInfoPost, dfInfoAnte);
          // GB 2012-09-28: Do we even need to keep post information around after the transfer 
          //                function is done or can we just deallocate it?
          //else                     NodeState::copyLattices  (dfInfoPost, dfInfoAnte);
        // If this is not the first CFGNode, create a new post state. It will be merged into the lattices in 
        // the NodeState after the transfer function
        } else {
          // Since this is not the first CFGNode within Part p, create a new post state for it
          dfInfoPostPtr = boost::make_shared<map<PartEdgePtr, vector<Lattice*> > >();
          dfInfoPost = *dfInfoPostPtr.get();
          
          Dbg::dbg << "=================================="<<endl;
          Dbg::dbg << "Creating outgoing state from incoming state"<<endl;
          
          NodeState::copyLatticesOW(dfInfoPost, dfInfoAnte);
        }

        // <<<<<<<<<<<<<<<<<<< TRANSFER FUNCTION <<<<<<<<<<<<<<<<<<<

        if(analysisDebugLevel>=1) {
          Dbg::dbg << " ==================================  "<<endl;
          Dbg::dbg << "  Transferring the outgoing  Lattice ... "<<endl;
        }
        if (isSgFunctionCallExp(sgn))
          //transferFunctionCall(func, part, *c, state);
          dynamic_cast<InterProceduralDataflow*>(interAnalysis)->
                                                   transfer(func, part, *c, *state, dfInfoPost[NULLPartEdge]);
        
        // When a dfInfo map goes into a transfer function it must only have one key: the NULL edge
        ROSE_ASSERT(dfInfoPost.size()==1);
        ROSE_ASSERT(dfInfoPost.find(NULLPartEdge) != dfInfoPost.end());

        boost::shared_ptr<IntraDFTransferVisitor> transferVisitor = getTransferVisitor(func, part, *c, *state, dfInfoPost);
        sgn->accept(*transferVisitor);
        modified = transferVisitor->finish() || modified;
                
        // The transfer function must have either left dfInfoPost's NULL edge key alone or created one key for each
        // descendant edge
        list<PartEdgePtr> descEdges = getEdgesToDescendants(part);
        // If the key is still the NULL edge
        if(dfInfoPost.size()==1 && (dfInfoPost.find(NULLPartEdge) != dfInfoPost.end())) {
          // Adjust dfInfoPost to make one copy of the value for each descendant edge
          
          Dbg::dbg << "Descendant edges: #descEdges="<<descEdges.size()<<endl;
          /*for(list<PartEdgePtr>::iterator e=descEdges.begin(); e!=descEdges.end(); e++)
          { Dbg::indent ind; Dbg::dbg << ":" << (*e)->str() << endl; }*/
          Dbg::dbg << "dfInfoPost="<<endl;
          { Dbg::indent ind; Dbg::dbg << NodeState::str(dfInfoPost) << endl; }
          
          if(descEdges.size() > 0) {
            list<PartEdgePtr>::iterator first=descEdges.begin();
            // First, map the original key to the first descendant edge
            dfInfoPost[*first] = dfInfoPost[NULLPartEdge];
            dfInfoPost.erase(NULLPartEdge);

            // Set to *first the PartEdge of all the lattices stored under this edge
            for(vector<Lattice*>::iterator l=dfInfoPost[*first].begin(); l!=dfInfoPost[*first].end(); l++) {
              //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;first="<<(*first)->str()<<endl;
              (*l)->setPartEdge(*first); //modified = (*l)->setPartEdge(*first) || modified;
            }

            // Now copy its value to the other descendant edges
            list<PartEdgePtr>::iterator e=first;
            for(e++; e!=descEdges.end(); e++) {
              NodeState::copyLatticesOW(dfInfoPost, *e, dfInfoPost, *first, true);
              // Set to *e the PartEdge of all the lattices stored under this edge
              for(vector<Lattice*>::iterator l=dfInfoPost[*e].begin(); l!=dfInfoPost[*e].end(); l++) {
                //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;e="<<(*e)->str()<<endl;
                (*l)->setPartEdge(*e);//modified = (*l)->setPartEdge(*e) || modified;
              }
            }
          }
        // If the key has been changed
        } else {
          // First, verify that it has been changed correctly
          ROSE_ASSERT(descEdges.size() == dfInfoPost.size());
          for(list<PartEdgePtr>::iterator e=descEdges.begin(); e!=descEdges.end(); e++)
            ROSE_ASSERT(dfInfoPost.find(*e) != dfInfoPost.end());
        }
        
        // >>>>>>>>>>>>>>>>>>> TRANSFER FUNCTION >>>>>>>>>>>>>>>>>>>
        if(analysisDebugLevel>=1) {
          Dbg::dbg << "Transferred: outgoing Lattice :"<<endl;
          {Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg <<NodeState::str(dfInfoPost); }
          Dbg::dbg << "Transferred: "<<(modified? "<font color=\"#990000\">Modified</font>": "<font color=\"#000000\">Not Modified</font>")<<endl;
        }
        
        // If this is not the first CFGNode within this Part, merge its outgoing lattices with the outgoing
        // lattices produced by the transfer function's execution on the prior CFGNodes in this Part
        if(c!=v.begin()) {
          if(analysisDebugLevel>=1) {
            Dbg::dbg << "==================================  "<<endl;
            Dbg::dbg << "Merging lattice for prior CFGNodes:"<<endl;
            {Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg <<NodeState::str(getLatticePost(state)); }
            Dbg::dbg << "With lattice  for the current CFGNodes:"<<endl;
            {Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg <<NodeState::str(dfInfoPost); }
          }
          ROSE_ASSERT(getLatticePost(state).begin()->first == NULLPartEdge);
          modified = NodeState::unionLatticeMaps(getLatticePost(state), dfInfoPost) || modified;
          
          if(analysisDebugLevel>=1) {
            Dbg::dbg << "Merged within Part: Lattice"<<endl;
            {Dbg::indent ind(analysisDebugLevel, 1); Dbg::dbg <<NodeState::str(getLatticePost(state)); }
          }
        }
      } // for(vector<CFGNode>::iterator c=v.begin(); c!=v.end(); c++) {
      
      // =================== Populate the generated outgoing lattice to descendants (meetUpdate) ===================
      // If this is not the last node in the function
      
      if(**curNodeIt != ultimatePart)
      {
        list<PartPtr> descendants = getDescendants(part);
        list<PartEdgePtr> descEdges = getEdgesToDescendants(part);
        
        if(analysisDebugLevel>=1){
          Dbg::dbg << " ==================================  "<<endl;
          Dbg::dbg << " Propagating/Merging the outgoing  Lattice to all descendant nodes("<<descendants.size()<<")"<<endl;
        }
        Dbg::indent ind(analysisDebugLevel, 1);
        
        // Iterate over all descendants
        list<PartPtr>::iterator d;
        list<PartEdgePtr>::iterator de;
        for(d = descendants.begin(), de = descEdges.begin(); d != descendants.end(); d++, de++)
        {
          // The CFG node corresponding to the current descendant of n
          PartPtr nextNode = *d;
          if(analysisDebugLevel>=1) Dbg::dbg << "    Descendant: "<<nextNode->str()<<endl;
          
          NodeState* nextState = NodeState::getNodeState(this, nextNode);
          
          // Make sure that dfInfoPost has a key for this descendant
          ROSE_ASSERT(dfInfoPost.find(*de) != dfInfoPost.end());
                  
          // The temporary dfInfo map for this descendant will have its Lattice* vector from dfInfoPost mapped 
          // under the NULL edge key
          map<PartEdgePtr, vector<Lattice*> > dfInfoNext;
          dfInfoNext[NULLPartEdge] = dfInfoPost[*de];
          
          // Propagate the Lattices below this node to its descendant
          modified = propagateStateToNextNode(dfInfoNext, part, getLatticeAnte(nextState), nextNode);
          if(analysisDebugLevel>=1){
            Dbg::dbg << "Propagated/merged: "<<(modified? "<font color=\"#990000\">Modified</font>": "<font color=\"#000000\">Not Modified</font>")<<endl;
            Dbg::dbg << "<hline>";
          }
          // If the next node's state gets modified as a result of the propagation, or the next node has not yet been
          // visited, add it to the processing queue.
          Dbg::dbg << "Final modified="<<modified<<", visited="<<(visited.find(nextNode)!=visited.end())<<" nextNode="<<nextNode->str()<<endl;
          if(modified || visited.find(nextNode)==visited.end())
            curNodeIt->add(nextNode);
        }
      }
      
      /*// Deallocate dfInfoPost and its Lattices
      for(map<PartEdgePtr, vector<Lattice*> >::iterator e=dfInfoPost.begin(); e!=dfInfoPost.end(); e++)
        for(vector<Lattice*>::iterator l=e->second.begin(); l!=e->second.end(); l++)
          delete *l;*/
      // Save dfInfoPost in the NodeState
      setLatticePost(state, dfInfoPost, firstVisit);
      
      (*curNodeIt)++;
    } // while(curNodeIt!=dataflowPartIterator::end())
  } // for(list<PartPtr>::iterator start=startingParts.begin(); start!=startingParts.end(); start++) {
  
  Dbg::dbg << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ComposedAnalysis::runAnalysis" << endl;
}

}; // namespace dataflow
