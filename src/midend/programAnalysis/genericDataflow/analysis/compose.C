#include "compose.h"
#include "stx_analysis.h"
#include "const_prop_analysis.h"
#include <boost/enable_shared_from_this.hpp>
#include "printAnalysisStates.h"

namespace dataflow
{
int composerDebugLevel=1;

// Implements a memory location abstraction focused on array access expressions. For
// each expression it uses the memory location and value abstractions implemented by 
// servers to determine whether two such expressions correspond to the same or 
// different locations. For other expressions directly uses the equality methods of 
// the memory location abstraction implemented by the server.
// WORK IN PROGRESS
/*class ArrayAccessAnalysis : ComposedAnalysis
{
   Composer* composer;
   ArrayAccessAnalysis(Composer* composer) : composer(composer) {}

   // This analysis performs no dataflow   
   void genInitState(PartPtr p, Lattice** initLattice, 
                     std::vector<NodeFact*>& initFacts) { }
   void transfer(SgNode &n, Part& p) { }
   
   CPValueObject* Expr2Val(SgExpression* e, PartPtr p) 
   {
      if(isSgArrayAccess(e)) {
         return new AAMemLocObject(
                     composer->Expr2MemLoc(isSgArrayAccess(e)->array_exp(), p, this)
                     composer->Expr2MemLoc(isSgArrayAccess(e)->index_exp(), p, this));
      } else {
         return new AAMemLocObject(composer->Expr2MemLoc(e, p, this), NULL);
      }
      return p.state[this].get(composer->Expr2MemLoc(e, p, this)));
   }
};

// Represents either (i) an expression of the form a[i] or (**q)[a+b*c] (field index!=NULL) or 
//                  (ii) any other expression that denotes a memory location (field index==NULL).
class AAMemLocObject : MemLocObject
{
   MemLocObject* array;
   ValueObject* index;
   
   AAMemLocObject(MemLocObject* array, ValueObject* index) : array(array), index(index)
   {
      // The array field must be non-NULL but the index field may be NULL to indicate 
      // that this memory  location doesn't correspond to an array index expression
      ROSE_ASSERT(array);   
   }
   
   // One AAMemLocObject MAY equal another if both their array memory location and 
   // their index values may be equal to each other
   bool mayEqual(AbstractObject* o)
   {
      AAMemLocObject* oAA;
      ROSE_ASSERT(oAA=dynamic_cast<AAMemLocObject*>(o));
      
      // If one object is an array expression and the other is not, the only conservative 
      // answer that can be provided is that they may be equal. We may be able to achieve 
      // more precision if we can evaluate expressions to see the aliasing relationships 
      // between the two objects
      if(index==NULL != o->index==NULL)
         return true;
      
      // If both are AAMemLocObjects, use their respective mayEqual methods
      return oAA->array->mayEqual(array) && 
            (index? oAA->index->mayEqual(index) : true);
   }
   
   // One AAMemLocObject MUST equal another if both their array memory location and their 
   // index values must be equal to each other
   bool mustEqual(AbstractObject* o)
   {
      AAMemLocObject* oAA;
      ROSE_ASSERT(oAA=dynamic_cast<AAMemLocObject*>(o));
      
      // If one object is an array expression and the other is not, we cannot conservatively 
      // guarantee that they must be equal. We may be able to aachieve more precision if we 
      // can evaluate expressions to see the aliasing relationships between the two objects
      if(index==NULL != o->index==NULL)
         return false;
      
      // If both are AAMemLocObjects, use their respective mustEqual methods
      return oAA->array->mustEqual(array) && oAA->index->mustEqual(index);
   }
   
   // Don't have good idea how to represent a finite number of options 
   
   // This AAMemLocObject corresponds to a finite number of memory locations if both its 
   // array and index are finite sets
   bool isFiniteSet()
   {
      return array->isFiniteSet() && index->isFiniteSet();
   }
   
   set<AbstractObj> getValueSet()
   {
      Not yet clear how to represent concrete memory locations. Are these guaranteed to be 
      lexical memory locations or should we keep them opaque and give them precise equality 
      operations?
   }* /
   
   string str(string& indent)
   {
      ostringstream oss;
      oss <<           "[AAMemLocObj: \n";
      oss << indent << "    array="<<array->str(indent+"          ")<<"\n";
      oss << indent << "    index="<<(index? index->str(indent+"          ") : "NULL")<<"]";
      return oss.str();
   }
};

// Backwards analysis that maintains a set of memory locations that are read along some 
// path that follows the current location before they are over-written. Implements a 
// MemLocObject that considers all dead objects as not may-equal and not must-equal to any 
// live object but are must-equal and may-equal to each other.
class LiveDeadAnalysis : ComposedAnalysis
{
   Composer* composer;
   LiveDeadAnalysis(Composer* composer) : composer(composer) {}
   
   void genInitState(PartPtr p, Lattice** initLattice, 
                     std::vector<NodeFact*>& initFacts) { 
      *initLattice = composer->NewMemLocSet();
   }
   
   void transfer(SgNode &n, Part& p) { 
      must implement
      for instance, to process an (SgAssignOp* op):
         // The lhs is dead but the rhs is live
         p.state[this].remove(composer->Expr2MemLoc(op->get_lhs_op(), p, this));
         p.state[this].insert(composer->Expr2MemLoc(op->get_rhs_op(), p, this));
   }
   
   LDMemLocObject* Expr2MemLoc(SgExpression* e, PartPtr p) {
      // If e corresponds to a live memory location, return an LDMemLocObject that 
      // corresponds to it
      if(p.state[this].contains(composer->Expr2MemLoc(e, p, this)))
         return new LDMemLocObject(composer->Expr2MemLoc(e, p, this))
      // Otherwise, return a dead LDMemLocObject
      else
         return new LDMemLocObject(NULL);
   }
};

// Represents either (i) a live memory location (field obj!=NULL) or
//                  (ii) a dead memory location (field obj==NULL).
class LDMemLocObject : MemLocObject
{
   MemLocObject* obj;
   
   LDMemLocObject(MemLocObject* obj) : obj(obj)
   { }
   
   // One LDMemLocObject MAY equal another if they're both live and the objects themselves 
   // report thatthey may be equal
   bool mayEqual(AbstractObject* o)
   {
      LDMemLocObject* oLD;
      ROSE_ASSERT(oLD=dynamic_cast<LDMemLocObject*>(o));
      
      return obj==NULL || oLD->obj==NULL ||
             obj->mayEqual(oLD);
   }
   
   // One LDMemLocObject MUST equal another if they're both live and the objects themselves 
   // report that they must be equal
   bool mustEqual(AbstractObject* o)
   {
      LDMemLocObject* oLD;
      ROSE_ASSERT(oLD=dynamic_cast<LDMemLocObject*>(o));
      
      return obj==NULL || oLD->obj==NULL ||
             obj->mustEqual(oLD);
   }
   
   // Don't have good idea how to represent a finite number of options 
   
   // This LDMemLocObject corresponds to a finite number of memory locations if it is dead 
   // or the object itself corresponds to a finite number of memory locations
   bool isFiniteSet()
   {
      return obj==NULL || obj->isFiniteSet();
   }
   
   set<AbstractObj> getValueSet()
   {
      if(obj==NULL) {
         return empty set;
      }
      Not yet clear how to represent concrete memory locations. Are these guaranteed to be 
      lexical memory locations or should we keep them opaque and give them precise equality 
      operations?
   } * /
   
   string str(string& indent)
   {
      ostringstream oss;
      oss << "[LDMemLocObject: \obj="<<(obj? obj->str(indent+"          ") : "NULL")<<"]";
      return oss.str();
   }
};*/

// ------------------------------
// ----- Composition Driver -----
// ------------------------------
  
ChainComposer::ChainComposer(int argc, char** argv, list<ComposedAnalysis*>& analyses) : allAnalyses(analyses)
{
  //cout << "#allAnalyses="<<allAnalyses.size()<<endl;
  // Create an instance of the syntactic analysis and insert it at the front of the list.
  // This analysis will be called last if matching functions do not exist in any other
  // analysis and does not need a round of fixed-point iteration to produce its results.
  allAnalyses.push_front((ComposedAnalysis*)new SyntacticAnalysis());
  
  // Inform each analysis of the composer's identity
  //cout << "#allAnalyses="<<allAnalyses.size()<<endl;
  for(list<ComposedAnalysis*>::iterator a=allAnalyses.begin(); a!=allAnalyses.end(); a++) {
    (*a)->setComposer(this);
  }
  
  project = frontend(argc,argv);
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
    ostringstream label; label << "Running Analysis " << (*a)->str("");
    if(composerDebugLevel>=1) Dbg::enterFunc(label.str());
    ContextInsensitiveInterProceduralDataflow inter_cc(*a, graph);
    inter_cc.runAnalysis();
    /*UnstructuredPassInterDataflow up_cc(*a);
    up_cc.runAnalysis();*/
    
    if(composerDebugLevel>=1) Dbg::exitFunc(label.str());

    if(composerDebugLevel >= 3) {
      Dbg::enterFunc(label.str()+" Final State");
      std::vector<int> factNames;
      std::vector<int> latticeNames;
      latticeNames.push_back(0);
      printAnalysisStates paa(*a, factNames, latticeNames, printAnalysisStates::below, "::::");
      UnstructuredPassInterAnalysis up_paa(paa);
      up_paa.runAnalysis();
      Dbg::exitFunc(label.str()+" Final State");
    }
    
    // Record that we've completed the given analysis
    doneAnalyses.push_back(*a);
  }
  
  return;
}
 
// Generic function that looks up the composition chain from the given client 
// analysis and returns the result produced by the first instance of the function 
// called by the caller object found along the way.
template<class RetObject>
boost::shared_ptr<RetObject> ChainComposer::callServerAnalysisFunc(SgNode* n, PartPtr p, ComposedAnalysis* client, 
                                   FuncCaller<RetObject>& caller) {
  if(composerDebugLevel>=1) Dbg::dbg << "ChainComposer::callServerAnalysisFunc() "<<caller.funcName()<<" #doneAnalyses="<<doneAnalyses.size()<<endl;
  /*for(list<ComposedAnalysis*>::reverse_iterator a=doneAnalyses.rbegin(); a!=doneAnalyses.rend(); a++) {
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<(*a)->str("")<<endl;
  }*/
  list<ComposedAnalysis*> doneAnalyses_back;
  // Iterate backwards looking for an analysis that implements caller() behind in the chain of completed analyses
  list<ComposedAnalysis*>::reverse_iterator a=doneAnalyses.rbegin();
  while(doneAnalyses.size() >= 0) {
    ostringstream label; label << caller.funcName() << "  : " << (*a)->str("") << " ("<<n->class_name()<<" | "<<Dbg::escape(n->unparseToString())<<")";
    if(composerDebugLevel>=1) Dbg::enterFunc(label.str());
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
      boost::shared_ptr<RetObject> v(caller(n, p, curAnalysis));
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
      if(composerDebugLevel>=1) Dbg::dbg << "Returning "<<v->strp(p, "")<<endl;
      if(composerDebugLevel>=1) Dbg::exitFunc(label.str());
      return v;
    } catch (NotImplementedException exc) {
      if(composerDebugLevel>=1) Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<caller.funcName()<<" Not Implemented. Advancing to "<<(*a)->str("")<<endl;
      if(composerDebugLevel>=1) Dbg::exitFunc(label.str());
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


// -------------------------------------
// ----- Expression Interpretation -----
// -------------------------------------

// These classes wrap the functionality of calling a specific function within an 
// Analysis or a Part
class Expr2ValCaller : public FuncCaller<ValueObject>
{
  public:
  // Calls the given analysis' implementation of Expr2Val within the given node
  boost::shared_ptr<ValueObject> operator()(SgNode* n, PartPtr p, ComposedAnalysis* a)
  { 
      //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Expr2ValCaller("<<a->str("")<<")"<<endl;
      return a->Expr2Val(n, p); }
  
  string funcName() { return "Expr2Val"; }
};

class Expr2MemLocCaller : public FuncCaller<MemLocObject>
{
  public:
  // Calls the given analysis' implementation of Expr2MemLoc within the given node
  boost::shared_ptr<MemLocObject> operator()(SgNode* n, PartPtr p, ComposedAnalysis* a)
  { return a->Expr2MemLoc(n, p); }
  
  string funcName() { return "Expr2MemLoc"; }
};

class Expr2CodeLocCaller : public FuncCaller<CodeLocObject>
{
  public:
  // Calls the given analysis' implementation of Expr2CodeLoc within the given node
  boost::shared_ptr<CodeLocObject> operator()(SgNode* n, PartPtr p, ComposedAnalysis* a)
  { return a->Expr2CodeLoc(n, p); }
  
  string funcName() { return "Expr2CodeLoc"; }
};

ValueObjectPtr ChainComposer::Expr2Val(SgNode* n, PartPtr p, ComposedAnalysis* client) { 
  Expr2ValCaller c;
  return callServerAnalysisFunc<ValueObject>(n, p, client, c);
}

MemLocObjectPtrPair ChainComposer::Expr2MemLoc(SgNode* n, PartPtr p, ComposedAnalysis* client) { 
  // Return the pair of <object that specifies the expression temporary of n, 
  //                     object that specifies the memory location that n corresponds to>
  Expr2MemLocCaller c;
  MemLocObjectPtr mem = callServerAnalysisFunc<MemLocObject>(n, p, client, c);
  // If mem is an expression object returned by the syntactic analysis, there is no object that
  // specifies n's memory location
  if(boost::dynamic_pointer_cast<ExprObj>(mem))
    // Return mem as n's expression object and do not return an object for n's memory location
    return MemLocObjectPtrPair(mem, MemLocObjectPtr());
  // If mem actually corresponds to a location in memory 
  else
    // Generate a fresh object for n's expression temporary and return it along with mem
    return MemLocObjectPtrPair(
              isSgExpression(n) && !isSgVarRefExp(n) ? 
                createExpressionMemLocObject(isSgExpression(n), isSgExpression(n)->get_type(), p) :
                MemLocObjectPtr(),
              mem);
}

CodeLocObjectPtrPair ChainComposer::Expr2CodeLoc(SgNode* n, PartPtr p, ComposedAnalysis* client) { 
  // Return the pair of <object that specifies the expression temporary of n, 
  //                     object that specifies the memory location that n corresponds to>
  // GB: !!! Right now we don't have a firm idea of how to manage CodeLocObjects and have not yet implemented
  //     !!! an ExprObj for them. When we have done so, this code will likely mirror the code for Expr2MemLoc.
  Expr2CodeLocCaller c;
  return CodeLocObjectPtrPair(boost::make_shared<StxCodeLocObject>(n, p),
                              callServerAnalysisFunc<CodeLocObject>(n, p, client, c));
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

}; //namespace dataflow;
