#ifndef DEAD_PATH_ELIM_ANALYSIS_H
#define DEAD_PATH_ELIM_ANALYSIS_H

#include "compose.h"
#include "boost/enable_shared_from_this.hpp"

namespace dataflow
{
/* ######################################
   ##### Live Dead Path Elimination #####
   ###################################### */

// This is an analysis that implements the partition graph, ensuring that all dead paths are eliminated and not
// shown to client analyses.

extern int deadPathElimAnalysisDebugLevel;

class DeadPathElimTransfer;
class DeadPathElimAnalysis;
class DeadPathElimPart;
typedef CompSharedPtr<DeadPathElimPart> DeadPathElimPartPtr;
class DeadPathElimPartEdge;
typedef CompSharedPtr<DeadPathElimPartEdge> DeadPathElimPartEdgePtr;

// This object's current level in the lattice: (bottom, dead, live)
enum DPELevel {bottom=0, dead=1, live=2};

class DeadPathElimPart : public Part, public boost::enable_shared_from_this<DeadPathElimPart>
{
  private:
  
  friend class DeadPathElimPartEdge;
  friend class DeadPathElimTransfer;
  
  public:
  DeadPathElimPart(PartPtr base, ComposedAnalysis* analysis);
  DeadPathElimPart(const DeadPathElimPart& that);
  
  public:
  // -------------------------------------------
  // Functions that need to be defined for Parts
  // -------------------------------------------
  
  std::list<PartEdgePtr> outEdges();
  std::list<PartEdgePtr> inEdges();
  std::set<CFGNode> CFGNodes();
  
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
  bool less(const PartPtr& o)  const;
  
  // Pretty print for the object
  std::string str(std::string indent="");
}; // class DeadPathElimPart

class DeadPathElimPartEdge : public FiniteLattice, public PartEdge {  
  // The part that this object is wrapping with live/dead status
  DeadPathElimPartPtr src;
  DeadPathElimPartPtr tgt;
  
  // For edges from parts that contain CFGNodes that correspond to conditionals (if, switch, while test, etc.)
  // records a mapping from these CFGNodes to the value outcome that leads control along this edge.
  std::map<CFGNode, boost::shared_ptr<SgValueExp> > predVals;

  DPELevel level;
  
  friend class DeadPathElimPart; 
  friend class DeadPathElimTransfer;
  
  public:
  /* GB 2012-10-15 - Commented out because this constructor makes it difficult to set the lattice of the created edge
  DeadPathElimPartEdge(DeadPathElimPartPtr src, DeadPathElimPartPtr tgt, 
                       PartEdgePtr baseEdge, DeadPathElimAnalysis* analysis);*/
  
  // Constructor to be used when constructing the edges (e.g. from genInitLattice()).  
  DeadPathElimPartEdge(PartEdgePtr baseEdge, ComposedAnalysis* analysis, DPELevel level);
  
  // Constructor to be used when traversing the part graph created by the DeadPathElimAnalysis, after
  // all the DeadPathElimPartEdges have been constructed and stored in NodeStates.
  DeadPathElimPartEdge(PartEdgePtr baseEdge, ComposedAnalysis* analysis);
  
  DeadPathElimPartEdge(const DeadPathElimPartEdge& that);
  
  // Returns a pointer to the DeadPathElimAnalysis that created this part edge
  DeadPathElimAnalysis* dpeAnalysis();
  
  PartPtr source();
  PartPtr target();
  
  // Overload the setPartEdge (from Lattice) and setParent (from Part) methods to ensure that they
  // are always set in a consistent manner regardless of which one is called
  // Sets the PartEdge that this Lattice's information corresponds to. 
  // Returns true if this causes the edge to change and false otherwise
  bool setPartEdge(PartEdgePtr latPEdge);
  // Sets this Part's parent
  void setParent(PartEdgePtr parent);
  
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
  
  // Adds a mapping from a CFGNode to the outcome of its predicate
  void mapPred2Val(CFGNode n, boost::shared_ptr<SgValueExp> val);
  
  // Empties out the mapping of CFGNodes to the outcomes of their predicates
  void clearPred2Val();
    
  bool equal(const PartEdgePtr& o) const;
  bool less(const PartEdgePtr& o)  const;
  
  // Pretty print for the object
  std::string str(std::string indent="");
  
  public:
  // ----------------------------------------------
  // Functions that need to be defined for Lattices
  // ----------------------------------------------
  void initialize();
  
  // Returns a copy of this lattice
  Lattice* copy() const;
  
  // Overwrites the state of "this" Lattice with "that" Lattice
  void copy(Lattice* that);
  
  bool operator==(Lattice* that) /*const*/;
  
// Called by analyses to transfer this lattice's contents from across function scopes from a caller function 
  //    to a callee's scope and vice versa. If this this lattice maintains any information on the basis of 
  //    individual MemLocObjects these mappings must be converted, with MemLocObjects that are keys of the ml2ml 
  //    replaced with their corresponding values. If a given key of ml2ml does not appear in the lattice, it must
  //    be added to the lattice and assigned a default initial value. In many cases (e.g. over-approximate sets 
  //    of MemLocObjects) this may not require any actual insertions. If the value of a given ml2ml mapping is 
  //    NULL (empty boost::shared_ptr), any information for MemLocObjects that must-equal to the key should be 
  //    deleted.
  // The function takes newPEdge, the edge that points to the part within which the values of ml2ml should be 
  //    interpreted. It corresponds to the code region(s) to which we are remapping.
  // remapML must return a freshly-allocated object.
  Lattice* remapML(const std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >& ml2ml, PartEdgePtr newPEdge);
  
  // Adds information about the MemLocObjects in newL to this Lattice, overwriting any information previously 
  //    maintained in this lattice about them.
  // Returns true if the Lattice state is modified and false otherwise.
  bool replaceML(Lattice* newL);
  
  // Computes the meet of this and that and saves the result in this
  // Returns true if this causes this to change and false otherwise
  bool meetUpdate(Lattice* that);
  
  // Set this Lattice object to represent the set of all possible execution prefixes.
  // Return true if this causes the object to change and false otherwise.
  bool setToFull();
  
  // Set this Lattice object to represent the of no execution prefixes (empty set)
  // Return true if this causes the object to change and false otherwise.
  bool setToEmpty();
  
  // Set this Lattice object to represent a dead part
  bool setToDead();
};

/********************************
 ***** DeadPathElimAnalysis *****
 ********************************/

class DeadPathElimTransfer : public IntraDFTransferVisitor
{
  DeadPathElimAnalysis* dpea;
  PartPtr part;
  CFGNode cn;
  bool modified;

  public:
  DeadPathElimTransfer(const Function &f, PartPtr part, CFGNode cn, NodeState &s, std::map<PartEdgePtr, std::vector<Lattice*> > &d, 
                       DeadPathElimAnalysis* dpea);

  bool finish();

  void visit(SgIfStmt *sgn);
  void visit(SgNode *sgn);
};

class DeadPathElimAnalysis : public IntraFWDataflow
{
  protected:
   
  public:
  DeadPathElimAnalysis();
  
  // Initializes the state of analysis lattices at the given function, part and edge into our out of the part
  // by setting initLattices to refer to freshly-allocated Lattice objects.
  void genInitLattice(const Function& func, PartPtr part, PartEdgePtr pedge, 
                      std::vector<Lattice*>& initLattices);
  
  bool transfer(const Function& func, PartPtr part, CFGNode cn, NodeState& state, 
                std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo);
  
  boost::shared_ptr<IntraDFTransferVisitor> getTransferVisitor(const Function& func, PartPtr part, CFGNode cn, 
                                                               NodeState& state, map<PartEdgePtr, vector<Lattice*> >& dfInfo);
  
  // Abstract interpretation functions that return this analysis' abstractions that 
  // represent the outcome of the given SgExpression. The default implementations of 
  // these throw NotImplementedException so that if a derived class does not implement 
  // any of these functions, the Composer is informed.
  //
  // The objects returned by these functions are expected to be deallocated by their callers.
  
  // Calls composer->Expr2Val() on the base edge of pedge
  ValueObjectPtr   Expr2Val    (SgNode* n, PartEdgePtr pedge);
  
  // Calls composer->Expr2CodeLoc() on the base edge of pedge
  MemLocObjectPtr  Expr2MemLoc (SgNode* n, PartEdgePtr pedge);
  
  // Calls composer->Expr2CodeLoc() on the base edge of pedge
  CodeLocObjectPtr Expr2CodeLoc(SgNode* n, PartEdgePtr pedge);
  
  // Return the anchor Parts of a given function
  public:
  PartPtr GetFunctionStartPart_Spec(const Function& func);
  PartPtr GetFunctionEndPart_Spec(const Function& func);
  
  // Pretty print for the object
  std::string str(std::string indent="")
  { return "DeadPathElimAnalysis"; }
};

}; //namespace dataflow

#endif  /* DEAD_PATH_ELIM_ANALYSIS_H */

