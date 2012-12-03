#include "abstract_object.h"
#include "stx_analysis.h"
#include <iostream>

using namespace std;

namespace dataflow
{
bool AbstractObject::mustEqual(AbstractObjectPtr o, PartEdgePtr pedge)
{
  // If both AbstractObjects have non-NULL bases, we can tell that they're must-equal by simply confirming
  // that their bases are equal
  if(base && o->base && base==o->base) {
    //Dbg::dbg << "AbstractObject::mustEqual() base="<<cfgUtils::SgNode2Str(base)<<" o->base="<<cfgUtils::SgNode2Str(o->base)<<endl;
    return true;
  // Otherwise, we don't know and must answer conservatively
  } else return false;
}

/* ########################
   ##### MemLocObject ##### 
   ######################## */

MemLocObjectPtr NULLMemLocObject;
  
// General version of mayEqual and mustEqual that implements may/must equality with respect to ExprObj
// and uses the derived class' may/mustEqual check for all the other cases
bool MemLocObject::mayEqual(MemLocObjectPtr o, PartEdgePtr pedge)
{
  // If both this and that are both expression objects or both not expression objects, use the
  // derived class' equality check
  //Dbg::dbg << "MemLocObject::mayEqual() dynamic_cast<const ExprObj*>(this)="<<dynamic_cast<const ExprObj*>(this)<<" dynamic_cast<const ExprObj*>(o.get())="<<dynamic_cast<const ExprObj*>(o.get())<<endl;
  if((dynamic_cast<const ExprObj*>(this)  && dynamic_cast<const ExprObj*>(o.get())) ||
     (!dynamic_cast<const ExprObj*>(this) && !dynamic_cast<const ExprObj*>(o.get())))
  { return mayEqualML(o, pedge); }
  // Otherwise, we know they're not equal
  { return false; }
}
bool MemLocObject::mustEqual(MemLocObjectPtr o, PartEdgePtr pedge)
{
  if(AbstractObject::mustEqual(boost::static_pointer_cast<AbstractObject>(o), pedge)) return true;
  
  // If both this and that are both expression objects or both not expression objects, use the
  // derived class' equality check
  //Dbg::dbg << "MemLocObject::mustEqual() dynamic_cast<const ExprObj*>(this)="<<dynamic_cast<const ExprObj*>(this)<<"="<<const_cast<MemLocObject*>(this)->str("")<<endl;
  //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;dynamic_cast<const ExprObj*>(o.get())="<<dynamic_cast<const ExprObj*>(o.get())<<"="<<o->str("")<<endl;
  if((dynamic_cast<const ExprObj*>(this)  && dynamic_cast<const ExprObj*>(o.get())) ||
     (!dynamic_cast<const ExprObj*>(this) && !dynamic_cast<const ExprObj*>(o.get())))
  { return mustEqualML(o, pedge); }
  // Otherwise, we know they're not equal
  { return false; }
}

bool MemLocObject::mayEqual(AbstractObjectPtr o, PartEdgePtr pedge)
{
  MemLocObjectPtr mo = boost::dynamic_pointer_cast<MemLocObject>(o);
  if(mo) return mayEqual(mo, pedge);
  else   return false;
}

bool MemLocObject::mustEqual(AbstractObjectPtr o, PartEdgePtr pedge)
{
  if(AbstractObject::mustEqual(o, pedge)) return true;
  
  MemLocObjectPtr mo = boost::dynamic_pointer_cast<MemLocObject>(o);
  if(mo) return mustEqual(mo, pedge);
  else   return false;
}

/* ###############################
   ##### MemLocObjectPtrPair ##### 
   ############################### */

// Returns whether this object may/must be equal to o within the given Part p
bool MemLocObjectPtrPair::mayEqual(MemLocObjectPtrPair that, PartEdgePtr pedge)
{
  // Both this and that have the same structure
  assert((expr && that.expr) || (!mem && !that.mem));
  // At least one of expr or mem have to be non-NULL
  assert(expr || mem);
  return (expr ? expr->mayEqual(that.expr, pedge): false) ||
         (mem  ? mem->mayEqual (that.mem, pedge): false); 
}

bool MemLocObjectPtrPair::mustEqual(MemLocObjectPtrPair that, PartEdgePtr pedge)
{
  // Both this and that have the same structure
  assert((expr && that.expr) || (!mem && !that.mem));
  // At least one of expr or mem have to be non-NULL
  assert(expr || mem);
  return (expr ? expr->mustEqual(that.expr, pedge): true) &&
         (mem  ? mem->mustEqual (that.mem, pedge): true);
}

// Allocates a copy of this object and returns a pointer to it
MemLocObjectPtrPair MemLocObjectPtrPair::copyML() const {
  return MemLocObjectPtrPair(*this);
}

bool MemLocObjectPtrPair::isScalar()
{
  // At least one of expr or mem have to be non-NULL
  assert(expr || mem);
  if(expr && mem) {
    bool exprScalar = expr->isScalar();
    bool memScalar  = mem->isScalar();
    assert(exprScalar == memScalar);
    return exprScalar;
  }
  if(expr) return expr->isScalar() ? true: false;
  if(mem)  return mem->isScalar() ? true: false;
  ROSE_ASSERT(0);
}

bool MemLocObjectPtrPair::isFunctionMemLoc()
{
  // At least one of expr or mem have to be non-NULL
  assert(expr || mem);
  if(expr && mem) {
    bool exprF = expr->isFunctionMemLoc();
    bool memF  = mem->isFunctionMemLoc();
    assert(exprF == memScalar);
    return exprF;
  }
  if(expr) return expr->isFunctionMemLoc() ? true: false;
  if(mem)  return mem->isFunctionMemLoc() ? true: false;
  ROSE_ASSERT(0);
}

bool MemLocObjectPtrPair::isLabeledAggregate()
{
  // At least one of expr or mem have to be non-NULL
  assert(expr || mem);
  if(expr && mem) {
    bool exprLA = expr->isLabeledAggregate();
    bool memLA  = mem->isLabeledAggregate();
    assert(exprLA == memLA);
    return exprLA;
  }
  if(expr) return expr->isLabeledAggregate() ? true: false;
  if(mem)  return mem->isLabeledAggregate() ? true: false;
  ROSE_ASSERT(0);
}

bool MemLocObjectPtrPair::isArray()
{
  // At least one of expr or mem have to be non-NULL
  assert(expr || mem);
  if(expr && mem) {
    bool exprArray = expr->isArray();
    bool memArray  = mem->isArray();
    assert(exprArray == memScalar);
    return exprArray;
  }
  if(expr) return expr->isArray() ? true: false;
  if(mem)  return mem->isArray() ? true: false;
  ROSE_ASSERT(0);
}

bool MemLocObjectPtrPair::isPointer()
{
  // At least one of expr or mem have to be non-NULL
  assert(expr || mem);
  if(expr && mem) {
    bool exprPointer = expr->isPointer();
    bool memPointer  = mem->isPointer();
    assert(exprPointer == memScalar);
    return exprPoint;
  }
  if(expr) return expr->isPointer() ? true: false;
  if(mem)  return mem->isPointer() ? true: false;
  ROSE_ASSERT(0);
}

// pretty print for the object
std::string MemLocObjectPtrPair::str(std::string indent)
{ 
  ostringstream oss;
  oss << "[";
  if(expr) {
    oss << "expr=" << expr->str(indent);
    if(mem) oss << endl;
  }
  if(mem) {
    if(expr) oss << indent;
    oss << "mem="  << mem->str(indent);
  }
  oss << "]";
  return oss.str();
}

std::string MemLocObjectPtrPair::strp(PartEdgePtr pedge, std::string indent)
{ 
  ostringstream oss;
  oss << "[";
  if(expr) oss << "expr=" << expr->strp(pedge, indent) << endl;
  if(mem) {
    if(expr) oss << indent;
    oss << "mem="  << mem->strp(pedge, indent);
  }
  oss << "]";
  return oss.str();
}

/* ################################
   ##### CombinedMemLocObject ##### 
   ################################ */

template <bool defaultMayEq>
CombinedMemLocObject<defaultMayEq>::CombinedMemLocObject(MemLocObjectPtr memLoc) : MemLocObject(NULL) {
  memLocs.push_back(memLoc);
}

template <bool defaultMayEq>
CombinedMemLocObject<defaultMayEq>::CombinedMemLocObject(const std::list<MemLocObjectPtr>& memLocs) : MemLocObject(NULL), memLocs(memLocs) {}

// Creates a new CombinedMemLocObject. If all the sub-objects have a given type (Scalar, FunctionMemLoc, 
// LabeledAggregate, Array or Pointer), the created CombinedMemLocObject has hte same type. Otherwise, the
// created CombinedMemLocObject is an instance of the generic CombinedMemLocObject class.
template <bool defaultMayEq>
boost::shared_ptr<CombinedMemLocObject<defaultMayEq> > CombinedMemLocObject<defaultMayEq>::create(MemLocObjectPtr memLoc)
{
  list<MemLocObjectPtr> memLocs; memLocs.push_back(memLoc);

/*       if(memLoc->isScalar())           return CombinedScalar<defaultMayEq>::create(memLocs);
  else if(memLoc->isFunctionMemLoc())   return CombinedFunctionMemLoc<defaultMayEq>::create(memLocs);
  else if(memLoc->isLabeledAggregate()) return CombinedLabeledAggregate<defaultMayEq>::create(memLocs);
  else if(memLoc->isArray())            return CombinedArray<defaultMayEq>::create(memLocs);
  else if(memLoc->isPointer())          return CombinedPointer<defaultMayEq>::create(memLocs);*/
  
       if(memLoc->isScalar())           return boost::make_shared<CombinedScalar<defaultMayEq> >(memLocs);
  else if(memLoc->isFunctionMemLoc())   return boost::make_shared<CombinedFunctionMemLoc<defaultMayEq> >(memLocs);
  else if(memLoc->isLabeledAggregate()) return boost::make_shared<CombinedLabeledAggregate<defaultMayEq> >(memLocs);
  else if(memLoc->isArray())            return boost::make_shared<CombinedArray<defaultMayEq> >(memLocs);
  else if(memLoc->isPointer())          return boost::make_shared<CombinedPointer<defaultMayEq> >(memLocs);
  
  Dbg::dbg << "<font color=\"$#ff0000\">"<<memLoc->str()<<"</font>"<<endl;
  ROSE_ASSERT(0);
}

template <bool defaultMayEq>
boost::shared_ptr<CombinedMemLocObject<defaultMayEq> > CombinedMemLocObject<defaultMayEq>::create(const std::list<MemLocObjectPtr>& memLocs)
{
  // Determine if all the sub-memory locations are all the same type
  int allScalar=1, allFunctionMemLoc=1, allLabeledAggregate=1, allArray=1, allPointer=1;
  
  //Dbg::dbg << "CombinedMemLocObject<defaultMayEq>::create()"<<endl;
  Dbg::indent ind;
  for(std::list<MemLocObjectPtr>::const_iterator ml=memLocs.begin(); ml!=memLocs.end(); ml++) {
    Dbg::dbg << (*ml)->str()<<endl;
  }
  for(std::list<MemLocObjectPtr>::const_iterator ml=memLocs.begin(); ml!=memLocs.end(); ml++) {
    if(!(*ml)->isScalar())           allScalar           = 0;
    if(!(*ml)->isFunctionMemLoc())   allFunctionMemLoc   = 0;
    if(!(*ml)->isLabeledAggregate()) allLabeledAggregate = 0;
    if(!(*ml)->isArray())            allArray            = 0;
    if(!(*ml)->isPointer())          allPointer          = 0;
    
    //Dbg::dbg << "CombinedMemLocObject<"<<defaultMayEq<<">::create() ml="<<(*ml)->str()<<endl;
  }
  // Either all memLocs have the same one type or they're different
  ROSE_ASSERT(allScalar+allFunctionMemLoc+allLabeledAggregate+allArray+allPointer==0 ||
              allScalar+allFunctionMemLoc+allLabeledAggregate+allArray+allPointer==1);
  
  //Dbg::dbg << "CombinedMemLocObject<"<<defaultMayEq<<">::create() allScalar="<<allScalar<<" allFunctionMemLoc="<<allFunctionMemLoc<<" allLabeledAggregate="<<allLabeledAggregate<<" allArray="<<allArray<<" allPointer="<<allPointer<<endl;
  
  // If they're all the same type, create a combined object of the appropriate type
/*       if(allScalar)           return CombinedScalar<defaultMayEq>::create(memLocs);
  else if(allFunctionMemLoc)   return CombinedFunctionMemLoc<defaultMayEq>::create(memLocs);
  else if(allLabeledAggregate) return CombinedLabeledAggregate<defaultMayEq>::create(memLocs);
  else if(allArray)            return CombinedArray<defaultMayEq>::create(memLocs);
  else if(allPointer)          return CombinedPointer<defaultMayEq>::create(memLocs);*/
  
       if(allScalar)           return boost::make_shared<CombinedScalar<defaultMayEq> >(memLocs);
  else if(allFunctionMemLoc)   return boost::make_shared<CombinedFunctionMemLoc<defaultMayEq> >(memLocs);
  else if(allLabeledAggregate) return boost::make_shared<CombinedLabeledAggregate<defaultMayEq> >(memLocs);
  else if(allArray)            return boost::make_shared<CombinedArray<defaultMayEq> >(memLocs);
  else if(allPointer)          return boost::make_shared<CombinedPointer<defaultMayEq> >(memLocs);
  
  // Otherwise, create a generic CombinedMemLocObject
  //Dbg::dbg << "Returning generic"<<endl;
  return boost::make_shared<CombinedMemLocObject<defaultMayEq> >(memLocs);
}

template <bool defaultMayEq>
void CombinedMemLocObject<defaultMayEq>::add(MemLocObjectPtr memLoc) {
  memLocs.push_back(memLoc);
}

// Returns whether this object may/must be equal to o within the given Part p
template <bool defaultMayEq>
bool CombinedMemLocObject<defaultMayEq>::mayEqualML(MemLocObjectPtr o, PartEdgePtr pedge)
{
  boost::shared_ptr<CombinedMemLocObject> that = boost::dynamic_pointer_cast<CombinedMemLocObject>(o);
  ROSE_ASSERT(that);
  
  // If the two combination objects include different numbers of MemLocObjects, say that they may be equal since 
  // we can't be sure either way.
  if(memLocs.size() != that->memLocs.size()) return true;
  
  // Compare all the pairs of MemLocObjects in memLocs and that.memLocs, returning defaultMayEq if any pair
  // returns defaultMayEq since we're looking for the tightest (if defaultMayEq=false) / loosest (if defaultMayEq=true) 
  // answer that any MemLocObject in memLocs can give
  for(list<MemLocObjectPtr>::iterator thisIt=memLocs.begin(), thatIt=that->memLocs.begin();
      thisIt!=memLocs.end();
      thisIt++, thatIt++)
  {
    if((*thisIt)->mayEqual(*thatIt, pedge) == defaultMayEq) return defaultMayEq;
  }
  
  return !defaultMayEq;
}

template <bool defaultMayEq>
bool CombinedMemLocObject<defaultMayEq>::mustEqualML(MemLocObjectPtr o, PartEdgePtr pedge)
{
  boost::shared_ptr<CombinedMemLocObject> that = boost::dynamic_pointer_cast<CombinedMemLocObject>(o);
  ROSE_ASSERT(that);
  
  // If the two combination  objects include different numbers of MemLocObjects, say that they are not must equal since 
  // we can't be sure either way.
  if(memLocs.size() != that->memLocs.size()) return false;
  
  // Compare all the pairs of MemLocObjects in memLocs and that.memLocs, returning !defaultMayEq if any pair
  // returns !defaultMayEqual since we're looking for the tightest answer that any MemLocObject in memLocs can give
  for(list<MemLocObjectPtr>::iterator thisIt=memLocs.begin(), thatIt=that->memLocs.begin();
      thisIt!=memLocs.end();
      thisIt++, thatIt++)
  {
    if((*thisIt)->mustEqual(*thatIt, pedge) == !defaultMayEq) return !defaultMayEq;
  }
  
  return defaultMayEq;
}

// Returns true if this object is live at the given part and false otherwise
template <bool defaultMayEq>
bool CombinedMemLocObject<defaultMayEq>::isLive(PartEdgePtr pedge) const
{
  // If this is a union type (defaultMayEq=true), an object is live if any of its components are live (weakest constraint)
  // If this is an intersection type (defaultMayEq=false), an object is dead if any of its components are dead (strongest constraint)
  for(list<MemLocObjectPtr>::const_iterator ml=memLocs.begin(); ml!=memLocs.end(); ml++)
    if((*ml)->isLive(pedge) == defaultMayEq) return defaultMayEq;
  
  return !defaultMayEq;
}

// Allocates a copy of this object and returns a pointer to it
template <bool defaultMayEq>
MemLocObjectPtr CombinedMemLocObject<defaultMayEq>::copyML() const
{ return boost::make_shared<CombinedMemLocObject>(memLocs); }

template <bool defaultMayEq>
std::string CombinedMemLocObject<defaultMayEq>::str(std::string indent)
{
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionMLO" : "IntersectMLO")<<": ";
  if(memLocs.size()>1) oss << endl;
  for(list<MemLocObjectPtr>::iterator ml=memLocs.begin(); ml!=memLocs.end(); ) {
    if(ml!=memLocs.begin()) oss << indent << "&nbsp;&nbsp;&nbsp;&nbsp;";
    oss << (*ml)->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    ml++;
    if(ml!=memLocs.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}

// Create a function that uses examples of combined objects to force the compiler to generate these classes
static void exampleCombinedMemLocObjects2(MemLocObjectPtr ml, std::list<MemLocObjectPtr> mls, IntersectMemLocObject& i, UnionMemLocObject& u, IntersectMemLocObject& i2, UnionMemLocObject& u2);
static void exampleCombinedMemLocObjects(MemLocObjectPtr ml, std::list<MemLocObjectPtr> mls)
{
  IntersectMemLocObject exampleIntersectObject(ml);
  UnionMemLocObject     exampleUnionObject(ml);
  IntersectMemLocObject exampleIntersectObject2(mls);
  UnionMemLocObject     exampleUnionObject2(mls);
  boost::shared_ptr<UnionMemLocObject> r1 = UnionMemLocObject::create(ml);
  boost::shared_ptr<UnionMemLocObject> r2 = UnionMemLocObject::create(mls);
  boost::shared_ptr<IntersectMemLocObject> r3 = IntersectMemLocObject::create(ml);
  boost::shared_ptr<IntersectMemLocObject> r4 = IntersectMemLocObject::create(mls);
  exampleCombinedMemLocObjects2(ml, mls, exampleIntersectObject, exampleUnionObject, exampleIntersectObject2, exampleUnionObject2);
}
static void exampleCombinedMemLocObjects2(MemLocObjectPtr ml, std::list<MemLocObjectPtr> mls, IntersectMemLocObject& i, UnionMemLocObject& u, IntersectMemLocObject& i2, UnionMemLocObject& u2)
{
  exampleCombinedMemLocObjects(ml, mls);
}

/* #########################
   ##### CodeLocObject ##### 
   ######################### */

CodeLocObjectPtr NULLCodeLocObject;

/* ################################
   ##### CodeLocObjectPtrPair ##### 
   ################################ */


// Returns whether this object may/must be equal to o within the given Part p
bool CodeLocObjectPtrPair::mayEqual(CodeLocObjectPtrPair that, PartEdgePtr pedge)
{
  // Both this and that have the same structure
  assert((expr && that.expr) || (!mem && !that.mem));
  // At least one of expr or mem have to be non-NULL
  assert(expr || mem);
  return (expr ? expr->mayEqual(that.expr, pedge): false) ||
         (mem  ? mem->mayEqual (that.mem, pedge): false); 
}

bool CodeLocObjectPtrPair::mustEqual(CodeLocObjectPtrPair that, PartEdgePtr pedge)
{
  // Both this and that have the same structure
  assert((expr && that.expr) || (!mem && !that.mem));
  // At least one of expr or mem have to be non-NULL
  assert(expr || mem);
  return (expr ? expr->mustEqual(that.expr, pedge): true) &&
         (mem  ? mem->mustEqual (that.mem, pedge): true);
}

// Returns a copy of this object
CodeLocObjectPtrPair CodeLocObjectPtrPair::copyCL() const {
  return CodeLocObjectPtrPair(*this);
}

// pretty print for the object
std::string CodeLocObjectPtrPair::str(std::string indent)
{ 
  ostringstream oss;
  oss << "[";
  if(expr) oss << "expr=" << expr->str(indent) << endl;
  if(mem) {
    if(expr) oss << indent;
    oss << "mem="  << mem->str(indent);
  }
  oss << "]";
  return oss.str();
}

std::string CodeLocObjectPtrPair::strp(PartEdgePtr pedge, std::string indent)
{ 
  ostringstream oss;
  oss << "[";
  if(expr) oss << "expr=" << expr->strp(pedge, indent) << endl;
  if(mem) {
    if(expr) oss << indent;
    oss << "mem="  << mem->strp(pedge, indent);
  }
  oss << "]";
  return oss.str();
}

/* #################################
   ##### CombinedCodeLocObject ##### 
   ################################# */

template <bool defaultMayEq>
CombinedCodeLocObject<defaultMayEq>::CombinedCodeLocObject(CodeLocObjectPtr codeLoc): CodeLocObject(NULL) {
  codeLocs.push_back(codeLoc);
}

template <bool defaultMayEq>
CombinedCodeLocObject<defaultMayEq>::CombinedCodeLocObject(const list<CodeLocObjectPtr>& codeLocs) : CodeLocObject(NULL), codeLocs(codeLocs) {}

template <bool defaultMayEq>
void CombinedCodeLocObject<defaultMayEq>::add(CodeLocObjectPtr codeLoc) {
  codeLocs.push_back(codeLoc);
}

// Returns whether this object may/must be equal to o within the given Part p
// These methods are private to prevent analyses from calling them directly.
template <bool defaultMayEq>
bool CombinedCodeLocObject<defaultMayEq>::mayEqualCL(CodeLocObjectPtr o, PartEdgePtr pedge)
{
  boost::shared_ptr<CombinedCodeLocObject> that = boost::dynamic_pointer_cast<CombinedCodeLocObject>(o);
  // If the two combination objects include different numbers of CodeLocObjects, say that they may be equal since 
  // we can't be sure either way.
  if(codeLocs.size() != that->codeLocs.size()) return true;
  
  // Compare all the pairs of CodeLocObjects in codeLocs and that.codeLocs, returning defaultMayEq if any pair
  // returns defaultMayEq since we're looking for the tightest (if defaultMayEq=false) / loosest (if defaultMayEq=true) 
  // answer that any CodeLocObject in codeLocs can give
  for(list<CodeLocObjectPtr>::iterator thisIt=codeLocs.begin(), thatIt=that->codeLocs.begin();
      thisIt!=codeLocs.end();
      thisIt++, thatIt++)
  {
    if((*thisIt)->mayEqual(*thatIt, pedge) == defaultMayEq) return defaultMayEq;
  }
  
  return !defaultMayEq;
}

template <bool defaultMayEq>
bool CombinedCodeLocObject<defaultMayEq>::mustEqualCL(CodeLocObjectPtr o, PartEdgePtr pedge)
{
  boost::shared_ptr<CombinedCodeLocObject> that = boost::dynamic_pointer_cast<CombinedCodeLocObject>(o);
  // If the two combination  objects include different numbers of CodeLocObjects, say that they are not must equal since 
  // we can't be sure either way.
  if(codeLocs.size() != that->codeLocs.size()) return false;
  
  // Compare all the pairs of CodeLocObjects in codeLocs and that.codeLocs, returning !defaultMayEq if any pair
  // returns !defaultMayEqual since we're looking for the tightest answer that any CodeLocObject in codeLocs can give
  for(list<CodeLocObjectPtr>::iterator thisIt=codeLocs.begin(), thatIt=that->codeLocs.begin();
      thisIt!=codeLocs.end();
      thisIt++, thatIt++)
  {
    if((*thisIt)->mustEqual(*thatIt, pedge) == !defaultMayEq) return !defaultMayEq;
  }
  
  return defaultMayEq;
}

// Returns true if this object is live at the given part and false otherwise
template <bool defaultMayEq>
bool CombinedCodeLocObject<defaultMayEq>::isLive(PartEdgePtr pedge) const
{
  // If this is a union type (defaultMayEq=true), an object is live if any of its components are live (weakest constraint)
  // If this is an intersection type (defaultMayEq=false), an object is dead if any of its components are dead (strongest constraint)
  for(list<CodeLocObjectPtr>::const_iterator cl=codeLocs.begin(); cl!=codeLocs.end(); cl++)
    if((*cl)->isLive(pedge) == defaultMayEq) return defaultMayEq;
  
  return !defaultMayEq;
}

// Allocates a copy of this object and returns a pointer to it
template <bool defaultMayEq>
CodeLocObjectPtr CombinedCodeLocObject<defaultMayEq>::copyCL() const
{ return boost::make_shared<CombinedCodeLocObject>(codeLocs); }

template <bool defaultMayEq>
std::string CombinedCodeLocObject<defaultMayEq>::str(std::string indent)
{
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionCLO" : "IntersectCLO")<<": ";
  if(codeLocs.size()>1) oss << endl;
  for(list<CodeLocObjectPtr>::iterator cl=codeLocs.begin(); cl!=codeLocs.end(); ) {
    if(cl!=codeLocs.begin()) oss << indent << "&nbsp;&nbsp;&nbsp;&nbsp;";
    oss << (*cl)->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    cl++;
    if(cl!=codeLocs.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}

// Create a function that uses examples of combined objects to force the compiler to generate these classes
static void exampleCombinedCodeLocObjects2(CodeLocObjectPtr cl, std::list<CodeLocObjectPtr> cls, IntersectCodeLocObject& i, UnionCodeLocObject& u, IntersectCodeLocObject& i2, UnionCodeLocObject& u2);
static void exampleCombinedCodeLocObjects(CodeLocObjectPtr cl, std::list<CodeLocObjectPtr> cls)
{
  IntersectCodeLocObject exampleIntersectObject(cl);
  UnionCodeLocObject     exampleUnionObject(cl);
  IntersectCodeLocObject exampleIntersectObject2(cls);
  UnionCodeLocObject     exampleUnionObject2(cls);
  exampleCombinedCodeLocObjects2(cl, cls, exampleIntersectObject, exampleUnionObject, exampleIntersectObject2, exampleUnionObject2);
}
static void exampleCombinedCodeLocObjects2(CodeLocObjectPtr cl, std::list<CodeLocObjectPtr> cls, IntersectCodeLocObject& i, UnionCodeLocObject& u, IntersectCodeLocObject& i2, UnionCodeLocObject& u2)
{
  exampleCombinedCodeLocObjects(cl, cls);
}

/* #######################
   ##### ValueObject ##### 
   ####################### */

ValueObjectPtr NULLValueObject;

bool ValueObject::mayEqual(AbstractObjectPtr o, PartEdgePtr pedge)
{
  if(AbstractObject::mustEqual(boost::static_pointer_cast<AbstractObject>(o), pedge)) return true;
  
  ValueObjectPtr vo = boost::dynamic_pointer_cast<ValueObject>(o);
  if(vo) return mayEqual(vo, pedge);
  else   return false;
}

bool ValueObject::mustEqual(AbstractObjectPtr o, PartEdgePtr pedge)
{
  if(AbstractObject::mustEqual(boost::static_pointer_cast<AbstractObject>(o), pedge)) return true;
  
  ValueObjectPtr vo = boost::dynamic_pointer_cast<ValueObject>(o);
  if(vo) return mustEqual(vo, pedge);
  else   return false;
}

// Returns true if this SgValueExp is convertible into a boolean
bool ValueObject::isValueBoolCompatible(boost::shared_ptr<SgValueExp> val)
{
  return isSgCharVal(val.get())                ||
         isSgBoolValExp(val.get())             ||
         isSgEnumVal(val.get())                ||
         isSgIntVal(val.get())                 ||
         isSgLongIntVal(val.get())             ||
         isSgLongLongIntVal(val.get())         ||
         isSgShortVal(val.get())               ||
         isSgUnsignedCharVal(val.get())        ||
         isSgUnsignedLongVal(val.get())        ||
         isSgUnsignedLongLongIntVal(val.get()) ||
         isSgUnsignedShortVal(val.get())       ||
         isSgWcharVal(val.get());
}

// Convert the value of the given SgValueExp, cast to a boolean
bool ValueObject::SgValue2Bool(boost::shared_ptr<SgValueExp> val)
{
       if(isSgCharVal(val.get()))                return isSgCharVal(val.get())->get_value();
  else if(isSgBoolValExp(val.get()))             return isSgBoolValExp(val.get())->get_value();
  else if(isSgEnumVal(val.get()))                return isSgEnumVal(val.get())->get_value();
  else if(isSgIntVal(val.get()))                 return isSgIntVal(val.get())->get_value();
  else if(isSgLongIntVal(val.get()))             return isSgLongIntVal(val.get())->get_value();
  else if(isSgLongLongIntVal(val.get()))         return isSgLongLongIntVal(val.get())->get_value();
  else if(isSgShortVal(val.get()))               return isSgShortVal(val.get())->get_value();
  else if(isSgUnsignedCharVal(val.get()))        return isSgUnsignedCharVal(val.get())->get_value();
  else if(isSgUnsignedLongVal(val.get()))        return isSgUnsignedLongVal(val.get())->get_value();
  else if(isSgUnsignedLongLongIntVal(val.get())) return isSgUnsignedLongLongIntVal(val.get())->get_value();
  else if(isSgUnsignedShortVal(val.get()))       return isSgUnsignedShortVal(val.get())->get_value();
  else if(isSgWcharVal(val.get()))               return isSgWcharVal(val.get())->get_valueUL();
  else {
    Dbg::dbg << "val="<<cfgUtils::SgNode2Str(val.get())<<endl;
    ROSE_ASSERT(0);
  }
}

// Returns true if this object is live at the given part and false otherwise.
// Values are always live.
bool ValueObject::isLive(PartEdgePtr pedge) const
{ return true; }

// Allocates a copy of this object and returns a pointer to it
AbstractObjectPtr ValueObject::copyAO() const
{ return copyV(); }

// Returns true if the two SgValueExps correspond to the same value when cast to the given type (if t!=NULL)
bool ValueObject::equalValueExp(SgValueExp* e1, SgValueExp* e2, SgType* t)
{
  // Currently not handling type conversions
  ROSE_ASSERT(t==NULL);
  
  if(e1->variantT() != e2->variantT()) return false;
  
  if(isSgBoolValExp(e1))             { return isSgBoolValExp(e1)->get_value()             == isSgBoolValExp(e2)->get_value(); }
  if(isSgCharVal(e1))                { return isSgCharVal(e1)->get_value()                == isSgCharVal(e2)->get_value(); }
  if(isSgComplexVal(e1))             { return equalValueExp(isSgComplexVal(e1)->get_real_value(),      isSgComplexVal(e2)->get_real_value()) &&
                                              equalValueExp(isSgComplexVal(e1)->get_imaginary_value(), isSgComplexVal(e2)->get_imaginary_value()); }
  if(isSgDoubleVal(e1))              { return isSgDoubleVal(e1)->get_value()              == isSgDoubleVal(e2)->get_value(); }
  if(isSgEnumVal(e1))                { return isSgEnumVal(e1)->get_value()                == isSgEnumVal(e2)->get_value(); }
  if(isSgFloatVal(e1))               { return isSgFloatVal(e1)->get_value()               == isSgFloatVal(e2)->get_value(); }
  if(isSgIntVal(e1))                 { return isSgIntVal(e1)->get_value()                 == isSgIntVal(e2)->get_value(); }
  if(isSgLongDoubleVal(e1))          { return isSgLongDoubleVal(e1)->get_value()          == isSgLongDoubleVal(e2)->get_value(); }
  if(isSgLongIntVal(e1))             { return isSgLongIntVal(e1)->get_value()             == isSgLongIntVal(e2)->get_value(); }
  if(isSgLongLongIntVal(e1))         { return isSgLongLongIntVal(e1)->get_value()         == isSgLongLongIntVal(e2)->get_value(); }
  if(isSgShortVal(e1))               { return isSgShortVal(e1)->get_value()               == isSgShortVal(e2)->get_value(); }
  if(isSgStringVal(e1))              { return isSgStringVal(e1)->get_value()              == isSgStringVal(e2)->get_value(); }
  if(isSgWcharVal(e1))               { return isSgWcharVal(e1)->get_value()               == isSgWcharVal(e2)->get_value(); }
  if(isSgUnsignedCharVal(e1))        { return isSgUnsignedCharVal(e1)->get_value()        == isSgUnsignedCharVal(e2)->get_value(); }
  if(isSgUnsignedIntVal(e1))         { return isSgUnsignedIntVal(e1)->get_value()         == isSgUnsignedIntVal(e2)->get_value(); }
  if(isSgUnsignedLongLongIntVal(e1)) { return isSgUnsignedLongLongIntVal(e1)->get_value() == isSgUnsignedLongLongIntVal(e2)->get_value(); }
  if(isSgUnsignedLongVal(e1))        { return isSgUnsignedLongVal(e1)->get_value()        == isSgUnsignedLongVal(e2)->get_value(); }
  if(isSgUnsignedShortVal(e1))       { return isSgWcharVal(e1)->get_value()               == isSgUnsignedShortVal(e2)->get_value(); }
  if(isSgUpcMythread(e1))            { return isSgUpcMythread(e1)->get_value()            == isSgUpcMythread(e2)->get_value(); }
  if(isSgUpcThreads(e1))             { return isSgUpcThreads(e1)->get_value()             == isSgUpcThreads(e2)->get_value(); }
  
  ROSE_ASSERT(0);
}

/* ################################
   ##### CombinedValueObject ##### 
   ################################ */

template <bool defaultMayEq>
CombinedValueObject<defaultMayEq>::CombinedValueObject(ValueObjectPtr val) : ValueObject(NULL) {
  vals.push_back(val);
}

template <bool defaultMayEq>
CombinedValueObject<defaultMayEq>::CombinedValueObject(const list<ValueObjectPtr>& vals) : ValueObject(NULL), vals(vals) {}

template <bool defaultMayEq>
void CombinedValueObject<defaultMayEq>::add(ValueObjectPtr val) {
  vals.push_back(val);
}

// Returns whether this object may/must be equal to o within the given Part p
// These methods are private to prevent analyses from calling them directly.
template <bool defaultMayEq>
bool CombinedValueObject<defaultMayEq>::mayEqual(ValueObjectPtr o, PartEdgePtr pedge)
{
  boost::shared_ptr<CombinedValueObject> that = boost::dynamic_pointer_cast<CombinedValueObject>(o);
  // If the two combination objects include different numbers of ValueObjects, say that they may be equal since 
  // we can't be sure either way.
  if(vals.size() != that->vals.size()) return true;
  
  // Compare all the pairs of ValueObjects in vals and that.vals, returning defaultMayEq if any pair
  // returns defaultMayEq since we're looking for the tightest (if defaultMayEq=false) / loosest (if defaultMayEq=true) 
  // answer that any ValueObject in vals can give
  for(list<ValueObjectPtr>::iterator thisIt=vals.begin(), thatIt=that->vals.begin();
      thisIt!=vals.end();
      thisIt++, thatIt++)
  {
    if((*thisIt)->mayEqual(*thatIt, pedge) == defaultMayEq) return defaultMayEq;
  }
  
  return !defaultMayEq;
}

template <bool defaultMayEq>
bool CombinedValueObject<defaultMayEq>::mustEqual(ValueObjectPtr o, PartEdgePtr pedge)
{
  boost::shared_ptr<CombinedValueObject> that = boost::dynamic_pointer_cast<CombinedValueObject>(o);
  // If the two combination  objects include different numbers of ValueObjects, say that they are not must equal since 
  // we can't be sure either way.
  if(vals.size() != that->vals.size()) return false;
  
  // Compare all the pairs of ValueObjects in vals and that.vals, returning !defaultMayEq if any pair
  // returns !defaultMayEqual since we're looking for the tightest answer that any ValueObject in vals can give
  for(list<ValueObjectPtr>::iterator thisIt=vals.begin(), thatIt=that->vals.begin();
      thisIt!=vals.end();
      thisIt++, thatIt++)
  {
    if((*thisIt)->mustEqual(*thatIt, pedge) == !defaultMayEq) return !defaultMayEq;
  }
  
  return defaultMayEq;
}

// Returns true if this ValueObject corresponds to a concrete value that is statically-known
template <bool defaultMayEq>
bool CombinedValueObject<defaultMayEq>::isConcrete()
{
  // The combined object is concrete if 
  // intersect (defaultMayEq=false) : any sub-value is concrete
  // union (defaultMayEq=true) : all the sub-values are concrete and have the same type and value
  
  // Intersection
  if(defaultMayEq==false) {
    for(list<ValueObjectPtr>::iterator v=vals.begin(); v!=vals.end(); v++) {
      if((*v)->isConcrete()) return true;
    }
    return false;
  // Union
  } else {
    list<ValueObjectPtr>::iterator firstI = vals.begin();
    ValueObjectPtr first = *firstI;
    
    // The union is not concrete if 
    for(list<ValueObjectPtr>::iterator v=vals.begin(); v!=vals.end(); v++) {
      /*Dbg::dbg << "CombinedValueObject<defaultMayEq>::isConcrete(), v="<<(*v)->str()<<" isConcrete="<<(*v)->isConcrete()<<endl;
      if((*v)->isConcrete()) {
        Dbg::dbg << "my type="<<(*v)->getConcreteType().get()->variantT()<<"="<<cfgUtils::SgNode2Str((*v)->getConcreteType().get())<<" begin type="<<(*vals.begin())->getConcreteType().get()->variantT()<<"="<<cfgUtils::SgNode2Str((*vals.begin())->getConcreteType().get())<<endl;
        if((*v)->getConcreteType().get()->variantT() == (*vals.begin())->getConcreteType().get()->variantT())
          Dbg::dbg << " ValueObject::equalValueExp((*v)->getConcreteValue().get(), (*vals.begin())->getConcreteValue().get())="<<ValueObject::equalValueExp((*v)->getConcreteValue().get(), (*vals.begin())->getConcreteValue().get())<<endl;
      }*/
      //Dbg::dbg << "v="<<(*v)->str()<<endl;
      //Dbg::dbg << "(*v)->getConcreteType()->variantT()="<<t->variantT()<<endl;
      //Dbg::dbg << "first->getConcreteType()->variantT()="<<first->getConcreteType()->variantT()<<endl;
      // Any sub-value is not concrete, OR
      if(!(*v)->isConcrete() || 
         // Any pair of sub-values have different types, OR
         (*v)->getConcreteType()->variantT() != first->getConcreteType()->variantT() ||
         // Any pair of sub-values have different values
         !ValueObject::equalValueExp((*v)->getConcreteValue().get(), first->getConcreteValue().get())) {
        return false;
      }
    }
    return true;
  }
}

// Returns the type of the concrete value (if there is one)
template <bool defaultMayEq>
SgType* CombinedValueObject<defaultMayEq>::getConcreteType()
{
  ROSE_ASSERT(isConcrete());
  
  return (*vals.begin())->getConcreteType();
}

// Returns the concrete value (if there is one) as an SgValueExp, which allows callers to use
// the normal ROSE mechanisms to decode it
template <bool defaultMayEq>
boost::shared_ptr<SgValueExp> CombinedValueObject<defaultMayEq>::getConcreteValue()
{
  ROSE_ASSERT(isConcrete());
  
  return (*vals.begin())->getConcreteValue();
}

// Allocates a copy of this object and returns a pointer to it
template <bool defaultMayEq>
ValueObjectPtr CombinedValueObject<defaultMayEq>::copyV() const
{ return boost::make_shared<CombinedValueObject>(vals); }

template <bool defaultMayEq>
std::string CombinedValueObject<defaultMayEq>::str(std::string indent)
{
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionVO" : "IntersectVO")<<": ";
  if(vals.size()>1) oss << endl;
  for(list<ValueObjectPtr>::iterator v=vals.begin(); v!=vals.end(); ) {
    if(v!=vals.begin()) oss << indent << "&nbsp;&nbsp;&nbsp;&nbsp;";
    oss << (*v)->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    v++;
    if(v!=vals.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}

// Create a function that uses examples of combined objects to force the compiler to generate these classes
static void exampleCombinedValueObjects2(ValueObjectPtr val, std::list<ValueObjectPtr> vals, IntersectValueObject& i, UnionValueObject& u, IntersectValueObject& i2, UnionValueObject& u2);
static void exampleCombinedValueObjects(ValueObjectPtr val, std::list<ValueObjectPtr> vals)
{
  IntersectValueObject exampleIntersectObject(val);
  UnionValueObject     exampleUnionObject(val);
  IntersectValueObject exampleIntersectObject2(vals);
  UnionValueObject     exampleUnionObject2(vals);
  exampleCombinedValueObjects2(val, vals, exampleIntersectObject, exampleUnionObject, exampleIntersectObject2, exampleUnionObject2);
}
static void exampleCombinedValueObjects2(ValueObjectPtr val, std::list<ValueObjectPtr> vals, IntersectValueObject& i, UnionValueObject& u, IntersectValueObject& i2, UnionValueObject& u2)
{
  exampleCombinedValueObjects(val, vals);
}
/* ############################
   ##### LabeledAggregate ##### 
   ############################ */

std::list<boost::shared_ptr<LabeledAggregateField> > LabeledAggregate::getElements(PartEdgePtr pedge) const 
{
  std::list<boost::shared_ptr<LabeledAggregateField> > rt;
  cerr<<"Error. Direct call to base class's getElements() is not allowed."<<endl;
  ROSE_ASSERT (false);
  return rt;
}

size_t LabeledAggregate::fieldCount(PartEdgePtr pedge)
{
  cerr<<"Error. Direct call to base class's filedCount() is not allowed."<<endl;
  ROSE_ASSERT (false);
  return 0;
}

/*
  bool LabeledAggregate:: operator == (const LabeledAggregate& o2) const
  {
    cerr<<"Error. Direct call to base class's operator == () is not allowed."<<endl;
    ROSE_ASSERT (false);
    return false;  
  }

  bool LabeledAggregate::operator < ( const LabeledAggregate& o2) const
  {
    cerr<<"Error. Direct call to base class's operator <() is not allowed."<<endl;
    ROSE_ASSERT (false);
    return false;  
  }
*/ 

/* #################################
   ##### LabeledAggregateField ##### 
   ################################# */

std::string LabeledAggregateField::getName(PartEdgePtr pedge)
{
 cerr<<"Error. Direct call to base class's getName() is not allowed."<<endl;
 ROSE_ASSERT (false);
 return "";  
}

MemLocObjectPtr LabeledAggregateField::getField(PartEdgePtr pedge)
{

 cerr<<"Error. Direct call to base class's getField() is not allowed."<<endl;
 ROSE_ASSERT (false);
 MemLocObjectPtr p;
 return p;
}
size_t LabeledAggregateField::getIndex(PartEdgePtr pedge)
{

  cerr<<"Error. Direct call to base class's getIndex() is not allowed."<<endl;
  ROSE_ASSERT (false);
  return 0;
}

//std::string LabeledAggregateField::str(const string& indent)
// pretty print for the object
std::string LabeledAggregateField::str(std::string indent)
{
  
 cerr<<"Error. Direct call to base class's str() is not allowed."<<endl;
 ROSE_ASSERT (false);
 return "";  
}

/*void LabeledAggregateField::setField(MemLocObjectPtr f)
{
  cerr<<"Error. Direct call to base class's setField() is not allowed."<<endl;
  ROSE_ASSERT (false);
}*/

/*void LabeledAggregateField::setParent(boost::shared_ptr<LabeledAggregate> p)
{
 cerr<<"Error. Direct call to base class's setParent() is not allowed."<<endl;
 ROSE_ASSERT (false);
}*/

boost::shared_ptr<LabeledAggregate> LabeledAggregateField::getParent(PartEdgePtr pedge)
{
  cerr<<"Error. Direct call to base class's getParent() is not allowed."<<endl;
  ROSE_ASSERT (false);
  boost::shared_ptr<LabeledAggregate> p;
  return p;
}

/* #######################
   ##### IndexVector ##### 
   ####################### */

//std::string IndexVector::str(const string& indent)
// pretty print for the object
std::string IndexVector::str(std::string indent)
{
 cerr<<"Error. Direct call to base class (IndexVector)'s str() is not allowed."<<endl;
 ROSE_ASSERT (false);
 return "";  
}
bool IndexVector::mayEqual (IndexVectorPtr other, PartEdgePtr pedge)
{
 cerr<<"Error. Direct call to base class (IndexVector)'s mayEqual() is not allowed."<<endl;
 ROSE_ASSERT (false);
 return false;  
}
bool IndexVector::mustEqual (IndexVectorPtr other, PartEdgePtr pedge)
{
 cerr<<"Error. Direct call to base class (IndexVector)'s mustEqual is not allowed."<<endl;
 ROSE_ASSERT (false);
 return false;  
}

/* #################
   ##### Array ##### 
   ################# */

MemLocObjectPtr Array::getElements(PartEdgePtr pedge)
{
  cerr<<"Error. Direct call to base class (Array)'s getElements() is not allowed."<<endl;
  ROSE_ASSERT (false);
  MemLocObjectPtr p;
  return p;  
}

MemLocObjectPtr Array::getElements(IndexVectorPtr ai, PartEdgePtr pedge)
{
  cerr<<"Error. Direct call to base class (Array)'s getElements(IndexVector* ai) is not allowed."<<endl;
  ROSE_ASSERT (false);
  MemLocObjectPtr p;
  return p;
}

size_t Array::getNumDims(PartEdgePtr pedge)
{
  cerr<<"Error. Direct call to base class (Array)'s getNumDims( ) is not allowed."<<endl;
  ROSE_ASSERT (false);
  return 0;
}

MemLocObjectPtr Array::getDereference(PartEdgePtr pedge)
{
  cerr<<"Error. Direct call to base class (Array)'s getDereference( ) is not allowed."<<endl;
  ROSE_ASSERT (false);
  MemLocObjectPtr p;
  return p;
}
/*
  bool Array:: operator == (const MemLocObject & o2) const
  {
    cerr<<"Error. Direct call to base class (Array)'s operator == () is not allowed."<<endl;
    ROSE_ASSERT (false);
    return false;  
  }

  bool Array:: operator  (const MemLocObject & o2) const
  {
    cerr<<"Error. Direct call to base class (Array)'s operator == () is not allowed."<<endl;
    ROSE_ASSERT (false);
    return false;  
  }
*/

/* ###################
   ##### Pointer ##### 
   ################### */

MemLocObjectPtr Pointer::getDereference(PartEdgePtr pedge)
{
  cerr<<"Error. Direct call to base class (Pointer)'s getDereference( ) is not allowed."<<endl;
  ROSE_ASSERT (false);
  MemLocObjectPtr p;
  return p;
}

/* ##################################################
   ##### Specific Types of CombinedMemLocObject ##### 
   ################################################## */

/* ##########################
   ##### CombinedScalar #####
   ########################## */

template <bool defaultMayEq>
CombinedScalar<defaultMayEq>::CombinedScalar(const std::list<MemLocObjectPtr>& memLocs) : 
    MemLocObject(NULL), CombinedMemLocObject<defaultMayEq>(memLocs), Scalar(NULL) {}

// pretty print for the object
template <bool defaultMayEq>
std::string CombinedScalar<defaultMayEq>::str(std::string indent) {
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionScal" : "IntersectScal")<<": ";
  for(list<MemLocObjectPtr>::iterator ml=CombinedMemLocObject<defaultMayEq>::memLocs.begin(); ml!=CombinedMemLocObject<defaultMayEq>::memLocs.end(); ) {
    oss << (*ml)->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    ml++;
    if(ml!=CombinedMemLocObject<defaultMayEq>::memLocs.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}

template <bool defaultMayEq>
std::string CombinedScalar<defaultMayEq>::strp(PartEdgePtr pedge, std::string indent) {
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionScal" : "IntersectScal")<<": ";
  for(list<MemLocObjectPtr>::iterator ml=CombinedMemLocObject<defaultMayEq>::memLocs.begin(); ml!=CombinedMemLocObject<defaultMayEq>::memLocs.end(); ) {
    oss << (*ml)->strp(pedge, indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    ml++;
    if(ml!=CombinedMemLocObject<defaultMayEq>::memLocs.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}

/* ################################## 
   ##### CombinedFunctionMemLoc ##### 
   ################################## */

template <bool defaultMayEq>
CombinedFunctionMemLoc<defaultMayEq>::CombinedFunctionMemLoc(const std::list<MemLocObjectPtr>& memLocs) : 
    MemLocObject(NULL), CombinedMemLocObject<defaultMayEq>(memLocs), FunctionMemLoc(NULL) {}

// pretty print for the object
template <bool defaultMayEq>
std::string CombinedFunctionMemLoc<defaultMayEq>::str(std::string indent) {
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionFML" : "IntersectFML")<<": ";
  for(list<MemLocObjectPtr>::iterator ml=CombinedMemLocObject<defaultMayEq>::memLocs.begin(); ml!=CombinedMemLocObject<defaultMayEq>::memLocs.end(); ) {
    oss << (*ml)->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    ml++;
    if(ml!=CombinedMemLocObject<defaultMayEq>::memLocs.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}

template <bool defaultMayEq>
std::string CombinedFunctionMemLoc<defaultMayEq>::strp(PartEdgePtr pedge, std::string indent) {
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionFML" : "IntersectFML")<<": ";
  for(list<MemLocObjectPtr>::iterator ml=CombinedMemLocObject<defaultMayEq>::memLocs.begin(); ml!=CombinedMemLocObject<defaultMayEq>::memLocs.end(); ) {
    oss << (*ml)->strp(pedge, indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    ml++;
    if(ml!=CombinedMemLocObject<defaultMayEq>::memLocs.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}


/* #########################################
   ##### CombinedLabeledAggregateField #####
   ######################################### */
/*
template <bool defaultMayEq>
CombinedLabeledAggregateField<defaultMayEq>::CombinedLabeledAggregateField(string name, size_t index, 
        //boost::shared_ptr<CombinedMemLocObject<defaultMayEq> > field, 
        std::list<LabeledAggregateFieldPtr> fields,
        LabeledAggregatePtr parent)
  : name(name), index(index), fields(fields), parent(parent)
{}

// Creates a new CombinedLabeledAggregateField when called by CombinedLabeledAggregate::create(). It is assumed that all the 
// sub-objects have type LabeledAggregateField. The function returns a CombinedLabeledAggregate if they also 
// have the same internal structure and a NULL shared_ptr otherwise.
template <bool defaultMayEq>
boost::shared_ptr<CombinedLabeledAggregateField<defaultMayEq> > 
  CombinedLabeledAggregateField<defaultMayEq>::create(
                                        const std::list<LabeledAggregateFieldPtr>& fields, LabeledAggregatePtr parent, 
                                        PartEdgePtr pedge)
{
  // Determine whether all sub-fields have the same name and if so, record it
  string name="";
  int numSameName=0;
  for(std::list<LabeledAggregateFieldPtr>::const_iterator f=fields.begin(); f!=fields.end(); f++) {
    if(numSameName==0) name = (*f)->getName(); 
    else if(name!=(*f)->getName(pedge)) break;
    numSameName++;
  }
  
  // If all sub-fields have the same name
  if(numSameName == fields.size()) {
    // Determine whether all sub-fields have the same index and if so, record it
    size_t index=0;
    int numSameIndex=0;
    for(std::list<LabeledAggregateFieldPtr>::const_iterator f=fields.begin(); f!=fields.end(); f++) {
      if(numSameIndex==0) index = (*f)->getIndex(); 
      else if(index!=(*f)->getIndex()) break;
      numSameIndex++;
    }

    if(numSameIndex == fields.size()) {
      return boost::make_shared<CombinedLabeledAggregate<defaultMayEq> >(name, index, fields, str, parent);
    }
  }
  
  // If the sub-fields are not compatible, return NULL
  boost::shared_ptr<CombinedLabeledAggregateField<defaultMayEq> > N;
  return N;
}*/

template <bool defaultMayEq>
CombinedLabeledAggregateField<defaultMayEq>::CombinedLabeledAggregateField(const std::list<LabeledAggregateFieldPtr>& fields)
  : fields(fields)
{}

// field name
template <bool defaultMayEq>
std::string CombinedLabeledAggregateField<defaultMayEq>::getName(PartEdgePtr pedge) {
  string name="";
  size_t numSameName=0;
  for(std::list<LabeledAggregateFieldPtr>::const_iterator f=fields.begin(); f!=fields.end(); f++) {
    if(numSameName==0) name = (*f)->getName(pedge); 
    else if(name!=(*f)->getName(pedge)) break;
    numSameName++;
  }
  
  // All the sub-field lists must have the same structure
  ROSE_ASSERT(numSameName == fields.size());
  
  return name;
}

// The field's index within its parent object. The first field has index 0.  
template <bool defaultMayEq>
size_t CombinedLabeledAggregateField<defaultMayEq>::getIndex(PartEdgePtr pedge) { 
  // Determine whether all sub-fields have the same index and if so, return it
  size_t index=0;
  size_t numSameIndex=0;
  for(std::list<LabeledAggregateFieldPtr>::const_iterator f=fields.begin(); f!=fields.end(); f++) {
    if(numSameIndex==0) index = (*f)->getIndex(pedge); 
    else if(index!=(*f)->getIndex(pedge)) break;
    numSameIndex++;
  }

  // All the sub-field lists must have the same structure
  ROSE_ASSERT(numSameIndex == fields.size());
  
  return index;  
}

// Pointer to an abstract description of the field
template <bool defaultMayEq>
MemLocObjectPtr CombinedLabeledAggregateField<defaultMayEq>::getField(PartEdgePtr pedge) { 
  // Collect together the MemLocObjectPtrs that represent the field and turn them into a CombinedMemLocObject
  list<MemLocObjectPtr> memLocs;
  for(std::list<LabeledAggregateFieldPtr>::iterator f=fields.begin(); f!=fields.end(); f++)
    memLocs.push_back((*f)->getField(pedge));
  
  return CombinedMemLocObject<defaultMayEq>::create(memLocs);
}

// the parent obj this field belongs to
template <bool defaultMayEq>
LabeledAggregatePtr CombinedLabeledAggregateField<defaultMayEq>::getParent(PartEdgePtr pedge) 
{ 
  // Collect together the MemLocObjectPtrs that represent the field and turn them into a CombinedMemLocObject
  list<MemLocObjectPtr> memLocs;
  for(std::list<LabeledAggregateFieldPtr>::iterator f=fields.begin(); f!=fields.end(); f++)
    memLocs.push_back((*f)->getParent(pedge));
  
  MemLocObjectPtr parent = CombinedMemLocObject<defaultMayEq>::create(memLocs);
  ROSE_ASSERT(parent->isLabeledAggregate());
  return parent->isLabeledAggregate();
}

// pretty print for the object
template <bool defaultMayEq>
std::string CombinedLabeledAggregateField<defaultMayEq>::str(std::string indent) {
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionLAF" : "IntersectLAF")<<": ";
  for(std::list<LabeledAggregateFieldPtr>::iterator f=fields.begin(); f!=fields.end(); ) {
    oss << (*f)->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    f++;
    if(f!=fields.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}

template <bool defaultMayEq>
std::string CombinedLabeledAggregateField<defaultMayEq>::strp(PartEdgePtr pedge, std::string indent) {
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionLAF" : "IntersectLAF")<<": ";
  for(std::list<LabeledAggregateFieldPtr>::iterator f=fields.begin(); f!=fields.end(); ) {
    oss << (*f)->strp(pedge, indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    f++;
    if(f!=fields.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}


/* ####################################
   ##### CombinedLabeledAggregate #####
   #################################### */

/*template <bool defaultMayEq>
CombinedLabeledAggregate<defaultMayEq>::CombinedLabeledAggregate(
                           size_t combinedFieldCount, 
                           std::list<boost::shared_ptr<CombinedLabeledAggregateField<defaultMayEq> > > combinedFields,
                           string strRep) :
        combinedFieldCount(combinedFieldCount), combinedFields(combinedFields), strRep(strRep)
{ }

template <bool defaultMayEq>
boost::shared_ptr<CombinedMemLocObject<defaultMayEq> >
  CombinedLabeledAggregate<defaultMayEq>::create(const std::list<MemLocObjectPtr>& memLocs)
{
  // Determine if all the sub-labeled aggregates have the same internal structure
  
  // First check if the number of fields is the same
  
  
  // If the numver of fields is the same in all memory locations in memLocs
  if(numSameFieldCount == memLocs.size()) {
    list<boost::shared_ptr<CombinedLabeledAggregateField<defaultMayEq> > > combinedFields;
    
    // For each field of the aggregate this is a list of fields from each sub-ml from memLocs;
    vector<list<LabeledAggregateFieldPtr> > fields2ml;

    // Get the fields from each memory location in memLocs
    for(std::list<MemLocObjectPtr>::iterator ml=memLocs.begin(); ml!=memLocs.end(); ml++) {
      list<LabeledAggregateFieldPtr> curMLElements = (*ml)->isLabeledAggregate()->getElements();
      int i=0;
      for(list<LabeledAggregateFieldPtr>::iterator f=mlFields.begin(); f!=mlFields.end(); f++, i++) {
        fields2ml[i].push_back(*f);
      }
    }

    // From each of these field lists attempt to construct a CombinedLabeledAggregateField
    for(vector<list<LabeledAggregateFieldPtr> >::iterator fml=fields2ml.begin(); fml!=fields2ml.end(); fml++) {
      boost::shared_ptr<CombinedLabeledAggregateField<defaultMayEq> > comboField =
        CombinedLabeledAggregateField<defaultMayEq>::create(*fml, 
            boost::dynamic_pointer_cast<boost::shared_ptr<CombinedLabeledAggregate<defaultMayEq> > >(shared_from_this()));
      // If this CombinedLabeledAggregateField was successfully created (same field in all memory location in memLocs),
      // add it to combinedFields
      if(comboField) combinedFields.push_back(field);
      else      break;
    }
  
    // If we've successfully created CombinedLabeledAggregateFields for all the fields in all the memory locations in memLocs
    if(combinedFields.size() == fields2ml.size()) {
      // Compute this object's string representation
      ostringstream oss;
      oss << "["<<(defaultMayEq? "UnionLA" : "IntersectLA")<<": ";
      
      for(vector<list<LabeledAggregateFieldPtr> >::iterator fml=fields2ml.begin(); fml!=fields2ml.end(); ) {
        oss << fml->str();
        if(fml!=fields2ml.end()) oss << endl;
        fml++;
      }
      oss << "]";
      return boost::make_shared<CombinedLabeledAggregate<defaultMayEq> >(combinedFieldCount, combinedFields, oss.str());
    }
  }
  
  // If there is any inconsistency between the memory locations in memlocs, return a generic CombinedMemLocObject object
  return boost::make_shared<CombinedMemLocObject<defaultMayEq> >(memLocs);
}*/

template <bool defaultMayEq>
CombinedLabeledAggregate<defaultMayEq>::CombinedLabeledAggregate(const list<MemLocObjectPtr>& memLocs) : 
    MemLocObject(NULL), CombinedMemLocObject<defaultMayEq>(memLocs), LabeledAggregate(NULL)
{
  for(list<MemLocObjectPtr>::const_iterator ml=memLocs.begin(); ml!=memLocs.end(); ml++) {
    ROSE_ASSERT((*ml)->isLabeledAggregate());
    labAggrs.push_back((*ml)->isLabeledAggregate());
  }
}

template <bool defaultMayEq>
size_t CombinedLabeledAggregate<defaultMayEq>::fieldCount(PartEdgePtr pedge)
{
  // Make sure that the number of fields is the same in all sub-LabeledAggregates and record the common value
  size_t combinedFieldCount=0;
  size_t numSameFieldCount=0;
  for(list<LabeledAggregatePtr>::const_iterator la=labAggrs.begin(); la!=labAggrs.end(); la++) {
    if(combinedFieldCount==0) combinedFieldCount=(*la)->fieldCount(pedge);
    else if(combinedFieldCount != (*la)->fieldCount(pedge)) break;
    numSameFieldCount++;
  }
  
  // All the sub-aggregates must have the same structure
  ROSE_ASSERT(numSameFieldCount == labAggrs.size());
  
  return combinedFieldCount;
}

// Returns a list of field
template <bool defaultMayEq>
list<LabeledAggregateFieldPtr> CombinedLabeledAggregate<defaultMayEq>::getElements(PartEdgePtr pedge) const
{
  list<LabeledAggregateFieldPtr> combinedFields;
  // The vector contains an entry for each field of the CombinedLabeledAggregate, and this entry contains a list
  // of LabeledAggregateField objects for that field, one from each sub-aggregate in labAggrs.
  vector<list<LabeledAggregateFieldPtr> > fields2aggr;

  // Get the fields from each LabeledAggregate  in labAggrs
  for(list<LabeledAggregatePtr>::const_iterator la=labAggrs.begin(); la!=labAggrs.end(); la++) {
    list<LabeledAggregateFieldPtr> curMLElements = (*la)->getElements(pedge);
    fields2aggr.push_back(curMLElements);
    // int i=0;
    // for(list<LabeledAggregateFieldPtr>::iterator f=curMLElements.begin(); f!=curMLElements.end(); f++, i++) {
    //     std::cout << "curMLelemnts field: " << (*f)->strp(pedge) << std::endl;
    //   fields2aggr[i].push_back(*f);
    // }
  }
  // From each of these field lists attempt to construct a CombinedLabeledAggregateField
  for(vector<list<LabeledAggregateFieldPtr> >::iterator fml=fields2aggr.begin(); fml!=fields2aggr.end(); fml++) {
    /*boost::shared_ptr<CombinedLabeledAggregateField<defaultMayEq> > comboField =
      CombinedLabeledAggregateField<defaultMayEq>::create(*fml, 
          boost::dynamic_pointer_cast<boost::shared_ptr<CombinedLabeledAggregate<defaultMayEq> > >(shared_from_this()),
          pedge);
    // If this CombinedLabeledAggregateField was successfully created (same field in all memory location in memLocs),
    // add it to combinedFields
    if(comboField) combinedFields.push_back(field);
    else      break;*/
    
    combinedFields.push_back(boost::shared_ptr<LabeledAggregateField>(boost::make_shared<CombinedLabeledAggregateField<defaultMayEq> >(*fml)));
  }
  // All the fields must have the same structure
  //ROSE_ASSERT(combinedFields.size() == fields2aggr.size());
  
  return combinedFields;
}

// pretty print for the object
template <bool defaultMayEq>
std::string CombinedLabeledAggregate<defaultMayEq>::str(std::string indent)
{
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionLA" : "IntersectLA")<<"]";
  return oss.str();
}

template <bool defaultMayEq>
std::string CombinedLabeledAggregate<defaultMayEq>::strp(PartEdgePtr pedge, std::string indent)
{ 
  // Compute this object's string representation
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionLA" : "IntersectLA")<<": ";

  list<LabeledAggregateFieldPtr> elements = getElements(pedge);
  for(list<LabeledAggregateFieldPtr>::iterator e=elements.begin(); e!=elements.end(); ) {
    oss << (*e)->strp(pedge, indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    e++;
    if(e!=elements.end()) oss << endl;
  }
  oss << "]";
  return oss.str();
}

// GB 2012-10-19 - It doesn't appear that we need CombinedIndexVectors since the IndexVectors passed to getElements()
//                 are going to be CombinedValues, not CombinedIndexVectors
/* ###############################
   ##### CombinedIndexVector #####
   ############################### * /

template <bool defaultMayEq>
CombinedIndexVector<defaultMayEq>::CombinedIndexVector(const std::list<IndexVectorPtr>& ivectors, int combinedSize) : 
        ivectors(ivectors), combinedSize(combinedSize)
{}

// Creates a new CombinedIndexVector when called by createCombinedArray. It is assumed that all the 
// sub-objects have type CombinedIndexVector. The function returns a CombinedIndexVector if they also 
// have the same internal structure and NULL otherwise.
template <bool defaultMayEq>
boost::shared_ptr<CombinedIndexVector<defaultMayEq> > 
  CombinedIndexVector<defaultMayEq>::createCombinedIndexVector(const std::list<IndexVectorPtr>& ivectors)
{
  // Determine if all the sub-index vectors have the same internal structure
  
  // Check if the size of all the vectors is the same
  size_t combinedSize=0;
  int numSameSize=0;
  for(std::list<IndexVectorPtr>::const_iterator iv=ivectors.begin(); iv!=ivectors.end(); iv++) {
    ROSE_ASSERT();
    if(combinedSize==-1) combinedSize=(*iv)->getSize();
    else if(combinedSize != (*iv)->getSize()) break;
    numSameSize++;
  }

  // If all the index vectors have the same size
  if(numSameSize == ivectors.size()) {
    return boost::make_shared<CombinedIndexVector<defaultMayEq> >(ivectors, combinedSize);
  }
  
  // If the sub-index vectors are not compatible, return NULL
  boost::shared_ptr<CombinedIndexVector<defaultMayEq> > N;
  return N;
}; // namespace dataflow


// the index vector's length
template <bool defaultMayEq>
size_t CombinedIndexVector<defaultMayEq>::getSize()
{ return combinedSize; }

// pretty print for the object
template <bool defaultMayEq>
std::string CombinedIndexVector<defaultMayEq>::str(std::string indent)
{
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionCIV" : "IntersectCIV")<<": ";
  for(std::list<IndexVectorPtr>::const_iterator iv=ivectors.begin(); iv!=ivectors.end(); iv++) {
    oss << f->str();
    if(f!=fields.end()) oss << endl;
    f++;
  }
  oss << "]";
  
  return oss.str();
}

// equal operator
template <bool defaultMayEq>
bool CombinedIndexVector<defaultMayEq>::mayEqual (IndexVectorPtr other, PartEdgePtr pedge)
{
  // If the two combination objects include different numbers of sub-index vectors, say that they may be equal since 
  // we can't be sure either way.
  if(ivectors.size() != that->ivectors.size()) return true;
  
  // Compare all the pairs of IndexVEctors in ivectors and that.ivectors, returning defaultMayEq if any pair
  // returns defaultMayEq since we're looking for the tightest (if defaultMayEq=false) / loosest (if defaultMayEq=true) 
  // answer that any MemLocObject in memLocs can give
  for(std::list<IndexVectorPtr>::iterator thisIt=ivectors.begin(), thatIt=that->ivectors.begin(); 
      thisIt!=ivectors.end(); thisIt++, thatIt++)
  {
    if((*thisIt)->mayEqual(*thatIt, pedge) == defaultMayEq) return defaultMayEq;
  }
  
  return !defaultMayEq;
}

template <bool defaultMayEq>
bool CombinedIndexVector<defaultMayEq>::mustEqual (IndexVectorPtr other, PartEdgePtr pedge)
{
  // If the two combination objects include different numbers of IndexVectors, say that they are not must equal since 
  // we can't be sure either way.
  if(ivectors.size() != that->ivectors.size()) return false;
  
  // Compare all the pairs of IndexVectors in ivectors and that.ivectors, returning !defaultMayEq if any pair
  // returns !defaultMayEqual since we're looking for the tightest answer that any IndexVector in ivectors can give
  for(std::list<IndexVectorPtr>::iterator thisIt=ivectors.begin(), thatIt=that->ivectors.begin(); 
      thisIt!=ivectors.end(); thisIt++, thatIt++)
  {
    if((*thisIt)->mustEqual(*thatIt, pedge) == !defaultMayEq) return !defaultMayEq;
  }
  
  return defaultMayEq;  
}*/

/* #########################
   ##### CombinedArray #####
   ######################### */

/*template <bool defaultMayEq>
CombinedArray<defaultMayEq>::CombinedArray(size_t combinedDims, const std::list<MemLocObjectPtr>& arrays) :
    combinedDims(combinedDims), arrays(arrays)
{}

// Creates a new CombinedArray when called by createCombinedMemLocObject. It is assumed that all the 
// sub-objects have type Array. The function returns a CombinedArray if they also 
// have the same internal structure and a generic CombinedMemLocObject instance otherwise.
template <bool defaultMayEq>
boost::shared_ptr<CombinedMemLocObject<defaultMayEq> >
  CombinedArray<defaultMayEq>::create(const std::list<MemLocObjectPtr>& memLocs)
{
  // Determine if all the sub-arrays have the same internal structure
  
  // First check if the number of dimensions is the same
  size_t combinedDims=0;
  int numSameDims=0;
  for(std::list<MemLocObjectPtr>::iterator ml=memLocs.begin(); ml!=memLocs.end(); ml++) {
    ROSE_ASSERT((*ml)->isArray());
    if(combinedDims==-1) combinedDims=(*ml)->isArray()->getNumDims();
    else if(combinedDims != (*ml)->isArray()->getNumDims()) break;
    numSameDims++;
  }
  
  // If the number of fields is the same in all memory locations in memLocs
  if(numSameDims == memLocs.size())
    return boost::make_shared<CombinedArray<defaultMayEq> >(combinedDims, memLocs);
  
  // If there is any inconsistency between the memory locations in memlocs, return a generic CombinedMemLocObject object
  return boost::make_shared<CombinedMemLocObject<defaultMayEq> >(memLocs);
}*/

template <bool defaultMayEq>
CombinedArray<defaultMayEq>::CombinedArray(const list<MemLocObjectPtr>& memLocs) : 
    MemLocObject(NULL), CombinedMemLocObject<defaultMayEq>(memLocs), Array(NULL)
{
  for(list<MemLocObjectPtr>::const_iterator ml=memLocs.begin(); ml!=memLocs.end(); ml++) {
    ROSE_ASSERT((*ml)->isArray());
    arrays.push_back((*ml)->isArray());
  }
}

// Returns a memory object that corresponds to all the elements in the given array
template <bool defaultMayEq>
MemLocObjectPtr CombinedArray<defaultMayEq>::getElements(PartEdgePtr pedge)
{
  list<MemLocObjectPtr> elements;
  for(list<ArrayPtr>::iterator a=arrays.begin(); a!=arrays.end(); a++)
    elements.push_back((*a)->getElements(pedge));
  
  return CombinedMemLocObject<defaultMayEq>::create(elements);
}

// Returns the memory object that corresponds to the elements described by the given abstract index, 
// which represents one or more indexes within the array
template <bool defaultMayEq>
MemLocObjectPtr CombinedArray<defaultMayEq>::getElements(IndexVectorPtr ai, PartEdgePtr pedge)
{
  list<MemLocObjectPtr> elements;
  for(list<ArrayPtr>::iterator a=arrays.begin(); a!=arrays.end(); a++)
    elements.push_back((*a)->getElements(ai, pedge));
  
  return CombinedMemLocObject<defaultMayEq>::create(elements);
}

// number of dimensions of the array
template <bool defaultMayEq>
size_t CombinedArray<defaultMayEq>::getNumDims(PartEdgePtr pedge)
{ 
  // Make sure that the number of dimensions is the same in all sub-Arrays and record the common value
  size_t combinedDims=0;
  size_t numSameDims=0;
  for(std::list<ArrayPtr>::iterator a=arrays.begin(); a!=arrays.end(); a++) {
    if(combinedDims==0) combinedDims=(*a)->getNumDims(pedge);
    else if(combinedDims != (*a)->getNumDims(pedge)) break;
    numSameDims++;
  }
  
  // All the sub-arrays must have the same structure
  ROSE_ASSERT(numSameDims == arrays.size());
  
  return combinedDims;
}

//--- pointer like semantics
// support dereference of array object, similar to the dereference of pointer
// Return the element object: array[0]
template <bool defaultMayEq>
MemLocObjectPtr CombinedArray<defaultMayEq>::getDereference(PartEdgePtr pedge)
{
  list<MemLocObjectPtr> elements;
  for(list<ArrayPtr>::iterator a=arrays.begin(); a!=arrays.end(); a++)
    elements.push_back((*a)->getDereference(pedge));
  
  return CombinedMemLocObject<defaultMayEq>::create(elements);
}

// pretty print for the object
template <bool defaultMayEq>
std::string CombinedArray<defaultMayEq>::str(std::string indent) {
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionArr" : "IntersectArr")<<": ";
  for(list<ArrayPtr>::iterator a=arrays.begin(); a!=arrays.end(); ) {
    oss << (*a)->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    a++;
    if(a!=arrays.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}

template <bool defaultMayEq>
std::string CombinedArray<defaultMayEq>::strp(PartEdgePtr pedge, std::string indent) {
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionArr" : "IntersectArr")<<": ";
  for(list<ArrayPtr>::iterator a=arrays.begin(); a!=arrays.end(); ) {
    oss << (*a)->strp(pedge, indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    a++;
    if(a!=arrays.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}


/* ###########################
   ##### CombinedPointer #####
   ########################### */

/*template <bool defaultMayEq>
CombinedPointer<defaultMayEq>::CombinedPointer(const std::list<MemLocObjectPtr>& pointers) : pointers(pointers)
{}

template <bool defaultMayEq>
boost::shared_ptr<CombinedPointer<defaultMayEq> > 
  CombinedPointer<defaultMayEq>::create(const std::list<MemLocObjectPtr>& memLocs)
{
  // There is no consistency check for pointers and it is assumed that all members of memLocs are pointers 
  // so we just create the object
  return boost::make_shared<CombinedPointer<defaultMayEq> >(memLocs);
}*/

template <bool defaultMayEq>
CombinedPointer<defaultMayEq>::CombinedPointer(const list<MemLocObjectPtr>& memLocs) : 
    MemLocObject(NULL), CombinedMemLocObject<defaultMayEq>(memLocs), Pointer(NULL)
{
  for(list<MemLocObjectPtr>::const_iterator ml=memLocs.begin(); ml!=memLocs.end(); ml++) {
    ROSE_ASSERT((*ml)->isPointer());
    pointers.push_back((*ml)->isPointer());
  }
}

template <bool defaultMayEq>
MemLocObjectPtr CombinedPointer<defaultMayEq>::getDereference(PartEdgePtr pedge)
{
  list<MemLocObjectPtr> elements;
  for(list<PointerPtr>::iterator p=pointers.begin(); p!=pointers.end(); p++)
    elements.push_back((*p)->getDereference(pedge));
  
  return CombinedMemLocObject<defaultMayEq>::create(elements);
}

// pretty print for the object
template <bool defaultMayEq>
std::string CombinedPointer<defaultMayEq>::str(std::string indent) {
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionPtr" : "IntersectPtr")<<": ";
  for(list<PointerPtr>::iterator p=pointers.begin(); p!=pointers.end(); ) {
    oss << (*p)->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    p++;
    if(p!=pointers.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}

template <bool defaultMayEq>
std::string CombinedPointer<defaultMayEq>::strp(PartEdgePtr pedge, std::string indent) {
  // Collect the string representations of the sub-fields
  ostringstream oss;
  oss << "["<<(defaultMayEq? "UnionPtr" : "IntersectPtr")<<": ";
  for(list<PointerPtr>::iterator p=pointers.begin(); p!=pointers.end(); ) {
    oss << (*p)->strp(pedge, indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
    p++;
    if(p!=pointers.end()) oss << endl;
  }
  oss << "]";
  
  return oss.str();
}

} //namespace dataflow
