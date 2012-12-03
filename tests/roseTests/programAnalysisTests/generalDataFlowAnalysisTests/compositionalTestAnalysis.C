#include "rose.h"
#include "compose.h"
#include "const_prop_analysis.h"
#include "live_dead_analysis.h"
#include "ortho_array_analysis.h"
#include "dead_path_elim_analysis.h"
#include "printAnalysisStates.h"
#include <vector>
#include <ctype.h>
#include <boost/xpressive/xpressive.hpp>
#include <boost/xpressive/regex_actions.hpp>

using namespace std;
using namespace dataflow;

int main(int argc, char** argv)
{
  printf("========== S T A R T ==========\n");
  Rose_STL_Container<string> args = CommandlineProcessing::generateArgListFromArgcArgv(argc, argv);

  // strip the dataflow analysis options
  Rose_STL_Container<string> dataflowoptions = CommandlineProcessing::generateOptionList(args, "-dataflow:");
 
  SgProject* project = frontend(argc, argv);
  
  initAnalysis();
  Dbg::init("Composed Analysis", ".", "index.html");

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
    
    composer = as_xpr(icase("loosechain")) | icase("loosepar");
    analysis = as_xpr(icase("constantpropagation")) | icase("livedead");
    analysisList = '(' >> (by_ref(analysis) | by_ref(compSpec)) >> *(*_s >> "," >> *_s >> (by_ref(analysis) | by_ref(compSpec))) >> ')';
    compSpec = "fuse: ">>by_ref(composer) >> analysisList;
    
    smatch what;
    
    string pragmaS = pragma->get_pragma();
    
    // Convert the paragma to all lower-case before checking it against the grammar
    for(int i=0; i<pragmaS.length(); i++) pragmaS[i] = tolower(pragmaS[i]);
    
    // Remove leading spaces
    int startNonSpace=0;
    while(startNonSpace<pragmaS.length() && (pragmaS[startNonSpace]==' ' || pragmaS[startNonSpace]=='\t')) startNonSpace++;
    if(startNonSpace>0) pragmaS.erase(0, startNonSpace);
    
    // Remove trailing spaces
    int endNonSpace=pragmaS.length()-1;
    while(endNonSpace>=0 && (pragmaS[endNonSpace]==' ' || pragmaS[endNonSpace]=='\t')) endNonSpace--;
    if(endNonSpace<pragmaS.length()-1) pragmaS.erase(endNonSpace+1);
    
    cout << "pragmaS = \""<<pragmaS<<"\"\n";
    
    // If this is a command for the compositional framework
    if(pragmaS.find("fuse:")==0) {
      if(regex_match(pragmaS, what, compSpec)) {
        cout << "MATCH composer\n";
        std::for_each(what.nested_results().begin(),
                      what.nested_results().end(),
                      output_nested_results());
      } else
        cout << "FAIL composer\n";
    }
    
    //cout << pragma->get_pragma() << endl;
    char analysisName[100];
    int ret = sscanf(pragma->get_pragma().c_str(), "ChainComposer %s", analysisName);
    if(ret==1) {
      //cout << "analysisName="<<string(analysisName)<<endl;
      // Convert the name of the analysis to all lower-case
      for(char* p=analysisName; *p; p++) *p = tolower(*p);
      
      ComposedAnalysis* a = NULL;
      // Find the analysis it denotes and add it to the analysis chain
      if(strcmp(analysisName, "constantpropagationanalysis")==0)  a = new ConstantPropagationAnalysis();
      else if(strcmp(analysisName, "livedeadmemanalysis")==0)     a = new LiveDeadMemAnalysis();
      else if(strcmp(analysisName, "deadpathelimanalysis")==0)    a = new DeadPathElimAnalysis();
      else if(strcmp(analysisName, "orthogonalarrayanalysis")==0) a = new OrthogonalArrayAnalysis();
      ROSE_ASSERT(a);
      
      //list<ComposedAnalysis*> subAnalyses;
      //subAnalyses.push_back(a);
      //LooseParallelComposer* lpc = new LooseParallelComposer(subAnalyses);
      //analyses.push_back(lpc);
      analyses.push_back(a);
    }
  }
  checkDataflowInfoPass* cdip = new checkDataflowInfoPass();
  
  ChainComposer cc(analyses, cdip, false);
  cc.runAnalysis();

  if(cdip->getNumErrors() > 0) cout << cdip->getNumErrors() << " Errors Reported!"<<endl;
  else                        cout << "PASS"<<endl;
  printf("==========  E  N  D  ==========\n");
  
  return 0;
}

