#ifndef ANALYSIS_COMMON
#define ANALYSIS_COMMON

#include "genericDataflowCommon.h"
#include "cfgUtils.h"
#include "CallGraphTraverse.h"

namespace dataflow {
// initializes the compiler analysis framework
void initAnalysis(SgProject* project=NULL);

// returns the call graph of the current project
SgIncidenceDirectedGraph* getCallGraph();
};
#endif
