#ifndef PARTITIONS_H
#define PARTITIONS_H

#include "rose.h"
#include <boost/function.hpp>
//#include <boost/lambda/lambda.hpp>
//#include <boost/lambda/bind.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include "cfgUtils.h"
#include "AnalysisDebuggingUtils.h"

namespace dataflow {
// ----------------
// ----- Part -----
// ----------------

class Part;
//typedef boost::shared_ptr<Part> PartPtr;
//typedef boost::shared_ptr<const Part> ConstPartPtr;
class PartEdge;
//typedef boost::shared_ptr<PartEdge> PartEdgePtr;
//typedef boost::shared_ptr<const PartEdge> ConstPartEdgePtr;

// Wrapper for boost:shared_ptr<Type> that can be used as keys in maps because it wraps comparison 
// operations by forwarding them to the Type's own comparison operations. In contrast, the base 
// boost::shared_ptr uses pointer equality.
template <class Type>
class CompSharedPtr : public printable
{
  //public:
  boost::shared_ptr<Type> ptr;
  
  public:
  CompSharedPtr() {}
  
  // Wraps a raw pointer of an object that has a dynamic copying method with a comparable shared pointer
  CompSharedPtr(Type* p) {
    Type* c = dynamic_cast<Type*>(p->copy());
    ROSE_ASSERT(c);
    ptr = boost::shared_ptr<Type>(c);
  }
    
  // Copy constructor
  CompSharedPtr(const CompSharedPtr<Type>& o) : ptr(o.ptr) {}
  
  // Constructor for converting across CompSharedPtr wrappers of compatible types
  template <class OtherType>
  CompSharedPtr(const CompSharedPtr<OtherType>& o) : ptr(boost::static_pointer_cast<Type>(o.ptr)) {}
  
  // Constructor for wrapping boost::shared_ptr with a CompoSharedPtr
  CompSharedPtr(boost::shared_ptr<Type> ptr) : ptr(ptr) {}
  
  template <class OtherType>
  CompSharedPtr(boost::shared_ptr<OtherType> ptr) : ptr(boost::static_pointer_cast<Type>(ptr)) {}
  
  template <class OtherType>
  friend class CompSharedPtr;
  
  CompSharedPtr& operator=(const CompSharedPtr& o) { ptr = o.ptr; return *this; }
  // If both ptr and o.ptr are != NULL, use their equality operator
  // If both ptr and o.ptr are == NULL, they are equal
  // If only one is == NULL but the other is not, order the NULL object as < the non-NULL object
  bool operator==(const CompSharedPtr<Type> & o) const { if(ptr.get()!=NULL) { if(o.get()!=NULL) return (*ptr.get()) == o; else return false; } else { if(o.get()!=NULL) return false; else return true;  } }
  bool operator< (const CompSharedPtr<Type> & o) const { if(ptr.get()!=NULL) { if(o.get()!=NULL) return (*ptr.get()) <  o; else return false; } else { if(o.get()!=NULL) return true;  else return false; } }
  bool operator!=(const CompSharedPtr<Type> & o) const { if(ptr.get()!=NULL) { if(o.get()!=NULL) return (*ptr.get()) != o; else return true;  } else { if(o.get()!=NULL) return true;  else return false; } }
  bool operator>=(const CompSharedPtr<Type> & o) const { if(ptr.get()!=NULL) { if(o.get()!=NULL) return (*ptr.get()) >= o; else return true;  } else { if(o.get()!=NULL) return false; else return true;  } }
  bool operator<=(const CompSharedPtr<Type> & o) const { if(ptr.get()!=NULL) { if(o.get()!=NULL) return (*ptr.get()) <= o; else return false; } else { if(o.get()!=NULL) return true;  else return true;  } }
  bool operator> (const CompSharedPtr<Type> & o) const { if(ptr.get()!=NULL) { if(o.get()!=NULL) return (*ptr.get()) >  o; else return true;  } else { if(o.get()!=NULL) return false; else return false; } }
  
  Type* get() const { return ptr.get(); }
  const Type* operator->() const { return ptr.get(); }
  Type* operator->() { return ptr.get(); }
  
  operator bool() const { return (bool) ptr.get(); }
  
  //PartPtr operator * () { return ptr; }
  
  std::string str(std::string indent="") { return ptr->str(indent); }
};
typedef CompSharedPtr<Part> PartPtr;
typedef CompSharedPtr<PartEdge> PartEdgePtr;

// Returns a new instance of a CompSharedPtr that refers to an instance of CompSharedPtr<Type>
// GB 2012-09-21: We have created an instance of this function for cases with 0-9 input parameters since that is 
//                what boost::make_shared provides when the compiler doesn't support varyadic types. Support for 
//                varyadic types is future work.
template <class Type>
CompSharedPtr<Type> makePart()
{ return boost::make_shared<Type>(); }

template <class Type, class Arg1>
CompSharedPtr<Type> makePart(Arg1 arg1)
{ return boost::make_shared<Type>(arg1); }

template <class Type, class Arg1, class Arg2>
CompSharedPtr<Type> makePart(Arg1 arg1, Arg2 arg2)
{ return boost::make_shared<Type>(arg1, arg2); }

template <class Type, class Arg1, class Arg2, class Arg3>
CompSharedPtr<Type> makePart(Arg1 arg1, Arg2 arg2, Arg3 arg3)
{ return boost::make_shared<Type>(arg1, arg2, arg3); }

template <class Type, class Arg1, class Arg2, class Arg3, class Arg4>
CompSharedPtr<Type> makePart(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
{ return boost::make_shared<Type>(arg1, arg2, arg3, arg4); }

template <class Type, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5>
CompSharedPtr<Type> makePart(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
{ return boost::make_shared<Type>(arg1, arg2, arg3, arg4, arg5); }

template <class Type, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6>
CompSharedPtr<Type> makePart(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
{ return boost::make_shared<Type>(arg1, arg2, arg3, arg4, arg5, arg6); }

template <class Type, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7>
CompSharedPtr<Type> makePart(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7)
{ return boost::make_shared<Type>(arg1, arg2, arg3, arg4, arg5, arg6, arg7); }

template <class Type, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8>
CompSharedPtr<Type> makePart(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8)
{ return boost::make_shared<Type>(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); }

template <class Type, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6, class Arg7, class Arg8, class Arg9>
CompSharedPtr<Type> makePart(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9)
{ return boost::make_shared<Type>(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9); }

// Wrapper for boost::dynamic_pointer_cast for CompSharedPtr
// Used as: dynamic_part_cast<SomePartImplementation>(objectWithPartPtrType);
template <class TargetType, class SourceType>
CompSharedPtr<TargetType> dynamic_part_cast(CompSharedPtr<SourceType> s)
{ 
  if(dynamic_cast<TargetType*>(s.get())) return CompSharedPtr<TargetType>(s);
  else {
    CompSharedPtr<TargetType> null;
    return null;
  }
}

template <class TargetType, class SourceType>
const CompSharedPtr<TargetType> dynamic_const_part_cast(CompSharedPtr<SourceType> s)
{ 
  if(dynamic_cast<const TargetType*>(s.get())) return CompSharedPtr<TargetType>(s);
  else {
    CompSharedPtr<TargetType> null;
    return null;
  }
}

template <class TargetType, class SourceType>
CompSharedPtr<TargetType> static_part_cast(const CompSharedPtr<SourceType> s)
{ 
  if(static_cast<TargetType*>(s.get())) return CompSharedPtr<TargetType>(s);
  else {
    CompSharedPtr<TargetType> null;
    return null;
  }
}

template <class TargetType, class SourceType>
const CompSharedPtr<TargetType> static_const_part_cast(const CompSharedPtr<SourceType> s)
{ 
  if(static_cast<const TargetType*>(s.get())) return CompSharedPtr<TargetType>(s);
  else {
    CompSharedPtr<TargetType> null;
    return null;
  }
}

// Wrapper for boost::make_shared_from_this.
// Used as: make_part_from_this(make_shared_from_this());
template <class Type>
CompSharedPtr<Type> make_part_from_this(boost::shared_ptr<Type> s)
{ return CompSharedPtr<Type>(s); }

// Initializes a shared pointer from a raw pointer
template <class Type>
CompSharedPtr<Type> init_part(Type* p)
{
  return CompSharedPtr<Type>(p);
}

class Part : public printable {
  protected:
  ComposedAnalysis* analysis;
  PartPtr parent;
  
  public:
  Part(ComposedAnalysis* analysis, PartPtr parent) : analysis(analysis), parent(parent) {}
  Part(const Part& that) : analysis(that.analysis), parent(that.parent) {}
  // Returns true if this PartEdge comes from the same analysis as that PartEdge and false otherwise
  bool compatible(const Part& that) { return analysis == that.analysis; }
  bool compatible(PartPtr that)     { return analysis == that->analysis; }
  
  // Returns the Part from which this Part is derived. This function documents the hierarchical descent of this Part
  // and makes it possible to find the common parent of parts derived from different analyses.
  // Returns NULLPart if this part has no parents (i.e. it is implemented by the syntactic analysis)
  virtual PartPtr getParent() const { return parent; }
  
  // Sets this Part's parent
  virtual void setParent(PartPtr parent) { this->parent = parent; }
  
  virtual std::list<PartEdgePtr> outEdges()=0;
  virtual std::list<PartEdgePtr> inEdges()=0;
  virtual std::set<CFGNode> CFGNodes()=0;
  
  /*// Let A={ set of execution prefixes that terminate at the given anchor SgNode }
  // Let O={ set of execution prefixes that terminate at anchor's operand SgNode }
  // Since to reach a given SgNode an execution must first execute all of its operands it must
  //    be true that there is a 1-1 mapping m() : O->A such that o in O is a prefix of m(o).
  // This function is the inverse of m: given the anchor node and operand as well as the
  //    Part that denotes a subset of A (the function is called on this part), 
  //    it returns a list of Parts that partition O.
  virtual std::list<PartPtr> getOperandPart(SgNode* anchor, SgNode* operand)=0;*/
  
  // Returns a PartEdgePtr, where the source is a wild-card part (NULLPart) and the target is this Part
  virtual PartEdgePtr inEdgeFromAny()=0;
  // Returns a PartEdgePtr, where the target is a wild-card part (NULLPart) and the source is this Part
  virtual PartEdgePtr outEdgeToAny()=0;
  
  // Applies the given lambda to all the CFGNodes within this part.
  // Returns true if the lambda returns true on ANY of them.
  template <typename Ret> 
  Ret mapCFGNodeANY(boost::function<bool(const CFGNode&)> func) {
    Ret r = (Ret)NULL;
    std::set<CFGNode> v=CFGNodes();
    for(std::set<CFGNode>::iterator i=v.begin(); i!=v.end(); i++) {
      if((r = func(*i))) return r;
    }
    return r;
  }
  
  // Applies the given lambda to all the CFGNodes within this part.
  // Returns true if the lambda returns true on ALL of them.
  template <typename Ret> 
  Ret mapCFGNodeALL(boost::function<bool(const CFGNode&)> func){
    Ret r = (Ret)NULL;
    std::set<CFGNode> v=CFGNodes();
    for(std::set<CFGNode>::iterator i=v.begin(); i!=v.end(); i++) {
      if(!(r = func(*i))) return r;
    }
    return r;
  }
  
  // If there exist one or more CFGNodes within this part have sub-type NodeType of SgNode,
  // returns a pointer to one of them.
  template <class NodeType>
  NodeType* maySgNodeAny() {
    std::set<CFGNode> v=CFGNodes();
    for(std::set<CFGNode>::iterator i=v.begin(); i!=v.end(); i++) {
      if(dynamic_cast<NodeType*>(i->getNode()) == NULL)  return NULL;
    }
    return dynamic_cast<NodeType*>(v.begin()->getNode());

    /*boost::function<NodeType* (CFGNode)> c = dynamic_cast<NodeType*>(boost::lambda::_1);
    return mapCFGNodeALL<NodeType*>(c);
    return NULL;*/
  }
  
  // If all the CFGNodes within this part have sub-type NodeType of SgNode,
  // returns a pointer to one of them.
  template <class NodeType>
  NodeType* mustSgNodeAll() {
    std::set<CFGNode> v=CFGNodes();
    for(std::set<CFGNode>::iterator i=v.begin(); i!=v.end(); i++) {
      if(dynamic_cast<NodeType*>(i->getNode()) == NULL)  return NULL;
    }
    return dynamic_cast<NodeType*>(v.begin()->getNode());
    
    //boost::function<NodeType* (CFGNode)> c = dynamic_cast<NodeType*>(boost::lambda::_1);
    //return mapCFGNodeALL<NodeType*>(c);
    //return NULL;
  }
  
  // If the filter accepts (returns true) on any of the CFGNodes within this part, return true)
  bool filterAny(bool (*filter) (CFGNode cfgn));
  
  // If the filter accepts (returns true) on all of the CFGNodes within this part, return true)
  bool filterAll(bool (*filter) (CFGNode cfgn));
  
  // The the base equality and comparison operators are implemented in Part and these functions
  // call the equality and inequality test functions supplied by derived classes as needed
  
  // If this and that come from the same analysis, call the type-specific equality test implemented
  // in the derived class. Otherwise, these Parts are not equal.
  bool operator==(const PartPtr& that) const;
  virtual bool equal(const PartPtr& that) const=0;
  
  // If this and that come from the same analysis, call the type-specific inequality test implemented
  // in the derived class. Otherwise, determine inequality by comparing the analysis pointers.
  bool operator<(const PartPtr& that) const;
  virtual bool less(const PartPtr& that) const=0;
  
  bool operator!=(const PartPtr& that) const;
  bool operator>=(const PartPtr& that) const;
  bool operator<=(const PartPtr& that) const;
  bool operator> (const PartPtr& that) const;
};
extern PartPtr NULLPart;

class PartEdge : public printable {
  protected:
  ComposedAnalysis* analysis;
  PartEdgePtr parent;
  
  public:
  PartEdge(ComposedAnalysis* analysis, PartEdgePtr parent) : analysis(analysis), parent(parent) {}
  PartEdge(const PartEdge& that) : analysis(that.analysis), parent(that.parent) {}
  // Returns true if this PartEdge comes from the same analysis as that PartEdge and false otherwise
  bool compatible(const PartEdge& that) { return analysis == that.analysis; }
  bool compatible(PartEdgePtr that)     { return analysis == that->analysis; }
  
  // Returns the PartEdge from which this PartEdge is derived. This function documents the hierarchical descent of this 
  // PartEdgeand makes it possible to find the common parent of parts derived from different analyses.
  // Returns NULLPartEdge if this part has no parents (i.e. it is implemented by the syntactic analysis)
  virtual PartEdgePtr getParent() const { return parent; }
  
  // Sets this PartEdge's parent
  virtual void setParent(PartEdgePtr parent) { this->parent = parent; }
  
  virtual PartPtr source()=0;
  virtual PartPtr target()=0;
  
  // Let A={ set of execution prefixes that terminate at the given anchor SgNode }
  // Let O={ set of execution prefixes that terminate at anchor's operand SgNode }
  // Since to reach a given SgNode an execution must first execute all of its operands it must
  //    be true that there is a 1-1 mapping m() : O->A such that o in O is a prefix of m(o).
  // This function is the inverse of m: given the anchor node and operand as well as the
  //    PartEdge that denotes a subset of A (the function is called on this PartEdge), 
  //    it returns a list of PartEdges that partition O.
  virtual std::list<PartEdgePtr> getOperandPartEdge(SgNode* anchor, SgNode* operand)=0;
  
  // If the source Part corresponds to a conditional of some sort (if, switch, while test, etc.)
  // it must evaluate some predicate and depending on its value continue, execution along one of the
  // outgoing edges. The value associated with each outgoing edge is fixed and known statically.
  // getPredicateValue() returns the value associated with this particular edge. Since a single 
  // Part may correspond to multiple CFGNodes getPredicateValue() returns a map from each CFG node
  // within its source part that corresponds to a conditional to the value of its predicate along 
  // this edge.
  virtual std::map<CFGNode, boost::shared_ptr<SgValueExp> > getPredicateValue()=0;
  
  // The the base equality and comparison operators are implemented in Part and these functions
  // call the equality and inequality test functions supplied by derived classes as needed
  
  // If this and that come from the same analysis, call the type-specific equality test implemented
  // in the derived class. Otherwise, these Parts are not equal.
  bool operator==(const PartEdgePtr& that) const;
  virtual bool equal(const PartEdgePtr& that) const=0;
  
  // If this and that come from the same analysis, call the type-specific inequality test implemented
  // in the derived class. Otherwise, determine inequality by comparing the analysis pointers.
  bool operator<(const PartEdgePtr& that) const;
  virtual bool less(const PartEdgePtr& that) const=0;
  
  bool operator!=(const PartEdgePtr& that) const;
  bool operator>=(const PartEdgePtr& that) const;
  bool operator<=(const PartEdgePtr& that) const;
  bool operator> (const PartEdgePtr& that) const;
};
extern PartEdgePtr NULLPartEdge;

class IntersectionPart;
typedef CompSharedPtr<IntersectionPart> IntersectionPartPtr;
class IntersectionPartEdge;
typedef CompSharedPtr<IntersectionPartEdge> IntersectionPartEdgePtr;

// The intersection of multiple Parts and PartEdges. Partition graph intersections are primarily useful for 
// parallel composition of analyses. In this use-case we have multiple analyses, a subset of which implements their 
// own partition graphs and the rest of which do not. To support this use-case we need to 
// - Provide a way to compute intersections the sub-parts and sub-part edges that are implemented. 
//   This is implemented in methods like Part::outEdges that consider the votes from all the sub-analyses
//   and return all the permutations of those votes.
// - When queries are performed on analyses that do or do not implement partition graphs, we need to 
//   make it easy to extract the part or part edge that is meaningful to each analysis. We support this
//   by maintaining for each IntersectionPart and IntersectionPartEdge a mapping from all analyses that implement
//   partition graphs to the Part/PartEdge they implement. For all analyses that do not implement partition
//   graphs we maintain the Part/PartEdge that these analyses operate on, which is the parent of the 
//   IntersectionPart/IntersectionPartEdge.


// The intersection of multiple Parts. Maintains multiple Parts and responds to API calls with the most 
//   accurate response that its constituent objects return.
class IntersectionPart : public Part
{
  // Parts from analyses that implement partition graphs
  std::map<ComposedAnalysis*, PartPtr> parts;

  public:
  
  //IntersectionPart(PartPtr part, ComposedAnalysis* analysis);
  IntersectionPart(const std::map<ComposedAnalysis*, PartPtr>& parts, PartPtr parent, ComposedAnalysis* analysis);
  
  // Returns the Part associated with this analysis. If the analysis does not implement the partition graph
  // (is not among the keys of parts), returns the parent Part.
  PartPtr getPart(ComposedAnalysis* analysis);
  
  // Returns the list of outgoing IntersectionPartEdge of this Part, which are the cross-product of the outEdges()
  // of its sub-parts.
  std::list<PartEdgePtr> outEdges();
  /*// Recursive computation of the cross-product of the outEdges of all the sub-parts of this Intersection part.
  // Hierarchically builds a recursion tree that contains more and more combinations of PartsPtr from the outEdges
  // of different sub-parts. When the recursion tree reaches its full depth (one level per part in parts), it creates
  // an intersection the current combination of 
  // partI - refers to the current part in parts
  // outPartEdges - the list of outgoing edges of the current combination of this IntersectionPart's sub-parts, 
  //         upto partI
  void outEdges_rec(std::map<ComposedAnalysis*, PartPtr>::iterator partI, 
                    std::map<ComposedAnalysis*, PartPtr> outPartEdges, 
                    std::vector<PartEdgePtr>& edges);*/
  
  // Returns the list of incoming IntersectionPartEdge of this Part, which are the cross-product of the inEdges()
  // of its sub-parts.
  std::list<PartEdgePtr> inEdges();
  
  /*// Recursive computation of the cross-product of the inEdges of all the sub-parts of this Intersection part.
  // Hierarchically builds a recursion tree that contains more and more combinations of PartsPtr from the inEdges
  // of different sub-parts. When the recursion tree reaches its full depth (one level per part in parts), it creates
  // an intersection the current combination of 
  // partI - refers to the current part in parts
  // inPartEdges - the list of incoming edges of the current combination of this IntersectionPart's sub-parts, 
  //         upto partI
  void inEdges_rec(std::list<PartPtr>::iterator partI, std::list<PartEdgePtr> inPartEdges, 
                   std::vector<PartEdgePtr>& edges);*/
    
  // Returns the intersection of the lists of CFGNodes returned by the Parts in parts
  std::set<CFGNode> CFGNodes();
  
  /*
  // Let A={ set of execution prefixes that terminate at the given anchor SgNode }
  // Let O={ set of execution prefixes that terminate at anchor's operand SgNode }
  // Since to reach a given SgNode an execution must first execute all of its operands it must
  //    be true that there is a 1-1 mapping m() : O->A such that o in O is a prefix of m(o).
  // This function is the inverse of m: given the anchor node and operand as well as the
  //    Part that denotes a subset of A (the function is called on this part), 
  //    it returns a list of Parts that partition O.
  std::list<PartPtr> getOperandPart(SgNode* anchor, SgNode* operand);
  
  // Recursive computation of the cross-product of the getOperandParts of all the sub-parts of this Intersection part.
  // Hierarchically builds a recursion tree that contains more and more combinations of PartsPtr from the inEdges
  // of different sub-parts. When the recursion tree reaches its full depth (one level per part in parts), it creates
  // an intersection the current combination of 
  // partI - refers to the current part in parts
  // accumOperandParts - the list of incoming parts of the current combination of this IntersectionPart's sub-parts, 
  //         upto partI
  void getOperandPart_rec(SgNode* anchor, SgNode* operand,
                          std::list<PartPtr>::iterator partI, std::list<PartPtr> accumOperandParts, 
                          std::list<PartPtr>& allParts);*/
  
  // Returns a PartEdgePtr, where the source is a wild-card part (NULLPart) and the target is this Part
  PartEdgePtr inEdgeFromAny();
  
  // Returns a PartEdgePtr, where the target is a wild-card part (NULLPart) and the source is this Part
  PartEdgePtr outEdgeToAny();
  
  // Two IntersectionParts are equal if their parents and all their constituent sub-parts are equal
  bool equal(const PartPtr& that) const;
  
  // Lexicographic ordering: This IntersectionPart is < that IntersectionPart if 
  // - their parents are < ordered, OR
  // - if this has fewer parts than that, OR
  // - there exists an index i in this.parts and that.parts s.t. forall j<i. this.parts[j]==that.parts[j] and 
  //   this.parts[i] < that.parts[i].
  bool less(const PartPtr& that) const;
  
  std::string str(std::string indent="");
};


class IntersectionPartEdge : public PartEdge
{
  // The edges being intersected
  std::map<ComposedAnalysis*, PartEdgePtr> edges;
  
  public:
  
  //IntersectionPartEdge(PartEdgePtr edge, ComposedAnalysis* analysis);
  IntersectionPartEdge(const std::map<ComposedAnalysis*, PartEdgePtr>& edges, PartEdgePtr parent, ComposedAnalysis* analysis);
  
  // Returns the PartEdge associated with this analysis. If the analysis does not implement the partition graph
  // (is not among the keys of parts), returns the parent PartEdge.
  PartEdgePtr getPartEdge(ComposedAnalysis* analysis);
  
  // Return the part that intersects the sources of all the sub-edges of this IntersectionPartEdge
  PartPtr source();
  
  // Return the part that intersects the targets of all the sub-edges of this IntersectionPartEdge
  PartPtr target();
  
  // Let A={ set of execution prefixes that terminate at the given anchor SgNode }
  // Let O={ set of execution prefixes that terminate at anchor's operand SgNode }
  // Since to reach a given SgNode an execution must first execute all of its operands it must
  //    be true that there is a 1-1 mapping m() : O->A such that o in O is a prefix of m(o).
  // This function is the inverse of m: given the anchor node and operand as well as the
  //    PartEdge that denotes a subset of A (the function is called on this PartEdge), 
  //    it returns a list of PartEdges that partition O.
  std::list<PartEdgePtr> getOperandPartEdge(SgNode* anchor, SgNode* operand);
  
  private:
  // Recursive computation of the cross-product of the getOperandParts of all the sub-part edges of this Intersection part edge.
  // Hierarchically builds a recursion tree that contains more and more combinations of PartEdgePtrs from the results of
  // getOperandPart of different sub-part edges. When the recursion tree reaches its full depth (one level per edge in edges), 
  // it creates an intersection the current combination of edges.
  // edgeI - refers to the current edge in edges
  // accumOperandPartEdges - the list of incoming edgesof the current combination of this IntersectionPartEdges's sub-Edges, 
  //         upto edgeI
  void getOperandPartEdge_rec(SgNode* anchor, SgNode* operand,
                              std::list<PartEdgePtr>::iterator edgeI, std::list<PartEdgePtr> accumOperandPartEdges, 
                              std::list<PartEdgePtr>& allPartEdges);
  
  // If the source Part corresponds to a conditional of some sort (if, switch, while test, etc.)
  // it must evaluate some predicate and depending on its value continue, execution along one of the
  // outgoing edges. The value associated with each outgoing edge is fixed and known statically.
  // getPredicateValue() returns the value associated with this particular edge. Since a single 
  // Part may correspond to multiple CFGNodes getPredicateValue() returns a map from each CFG node
  // within its source part that corresponds to a conditional to the value of its predicate along 
  // this edge.
  std::map<CFGNode, boost::shared_ptr<SgValueExp> > getPredicateValue();  

  public:
  // Two IntersectionPartEdges are equal of all their constituent sub-parts are equal
  bool equal(const PartEdgePtr& o) const;
  
  // Lexicographic ordering: This IntersectionPartEdge is < that IntersectionPartEdge if this has fewer edges than that or
  // there exists an index i in this.edges and that.edges s.t. forall j<i. this.edges[j]==that.edges[j] and 
  // this.edges[i] < that.edges[i].
  bool less(const PartEdgePtr& o) const;
  
  std::string str(std::string indent="");
};

}; // namespace dataflow

#endif
