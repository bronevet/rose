#ifndef DATAFLOW_H
#define DATAFLOW_H

#include "analysisCommon.h"
#include "nodeState.h"
#include "analysis.h"
#include "lattice.h"
#include "abstract_object.h"
#include <boost/shared_ptr.hpp>
#include <vector>
#include <set>
#include <map>
#include <string>

// !!! NOTE: THE CURRENT INTER-/INTRA-PROCEDURAL ANALYSIS API EFFECTIVELY ASSUMES THAT EACH ANALYSIS WILL BE EXECUTED
// !!!       ONCE BECAUSE DURING A GIVEN ANALYSIS PASS THE INTRA- ANALYSIS MAY ACCUMULATE STATE AND THERE IS NO
// !!!       API FUNCTION THAT THE INTER- ANALYSIS CAN USE THE RE-INITIALIZE THE STATE OF THE INTRA- ANALYSIS.
namespace dataflow {
  
/*************************
 *** Dataflow Analyses ***
 *************************/
class InterProceduralDataflow;

class IntraProceduralDataflow : virtual public IntraProceduralAnalysis
{
  public:
  // Set of functions that have already been visited by this analysis, used
  // to make sure that the dataflow state of previously-visited functions is
  // not re-initialized when they are visited again.
  std::set<Function> visited;

  void setInterAnalysis(InterProceduralDataflow* interDataflowAnalysis)
  { this->interAnalysis = (InterProceduralAnalysis*)interDataflowAnalysis; }

  void setInterAnalysis(IntraProceduralDataflow* intraDFAnalysis)
  { this->interAnalysis = intraDFAnalysis->interAnalysis; }

  // Dataflow version of the function that intra-procedural analysis on the given function.
  // Takes in:
  //    state - the function's NodeState
  //    analyzeDueToCallers - true if this function is analyzed due to changes in the the dataflow state from
  //       its caller functions at their call sites to this function
  //    calleesUpdated - true if the function is analyzed due to changes of dataflow state of functions called
  //       by this function at their exit points (i.e. points where this state affects the caller)
  // Returns true if the function's NodeState gets modified as a result and false otherwise
  virtual void runAnalysis(const Function& func, NodeState* state, bool analyzeDueToCallers, std::set<Function> calleesUpdated)=0;

  // Calls the full dataflow runAnalysis with dummy arguments to make it possible to use IntraProceduralDataflow
  // as if it were an IntraProceduralAnalysis
  void runAnalysis(const Function& func, NodeState* state)
  {
    // Each function is analyzed as if it were called directly by the language's runtime, ignoring
    // the application's actual call graph
    bool analyzeDueToCallers = true;

    // We ignore the application's call graph, so it doesn't matter whether this function calls other functions
    std::set<Function> calleesUpdated;

    runAnalysis(func, state, analyzeDueToCallers, calleesUpdated);
  }

  InterProceduralDataflow* getInterAnalysis() const
  {
    return (InterProceduralDataflow*)interAnalysis;
  }
};

/// Apply an analysis A's transfer function at a particular AST node type
class IntraDFTransferVisitor : public ROSE_VisitorPatternDefaultBase
{
  protected:
  // Common arguments to the underlying transfer function
  const Function &func;
  PartPtr part;
  CFGNode cn;
  NodeState &nodeState;
  std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo;

  public:
  IntraDFTransferVisitor(const Function &f, PartPtr p, CFGNode cn, NodeState &s, 
                         std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo)
    : func(f), part(p), nodeState(s), dfInfo(dfInfo)
  { }

  virtual bool finish() = 0;
  virtual ~IntraDFTransferVisitor() { }
};

class IntraUnitDataflow : virtual public IntraProceduralDataflow
{
  public:

  // the transfer function that is applied to every node
  // part - The Part that is being processed
  // state - the NodeState object that describes the state of the node, as established by earlier
  //   analysis passes
  // dfInfo - The Lattices that this transfer function operates on. The function take a map of lattices, one for
  //   each edge that departs from this part (outgoing for forward analyses and incoming for backwards)
  //   as input and overwrites them with the result of the transfer.
  // Returns true if any of the input lattices changed as a result of the transfer function and
  //    false otherwise.
  virtual bool transfer(const Function& func, PartPtr part, CFGNode cn, NodeState& state, 
      std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo)=0;

  class DefaultTransfer : public IntraDFTransferVisitor
  {
    bool modified;
    IntraUnitDataflow *analysis;
    public:
    DefaultTransfer(const Function& func, PartPtr part, CFGNode cn, NodeState& state, 
        std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo, IntraUnitDataflow *a)
      : IntraDFTransferVisitor(func, part, cn, state, dfInfo), modified(false), analysis(a)
      { }

    void visit(SgNode *n) { modified = analysis->transfer(func, part, cn, nodeState, dfInfo); }
    bool finish() { return modified; }
  };


  // \todo \pp IMO. the function getTransferVisitor is not necessary and can be removed.
  //     Users wanting to write the analysis based on visitors can do so
  //     in the transfer function. (This safes one memory allocation, deallocation,
  //     and boost::shared_pointer management overhead per transfer).
  //     A transfer function using the visitor would look like (if desired this can be
  //     simplified by providing a convenience function taking a visitor as argument):
  // \code
  //     virtual bool transfer(const Function& func, PartPtr p, NodeState& state, const std::vector<Lattice*>& dfInfo, std::vector<Lattice*>** retState, bool fw)
  //     {
  //       MyTransferVisitor visitor(myarguments, func, n, ...);
  //       n.getNode().accept(visitor);
  //       return visitor.finish();
  //     }
  // \endcode
  virtual boost::shared_ptr<IntraDFTransferVisitor> getTransferVisitor(
                const Function& func, PartPtr part, CFGNode cn,
                NodeState& state, 
                std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo)
  { return boost::shared_ptr<IntraDFTransferVisitor>(new DefaultTransfer(func, part, cn, state, dfInfo, this)); }
};

} // namespace dataflow;
#endif
