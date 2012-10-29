#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "partitionIterator.h"
#include "cfgUtils.h"
#include "CallGraphTraverse.h"
#include "analysisCommon.h"

namespace dataflow {
class Analysis;
};

#include "lattice.h"
#include "nodeState.h"
//#include "variables.h"
#include "partitions.h"
//#include "varSets.h"
#include <vector>
#include <set>
#include <map>

namespace dataflow {

extern int analysisDebugLevel;

// Returns a boost shared pointers to variables that should be
// deallocated when the shared_ptr's reference count drops. Useful
// for passing unmanaged memory to code that takes boost shared 
// pointers as arguments.
void dummy_dealloc(int *);
#ifdef ANALYSIS_C
void dummy_dealloc(int *) {} // Deallocator that does nothing
#endif
template <class RefType>
boost::shared_ptr<RefType> shared_ptr_for_static(RefType& o) 
{
  return boost::shared_ptr<RefType>(&o, dummy_dealloc);
}

class Analysis
{
  public:
    // a filter function to decide which raw CFG node to show (if return true) or hide (otherwise)  
    // This is required to support custom filters of virtual CFG
    // Custom filter is set inside the intra-procedural analysis.
    // Inter-procedural analysis will copy the filter from its intra-procedural analysis during the call to its constructor.
    bool (*filter) (CFGNode cfgn); 
    Analysis(bool (*f)(CFGNode) = defaultFilter):filter(f) {}

    //virtual void method_to_enable_polymorphism() {}
};

class InterProceduralAnalysis;

class IntraProceduralAnalysis : virtual public Analysis
{
  protected:
  InterProceduralAnalysis* interAnalysis;
  
  public:
  void setInterAnalysis(InterProceduralAnalysis* interAnalysis)
  { this->interAnalysis = interAnalysis; }
  
  // runs the intra-procedural analysis on the given function, returns true if 
  // the function's NodeState gets modified as a result and false otherwise
  // state - the function's NodeState
  
  /* GB: For some reason the compiler complains that SyntacticAnalysis doesn't implement this
   when it is implemented in its ancestor IntraProceduralDataflow. As such, 
   a dummy implementation is provided here. */
  virtual void runAnalysis(const Function& func, NodeState* state);
  
  virtual ~IntraProceduralAnalysis();
};

class InterProceduralAnalysis : virtual public Analysis
{
  protected:
  IntraProceduralAnalysis* intraAnalysis;
  
  InterProceduralAnalysis(IntraProceduralAnalysis* intraAnalysis)
  {
    this->intraAnalysis = intraAnalysis; 
    // inform the intra-procedural analysis that this inter-procedural analysis will be running it
    intraAnalysis->setInterAnalysis(this);
  }
  
  virtual void runAnalysis()=0;
  
  virtual ~InterProceduralAnalysis();
};

/********************************
 *** UnstructuredPassAnalyses ***
 ********************************/
class ComposedAnalysis;

// A driver class which simply iterates through all CFG nodes of a specified function
class UnstructuredPassIntraAnalysis : virtual public IntraProceduralAnalysis
{
  public:
  ComposedAnalysis* analysis;
  
  UnstructuredPassIntraAnalysis(ComposedAnalysis* analysis) : analysis(analysis) {}
  
  // runs the intra-procedural analysis on the given function, returns true if 
  // the function's NodeState gets modified as a result and false otherwise
  // state - the function's NodeState
  void runAnalysis(const Function& func, NodeState* state);
  
  virtual void visit(const Function& func, PartPtr p, NodeState& state)=0;
};
// A driver class which simply iterates all function definitions one by one and call intra-procedural analysis on each of them.
class UnstructuredPassInterAnalysis : virtual public InterProceduralAnalysis
{
  public:
  UnstructuredPassInterAnalysis(IntraProceduralAnalysis& intraAnalysis) : InterProceduralAnalysis(&intraAnalysis)
  { }
    
  void runAnalysis();
};
}; // namespace dataflow
#endif
