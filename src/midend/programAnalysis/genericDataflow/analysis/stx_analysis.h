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

/*class StxValueObject;
class StxMemLocObject;
class StxCodeLocObject;*/

/********************
 ***** ANALYSIS *****
 ********************/

class SyntacticAnalysis : virtual public IntraUndirDataflow
{
  public:
  SyntacticAnalysis() {}
  
  void runAnalysis(const Function&  func, NodeState* state, bool, std::set<Function>) { }
  
  // The genInitState and transfer functions are dummy since this is not a dataflow 
  // analysis.
  //void genInitState(const Part& p, Lattice** initLattice, 
  //                  std::vector<NodeFact*>& initFacts) {}
  void genInitState(const Function& func, PartPtr p, const NodeState& state, std::vector<Lattice*>& initLattices, std::vector<NodeFact*>& initFacts)
  {}
   
   //void transfer(SgNode &n, Part& p) {}
  bool transfer(const Function& func, PartPtr p, NodeState& state, const std::vector<Lattice*>& dfInfo) {
    return true;
  }
  
  // Maps the given SgNode to an implementation of the ValueObject abstraction.
  ValueObjectPtr   Expr2Val(SgNode* e, PartPtr p);
  static ValueObjectPtr   Expr2ValStatic(SgNode* e, PartPtr p);
  
  // Maps the given SgNode to an implementation of the MemLocObject abstraction.
  MemLocObjectPtr Expr2MemLoc(SgNode* e, PartPtr p);
  static MemLocObjectPtr Expr2MemLocStatic(SgNode* e, PartPtr p);
  
  // Maps the given SgNode to an implementation of the Expr2CodeLoc abstraction.
  CodeLocObjectPtr Expr2CodeLoc(SgNode* e, PartPtr p);
  static CodeLocObjectPtr Expr2CodeLocStatic(SgNode* e, PartPtr p);
  
  // pretty print for the object
  std::string str(std::string indent="")
  { return "SyntacticAnalysis"; }
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
  
  bool mayEqual(ValueObjectPtr o, PartPtr p) const;
  bool mustEqual(ValueObjectPtr o, PartPtr p) const;

  // Returns true if the given pair of SgValueExps represent the same value and false otherwise
  static bool equalValExp(SgValueExp* a, SgValueExp* b);

  /* Don't have good idea how to represent a finite number of options 
  bool isFiniteSet();
  set<AbstractObj> getValueSet();*/
  
  std::string str(std::string indent) const; // pretty print for the object
  std::string str(std::string indent) { return ((const StxValueObject*)this)->str(indent); }
    
  // Allocates a copy of this object and returns a pointer to it
  ValueObjectPtr copyV() const;
};
typedef boost::shared_ptr<StxValueObject> StxValueObjectPtr;

/*class StxMemLocObject : public MemLocObject
{
};*/

class StxCodeLocObject : public CodeLocObject
{
  public:
  PartPtr part;
  SgExpression* code;
  
  StxCodeLocObject(SgNode* n, PartPtr p);
  StxCodeLocObject(const StxCodeLocObject& that);
  
  bool mayEqualCL(CodeLocObjectPtr o, PartPtr p) const;
  bool mustEqualCL(CodeLocObjectPtr o, PartPtr p) const;

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
  class StxMemLocObject
  {
    protected:
    // Flag indicating whether this MemLocObject is currently in scope. For memory locations that are tied to concrete
    // lexical entities such as expressions or static variables it is easy to tell if the object cannot exist within
    // a given part because it is out of scope. We default to treating all aliased objects as being in scope.
    //bool inScope;
      
    SgType* type;
        
    PartPtr part;
    
    public:
    StxMemLocObject(SgType* t, PartPtr part) : /*inScope(true), */type(t), part(part) {}
    //StxMemLocObject(bool inScope, SgType* t, PartPtr part) : inScope(inScope), type(t), part(part) {}
    
    // equal() should be called by mayEqualML and mustEqualML of any derived classes
    // to ensure that the in-scope or out-of-scope issues are taken into account. 
    // If equal() returns defEqual or defNotEqual then mayEqualML and mustEqualML should 
    // return true and false, respectively. If equal() returns unknown, mayEqualML and 
    // mustEqualML should continue more refined processing.
    typedef enum {defEqual, defNotEqual, unknown} eqType;
    eqType equal(MemLocObjectPtr that_arg, PartPtr part) const
    {
      StxMemLocObjectPtr that = boost::dynamic_pointer_cast <StxMemLocObject> (that_arg);
      return equal(that, part);
    }
    eqType equal(StxMemLocObjectPtr that, PartPtr part) const
    {
      if(isInScope(part)) {
        // One is in-scope but the other is out-of-scope: different classes
        if(!that->isInScope(part)) return defNotEqual;
        // Both are in-scope: need more refined processing
        else               return unknown;
      } else {
        // Both are out-of-scope: same class
        if(!that->isInScope(part)) return defEqual;
        // One is in-scope but the other is out-of-scope: different classes
        else               return defNotEqual;
      }
    }
    
    virtual bool isInScope(PartPtr part) const=0;
    
    SgType* getType() const {return type;}
    /*std::set<SgType*> getType() const
    {
      std::set<SgType*> rt;
      rt.insert(type);
      return rt;
    }*/
    PartPtr getPart()    const {return part;}
  };
  
  // Base class for StxMemLocObjects that are out of scope.
  class OutOfScope_StxMemLocObject : public StxMemLocObject
  {
    public:
    OutOfScope_StxMemLocObject(SgType* t, PartPtr part) : StxMemLocObject(/*false, */t, part) {}
    
    bool isInScope(PartPtr part) const { return false; }
  };

  // A simple implementation of the abstract memory object interface
  // Four categories: scalar, labeled aggregate, array, and pointer
  class Scalar_Impl : public Scalar/*, public StxMemLocObject*/
  {
    public:
      // We implement operator < () at this level
      bool operator < (const MemLocObject& other) const ;
  };

  class Function_Impl : public FunctionMemLoc/*, public StxMemLocObject*/
  {
    public:
      // We implement operator < () at this level
      bool operator < (const MemLocObject& other) const ;
  };


  class LabeledAggregate_Impl : public LabeledAggregate/*, public StxMemLocObject*/
  {
    public:
      bool operator < (const MemLocObject& other) const ;

      size_t fieldCount() const {return elements.size(); };
      // Returns a list of fields
      std::vector<boost::shared_ptr<LabeledAggregateField> > getElements() const {return elements;};
      std::vector<boost::shared_ptr<LabeledAggregateField> >& getElements() {return elements;};
    private:
      std::vector<boost::shared_ptr<LabeledAggregateField> > elements; 
  };

  class Array_Impl : public Array/*, public StxMemLocObject*/
  {
    public:   
      bool operator < (const MemLocObject& other) const ;
  };

  class Pointer_Impl: public Pointer/*, public StxMemLocObject*/
  {
    public: 
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
      size_t getSize() const {  return index_vector.size(); };
      /*GB: Deprecating IndexSets and replacing them with ValueObjects.
      std::vector<IndexSet *> index_vector; // a vector of memory objects of named objects or temp expression objects */
      std::vector<ValueObjectPtr> index_vector; // a vector of memory objects of named objects or temp expression objects
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const IndexVector_Impl*)this)->str(indent); }
      
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator== (const IndexVector & other) const; */
      bool mayEqual(IndexVectorPtr other, const Part& p) const ;
      bool mustEqual(IndexVectorPtr other, const Part& p) const ;
  };
  typedef boost::shared_ptr<IndexVector_Impl> IndexVector_ImplPtr;

  class NamedObj; 
  class LabeledAggregateField_Impl : public LabeledAggregateField
  {
    public:
      LabeledAggregateField_Impl(MemLocObjectPtr f, LabeledAggregatePtr p): field (f), parent(p) {}
      std::string getName(); // field name
      size_t getIndex(); // The field's index within its parent object. The first field has index 0.

      MemLocObjectPtr getField() { return field;}; // Pointer to an abstract description of the field
      void setField(MemLocObjectPtr f) {field = f;}; // Pointer to an abstract description of the field

      LabeledAggregatePtr getParent() {return parent;};
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
      ExprObj (SgExpression* a, SgType* t, PartPtr part): StxMemLocObject(t, part), anchor_exp(a) {};
      
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      // equal if and only the o2 is another ExprObj with the same SgExpression anchor
      bool operator== (const ExprObj& o2) const;
      
      // this is necessary to allow derived classes to implement MemLocObject::operator==(MemLocObject&)
      bool operator== (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool isConstant () {return isSgValueExp(anchor_exp); } ; // if the expression object represent a constant value (SgValueExp)
      
      bool isInScope(PartPtr part) const;
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ExprObj*)this)->str(indent); }
      
      std::string strp(PartPtr part, std::string indent) const; // pretty print for the object
      std::string strp(PartPtr part, std::string indent) { return ((const ExprObj*)this)->str(indent); }
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
      NamedObj (SgSymbol* a, SgType* t, PartPtr part, MemLocObjectPtr p, IndexVectorPtr iv): StxMemLocObject(t, part), anchor_symbol(a), parent(p), array_index_vector (iv){};
      //SgType* getType() const {return type;}
      MemLocObjectPtr getParent() {return parent; } 
      SgSymbol* getSymbol() {return anchor_symbol;}

      std::string getName() {return anchor_symbol->get_name().getString(); }

      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const NamedObj*)this)->str(indent); }

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator== (const NamedObj& o2) const;*/
      bool mayEqualML(NamedObjPtr o2, PartPtr p) const; 
      bool mustEqualML(NamedObjPtr o2, PartPtr p) const; 

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator== (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      bool isInScope(PartPtr part) const;
  };

  //  Memory regions that may be accessible via a pointer, such as heap memory
  //  This implementation does not track accurate aliases, an aliased memory object and 
  //  an aliased or named object may be equal if they have the same typ
  class AliasedObj;
  typedef boost::shared_ptr<AliasedObj> AliasedObjPtr;
  class AliasedObj: public StxMemLocObject
  {  // One object for each type
    public: 
      SgType* type; 
      AliasedObj (SgType* t, PartPtr part): StxMemLocObject(t, part) {};
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const AliasedObj*)this)->str(indent); }

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const AliasedObj & o2) const;*/
      bool mayEqualML(AliasedObjPtr o2, PartPtr p) const;
      bool mustEqualML(AliasedObjPtr o2, PartPtr p) const;

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const;
      
      bool isInScope(PartPtr part) const;
  };


  //Derived classes for each kind of each category
  // -------------------------------
  // ----- Out-of-scope object -----
  // -------------------------------
  
  class ScalarOutOfScopeObj : public Scalar_Impl, public OutOfScope_StxMemLocObject
  {
    public:
    ScalarOutOfScopeObj(SgType* t, PartPtr part);
    MemLocObjectPtr copyML() const;
    
    bool mayEqualML(MemLocObjectPtr that,  PartPtr part) const { return StxMemLocObject::equal(that,  part) == defEqual ? true: false; }
    bool mustEqualML(MemLocObjectPtr that, PartPtr part) const { return StxMemLocObject::equal(that,  part) == defEqual ? true: false; }
    std::string str(std::string indent) const { return "ScalarOutOfScopeObj"; }
    std::string str(std::string indent)       { return "ScalarOutOfScopeObj"; }
  };
  
  class FunctionOutOfScopeObj : public Function_Impl, public OutOfScope_StxMemLocObject
  {
    public:
    FunctionOutOfScopeObj(SgType* t, PartPtr part);
    MemLocObjectPtr copyML() const;
    
    bool mayEqualML(MemLocObjectPtr that,  PartPtr part) const { return StxMemLocObject::equal(that,  part) == defEqual ? true: false; }
    bool mustEqualML(MemLocObjectPtr that, PartPtr part) const { return StxMemLocObject::equal(that,  part) == defEqual ? true: false; }
    std::string str(std::string indent) const { return "FunctionOutOfScopeObj"; }
    std::string str(std::string indent)       { return "FunctionOutOfScopeObj"; }
  };
  
  class LabeledAggregateOutOfScopeObj : public LabeledAggregate_Impl, public OutOfScope_StxMemLocObject
  {
    public:
    LabeledAggregateOutOfScopeObj(SgType* t, PartPtr part);
    MemLocObjectPtr copyML() const;
    
    bool mayEqualML(MemLocObjectPtr that,  PartPtr part) const { return StxMemLocObject::equal(that,  part) == defEqual ? true: false; }
    bool mustEqualML(MemLocObjectPtr that, PartPtr part) const { return StxMemLocObject::equal(that,  part) == defEqual ? true: false; }
    std::string str(std::string indent) const { return "LabeledAggregateOutOfScopeObj"; }
    std::string str(std::string indent)       { return "LabeledAggregateOutOfScopeObj"; }
    
    size_t fieldCount() const;
    // Returns a list of fields
    std::vector<LabeledAggregateFieldPtr> getElements() const;
  };
  
  class ArrayOutOfScopeObj : public Function_Impl, public OutOfScope_StxMemLocObject
  {
    public:
    ArrayOutOfScopeObj(SgType* t, PartPtr part);
    MemLocObjectPtr copyML() const;
    
    bool mayEqualML(MemLocObjectPtr that,  PartPtr part) const { return StxMemLocObject::equal(that,  part) == defEqual ? true: false; }
    bool mustEqualML(MemLocObjectPtr that, PartPtr part) const { return StxMemLocObject::equal(that,  part) == defEqual ? true: false; }
    std::string str(std::string indent) const { return "ArrayOutOfScopeObj"; }
    std::string str(std::string indent)       { return "ArrayOutOfScopeObj"; }
    
   boost::shared_ptr<MemLocObject> getElements();
   boost::shared_ptr<MemLocObject> getElements(IndexVectorPtr ai);
   size_t getNumDims();
   boost::shared_ptr<MemLocObject> getDereference();
  };
  
  class PointerOutOfScopeObj : public Function_Impl, public OutOfScope_StxMemLocObject
  {
    public:
    PointerOutOfScopeObj(SgType* t, PartPtr part);
    MemLocObjectPtr copyML() const;
    
    bool mayEqualML(MemLocObjectPtr that,  PartPtr part) const { return StxMemLocObject::equal(that,  part) == defEqual ? true: false; }
    bool mustEqualML(MemLocObjectPtr that, PartPtr part) const { return StxMemLocObject::equal(that,  part) == defEqual ? true: false; }
    std::string str(std::string indent) const { return "PointerOutOfScopeObj"; }
    std::string str(std::string indent)       { return "PointerOutOfScopeObj"; }
    
    // used for a pointer to non-array
    MemLocObjectPtr getDereference();
    // used for a pointer to an array
    MemLocObjectPtr getElements();
  };
  
  // -----------------------------
  // ----- Expression object -----
  // -----------------------------
  class ScalarExprObj: public Scalar_Impl, public ExprObj
  {
    public:
      ScalarExprObj(SgExpression* e, SgType* t, PartPtr part): ExprObj(e, t, part) {}
      ScalarExprObj(const ScalarExprObj& that): ExprObj(that.anchor_exp, that.type, that.part) {}
      //std::set<SgType*> getType() const;
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      // Implement MemLocObject::operator== () at this level, through ExprObj::operator==().
      bool operator == (const MemLocObject& o2) const;*/
      // multiple inheritance, must be clear about the behavior inherited from both parents
      
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ScalarExprObj*)this)->str(indent); }
      std::string strp(PartPtr part, std::string indent) const; // pretty print for the object
      std::string strp(PartPtr part, std::string indent) { return ((const ScalarExprObj*)this)->strp(part, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  // Does it make sense to have expressions which are of a function type?
  //  I guess so, like function passed as parameter, or a  pointer to a function?
  class FunctionExprObj: public Function_Impl, public ExprObj
  {
    public:
      FunctionExprObj(SgExpression* e, SgType* t, PartPtr part): ExprObj(e, t, part) {}
      FunctionExprObj(const FunctionExprObj& that): ExprObj(that.anchor_exp, that.type, that.part) {}
      //std::set<SgType*> getType() const;
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      // Implement MemLocObject::operator== () at this level, through ExprObj::operator==().
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 

      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const FunctionExprObj*)this)->str(indent); }
      std::string strp(PartPtr part, std::string indent) const; // pretty print for the object
      std::string strp(PartPtr part, std::string indent) { return ((const FunctionExprObj*)this)->strp(part, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class LabeledAggregateExprObj: public LabeledAggregate_Impl, public ExprObj
  {
    public:
      LabeledAggregateExprObj(SgExpression* s, SgType* t, PartPtr part);
      LabeledAggregateExprObj(const LabeledAggregateExprObj& that);
      void init(SgExpression* e, SgType* t);
      //std::set<SgType*> getType();

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      // Returns true if this object and that object may/must refer to the same labeledAggregate memory object.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const LabeledAggregateExprObj*)this)->str(indent); }
      std::string strp(PartPtr part, std::string indent) const; // pretty print for the object
      std::string strp(PartPtr part, std::string indent) { return ((const LabeledAggregateExprObj*)this)->strp(part, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class ArrayExprObj: public Array_Impl, public ExprObj
  {
    public:
      ArrayExprObj(SgExpression* e, SgType* t, PartPtr par): ExprObj (e,t,part) {}
      ArrayExprObj(const ArrayExprObj& that): ExprObj(that.anchor_exp, that.type, that.part) {}
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      //TODO other member functions still make sense?
      bool operator == (const MemLocObject& o2) const;*/
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      // GB: 2012-08-27: should be implementing the following functions here:
      //                 Array::getElements(), getElements(IndexVectorPtr ai), getNumDims(), getDereference()
      boost::shared_ptr<MemLocObject> getElements();
      boost::shared_ptr<MemLocObject> getElements(IndexVectorPtr ai);
      size_t getNumDims();
      boost::shared_ptr<MemLocObject> getDereference();
   
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ArrayExprObj*)this)->str(indent); }
      std::string strp(PartPtr part, std::string indent) const; // pretty print for the object
      std::string strp(PartPtr part, std::string indent) { return ((const ArrayExprObj*)this)->strp(part, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class PointerExprObj: public Pointer_Impl, public ExprObj
  {
    public:
      PointerExprObj(SgExpression* e, SgType* t, PartPtr par): ExprObj (e,t,part) {}
      PointerExprObj(const PointerExprObj& that): ExprObj(that.anchor_exp, that.type, that.part) {}
      //std::set<SgType*> getType();
      // used for a pointer to non-array
      MemLocObjectPtr getDereference();
      // used for a pointer to an array
      MemLocObjectPtr getElements();
      // Returns true if this pointer refers to the same abstract object as that pointer.
      //GB: getDereference subsumes this
      //bool equalPoints(const Pointer & that);

      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      // Returns true if this object and that object may/must refer to the same pointer memory object.
      bool operator == (const MemLocObject& o2) const;*/
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const PointerExprObj*)this)->str(indent); }
      std::string strp(PartPtr part, std::string indent) const; // pretty print for the object
      std::string strp(PartPtr part, std::string indent) { return ((const PointerExprObj*)this)->strp(part, indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  // ------------------------
  // ----- Named object -----
  // ------------------------
  class ScalarNamedObj: public Scalar_Impl, public NamedObj 
  {
    public:
      ScalarNamedObj(SgSymbol* s, SgType* t, PartPtr part, MemLocObjectPtr p, IndexVectorPtr iv): NamedObj (s,t,part, p, iv) {}
      ScalarNamedObj(const ScalarNamedObj& that): NamedObj(that.anchor_symbol, that.type, that.part, that.parent, that.array_index_vector) {}
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject & that) const ;*/
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ScalarNamedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class FunctionNamedObj: public Function_Impl, public NamedObj 
  {
    public:

      // simple constructor, a function symbol is enough
      FunctionNamedObj(SgSymbol* s, PartPtr part): NamedObj (s, s->get_type(), part, MemLocObjectPtr(), IndexVectorPtr()) {}
      FunctionNamedObj(const FunctionNamedObj& that): NamedObj(that.anchor_symbol, that.type, that.part, that.parent, that.array_index_vector) {}
      // I am not sure when a function can be used as a child and an array element. But this is
      // provided just in case
      FunctionNamedObj (SgSymbol* s, SgType* t, PartPtr part, MemLocObjectPtr p, IndexVectorPtr iv): NamedObj (s,t, part, p, iv) {}
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject & that) const ;*/
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const FunctionNamedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class LabeledAggregateNamedObj: public LabeledAggregate_Impl, public NamedObj
  {
    public:
      LabeledAggregateNamedObj(SgSymbol* s, SgType* t, PartPtr part, MemLocObjectPtr p, IndexVectorPtr iv);
      LabeledAggregateNamedObj(const LabeledAggregateNamedObj& that);
      void init(SgSymbol* s, SgType* t, MemLocObjectPtr p, IndexVectorPtr iv);
      //std::set<SgType*> getType();

      // Returns true if this object and that object may/must refer to the same labeledAggregate memory object.
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject & that) const ;*/
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const LabeledAggregateNamedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class ArrayNamedObj;
  class ArrayNamedObj: public Array_Impl, public NamedObj, public boost::enable_shared_from_this<ArrayNamedObj>
  {
    public:
      ArrayNamedObj(SgSymbol* s, SgType* t, PartPtr part, MemLocObjectPtr p, IndexVectorPtr iv);
      ArrayNamedObj(const ArrayNamedObj& that);
      void init(SgSymbol* s, SgType* t, MemLocObjectPtr p, IndexVectorPtr iv);
      //std::set <SgType*> getType();
      std::string toString();

      // Returns a memory object that corresponds to all the elements in the given array
      // GB 2012-08-27: This doesn't look right. The contents of an array have one less level of indirection than 
      //                the array itself. You shouldn't be able to call getElements on the contents of an int array.
      MemLocObjectPtr getElements() { return MemLocObjectPtr(this); } ; 
      // Returns the memory object that corresponds to the elements described by the given abstract index, 
      MemLocObjectPtr getElements(IndexVectorPtr ai);

      // number of dimensions of the array
      virtual size_t getNumDims() const;

      // rare case that an array is dereferenced, treated as array[0]
      MemLocObjectPtr getDereference();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject & that) const;*/
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const;
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ArrayNamedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class PointerNamedObj: public Pointer_Impl, public NamedObj
  {
    public:
      PointerNamedObj(SgSymbol* s, SgType* t, PartPtr part, MemLocObjectPtr p, IndexVectorPtr iv): NamedObj (s,t, part, p, iv) {}
      PointerNamedObj(const PointerNamedObj& that): NamedObj(that.anchor_symbol, that.type, that.part, that.parent, that.array_index_vector) {}
      //std::set<SgType*> getType() const;
      // used for a pointer to non-array
      MemLocObjectPtr getDereference();
      // used for a pointer to an array
      MemLocObjectPtr getElements();
      // Returns true if this pointer refers to the same abstract object as that pointer.
      //GB: getDereference subsumes this
      //bool equalPoints(const Pointer & that);
      // Returns true if this object and that object may/must refer to the same pointer memory object.
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject & that) const ;*/
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const PointerNamedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  // --------------------------
  // ----- Aliased object -----
  // --------------------------
  class ScalarAliasedObj: public Scalar_Impl, public AliasedObj
  {
    public:
      ScalarAliasedObj(SgType* t, PartPtr part): AliasedObj(t, part){}
      ScalarAliasedObj(const ScalarAliasedObj& that): AliasedObj(that.type, that.part) {}
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ScalarAliasedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class FunctionAliasedObj: public Function_Impl, public AliasedObj
  {
    public:
      FunctionAliasedObj(SgType* t, PartPtr part): AliasedObj(t, part){}
      FunctionAliasedObj(const FunctionAliasedObj& that): AliasedObj(that.type, that.part) {}
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const FunctionAliasedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };
  
  class LabeledAggregateAliasedObj : public  LabeledAggregate_Impl, public AliasedObj
  {
    public:
      LabeledAggregateAliasedObj(SgType* t, PartPtr part): AliasedObj(t, part){}
      LabeledAggregateAliasedObj(const LabeledAggregateAliasedObj& that): AliasedObj(that.type, that.part) {}
      //std::set<SgType*> getType();
      //TODO
      // size_t fieldCount();
      // std::list<LabeledAggregateField*> getElements() const;
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const LabeledAggregateAliasedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class ArrayAliasedObj: public Array_Impl, public AliasedObj
  {
    public:
      ArrayAliasedObj (SgType* t, PartPtr part): AliasedObj(t, part){}
      ArrayAliasedObj(const ArrayAliasedObj& that): AliasedObj(that.type, that.part) {}
      //std::set<SgType*> getType();

      //TODO
      // MemLocObject* getElements();
      //  MemLocObject* getElements(AbstractIndex* ai);
      //  getNumDims();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      // GB: 2012-08-27: should be implementing the following functions here:
      //                 Array::getElements(), getElements(IndexVectorPtr ai), getNumDims(), getDereference()
      boost::shared_ptr<MemLocObject> getElements();
      boost::shared_ptr<MemLocObject> getElements(IndexVectorPtr ai);
      size_t getNumDims();
      boost::shared_ptr<MemLocObject> getDereference();
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const ArrayAliasedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  class PointerAliasedObj: public Pointer_Impl, public AliasedObj
  {
    public:
      PointerAliasedObj (SgType* t, PartPtr part): AliasedObj(t, part){}
      PointerAliasedObj(const PointerAliasedObj& that): AliasedObj(that.type, that.part) {}
      MemLocObjectPtr getDereference();
      // MemLocObject * getElements() const;
      // Returns true if this pointer refers to the same abstract object as that pointer.
      //GB: getDereference subsumes this
      //bool equalPoints(const Pointer & that);
      //std::set<SgType*> getType();
      /* GB: Deprecating the == operator. Now that some objects can contain AbstractObjects any equality test must take the current part as input.
      bool operator == (const MemLocObject& o2) const; */
      bool mayEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      bool mustEqualML(MemLocObjectPtr o2, PartPtr p) const; 
      
      std::string str(std::string indent) const; // pretty print for the object
      std::string str(std::string indent) { return ((const PointerAliasedObj*)this)->str(indent); }
      
      // Allocates a copy of this object and returns a pointer to it
      MemLocObjectPtr copyML() const;
  };

  // lower level, internal  builder for different objects -----------------------
  // Users should use MemLocObject* ObjSetFactory::createObjSet (SgNode* n) instead
  
  // Create an aliased obj set from a type. It can return NULL since not all types are supported.
  MemLocObjectPtr createAliasedMemLocObject(SgType*t, PartPtr part);  // One object per type, Type based alias analysis
  MemLocObjectPtr createNamedMemLocObject(SgSymbol* anchor_symbol, SgType* t, PartPtr part, MemLocObjectPtr parent, IndexVectorPtr iv); // any 
  MemLocObjectPtr createNamedMemLocObject(SgVarRefExp* r, PartPtr part); // create NamedMemLocObject or AliasedMemLocObject (for pointer type) from a variable reference 
  MemLocObjectPtr createNamedMemLocObject(SgPntrArrRefExp* r, PartPtr part); // create NamedMemLocObject from an array element access
  MemLocObjectPtr createExpressionMemLocObject(SgExpression* anchor_exp, SgType*t, PartPtr part); 
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
  void fillUpElements (MemLocObject* p, std::vector<boost::shared_ptr<LabeledAggregateField> > & elements, SgClassType* c_t, PartPtr part);

  // convert std::vector<SgExpression*>* subscripts to IndexVectorPtr  array_index_vector
  IndexVectorPtr generateIndexVector (std::vector<SgExpression*>& subscripts);
}; //namespace dataflow

#endif