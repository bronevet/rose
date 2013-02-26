#include "pointsToAnalysis.h"

using namespace dataflow;

PointsToAnalysisTransfer::PointsToAnalysisTransfer(const Function& func, PartPtr part,
                                                   CFGNode cn, NodeState& state,
                                                   std::map<PartEdgePtr, std::vector<Lattice*> >& dfInfo,
                                                   Composer* _composer, PointsToAnalysis* _analysis)
  :IntraDFTransferVisitor(func, part, cn, state, dfInfo),
   composer(_composer),
   analysis(_analysis),
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
  AbstractObjectSetPtr _aoset = boost::dynamic_pointer_cast<AbstractObjectSet> (productLattice->get(o));
  ROSE_ASSERT(_aoset);
  Dbg::dbg << "getLattice(o): o=" << o->strp(part->inEdgeFromAny()) << ", lattice=" << _aoset->strp(part->inEdgeFromAny()) << "\n";
  return _aoset;
}

void PointsToAnalysisTransfer::visit(SgAssignOp* sgn)
{
  Dbg::dbg << "Transfer Function: " << sgn->unparseToString() << std::endl;
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
  PointsToAnalysisTransfer* idftv = new PointsToAnalysisTransfer (func, _part, cn, state, dfInfo, getComposer(), this);
  return boost::shared_ptr<IntraDFTransferVisitor>(idftv);
}

std::string PointsToAnalysis::str(std::string indent="")
{ 
  return "PointsToAnalysis"; 
}


MemLocObjectPtr PointsToAnalysis::Expr2MemLoc(SgNode* sgn, PartEdgePtr pedge)
{
  return getComposer()->Expr2MemLoc(sgn, pedge, this);
}
