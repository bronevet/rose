#ifndef _POINTSTOANALYSIS_H
#define _POINTSTOANALYSIS_H

/*
 * Simple Intra PointsTo Analysis
 * Lattice - AbstractObjectSet
 * AbstractObjectMap (ProductLattice) stores an AbstractObjectSet for each MemLocObjectPtr
 * author: sriram@cs.utah.edu
 */

#include "compose.h"
#include "abstract_object_map.h"
#include "abstract_object_set.h"

namespace dataflow
{
  class PointsToAnalysis;
  // Transfer functions for the PointsTo analysis
  class PointsToAnalysisTransfer : public IntraDFTransferVisitor
  {
    typedef boost::shared_ptr<AbstractObjectSet> AbstractObjectSetPtr;
    Composer* composer;
    PointsToAnalysis* analysis;
    // pointer to node state of the analysis at this part
    AbstractObjectMap* productLattice;
    // used by the analysis to determine if the states modified or not
    bool modified;
  public:
    PointsToAnalysisTransfer(const Function& func, PartPtr part, CFGNode cn, NodeState& state,
                             std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo,
                             Composer* composer, PointsToAnalysis* analysis);

    // set the pointer of AbstractObjectMap at this PartEdge
    void setProductLattice();

    bool finish();

    // lattice access functions from the map (product lattice)
    // copied from VariableStateTransfer.h
    AbstractObjectSetPtr getLattice(SgExpression* sgexp);
    AbstractObjectSetPtr getLatticeOperand(SgNode* sgn, SgExpression* operand);
    AbstractObjectSetPtr getLatticeCommon(MemLocObjectPtr ml);
    AbstractObjectSetPtr getLattice(const AbstractObjectPtr o);

    // Transfer functions
    void visit(SgAssignOp* sgn);
  };

  class PointsToAnalysis : public virtual IntraFWDataflow
  {
  public:
    PointsToAnalysis() { }

    void genInitLattice(const Function& func, PartPtr part, PartEdgePtr pedge,
                        std::vector<Lattice*>& initLattices);

    bool transfer(const Function& func, PartPtr part, CFGNode cn, NodeState& state, 
                  std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo) { ROSE_ASSERT(false); return false; }

    boost::shared_ptr<IntraDFTransferVisitor> 
      getTransferVisitor(const Function& func, PartPtr part, CFGNode cn, NodeState& state, 
                         std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo);
    
    // functions called by composer
    MemLocObjectPtr Expr2MemLoc(SgNode* sgn, PartEdgePtr pedge);

    std::string str(std::string indent); 

    friend class PointsToAnalysisTransfer;
  };

  // typedef std::list<MemLocObjectPtr> PointsToSet;
  // class PointsToML : public MemLocObject
  // {
  //   PointsToSet pts;
  //   MemLocObjectPtr mem;
  // public:
  //   PointsToML(SgNode* sgn, MemLocObjectPtr _mem) : MemLocObject(sgn), mem(_mem) { }
  //   PointsToML(const PointsToML& that);
  //   MemLocObjectPtr copyML() const { return boost::make_shared<PointsToML>(*this); }
    
  //   bool mayEqualML(MemLocObjectPtr that, PartEdgePtr pedge);
  //   bool mustEqualML(MemLocObjectPtr that, PartEdgePtr pedge);
    
  //   bool isLive(PartEdgePtr pedge) const { return mem->isLive(pedge); }

  //   PointsToSet getPointsToSet() { return pts; }
        
  //   std::string str(std::string indent) const;
  //   std::string str(std::string indent) { return ((const PointsToML*)this)->str(""); }
  // };
};

#endif
