#ifndef _DEFUSEQUERY_H
#define _DEFUSEQUERY_H

/*************************************************************
 * Copyright: (C) 2013 by Sriram Aananthakrishnan            *
 * Author   : Sriram Aananthakrishnan                        *
 * email    : aananthakris1@llnl.gov                         *
 *************************************************************/


#include "AstTerm.h"
#include "AstMatching.h"
#include "VariableIdMapping.h"
#include "Miscellaneous.h"
#include <set>
#include <utility>

#include "VariableIdUtils.h"

using namespace CodeThorn;

// VariableId is short hand for named memory
// locations

// basic type info to describe some type
// information about the variable
// type info is associated with the respective VariableId
enum VariableIdTypeInfo {
  VARIABLE,
  ARRAY,
  POINTER,
  REFERENCE
};

typedef std::pair<VariableId, VariableIdTypeInfo> VariableIdInfo;

// map with VariableId as the key
//
typedef std::map<VariableId, VariableIdTypeInfo> VariableIdInfoMap;

// VarsInfo is used to represent the result
// of def/use query on expressions. 
// VarsInfo is a pair <VariableIdInfoMap, bool>
// @VariableIdInfoMap: set of named memory locations 
// (variables) represented by their VariableId and their
// associated VariableIdTypeInfo
// @bool: flag is set if the expression modifies/uses
// memory locations that are different from 
// the named memory locations appearing on the
// expression itself (example: *p or array + 5)
// flag is an indicator for requiring more
// sophisticated pointer analysis to answer the
// def/use query on this expression
typedef std::pair<VariableIdInfoMap, bool> VarsInfo;

// it is expensive to answer def/use queries on demand
// for expressions that involve function calls
// as it would mean walking all the expressions
// in the function and the functions that it may call
// we simply collect all the function calls in the expression
// @FunctionCallExpSet: consitsts of all the functions
// that this expression is calling
typedef std::set<SgFunctionCallExp*> FunctionCallExpSet;

/*************************************************
 *************** DefUseVarsInfo ****************
 *************************************************/

// determined completely based on syntactic information
// def_vars_info consists of VariableIds which are written by the expression
// use_vars_info consists of VariableIds which are read but not modified by this expression
// func_set consists of all SgFunctionCallExps that are invoked by this expression
class DefUseVarsInfo
{
  VarsInfo def_vars_info;
  VarsInfo use_vars_info;
  FunctionCallExpSet func_set;
   
public:
  DefUseVarsInfo() { }
  DefUseVarsInfo(const VarsInfo& _def_info, const VarsInfo& _use_info, const FunctionCallExpSet& _fset);
  
  // returns the corresponding info about the memory locations
  VarsInfo getDefVarsInfo();
  VarsInfo getUseVarsInfo();
  VarsInfo& getDefVarsInfoMod(); 
  VarsInfo& getUseVarsInfoMod();
  const VarsInfo& getDefVarsInfoRef() const;
  const VarsInfo& getUseVarsInfoRef() const;

  FunctionCallExpSet getFunctionCallExpSet();
  FunctionCallExpSet& getFunctionCallExpSetMod();
  const FunctionCallExpSet& getFunctionCallExpSetRef() const;
  
  // copy sets to handle side-effects
  void copyDefToUse();
  void copyUseToDef();
  
  bool isDefSetEmpty();
  bool isUseSetEmpty();
  bool isFunctionCallExpSetEmpty();

  // returns the flag func_modify
  bool isModByFunction();

  // returns the flag of VarsInfo
  bool isDefSetModByPointer();
  bool isUseSetModByPointer();

  DefUseVarsInfo operator+(const DefUseVarsInfo& duvi1);

  std::string varsInfoPrettyPrint(VarsInfo& vars_info, VariableIdMapping& vidm);
  std::string functionCallExpSetPrettyPrint(FunctionCallExpSet& func_calls_info);

  // for more readability
  std::string str(VariableIdMapping& vidm);  
};

// used by the getDefUseVarsInfo_rec to traverse the 
// structure of the expression and call the appropriate
// getDefUseLHS/RHS functions
class ExprWalker : public ROSE_VisitorPatternDefaultBase
{
  VariableIdMapping& vidm;
  // if set then we are processing an expression on lhs of assignment
  bool isModExpr;
  DefUseVarsInfo duvi;

public:
  ExprWalker(VariableIdMapping& _vidm, bool isModExpr);
  // lhs of assignment operator are always lvalues
  // process them 
  void visit(SgAssignOp* sgn);
  void visit(SgCompoundAssignOp* sgn);

  // recurse on sub-expressions
  void visit(SgBinaryOp* sgn);

  void visit(SgCastExp* sgn);
  void visit(SgAddressOfOp* sgn);
  void visit(SgMinusMinusOp* sgn);
  void visit(SgMinusOp* sgn);
  void visit(SgUnaryAddOp* sgn);
  void visit(SgNotOp* sgn);
  void visit(SgPlusPlusOp* sgn);
  void visit(SgSizeOfOp* sgn);
  void visit(SgBitComplementOp* sgn);

  void visit(SgFunctionCallExp* sgn);
  void visit(SgExprListExp* sgn);
  void visit(SgConditionalExp* sgn);
  
  void visit(SgAssignInitializer* sgn);
  void visit(SgConstructorInitializer* sgn);
  void visit(SgAggregateInitializer* sgn);

  // recursion undwinds on basic expressions
  // that represent memory
  //  
  void visit(SgVarRefExp* sgn);
  void visit(SgPntrArrRefExp* sgn);
  void visit(SgPointerDerefExp* sgn);
  void visit(SgDotExp* sgn);
  void visit(SgArrowExp* sgn);
  void visit(SgInitializedName* sgn);

  // no processing required
  void visit(SgValueExp* sgn);
  void visit(SgFunctionRefExp* sgn);
  void visit(SgMemberFunctionRefExp* sgn);
  void visit(SgThisExp* sgn);

  void visit(SgExpression* sgn);

  // helper methods
  void visitSgUnaryOpNoMod(SgUnaryOp* sgn);


  DefUseVarsInfo getDefUseVarsInfo();
};

// interface function to
// identify modified and used locations based on syntax
//  a[i] = expr; a is modified and i, expr are used 
// *p = expr; since we dont know anything about p
// the locations that are modified involve all arrays and variables in addressTakenSet
// if the expression involve functions or function pointers it can 
// potentially modify everything
DefUseVarsInfo getDefUseVarsInfo(SgNode* sgn, VariableIdMapping& vidm);

// internal implementation
// @sgn: root node
// @duvi: the defuse object that will be built
// @isModExpr: set to true if the expression is modifying a memory location
DefUseVarsInfo getDefUseVarsInfo_rec(SgNode* sgn, VariableIdMapping& vidm, bool isModExpr);

#endif

