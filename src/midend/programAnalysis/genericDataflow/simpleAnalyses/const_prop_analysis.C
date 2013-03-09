#include "const_prop_analysis.h"

#include <boost/bind.hpp>
#include <boost/mem_fn.hpp>
#include <boost/make_shared.hpp>
  
namespace dataflow {
int constantPropagationAnalysisDebugLevel = 2;

// **********************************************************************
//              ConstantPropagationLattice
// **********************************************************************

CPValueObject::CPValueObject(PartEdgePtr pedge) : Lattice(pedge), FiniteLattice(pedge), ValueObject(NULL)
{
  this->value = 0;
  this->level = bottom;
}

CPValueObject::CPValueObject(int v, PartEdgePtr pedge) : Lattice(pedge), FiniteLattice(pedge), ValueObject(NULL)
{
  this->value = v;
  this->level = constantValue;
}

CPValueObject::CPValueObject(short level, int v, PartEdgePtr pedge) : Lattice(pedge), FiniteLattice(pedge), ValueObject(NULL)
{
  this->value = v;
  this->level = level;
}

// This is the same as the implicit definition, so it might not be required to be defined explicitly.
// I am searching for the minimal example of the use of the data flow classes.
CPValueObject::CPValueObject(const CPValueObject & that) : Lattice(that.latPEdge), FiniteLattice(that.latPEdge), ValueObject(that)
{
  this->value = that.value;
  this->level = that.level;
}

int
CPValueObject::getValue() const
{
   return value;
}

short
CPValueObject::getLevel() const
{
   return level;
}
  
bool
CPValueObject::setValue(int x)
 {
  // These are more than access functions, they return if the state of the lattice has changed.
  bool modified = this->level != constantValue || this->value != value;
  this->value = x;
  level = constantValue;
  return modified;
}

bool
CPValueObject::setLevel(short x)
{
  // These are more than access functions, they return if the state of the lattice has changed.
  bool modified = this->level != x;
  level = x;
  return modified;
}

bool
CPValueObject::setBottom()
{
  // These are more than access functions, they return if the state of the lattice has changed.
  bool modified = this->level != bottom;
  this->value = 0;
  level = bottom;
  return modified;
}

bool
CPValueObject::setTop()
{
  // These are more than access functions, they return if the state of the lattice has changed.
  bool modified = this->level != top;
  this->value = 0;
  level = top;
  return modified;
}

// **********************************************
// Required definition of pure virtual functions.
// **********************************************
void
CPValueObject::initialize()
{
  // Use the default constructor (implemented above).
  // So nothing to do here.
}


// returns a copy of this lattice
Lattice*
CPValueObject::copy() const
{
  return new CPValueObject(*this);
}


// overwrites the state of "this" Lattice with "that" Lattice
void
CPValueObject::copy(Lattice* X)
{
  Lattice::copy(X);
  CPValueObject* that = dynamic_cast<CPValueObject*>(X);

  this->value = that->value;
  this->level = that->level;
}


bool
CPValueObject::operator==(Lattice* X) /*const*/
{
  // Implementation of equality operator.
  CPValueObject* that = dynamic_cast<CPValueObject*>(X);
  return (value == that->value) && (level == that->level);
}

// computes the meet of this and that and saves the result in this
// returns true if this causes this to change and false otherwise
bool
CPValueObject::meetUpdate(Lattice* X)
{
  CPValueObject* that = dynamic_cast<CPValueObject*>(X);

  // Need to handle bottom, copy from the other side.

  // This is the more technically interesting required function.
  //   2. unknownValue, missing the cases that meet bottom, top ??
  if (this->level == unknownValue && that->level == unknownValue) {
    // leave it and return false
    return false;
  }
  else
  {
    if (this->level == unknownValue && that->level == constantValue)
    {
      this->level = constantValue;
      this->value = that->value;
      return true;
    }
    else
    {   //  3. const value: missing the cases of meeting bottom and top ??
      if (this->level == constantValue && that->level == unknownValue) {
        return false;
      } else {
        if (this->level == constantValue && that->level == constantValue) {
          if (this->value == that->value) {
            return false;
           } else {
             this->level = top;
             return true;
           }
        }
        else // 4.  undefinedValue  is not covered ???
        {
          if (this->level == bottom) { // 1. can only go up: must be the value of that lattice since this lattice is already the bottom
            this->level = that->level;
            this->value = that->value;

            return (that->level != bottom);
           } else {
             if (this->level == top) { //5.  already at the top. Cannot go up further.
               return false;
             } else {
               if (that->level == top) {
                 bool modified = this->level != top;
                 this->level = top;
                 return modified;
               }
             }
          }
        }
      }
    }
  }

  // Make up a return value for now.
  return false;
}

// Set this Lattice object to represent the set of all possible execution prefixes.
// Return true if this causes the object to change and false otherwise.
bool CPValueObject::setToFull()
{
  return setTop();
}

// Set this Lattice object to represent the of no execution prefixes (empty set)
// Return true if this causes the object to change and false otherwise.
bool CPValueObject::setToEmpty()
{
  return setBottom();
}

string
CPValueObject::str(string indent) const
{
  return strp(latPEdge, indent);
}

string
CPValueObject::strp(PartEdgePtr pedge, string indent) const
{    
  ostringstream outs;
  if(level == bottom)
    outs << "[bottom]";
  else if(level == unknownValue)
    outs << "[unknown: "<<value<<"]";
  else if(level == constantValue)
    outs << "[const: "<<value<<"]";
  else if(level == top)
    outs << "[top]";

  return outs.str();
}


bool CPValueObject::mayEqual(ValueObjectPtr o, PartEdgePtr pedge)
{
  return mustEqual(o, pedge);
}

bool CPValueObject::mustEqual(ValueObjectPtr o, PartEdgePtr pedge)
{
  CPValueObjectPtr that = boost::dynamic_pointer_cast<CPValueObject>(o);
  if(!that) { return false; }
  return (value == that->value) && (level == that->level);
}

// Allocates a copy of this object and returns a pointer to it
ValueObjectPtr CPValueObject::copyV() const
{
  return boost::make_shared<CPValueObject>(*this);
}

// Returns true if this ValueObject corresponds to a concrete value that is statically-known
bool CPValueObject::isConcrete()
{ return level==constantValue; }

// Returns the type of the concrete value (if there is one)
SgType* CPValueObject::getConcreteType()
{
  ROSE_ASSERT(isConcrete());
  // GB 2012-09-26: We only support integer types for our constants. This will need to be improved.
  return SageBuilder::buildIntType();
}

// Returns the concrete value (if there is one) as an SgValueExp, which allows callers to use
// the normal ROSE mechanisms to decode it
boost::shared_ptr<SgValueExp> CPValueObject::getConcreteValue()
{
  ROSE_ASSERT(isConcrete());
  return boost::shared_ptr<SgValueExp>(SageBuilder::buildIntVal(value));
}
  
/*string CPValueObject::str(const string& indent)
{
  ostringstream outs;
  if(level == bottom)
    outs << indent << "[level: bottom]";
  else if(level == unknownValue)
    outs << indent << "[level: unknownValue, val = "<<value<<"]";
  else if(level == constantValue)
    outs << indent << "[level: constantValue, val = "<<value<<"]";
  else if(level == top)
    outs << indent << "[level: top]";
  
  return outs.str();
}*/

// **********************************************************************
//            ConstantPropagationAnalysisTransfer
// **********************************************************************

template <typename T>
void ConstantPropagationAnalysisTransfer::transferArith(SgBinaryOp *sgn, T transferOp)
   {
   CPValueObjectPtr arg1Lat, arg2Lat, resLat;
   getLattices(sgn, arg1Lat, arg2Lat, resLat);
      {
      transferOp(this, arg1Lat, arg2Lat, resLat);
      setLattice(sgn, resLat);
      if (isSgCompoundAssignOp(sgn))
        setLatticeOperand(sgn, sgn->get_lhs_operand(), resLat);
      }
   }

void
ConstantPropagationAnalysisTransfer::transferArith(SgBinaryOp *sgn, TransferOp transferOp)
   {
   transferArith(sgn, boost::mem_fn(transferOp));
   }

void 
ConstantPropagationAnalysisTransfer::transferIncrement(SgUnaryOp *sgn)
   {
   CPValueObjectPtr arg1Lat, arg2Lat, resLat;
   getLattices(sgn, arg1Lat, arg2Lat, resLat);
   transferAdditive(arg1Lat, arg2Lat, resLat, isSgPlusPlusOp(sgn));
   setLattice(sgn, resLat);
   // GB: shouldn't need to deallocate when we consistently use boost shared pointers
   //delete arg2Lat; // Allocated by getLattices
   setLatticeOperand(sgn, sgn->get_operand(), resLat);
   }

void
ConstantPropagationAnalysisTransfer::transferAdditive(CPValueObjectPtr arg1Lat, CPValueObjectPtr arg2Lat, CPValueObjectPtr resLat, bool isAddition)
   {
   if (arg1Lat->getLevel() == CPValueObject::bottom || arg2Lat->getLevel() == CPValueObject::bottom)
      {
      updateModified(resLat->setLevel(CPValueObject::bottom));
      }
     else 
      {
      // Both knownValue
      if(arg1Lat->getLevel() == CPValueObject::constantValue && arg2Lat->getLevel() == CPValueObject::constantValue) 
         {
         updateModified(resLat->setValue(isAddition ? arg1Lat->getValue() + arg2Lat->getValue() : arg1Lat->getValue() - arg2Lat->getValue()));
         }
        else
         {
         // Else => Top
         updateModified(resLat->setLevel(CPValueObject::top));
         }
      }
   }


void
ConstantPropagationAnalysisTransfer::transferMultiplicative(CPValueObjectPtr arg1Lat, CPValueObjectPtr arg2Lat, CPValueObjectPtr resLat)
   {
   if (arg1Lat->getLevel() == CPValueObject::bottom || arg2Lat->getLevel() == CPValueObject::bottom)
      {
      updateModified(resLat->setLevel(CPValueObject::bottom));
      }
     else 
      {
     // Both knownValue
      if(arg1Lat->getLevel() == CPValueObject::constantValue && arg2Lat->getLevel() == CPValueObject::constantValue) 
         {
         updateModified(resLat->setValue(arg1Lat->getValue() * arg2Lat->getValue()));
         }
        else
         {
        // Else => Top
         updateModified(resLat->setLevel(CPValueObject::top));
         }
      }
   }

void
ConstantPropagationAnalysisTransfer::transferDivision(CPValueObjectPtr arg1Lat, CPValueObjectPtr arg2Lat, CPValueObjectPtr resLat)
   {
   if (arg1Lat->getLevel() == CPValueObject::bottom || arg2Lat->getLevel() == CPValueObject::bottom)
      {
      updateModified(resLat->setLevel(CPValueObject::bottom));
      }
     else 
      {
     // Both knownValue
      if(arg1Lat->getLevel() == CPValueObject::constantValue && arg2Lat->getLevel() == CPValueObject::constantValue) 
         {
         updateModified(resLat->setValue(arg1Lat->getValue() / arg2Lat->getValue()));
         }
        else
         {
        // Else => Top
         updateModified(resLat->setLevel(CPValueObject::top));
         }
      }
   }

void
ConstantPropagationAnalysisTransfer::transferMod(CPValueObjectPtr arg1Lat, CPValueObjectPtr arg2Lat, CPValueObjectPtr resLat)
   {
   if (arg1Lat->getLevel() == CPValueObject::bottom || arg2Lat->getLevel() == CPValueObject::bottom)
      {
      updateModified(resLat->setLevel(CPValueObject::bottom));
      }
     else 
      {
     // Both knownValue
      if(arg1Lat->getLevel() == CPValueObject::constantValue && arg2Lat->getLevel() == CPValueObject::constantValue) 
         {
         updateModified(resLat->setValue(arg1Lat->getValue() % arg2Lat->getValue()));
         }
        else
         {
        // Else => Top
         updateModified(resLat->setLevel(CPValueObject::top));
         }
      }
   }

void
ConstantPropagationAnalysisTransfer::transferLogical(CPValueObjectPtr arg1Lat, CPValueObjectPtr arg2Lat, 
                                                     CPValueObjectPtr resLat, SgExpression* expr)
{
  if (arg1Lat->getLevel() == CPValueObject::bottom || arg2Lat->getLevel() == CPValueObject::bottom)
    updateModified(resLat->setLevel(CPValueObject::bottom));
  else {
    // Both knownValue
    if(arg1Lat->getLevel() == CPValueObject::constantValue && arg2Lat->getLevel() == CPValueObject::constantValue) {
      switch(expr->variantT()) {
        case V_SgGreaterOrEqualOp: updateModified(resLat->setValue(arg1Lat->getValue() >= arg2Lat->getValue())); break;
        case V_SgGreaterThanOp:    updateModified(resLat->setValue(arg1Lat->getValue() >  arg2Lat->getValue())); break;
        case V_SgLessOrEqualOp:    updateModified(resLat->setValue(arg1Lat->getValue() <= arg2Lat->getValue())); break;
        case V_SgLessThanOp:       updateModified(resLat->setValue(arg1Lat->getValue() <  arg2Lat->getValue())); break;
        case V_SgEqualityOp:       updateModified(resLat->setValue(arg1Lat->getValue() == arg2Lat->getValue())); break;
        case V_SgNotEqualOp:       updateModified(resLat->setValue(arg1Lat->getValue() != arg2Lat->getValue())); break;
        case V_SgAndOp:            updateModified(resLat->setValue(arg1Lat->getValue() && arg2Lat->getValue())); break;
        case V_SgOrOp:             updateModified(resLat->setValue(arg1Lat->getValue() || arg2Lat->getValue())); break;
        default: ROSE_ASSERT(0);
      }
    } else {
      // Else => Top
      updateModified(resLat->setLevel(CPValueObject::top));
    }
  }
}

// Values
void
ConstantPropagationAnalysisTransfer::visit(SgLongLongIntVal *sgn)
   {
   }

void
ConstantPropagationAnalysisTransfer::visit(SgLongIntVal *sgn)
   {
   }

void
ConstantPropagationAnalysisTransfer::visit(SgIntVal *sgn)
   {
   if(constantPropagationAnalysisDebugLevel>=1) Dbg::dbg << "ConstantPropagationAnalysisTransfer::visit() composer="<<composer<<endl;
   ROSE_ASSERT(sgn);
   //CPValueObjectPtr resLat = getLattice(sgn);
   CPValueObjectPtr resLat(new CPValueObject(sgn->get_value(), part->inEdgeFromAny()));
   ROSE_ASSERT(resLat);
   //resLat->setValue(sgn->get_value());
   //resLat->setLevel(CPValueObject::constantValue);
   setLattice(sgn, resLat);
   }

void
ConstantPropagationAnalysisTransfer::visit(SgShortVal *sgn)
   {
   }

void
ConstantPropagationAnalysisTransfer::visit(SgUnsignedLongLongIntVal *sgn)
   {
   }

void
ConstantPropagationAnalysisTransfer::visit(SgUnsignedLongVal *sgn)
   {
   }

void
ConstantPropagationAnalysisTransfer::visit(SgUnsignedIntVal *sgn)
   {
   }

void
ConstantPropagationAnalysisTransfer::visit(SgUnsignedShortVal *sgn)
   {
   }

void
ConstantPropagationAnalysisTransfer::visit(SgValueExp *sgn)
   {
   }

// Arithmetic Operations
void
ConstantPropagationAnalysisTransfer::visit(SgPlusAssignOp *sgn)
   {
   transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferAdditive, _1, _2, _3, _4, true ));
   }

void
ConstantPropagationAnalysisTransfer::visit(SgMinusAssignOp *sgn)
   {
   transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferAdditive, _1, _2, _3, _4, false));
   }

void
ConstantPropagationAnalysisTransfer::visit(SgMultAssignOp *sgn)
   {
   transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferMultiplicative, _1, _2, _3, _4 ));
   }

void
ConstantPropagationAnalysisTransfer::visit(SgDivAssignOp *sgn)
   {
   transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferDivision, _1, _2, _3, _4 ));
   }

void
ConstantPropagationAnalysisTransfer::visit(SgModAssignOp *sgn)
   {
   transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferMod, _1, _2, _3, _4 ));
   }

void
ConstantPropagationAnalysisTransfer::visit(SgAddOp *sgn)
   {
   transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferAdditive, _1, _2, _3, _4, true ));
   }

void
ConstantPropagationAnalysisTransfer::visit(SgSubtractOp *sgn)
   {
   transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferAdditive, _1, _2, _3, _4, false));
   }

void
ConstantPropagationAnalysisTransfer::visit(SgMultiplyOp *sgn)
   {
   transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferMultiplicative, _1, _2, _3, _4 ));
   }

void
ConstantPropagationAnalysisTransfer::visit(SgDivideOp *sgn)
   {
   transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferDivision, _1, _2, _3, _4 ));
   }

void
ConstantPropagationAnalysisTransfer::visit(SgModOp *sgn)
   {
   transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferMod, _1, _2, _3, _4 ));
   }

// Increment Operations
void
ConstantPropagationAnalysisTransfer::visit(SgPlusPlusOp *sgn)
   {
   transferIncrement(sgn);
   }

void
ConstantPropagationAnalysisTransfer::visit(SgMinusMinusOp *sgn)
   {
   transferIncrement(sgn);
   }

// Unary Operations
void
ConstantPropagationAnalysisTransfer::visit(SgUnaryAddOp *sgn)
   {
   setLattice(sgn, getLatticeOperand(sgn, sgn->get_operand()));
   }

void
ConstantPropagationAnalysisTransfer::visit(SgMinusOp *sgn)
   {
   CPValueObjectPtr operandLat = getLatticeOperand(sgn, sgn->get_operand());

   // This fixes up the value if it is relevant (where level is neither top not bottom).
   operandLat->setValue(-operandLat->getValue());
   
   setLattice(sgn, operandLat);
   }

void 
ConstantPropagationAnalysisTransfer::visit(SgCastExp *sgn)
   {
   setLattice(sgn, getLatticeOperand(sgn, sgn->get_operand()));
   }

// Logical Operations
void
ConstantPropagationAnalysisTransfer::visit(SgGreaterOrEqualOp *sgn)
{ transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferLogical, _1, _2, _3, _4, sgn)); }

void
ConstantPropagationAnalysisTransfer::visit(SgGreaterThanOp *sgn)
{ transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferLogical, _1, _2, _3, _4, sgn)); }

void
ConstantPropagationAnalysisTransfer::visit(SgLessOrEqualOp *sgn)
{ transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferLogical, _1, _2, _3, _4, sgn)); }

void
ConstantPropagationAnalysisTransfer::visit(SgLessThanOp *sgn)
{ transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferLogical, _1, _2, _3, _4, sgn)); }

void
ConstantPropagationAnalysisTransfer::visit(SgEqualityOp *sgn)
{ transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferLogical, _1, _2, _3, _4, sgn)); }

void
ConstantPropagationAnalysisTransfer::visit(SgNotEqualOp *sgn)
{ transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferLogical, _1, _2, _3, _4, sgn)); }

void
ConstantPropagationAnalysisTransfer::visit(SgAndOp *sgn)
{ transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferLogical, _1, _2, _3, _4, sgn)); }

void
ConstantPropagationAnalysisTransfer::visit(SgOrOp *sgn)
{ transferArith(sgn, boost::bind(&ConstantPropagationAnalysisTransfer::transferLogical, _1, _2, _3, _4, sgn)); }

bool
ConstantPropagationAnalysisTransfer::finish()
   {
   return modified;
   }

ConstantPropagationAnalysisTransfer::ConstantPropagationAnalysisTransfer(
          const Function& func, PartPtr part, CFGNode cn, NodeState& state, 
          std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo, 
          Composer* composer, ConstantPropagationAnalysis* analysis)
   : VariableStateTransfer<CPValueObject>(func, state, dfInfo, boost::make_shared<CPValueObject>(part->inEdgeFromAny()), 
                                          composer, analysis, part, cn, constantPropagationAnalysisDebugLevel)
   {
   }




// **********************************************************************
//             ConstantPropagationAnalysis
// **********************************************************************

// GB: Is this needed for boost shared-pointers?
ConstantPropagationAnalysis::ConstantPropagationAnalysis()
{
}

// Initializes the state of analysis lattices at the given function, part and edge into our out of the part
// by setting initLattices to refer to freshly-allocated Lattice objects.
void ConstantPropagationAnalysis::genInitLattice(const Function& func, PartPtr part, PartEdgePtr pedge, 
                                                 std::vector<Lattice*>& initLattices)
{
  AbstractObjectMap* l = new AbstractObjectMap(new MustEqualFunctor(), 
                                               boost::make_shared<CPValueObject>(pedge/*part->inEdgeFromAny()*/),
                                               pedge);
  /*Dbg::dbg << "ConstantPropagationAnalysis::initializeState, analysis="<<returning l="<<l<<" n=<"<<Dbg::escape(p.getNode()->unparseToString())<<" | "<<p.getNode()->class_name()<<" | "<<p.getIndex()<<">\n";
  Dbg::dbg << "    l="<<l->str("    ")<<endl;*/
  initLattices.push_back(l);

  // GB: WE NEED TO INITIALIZE THIS LATTICE WITH THE CURRENTLY LIVE VARIABLES. E.G. AS INITIALIZATION-TIME
}
  
bool
ConstantPropagationAnalysis::transfer(const Function& func, PartPtr p, CFGNode cn, NodeState& state, 
                                      std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo)
   {
   assert(0); 
   return false;
   }

boost::shared_ptr<IntraDFTransferVisitor>
ConstantPropagationAnalysis::getTransferVisitor(const Function& func, PartPtr part, CFGNode cn, NodeState& state, 
                                                std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo)
   {
  // Why is the boost shared pointer used here?
   ConstantPropagationAnalysisTransfer* t = new ConstantPropagationAnalysisTransfer(func, part, cn, state, dfInfo, getComposer(), this);
   return boost::shared_ptr<IntraDFTransferVisitor>(t);
   }

ValueObjectPtr ConstantPropagationAnalysis::Expr2Val(SgNode* n, PartEdgePtr pedge)
{
  if(constantPropagationAnalysisDebugLevel>=1) Dbg::dbg << "ConstantPropagationAnalysis::Expr2Val(n="<<cfgUtils::SgNode2Str(n)<<", pedge="<<pedge->str()<<") this="<<this<<endl;
  // If pedge doesn't have wildcards
  if(pedge->source() && pedge->target()) {
    NodeState* state = NodeState::getNodeState(this, pedge->source());
    //Dbg::dbg << "state="<<state->str(this)<<endl;
    AbstractObjectMap* cpMap = dynamic_cast<AbstractObjectMap*>(state->getLatticeBelow(this, pedge, 0));
    if(cpMap == NULL) {
      Lattice* l = state->getLatticeBelow(this, pedge, 0);
      Dbg::dbg << "l="<<l->str()<<endl;
    }
    ROSE_ASSERT(cpMap);
    
    // MemLocObjectPtrPair p = composer->Expr2MemLoc(n, pedge, this);
    MemLocObjectPtr p = composer->Expr2MemLoc(n, pedge, this);
    if(constantPropagationAnalysisDebugLevel>=2) {
      Dbg::indent ind;
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;p="<<p->str()<<endl;
      Dbg::dbg << "cpMap="<<cpMap<<"="<<cpMap->str()<<endl;
    }

    // Return the lattice associated with n's expression since that is likely to be more precise
    // but if it is not available, used the memory object
    // return (p.expr ? boost::dynamic_pointer_cast<ValueObject>(cpMap->get(p.expr)) :
    //                  boost::dynamic_pointer_cast<ValueObject>(cpMap->get(p.mem)));
    return (boost::dynamic_pointer_cast<ValueObject>(cpMap->get(p)));
  // If the target of this edge is a wildcard
  } else if(pedge->source()) {
    NodeState* state = NodeState::getNodeState(this, pedge->source());
    //Dbg::dbg << "state="<<state->str(this)<<endl;
    
    // Merge the lattices along all the outgoing edges
    map<PartEdgePtr, std::vector<Lattice*> >& e2lats = state->getLatticeBelowAllMod(this);
    ROSE_ASSERT(e2lats.size()>=1);
    boost::shared_ptr<CPValueObject> mergedVal = boost::make_shared<CPValueObject>(pedge);
    for(map<PartEdgePtr, std::vector<Lattice*> >::iterator lats=e2lats.begin(); lats!=e2lats.end(); lats++) {
      PartEdge* edgePtr = lats->first.get();
      ROSE_ASSERT(edgePtr->source() == pedge.get()->source());
      
      AbstractObjectMap* cpMap = dynamic_cast<AbstractObjectMap*>(state->getLatticeBelow(this, lats->first, 0));
      ROSE_ASSERT(cpMap);
      
      // MemLocObjectPtrPair p = composer->Expr2MemLoc(n, pedge, this);
      MemLocObjectPtr p = composer->Expr2MemLoc(n, pedge, this);
      if(constantPropagationAnalysisDebugLevel>=2) {
        Dbg::indent ind;
        Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;p="<<p->str()<<endl;
        Dbg::dbg << "cpMap="<<cpMap<<"="<<cpMap->str()<<endl;
      }
      
      // boost::shared_ptr<CPValueObject> val = 
      //         boost::dynamic_pointer_cast<CPValueObject>
      //           (p.expr ? boost::dynamic_pointer_cast<ValueObject>(cpMap->get(p.expr)) :
      //                     boost::dynamic_pointer_cast<ValueObject>(cpMap->get(p.mem)));
      boost::shared_ptr<CPValueObject> val = 
        boost::dynamic_pointer_cast<CPValueObject> (boost::dynamic_pointer_cast<ValueObject>(cpMap->get(p)));

      mergedVal->meetUpdate(val.get());
    }
    return mergedVal;
  // If the source of this edge is a wildcard
  } else if(pedge->target()) {
    NodeState* state = NodeState::getNodeState(this, pedge->target());
    if(constantPropagationAnalysisDebugLevel>=2) Dbg::dbg << "state="<<state->str()<<endl;
    AbstractObjectMap* cpMap = dynamic_cast<AbstractObjectMap*>(state->getLatticeAbove(this, NULLPartEdge, 0));
    ROSE_ASSERT(cpMap);
    
    // MemLocObjectPtrPair p = composer->Expr2MemLoc(n, pedge, this);
    MemLocObjectPtr p = composer->Expr2MemLoc(n, pedge, this);
    if(constantPropagationAnalysisDebugLevel>=2) {
      Dbg::indent ind;
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;p="<<p->str()<<endl;
      Dbg::dbg << "cpMap="<<cpMap<<"="<<cpMap->str()<<endl;
    }

    // Return the lattice associated with n's expression since that is likely to be more precise
    // but if it is not available, used the memory object
    // return (p.expr ? boost::dynamic_pointer_cast<ValueObject>(cpMap->get(p.expr)) :
    //                  boost::dynamic_pointer_cast<ValueObject>(cpMap->get(p.mem)));
    return boost::dynamic_pointer_cast<ValueObject>(cpMap->get(p));
  }
  ROSE_ASSERT(0);
}

}; // namespace dataflow;
