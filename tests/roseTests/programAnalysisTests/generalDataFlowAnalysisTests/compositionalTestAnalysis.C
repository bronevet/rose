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

  Rose_STL_Container<string> args = CommandlineProcessing::generateArgListFromArgcArgv(argc, argv);

  // strip the dataflow analysis options
  Rose_STL_Container<string> dataflowoptions = CommandlineProcessing::generateOptionList(args, "-dataflow:");
 
  SgProject* project = frontend(argc, argv);

  // Set up the composition chain requested by the user
  list<ComposedAnalysis*> analyses;

  // compose the analysis specified in the commandline
  // NOTE: prefix is empty as it was stripped out by generateOptionList()
  if(CommandlineProcessing::isOption(dataflowoptions, "", "constantpropagationanalysis", false)) {
      analyses.push_back(new ConstantPropagationAnalysis());
  }
  else if(CommandlineProcessing::isOption(dataflowoptions, "", "livedeadmemanalysis", false)) {
      analyses.push_back(new LiveDeadMemAnalysis());
  }
  else if(CommandlineProcessing::isOption(dataflowoptions, "", "deadpathelimanalysis", false)) {
      analyses.push_back(new DeadPathElimAnalysis());
  }
  else if(CommandlineProcessing::isOption(dataflowoptions, "", "orthogonalarrayanalysis", false)) {
      analyses.push_back(new OrthogonalArrayAnalysis());   
  }

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
  checkDataflowInfoPass* cdip = new checkDataflowInfoPass();
  
  ChainComposer cc(argc, argv, analyses, cdip, false);
  cc.runAnalysis();

  if(cdip->getNumErrors() > 0) cout << cdip->getNumErrors() << " Errors Reported!"<<endl;
  else                        cout << "PASS"<<endl;
  printf("==========  E  N  D  ==========\n");
  
  return 0;
}

