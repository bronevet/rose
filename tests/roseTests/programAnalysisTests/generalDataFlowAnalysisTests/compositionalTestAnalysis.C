#include "rose.h"
#include "compose.h"
#include "const_prop_analysis.h"
#include "live_dead_analysis.h"
#include "ortho_array_analysis.h"
#include "dead_path_elim_analysis.h"
#include "printAnalysisStates.h"
#include <vector>
#include <ctype.h>

using namespace std;
using namespace dataflow;



int main(int argc, char** argv)
{
  printf("========== S T A R T ==========\n");
 
  SgProject* project = frontend(argc,argv);
  // Set up the composition chain requested by the user
  list<ComposedAnalysis*> analyses;

  // Look for pragmas that identify the preferred analysis chain to be used on this input
  Rose_STL_Container<SgNode*> pragmas = NodeQuery::querySubTree(project, V_SgPragma);
  for(Rose_STL_Container<SgNode*>::iterator p=pragmas.begin(); p!=pragmas.end(); p++) {
    SgPragma* pragma = isSgPragma(*p);
    ROSE_ASSERT(pragma);
    //cout << pragma->get_pragma() << endl;
    char analysisName[100];
    int ret = sscanf(pragma->get_pragma().c_str(), "ChainComposer %s", analysisName);
    if(ret==1) {
      //cout << "analysisName="<<string(analysisName)<<endl;
      // Convert the name of the analysis to all lower-case
      for(char* p=analysisName; *p; p++) *p = tolower(*p);
      
      // Find the analysis it denotes and add it to the analysis chain
           if(strcmp(analysisName, "constantpropagationanalysis")==0) analyses.push_back(new ConstantPropagationAnalysis());
      else if(strcmp(analysisName, "livedeadmemanalysis")==0)         analyses.push_back(new LiveDeadMemAnalysis());
      else if(strcmp(analysisName, "deadpathelimanalysis")==0)        analyses.push_back(new DeadPathElimAnalysis());
      else if(strcmp(analysisName, "orthogonalarrayanalysis")==0)     analyses.push_back(new OrthogonalArrayAnalysis());
    }
  }
  // Add the correctness checking analysis to run after all the others
  checkDataflowInfoPass* cdip = new checkDataflowInfoPass();
  analyses.push_back(cdip);
  
  ChainComposer cc(argc, argv, analyses, cdip, false, project);
  cc.runAnalysis();

  if(cdip->getNumErrors() > 0) cout << cdip->getNumErrors() << " Errors Reported!"<<endl;
  else                        cout << "PASS"<<endl;
  printf("==========  E  N  D  ==========\n");
  
  return 0;
}

//#include "rose.h"
//
//#include <list>
//#include <sstream>
//#include <iostream>
//#include <fstream>
//#include <string.h>
//#include <map>
//
//using namespace std;
//
//#include "genericDataflowCommon.h"
//#include "VirtualCFGIterator.h"
//#include "cfgUtils.h"
//#include "CallGraphTraverse.h"
//#include "analysisCommon.h"
//#include "analysis.h"
//#include "dataflow.h"
//#include "latticeFull.h"
//#include "printAnalysisStates.h"
//
//#if 0
//#include "common.h"
//#include "variables.h"
//#include "cfgUtils.h"
//#include "analysisCommon.h"
//#include "functionState.h"
//#include "latticeFull.h"
//#include "analysis.h"
//#include "dataflow.h"
//#include "liveDeadVarAnalysis.h"
//#include "saveDotAnalysis.h"
//#endif
//
//// Include the header file specific to our analysis.
//// #include "divAnalysis.h"
//#include "constantPropagation.h"
//
//int numFails = 0, numPass = 0;
//
//class evaluateAnalysisStates : public UnstructuredPassIntraAnalysis
//   {
//     public:
//          ConstantPropagationAnalysis* div;
//          string indent;
//          map<string, map<SgName, ConstantPropagationLattice> > expectations;
//          int total_expectations;
//  
//          evaluateAnalysisStates(ConstantPropagationAnalysis* div_, string indent_);
//          void visit(const Function& func, const DataflowNode& n, NodeState& state);
//   };
//
//
//evaluateAnalysisStates::evaluateAnalysisStates(ConstantPropagationAnalysis* div_, string indent_)
//   : div(div_), indent(indent_), total_expectations(0)
//   {
//     expectations["testFunc1"]["a"] = ConstantPropagationLattice(ConstantPropagationLattice::constantValue,1);
//     expectations["testFunc1"]["b"] = ConstantPropagationLattice(ConstantPropagationLattice::constantValue,4);
//     expectations["testFunc1"]["c"] = ConstantPropagationLattice(ConstantPropagationLattice::constantValue,5);
//
//     for (map<string, map<SgName, ConstantPropagationLattice> >::iterator i = expectations.begin(); i != expectations.end(); ++i)
//          total_expectations += i->second.size();
//
//     cout << "Total expectations: " << total_expectations << endl;
//   }
//
//void
//evaluateAnalysisStates::visit(const Function& func, const DataflowNode& n, NodeState& state)
//   {
//     SgFunctionCallExp *fnCall = isSgFunctionCallExp(n.getNode());
//     if (!fnCall)
//          return;
//
//     if (!fnCall->getAssociatedFunctionSymbol()) 
//          return;
//
//     string funcName = fnCall->getAssociatedFunctionSymbol()->get_name().getString();
//     if (funcName.find("testFunc") == string::npos)
//          return;
//
//     FiniteVarsExprsProductLattice *lat = dynamic_cast<FiniteVarsExprsProductLattice *>(state.getLatticeAbove(div)[0]);
//     cout << indent << "Lattice before call to " << funcName << ": " << lat->str() << endl;
//
//     set<varID> allVars = lat->getAllVars();
//     for (set<varID>::iterator i = allVars.begin(); i != allVars.end(); ++i)
//        {
//          string name = i->str();
//          cout << "Variable " << name << " ";
//
//          if (expectations[funcName].find(name) == expectations[funcName].end())
//             {
//               cout << "unspecified" << endl;
//               continue;
//             }
//
//          Lattice *got = lat->getVarLattice(*i);
//          ROSE_ASSERT(got);
//          if (expectations[funcName][name] != got)
//             {
//               cout << "mismatched: " << got->str() << " was not the expected " << expectations[funcName][name].str();
//               numFails++;
//             }
//            else
//             {
//               cout << "matched";
//               numPass++;
//             }
//          cout << endl;
//        }
//   }
//
//
//int
//main( int argc, char * argv[] ) 
//   {
//     printf("========== S T A R T ==========\n");
//
//  // Build the AST used by ROSE
//     SgProject* project = frontend(argc,argv);
//
//     initAnalysis(project);
//     Dbg::init("Divisibility Analysis Test", ".", "index.html");
//
//  /* analysisDebugLevel = 0;
//
//     SaveDotAnalysis sda;
//     UnstructuredPassInterAnalysis upia_sda(sda);
//     upia_sda.runAnalysis();
//   */
//
//     liveDeadAnalysisDebugLevel = 1;
//     analysisDebugLevel = 1;
//     if (liveDeadAnalysisDebugLevel)
//        {
//          printf("*********************************************************************\n");
//          printf("*****************   Constant Propagation Analysis   *****************\n");
//          printf("*********************************************************************\n");
//        }
//
//     LiveDeadVarsAnalysis ldva(project);
//     UnstructuredPassInterDataflow ciipd_ldva(&ldva);
//     ciipd_ldva.runAnalysis();
//
//     CallGraphBuilder cgb(project);
//     cgb.buildCallGraph();
//     SgIncidenceDirectedGraph* graph = cgb.getGraph(); 
//
//     analysisDebugLevel = 1;
//     ConstantPropagationAnalysis divA(&ldva);
//     ContextInsensitiveInterProceduralDataflow divInter(&divA, graph);
//     divInter.runAnalysis();
//
//     evaluateAnalysisStates eas(&divA, "    ");
//     UnstructuredPassInterAnalysis upia_eas(eas);
//     upia_eas.runAnalysis();
//
//// Liao 1/6/2012, optionally dump dot graph of the analysis result
////     Dbg::dotGraphGenerator(&divA);
//     if (numFails == 0 && numPass == eas.total_expectations)
//          printf("PASS: %d / %d\n", numPass, eas.total_expectations);
//       else
//          printf("FAIL!: %d / %d\n", numPass, eas.total_expectations);
//
//     printf("==========  E  N  D  ==========\n");
//
//  // Unparse and compile the project (so this can be used for testing)
//     return /*backend(project) +*/ numFails;
//   }



