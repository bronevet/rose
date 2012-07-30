#ifndef LIVEDEADMEMANALYSIS_H
#define LIVEDEADMEMANALYSIS_H

#include "compose.h"
#include "abstract_object_set.h"

#include <map>
#include <set>
#include <vector>
#include <string>
#include <iostream>
#include <boost/shared_ptr.hpp>

/* Limitations
   ===========
 * - Variable expressions get declared dead at SgInitializedName nodes as do the expressions for arrays.
 *   However, objects for array[i] are not declared dead at array's SgInitializedName.
 * - Some SgNodes such as SgCast are omitted from the CFGNode traversal. However, since they may appear
 *   as operands, ExprObjs for them do get introduced but never get killed since their SgNodes are never visited.
 */

namespace dataflow {
extern int liveDeadAnalysisDebugLevel;

// Virtual class that allows users of the LiveDeadVarsAnalysis to mark certain variables as 
// being used inside a function call if the function's body is not available.
// NOTE: not sure where the definition exists for this class
class funcSideEffectUses
{
public:
    // Returns the set of variables that are used in a call to the given function for which a body has not been provided.
    // The function is also provided with the Part where the function was called, as well as its state.
    virtual std::set<MemLocObjectPtr> usedVarsInFunc(const Function& func, PartPtr p, NodeState& state)=0;
};

class LiveDeadMemTransfer : public IntraDFTransferVisitor
{
    std::string indent;
    AbstractObjectSet* liveLat;
    ComposerExpr2MemLocPtr ceml;

    bool modified;
    /*// Expressions that are assigned by the current operation
    std::set<SgExpression*>   assignedExprs;
    // Variables that are assigned by the current operation
    std::set<MemLocObjectPtr> assignedMem;
    // Variables that are used/read by the current operation
    std::set<MemLocObjectPtr> usedMem;*/
    AbstractObjectSet assigned;
    AbstractObjectSet used;
    
    PartPtr part;

    funcSideEffectUses *fseu;

    friend class LDMAExpressionTransfer;

    // Note that the variable corresponding to this expression is assigned
    void assign(SgExpression *);
    void assign(AbstractObjectPtr mem);
    
    // Note that the variable corresponding to this expression is used
    void use(SgExpression *);
    void use(AbstractObjectPtr mem);

public:
LiveDeadMemTransfer(const Function &f, PartPtr p, NodeState &s, const std::vector<Lattice*> &d, ComposerExpr2MemLocPtr ceml_, funcSideEffectUses *fseu_)
    : IntraDFTransferVisitor(f, p, s, d), indent("    "), 
    liveLat(dynamic_cast<AbstractObjectSet*>(*(dfInfo.begin()))), 
    ceml(ceml_), modified(false), fseu(fseu_), assigned(p), used(p), part(p)
    {
        if(liveDeadAnalysisDebugLevel>=1) Dbg::dbg << indent << "LiveDeadMemTransfer: liveLat="<<liveLat->str(indent + "    ")<<std::endl;
        // Make sure that all the lattice is initialized
        liveLat->initialize();
    }

    bool finish();

    void visit(SgExpression *);
    void visit(SgInitializedName *);
    void visit(SgReturnStmt *);
    void visit(SgExprStatement *);
    void visit(SgCaseOptionStmt *);
    void visit(SgIfStmt *);
    void visit(SgForStatement *);
    void visit(SgWhileStmt *);
    void visit(SgDoWhileStmt *);
};

class LDMemLocObject;

class LiveDeadMemAnalysis : public ComposedAnalysis, public IntraBWDataflow
{
protected:
    funcSideEffectUses* fseu;
    ComposerExpr2MemLocPtr ceml;
    
public:
    LiveDeadMemAnalysis(funcSideEffectUses* fseu=NULL);
    
    // Generates the initial lattice state for the given dataflow node, in the given function, with the given NodeState
    void genInitState(const Function& func, PartPtr p, const NodeState& state,
                      std::vector<Lattice*>& initLattices, std::vector<NodeFact*>& initFacts);
        
    boost::shared_ptr<IntraDFTransferVisitor> getTransferVisitor(const Function& func, PartPtr p,
                                                                 NodeState& state, const std::vector<Lattice*>& dfInfo)
    { return boost::shared_ptr<IntraDFTransferVisitor>(
    		new LiveDeadMemTransfer(func, p, state, dfInfo, 
    		                        ComposerExpr2MemLocPtr(new ComposerExpr2MemLoc(*getComposer(), p, *((ComposedAnalysis*)this))),
    		                        fseu)); }

    bool transfer(const Function& func, PartPtr p, NodeState& state, const std::vector<Lattice*>& dfInfo) { assert(0); return false; }
    
    // Maps the given SgNode to an implementation of the MemLocObject abstraction.
    MemLocObjectPtr Expr2MemLoc(SgNode* n, PartPtr p);
    
    // pretty print for the object
    std::string str(std::string indent="")
    { return "LiveDeadMemAnalysis"; }
};

class LDMemLocObject : public virtual MemLocObject
{
  protected:
  MemLocObjectPtr parent;
  LiveDeadMemAnalysis* ldva;
  PartPtr part;
  //bool iAmLive;

  public:
  LDMemLocObject(MemLocObjectPtr parent_, LiveDeadMemAnalysis* ldva, PartPtr p);
  LDMemLocObject(const LDMemLocObject& that);

  bool mayEqualML(MemLocObjectPtr o, PartPtr p) const;
  bool mustEqualML(MemLocObjectPtr o, PartPtr p) const;
  
  // pretty print for the object
  std::string str(std::string indent="") const;
  std::string str(std::string indent="") { return ((const LDMemLocObject*)this)->str(indent); }
  std::string strp(PartPtr part, std::string indent="");
  
  // Allocates a copy of this object and returns a pointer to it
  MemLocObjectPtr copyML() const;
  
  private:
  // Dummy Virtual method to make it possible to dynamic cast from LDMemLocObjectPtr to MemLocObjectPtr
  virtual void foo() {}
};
typedef boost::shared_ptr<LDMemLocObject> LDMemLocObjectPtr;

// Creates an instance of an LDMemLocObject that belongs to one of the MemLocObject categories
// (LDMemLocObject sub-classes): LDScalar, LDFunctionMemLoc, LDLabeledAggregate, LDArray or LDPointer.
LDMemLocObjectPtr createLDMemLocObjectCategory(MemLocObjectPtr parent, LiveDeadMemAnalysis* ldva, PartPtr p);

//memory object that has no internal structure
class LDScalar : virtual public LDMemLocObject, virtual public Scalar
{
 public:
   LDScalar(MemLocObjectPtr parent, LiveDeadMemAnalysis* ldva, PartPtr p);
   
  // Implement the required functions by calling the real copies in LDMemLocObject
  bool mayEqual(MemLocObjectPtr o, PartPtr p) const  { return LDMemLocObject::mayEqual(o, p); }
  bool mustEqual(MemLocObjectPtr o, PartPtr p) const { return LDMemLocObject::mayEqual(o, p); }
  std::string str(std::string indent="") const { return LDMemLocObject::str(indent); }
  std::string str(std::string indent="") { return LDMemLocObject::str(indent); }
  std::string strp(PartPtr part, std::string indent="") { return LDMemLocObject::strp(part, indent); }
  MemLocObjectPtr copyML() const { return LDMemLocObject::copyML(); }
};
typedef boost::shared_ptr<LDScalar> LDScalarPtr;

// memory object to model function objects in memory
class LDFunctionMemLoc: virtual public LDMemLocObject, virtual public FunctionMemLoc
{
public:  
  LDFunctionMemLoc(MemLocObjectPtr parent, LiveDeadMemAnalysis* ldva, PartPtr p);
  
  // Implement the required functions by calling the real copies in LDMemLocObject
  bool mayEqual(MemLocObjectPtr o, PartPtr p) const  { return LDMemLocObject::mayEqual(o, p); }
  bool mustEqual(MemLocObjectPtr o, PartPtr p) const { return LDMemLocObject::mayEqual(o, p); }
  std::string str(std::string indent="") const { return LDMemLocObject::str(indent); }
  std::string str(std::string indent="") { return LDMemLocObject::str(indent); }
  std::string strp(PartPtr part, std::string indent="") { return LDMemLocObject::strp(part, indent); }
  MemLocObjectPtr copyML() const { return LDMemLocObject::copyML(); }
};
typedef boost::shared_ptr<LDFunctionMemLoc> LDFunctionMemLocPtr;

// a memory object that contains a finite number of explicitly labeled memory objects, such as structs, classes and bitfields
class LDLabeledAggregate: virtual public LDMemLocObject, virtual public LabeledAggregate
{
 public:
   LDLabeledAggregate(MemLocObjectPtr parent, LiveDeadMemAnalysis* ldva, PartPtr p);
   
   // number of fields
   size_t fieldCount();

   // Returns a list of field
   std::vector<boost::shared_ptr<LabeledAggregateField> > getElements() const; 
   
  // Implement the required functions by calling the real copies in LDMemLocObject
  bool mayEqual(MemLocObjectPtr o, PartPtr p) const  { return LDMemLocObject::mayEqual(o, p); }
  bool mustEqual(MemLocObjectPtr o, PartPtr p) const { return LDMemLocObject::mayEqual(o, p); }
  std::string str(std::string indent="") const { return LDMemLocObject::str(indent); }
  std::string str(std::string indent="") { return LDMemLocObject::str(indent); }
  std::string strp(PartPtr part, std::string indent="") { return LDMemLocObject::strp(part, indent); }
  MemLocObjectPtr copyML() const { return LDMemLocObject::copyML(); }
};
typedef boost::shared_ptr<LDLabeledAggregate> LDLabeledAggregatePtr;

// Some programming languages don't have the concept of pointers. We provide explicit support for Array
class LDArray: virtual public LDMemLocObject, virtual public Array
{
 public:
   LDArray(MemLocObjectPtr parent, LiveDeadMemAnalysis* ldva, PartPtr p);
   
   // Returns a memory object that corresponds to all the elements in the given array
   MemLocObjectPtr getElements() ;
   // Returns the memory object that corresponds to the elements described by the given abstract index, 
   // which represents one or more indexes within the array
   MemLocObjectPtr getElements(IndexVectorPtr ai) ;

   // number of dimensions of the array
   size_t getNumDims();

   //--- pointer like semantics
   // support dereference of array object, similar to the dereference of pointer
   // Return the element object: array[0]
   MemLocObjectPtr getDereference();
   
  // Implement the required functions by calling the real copies in LDMemLocObject
  bool mayEqual(MemLocObjectPtr o, PartPtr p) const  { return LDMemLocObject::mayEqual(o, p); }
  bool mustEqual(MemLocObjectPtr o, PartPtr p) const { return LDMemLocObject::mayEqual(o, p); }
  std::string str(std::string indent="") const { return LDMemLocObject::str(indent); }
  std::string str(std::string indent="") { return LDMemLocObject::str(indent); }
  std::string strp(PartPtr part, std::string indent="") { return LDMemLocObject::strp(part, indent); }
  MemLocObjectPtr copyML() const { return LDMemLocObject::copyML(); }
};
typedef boost::shared_ptr<LDArray> LDArrayPtr;

class LDPointer: virtual public LDMemLocObject, virtual public Pointer
{
 public:
   LDPointer(MemLocObjectPtr parent, LiveDeadMemAnalysis* ldva, PartPtr p);
   
   MemLocObjectPtr getDereference() ;
   // Returns true if this pointer refers to the same abstract object as that pointer.
   bool equalPoints(const Pointer & that);
   
  // Implement the required functions by calling the real copies in LDMemLocObject
  bool mayEqual(MemLocObjectPtr o, PartPtr p) const  { return LDMemLocObject::mayEqual(o, p); }
  bool mustEqual(MemLocObjectPtr o, PartPtr p) const { return LDMemLocObject::mayEqual(o, p); }
  std::string str(std::string indent="") const { return LDMemLocObject::str(indent); }
  std::string str(std::string indent="") { return LDMemLocObject::str(indent); }
  std::string strp(PartPtr part, std::string indent="") { return LDMemLocObject::strp(part, indent); }
  MemLocObjectPtr copyML() const { return LDMemLocObject::copyML(); }
};
typedef boost::shared_ptr<LDPointer> LDPointerPtr;

// Initialize vars to hold all the variables and expressions that are live at PartPtr p
void getAllLiveMemAt(LiveDeadMemAnalysis* ldva, PartPtr p, const NodeState& state, std::set<AbstractObjectPtr>& vars, std::string indent="");

// Returns the set of variables and expressions that are live at PartPtr p
std::set<AbstractObjectPtr> getAllLiveMemAt(LiveDeadMemAnalysis* ldva, PartPtr p, const NodeState& state, std::string indent="");

// Returns true if the given MemLocObject must be live at the given Part
bool isLiveMust(MemLocObjectPtr mem, LiveDeadMemAnalysis* ldva, PartPtr p, const NodeState& state, string indent);

// Returns true if the given MemLocObject may be live at the given Part
bool isLiveMay(MemLocObjectPtr mem, LiveDeadMemAnalysis* ldva, PartPtr p, const NodeState& state, string indent);

}; // namespace dataflow

#endif
