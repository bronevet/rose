#if !defined(FUNCTION_STATE_H) && !defined(NO_FUNCTION_STATE_H)
#define FUNCTION_STATE_H

#include "analysisCommon.h"
#include "nodeState.h"
#include "CallGraphTraverse.h"
#include "compose.h"
#include "abstract_object.h"
#include <map>
#include <set>

namespace dataflow {
class FunctionState
{
  friend class CollectFunctions;
  public:
  Function func;
  NodeState state;
  // The lattices that describe the value of the function's return variables
  //NodeState retState;

  private:
  static std::set<FunctionState*> allDefinedFuncs;        
  static std::set<FunctionState*> allFuncs;
  static bool allFuncsComputed;

  public:
  FunctionState(Function &func): 
          func(func),
          state(/*func.get_declaration()->cfgForBeginning()*/)
  {}

  Function& getFunc();

  // returns a set of all the functions whose bodies are in the project
  static std::set<FunctionState*>& getAllDefinedFuncs();

  // returns a set of all the functions whose declarations are in the project
  static std::set<FunctionState*>& getAllFuncs();

  // returns the FunctionState associated with the given function
  // func may be any defined function
  static FunctionState* getDefinedFuncState(const Function& func);

  // returns the FunctionState associated with the given function
  // func may be any declared function
  static FunctionState* getFuncState(const Function& func);

  /* These functions support inter-procedural transfer of information from callers to callees and back.
   * Forwards analyses:
   *  caller->callee: Need to map all the MemLocObjects of the call's arguments to the MemLocObjects of the 
   *                  corresponding function parameters.
   *  callee->caller: Need to map all the MemLocObjects of the function's by-reference parameters to the
   *                  MemLocObjects of the function call's arguments and map the union of the returned values
   *                  that is assumed to be associated with the expression MemLocObject of the function's declaration 
   *                  to the MemLocObject of the SgFunctionCallExp of the call.
   * Backwards analyses:
   * caller->callee: reverse of forward callee->caller
   * callee->caller: reverse of forward caller->callee */

  // Given a function call, sets argParamMap to map all arguments of this function call to their 
  // corresponding parameters.
  // Supports caller->callee transfers for forwards analyses and callee->caller transfers for backwards analyses.
  static void setArgParamMap(PartPtr callPart, SgFunctionCallExp* call, 
                             std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >& argParamMap,
                             Composer* composer, ComposedAnalysis* analysis);

  // Given a function call, sets paramArgByRef2ParamMap to map all arguments of this function call that are passed by 
  // reference to their corresponding parameters and to map the call's SgFunctionCallExp expression to the MemLocObject 
  // that denotes the function's declaration (associated with its return value).
  // Supports callee->caller transfers for forwards analyses and caller->callee transfers for backwards analyses.
  static void setArgByRef2ParamMap(PartPtr callPart, SgFunctionCallExp* call, 
                                   std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >& paramArgByRef2ParamMap,
                                   Composer* composer, ComposedAnalysis* analysis);

  // Given a map produced by setArgParamMap or setArgByRef2ParamMap, return the same map but where the key->value 
  // mappings are inverted to value->key
  static std::set<pair<MemLocObjectPtr, MemLocObjectPtr> > 
          invertArg2ParamMap(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> > ml2ml);
};

class CollectFunctions : public TraverseCallGraphUnordered/*TraverseCallGraphBottomUp<int>*/
{
        public:
        CollectFunctions(SgIncidenceDirectedGraph* graph) : TraverseCallGraphUnordered/*TraverseCallGraphBottomUp<int>*/(graph)
        {}
        
        //int visit(const CGFunction* func, list<int> fromCallees);
        void visit(const CGFunction* func);
        
        virtual ~CollectFunctions() {}
};
}; // namespace dataflow
#endif
