//#pragma ChainComposer ConstantPropagationAnalysis
//#pragma ChainComposer DeadPathElimAnalysis
//#pragma ChainComposer PointsToAnalysis
//#pragma ChainComposer ConstantPropagationAnalysis

#pragma fuse lc(cp, dp, pt, cp)

void CompDebugAssert(bool, ...);

int foo()
{
  int x=5,y=12;
  int *p;
  if(x<y) p = &x;
  else p = &y;
  CompDebugAssert(*p+5 == 10);
  return *p+5;  
}
