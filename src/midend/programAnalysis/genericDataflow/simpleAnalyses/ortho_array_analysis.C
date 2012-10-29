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
    MemLocObjectPtrPair array = composer->Expr2MemLoc(arrayNameExp, pedge, this);
    assert(array.isArray());
    
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

    // GB: Do we need to deallocate subscripts???
    Dbg::dbg << "OrthogonalArrayAnalysis::Expr2MemLoc() array->getElements(iv)"<<endl;
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;array="<<array.strp(pedge, "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;iv="<<iv->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    
    MemLocObjectPtr tmp = array.mem ? array.mem->isArray()->getElements(iv, pedge) :
                                      array.expr->isArray()->getElements(iv, pedge);
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;result="<<tmp<<"="<<tmp.get()<<endl;
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;result="<<tmp->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    return tmp;
    //return boost::make_shared<OrthoArrayMemLocObject>(array, iv, p);
  } else {
    MemLocObjectPtrPair notArray = composer->Expr2MemLoc(n, pedge, this);
    // Return the memory location if possible but if not, return the expression object
    return (notArray.mem ? notArray.mem : notArray.expr);
    //return boost::make_shared<OrthoArrayMemLocObject>(notArray, p);
  }
}

}; // namespace dataflow