#ifndef ORTHO_ARRAY_ANALYSIS_H
#define ORTHO_ARRAY_ANALYSIS_H

#include "compose.h"

namespace dataflow
{
/***********************************
 ***** OrthoArrayMemLocObject *****
 ***********************************/

// This is an analysis that interprets array expressions of the form a[i] by orthogonally 
// combining information from a memory location analysis to interpret "a" and a value analysis
// to interpret "i".

extern int orthoArrayAnalysisDebugLevel;

class OrthoIndexVector_Impl : public IndexVector
{
  public:
    size_t getSize() const {  return index_vector.size(); };
    /*GB: Deprecating IndexSets and replacing them with ValueObjects.
    std::vector<IndexSet *> index_vector; // a vector of memory objects of named objects or temp expression objects */
    std::vector<ValueObjectPtr> index_vector; // a vector of memory objects of named objects or temp expression objects
    std::string str(std::string indent) const; // pretty print for the object
    std::string str(std::string indent) { return ((const OrthoIndexVector_Impl*)this)->str(indent); }

    bool mayEqual(IndexVectorPtr other, PartEdgePtr pedge);
    bool mustEqual(IndexVectorPtr other, PartEdgePtr pedge);
};
typedef boost::shared_ptr<OrthoIndexVector_Impl> OrthoIndexVector_ImplPtr;

/*class OrthoArrayMemLocObject : public MemLocObject
{
  protected:
  // If this MemLocObject represents an array expression a[i] this represents the "a".
  // Otherwise, this represents the entire expression.
  //MemLocObjectPtr array;
  // If this MemLocObject represents an array expression a[i] this represents the "i".
  // Otherwise, it is null
  //ValueObjectPtr value;
  // If this MemLocObject represents an array expression a[i] represents the entire "a[i]".
  // Otherwise, it is null
  MemLocObjectPtr arrayElt;
  // If this MemLocObject doesn't represent an array expression, it is pointed to by this
  MemLocObjectPtr notArray;
  PartPtr part;

  public:
  OrthoArrayMemLocObject(MemLocObjectPtr array, OrthoIndexVector_ImplPtr value, PartPtr p);
  OrthoArrayMemLocObject(MemLocObjectPtr notArray, PartPtr p);
  OrthoArrayMemLocObject(const OrthoArrayMemLocObject& that);

  bool mayEqual(MemLocObjectPtr o, PartPtr p) const;
  bool mustEqual(MemLocObjectPtr o, PartPtr p) const;
  
  // pretty print for the object
  std::string str(std::string indent="") const;
  std::string str(std::string indent="") { return ((const OrthoArrayMemLocObject*)this)->str(indent); }
  std::string strp(PartPtr part, std::string indent="") const;
  std::string strp(PartPtr part, std::string indent="") { return ((const OrthoArrayMemLocObject*)this)->strp(part, indent); }
  
  // Allocates a copy of this object and returns a pointer to it
  MemLocObjectPtr copyML() const;
  
  private:
  // Dummy Virtual method to make it possible to dynamic cast from OrthoArrayMemLocObjectPtr to MemLocObjectPtr
  virtual void foo() {}
};
typedef boost::shared_ptr<OrthoArrayMemLocObject> OrthoArrayMemLocObjectPtr;
*/

/***********************************
 ***** OrthogonalArrayAnalysis *****
 ***********************************/

class OrthogonalArrayAnalysis : virtual public IntraUndirDataflow
{
  public:
  OrthogonalArrayAnalysis() : IntraUndirDataflow() {}
    
  // The genInitLattice, genInitFact and transfer functions are not implemented since this 
  // is not a dataflow analysis.
   
  // Maps the given SgNode to an implementation of the MemLocObject abstraction.
  // Variant of Expr2Val where Part field is ignored since it makes no difference for the syntactic analysis.
  MemLocObjectPtr  Expr2MemLoc (SgNode* n, PartEdgePtr pedge);

  // pretty print for the object
  std::string str(std::string indent="")
  { return "OrthogonalArrayAnalysis"; }
};

}; //namespace dataflow

#endif