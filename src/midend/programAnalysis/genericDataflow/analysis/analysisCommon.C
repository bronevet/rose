#include "analysisCommon.h"
#include "sageInterface.h"
using namespace cfgUtils;
using namespace SageInterface;

namespace dataflow {
static SgIncidenceDirectedGraph* callGraph;

void initAnalysis()
{
        // Create the Call Graph
        CallGraphBuilder cgb(getProject());
        cgb.buildCallGraph();
        callGraph = cgb.getGraph(); 
        //GenerateDotGraph(graph, "test_example.callgraph.dot");
        
        // Create unique annotations on each expression to make it possible to assign each expression a unique variable name
        SageInterface::annotateExpressionsWithUniqueNames(getProject());
}

SgIncidenceDirectedGraph* getCallGraph()
{
        return callGraph;
}
};