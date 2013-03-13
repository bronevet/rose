#include "rose.h"
#include "compose.h"
#include "const_prop_analysis.h"
#include "live_dead_analysis.h"
#include "ortho_array_analysis.h"
#include "dead_path_elim_analysis.h"
#include "printAnalysisStates.h"
#include "pointsToAnalysis.h"
#include <vector>
#include <ctype.h>
#include <boost/xpressive/xpressive.hpp>
#include <boost/xpressive/regex_actions.hpp>

using namespace std;
using namespace dataflow;
using namespace boost::xpressive;

// Regex expressions for the composition command, defined globally so that they can be used inside main 
// (where they're initialized) as well as inside output_nested_results()
sregex composer, lcComposer, lpComposer, analysis, cpAnalysis, ldAnalysis, oaAnalysis, dpAnalysis, ptAnalysis, analysisList, compSpec;

// Displays nested results to std::cout with indenting
struct output_nested_results
{
  typedef enum {looseSeq, loosePar, tight, unknown} composerType;
  
  int tabs_;
  composerType parentComposerType;
  list<ComposedAnalysis*>& parentSubAnalyses;
  Composer** parentComposer; 
  checkDataflowInfoPass* cdip;
      
  output_nested_results(int tabs, composerType& parentComposerType, list<ComposedAnalysis*>& parentSubAnalyses, Composer** parentComposer, checkDataflowInfoPass* cdip)
      : tabs_( tabs ), parentComposerType(parentComposerType), parentSubAnalyses(parentSubAnalyses), parentComposer(parentComposer), cdip(cdip)
  {
  }

  template< typename BidiIterT >
  void operator ()( match_results< BidiIterT > const &what ) 
  {
    // first, do some indenting
    typedef typename std::iterator_traits< BidiIterT >::value_type char_type;
    char_type space_ch = char_type(' ');

    string match = what[0];
    
    // output the match
    std::fill_n( std::ostream_iterator<char_type>( std::cout ), tabs_ * 4, space_ch ); cout << match << '\n';
    
    // If this term is an analysis rather than a composer
    smatch subWhat;
    if(regex_match(match, subWhat, analysis)) {
      // Create the selected analysis and add it to the parent's sub-analysis list
           if(regex_match(match, subWhat, cpAnalysis)) { parentSubAnalyses.push_back(new ConstantPropagationAnalysis()); }
      else if(regex_match(match, subWhat, ldAnalysis)) { parentSubAnalyses.push_back(new LiveDeadMemAnalysis()); }
      else if(regex_match(match, subWhat, oaAnalysis)) { parentSubAnalyses.push_back(new OrthogonalArrayAnalysis()); }
      else if(regex_match(match, subWhat, dpAnalysis)) { parentSubAnalyses.push_back(new DeadPathElimAnalysis()); }
      else if(regex_match(match, subWhat, ptAnalysis)) { parentSubAnalyses.push_back(new PointsToAnalysis()); }
    // Otherwise, if this is a composer, create the analyses in its sub-analysis list and then create the composer
    } else if(regex_match(match, subWhat, lcComposer)) {
      std::fill_n( std::ostream_iterator<char_type>( std::cout ), tabs_ * 4, space_ch ); cout << "LOOSE SEQUENTIAL\n"<<endl;
      parentComposerType = looseSeq;
    } else if(regex_match(match, subWhat, lpComposer)) {
      std::fill_n( std::ostream_iterator<char_type>( std::cout ), tabs_ * 4, space_ch ); cout << "LOOSE PARALLEL\n"<<endl;
      parentComposerType = loosePar;
    // Finally, if this is a list of analyses for a given parent composer
    } else {
      ROSE_ASSERT(parentComposerType != unknown);
      list<ComposedAnalysis*> mySubAnalyses;
      composerType myComposerType = unknown;
      
      // Output any nested matches
      output_nested_results ons(tabs_ + 1, myComposerType, mySubAnalyses, NULL, NULL);
      std::for_each(
          what.nested_results().begin(),
          what.nested_results().end(),
          ons);
      std::fill_n( std::ostream_iterator<char_type>( std::cout ), tabs_ * 4, space_ch ); cout << "#mySubAnalyses="<<mySubAnalyses.size()<<endl;
      for(list<ComposedAnalysis*>::iterator i=mySubAnalyses.begin(); i!=mySubAnalyses.end(); i++)
      { std::fill_n( std::ostream_iterator<char_type>( std::cout ), tabs_ * 4, space_ch ); cout << "    "<<(*i)->str()<<endl; }
      
      if(parentComposerType == looseSeq) {
        ChainComposer* cc = new ChainComposer(mySubAnalyses, cdip, true);
        // Until ChainComposer is made to be a ComposedAnalysis, we cannot add it to the parentSubAnalyses list. This means that 
        // LooseComposer can only be user at the outer-most level of composition
        // !!!parentSubAnalyses.push_back(cc);
        *parentComposer = cc;
      } else if(parentComposerType == loosePar) {
        LooseParallelComposer* lp = new LooseParallelComposer(mySubAnalyses);
        parentSubAnalyses.push_back(lp);
        *parentComposer = lp;
      }
    }
  }
};

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
  else if(CommandlineProcessing::isOption(dataflowoptions, "", "pointstoanalysis", false)) {
      analyses.push_back(new PointsToAnalysis());   
  }
  // Look for pragmas that identify the preferred analysis chain to be used on this input
  
  string pragmaCmd = "";
  
  Rose_STL_Container<SgNode*> pragmas = NodeQuery::querySubTree(project, V_SgPragma);
  for(Rose_STL_Container<SgNode*>::iterator p=pragmas.begin(); p!=pragmas.end(); p++) {
    SgPragma* pragma = isSgPragma(*p);
    ROSE_ASSERT(pragma);
    
    cout << "pragma: "<< pragma->get_pragma() << endl;
    sregex pragmaLine = *_s >> as_xpr("fuse") >> *_s >> (s1=+~_n);
    smatch what;
    
    /*char analysisName[10000];
    int ret = sscanf(pragma->get_pragma().c_str(), "%s", pragmaType);
    if(ret==1 && strcmp(pragmaType, fuse)==0) {*/
    if(regex_match(pragma->get_pragma(), what, pragmaLine)) {
      cout << "MATCH: #what="<<what.size()<<"\n";
      for(unsigned int i=0; i<what.size(); i++) {
        cout << "what["<<i<<"] = "<<what[i]<<endl;
      }
        /*
        //cout << "analysisName="<<string(analysisName)<<endl;
        // Convert the name of the analysis to all lower-case
        char* p;
        for(p=analysisName; *p; p++) *p = tolower(*p);

        // Remote any trailing line breaks and/or carriage returns
        while(p!=analysisName && (*p=='\n' || *p=='\r')) { *p--; }
        *p = '\0';

        printf("analysisName=%s\n", analysisName);
        pragmaCmd.append(analysisName);*/
        
      ROSE_ASSERT(what.size()==2);
      pragmaCmd.append(what[1]);
      
      /*ComposedAnalysis* a = NULL;
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
      */
    }
  }

  lcComposer = as_xpr(icase("loosechain")) | icase("lc");
  lpComposer = as_xpr(icase("loosepar"))   | icase("lp");
  composer = by_ref(lcComposer) | by_ref(lpComposer);
  //composer = as_xpr(icase("loosechain")) | icase("lc") | icase("loosepar") | icase("lp");
  
  cpAnalysis = as_xpr(icase("constantpropagationanalysis")) | icase("constantpropagation") | icase("cp");
  ldAnalysis = as_xpr(icase("livedeadmemanalysis"))         | icase("livedead")            | icase("ld");
  oaAnalysis = as_xpr(icase("livedeadmemanalysis"))         | icase("orthoarray")          | icase("oa");
  dpAnalysis = as_xpr(icase("deadpathelimanalysis"))        | icase("deadpath")            | icase("dp");
  ptAnalysis = as_xpr(icase("pointstoanalysis"))            | icase("pointsto")            | icase("pt");
  analysis = by_ref(cpAnalysis) | by_ref(ldAnalysis) | by_ref(oaAnalysis) | by_ref(dpAnalysis) | by_ref(ptAnalysis);
  /*analysis = as_xpr(icase("constantpropagationanalysis")) | icase("constantpropagation") | icase("cp") | 
             as_xpr(icase("livedeadmemanalysis"))         | icase("livedead")            | icase("ld") |
             as_xpr(icase("livedeadmemanalysis"))         | icase("orthoarray")          | icase("oa") |
             as_xpr(icase("deadpathelimanalysis"))        | icase("deadpath")            | icase("dp");*/
  analysisList = '(' >> *_s >> (by_ref(analysis) | by_ref(compSpec)) >> *(*_s >> "," >> *_s >> (by_ref(analysis) | by_ref(compSpec))) >> *_s >> ')';
  compSpec = *_s >> by_ref(composer) >> *_s >> analysisList >> *_s;

/*
  // Remove leading spaces
  unsigned int startNonSpace=0;
  while(startNonSpace<pragmaCmd.length() && (pragmaCmd[startNonSpace]==' ' || pragmaCmd[startNonSpace]=='\t')) startNonSpace++;
  if(startNonSpace>0) pragmaCmd.erase(0, startNonSpace);

  // Remove trailing spaces
  unsigned int endNonSpace=pragmaCmd.length()-1;
  while(endNonSpace>=0 && (pragmaCmd[endNonSpace]==' ' || pragmaCmd[endNonSpace]=='\t')) endNonSpace--;
  if(endNonSpace<pragmaCmd.length()-1) pragmaCmd.erase(endNonSpace+1);*/

  cout << "pragmaCmd = \""<<pragmaCmd<<"\"\n";

  // If this is a command for the compositional framework
  if(pragmaCmd.size()>0) {
    smatch what;
    if(regex_match(pragmaCmd, what, compSpec)) {
      cout << "MATCH composer\n";
      list<ComposedAnalysis*>  mySubAnalyses;
      Composer* rootComposer = NULL;
      output_nested_results::composerType rootComposerType = output_nested_results::unknown;
      
      checkDataflowInfoPass* cdip = new checkDataflowInfoPass();
      
      output_nested_results ons(0, rootComposerType, mySubAnalyses, &rootComposer, cdip);
      std::for_each(what.nested_results().begin(),
                    what.nested_results().end(),
                    ons);
      ROSE_ASSERT(rootComposer!=NULL);
      ((ChainComposer*)rootComposer)->runAnalysis();
      
      cout << "rootComposer="<<rootComposer<<" cdip->getNumErrors()="<<cdip->getNumErrors()<<endl;
      if(cdip->getNumErrors() > 0) cout << cdip->getNumErrors() << " Errors Reported!"<<endl;
      else                        cout << "PASS"<<endl;

    } else
      cout << "FAIL composer\n";
  }
  // passed by command line argument
  // currently command line assumes loose sequential
  else if(analyses.size() > 0)
  {
    checkDataflowInfoPass* cdip = new checkDataflowInfoPass();
    ChainComposer cc(analyses, cdip, true);
    cc.runAnalysis();
    if(cdip->getNumErrors() > 0) cout << cdip->getNumErrors() << "Errors Reported!" << endl;
    else                         cout << "PASS" << endl;
  }
  /*checkDataflowInfoPass* cdip = new checkDataflowInfoPass();
  
  ChainComposer cc(analyses, cdip, false);
  cc.runAnalysis();*/

  printf("==========  E  N  D  ==========\n");
  
  return 0;
}

