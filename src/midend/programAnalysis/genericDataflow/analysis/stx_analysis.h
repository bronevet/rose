#ifndef STX_ANALYSIS_H
#define STX_ANALYSIS_H

#include "partitions.h"
#include "abstract_object.h"
#include "dataflow.h"
#include "compose.h"
#include "CallGraphTraverse.h"
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>

namespace dataflow {
/*****************************
 ***** SyntacticAnalysis *****
 *****************************/

// This is a non-dataflow analysis that considers just the syntactic information 
// available about the memory locations, values and control locations at at CFG node. 
// It can precisely interpret static variables and lexical values but provides no 
// information that cannot be directly derived from the text of the code such as 
// the referents of pointers.

/********************
 ***** ANALYSIS *****
 ********************/

class SyntacticAnalysis : virtual public IntraUndirDataflow
{
  public:
  SyntacticAnalysis() {}
  
  void runAnalysis(const Function&  func, NodeState* state, bool, std::set<Function>) { }
  
  // The genInitLattice, genInitFact and transfer functions are not implemented since this 
  // is not a dataflow analysis.
  
  // Maps the given SgNode to an implementation of the ValueObject abstraction.
  ValueObjectPtr   Expr2Val(SgNode* e, PartEdgePtr pedge);
  static ValueObjectPtr   Expr2ValStatic(SgNode* e, PartEdgePtr pedge);
  
  // Maps the given SgNode to an implementation of the MemLocObject abstraction.
  MemLocObjectPtr Expr2MemLoc(SgNode* e, PartEdgePtr pedge);
  static MemLocObjectPtr Expr2MemLocStatic(SgNode* e, PartEdgePtr pedge);
  
  // Maps the given SgNode to an implementation of the Expr2CodeLoc abstraction.
  CodeLocObjectPtr Expr2CodeLoc(SgNode* e, PartEdgePtr pedge);
  static CodeLocObjectPtr Expr2CodeLocStatic(SgNode* e, PartEdgePtr pedge);
  
  // Return the anchor Parts of a given function
  PartPtr GetFunctionStartPart(const Function& func);
  PartPtr GetFunctionEndPart(const Function& func);
  
  // pretty print for the object
  std::string str(std::string indent="")
  { return "SyntacticAnalysis"; }
};

/**********************
 ***** PARTITIONS *****
 **********************/

class StxPart;
class StxPartEdge;

// A NULL CFGNode that is used as a wild-card for termination points of edges to/from anywhere
extern CFGNode NULLCFGNode;

//typedef boost::shared_ptr<StxPart> StxPartPtr;
//typedef boost::shared_ptr<StxPartEdge> StxPartEdgePtr;
typedef CompSharedPtr<StxPart> StxPartPtr;
typedef CompSharedPtr<StxPartEdge> StxPartEdgePtr;

class StxPart : public Part
{
  CFGNode n;
  bool (*filter) (CFGNode cfgn); // a filter function to decide which raw CFG node to show (if return true) or hide (otherwise)
  
  friend class StxPartEdge;
  
  public:
  StxPart(CFGNode n, ComposedAnalysis* analysis, bool (*f) (CFGNode) = defaultFilter): Part(analysis), n(n), filter(f) {}
  StxPart(const StxPart& part):    Part((const Part&)part), n(part.n), filter(part.filter) {} 
  StxPart(const StxPartPtr& part): Part((const Part&)part), n(part->n), filter(part->filter) {} 
  StxPart(const StxPart& part,    bool (*f) (CFGNode) = defaultFilter): Part((const Part&)part), n(part.n), filter (f) {}
  StxPart(const StxPartPtr& part, bool (*f) (CFGNode) = defaultFilter): Part((const Part&)part), n(part->n), filter (f) {}
        
  std::vector<PartEdgePtr> outEdges();
  std::vector<StxPartEdgePtr> outStxEdges();
  std::vector<PartEdgePtr> inEdges();
  std::vector<StxPartEdgePtr> inStxEdges();
  std::set<CFGNode>  CFGNodes();
  
  /*
  // Let A={ set of execution prefixes that terminate at the given anchor SgNode }
  // Let O={ set of execution prefixes that terminate at anchor's operand SgNode }
  // Since to reach a given SgNode an execution must first execute all of its operands it must
  //    be true that there is a 1-1 mapping m() : O->A such that o in O is a prefix of m(o).
  // This function is the inverse of m: given the anchor node and operand as well as the
  //    Part that denotes a subset of A (the function is called on this part), 
  //    it returns a list of Parts that partition O.
  std::list<PartPtr> getOperandPart(SgNode* anchor, SgNode* operand);*/
  
  // Returns a PartEdgePtr, where the source is a wild-card part (NULLPart) and the target is this Part
  PartEdgePtr inEdgeFromAny();
  // Returns a PartEdgePtr, where the target is a wild-card part (NULLPart) and the source is this Part
  PartEdgePtr outEdgeToAny();
  
  bool equal(const PartPtr& o) const;
  bool less(const PartPtr& o) const;
  
  std::string str(std::string indent="");
};

class StxPartEdge : public PartEdge
{
  CFGPath p;
  bool (*filter) (CFGNode cfgn);

  public:
  StxPartEdge(CFGNode src, CFGNode tgt, ComposedAnalysis* analysis, bool (*f) (CFGNode) = defaultFilter): 
      PartEdge(analysis), p(CFGEdge(src, tgt)), filter(f) {}
  StxPartEdge(CFGPath p, ComposedAnalysis* analysis, bool (*f) (CFGNode) = defaultFilter): 
      PartEdge(analysis), p(p), filter(f) {}
  StxPartEdge(const StxPartEdge& dfe): PartEdge((const PartEdge&)dfe), p(dfe.p), filter(dfe.filter) {}
  
  PartPtr source();
  PartPtr target();
  
  CFGPath getPath() const { return p; }
  
  // Let A={ set of execution prefixes that terminate at the given anchor SgNode }
  // Let O={ set of execution prefixes that terminate at anchor's operand SgNode }
  // Since to reach a given SgNode an execution must first execute all of its operands it must
  //    be true that there is a 1-1 mapping m() : O->A such that o in O is a prefix of m(o).
  // This function is the inverse of m: given the anchor node and operand as well as the
  //    PartEdge that denotes a subset of A (the function is called on this PartEdge), 
  //    it returns a list of PartEdges that partition O.
  std::list<PartEdgePtr> getOperandPartEdge(SgNode* anchor, SgNode* operand);
  
  // If the source Part corresponds to a conditional of some sort (if, switch, while test, etc.)
  // it must evaluate some predicate and depending on its value continue, execution along one of the
  // outgoing edges. The value associated with each outgoing edge is fixed and known statically.
  // getPredicateValue() returns the value associated with this particular edge. Since a single 
  // Part may correspond to multiple CFGNodes getPredicateValue() returns a map from each CFG node
  // within its source part that corresponds to a conditional to the value of its predicate along 
  // this edge.
  std::map<CFGNode, boost::shared_ptr<SgValueExp> > getPredicateValue();
  
  bool equal(const PartEdgePtr& o) const;
  bool less(const PartEdgePtr& o) const;
  
  std::string str(std::string indent="");
};

/****************************
 ***** ABSTRACT OBJECTS *****
 ****************************/

class StxValueObject : public ValueObject
{
  public:
  SgValueExp* val;
  
  StxValueObject(SgNode* n);
  StxValueObject(const StxValueObject& that);
  
  bool mayEqual(ValueObjectPtr o, PartEdgePtr pedge);
  bool mustEqual(ValueObjectPtr o, PartEdgePtr pedge);

  // Returns true if the given pair of SgValueExps represent the same value and false otherwise
  static bool equalValExp(SgValueExp* a, SgValueExp* b);

  // Returns true if this ValueObject corresponds to a concrete value that is statically-known
  bool isConcrete();
  // Returns the type of the concrete value (if there is one)
  SgType* getConcreteType();
  // Returns the concrete value (if there is one) as an SgValueExp, which allows callers to use
  // the normal ROSE mechanisms to decode it
  boost::shared_ptr<SgValueExp> getConcreteValue();
  
  std::string str(std::string indent) const; // pretty print for the object
  std::string str(std::string indent) { return ((const StxValueObject*)this)->str(indent); }
    
  // Allocates a copy of this object and returns a pointer to it
  ValueObjectPtr copyV() const;
};
typedef boost::shared_ptr<StxValueObject> StxValueObjectPtr;

class StxCodeLocObject : public CodeLocObject
{
  public:
  PartEdgePtr pedge;
  SgExpression* code;
  
  StxCodeLocObject(SgNode* n, PartEdgePtr pedge);
  StxCodeLocObject(const StxCodeLocObject& that);
  
  bool mayEqualCL(CodeLocObjectPtr o, PartEdgePtr pedge);
  bool mustEqualCL(CodeLocObjectPtr o, PartEdgePtr pedge);
  
  // Returns true if this object is live at the given part and false otherwise
  // We don't currently support scope analysis for CodeLocObjects, so we default to them all being live.
  bool isLive(PartEdgePtr pedge) const
  { return true; }

  /* Don't have good idea how to represent a finite number of options 
  bool isFiniteSet();
  set<AbstractObj> getValueSet();*/
   
  std::string str(std::string indent) const; // pretty print for the object
  std::string str(std::string indent) { return ((const StxCodeLocObject*)this)->str(indent); }
  
  // Allocates a copy of this object and returns a pointer to it
  CodeLocObjectPtr copyCL() const;
};
typedef boost::shared_ptr<StxCodeLocObject> StxCodeLocObjectPtr;

/******************************************
 ***** ABSTRACT OBJECT IMPLEMENTATION *****
 ******************************************/

  // One simple implementation for abstract memory object
  // This implementation is tied to the ROSE AST (another set of base classes such as expression,
  // named, and aliased objects)
  //
  // Big picture of this implementation
  // How the multiple inheritance results in the classes here
  //
  //                      Scalar            LabeledAggregate        Array       Pointer  Function
  // -----------------------------------------------------------------------------
  // Expression Objects | ScalarExp          LabAggreExp 
  // Named objects      | ScalarNamedObj    
  // Aliased Objects    | ScalarAliasedObj  LabeledAggregateAliasedObj ArrayAliasedObj  PointerAliasedObj

  class StxMemLocObject;
  typedef boost::shared_ptr<StxMemLocObject> StxMemLocObjectPtr;
  class StxMemLocObject: public virtual MemLocObject
  {
    protected:
    SgType* type;
    
    public:
    StxMemLocObject(SgNode* n, SgType* t);
    
    // equal() should be called by mayEqualML and mustEqualML of any derived classes
    // to ensure that the in-scope or out-of-scope issues are taken into account. 
    // If equal() returns defEqual or defNotEqual then mayEqualML and mustEqualML should 
    // return true and false, respectively. If equal() returns unknown, mayEqualML and 
    // mustEqualML should continue more refined processing.
    typedef enum {defEqual, defNotEqual, unknown} eqType;
    eqType equal(MemLocObjectPtr that_arg, PartEdgePtr pedge);
    
    SgType* getType() const {return type;}
    /*std::set<SgType*> getType() const
    {
      std::set<SgType*> rt;
      rt.insert(type);
      return rt;
    }*/
    //PartPtr getPart()    const {return part;}
  };
  
  // Base class for StxMemLocObjects that are out of scope.
  class OutOfScope_StxMemLocObject : public StxMemLocObject
  {
    public:
    OutOfScope_StxMemLocObject(SgNode* n, SgType* t) : MemLocObject(n), StxMemLocObject(n, t) {}
    
    // Returns true if this MemLocObject is in-scope at the given part and false otherwise
    bool isLive(PartEdgePtr pedge) const { return false; }
  };

  // A simple implementation of the abstract memory object interface
  // Four categories: scalar, labeled aggregate, array, and pointer
  class Scalar_Impl : public Scalar/*, public StxMemLocObject*/
  {
    public:
      Scalar_Impl(SgNode* n) : MemLocObject(n), Scalar(n) {}
      Scalar_Impl(const Scalar_Impl& that) : MemLocObject((const MemLocObject&)that), Scalar(that) {}
      // We implement operator < () at this level
      bool operator < (const MemLocObject& other) const ;
  };

  class Function_Impl : public FunctionMemLoc/*, public StxMemLocObject*/
  {
    public:
      Function_Impl(SgNode* n) : MemLocObject(n), FunctionMemLoc(n) {}
      Function_Impl(const Scalar_Impl& that) : MemLocObject((const MemLocObject&)that), FunctionMemLoc((const FunctionMemLoc&)that) {}
      // We implement operator < () at this level
      bool operator < (const MemLocObject& other) const ;
  };


  // #SA 10/15/12
  // Deriving from enable_shared_from_this provides shared_from_this() to create a shared_ptr using 'this'
  //  
  class LabeledAggregate_Impl : public LabeledAggregate
  {
    public:
      LabeledAggregate_Impl(SgNode* n) : MemLocObject(n), LabeledAggregate(n) {}
      LabeledAggregate_Impl(const Scalar_Impl& that) : MemLocObject((const MemLocObject&)that), LabeledAggregate((const LabeledAggregate&)that) {}
      bool operator < (const MemLocObject& other) const ;

      size_t fieldCount(PartEdgePtr pedge) const {return elements.size(); };
      // Returns a list of fields
      std::list<LabeledAggregateFieldPtr >  getElements(PartEdgePtr pedge) const {return elements;};
      std::list<LabeledAggregateFieldPtr >& getElements(PartEdgePtr pedge) {return elements;};
    protected:
      std::list<LabeledAggregateFieldPtr > elements; 
  };

  class Array_Impl : public Array/*, public StxMemLocObject*/
  {
    public:   
      Array_Impl(SgNode* n) : MemLocObject(n), Array(n) {}
      Array_Impl(const Scalar_Impl& that) : MemLocObject((const MemLocObject &)that), Array((const Array&)that) {}
      bool operator < (const MemLocObject& other) const ;
  };

  class Pointer_Impl: public Pointer/*, public StxMemLocObject*/
  {
    public: 
      Pointer_Impl(SgNode* n) : MemLocObject(n), Pointer(n) {}
      Pointer_Impl(const Scalar_Impl& that) : MemLocObject((const MemLocObject&)that), Pointer((const Pointer&)that) {}
      bool operator < (const MemLocObject& other) const ;
  };
  
  /*GB: Deprecating IndexSets and replacing them with ValueObjects.
  // we reuse the ConstIndexSet if the value is the same
  class ConstIndexSet: public IndexSet
  {
    public:
      static ConstIndexSet* get_inst(size_t value);
      // only accept strict integer value expression, see bool SageInterface::isStrictIntegerType(SgType* t)
      static ConstIndexSet* get_inst(SgValueExp * v_exp);

      size_t getValue() {return value; };
      // we should not tweak the value once an instance is created. So there is no setValue()
      // integer index equal to another integer index if the integer values are the same
      bool operator== (const IndexSet & other) const; 
      bool mayEqual(const IndexSet & other, const Part& p) const; 
      bool mustEqual(const IndexSet & other, const Part& p) const; 
      bool operator != (const IndexSet & other) const; 
      
      std::string str(const std::string& indent) { return "[ConstIndexSet: "+StringUtility::numberToString(value) + "]"; }
    private:
      size_t value;
      ConstIndexSet(size_t i):IndexSet(Integer_type), value(i) {}
      static std::map <size_t, ConstIndexSet * >  constIndexMap;
  };

  // We only create at most one instance of this unknown indexset. It is a singleton.
  class UnknownIndexSet: public IndexSet
  {
    public:
      static UnknownIndexSet* get_inst();
      // unknown index may equal to other
      bool operator== (const IndexSet & other) const {return true; };  // may be equal to any others
      bool mayEqual(const IndexSet & other, const Part& p) const {return true; };  
      bool mustEqual(const IndexSet & other, const Part& p) const {return false; }; 

      bool operator != (const IndexSet & other) const {return true; };  
      
      std::string str(const std::string& indent) { return "[UnknownIndexSet]"; }
    private:
      UnknownIndexSet (): IndexSet(Unknown_type) { }
      static UnknownIndexSet* inst; 
  };*/
  
  // The most intuitive implementation of array index vector
  class IndexVector_Impl : public IndexVector
  {
    public:
      size_t getSize(PartEdgePtr pedge) const {  return index_vector.size(); };
      /*GB: Deprecating IndexSets and replacing them with ValueObjects.
      std::vector<IndexSet *> index_vector; // a vector of memory objects of named objects or temp expression objects */
      std::vector<ValueObjectPtr> index_vector; // a vector of memory objects of named objects or temp expression objects
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const IndexVector_Impl*)this)->str(indent); }
      
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator== (const IndexVector & other) const; */
      bool mayEqual(IndexVectorPtr other, PartEdgePtr pedge);
      bool mustEqual(IndexVectorPtr other, PartEdgePtr pedge);
  };
  typedef boost::shared_ptr<IndexVector_Impl> IndexVector_ImplPtr;

  class NamedObj; 
  class LabeledAggregateField_Impl : public LabeledAggregateField
  {
    public:
      LabeledAggregateField_Impl(MemLocObjectPtr f, LabeledAggregatePtr p): field (f), parent(p) {}
      std::string getName(PartEdgePtr pedge); // field name
      size_t getIndex(PartEdgePtr pedge); // The field's index within its parent object. The first field has index 0.

      MemLocObjectPtr getField(PartEdgePtr pedge) { return field;}; // Pointer to an abstract description of the field
      void setField(MemLocObjectPtr f) {field = f;}; // Pointer to an abstract description of the field

      LabeledAggregatePtr getParent(PartEdgePtr pedge) {return parent;};
      void setParent(LabeledAggregatePtr p) {parent = p; };

      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const LabeledAggregateField_Impl*)this)->str(indent); }
    private:
      MemLocObjectPtr field; // this should be a named obj 
      LabeledAggregatePtr parent; 
  };

  // The connection to the ROSE AST, all concrete type, size , etc. information come from this side
  // -----------------------------------------------------------------------------------------------
  //
  // Three kinds of memory objects in ROSE AST: each of them can be one of the four categories above.
  // 1) SgExpression temporary variables: each SgExpression which is not named memory objects. 
  //                         They can be seen as compiler-generated discrete temp variables
  // 2) named memory objects : one object for each named variable 
  // 3) aliased memory objects: one object for each type, used for a vague memory object
  //                             representing a set of aliased objects. All aliased objects of the
  //                             same type are represented by a single aliased memory object

  class ExprObj;
  typedef boost::shared_ptr<ExprObj> ExprObjPtr;
  class ExprObj: public StxMemLocObject // one object for each SgExpression which does not have a corresponding symbol
    // They are similar to compiler-generated temporaries for three operand AST format
  { 
    public:
      SgExpression* anchor_exp; 
      
      // MemLocObject* parent; // this field is not necessary, if this ExprObj is a filed of some aggregate types, this ExprObj should be NamedObj
      ExprObj(SgExpression* a, SgType* t): MemLocObject(a), StxMemLocObject(a, t), anchor_exp(a) {};
      ExprObj(const ExprObj& that) : MemLocObject((const MemLocObject &)that), StxMemLocObject((const StxMemLocObject&)that), anchor_exp(that.anchor_exp) {};
      
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      // equal if and only the o2 is another ExprObj with the same SgExpression anchor
      bool operator== (const ExprObj& o2) const;
      
      // this is necessary to allow derived classes to implement MemLocObject::operator==(MemLocObject&)
      bool operator== (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool isConstant() {return isSgValueExp(anchor_exp); } ; // if the expression object represent a constant value (SgValueExp)
      
      // Returns true if this MemLocObject is in-scope at the given part and false otherwise
      bool isLive(PartEdgePtr pedge) const;
      
      virtual std::string str(std::string indent) const; // pretty print for the object
      virtual std::string str(std::string indent) { return ((const ExprObj*)this)->str(indent); }
      
      virtual std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      virtual std::string strp(PartEdgePtr pedge, std::string indent) { return ((const ExprObj*)this)->strp(pedge, indent); }
  };

  // Correspond to variables that are explicit named in the source code
  // Including: local, global, and static variables, as well as their fields
  // Named objects may not directly alias each other since they must be stored disjointly
  // In most cases, two named objects are equal to each other if they correspond to the same entry in the application's symbol table. 
  //
  // However, for a symbol representing a field of a structure, it can be shared by many instances
  // of the structure. In this case, a parent MemLocObject is needed to distinguish between them 
  class NamedObj;
  typedef boost::shared_ptr<NamedObj> NamedObjPtr;
  class NamedObj: public StxMemLocObject  // one object for each named variable with a symbol
  { 
    public:
      SgSymbol* anchor_symbol; // could be a symbol for variable, function, class/structure, etc.
      // NOTE: in most cases, the type field should be anchor_symbol->get_type(). But array element's type can be different from its array type
      MemLocObjectPtr parent; //exists for 1) compound variables like a.b, where a is b's parent, 2) also for array element, where array is the parent
      IndexVectorPtr  array_index_vector; // exists for array element: the index vector of an array element. Ideally this data member could be reused for index of field of structure/class

      //Is this always true that the parent of a named object must be an expr object?
      NamedObj (SgNode* n, SgSymbol* a, SgType* t, MemLocObjectPtr p, IndexVectorPtr iv): 
            MemLocObject(n), StxMemLocObject(n, t), anchor_symbol(a), parent(p), array_index_vector (iv){};
      NamedObj (const NamedObj& that) : MemLocObject((const MemLocObject&)that), StxMemLocObject((const StxMemLocObject&)that), anchor_symbol(that.anchor_symbol), 
                                        parent(that.parent), array_index_vector(that.array_index_vector) {};
      //SgType* getType() const {return type;}
      MemLocObjectPtr getParent() {return parent; } 
      SgSymbol* getSymbol() {return anchor_symbol;}

      std::string getName() {return anchor_symbol->get_name().getString(); }

      virtual std::string str(std::string indent) const; // pretty print for the object
      virtual std::string str(std::string indent) { return ((const NamedObj*)this)->str(indent); }
      
      virtual std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      virtual std::string strp(PartEdgePtr pedge, std::string indent) { return ((const NamedObj*)this)->strp(pedge, indent); }

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator== (const NamedObj& o2) const;*/
      bool mayEqualML(NamedObjPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(NamedObjPtr o2, PartEdgePtr pedge); 

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator== (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      // Returns true if this MemLocObject is in-scope at the given part and false otherwise
      bool isLive(PartEdgePtr pedge) const;
  };

  //  Memory regions that may be accessible via a pointer, such as heap memory
  //  This implementation does not track accurate aliases, an aliased memory object and 
  //  an aliased or named object may be equal if they have the same typ
  class AliasedObj;
  typedef boost::shared_ptr<AliasedObj> AliasedObjPtr;
  class AliasedObj: public StxMemLocObject
  {  // One object for each type
    public: 
      //#SA
      // StxMemLocObject 'type' is sufficient
      //SgType* type; 
      AliasedObj(SgNode* n, SgType* t): MemLocObject(n), StxMemLocObject(n, t) {};
      AliasedObj(const AliasedObj& that): MemLocObject((const MemLocObject&)that), StxMemLocObject((const StxMemLocObject&)that) {};
      
      virtual std::string str(std::string indent) const; // pretty print for the object
      virtual std::string str(std::string indent) { return ((const AliasedObj*)this)->str(indent); }

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const AliasedObj & o2) const;*/
      bool mayEqualML(AliasedObjPtr o2, PartEdgePtr pedge);
      bool mustEqualML(AliasedObjPtr o2, PartEdgePtr pedge);

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge);
      
      // Returns true if this MemLocObject is in-scope at the given part and false otherwise
      bool isLive(PartEdgePtr pedge) const;
  };


  //Derived classes for each kind of each category
  // -------------------------------
  // ----- Out-of-scope object -----
  // -------------------------------
  
  class ScalarOutOfScopeObj : public Scalar_Impl, public OutOfScope_StxMemLocObject
  {
    public:
    ScalarOutOfScopeObj(SgNode* n, SgType* t);
    MemLocObjectPtr copyML() const;
    
    bool mayEqualML(MemLocObjectPtr that,  PartEdgePtr pedge) { return StxMemLocObject::equal(that,  pedge) == defEqual ? true: false; }
    bool mustEqualML(MemLocObjectPtr that, PartEdgePtr pedge) { return StxMemLocObject::equal(that,  pedge) == defEqual ? true: false; }
    std::string str(std::string indent) const { return "ScalarOutOfScopeObj"; }
    std::string str(std::string indent)       { return "ScalarOutOfScopeObj"; }
  };
  
  class FunctionOutOfScopeObj : public Function_Impl, public OutOfScope_StxMemLocObject
  {
    public:
    FunctionOutOfScopeObj(SgNode* n, SgType* t);
    MemLocObjectPtr copyML() const;
    
    bool mayEqualML(MemLocObjectPtr that,  PartEdgePtr pedge) { return StxMemLocObject::equal(that,  pedge) == defEqual ? true: false; }
    bool mustEqualML(MemLocObjectPtr that, PartEdgePtr pedge) { return StxMemLocObject::equal(that,  pedge) == defEqual ? true: false; }
    std::string str(std::string indent) const { return "FunctionOutOfScopeObj"; }
    std::string str(std::string indent)       { return "FunctionOutOfScopeObj"; }
  };
  
  class LabeledAggregateOutOfScopeObj : public LabeledAggregate_Impl, public OutOfScope_StxMemLocObject
  {
    public:
    LabeledAggregateOutOfScopeObj(SgNode* n, SgType* t);
    MemLocObjectPtr copyML() const;
    
    bool mayEqualML(MemLocObjectPtr that,  PartEdgePtr pedge) { return StxMemLocObject::equal(that,  pedge) == defEqual ? true: false; }
    bool mustEqualML(MemLocObjectPtr that, PartEdgePtr pedge) { return StxMemLocObject::equal(that,  pedge) == defEqual ? true: false; }
    std::string str(std::string indent) const { return "LabeledAggregateOutOfScopeObj"; }
    std::string str(std::string indent)       { return "LabeledAggregateOutOfScopeObj"; }
    
    size_t fieldCount(PartEdgePtr pedge) const;
    // Returns a list of fields
    std::list<LabeledAggregateFieldPtr> getElements(PartEdgePtr pedge) const;
  };
  
  class ArrayOutOfScopeObj : public Array_Impl, public OutOfScope_StxMemLocObject
  {
    public:
    ArrayOutOfScopeObj(SgNode* n, SgType* t);
    MemLocObjectPtr copyML() const;
    
    bool mayEqualML(MemLocObjectPtr that,  PartEdgePtr pedge) { return StxMemLocObject::equal(that,  pedge) == defEqual ? true: false; }
    bool mustEqualML(MemLocObjectPtr that, PartEdgePtr pedge) { return StxMemLocObject::equal(that,  pedge) == defEqual ? true: false; }
    std::string str(std::string indent) const { return "ArrayOutOfScopeObj"; }
    std::string str(std::string indent)       { return "ArrayOutOfScopeObj"; }
    
   MemLocObjectPtr getElements(PartEdgePtr pedge);
   MemLocObjectPtr getElements(IndexVectorPtr ai, PartEdgePtr pedge);
   size_t getNumDims(PartEdgePtr pedge);
   boost::shared_ptr<MemLocObject> getDereference(PartEdgePtr pedge);
  };
  
  class PointerOutOfScopeObj : public Pointer_Impl, public OutOfScope_StxMemLocObject
  {
    public:
    PointerOutOfScopeObj(SgNode* n, SgType* t);
    MemLocObjectPtr copyML() const;
    
    bool mayEqualML(MemLocObjectPtr that,  PartEdgePtr pedge) { return StxMemLocObject::equal(that,  pedge) == defEqual ? true: false; }
    bool mustEqualML(MemLocObjectPtr that, PartEdgePtr pedge) { return StxMemLocObject::equal(that,  pedge) == defEqual ? true: false; }
    std::string str(std::string indent) const { return "PointerOutOfScopeObj"; }
    std::string str(std::string indent)       { return "PointerOutOfScopeObj"; }
    
    // used for a pointer to non-array
    MemLocObjectPtr getDereference(PartEdgePtr pedge);
    // used for a pointer to an array
    MemLocObjectPtr getElements(PartEdgePtr pedge);
  };
  
  // -----------------------------
  // ----- Expression object -----
  // -----------------------------
  class ScalarExprObj: public Scalar_Impl, public ExprObj
  {
    public:
      ScalarExprObj(SgExpression* e, SgType* t, PartEdgePtr pedge): MemLocObject(e), Scalar_Impl(e), ExprObj(e, t) {}
      ScalarExprObj(const ScalarExprObj& that): MemLocObject((const MemLocObject &)that), Scalar_Impl((const Scalar_Impl&) that), ExprObj(that.anchor_exp, that.type) {}
      //std::set<SgType*> getType() const;
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      // Implement MemLocObject::operator== () at this level, through ExprObj::operator==().
      bool operator == (const MemLocObject& o2) const;*/
      // multiple inheritance, must be clear about the behavior inherited from both parents
      
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ScalarExprObj*)this)->str(indent); }
      std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      std::string strp(PartEdgePtr pedge, std::string indent) { return ((const ScalarExprObj*)this)->strp(pedge, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  // Does it make sense to have expressions which are of a function type?
  //  I guess so, like function passed as parameter, or a  pointer to a function?
  class FunctionExprObj: public Function_Impl, public ExprObj
  {
    public:
      FunctionExprObj(SgExpression* e, SgType* t, PartEdgePtr pedge): MemLocObject(e), Function_Impl(e), ExprObj(e, t) {}
      FunctionExprObj(const FunctionExprObj& that): MemLocObject((const MemLocObject &)that), Function_Impl((const Function_Impl&) that), ExprObj(that.anchor_exp, that.type) {}
      //std::set<SgType*> getType() const;
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      // Implement MemLocObject::operator== () at this level, through ExprObj::operator==().
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 

      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const FunctionExprObj*)this)->str(indent); }
      std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      std::string strp(PartEdgePtr pedge, std::string indent) { return ((const FunctionExprObj*)this)->strp(pedge, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class LabeledAggregateExprObj: public LabeledAggregate_Impl, public ExprObj
  {
    public:
      LabeledAggregateExprObj(SgExpression* s, SgType* t, PartEdgePtr pedge);
      LabeledAggregateExprObj(const LabeledAggregateExprObj& that);
      void init(SgExpression* e, SgType* t, PartEdgePtr pedge);
      //std::set<SgType*> getType();

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      // Returns true if this object and that object may/must refer to the same labeledAggregate memory object.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const LabeledAggregateExprObj*)this)->str(indent); }
      std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      std::string strp(PartEdgePtr pedge, std::string indent) { return ((const LabeledAggregateExprObj*)this)->strp(pedge, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class ArrayExprObj: public Array_Impl, public ExprObj
  {
    public:
      ArrayExprObj(SgExpression* e, SgType* t, PartEdgePtr pedge): MemLocObject(e), Array_Impl(e), ExprObj (e,t) {}
      ArrayExprObj(const ArrayExprObj& that): MemLocObject((const MemLocObject &)that), Array_Impl((const Array_Impl&) that), ExprObj(that.anchor_exp, that.type) {}
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      //TODO other member functions still make sense?
      bool operator == (const MemLocObject& o2) const;*/
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      // GB: 2012-08-27: should be implementing the following functions here:
      //                 Array::getElements(), getElements(IndexVectorPtr ai), getNumDims(), getDereference()
      MemLocObjectPtr getElements(PartEdgePtr pedge);
      MemLocObjectPtr getElements(IndexVectorPtr ai, PartEdgePtr pedge);
      size_t getNumDims(PartEdgePtr pedge);
      boost::shared_ptr<MemLocObject> getDereference(PartEdgePtr pedge);
   
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ArrayExprObj*)this)->str(indent); }
      std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      std::string strp(PartEdgePtr pedge, std::string indent) { return ((const ArrayExprObj*)this)->strp(pedge, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class PointerExprObj: public Pointer_Impl, public ExprObj
  {
    public:
      PointerExprObj(SgExpression* e, SgType* t, PartEdgePtr pedge): MemLocObject(e), Pointer_Impl(e), ExprObj (e, t) {}
      PointerExprObj(const PointerExprObj& that): MemLocObject((const MemLocObject &)that), Pointer_Impl((const Pointer_Impl&) that), ExprObj(that.anchor_exp, that.type) {}
      //std::set<SgType*> getType();
      // used for a pointer to non-array
      MemLocObjectPtr getDereference(PartEdgePtr pedge);
      // used for a pointer to an array
      MemLocObjectPtr getElements(PartEdgePtr pedge);

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      // Returns true if this object and that object may/must refer to the same pointer memory object.
      bool operator == (const MemLocObject& o2) const;*/
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const PointerExprObj*)this)->str(indent); }
      std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      std::string strp(PartEdgePtr pedge, std::string indent) { return ((const PointerExprObj*)this)->strp(pedge, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  // ------------------------
  // ----- Named object -----
  // ------------------------
  class ScalarNamedObj: public Scalar_Impl, public NamedObj 
  {
    public:
      ScalarNamedObj(SgNode* n, SgSymbol* s, SgType* t, MemLocObjectPtr p, IndexVectorPtr iv, PartEdgePtr pedge): 
        MemLocObject(n), Scalar_Impl(n), NamedObj (n, s,t, p, iv) {}
      ScalarNamedObj(const ScalarNamedObj& that): MemLocObject((const MemLocObject &)that), Scalar_Impl(that), NamedObj((const NamedObj&)that) {}
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject & that) const ;*/
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ScalarNamedObj*)this)->str(indent); }
      
      std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      std::string strp(PartEdgePtr pedge, std::string indent) { return ((const NamedObj*)this)->strp(pedge, indent); }

      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class FunctionNamedObj: public Function_Impl, public NamedObj 
  {
    public:

      // simple constructor, a function symbol is enough
      FunctionNamedObj(SgNode* n, SgSymbol* s, PartEdgePtr pedge): 
          MemLocObject(n), Function_Impl(n), NamedObj(n, s, s->get_type(), MemLocObjectPtr(), IndexVectorPtr()) {}
      FunctionNamedObj(const FunctionNamedObj& that): 
          MemLocObject((const MemLocObject &)that), Function_Impl(that), NamedObj((const NamedObj&)that) {}
      // I am not sure when a function can be used as a child and an array element. But this is
      // provided just in case
      FunctionNamedObj (SgNode* n, SgSymbol* s, SgType* t, PartEdgePtr pedge, MemLocObjectPtr p, IndexVectorPtr iv): 
          MemLocObject(n), Function_Impl(n), NamedObj (n, s,t, p, iv) {}
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject & that) const ;*/
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const FunctionNamedObj*)this)->str(indent); }
      
      std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      std::string strp(PartEdgePtr pedge, std::string indent) { return ((const NamedObj*)this)->strp(pedge, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class LabeledAggregateNamedObj: public LabeledAggregate_Impl, public NamedObj
  {
    public:
      LabeledAggregateNamedObj(SgNode* n, SgSymbol* s, SgType* t, MemLocObjectPtr p, IndexVectorPtr iv, PartEdgePtr pedge);
      LabeledAggregateNamedObj(const LabeledAggregateNamedObj& that);
      void init(SgSymbol* s, SgType* t, MemLocObjectPtr p, IndexVectorPtr iv, PartEdgePtr pedge);
      //std::set<SgType*> getType();

      // Returns true if this object and that object may/must refer to the same labeledAggregate memory object.
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject & that) const ;*/
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const LabeledAggregateNamedObj*)this)->str(indent); }
      
      std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      std::string strp(PartEdgePtr pedge, std::string indent) { return ((const NamedObj*)this)->strp(pedge, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class ArrayNamedObj;
  class ArrayNamedObj: public Array_Impl, public NamedObj//, public boost::enable_shared_from_this<ArrayNamedObj>
  {
    public:
      ArrayNamedObj(SgNode* n, SgSymbol* s, SgType* t, MemLocObjectPtr p, IndexVectorPtr iv, PartEdgePtr pedge);
      ArrayNamedObj(const ArrayNamedObj& that);
      void init(SgSymbol* s, SgType* t, MemLocObjectPtr p, IndexVectorPtr iv);
      //std::set <SgType*> getType();
      std::string toString();

      // Returns a memory object that corresponds to all the elements in the given array
      // GB 2012-08-27: This doesn't look right. The contents of an array have one less level of indirection than 
      //                the array itself. You shouldn't be able to call getElements on the contents of an int array.
      MemLocObjectPtr getElements(PartEdgePtr pedge) { return MemLocObjectPtr(this); } ; 
      // Returns the memory object that corresponds to the elements described by the given abstract index, 
      MemLocObjectPtr getElements(IndexVectorPtr ai, PartEdgePtr pedge);

      // number of dimensions of the array
      virtual size_t getNumDims(PartEdgePtr pedge) const;

      // rare case that an array is dereferenced, treated as array[0]
      MemLocObjectPtr getDereference(PartEdgePtr pedge);
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject & that) const;*/
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge);
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ArrayNamedObj*)this)->str(indent); }
      
      std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      std::string strp(PartEdgePtr pedge, std::string indent) { return ((const NamedObj*)this)->strp(pedge, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class PointerNamedObj: public Pointer_Impl, public NamedObj
  {
    public:
      PointerNamedObj(SgNode*n, SgSymbol* s, SgType* t, MemLocObjectPtr p, IndexVectorPtr iv, PartEdgePtr pedge): 
          MemLocObject(n), Pointer_Impl(n), NamedObj (n, s,t, p, iv) {}
      PointerNamedObj(const PointerNamedObj& that): 
          MemLocObject((const MemLocObject &)that), Pointer_Impl((const Pointer_Impl&)that), NamedObj((const NamedObj&)that) {}
      //std::set<SgType*> getType() const;
      // used for a pointer to non-array
      MemLocObjectPtr getDereference(PartEdgePtr pedge);
      // used for a pointer to an array
      MemLocObjectPtr getElements(PartEdgePtr pedge);
      // Returns true if this object and that object may/must refer to the same pointer memory object.
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject & that) const ;*/
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const PointerNamedObj*)this)->str(indent); }
      
      std::string strp(PartEdgePtr pedge, std::string indent) const; // pretty print for the object
      std::string strp(PartEdgePtr pedge, std::string indent) { return ((const NamedObj*)this)->strp(pedge, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  // --------------------------
  // ----- Aliased object -----
  // --------------------------
  class ScalarAliasedObj: public Scalar_Impl, public AliasedObj
  {
    public:
      ScalarAliasedObj(SgNode* n, SgType* t, PartEdgePtr pedge): MemLocObject(n), Scalar_Impl(n), AliasedObj(n, t) {}
      ScalarAliasedObj(const ScalarAliasedObj& that): MemLocObject((const MemLocObject&)that), Scalar_Impl((const Scalar_Impl&)that), AliasedObj((const AliasedObj&)type) {}
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ScalarAliasedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class FunctionAliasedObj: public Function_Impl, public AliasedObj
  {
    public:
      FunctionAliasedObj(SgNode* n, SgType* t, PartEdgePtr pedge): MemLocObject(n), Function_Impl(n), AliasedObj(n, t){}
      FunctionAliasedObj(const FunctionAliasedObj& that):  MemLocObject((const MemLocObject&)that), Function_Impl((const Function_Impl&)that), AliasedObj((const AliasedObj&)type) {}
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const FunctionAliasedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };
  
  class LabeledAggregateAliasedObj : public  LabeledAggregate_Impl, public AliasedObj
  {
    public:
      LabeledAggregateAliasedObj(SgNode* n, SgType* t, PartEdgePtr pedge): MemLocObject(n), LabeledAggregate_Impl(n), AliasedObj((const AliasedObj&)type) {}
      LabeledAggregateAliasedObj(const LabeledAggregateAliasedObj& that): 
           MemLocObject((const MemLocObject&)that), LabeledAggregate_Impl((const LabeledAggregate_Impl&)that), AliasedObj((const AliasedObj&)type) {}
      //std::set<SgType*> getType();
      //TODO
      // size_t fieldCount();
      // std::list<LabeledAggregateField*> getElements() const;
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const LabeledAggregateAliasedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class ArrayAliasedObj: public Array_Impl, public AliasedObj
  {
    public:
      ArrayAliasedObj(SgNode* n, SgType* t, PartEdgePtr pedge): MemLocObject(n), Array_Impl(n), AliasedObj(n, t){}
      ArrayAliasedObj(const ArrayAliasedObj& that): 
           MemLocObject((const MemLocObject&)that), Array_Impl((const Array_Impl&)that), AliasedObj((const AliasedObj&)type) {}
      //std::set<SgType*> getType();

      //TODO
      // MemLocObject* getElements();
      //  MemLocObject* getElements(AbstractIndex* ai);
      //  getNumDims();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      // GB: 2012-08-27: should be implementing the following functions here:
      //                 Array::getElements(), getElements(IndexVectorPtr ai), getNumDims(), getDereference()
      MemLocObjectPtr getElements(PartEdgePtr pedge);
      MemLocObjectPtr getElements(IndexVectorPtr ai, PartEdgePtr pedge);
      size_t getNumDims(PartEdgePtr pedge);
      boost::shared_ptr<MemLocObject> getDereference(PartEdgePtr pedge);
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ArrayAliasedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class PointerAliasedObj: public Pointer_Impl, public AliasedObj
  {
    public:
      PointerAliasedObj(SgNode* n, SgType* t, PartEdgePtr pedge): MemLocObject(n), Pointer_Impl(n), AliasedObj(n, t) {}
      PointerAliasedObj(const PointerAliasedObj& that):  MemLocObject((const MemLocObject&)that), Pointer_Impl((const Pointer_Impl&)that), AliasedObj((const AliasedObj&)type) {}
      MemLocObjectPtr getDereference(PartEdgePtr pedge);
      // MemLocObject * getElements() const;
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      bool mustEqualML(MemLocObjectPtr o2, PartEdgePtr pedge); 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const PointerAliasedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  // lower level, internal  builder for different objects -----------------------
  // Users should use MemLocObject* ObjSetFactory::createObjSet (SgNode* n) instead
  
  // Create an aliased obj set from a type. It can return NULL since not all types are supported.
  MemLocObjectPtr createAliasedMemLocObject(SgNode* n, SgType*t, PartEdgePtr pedge);  // One object per type, Type based alias analysis
  MemLocObjectPtr createNamedMemLocObject(SgNode* n, SgSymbol* anchor_symbol, SgType* t, PartEdgePtr pedge, MemLocObjectPtr parent, IndexVectorPtr iv); // any 
  MemLocObjectPtr createNamedMemLocObject(SgNode* n, SgVarRefExp* r, PartEdgePtr pedge); // create NamedMemLocObject or AliasedMemLocObject (for pointer type) from a variable reference 
  MemLocObjectPtr createNamedMemLocObject(SgNode* n, SgPntrArrRefExp* r, PartEdgePtr pedge); // create NamedMemLocObject from an array element access
  MemLocObjectPtr createExpressionMemLocObject(SgExpression* anchor_exp, SgType*t, PartEdgePtr pedge); 
  // Return true if op is an operand of the given SgNode n and false otherwise.
  bool isOperand(SgNode* n, SgExpression* op);
  // MemLocObject* createMemLocObject(SgNode*); // top level catch all case, declared in memory_object.h

  // Helper functions 
  // --------------------------------------
  // debugging
  void dump_aliased_objset_map (); 

  // A helper function to decide if two types are aliased
  // two cases: 1 they are the same type
  //            2 they have overlap (one type is a subtype of the other)
  bool isAliased (const SgType * t1, const SgType * t2 ); 

  // a helper function to check if a symbol is corresponding to a member variable declaration within SgClassDefinition or not
  bool isMemberVariableDeclarationSymbol(SgSymbol * s);

  // a helper function to fill up elements of MemLocObject p from a class/structure type
  // #SA 10/15/12 - modified first parameter to LabeledAggregatePtr
  // Currently only labeled aggregate objects use this function
  void fillUpElements (MemLocObjectPtr p, std::list<boost::shared_ptr<LabeledAggregateField> > & elements, SgClassType* c_t, PartEdgePtr part);

  // convert std::vector<SgExpression*>* subscripts to IndexVectorPtr  array_index_vector
  IndexVectorPtr generateIndexVector (std::vector<SgExpression*>& subscripts);
}; //namespace dataflow

#endif
