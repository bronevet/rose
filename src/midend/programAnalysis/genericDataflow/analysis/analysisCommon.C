#include "analysisCommon.h"
#include "sageInterface.h"
using namespace cfgUtils;
using namespace SageInterface;

namespace dataflow {
static SgIncidenceDirectedGraph* callGraph;

void initAnalysis(SgProject* project)
{
  static bool firstTime=true;
  if(!project) project = getProject();
  
  if(firstTime) {
    // Create the Call Graph
    CallGraphBuilder cgb(project);
    cgb.buildCallGraph();
    callGraph = cgb.getGraph(); 
    printf("Call Graph Generated\n");fflush(stdout);
    //GenerateDotGraph(graph, "test_example.callgraph.dot");

    // Create unique annotations on each expression to make it possible to assign each expression a unique variable name
    SageInterface::annotateExpressionsWithUniqueNames(project);
    
    Dbg::init("Composed Analysis", "dbg", "index");
    
    
    firstTime=false;
  }
}

SgIncidenceDirectedGraph* getCallGraph()
{
  return callGraph;
}
};