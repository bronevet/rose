#include "ortho_array_analysis.h"
#include "compose.h"
#include <boost/make_shared.hpp>

namespace dataflow {
int orthoArrayAnalysisDebugLevel=1;

/*********************************
 ***** OrthoIndexVector_Impl *****
 *********************************/

std::string OrthoIndexVector_Impl::str(std::string indent) const // pretty print for the object  
{
  string rt;
  for(std::vector<ValueObjectPtr> ::const_iterator iter = index_vector.begin(); iter != index_vector.end(); iter++)
  {
    ValueObjectPtr current_index_field = *iter;
    rt += current_index_field->str(indent+"    ");
    if(iter != index_vector.begin()) rt += ", ";
  }
  return rt;
}

bool OrthoIndexVector_Impl::mayEqual(IndexVectorPtr other, PartEdgePtr pedge)
{
  //Dbg::dbg << "OrthoIndexVector_Impl::mayEqual()"<<endl;

  OrthoIndexVector_ImplPtr other_impl = boost::dynamic_pointer_cast<OrthoIndexVector_Impl>(other);
  /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;this="<<str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;other="<<other->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;*/

  // If other is not of a compatible type
  if(!other_impl) {
    // Cannot be sure that objects are not equal, so conservatively state they may be equal
    return true;
  }
  bool rt = false;

  bool has_diff_element = false;
  if (this->getSize() == other_impl->getSize()) 
  { // same size, no different element
    for (size_t i =0; i< other_impl->getSize(); i++)
    {
      //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<i<<" : mayEqual="<<this->index_vector[i]->mayEqual(other_impl->index_vector[i], p)<<endl;
      if (!(this->index_vector[i]->mayEqual(other_impl->index_vector[i], pedge)))
      {
        has_diff_element = true;
          break;
      }
    }
    if (!has_diff_element )
      rt = true;
  }
  //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<(rt ? "MAY-EQUAL": "NOT mayEqual")<<endl;
  return rt; 
}

bool OrthoIndexVector_Impl::mustEqual(IndexVectorPtr other, PartEdgePtr pedge)
{
  //Dbg::dbg << "OrthoIndexVector_Impl::mayEqual()"<<endl;
  
  OrthoIndexVector_ImplPtr other_impl = boost::dynamic_pointer_cast<OrthoIndexVector_Impl>(other);
  /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;this="<<str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;other="<<other->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;*/

  // If other is not of a compatible type
  if(!other_impl) {
    // Cannot be sure that objects must be equal, so conservatively don't claim this
    return false;
  }
  bool rt = false;

  bool has_diff_element = false;
  if (this->getSize() == other_impl->getSize()) 
  { // same size, no different element
    for (size_t i =0; i< other_impl->getSize(); i++)
    {
      //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<i<<" : mustEqual="<<this->index_vector[i]->mustEqual(other_impl->index_vector[i], p)<<endl;
      if (!(this->index_vector[i]->mustEqual(other_impl->index_vector[i], pedge)))
      {
        has_diff_element = true;
          break;
      }
    }
    if (!has_diff_element )
      rt = true;
  }
  //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<(rt ? "MUST-EQUAL": "NOT mustEqual")<<endl;
  return rt;
}

bool OrthoArrayML::mayEqualML(MemLocObjectPtr other, PartEdgePtr pedge)
{
  OrthoArrayMLPtr otherML = boost::dynamic_pointer_cast<OrthoArrayML>(other);
  ROSE_ASSERT(otherML);

  // compare only if two objects are same types
  if( (this->isArrayElement && !otherML->isArrayElement) ||
      (!this->isArrayElement && otherML->isArrayElement) ) return false;

  bool retval = false;

  // both are array element
  if(otherML->isArrayElement) {
    // making sure both are array elements
    ROSE_ASSERT(otherML->p_array); ROSE_ASSERT(otherML->p_iv);
    ROSE_ASSERT(p_array); ROSE_ASSERT(p_iv);

    // return true only if 
    // array objects mayequals the other and
    // index vector mayequals the other
    retval = (this->p_array)->mayEqual(otherML->p_array, pedge)
      && (this->p_iv)->mayEqual(otherML->p_iv, pedge);
  }
  // both ML are not array elements
  else {
    // making sure both are not array
    ROSE_ASSERT(p_notarray); ROSE_ASSERT(otherML->p_notarray);
    retval = p_notarray->mayEqual(otherML->p_notarray, pedge);
  }

  return retval;
}

bool OrthoArrayML::mustEqualML(MemLocObjectPtr other, PartEdgePtr pedge)
{
  OrthoArrayMLPtr otherML = boost::dynamic_pointer_cast<OrthoArrayML>(other);
  // if its not an array object, we know they are not equal
  // compare only if two objects are same types
  if( (this->isArrayElement && !otherML->isArrayElement) ||
      (!this->isArrayElement && otherML->isArrayElement) ) return false;

  bool retval = false;
  // both are array element
  if(otherML->isArrayElement) {
    // making sure both are array elements
    ROSE_ASSERT(otherML->p_array); ROSE_ASSERT(otherML->p_iv);
    ROSE_ASSERT(p_array); ROSE_ASSERT(p_iv);

    // return true only if 
    // array objects mayequals the other and
    // index vector mayequals the other
    retval = (this->p_array)->mayEqual(otherML->p_array, pedge)
      && (this->p_iv)->mayEqual(otherML->p_iv, pedge);
  }
  // both ML are not array elements
  else {
    // making sure both are not array
    ROSE_ASSERT(p_notarray); ROSE_ASSERT(otherML->p_notarray);
    retval = p_notarray->mayEqual(otherML->p_notarray, pedge);
  }
  return retval;
}

std::string OrthoArrayML::str(std::string indent) const
{
  ostringstream oss;
  oss << "[OrthoArrayML: ";
  if(isArrayElement) {
    oss << p_array->str(indent) << ", ";
    oss << p_iv->str(indent) << " ";
  }
  else {
    oss << p_notarray->str(indent) << indent;
  }
  oss << "]" << endl;
  return oss.str();
}

//NOTE: Do we have to always re-implement this for every analysis
bool OrthoArrayML::isLive(PartEdgePtr pedge) const
{
  // if the array is live, element is live
  //NOTE: recheck
  if(isArrayElement) return p_array->isLive(pedge);
  else return p_notarray->isLive(pedge);
}


/**********************************
 ***** OrthoArrayMemLocObject *****
 ********************************** /
OrthoArrayMemLocObject::OrthoArrayMemLocObject(MemLocObjectPtr array, OrthoIndexVector_ImplPtr iv, PartPtr p)
  : part(p)
{ 
  Dbg::dbg << "OrthoArrayMemLocObject::OrthoArrayMemLocObject()"<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;array="<<array->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;iv="<<iv->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  
  // Since this is a constructor for a[i] objects, array must be an array object
  // GB: what happens if array is a pointer? Our API doesn't support indexing into pointers
  ArrayPtr a = isArray(array);
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;a="<<a<<"="<<a.get()<<endl;
  assert(a);
  arrayElt = a->getElements(iv);
}

OrthoArrayMemLocObject::OrthoArrayMemLocObject(MemLocObjectPtr notArray, PartPtr p)
  : notArray(notArray), part(p)
{ }

OrthoArrayMemLocObject::OrthoArrayMemLocObject(const OrthoArrayMemLocObject& that): arrayElt(that.arrayElt), notArray(that.notArray), part(that.part)
{}

bool OrthoArrayMemLocObject::mayEqual(MemLocObjectPtr o, PartPtr part) const
{
  OrthoArrayMemLocObjectPtr that = boost::dynamic_pointer_cast<OrthoArrayMemLocObject>(o);
  Dbg::dbg << "OrthoArrayMemLocObject::mayEqual()"<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;this="<<str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;that=("<<that.get()<<")="<<o->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  
  // If that is of the right type
  if(that) {
    // And this and that are both array elements
    if(arrayElt && that->arrayElt) {
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;arrayElt->mayEqual(that->arrayElt, part)="<<arrayElt->mayEqual(that->arrayElt, part)<<endl;
      return arrayElt->mayEqual(that->arrayElt, part);
    // And this and that are both not array elements
    } else if(notArray && that->notArray) {
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;notArray->mayEqual(that->notArray, part)"<<notArray->mayEqual(that->notArray, part)<<endl;
      return notArray->mayEqual(that->notArray, part);
    }
    else
      return false;
  } else
    return false;
}

bool OrthoArrayMemLocObject::mustEqual(MemLocObjectPtr o, PartPtr part) const
{
  OrthoArrayMemLocObjectPtr that = boost::dynamic_pointer_cast<OrthoArrayMemLocObject>(o);
  Dbg::dbg << "OrthoArrayMemLocObject::mustEqual()"<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;this="<<str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;that=("<<that.get()<<")="<<o->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  
  // If that is of the right type
  if(that) {
    // And this and that are both array elements
    if(arrayElt && that->arrayElt) {
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;arrayElt->mustEqual(that->arrayElt, part)="<<arrayElt->mustEqual(that->arrayElt, part)<<endl;
      return arrayElt->mustEqual(that->arrayElt, part);
    // And this and that are both not array elements
    } else if(notArray && that->notArray) {
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;notArray->mustEqual(that->notArray, part)"<<notArray->mustEqual(that->notArray, part)<<endl;
      return notArray->mustEqual(that->notArray, part);
    } else
      return false;
  } else
    return false;
}

// pretty print for the object
std::string OrthoArrayMemLocObject::str(std::string indent) const
{
    return strp(part, indent);
}

std::string OrthoArrayMemLocObject::strp(PartPtr part, std::string indent) const
{
  ostringstream oss;
  oss << "[OrthoArrayMemLocObject: "<<(arrayElt? "arrayElt: ": "")<<(arrayElt? arrayElt->str("&nbsp;&nbsp;&nbsp;&nbsp;"): "")<<
                                      (notArray? "notArray: ": "")<<(notArray? notArray->str("&nbsp;&nbsp;&nbsp;&nbsp;"): "")<<"]";
  return oss.str();
}

// Allocates a copy of this object and returns a pointer to it
MemLocObjectPtr OrthoArrayMemLocObject::copyML() const
{
  return boost::make_shared<OrthoArrayMemLocObject>(*this);
}*/

/*******************************
 ***** OrthogonalArrayAnalysis *****
 *******************************/

// Maps the given SgNode to an implementation of the MemLocObject abstraction.
MemLocObjectPtr OrthogonalArrayAnalysis::Expr2MemLoc(SgNode* n, PartEdgePtr pedge)
{
  // If this is a top-most array index expression
  if(isSgPntrArrRefExp(n) && 
     (!isSgPntrArrRefExp (n->get_parent()) || !isSgPntrArrRefExp (isSgPntrArrRefExp (n->get_parent())->get_lhs_operand()))) {
    SgExpression* arrayNameExp = NULL;
    std::vector<SgExpression*>* subscripts = new std::vector<SgExpression*>;
    SageInterface::isArrayReference(isSgPntrArrRefExp(n), &arrayNameExp, &subscripts);
    // MemLocObjectPtrPair array = composer->Expr2MemLoc(arrayNameExp, pedge, this);
    MemLocObjectPtr array = composer->Expr2MemLoc(arrayNameExp, pedge, this);

    OrthoIndexVector_ImplPtr iv = boost::make_shared<OrthoIndexVector_Impl>();
    
    /*Dbg::dbg << "Predecessor Nodes #("<<p.inEdges().size()<<")="<<endl;
    Dbg::indent ind(1,1);
    for(std::vector<DataflowEdge>::const_iterator in=p.inEdges().begin(); in!=p.inEdges().end(); in++)
      Dbg::dbg << "["<<((*in).source().getNode() ? (*in).source().getNode()->unparseToString() : "NULL")<<" | "<<
                       ((*in).source().getNode() ? (*in).source().getNode()->class_name()      : "NULL")<<" | "<<(*in).source().getIndex()<<"]"<<endl;*/
    
    for (std::vector<SgExpression*>::iterator iter = subscripts->begin(); iter != subscripts->end(); iter++) {
      //CFGNode subNode(*iter, 2);
      Dbg::dbg << "subNode = ["<<(*iter)->unparseToString()<<" | "<<(*iter)->class_name()<<"]"<<endl;
      //DataflowNode subNodeDF(subNode, filter);
      iv->index_vector.push_back(composer->Expr2Val(*iter, pedge, this));
    }
    
    // MemLocObjectPtr tmp = array.mem ? array.mem->isArray()->getElements(iv, pedge) :
    //                                   array.expr->isArray()->getElements(iv, pedge);
    // ROSE_ASSERT(array->isArray());
    // MemLocObjectPtr tmp = array->isArray()->getElements(iv, pedge);
    ROSE_ASSERT(array); ROSE_ASSERT(iv);
    MemLocObjectPtr tmp = boost::make_shared<OrthoArrayML>(n, array, iv);

    // GB: Do we need to deallocate subscripts???
    Dbg::dbg << "OrthogonalArrayAnalysis::Expr2MemLoc() "<<endl;
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<tmp->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;

    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;result="<<tmp<<"="<<tmp.get()<<endl;
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;result="<<tmp->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    return tmp;
    //return boost::make_shared<OrthoArrayML>(array, iv, p);
  } else {    
    MemLocObjectPtr notArray = composer->Expr2MemLoc(n, pedge, this);

    //NOTE: if the analysis does not handle some expressions,
    // it must wrap them with its own memory/value object to
    // ensure consistent wrapping by the composer.
    return boost::make_shared<OrthoArrayML>(n, notArray);
  }
}

}; // namespace dataflow
