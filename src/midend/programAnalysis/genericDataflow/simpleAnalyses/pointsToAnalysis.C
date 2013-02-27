#include "pointsToAnalysis.h"

namespace dataflow
{
  int ptaDebugLevel = 2;

  PointsToAnalysisTransfer::PointsToAnalysisTransfer(const Function& func, PartPtr part,
                                                     CFGNode cn, NodeState& state,
                                                     std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo,
                                                     Composer* _composer, PointsToAnalysis* _analysis, const int& _debugLevel)
    :IntraDFTransferVisitor(func, part, cn, state, dfInfo),
     composer(_composer),
     analysis(_analysis),
     debugLevel(_debugLevel),
     modified(false)
  {
    // set the pointer of the map for this PartEdge
    setProductLattice();
  }

  bool PointsToAnalysisTransfer::finish()
  {
    return modified;
  }

  void PointsToAnalysisTransfer::setProductLattice()
  { 
    // copied from VariableStateTransfer.h
    ROSE_ASSERT(dfInfo.size()==1);
    ROSE_ASSERT(dfInfo[NULLPartEdge].size()==1);
    ROSE_ASSERT(*dfInfo[NULLPartEdge].begin());
    Lattice *l = *dfInfo[NULLPartEdge].begin();
    productLattice = (dynamic_cast<AbstractObjectMap*>(l));
    ROSE_ASSERT(productLattice);
  }

  // NOTE: Should we use pedge instead of part->inEdgeFromAny()
  PointsToAnalysisTransfer::AbstractObjectSetPtr 
  PointsToAnalysisTransfer::getLattice(SgExpression* sgexp) 
  {
    MemLocObjectPtr ml= composer->Expr2MemLoc(sgexp, part->inEdgeFromAny(), analysis);
    return getLatticeCommon(ml);
  }

  // NOTE: Should we use pedge instead of part->inEdgeFromAny()
  PointsToAnalysisTransfer::AbstractObjectSetPtr 
  PointsToAnalysisTransfer::getLatticeOperand(SgNode* sgn, SgExpression* operand) 
  { 
    MemLocObjectPtr oml = composer->OperandExpr2MemLoc(sgn, operand, part->inEdgeFromAny(), analysis); 
    return getLatticeCommon(oml);
  }

  PointsToAnalysisTransfer::AbstractObjectSetPtr 
  PointsToAnalysisTransfer::getLatticeCommon(MemLocObjectPtr p)
  {
    return getLattice(AbstractObjectPtr(p));
  }

  PointsToAnalysisTransfer::AbstractObjectSetPtr 
  PointsToAnalysisTransfer::getLattice(const AbstractObjectPtr o)
  {
    AbstractObjectSetPtr _aos = boost::dynamic_pointer_cast<AbstractObjectSet> (productLattice->get(o));
    ROSE_ASSERT(_aos);
    Dbg::dbg << "getLattice(o): o=" << o->strp(part->inEdgeFromAny()) << ", lattice=" << _aos->strp(part->inEdgeFromAny()) << "\n";
    return _aos;
  }

  void PointsToAnalysisTransfer::setLattice(SgExpression* sgexp,
                                            PointsToAnalysisTransfer::AbstractObjectSetPtr aos)
  {
    MemLocObjectPtr ml = composer->Expr2MemLoc(sgexp, part->inEdgeFromAny(), analysis);
    setLatticeCommon(ml, aos);
  }

  void PointsToAnalysisTransfer::setLatticeOperand(SgNode* sgn, SgExpression* operand,
                                                   PointsToAnalysisTransfer::AbstractObjectSetPtr aos)
  {
    MemLocObjectPtr ml = composer->OperandExpr2MemLoc(sgn, operand, part->inEdgeFromAny(), analysis);
    setLatticeCommon(ml, aos);
  }

  void PointsToAnalysisTransfer::setLatticeCommon(MemLocObjectPtr ml,
                                                  PointsToAnalysisTransfer::AbstractObjectSetPtr aos)
  {
    setLattice(AbstractObjectPtr(ml), aos);
  }

  void PointsToAnalysisTransfer::setLattice(const AbstractObjectPtr o,
                                            PointsToAnalysisTransfer::AbstractObjectSetPtr aos)
  {
    modified = modified || productLattice->insert(o, aos);
    if(debugLevel >= 2) {
      Dbg::dbg << productLattice->strp(part->inEdgeFromAny());
    }
  }

  // very trivial transfer function
  // NOTE: requires extension for a full blown analysis
  void PointsToAnalysisTransfer::visit(SgAssignOp* sgn)
  {
    SgExpression* rhs_operand = sgn->get_rhs_operand();
    SgExpression* lhs_operand = sgn->get_lhs_operand();
    // handle p = &x
    // NOTE: rhs can be a complex expression but code below only handles the trivial case
    if(isSgAddressOfOp(rhs_operand)) 
    {
      // operand of SgAddressOfOp should be a variable
      SgVarRefExp* sgvexp = isSgVarRefExp(isSgAddressOfOp(rhs_operand)->get_operand()); assert(sgvexp);    
      MemLocObjectPtr _ml = composer->OperandExpr2MemLoc(rhs_operand, sgvexp, part->inEdgeFromAny(), analysis); assert(_ml);

      // get the lattice for lhs_operand
      // insert memory object for rhs_operand into this lattice
      AbstractObjectSetPtr aos = getLatticeOperand(sgn, lhs_operand);
      aos->insert(_ml);
      // update the product lattice
      setLatticeOperand(sgn, lhs_operand, aos);
      modified = true;
    }
    //TODO: handle p = q
  }

  void PointsToAnalysis::genInitLattice(const Function& func, PartPtr part, PartEdgePtr pedge,
                                        std::vector<Lattice*>& initLattices)
  {
    AbstractObjectMap* productlattice = new AbstractObjectMap(new MayEqualFunctor(),
                                                              boost::make_shared<AbstractObjectSet>(pedge, AbstractObjectSet::may),
                                                              pedge);
    initLattices.push_back(productlattice);                                                                                                  
  }


  boost::shared_ptr<IntraDFTransferVisitor>
  PointsToAnalysis::getTransferVisitor(const Function& func, PartPtr _part, CFGNode cn, NodeState& state, 
                                       std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo)                                     
  {
    PointsToAnalysisTransfer* idftv = new PointsToAnalysisTransfer (func, _part, cn, state, dfInfo, getComposer(), this, ptaDebugLevel);
    return boost::shared_ptr<IntraDFTransferVisitor>(idftv);
  }

  std::string PointsToAnalysis::str(std::string indent="")
  { 
    return "PointsToAnalysis"; 
  }

  void copyAbstractObjectSet(const AbstractObjectSet& aos, std::list<MemLocObjectPtr>& list)
  {
    for(AbstractObjectSet::const_iterator it = aos.begin(); it != aos.end(); it++)
    {
      list.push_back(boost::dynamic_pointer_cast<MemLocObject> (*it));
    }
  }

  MemLocObjectPtr PointsToAnalysis::Expr2MemLoc(SgNode* sgn, PartEdgePtr pedge)
  {
    Dbg::dbg << "PointsToAnalysis::Expr2MemLoc(sgn=" << 
      cfgUtils::SgNode2Str(sgn) << ", pedge=" << pedge->str() << ",this=" << this << std::endl;

    // NOTE: source and target of edge are not wildcard
    if(pedge->source() && pedge->target())
    {
      NodeState* state = NodeState::getNodeState(this, pedge->source());
      Dbg::dbg << "state="<<state->str(this)<<endl;
      AbstractObjectMap* aom = dynamic_cast<AbstractObjectMap*>(state->getLatticeBelow(this, pedge, 0));
      ROSE_ASSERT(aom);
      // handle single dereferencing
      if(isSgPointerDerefExp(sgn))
      {
        if(isSgVarRefExp(isSgPointerDerefExp(sgn)->get_operand())) {
          MemLocObjectPtr oml = composer->OperandExpr2MemLoc(sgn, isSgPointerDerefExp(sgn)->get_operand(), pedge, this);
          // get set of items this pointer points to
          boost::shared_ptr<AbstractObjectSet> aos = boost::dynamic_pointer_cast<AbstractObjectSet> (aom->get(oml)); ROSE_ASSERT(aos.get());
          std::list<MemLocObjectPtr> pointsToSet;
          copyAbstractObjectSet(*aos, pointsToSet);
          PointsToMLPtr ptmlp = boost::make_shared<PointsToML> (pointsToSet);
          return boost::dynamic_pointer_cast<MemLocObject> (ptmlp);
        }
        else { assert(false); return PointsToMLPtr(); } // operand of derefexp is complicated and not handled       
      }          
      // handle pointer types
      else if(isSgVarRefExp(sgn))
      {
        // TODO: handle pointer type
        MemLocObjectPtr ml = composer->Expr2MemLoc(sgn, pedge, this);
        PointsToMLPtr ptmlp = boost::make_shared<PointsToML> (ml);
        return boost::dynamic_pointer_cast<MemLocObject> (ptmlp);
      }
      // wrap other objects
      else
      {
        MemLocObjectPtr ml = composer->Expr2MemLoc(sgn, pedge, this);
        PointsToMLPtr ptmlp = boost::make_shared<PointsToML> (ml);
        return boost::dynamic_pointer_cast<MemLocObject> (ptmlp);
      }
    }
    // TODO: handle if target of this edge is wildcard
    // NOTE: merge information across all outgoing edges
    else if(pedge->source()) { assert(false); return PointsToMLPtr(); }
    // TODO: handle if source of this edge is wildcard
    else if(pedge->target()) { assert(false); return PointsToMLPtr(); }
    else { assert(false); return PointsToMLPtr(); }
  }
};
