#include "functionState.h"
#include "cfgUtils.h"
#include  "abstract_object.h"
#include "analysis.h"
#include <map>
using std::map;
#include <set>
using std::set;

namespace dataflow {
/*********************
 *** FunctionState ***
 *********************/

Function& FunctionState::getFunc()
{
  return func;
}

set<FunctionState*> FunctionState::allDefinedFuncs;     
set<FunctionState*> FunctionState::allFuncs;
bool FunctionState::allFuncsComputed=false;
  
// returns a set of all the functions whose bodies are in the project
set<FunctionState*>& FunctionState::getAllDefinedFuncs()
{
  if(allFuncsComputed)
    return allDefinedFuncs;
  else
  {
    CollectFunctions collect(getCallGraph());
    collect.traverse();
    allFuncsComputed=true;
    return allFuncs;
  }
}

// returns a set of all the functions whose declarations are in the project
set<FunctionState*>& FunctionState::getAllFuncs()
{
  /*if(allFuncsComputed)
    return allFuncs;
  else*/
  if(!allFuncsComputed)
  {
    CollectFunctions collect(getCallGraph());
    collect.traverse();
    allFuncsComputed=true;
  }

  return allFuncs;
}

// returns the FunctionState associated with the given function
// func may be any defined function
FunctionState* FunctionState::getDefinedFuncState(const Function& func)
{
  for(set<FunctionState*>::iterator it=allDefinedFuncs.begin(); it!=allDefinedFuncs.end(); it++)  
    if((*it)->func == func)
      return *it;
  return NULL;
}

// returns the FunctionState associated with the given function
// func may be any declared function
FunctionState* FunctionState::getFuncState(const Function& func)
{
  if (!allFuncsComputed) // Liao, 4/6/2012, make sure the internal set is computed first.
  {
    getAllFuncs();
  }

  for(set<FunctionState*>::iterator it=allFuncs.begin(); it!=allFuncs.end(); it++)  
    if((*it)->func == func)
      return *it;
  return NULL;
}

// Given a function call, sets argParamMap to map all arguments of this function call to their 
// corresponding parameters.
// Supports caller->callee transfers for forwards analyses and callee->caller transfers for backwards analyses.
void FunctionState::setArgParamMap(PartPtr callPart, SgFunctionCallExp* call, 
           std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >& argParamMap,
           Composer* composer, ComposedAnalysis* analysis)
{
  Dbg::indent(analysisDebugLevel, 1);
  Function func(call);
  // Part that corresponds to the function, which for now is set to be the start of its definition
  PartPtr funcNode = analysis->getComposer()->GetFunctionStartPart(func, analysis);

  SgExpressionPtrList args = call->get_args()->get_expressions();
  //SgInitializedNamePtrList params = funcArgToParamByRef(call);
  SgInitializedNamePtrList params = func.get_params();
  /*cout << "callPart="<<callPart->str()<<endl;
  cout << "call="<<cfgUtils::SgNode2Str(call)<<endl;
  cout << "args="<<endl;
  for(SgExpressionPtrList::iterator a=args.begin(); a!=args.end(); a++)
    cout << "    "<<cfgUtils::SgNode2Str(*a)<<endl;
  cout << "params="<<endl;
  for(SgInitializedNamePtrList::iterator p=params.begin(); p!=params.end(); p++)
    cout << "    "<<cfgUtils::SgNode2Str(*p)<<endl;*/
  ROSE_ASSERT(args.size() == params.size());

  //cout << "setArgParamMap() #args="<<args.size()<<" #params="<<params.size()<<"\n";
  // the state of the callee's variables at the call site
  SgExpressionPtrList::iterator itA;
  SgInitializedNamePtrList::iterator itP;
  for(itA = args.begin(), itP = params.begin(); 
      itA!=args.end() && itP!=params.end(); 
      itA++, itP++)
  {
    // MemLocObjectPtrPair argP = composer->Expr2MemLoc(*itA, funcNode->inEdgeFromAny(), analysis);
    // The argument MemLoc is preferrably the argument expression but may be a memory location if the expression is not available
    MemLocObjectPtr arg = composer->Expr2MemLoc(*itA, funcNode->inEdgeFromAny(), analysis);
    // if(argP.expr) arg = argP.expr;
    // else    arg = argP.mem;
    
    if(analysisDebugLevel>=1) Dbg::dbg << "argParamMap["<<arg->str()<<"]="<< composer->Expr2MemLoc(*itP, funcNode->inEdgeFromAny(), analysis)->str()<<endl;
    argParamMap.insert(make_pair(arg,
         composer->Expr2MemLoc(*itP, funcNode->inEdgeFromAny(), analysis)));
  }
}

// Given a function call, sets paramArgByRef2ParamMap to map all arguments of this function call that are passed by 
// reference to their corresponding parameters and to map the call's SgFunctionCallExp expression to the MemLocObject 
// that denotes the function's declaration (associated with its return value).
// Supports callee->caller transfers for forwards analyses and caller->callee transfers for backwards analyses.
void FunctionState::setArgByRef2ParamMap(PartPtr callPart, SgFunctionCallExp* call, 
           std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >& paramArgByRef2ParamMap,
           Composer* composer, ComposedAnalysis* analysis)
{
  Dbg::indent(analysisDebugLevel, 1);
  Function func(call);
  // Part that corresponds to the function, which for now is set to be the start of its definition
  PartPtr funcNode = analysis->getComposer()->GetFunctionStartPart(func, analysis);

  SgExpressionPtrList args = call->get_args()->get_expressions(); 
  SgInitializedNamePtrList params = func.get_params();

  SgExpressionPtrList::iterator itArgs;
  SgInitializedNamePtrList::iterator itParams;
  //cout << "      #params="<<params.size()<<" #args="<<args.size()<<"\n";
  for(itParams = params.begin(), itArgs = args.begin(); 
      itParams!=params.end() && itArgs!=args.end(); 
      itParams++, itArgs++)
  {
    SgType* typeParam = (*itParams)->get_type();
    if(isSgReferenceType(typeParam)) {
  // If the current argument expression corresponds to a real memory location, make its key the MemLocObject 
  // that corresponds to its memory location
  /*Dbg::region reg(1,1, Dbg::region::midLevel, "setArgByRef2ParamMap");
  Dbg::dbg << "itParams=["<<(*itParams)->unparseToString()<<" | "<<(*itParams)->class_name()<<"]"<<endl;
  Dbg::dbg << "itParams MemLoc = "<<composer->Expr2MemLoc(*itParams, funcNode, analysis).strp(funcNode)<<endl;*/
  // if(isSgVarRefExp(*itArgs) || isSgPntrArrRefExp(*itArgs))
  //   paramArgByRef2ParamMap.insert(make_pair(composer->Expr2MemLoc(*itArgs, callPart->inEdgeFromAny(), analysis).mem,
  //             composer->Expr2MemLoc(*itParams, funcNode->outEdgeToAny(), analysis).mem));
  // // Otherwise, use the expression MemLocObject
  // else
  //   paramArgByRef2ParamMap.insert(make_pair(composer->Expr2MemLoc(*itArgs, callPart->inEdgeFromAny(), analysis).expr,
  //             composer->Expr2MemLoc(*itParams, funcNode->outEdgeToAny(), analysis).mem));
  //   }
      paramArgByRef2ParamMap.insert(make_pair(composer->Expr2MemLoc(*itArgs, callPart->inEdgeFromAny(), analysis),
                                              composer->Expr2MemLoc(*itParams, funcNode->outEdgeToAny(), analysis)));
    }
  
  // Add the mapping from the function's declaration (denotes the return value) to the function's call expression
  /*Dbg::dbg << "declSymbol=["<<func.get_declaration()->search_for_symbol_from_symbol_table()->unparseToString()<<" | "<<func.get_declaration()->search_for_symbol_from_symbol_table()->class_name()<<"]"<<endl;
  Dbg::dbg << "declSymbol MemLoc = "<<composer->Expr2MemLoc(func.get_declaration()->search_for_symbol_from_symbol_table(), funcNode, analysis).str()<<endl;
  Dbg::dbg << "declSymbol MemLoc (funcNode)= "<<composer->Expr2MemLoc(func.get_declaration()->search_for_symbol_from_symbol_table(), funcNode, analysis).strp(funcNode)<<endl;
  Dbg::dbg << "declSymbol MemLoc (callPart)= "<<composer->Expr2MemLoc(func.get_declaration()->search_for_symbol_from_symbol_table(), funcNode, analysis).strp(callPart)<<endl;*/

  // paramArgByRef2ParamMap.insert(make_pair(composer->Expr2MemLoc(call, callPart->inEdgeFromAny(), analysis).expr,
  //           composer->Expr2MemLoc(func.get_declaration()->search_for_symbol_from_symbol_table(), funcNode->outEdgeToAny(), analysis).mem));
  paramArgByRef2ParamMap.insert(make_pair(composer->Expr2MemLoc(call, callPart->inEdgeFromAny(), analysis),
                                          //composer->Expr2MemLoc(func.get_declaration()->search_for_symbol_from_symbol_table(), funcNode->outEdgeToAny(), analysis)
                                          boost::make_shared<FuncResultMemLocObject>(func)));

  }
}

// Given a map produced by setArgParamMap or setArgByRef2ParamMap, return the same map but where the key->value 
// mappings are inverted to value->key
std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >
  FunctionState::invertArg2ParamMap(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> > ml2ml)
{
  std::set<pair<MemLocObjectPtr, MemLocObjectPtr> > ret;
  for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::iterator m=ml2ml.begin(); m!=ml2ml.end(); m++)
    ret.insert(make_pair(m->second, m->first));
  return ret;
}

/************************
 *** CollectFunctions ***
 ************************/

//int CollectFunctions::visit(const CGFunction* cgFunc, list<int> fromCallees)
void CollectFunctions::visit(const CGFunction* cgFunc)
{
  Function func(cgFunc);
  FunctionState* fs = new FunctionState(func);
  
  //printf("CollectFunctions::visit func = %s\n", func.get_name().str());
  
  // if the function has a body
  if(func.get_definition())
    FunctionState::allDefinedFuncs.insert(fs);
  FunctionState::allFuncs.insert(fs);
  //return 0;
}
}; // namespace dataflow
