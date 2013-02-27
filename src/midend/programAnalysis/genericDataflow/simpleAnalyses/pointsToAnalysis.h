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
  extern int ptaDebugLevel;

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
    int debugLevel;
  public:
    PointsToAnalysisTransfer(const Function& func, PartPtr part, CFGNode cn, NodeState& state,
                             std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo,
                             Composer* composer, PointsToAnalysis* analysis, const int& _debugLevel);

    // set the pointer of AbstractObjectMap at this PartEdge
    void setProductLattice();

    bool finish();

    // lattice access functions from the map (product lattice)
    // copied from VariableStateTransfer.h
    AbstractObjectSetPtr getLattice(SgExpression* sgexp);
    AbstractObjectSetPtr getLatticeOperand(SgNode* sgn, SgExpression* operand);
    AbstractObjectSetPtr getLatticeCommon(MemLocObjectPtr ml);
    AbstractObjectSetPtr getLattice(const AbstractObjectPtr o);
    void setLattice(SgExpression* sgexp, AbstractObjectSetPtr aos);
    void setLatticeOperand(SgNode* sgn, SgExpression* operand, AbstractObjectSetPtr aos);
    void setLatticeCommon(MemLocObjectPtr ml, AbstractObjectSetPtr aos);
    void setLattice(const AbstractObjectPtr o, AbstractObjectSetPtr aos);

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

  // helper function to copy elements from abstract object set
  void copyAbstractObjectSet(const AbstractObjectSet& aos, std::list<MemLocObjectPtr>& list);

  // Object returned by PointsToAnalysis::Expr2MemLoc
  // Wraps object returned by the composer
  class PointsToML;
  typedef boost::shared_ptr<PointsToML> PointsToMLPtr;
  class PointsToML : public UnionMemLocObject
  {
    // NOTE: UnionMemLocObject is useful to store list
    // of items a memory object may point to

  public:
    PointsToML(MemLocObjectPtr _mem) : MemLocObject(NULL), UnionMemLocObject(_mem) { }
    PointsToML(std::list<MemLocObjectPtr> _lml) : MemLocObject(NULL), UnionMemLocObject(_lml) { }       
        
    std::string str(std::string indent)
    {
      std::ostringstream oss;
      oss << "[PointsToML: " << UnionMemLocObject::str(indent) << "]";
      return oss.str();
    }
  };
};

#endif
