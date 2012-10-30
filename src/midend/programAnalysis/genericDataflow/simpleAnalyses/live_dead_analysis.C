#include "live_dead_analysis.h"
#include "compose.h"
#include <boost/make_shared.hpp>

namespace dataflow {
int liveDeadAnalysisDebugLevel=1;

// ###############################
// ##### LiveDeadMemAnalysis #####
// ###############################

LiveDeadMemAnalysis::LiveDeadMemAnalysis(funcSideEffectUses* fseu): 
fseu(fseu)
{
}

// Initializes the state of analysis lattices at the given function, part and edge into our out of the part
// by setting initLattices to refer to freshly-allocated Lattice objects.
void LiveDeadMemAnalysis::genInitLattice(const Function& func, PartPtr part, PartEdgePtr pedge, 
                                         std::vector<Lattice*>& initLattices)
{
  AbstractObjectSet* s = new AbstractObjectSet(part->outEdgeToAny(), AbstractObjectSet::may);
  
  // If this part is the return statement of main(), make sure that its return value is live
  //if(func.get_name().getString() == "main" && 
  SgReturnStmt* returnStmt;
  if(func.get_declaration() == SageInterface::findMain(SageInterface::getGlobalScope(func.get_declaration())) && 
     (returnStmt = part->maySgNodeAny<SgReturnStmt>())) {
    // Get the memory location of the return statement's operand. Note that although this is a backward
    // analysis, the edge passed to OperandExpr2MemLoc() is the edge that comes into part. This is because
    // this edge denotes the set of executions that terminates at the return statement part and executions
    // always run forwards.
    MemLocObjectPtrPair p(composer->OperandExpr2MemLoc(returnStmt, returnStmt->get_expression(), part->inEdgeFromAny(), this));
    s->insert(p.expr? p.expr : p.mem);
  }
  
  initLattices.push_back(s);
}

/// Visits live expressions - helper to LiveDeadVarsTransfer
class LDMAExpressionTransfer : public ROSE_VisitorPatternDefaultBase
{
    LiveDeadMemTransfer &ldmt;

public:
    // Should only be called on expressions
    void visit(SgNode *) { assert(0); }
    // Catch up any other expressions that are not yet handled
    void visit(SgExpression * expr)
    {
        // Function Reference
        // !!! CURRENTLY WE HAVE NO NOTION OF VARIABLES THAT IDENTIFY FUNCTIONS, SO THIS CASE IS EXCLUDED FOR NOW
        /*} else if(isSgFunctionRefExp(sgn)) {*/
        /*} else if(isSgMemberFunctionRefExp(sgn)) {*/

        // !!! DON'T KNOW HOW TO HANDLE THESE
        /*} else if(isSgStatementExpression(sgn)) {(*/

        // Typeid
        // !!! DON'T KNOW WHAT TO DO HERE SINCE THE RETURN VALUE IS A TYPE AND THE ARGUMENT'S VALUE IS NOT USED
        /*} else if(isSgTypeIdOp(sgn)) {*/
        // Var Args
        // !!! DON'T HANDLE THESE RIGHT NOW. WILL HAVE TO IN THE FUTURE
        /*  SgVarArgOp 
            SgExpression *  get_operand_expr () const 
            SgVarArgCopyOp
            SgExpression *  get_lhs_operand () const
            SgExpression *  get_rhs_operand () const  
            SgVarArgEndOp 
            SgExpression *  get_operand_expr 
            SgVarArgStartOneOperandOp 
            SgExpression *  get_operand_expr () const 
            SgVarArgStartOp 
            SgExpression *  get_lhs_operand () const
            SgExpression *  get_rhs_operand () const */
        // !!! WHAT IS THIS?
        /*  SgVariantExpression*/


        // TODO: Make this assert(0), because unhandled expression types are likely to give wrong results
      ROSE_ASSERT(0);
    }
    // Plain assignment: lhs = rhs
    void visit(SgAssignOp *sgn) {
      ldmt.assign(sgn, sgn->get_lhs_operand());
      
      // If this expression or the value it writes are live, make the rhs live
      if(ldmt.isMemLocLive(sgn) || ldmt.isMemLocLive(sgn, sgn->get_lhs_operand())) {
        // If the lhs of the assignment is a complex expression (i.e. it refers to a variable that may be live) OR
        // if is a known expression that is known to may-be-live
        // THIS CODE ONLY APPLIES TO RHSs THAT ARE SIDE-EFFECT-FREE AND WE DON'T HAVE AN ANALYSIS FOR THAT YET
        /*if(!isVarExpr(sgn->get_lhs_operand()) || 
          (isVarExpr(sgn->get_lhs_operand()) && 
          liveLat->isLiveVar(SgExpr2Var(sgn->get_lhs_operand()))))
          { */
        ldmt.use(sgn, sgn->get_rhs_operand());
      }
    }
    // Initializer for a variable
    void visit(SgAssignInitializer *sgn) {
      // Only make the operand(s) live if the expression is live
      if(!ldmt.isMemLocLive(sgn)) return;
      
      Dbg::dbg << "LDMAExpressionTransfer::visit(SgAssignInitializer *sgn)"<<endl;
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;sgn="<<cfgUtils::SgNode2Str(sgn)<<endl;
      Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;sgn->get_operand()="<<cfgUtils::SgNode2Str(sgn->get_operand())<<endl;
      ldmt.use(sgn, sgn->get_operand());
    }
    // Initializer for a function arguments
    void visit(SgConstructorInitializer *sgn) {
      // Only make the operand(s) live if the expression is live
      if(!ldmt.isMemLocLive(sgn)) return;
      
      SgExprListExp* exprList = sgn->get_args();
      for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
          expr!=exprList->get_expressions().end(); expr++)
        ldmt.use(sgn, *expr);
    }
    // Initializer that captures internal stucture of structs or arrays ("int x[2] = {1,2};", it is the "1,2")
    // NOTE: Should this use abstractMemory interface ?
    void visit(SgAggregateInitializer *sgn) {
      // Only make the operand(s) live if the expression is live
      if(!ldmt.isMemLocLive(sgn)) return;
      
      SgExprListExp* exprList = sgn->get_initializers();
      for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
          expr!=exprList->get_expressions().end(); expr++)
        ldmt.use(sgn, *expr);
    }
    // Designated Initializer 
    void visit(SgDesignatedInitializer *sgn) {
      // Only make the operand(s) live if the expression is live
      if(!ldmt.isMemLocLive(sgn)) return;
      
      SgExprListExp* exprList = sgn->get_designatorList();
      for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
          expr!=exprList->get_expressions().end(); expr++)
        ldmt.use(sgn, *expr);
    }
    // Variable access
    void visit(SgVarRefExp *sgn) {
//  Liao, 4/5/2012. We cannot decide if a SgVarRefExp is read or written
//    without its context information: for example, in  a = b; both a and b are represented as
//    SgVarRefExp. But a is written and b is read.
//    We should let the ancestor node (like SgAssignOp) decide on the READ/Written of SgVarRefExp.
//  GB 2012-09-04: We don't need to care about read/write access for this because the only way that a
//    variable reference can be live is if it is used, not assigned by its parent expression.
      
      //ldmt.useMem(sgn);
    }
    void visit(SgFunctionRefExp* sgn) {
      // TODO: This should be handled when we properly support CodeLocObjects
    }
    // Array access
    void visit(SgPntrArrRefExp *sgn) {
      if(ldmt.isMemLocLive(sgn)) {
        Dbg::dbg << "visit(SgPntrArrRefExp *sgn)"<<endl;
        // The only way for this SgPntrArrRefExp to appear s if it is used by its parent expression
        ldmt.useMem(sgn);
        // Both the lhs and rhs are used to identify the memory location being accessed
        Dbg::dbg << "LHS"<<endl;
        ldmt.use(sgn, sgn->get_lhs_operand());
        Dbg::dbg << "RHS"<<endl;
        ldmt.use(sgn, sgn->get_rhs_operand());
      }
    }
    // Binary Operations
    void visit(SgBinaryOp *sgn) {
      /* GB 2012-09-05 : The liveness of the operand is not changed as a result of an auto-update
      // Self-update expressions, where the lhs is assigned
      if(isSgCompoundAssignOp(sgn))
        ldmt.assign(sgn, sgn->get_lhs_operand());*/
      
      Dbg::dbg << "LiveDead: visit("<<cfgUtils::SgNode2Str(sgn)<<") ldmt.isMemLocLive(sgn)="<<ldmt.isMemLocLive(sgn)<<endl;
      
      // If this expression is live or writes writes to a live memory location, make the operands live
      if(ldmt.isMemLocLive(sgn) || (isSgCompoundAssignOp(sgn) && ldmt.isMemLocLive(sgn, sgn->get_lhs_operand()))) {
        // Both the lhs and rhs are used
        ldmt.use(sgn, sgn->get_lhs_operand());
        ldmt.use(sgn, sgn->get_rhs_operand());
      }
    }
    // Unary Operations
    void visit(SgUnaryOp *sgn) {
      /* GB 2012-09-05 : The liveness of the operand is not changed as a result of an auto-update
       * // If this is an auto-update operation
      if(isSgMinusMinusOp(sgn) || isSgPlusPlusOp(sgn)) {
        // The argument is defined
        ldmt.assign(sgn, sgn->get_operand());
      }*/
        
        // If this expression is live or writes writes to a live memory location, make the operands live
      if(ldmt.isMemLocLive(sgn) || 
         ((isSgMinusMinusOp(sgn) || isSgPlusPlusOp(sgn)) && ldmt.isMemLocLive(sgn, sgn->get_operand())))
        // The argument is used
        ldmt.use(sgn, sgn->get_operand());
    }
    
    // Conditionals (condE ? trueE : falseE)
    void visit(SgConditionalExp *sgn) {
      // Only make the operand(s) live if the expression is live
      if(!ldmt.isMemLocLive(sgn)) return;
      
      // The arguments are used
      ldmt.use(sgn, sgn->get_conditional_exp());
      ldmt.use(sgn, sgn->get_true_exp());
      ldmt.use(sgn, sgn->get_false_exp());
    }
    // Delete
    void visit(SgDeleteExp *sgn) {
      // GB: 2012-09-05 - I don't think we need to process these in live-dead because a deleted memory location
      //      definitely cannot be used afterwards and must be dead at deletion time.
      
      // Delete expressions return nothing
      // The arguments are used
      //ldmt.use(sgn, sgn->get_variable());
    }
    // New
    void visit(SgNewExp *sgn) {
      // Only make the operand(s) live if the expression is live
      if(!ldmt.isMemLocLive(sgn)) return;
      
      // The placement arguments are used
      SgExprListExp* exprList = sgn->get_placement_args();
      // NOTE: placement args are optional
      // exprList could be NULL
      // check for NULL before adding to used set
      if(exprList) {
        for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
          expr!=exprList->get_expressions().end(); expr++)
          ldmt.use(sgn, *expr);
      }
                              
      // The placement arguments are used
      // check for NULL before adding to used set
      // not sure if this check is required for get_constructor_args()
      exprList = sgn->get_constructor_args()->get_args();
      if(exprList) {
        for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
          expr!=exprList->get_expressions().end(); expr++)
          ldmt.use(sgn, *expr);
      }
                              
      // The built-in arguments are used (DON'T KNOW WHAT THESE ARE!)
      // check for NULL before adding to used set
      // not sure if this check is required for get_builtin_args()
      if(sgn->get_builtin_args()) {
        ldmt.use(sgn, sgn->get_builtin_args());
      }
    }
    // Function Calls
    void visit(SgFunctionCallExp *sgn) {
      // Function calls and their side-effects are processed by inter-procedural analyses. Here we only need
      // to worry about the variable that identifies the function being called.
      
      // !!! CURRENTLY WE HAVE NO NOTION OF VARIABLES THAT IDENTIFY FUNCTIONS, SO THIS CASE IS EXCLUDED FOR NOW
      /*// The expression that identifies the called function is used
        ldmt.use(sgn, sgn->get_function());*/
                              
      /*// The function call's arguments are used
      SgExprListExp* exprList = sgn->get_args();
      for(SgExpressionPtrList::iterator expr=exprList->get_expressions().begin();
          expr!=exprList->get_expressions().end(); expr++)
          ldmt.use(sgn, *expr);
                              
      // If this function has no definition and the user provided a class to provide 
      // the variables that are used by such functions
      if(sgn->getAssociatedFunctionDeclaration() && 
         sgn->getAssociatedFunctionDeclaration()->get_definition()==NULL &&
         ldmt.fseu) {
          set<MemLocObjectPtr> funcUsedVars = ldmt.fseu->usedVarsInFunc(Function(sgn->getAssociatedFunctionDeclaration()), ldmt.part, ldmt.nodeState);
          //ldmt.use(sgn, funcUsedVars.begin(), funcUsedVars.end());
          for(set<MemLocObjectPtr>::iterator used=funcUsedVars.begin(); used!=funcUsedVars.end(); used++)
          ldmt.use(sgn, *used);
      }*/
    }
    // Sizeof
    void visit(SgSizeOfOp *sgn) {
      // Only make the operand(s) live if the expression is live
      if(!ldmt.isMemLocLive(sgn)) return;
      
      // XXX: The argument is NOT used, but its type is
      // NOTE: get_operand_expr() returns NULL when sizeof(type)
      // FIX: use get_operand_expr() only when sizeof() involves expr
      // ldmt.use(sgn, sgn->get_operand_expr());
      if(sgn->get_operand_expr()) {
        ldmt.use(sgn, sgn->get_operand_expr());
      }
    }
    // This
    void visit(SgThisExp *sgn) {}
    // Literal values
    void visit(SgValueExp* sgn) {}
    
    LDMAExpressionTransfer(LiveDeadMemTransfer &ldmt)
    : ldmt(ldmt)
    { }
};

// Note that the variable corresponding to this expression is assigned
void LiveDeadMemTransfer::assign(SgNode *sgn, SgExpression* operand)
{
  //Dbg::dbg << "LiveDeadMemTransfer::assign(sgn="<<cfgUtils::SgNode2Str(sgn)<<" operand="<<cfgUtils::SgNode2Str(operand)<<endl;
  MemLocObjectPtrPair p(composer->OperandExpr2MemLoc(sgn, operand, part->outEdgeToAny(), ldma)/*ceml->Expr2Obj(sgn)*/);
  //return boost::dynamic_pointer_cast<AbstractObject>(cpMap->get(ml));
  if(p.expr) assigned.insert(p.expr);
  // GB 2012-09-05 : Need this to handle lhs of SgAssignOps where the lhs is a memory location.
  if(p.mem) assigned.insert(p.mem);
}
/*void LiveDeadMemTransfer::assign(AbstractObjectPtr mem)
{
    assigned.insert(mem);
}*/

// Note that the variable corresponding to this expression is used
void LiveDeadMemTransfer::use(SgNode *sgn, SgExpression* operand)
{
  Dbg::dbg << "part->outEdgeToAny()="<<part->outEdgeToAny()->str()<<endl;
  MemLocObjectPtrPair p = composer->OperandExpr2MemLoc(sgn, operand, part->outEdgeToAny(), ldma);//ceml->Expr2Obj(sgn);
  Dbg::dbg << "LiveDeadMemTransfer::use(sgn=["<<Dbg::escape(sgn->unparseToString())<<" | "<<sgn->class_name()<<"]"<<endl;
  Dbg::dbg << "p="<<p.str()<<endl;
  // In almost all cases we only need expressions to use their operands, which are also expressions.
  if(p.expr) used.insert(p.expr);
  // At statement boundaries SgVarRefExp and SgArrPntrRefExp refer to real memory locations that were written by prior
  // statements. 
  if((isSgVarRefExp(operand) || isSgPntrArrRefExp(operand)) && p.mem)  used.insert(p.mem);
}
// Note that the memory location denoted by the corresponding SgVarRefExp is used
void LiveDeadMemTransfer::useMem(SgVarRefExp* sgn)
{
  MemLocObjectPtrPair p = composer->Expr2MemLoc(sgn, part->outEdgeToAny(), ldma);//ceml->Expr2Obj(sgn);
  Dbg::dbg << "LiveDeadMemTransfer::useMem(SgVarRefExp)(sgn=["<<Dbg::escape(sgn->unparseToString())<<" | "<<sgn->class_name()<<"]"<<endl;
  used.insert(p.mem);
}
// Note that the memory location denoted by the corresponding SgPntrArrRefExp is used
void LiveDeadMemTransfer::useMem(SgPntrArrRefExp* sgn)
{
  MemLocObjectPtrPair p = composer->Expr2MemLoc(sgn, part->outEdgeToAny(), ldma);//ceml->Expr2Obj(sgn);
  Dbg::dbg << "LiveDeadMemTransfer::useMem(SgPntrArrRefExp)(sgn=["<<Dbg::escape(sgn->unparseToString())<<" | "<<sgn->class_name()<<"]"<<endl;
  // If a memory object is available, insert it. Not all SgPntrArrRefExps correspond to a real memory location.
  // e.g. in array2d[a][b] the expression array2D[a] doesn't denote a memory location.
  if(p.mem) used.insert(p.mem);
}

/*void LiveDeadMemTransfer::use(AbstractObjectPtr mem)
{
    used.insert(mem);
}*/

// Returns true if the given expression is currently live and false otherwise
bool LiveDeadMemTransfer::isMemLocLive(SgExpression* sgn, SgExpression* operand) {
  MemLocObjectPtrPair p = composer->OperandExpr2MemLoc(sgn, operand, part->outEdgeToAny(), ldma);//ceml->Expr2Obj(expr);
  return (p.expr ? liveLat->containsMay(p.expr) : false) ||
         (p.mem  ? liveLat->containsMay(p.mem)  : false);
}

// Returns true if the given expression is currently live and false otherwise
bool LiveDeadMemTransfer::isMemLocLive(SgExpression* sgn) {
  MemLocObjectPtrPair p = composer->Expr2MemLoc(sgn, part->outEdgeToAny(), ldma);//ceml->Expr2Obj(expr);
  return (p.expr ? liveLat->containsMay(p.expr) : false) ||
         (p.mem  ? liveLat->containsMay(p.mem)  : false);
}

void LiveDeadMemTransfer::visit(SgExpression *sgn)
{
  //AbstractMemoryObject::ObjSet* objset = SgExpr2ObjSet(sgn);
  MemLocObjectPtrPair p = composer->Expr2MemLoc(sgn, part->outEdgeToAny(), ldma);//ceml->Expr2Obj(sgn);
  /**/
  LDMAExpressionTransfer helper(*this);
  sgn->accept(helper);
  
  // Remove the expression object itself since it has no uses above itself.
  // Do not remove the memory location object since it has just been used.
  /*if(!isSgVarRefExp(sgn) && !isSgPntrArrRefExp(sgn)) // Liao 4/5/2012, we should not remove SgVarRef or SgPntrArrRefExp since it may have uses above itself
  {
    //if(liveDeadAnalysisDebugLevel>=1) Dbg::dbg << "   Removing "<< mem.str("         ") <<endl;
    //modified = liveLat->remove(mem) || modified;
    if(assigned.insert(mem); /// ????
  }*/
  if(p.expr) assigned.insert(p.expr);
}

void LiveDeadMemTransfer::visit(SgInitializedName *sgn) {
  /*SgVarRefExp* exp = SageBuilder::buildVarRefExp(sgn);
  Dbg::dbg << "LiveDeadMemTransfer::visit(SgInitializedName: sgn=["<<Dbg::escape(sgn->unparseToString())<<" | "<<sgn->class_name()<<"]"<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;exp="<<exp<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;exp=["<<Dbg::escape(exp->unparseToString())<<" | "<<exp->class_name()<<"]"<<endl;*/
  MemLocObjectPtrPair p = composer->Expr2MemLoc(sgn, part->outEdgeToAny(), ldma); //ceml->Expr2Obj(exp);
  ROSE_ASSERT(p.mem);
  bool isLive = (p.mem  ? liveLat->containsMay(p.mem)  : false);
  /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;isLive="<<isLive<<endl;
  if(liveDeadAnalysisDebugLevel>=1)
    Dbg::dbg << indent << (isLive ? "Live Expression" : "Dead Expression") <<endl;*/

  // If this is a live variable and this is the instance of SgInitializedName that occurs immediately after 
  // the declaration's initializer AND this declaration has an initializer, add it as a use
  if(isLive && sgn->get_initializer())
    use(sgn, sgn->get_initializer());
  //if(p.expr) assigned.insert(p.expr);
  //if(p.mem)  assigned.insert(p.mem);
  assigned.insert(p.mem);
}

void LiveDeadMemTransfer::visit(SgReturnStmt *sgn) {
/*  
  WE CURRENTLY ASSUME THAT THE EXPRESSION OF A RETURN STATEMENT IS ALWAYS USED
  SHOULD ONLY ASSUME THIS FOR RETURN STA```TEMENT OF main()*/
  
  use(sgn, sgn->get_expression());
}
void LiveDeadMemTransfer::visit(SgExprStatement *sgn) {
    /*use(sgn, sgn->get_expression());*/
}
void LiveDeadMemTransfer::visit(SgSwitchStatement *sgn) {
  // The operand of a control-flow statement is automatically live only
  // if the control-flow statement leads to multiple control options
  // This is always true for switch statements since they can always skip all the cases.
  // However, in the future may want to check for whether the value of the switch is a constant.
  // GB 2012-09-19 : Which object corresponds to the expression that chooses the case?
  ROSE_ASSERT(0);
  //use(sgn, sgn->get_item_selector());
}
void LiveDeadMemTransfer::visit(SgCaseOptionStmt *sgn) {
  use(sgn, sgn->get_key());
  use(sgn, sgn->get_key_range_end());
}
void LiveDeadMemTransfer::visit(SgIfStmt *sgn) {
  // The operand of a control-flow statement is automatically live only
  // if the control-flow statement leads to multiple control options
  // This is always true for if statements since there's a difference between the true body executing and not executing
  // even if there is no false body.
  // However, in the future may want to check for whether the value of the switch is a constant.
  
  ROSE_ASSERT(isSgExprStatement(sgn->get_conditional()));
  use(sgn, isSgExprStatement(sgn->get_conditional())->get_expression());
}

void LiveDeadMemTransfer::visit(SgForStatement *sgn) {
  //Dbg::dbg << "test="<<Dbg::escape(sgn->get_test()->unparseToString()) << " | " << sgn->get_test()->class_name()<<endl;
  //Dbg::dbg << "increment="<<Dbg::escape(sgn->get_increment()->unparseToString()) << " | " << sgn->get_increment()->class_name()<<endl;
  
  // The operands of a for statement are automatically live since loops have an unknown
  // number of iterations that is decided based on these operands
  ROSE_ASSERT(isSgExprStatement(sgn->get_test()));
  use(sgn, isSgExprStatement(sgn->get_test())->get_expression());
  use(sgn, sgn->get_increment());
}
void LiveDeadMemTransfer::visit(SgWhileStmt *sgn) {
  // The operands of a while statement are automatically live since loops have an unknown
  // number of iterations that is decided based on these operands
  ROSE_ASSERT(isSgExprStatement(sgn->get_condition()));
  //Dbg::dbg << "condition="<<Dbg::escape(sgn->get_condition()->unparseToString()) << " | " << sgn->get_condition()->class_name()<<endl;
  use(sgn, isSgExprStatement(sgn->get_condition())->get_expression());
}
void LiveDeadMemTransfer::visit(SgDoWhileStmt *sgn) {
  // The operands of a do-while statement are automatically live since loops have an unknown
  // number of iterations that is decided based on these operands
  ROSE_ASSERT(isSgExprStatement(sgn->get_condition()));
  //Dbg::dbg << "condition="<<Dbg::escape(sgn->get_condition()->unparseToString()) << " | " << sgn->get_condition()->class_name()<<endl;
  use(sgn, isSgExprStatement(sgn->get_condition())->get_expression());
}

bool LiveDeadMemTransfer::finish()
{
  Dbg::indent ind(analysisDebugLevel, 1);
  // First process assignments, then uses since we may assign and use the same variable
  // and in the end we want to first remove it and then re-insert it.   
  if(liveDeadAnalysisDebugLevel>=1) {
    
    Dbg::dbg << "used=[";
    { Dbg::indent ind;
    for(AbstractObjectSet::const_iterator asgn=used.begin(); asgn!=used.end(); ) {
      Dbg::indent ind(analysisDebugLevel, 1);
      Dbg::dbg << (*asgn)->str();
      asgn++;
      if(asgn==used.end()) Dbg::dbg << "]";
      else                 Dbg::dbg << endl;
    }
    Dbg::dbg << endl; }
    
    Dbg::dbg << "assigned=[";
    { Dbg::indent ind;
    for(AbstractObjectSet::const_iterator asgn=assigned.begin(); asgn!=assigned.end(); ) {
      Dbg::indent ind(analysisDebugLevel, 1);
      Dbg::dbg << (*asgn)->str() << endl;
      asgn++;
      if(asgn==used.end()) Dbg::dbg << "]";
      else                 Dbg::dbg << endl;
    }
    Dbg::dbg << endl; }
    
    Dbg::dbg << "liveLat=";
    {Dbg::indent ind(analysisDebugLevel, 1);
     Dbg::dbg << liveLat->str("")<<endl;}
  }

  /* Live-In (node) = use(node) + (Live-Out (node) - Assigned (b))  
   * Live-Out (node) is the lattice after merging ???
   * */         
  // Record for each assigned expression:
  //    If the expression corresponds to a variable, record that the variable is dead.
  //    Otherwise, record that the expression that computes the assigned memory location is live
  for(AbstractObjectSet::const_iterator asgn=assigned.begin(); asgn!=assigned.end(); asgn++) {
    // If the lhs is a variable reference, remove it from live variables unless we also use this variable
    //if(isVarExpr(*asgn)) {
    // if(usedVars.find(SgExpr2Var(*asgn)) != usedVars.end()) // found in use?  Wrong condition!!
    if(!used.containsMay(*asgn)) // if not found in use, then remove it, Liao 4/5/2012
    {
      // if(liveDeadAnalysisDebugLevel>=1) {
      //     Dbg::dbg << indent << "    removing assigned expr <" << (*asgn)->class_name() <<":"<<(*asgn)->unparseToString();
      //     Dbg::dbg << ">"<<endl;
      // }
      modified = liveLat->remove(*asgn) || modified; 
    }
    else 
    {
      modified = liveLat->insert(*asgn) || modified;
      // if(liveDeadAnalysisDebugLevel>=1) {
      //     Dbg::dbg << indent << "    add assigned expr as live <" << (*asgn)->class_name() <<":"<<(*asgn)->unparseToString();
      // }
    }
  } // end for

  // Record that the used variables are live
  for(AbstractObjectSet::const_iterator var=used.begin(); var!=used.end(); var++)
    modified = liveLat->insert(*var) || modified;
  
  return modified;
}

/*******************************
 ***** LiveDeadMemAnalysis *****
 *******************************/

// Maps the given SgNode to an implementation of the MemLocObject abstraction.
MemLocObjectPtr LiveDeadMemAnalysis::Expr2MemLoc(SgNode* n, PartEdgePtr pedge)
{
  MemLocObjectPtrPair p = composer->Expr2MemLoc(n, pedge, this);
  Dbg::dbg << "LiveDeadMemAnalysis::Expr2MemLoc() p="<<p.strp(pedge)<<endl;
  if(p.mem) return createLDMemLocObjectCategory(n, p.mem, this);
  else      return p.expr;
}

/**************************
 ***** LDMemLocObject *****
 **************************/
LDMemLocObject::LDMemLocObject(SgNode* n, MemLocObjectPtr parent_, LiveDeadMemAnalysis* ldma)
  : MemLocObject(n), parent(parent_), ldma(ldma)
{
}

LDMemLocObject::LDMemLocObject(const LDMemLocObject& that) : 
    MemLocObject(that), parent(that.parent), ldma(that.ldma) 
{}

bool LDMemLocObject::mayEqualML(MemLocObjectPtr o, PartEdgePtr pedge)
{
  LDMemLocObjectPtr that = boost::dynamic_pointer_cast<LDMemLocObject>(o);
  ROSE_ASSERT(that);
  bool isThisLive = isLive(pedge);
  bool isThatLive = that->isLive(pedge);

  /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;LDMemLocObject::mayEqual"<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;this (live="<<isThisLive<<")="<<str("")<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;that (live="<<isThatLive<<")="<<o->str("")<<endl;*/
  
  if(!that) { /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;==>FALSE"<<endl;*/ return false; }
  //Dbg::dbg << "    isThisLive="<<isThisLive<<" isThisLive="<<isThisLive<<endl;
  // If both objects may be live, use the parents' equality operator
  if(isThisLive && isThatLive) {
    bool tmp=parent->mayEqual(that->parent, pedge);
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;==>"<<(tmp?"TRUE":"FALSE")<<endl; 
    return tmp;
  // If both objects are definitely not live, they're counted as being equal
  } else if(!isThisLive && !isThatLive) {
     //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;==>TRUE"<<endl; 
     return true;
  // Otherwise, they're in different classes and thus unequal
  } else
  { /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;==>FALSE"<<endl;*/ return false; }
}

bool LDMemLocObject::mustEqualML(MemLocObjectPtr o, PartEdgePtr pedge)
{
  LDMemLocObjectPtr that = boost::dynamic_pointer_cast<LDMemLocObject>(o);
  ROSE_ASSERT(that);
  bool isThisLive = isLive(pedge);
  bool isThatLive = that->isLive(pedge);
	
  /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;LDMemLocObject::mustEqual"<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;this (live="<<isThisLive<<")="<<str("")<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;that (live="<<isThatLive<<")="<<o->str("")<<endl;/*/
	
  if(!that) { /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;==>FALSE"<<endl;*/ return false; }
  // If both objects may be live, use the parents' equality operator
  if(isThisLive && isThisLive) {
    bool tmp=parent->mustEqual(that->parent, pedge);
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;==>"<<(tmp?"TRUE":"FALSE")<<endl; 
    return tmp;
  // If both objects are definitely not live, they're counted as being equal
  } else if(!isThisLive && !isThatLive) {
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;==>TRUE"<<endl; 
    return true;
  // Otherwise, they're in different classes and thus unequal
  } else
  { /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;==>FALSE"<<endl;*/ return false; }
}

// Returns true if this object is live at the given part and false otherwise
bool LDMemLocObject::isLive(PartEdgePtr pedge) const
{ 
  /*Dbg::dbg << "LDMemLocObject::isLive() part="<<cfgUtils::SgNode2Str(part.getNode())<<endl;
  Dbg::dbg << "parent str="<<parent->str()<<endl;
  Dbg::dbg << "parent strp="<<parent->strp(part)<<endl;
  Dbg::dbg << "MemLocObject::isFunctionMemLoc()="<<MemLocObject::isFunctionMemLoc()<<endl;
  Dbg::dbg << "live="<<isLiveMay(parent, ldma, part, *NodeState::getNodeState(part), "")<<endl;*/
  //Dbg::dbg << "state="<<NodeState::getNodeState(ldma, pedge->target())->str(ldma)<<endl;
  // We don't track liveness of function pointers so we default them to live
  if(parent->isFunctionMemLoc()) return true;
  else return isLiveMay(parent, ldma, pedge, "");
}

// pretty print for the object
std::string LDMemLocObject::str(std::string indent) const
{
  ostringstream oss;
  
  // Choose the string description to use based on the sub-type of this LDMemLocObject
  string name = "";
       if(dynamic_cast<const LDScalar*>(this))         name = "LDScalar";
  else if(dynamic_cast<const LDFunctionMemLoc*>(this)) name = "LDFunctionMemLoc";
  else if(dynamic_cast<const LDArray*>(this))          name = "LDArray";
  else if(dynamic_cast<const LDPointer*>(this))        name = "LDPointer";
  
  oss << "["<<name<<": "<<parent->str("    ")<<"]";
  return oss.str();
}

std::string LDMemLocObject::strp(PartEdgePtr pedge, std::string indent)
{
  ostringstream oss;
  if(isLive(pedge))
    oss << "[LDMemLocObject: LIVE: "<<parent->str("    ")<<"]";
  else
    oss << "[LDMemLocObject: DEAD]";
  return oss.str();
}

// Allocates a copy of this object and returns a pointer to it
MemLocObjectPtr LDMemLocObject::copyML() const
{
  return boost::make_shared<LDMemLocObject>(*this);
}

// Creates an instance of an LDMemLocObject that belongs to one of the MemLocObject categories
// (LDMemLocObject sub-classes): LDScalar, LDFunctionMemLoc, LDLabeledAggregate, LDArray or LDPointer.
LDMemLocObjectPtr createLDMemLocObjectCategory(SgNode* n, MemLocObjectPtr parent, LiveDeadMemAnalysis* ldma)
{
  if(parent->isScalar()) {
    //Dbg::dbg << "LiveDeadMemAnalysis::createLDMemLocObjectCategory() Scalar"<<endl;
    return boost::make_shared<LDScalar>(n, parent, ldma);
  } else if(parent->isFunctionMemLoc()) {
    //Dbg::dbg << "LiveDeadMemAnalysis::createLDMemLocObjectCategory() FunctionMemLoc"<<endl;
    return boost::make_shared<LDFunctionMemLoc>(n, parent, ldma);
  } else if(parent->isLabeledAggregate()) {
    //Dbg::dbg << "LiveDeadMemAnalysis::createLDMemLocObjectCategory() LabeledAggregate"<<endl;
    return boost::make_shared<LDLabeledAggregate>(n, parent, ldma);
  } else if(parent->isArray()) {
    //Dbg::dbg << "LiveDeadMemAnalysis::createLDMemLocObjectCategory() Array"<<endl;
    return boost::make_shared<LDArray>(n, parent, ldma);
  } else if(parent->isPointer()) {
    //Dbg::dbg << "LiveDeadMemAnalysis::createLDMemLocObjectCategory() Pointer"<<endl;
    return boost::make_shared<LDPointer>(n, parent, ldma);
  }
  // Control should not reach here
  Dbg::dbg << "<font color=\"#ff0000\">parent="<<parent->str()<<"</font>"<<endl;
  assert(0);
}

LDScalar::LDScalar(SgNode* n, MemLocObjectPtr parent, LiveDeadMemAnalysis* ldma)
  : MemLocObject(n), LDMemLocObject(n, parent, ldma)
{ /*Dbg::dbg << "LDScalar::LDScalar(parent="<<(parent ? parent->str("") : "NULL")<<")"<<endl;*/ }

LDFunctionMemLoc::LDFunctionMemLoc(SgNode* n, MemLocObjectPtr parent, LiveDeadMemAnalysis* ldma)
  :MemLocObject(n),  LDMemLocObject(n, parent, ldma)
{ /*Dbg::dbg << "LDFunctionMemLoc::LDFunctionMemLoc(parent="<<(parent ? parent->str("") : "NULL")<<")"<<endl;*/ }

LDLabeledAggregate::LDLabeledAggregate(SgNode* n, MemLocObjectPtr parent, LiveDeadMemAnalysis* ldma)
  : MemLocObject(n), LDMemLocObject(n, parent, ldma)
{/* Dbg::dbg << "LDLabeledAggregate::LDLabeledAggregate(parent="<<(parent ? parent->str("") : "NULL")<<")"<<endl;*/ }

size_t LDLabeledAggregate::fieldCount(PartEdgePtr pedge)
{
  //if(isLiveMay(parent, ldma, part, *NodeState::getNodeState(ldma, part), "")) 
    return parent->isLabeledAggregate()->fieldCount(pedge);
  //return 0;
}

std::list<LabeledAggregateFieldPtr> LDLabeledAggregate::getElements(PartEdgePtr pedge) const
{
  //if(isLiveMay(parent, ldma, part, *NodeState::getNodeState(ldma, part), "")) 
    return parent->isLabeledAggregate()->getElements(pedge);
  /*else {
    std::vector<boost::shared_ptr<LabeledAggregateField> > ret;
    return ret;
  }*/
}

LDArray::LDArray(SgNode* n, MemLocObjectPtr parent, LiveDeadMemAnalysis* ldma)
  : MemLocObject(n), LDMemLocObject(n, parent, ldma)
{ /*Dbg::dbg << "LDArray::LDArray(parent="<<(parent ? parent->str("") : "NULL")<<")"<<endl;*/ }

// Returns a memory object that corresponds to all the elements in the given array
MemLocObjectPtr LDArray::getElements(PartEdgePtr pedge)
{
  //if(isLiveMay(parent, ldma, part, *NodeState::getNodeState(ldma, part), "")) 
    return createLDMemLocObjectCategory(NULL, parent->isArray()->getElements(pedge), ldma);
  //else return MemLocObjectPtr();
}

// Returns the memory object that corresponds to the elements described by the given abstract index, 
// which represents one or more indexes within the array
MemLocObjectPtr LDArray::getElements(IndexVectorPtr ai, PartEdgePtr pedge)
{
  //if(isLiveMay(parent, ldma, part, *NodeState::getNodeState(ldma, part), "")) 
    return createLDMemLocObjectCategory(NULL, parent->isArray()->getElements(ai, pedge), ldma);
  //else return MemLocObjectPtr();
}

// number of dimensions of the array
size_t LDArray::getNumDims(PartEdgePtr pedge)
{
  if(isLive(pedge)) return parent->isArray()->getNumDims(pedge);
  else return 0;
}

//--- pointer like semantics
// support dereference of array object, similar to the dereference of pointer
// Return the element object: array[0]
MemLocObjectPtr LDArray::getDereference(PartEdgePtr pedge)
{
  //if(isLiveMay(parent, ldma, part, *NodeState::getNodeState(ldma, part), "")) 
    return createLDMemLocObjectCategory(NULL, parent->isArray()->getDereference(pedge), ldma);
  //else return MemLocObjectPtr();
}

LDPointer::LDPointer(SgNode* n, MemLocObjectPtr parent, LiveDeadMemAnalysis* ldma)
  : MemLocObject(n), LDMemLocObject(n, parent, ldma)
{ /*Dbg::dbg << "LDPointer::LDPointer(parent="<<(parent ? parent->str("") : "NULL")<<")"<<endl;*/ }

MemLocObjectPtr LDPointer::getDereference(PartEdgePtr pedge)
{
  //if(isLiveMay(parent, ldma, part, *NodeState::getNodeState(ldma, part), "")) 
  return createLDMemLocObjectCategory(NULL, parent->isPointer()->getDereference(pedge), ldma);
  //else return MemLocObjectPtr();
}

// Initialize vars to hold all the variables and expressions that are live at PartEdgePtr pedge
void getAllLiveMemAt(LiveDeadMemAnalysis* ldma, PartEdgePtr pedge, const NodeState& state, set<AbstractObjectPtr>& vars, string indent)
{
  //Dbg::dbg << "getAllLiveVarsAt() n="<<Dbg::escape(n.getNode()->unparseToString()) << " | " << n.getNode()->class_name()<<" | "<<n.getIndex()<<endl;
  //Dbg::dbg << "    state.getLatticeAbove(ldma): #="<<state.getLatticeAbove(ldma).size()<<endl;
  //for(vector<Lattice*>::const_iterator lat=state.getLatticeAbove(ldma).begin(); lat!=state.getLatticeAbove(ldma).end(); lat++) {
  //      Dbg::dbg <<"        lat="<<*lat<<endl;
  //      Dbg::dbg <<"        lat="<<(*lat)->str("            ")<<endl;
  //}
  //Dbg::dbg << "    state.getLatticeBelow(ldma): #="<<state.getLatticeBelow(ldma).size()<<endl;
  //for(vector<Lattice*>::const_iterator lat=state.getLatticeBelow(ldma).begin(); lat!=state.getLatticeBelow(ldma).end(); lat++) {
  //      Dbg::dbg <<"        lat="<<*lat<<endl;
  //      Dbg::dbg <<"        lat="<<(*lat)->str("            ")<<endl;
  //}
  //Dbg::dbg << "    state = "<<state.str(ldma, "        ")<<endl;
  //Dbg::dbg.flush();
      
  /*AbstractObjectSet* liveLAbove = dynamic_cast<AbstractObjectSet*>(*(state.getLatticeAbove(ldma).begin()));
  AbstractObjectSet* liveLBelow = dynamic_cast<AbstractObjectSet*>(*(state.getLatticeBelow(ldma).begin()));

  // The set of live vars AT this node is the union of vars that are live above it and below it
  for(AbstractObjectSet::const_iterator var=liveLAbove->begin(); var!=liveLAbove->end(); var++)
    vars.insert(*var);
  for(AbstractObjectSet::const_iterator var=liveLBelow->begin(); var!=liveLBelow->end(); var++)
    vars.insert(*var);*/
  
  AbstractObjectSet* liveL = dynamic_cast<AbstractObjectSet*>(state.getLatticeAbove(ldma, pedge, 0));
  ROSE_ASSERT(liveL);
  for(AbstractObjectSet::const_iterator var=liveL->begin(); var!=liveL->end(); var++)
    vars.insert(*var);
}

// Returns the set of variables and expressions that are live at PartEdgePtr pedge
set<AbstractObjectPtr> getAllLiveMemAt(LiveDeadMemAnalysis* ldma, PartEdgePtr pedge, const NodeState& state, string indent)
{
    set<AbstractObjectPtr> vars;
    getAllLiveMemAt(ldma, pedge, state, vars, indent);
    return vars;
}

// Returns true if the given MemLocObject must be live at the given PartEdgePtr pedge
bool isLiveMust(MemLocObjectPtr mem, LiveDeadMemAnalysis* ldma, PartEdgePtr pedge, NodeState& state, string indent)
{
  /*AbstractObjectSet* liveLAbove = dynamic_cast<AbstractObjectSet*>(*(state.getLatticeAbove(ldma).begin()));
  AbstractObjectSet* liveLBelow = dynamic_cast<AbstractObjectSet*>(*(state.getLatticeBelow(ldma).begin()));
  
  //Dbg::dbg << "isLiveMust: liveLAbove="<<liveLAbove->str("")<<endl;
  //Dbg::dbg << "isLiveMust: liveLBelow="<<liveLAbove->str("")<<endl;
  
  if(liveLAbove->containsMust(mem)) return true;
  if(liveLBelow->containsMust(mem)) return true;*/
  AbstractObjectSet* liveL = dynamic_cast<AbstractObjectSet*>(state.getLatticeAbove(ldma, pedge, 0));
  ROSE_ASSERT(liveL);
  if(liveL->containsMust(mem)) return true;
  
  return false;
}

// Returns true if the given MemLocObject may be live at the given PartEdgePtr pedge
bool isLiveMay(MemLocObjectPtr mem, LiveDeadMemAnalysis* ldma, PartEdgePtr pedge, string indent)
{
  //Dbg::dbg << "isLiveMay(mem="<<mem->str("")<<", pedge="<<pedge->str()<<")"<<endl;
  
  // If this is not a wildcard edge, check if mem is live along it
  if(pedge->source() && pedge->target()) {
    NodeState* state = NodeState::getNodeState(ldma, pedge->target());
    //Dbg::dbg << "state="<<state->str(ldma)<<endl;
    
    AbstractObjectSet* liveL = dynamic_cast<AbstractObjectSet*>(state->getLatticeAbove(ldma, pedge, 0));
    //Dbg::dbg << "isLiveMay: liveLAbove="<<liveL->str("")<<endl;
    ROSE_ASSERT(liveL);
    if(liveL->containsMay(mem)) return true;
  // If the source of this edge is a wildcard, mem is live if it is live along any incoming edge or 
  // the outgoing edge (for SgInitializedName we want to say that it is live immediately before its
  // declaration so that clients can ask for its MemLocObject at edges coming into the declaration).
  } else if(pedge->target()) {
    NodeState* state = NodeState::getNodeState(ldma, pedge->target());
    //Dbg::dbg << "state="<<state->str(ldma)<<endl;
    
    map<PartEdgePtr, std::vector<Lattice*> >& e2lats = state->getLatticeAboveAllMod(ldma);
    ROSE_ASSERT(e2lats.size()>=1);
    for(map<PartEdgePtr, std::vector<Lattice*> >::iterator lats=e2lats.begin(); lats!=e2lats.end(); lats++) {
      PartEdge* p = lats->first.get();
      ROSE_ASSERT(p->target() == pedge.get()->target());
      AbstractObjectSet* liveL = dynamic_cast<AbstractObjectSet*>(*(lats->second.begin()));
      
      //Dbg::dbg << "isLiveMay: edge="<<p->str()<<" liveLAbove="<<liveL->str("")<<endl;
    
      ROSE_ASSERT(liveL);
      if(liveL->containsMay(mem)) return true;
    }

    std::vector<Lattice*>& lats = state->getLatticeBelowMod(ldma);
    AbstractObjectSet* liveL = dynamic_cast<AbstractObjectSet*>(*(lats.begin()));
    ROSE_ASSERT(liveL);
    //Dbg::dbg << "isLiveMay: liveLBelow="<<liveL->str("")<<endl;
    if(liveL->containsMay(mem)) return true;
  // If the target of this edge is a wildcard, mem is live if it is live along any outgoing edge
  } else if(pedge->source()) {
    NodeState* state = NodeState::getNodeState(ldma, pedge->source());
    //Dbg::dbg << "isLiveMay: state="<<state->str(ldma)<<endl;
    
    std::vector<Lattice*>& lats = state->getLatticeBelowMod(ldma);
    AbstractObjectSet* liveL = dynamic_cast<AbstractObjectSet*>(*(lats.begin()));
    ROSE_ASSERT(liveL);
    //Dbg::dbg << "isLiveMay: liveLBelow="<<liveL->str("")<<endl;
    if(liveL->containsMay(mem)) return true;
  }
  return false;
}

}; // namespace dataflow
