#include "AnalysisDebuggingUtils.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <stdlib.h>

#include "genericDataflowCommon.h"
#include "VirtualCFGIterator.h"
#include "cfgUtils.h"
#include "CallGraphTraverse.h"
#include "analysisCommon.h"
#include "compose.h"
#include "functionState.h"
#include "latticeFull.h"
#include "printAnalysisStates.h"
#include "partitions.h"
#include "rose_config.h"
#include <sys/stat.h>
#include <sys/types.h>

using namespace std;
using namespace dataflow;
using namespace VirtualCFG;

//------------ function level dot graph output ----------------
// this is purposely put outside of the namespace scope for simplicity 
class analysisStatesToDOT : virtual public UnstructuredPassIntraAnalysis
{
  private:
    //    LiveDeadVarsAnalysis* lda; // reference to the source analysis
    Analysis* lda; // reference to the source analysis
    void printEdge(PartEdgePtr e); // print data flow edge
    void printNode(PartPtr n, std::string state_string); // print date flow node
  public:
    void visit(const Function& func, PartPtr p, NodeState& state); // visitor function
    std::ostream* ostr; 
    // must transfer the custom filter from l, or the default filter will kick in!
    analysisStatesToDOT (ComposedAnalysis* l): Analysis(l->filter), UnstructuredPassIntraAnalysis(l), lda(l) { }; 
};

void analysisStatesToDOT::printEdge (PartEdgePtr e)
{
  (*ostr) << e->source()->CFGNodes().begin()->id() << " -> " << e->target()->CFGNodes().begin()->id() << " [label=\"" << escapeString(e->str()) <<
    "\", style=\"" << "solid" << "\"];\n";
}

void analysisStatesToDOT::printNode(PartPtr part, std::string state_string)
{
  std::string id = part->CFGNodes().begin()->id();  // node id
  std::string nodeColor = "black";

  if (part->maySgNodeAny<SgStatement>())
    nodeColor = "blue";
  else if (part->maySgNodeAny<SgExpression>())
    nodeColor = "green";
  else if (part->maySgNodeAny<SgInitializedName>())
    nodeColor = "red";

  // node_id [label="", color="", style=""]
  (*ostr) << id << " [label=\""  << part->str() <<"\\n" << escapeString (state_string) << "\", color=\"" << nodeColor <<
    "\", style=\"solid\"];\n";
}

// This will be visited only once? Not sure, better check
void
analysisStatesToDOT::visit(const Function& func, PartPtr part, NodeState& state)
{ 
  std::string state_str = state.str( lda, " ");
  printNode(part, state_str);
  std::list <PartEdgePtr> outEdges = part->outEdges();
  for(list<PartEdgePtr>::iterator edge=outEdges.begin(); edge!=outEdges.end(); edge++)
  {
    printEdge(*edge);
  }
}
//------------ call graph level  driver ----------------
class IntraAnalysisResultsToDotFiles: public UnstructuredPassInterAnalysis
{
  public:
    // internal function level dot generation, a name for the output file
    IntraAnalysisResultsToDotFiles(IntraProceduralAnalysis & funcToDot)
      : InterProceduralAnalysis(&funcToDot), UnstructuredPassInterAnalysis(funcToDot){ }
    // For each function, call function level Dot file generator to generate separate dot file
    void runAnalysis() ;
};

void IntraAnalysisResultsToDotFiles::runAnalysis()
{
  set<FunctionState*> allFuncs = FunctionState::getAllDefinedFuncs();
  // Go through functions one by one, call an intra-procedural analysis on each of them
  // iterate over all functions with bodies
  for(set<FunctionState*>::iterator it=allFuncs.begin(); it!=allFuncs.end(); it++)
  {
    FunctionState* fState = *it;
    // compose the output file name as filename_mangled_function_name.dot
    Function *func = & (fState->getFunc());
    assert (func != NULL);
    SgFunctionDefinition* proc = func->get_definition();
    string file_name = StringUtility::stripPathFromFileName(proc->get_file_info()->get_filename());
    string file_func_name= file_name+ "_"+proc->get_mangled_name().getString();

    string full_output = file_func_name +"_cfg.dot";
    std::ofstream ostr(full_output.c_str());

    ostr << "digraph " << "mygraph" << " {\n";
    analysisStatesToDOT* dot_analysis = dynamic_cast <analysisStatesToDOT*> (intraAnalysis);
    assert (dot_analysis != NULL);
    dot_analysis->ostr = &ostr;
    dot_analysis->runAnalysis(fState->func, &(fState->state));
    ostr << "}\n";
  }

}

/******************
 ***** dbgBuf *****
 ******************/

namespace Dbg {

dbgBuf::dbgBuf()
{
  init(NULL);
}

dbgBuf::dbgBuf(std::streambuf* baseBuf)
{
  init(baseBuf);
}

void dbgBuf::init(std::streambuf* baseBuf)
{
  this->baseBuf = baseBuf;
  synched = true;
  ownerAccess = false;
  numOpenAngles = 0;
  //numDivs = 0;
  parentDivs.empty();
  parentDivs.push_back(0);
  //justSynched = false;
  needIndent = false;
  indents.push_back(std::list<std::string>());
  //cout << "Initially parentDivs (len="<<parentDivs.size()<<")\n";
}

// This dbgBuf has no buffer. So every character "overflows"
// and can be put directly into the teed buffers.
int dbgBuf::overflow(int c)
{
  //cout << "overflow\n";
  if (c == EOF)
  {
     return !EOF;
  }
  else
  {
    int const r1 = baseBuf->sputc(c);
    return r1 == EOF ? EOF : c;
  }
}

// Prints the indent to the stream buffer, returns 0 on success non-0 on failure
/*int dbgBuf::printIndent()
{
  string indent="";
  for(list<string>::iterator i=indents.begin(); i!=indents.end(); i++) indent+=*i;
  int r = baseBuf->sputn(indent.c_str(), indent.length());
  if(r!=indent.length()) return -1;
  return 0;
}*/

// Prints the given string to the stream buffer
int dbgBuf::printString(string s)
{
  int r = baseBuf->sputn(s.c_str(), s.length());
  if(r!=(int)s.length()) return -1;
  return 0;
}

// Get the current indentation depth within the current div
int dbgBuf::getIndentDepth()
{
  return indents.rbegin()->size();
}

// Get the current indentation within the current div
std::string dbgBuf::getIndent()
{
  string out = "";
  if(indents.size()>0) {
    for(std::list<std::string>::iterator it=indents.rbegin()->begin(); it!=indents.rbegin()->end(); it++)
      out += *it;
    //cout << "getIndent("<<out<<")"<<endl;
  }
  return out;
}

// Add more indentation within the current div
void dbgBuf::addIndent(std::string indent)
{
  ROSE_ASSERT(indents.size()>0);
  indents.rbegin()->push_back(indent);
  /*if(justSynched) {
    int ret = printString(indent); if(ret != 0) return;
  }*/
}

// Remove the most recently added indent within the current div
void dbgBuf::remIndent()
{
  ROSE_ASSERT(indents.size()>0);
  ROSE_ASSERT(indents.rbegin()->size()>0);
  indents.rbegin()->pop_back();
}

streamsize dbgBuf::xsputn(const char * s, streamsize n)
{
  /*cout << "dbgBuf::xsputn"<<endl;
  cout << "        funcs.size()="<<funcs.size()<<", baseBuf="<<baseBuf<<endl;
  cout.flush();*/
  
  // Reset justSynched since we're now adding new characters after the last line break, meaning that no 
  // additional indent will be needed for this line
  //justSynched = false;
  
  if(needIndent) {
    int ret = printString(getIndent()); if(ret != 0) return 0;
    needIndent = false;
  }
  
//      cout << "xputn() ownerAccess="<<ownerAccess<<" n="<<n<<" s=\""<<string(s)<<"\"\n";
  /*if(synched && !ownerAccess) {
          int ret2 = printIndent();
          if(ret2 != 0) return 0;
  }*/
  // If the owner is printing, output their text exactly
  if(ownerAccess) {
    return baseBuf->sputn(s, n);
  // Otherwise, replace all line-breaks with <BR>'s
  } else {
    int ret;
    int i=0;
    char br[]="<BR>\n";
    char space[]=" ";
    char spaceHTML[]="&nbsp;";
    char tab[]="\t";
    char tabHTML[]="&#09;";
    //char lt[]="&lt;";
    //char gt[]="&gt;";
    while(i<n) {
      int j;
      for(j=i; j<n && s[j]!='\n' && s[j]!=' ' && s[j]!='\t'/* && s[j]!='<' && s[j]!='>'*/; j++) {
        if(s[j]=='<') numOpenAngles++;
        else if(s[j]=='>') numOpenAngles--;
      }
  //                      cout << "char=\""<<s[j]<<"\" numOpenAngles="<<numOpenAngles<<"\n";
      // Send out all the bytes from the start of the string or the 
      // last line-break until this line-break
      if(j-i>0) {
        if(needIndent) {
          int ret = printString(getIndent()); if(ret != 0) return 0;
          needIndent = false;
        }

        ret = baseBuf->sputn(&(s[i]), j-i);
        if(ret != (j-i)) return 0;
        //cout << "   printing char "<<i<<" - "<<j<<"\n";
      }

      if(j<n) {
        // If we're at at line-break Send out the line-break
        if(s[j]=='\n') {
          ret = baseBuf->sputn(br, sizeof(br)-1);
          if(ret != sizeof(br)-1) return 0;
          //string indent = getIndent();
          //cout << "New Line indent=\""<<indent<<"\""<<endl;
          //baseBuf->sputn(indent.c_str(), indent.size());
          needIndent = true;
        } else if(s[j]==' ') {
          // If we're at a space and not inside an HTML tag, replace it with an HTML space escape code
          if(numOpenAngles==0) {
            ret = baseBuf->sputn(spaceHTML, sizeof(spaceHTML)-1);
            if(ret != sizeof(spaceHTML)-1) return 0;
          // If we're inside an HTML tag, emit a regular space character
          } else {
            ret = baseBuf->sputn(space, sizeof(space)-1);
            if(ret != sizeof(space)-1) return 0;
          }
        } else if(s[j]=='\t') {
          // If we're at a tab and not inside an HTML tag, replace it with an HTML tab escape code
          if(numOpenAngles==0) {
            ret = baseBuf->sputn(tabHTML, sizeof(tabHTML)-1);
            if(ret != sizeof(tabHTML)-1) return 0;
          // If we're inside an HTML tag, emit a regular tab character
          } else {
            ret = baseBuf->sputn(tab, sizeof(tab)-1);
            if(ret != sizeof(tab)-1) return 0;
          }
        }/* else if(s[j]=='<') {
          ret = baseBuf->sputn(lt, sizeof(lt)-1);
          if(ret != sizeof(lt)-1) return 0;
        } else if(s[j]=='>') {
          ret = baseBuf->sputn(gt, sizeof(gt)-1);
          if(ret != sizeof(gt)-1) return 0;
        }*/
        //cout << "   printing <BR>\n";
      }

      // Point i to immediately after the line-break
      i = j+1;
    }

    return n;
  }
}


// Sync buffer.
int dbgBuf::sync()
{
  int r = baseBuf->pubsync();
  if(r!=0) return -1;

  if(synched && !ownerAccess) {
    int ret;
    ret = printString("<br>\n");    if(ret != 0) return 0;
    /*ret = printString(getIndent()); if(ret != 0) return 0;
    justSynched=true;*/
    synched = false;
  }
  synched = true;

  return 0;
}

// Switch between the owner class and user code writing text
void dbgBuf::userAccessing() { ownerAccess = false; synched = true; }
void dbgBuf::ownerAccessing() { ownerAccess = true; synched = true; }

void dbgBuf::enterFunc(string funcName/*, string indent*/)
{
  //cout << "enterFunc("<<funcName<<")"<<endl;
  funcs.push_back(funcName);
  
  //indents.push_back(indent);
  // Increment the index of this function within its parent
  //cout << "Incrementing parentDivs (len="<<parentDivs.size()<<") from "<<*(parentDivs.rbegin())<<" to ";
  (*(parentDivs.rbegin()))++;
  /*cout << *(parentDivs.rbegin())<<": div=";
  ostringstream divName;
  for(list<int>::iterator d=parentDivs.begin(); d!=parentDivs.end(); d++) {
          if(d!=parentDivs.end())
                  divName << *d << "_";
  }
  cout << divName.str()<<"\n";*/
  // Add a new level to the parentDivs list, starting the index at 0
  parentDivs.push_back(0);
  //numDivs++;

  indents.push_back(std::list<std::string>());
}

void dbgBuf::exitFunc(string funcName)
{
  //cout << "exitFunc("<<funcName<<")"<<endl;
  if(funcName != funcs.back()) { 
    cout << "dbgStream::exitFunc() ERROR: exiting from function "<<funcName<<" which is not the most recent function entered!\n";
    cout << "funcs=\n";
    for(list<string>::iterator f=funcs.begin(); f!=funcs.end(); f++)
      cout << "    "<<*f<<"\n";
    cout.flush();
    baseBuf->pubsync();
    exit(-1);
  }
  funcs.pop_back();
  //indents.pop_back();
  parentDivs.pop_back();
  indents.pop_back();
}

// Returns the depth of enterFunc calls that have not yet been matched by exitFuncCalls
int dbgBuf::funcDepth()
{ return funcs.size(); }

/*********************
 ***** dbgStream *****
 *********************/

// Returns a string that contains n tabs
string tabs(int n)
{
  string s;
  for(int i=0; i<n; i++)
    s+="\t";
  return s;
}

dbgStream::dbgStream() : std::ostream(&defaultFileBuf), initialized(false)
{}

dbgStream::dbgStream(string title, string dbgFileName, string workDir, string imgPath)
        : std::ostream(&defaultFileBuf)
{
  init(title, dbgFileName, workDir, imgPath);
}

void dbgStream::init(string title, string dbgFileName, string workDir, string imgPath)
{
  // Initialize fileLevel with a 0 to make it possible to count top-level files
  fileLevel.push_back(0);
  
  this->title       = title;
  this->dbgFileName = dbgFileName;
  this->workDir     = workDir;
  this->imgPath     = imgPath;

  // Initialize colors with a list of light pastel colors 
  colors.push_back("FF97E8");
  colors.push_back("75D6FF");
  colors.push_back("72FE95");
  colors.push_back("8C8CFF");
  colors.push_back("57BCD9");
  colors.push_back("99FD77");
  colors.push_back("EDEF85");
  colors.push_back("B4D1B6");
  colors.push_back("FF86FF");
  colors.push_back("4985D6");
  colors.push_back("D0BCFE");
  colors.push_back("FFA8A8");
  colors.push_back("A4F0B7");
  colors.push_back("F9FDFF");
  colors.push_back("FFFFC8");
  colors.push_back("5757FF");
  colors.push_back("6FFF44");

  numImages++;
  
  
  enterFileLevel(title, true);
  
  initialized = true;
}

dbgStream::~dbgStream()
{
  if (!initialized)
    return;

  exitFileLevel(title, true);

  // Run CPP on the output file to incorporate the summary into the main file
  //{
  //      ostringstream cmd;
  //      cmd << "mv "<<dbgFileName<<" "<<dbgFileName<<".tmp";
  //      system(cmd.str().c_str());
  //}
  //
  //{
  //      ostringstream cmd;
  //      cmd << "cpp -E -x c -P -C "<<dbgFileName<<".tmp > "<<dbgFileName;
  //      //cout << cmd.str()<<"\n";
  //      system(cmd.str().c_str());
  //}
}

// Returns the string representation of the current file level
string dbgStream::fileLevelStr()
{
  ostringstream oss;
  for(list<int>::iterator i=fileLevel.begin(); i!=fileLevel.end(); ) { 
    oss << *i;
    i++;
    if(i!=fileLevel.end()) oss << "-";
  }
  return oss.str();
}

// Enter a new file level
void dbgStream::enterFileLevel(string flTitle, bool topLevel)
{
  ROSE_ASSERT(fileLevel.size()>0);
  // Increment the index of this file within the current nesting level
  (*fileLevel.rbegin())++;
  // Add a fresh level to the fileLevel list
  fileLevel.push_back(0);
  
  ostringstream indexFileName;   indexFileName  << dbgFileName << "."     << fileLevelStr() << ".html";
  ostringstream detailAbsFName;  detailAbsFName << workDir << "/detail."  << fileLevelStr();
  ostringstream detailRelFName;  detailRelFName            << "detail."  << fileLevelStr();
  ostringstream sumAbsFName;     sumAbsFName    << workDir << "/summary." << fileLevelStr();
  ostringstream sumRelFName;     sumRelFName               << "summary."  << fileLevelStr();
  
  //cout << "enterFileLevel("<<flTitle<<") topLevel="<<topLevel<<" #flTitles="<<flTitles.size()<<" #fileLevel="<<fileLevel.size()<<endl;
  // Add a new function level within the parent file that will refer to the child file
  if(!topLevel) enterFunc(flTitle, detailRelFName.str(), sumRelFName.str());
  flTitles.push_back(flTitle);
  
  ofstream indexFile;
  try {
    indexFile.open(indexFileName.str().c_str());
  } catch (ofstream::failure e)                                                                     
  { cout << "dbgStream::init() ERROR opening file \""<<indexFileName<<"\" for writing!"; exit(-1); }
  indexFiles.push_back(&indexFile);
  
  indexFile << "<frameset cols=\"20%,80%\">\n";
  indexFile << "\t<frame src=\""<<sumRelFName.str()<<".html\" name=\"summary\"/>\n";
  indexFile << "\t<frame src=\""<<detailRelFName.str() <<".html\" name=\"detail\"/>\n";
  indexFile << "</frameset>\n";
  indexFile.close();
  
  // Print out a link to the child file
  //if(!topLevel) (*this)<<"<a href=\""<<indexFileName.str()<<"\">LINK</a>"<<endl;
  
  ofstream *dbgFile = new ofstream();
  try {
    dbgFile->open((detailAbsFName.str()+".body").c_str());
  } catch (ofstream::failure e)
  { cout << "dbgStream::init() ERROR opening file \""<<detailAbsFName.str()<<"\" for writing!"; exit(-1); }
  dbgFiles.push_back(dbgFile);
  detailFileRelFNames.push_back(detailRelFName.str()+".body");
  
  //std::ostream::rdbuf(&buf);
  //buf.init(dbgFile.rdbuf());
  dbgBuf *nextBuf = new dbgBuf(dbgFile->rdbuf());
  fileBufs.push_back(nextBuf);
  // Call the paper class initialization function to connect it dbgBuf of the child file
  ostream::init(nextBuf);

  ofstream *summaryFile = new ofstream();
  try {
    //cout << "    sumAbsFName="<<fullFileName.str()<<endl;
    summaryFile->open((sumAbsFName.str()+".body").c_str());
  } catch (ofstream::failure e)
  { cout << "dbgStream::init() ERROR opening file \""<<sumAbsFName.str()<<"\" for writing!"; exit(-1); }
  summaryFiles.push_back(summaryFile);
  
  // Start the table in the current summary file
  (*summaryFiles.back()) << "\t\t<table width=\"100%\">\n";
  (*summaryFiles.back()) << "\t\t\t<tr width=\"100%\"><td width=50></td><td width=\"100%\">\n";
  

  /*nextBuf->ownerAccessing();
  printDetailFileHeader(flTitle);
  nextBuf->userAccessing();*/
  
  // Create the html file container for the summary html text
  printSummaryFileContainerHTML(sumAbsFName.str(), sumRelFName.str(), flTitle);
  
  // Create the html file container for the detail html text
  printDetailFileContainerHTML(detailAbsFName.str(), detailRelFName.str(), flTitle);
}

// Exit a current file level
void dbgStream::exitFileLevel(string flTitle, bool topLevel)
{
  //cout << "exitFileLevel("<<flTitle<<") topLevel="<<topLevel<<" #flTitles="<<flTitles.size()<<" #fileLevel="<<fileLevel.size()<<endl;
  ROSE_ASSERT(fileLevel.size()>1);
  
  /*fileBufs.back()->ownerAccessing();
  printDetailFileTrailer();
  fileBufs.back()->userAccessing();*/
  dbgFiles.back()->close();
  
  // Complete the table in the current summary file
  (*summaryFiles.back()) << "\t\t\t</td></tr>\n";
  (*summaryFiles.back()) << "\t\t</table>\n";
  summaryFiles.back()->close();
  
  fileLevel.pop_back();
  indexFiles.pop_back();
  dbgFiles.pop_back();
  detailFileRelFNames.pop_back();
  //buf.init(dbgFiles.back()->rdbuf());
  summaryFiles.pop_back();
  fileBufs.pop_back();
  // Call the ostream class initialization function to connect it dbgBuf of the parent file
  ostream::init(fileBufs.back());
  
  // Exit the function level within the parent file
  ROSE_ASSERT(flTitle == flTitles.back());
  if(!topLevel) exitFunc(flTitle);
  
  flTitles.pop_back();
}

void dbgStream::printSummaryFileContainerHTML(string absoluteFileName, string relativeFileName, string title)
{
  ofstream sum;
  ostringstream fullFName; fullFName << absoluteFileName << ".html";
  try { sum.open(fullFName.str().c_str()); }
  catch (ofstream::failure e)
  { cout << "dbgStream::init() ERROR opening file \""<<fullFName.str()<<"\" for writing!"; exit(-1); }
  
  sum << "<html>\n";
  sum << "\t<head>\n";
  sum << "\t<title>"<<title<<"</title>\n";
  sum << "\t<script type=\"text/javascript\">\n";
  sum << "\tfunction loadURLIntoDiv(doc, url, divName) {\n";
        sum << "\t\tvar xhr= new XMLHttpRequest();\n";
        sum << "\t\txhr.open('GET', url, true);\n";
        sum << "\t\txhr.onreadystatechange= function() {\n";
        sum << "\t\t\t//Wait until the data is fully loaded\n";
        sum << "\t\t\tif (this.readyState!==4) return;\n";
        sum << "\t\t\tdoc.getElementById(divName).innerHTML= this.responseText;\n";
        sum << "\t\t};\n";
        sum << "\t\txhr.send();\n";
        sum << "\t}\n";
  sum << "\n";
  sum << "\tfunction focusLinkHref(divID) {\n";
  sum << "\t\ttop.detail.location = \"detail." << fileLevelStr() << ".html#anchor\"+divID;\n";
  sum << "\t}\n";
  sum << "\n";
  sum << "// Set this page's initial contents\n";
  sum << "window.onload=function () { loadURLIntoDiv(document, '"<<relativeFileName<<".body', 'detailContents'); }\n";
  sum << "\t</script>\n";
  sum << "\t</head>\n";
  sum << "\t<body>\n";
  sum << "\t<h1>Summary</h1>\n";
  sum << "\t<div id='detailContents'></div>\n";
  sum << "\t</body>\n";
  sum << "</html>\n\n";
}

void dbgStream::printDetailFileContainerHTML(string absoluteFileName, string relativeFileName, string title)
{
  ofstream det;
  ostringstream fullFName; fullFName << absoluteFileName << ".html";
  try { det.open(fullFName.str().c_str()); }
  catch (ofstream::failure e)
  { cout << "dbgStream::init() ERROR opening file \""<<fullFName.str()<<"\" for writing!"; exit(-1); }
  
  det << "<html>\n";
  det << "\t<head>\n";
  det << "\t<title>"<<title<<"</title>\n";
  det << "\t<STYLE TYPE=\"text/css\">\n";
  det << "\tBODY\n";
  det << "\t\t{\n";
  det << "\t\tfont-family:courier;\n";
  det << "\t\t}\n";
  det << "\t.hidden { display: none; }\n";
  det << "\t.unhidden { display: block; }\n";
  det << "\t</style>\n";
  det << "\t<script type=\"text/javascript\">\n";
  det << "\tfunction unhide(divID) {\n";
  det << "\t  var parentDiv = document.getElementById(\"div\"+divID);\n";
  det << "\t\tif (parentDiv) {\n";
  det << "\t\t\t// Hide the parent div\n";
  det << "\t\t\tparentDiv.className=(parentDiv.className=='hidden')?'unhidden':'hidden';\n";
  det << "\t\t\t// Get all the tables\n";
  det << "\t\t\tvar childTbls = document.getElementsByTagName(\"table\");\n";
  det << "\t\t\tcondition = new RegExp(\"table\"+divID+\"*\");\n";
  det << "\t\t\tfor (var i=0; i<childTbls.length; i++){ \n";
  det << "\t\t\t\tvar child = childTbls[i];\n";
  det << "\t\t\t\t// Set the visibility status of each child table to be the same as its parent div\n";
  det << "\t\t\t\tif (\"table\"+divID!=child.id && child.nodeType==1 && child.id!=undefined && child.id.match(condition)) {\n";
  det << "\t\t\t\t    child.className=parentDiv.className;\n";
  det << "\t\t\t\t}\n";
  det << "\t\t\t}\n";
  det << "\t\t}\n";
  det << "\t}\n";
  det << "\n";
  det << "\tfunction loadURLIntoDiv(doc, url, divName) {\n";
        det << "\t\tvar xhr= new XMLHttpRequest();\n";
        det << "\t\txhr.open('GET', url, true);\n";
        det << "\t\txhr.onreadystatechange= function() {\n";
        det << "\t\t\t//Wait until the data is fully loaded\n";
        det << "\t\t\tif (this.readyState!==4) return;\n";
        det << "\t\t\tdoc.getElementById(divName).innerHTML= this.responseText;\n";
        det << "\t\t};\n";
        det << "\t\txhr.send();\n";
        det << "\t}\n";
  det << "\n";
  det << "\tfunction highlightLink(divID, newcolor) {\n";
  det << "\t\tvar sumLink = top.summary.document.getElementById(\"link\"+divID);\n";
  det << "\t\tsumLink.style.backgroundColor= newcolor;\n";
  det << "\t}\n";
  det << "\tfunction focusLink(divID, e) {\n";
  det << "\t\te = e || window.event;\n";
  det << "\t\tif('cancelBubble' in e) {\n";
  det << "\t\t\te.cancelBubble = true;\n";
  det << "\t\t\ttop.summary.location = \"summary." << fileLevelStr() << ".html#anchor\"+divID;\n";
  det << "\t\t}\n";
  det << "\t}\n";
  det << "\tfunction showNodes(imgNum, str) {\n";
  det << "\t\tvar f = document.getElementById('imgFrame_'+imgNum);\n";
  det << "\t\tf.width = \"100%\"\n";
  det << "\t\tf.height = 100;\n";
  det << "\t\tf = (f.contentWindow ? f.contentWindow : (f.contentDocument.document ? f.contentDocument.document : f.contentDocument));\n";
  det << "\t\tf.document.open();\n";
  det << "\t\tf.document.write(\"~/Compilers/focusOnNodes.pl "<<imgPath<<"/image_\"+imgNum+\".dot \"+str+\" > "<<imgPath<<"/image_\"+imgNum+\".focus.dot ; dot -Tsvg -o"<<imgPath<<"/image_\"+imgNum+\".focus.svg "<<imgPath<<"/image_\"+imgNum+\".focus.dot\");\n";
  det << "\t\tf.document.close();\n";
  det << "\t}\n";
  det << "\n";
  det << "// Set this page's initial contents\n";
  det << "window.onload=function () { loadURLIntoDiv(document, '"<<relativeFileName<<".body', 'detailContents'); }\n";
  det << "\t</script>\n";
  det << "\t</head>\n";
  det << "\t<body>\n";
  det << "\t<h1>"<<title<<"</h1>\n";
  det << "\t\t<table width=\"100%\">\n";
  det << "\t\t\t<tr width=\"100%\"><td width=50></td><td width=\"100%\">\n";
  det << "\t\t\t<div id='detailContents'></div>\n";
  det << "\t\t\t</td></tr>\n";
  det << "\t\t</table>\n";
  det << "\t</body>\n";
  det << "</html>\n\n";
  
  det.close();
}

void dbgStream::enterFunc(string funcName, string detailContentURL, string summaryContentURL)
{
  //if(contentURL != "") { cout << "enterFunc("<<funcName<<", "<<contentURL<<")"<<endl; }
  // Either both URLs are provided or neither is
  ROSE_ASSERT((detailContentURL=="" && summaryContentURL=="") ||
              (detailContentURL!="" && summaryContentURL!=""));
  
  fileBufs.back()->ownerAccessing();

  fileBufs.back()->enterFunc(funcName/*, indent*/);
  colorIdx++; // Advance to a new color for this func

  ostringstream divName;
  for(std::list<dbgBuf*>::iterator fb=fileBufs.begin(); fb!=fileBufs.end(); ) {
    //for(list<int>::iterator d=fileBufs.back()->parentDivs.begin(); d!=fileBufs.back()->parentDivs.end(); ) {
    for(list<int>::iterator d=(*fb)->parentDivs.begin(); d!=(*fb)->parentDivs.end(); ) {
      divName << *d;
      d++;
      //if(d!=fileBufs.back()->parentDivs.end()) {
      if(d!=(*fb)->parentDivs.end()) divName << "_";
    }
    fb++;
    if(fb!=fileBufs.end()) divName << ":";
  }
  
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size())<<"</td></tr>\n";
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size())<<"<tr width=\"100%\"><td width=50></td><td width=\"100%\">\n";
  //*(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<table bgcolor=\"#"<<colors[(fileBufs.back()->funcs.size()-1)%colors.size()]<<"\" width=\"100%\" id=\"table"<<divName.str()<<"\" style=\"border:1px solid white\" onmouseover=\"this.style.border='1px solid black'; highlightLink('"<<divName.str()<<"', '#F4FBAA');\" onmouseout=\"this.style.border='1px solid white'; highlightLink('"<<divName.str()<<"', '#FFFFFF');\" onclick=\"focusLink('"<<divName.str()<<"', event);\">\n";
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<table bgcolor=\"#"<<colors[(colorIdx-1)%colors.size()]<<"\" width=\"100%\" id=\"table"<<divName.str()<<"\" style=\"border:1px solid white\" onmouseover=\"this.style.border='1px solid black'; highlightLink('"<<divName.str()<<"', '#F4FBAA');\" onmouseout=\"this.style.border='1px solid white'; highlightLink('"<<divName.str()<<"', '#FFFFFF');\" onclick=\"focusLink('"<<divName.str()<<"', event);\">\n";
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<tr width=\"100%\"><td width=50></td><td width=\"100%\"><h2><a name=\"anchor"<<divName.str()<<"\" href=\"javascript:unhide('"<<divName.str()<<"');\">";
  fileBufs.back()->userAccessing();
  *(this) << funcName;
  fileBufs.back()->ownerAccessing();
  *(this) << "</a>\n";
  if(detailContentURL != "") {
    *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1);
    *(this) << "<a href=\"javascript:loadURLIntoDiv(top.detail.document, '"<<detailContentURL<<".body', 'div"<<divName.str()<<"'); loadURLIntoDiv(top.summary.document, '"<<summaryContentURL<<".body', 'sumdiv"<<divName.str()<<"')\">";
    *(this) << "<img src=\"img/divDL.gif\" width=25 height=35></a>\n";
    *(this) << "<a target=\"_top\" href=\"index."<<fileLevelStr()<<".html\">";
    *(this) << "<img src=\"img/divGO.gif\" width=35 height=25></a>\n";
  }
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"</h2></td></tr>\n";
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<tr width=\"100%\"><td width=50></td><td width=\"100%\"><div id=\"div"<<divName.str()<<"\" class=\"unhidden\">\n";
  this->flush();

  //summaryFiles.back() << "<li><a target=\"detail\" href=\"detail.html#"<<divName.str()<<"\">"<<funcName<<"</a><br>\n";
  //summaryFiles.back() << "<ul>\n";
  *summaryFiles.back() << "\t\t\t"<<tabs(fileBufs.back()->funcs.size())<<"</td></tr>\n";
  *summaryFiles.back() << "\t\t\t"<<tabs(fileBufs.back()->funcs.size())<<"<tr width=\"100%\"><td width=50></td><td width=\"100%\">\n";
  *summaryFiles.back() << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<table width=\"100%\" style=\"border:0px\">\n";
  *summaryFiles.back() << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<tr width=\"100%\"><td width=50></td><td id=\"link"<<divName.str()<<"\" width=\"100%\">";
  *summaryFiles.back() <<     "<a name=\"anchor"<<divName.str()<<"\" href=\"javascript:focusLinkHref('"<<divName.str()<<"')\">"<<funcName<<"</a> ("<<divName.str()<<")</td></tr>\n";
  *summaryFiles.back() << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<tr width=\"100%\"><td width=50></td><td width=\"100%\">\n";
  /*// If the corresponding div in the detail page may be filled in with additional content, we create another summary
  // div to fill in with the corresponding summary content
  if(detailContentURL != "") */*summaryFiles.back() << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<div id=\"sumdiv"<<divName.str()<<"\">\n";
  summaryFiles.back()->flush();

  fileBufs.back()->userAccessing();
}

void dbgStream::exitFunc(string funcName)
{
  //{ cout << "exitFunc("<<funcName<<", "<<contentURL<<")"<<endl; }
  fileBufs.back()->ownerAccessing();
  fileBufs.back()->exitFunc(funcName);
  colorIdx--; // Return to the last color for this func's parent
  ROSE_ASSERT(colorIdx>=0);
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"</td></tr>\n";
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"</table>\n";
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size())<<"</td></tr>\n";
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size())<<"<tr width=\"100%\"><td width=50></td><td width=\"100%\">\n";
  this->flush();
  fileBufs.back()->userAccessing();

  //summaryFiles.back() << "</ul>\n";
  *summaryFiles.back() << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"</div></td></tr>\n";
  *summaryFiles.back() << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"</table>\n";
  *summaryFiles.back() << "\t\t\t"<<tabs(fileBufs.back()->funcs.size())<<"</td></tr>\n";
  *summaryFiles.back() << "\t\t\t"<<tabs(fileBufs.back()->funcs.size())<<"<tr width=\"100%\"><td width=50></td><td width=\"100%\">\n";
  summaryFiles.back()->flush();
}

// Adds an image to the output with the given extension and returns the path of this image
// so that the caller can write to it.
string dbgStream::addImage(string ext)
{
  ostringstream imgFName; imgFName << imgPath << "/image_" << numImages << "." << ext;
  fileBufs.back()->ownerAccessing();
  *(this) << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<img src="<<imgFName.str()<<">\n";
  fileBufs.back()->userAccessing();
  return imgFName.str();
}

// Given a reference to an object that can be represented as a dot graph, create an image from it and add it to the output.
// Return the path of the image.
string dbgStream::addDOT(dottable& obj)
{
  ostringstream imgFName; imgFName << "dbg_imgs/image_" << numImages << ".svg";
  ostringstream graphName; graphName << "graph_"<<numImages;
  addDOT(imgFName.str(), graphName.str(), obj.toDOT(graphName.str()), *this);
  return imgFName.str();
}

// Add a given amount of indent space to all subsequent text within the current function
void dbgStream::addIndent(string indent)
{
  fileBufs.back()->addIndent(indent);
}

// Remove the most recently added indent
void dbgStream::remIndent()
{
  fileBufs.back()->remIndent();
}

// Given a reference to an object that can be represented as a dot graph, create an image of it and return the string
// that must be added to the output to include this image.
std::string dbgStream::addDOTStr(dottable& obj)
{
  ostringstream imgFName; imgFName << "dbg_imgs/image_" << numImages << ".svg";
  ostringstream graphName; graphName << "graph_"<<numImages;
  ostringstream ret;
  addDOT(imgFName.str(), graphName.str(), obj.toDOT(graphName.str()), ret);

  return ret.str();
}

// Given a representation of a graph in dot format, create an image from it and add it to the output.
// Return the path of the image.
std::string dbgStream::addDOT(string dot)
{
  ostringstream imgFName; imgFName << "dbg_imgs/image_" << numImages << ".svg";
  ostringstream graphName; graphName << "graph_"<<numImages;

  addDOT(imgFName.str(), graphName.str(), dot, *this);

  return imgFName.str();
}

// The common work code for all the addDOT methods
void dbgStream::addDOT(string imgFName, string graphName, string dot, ostream& ret)
{
  ostringstream dotFName; dotFName << imgPath << "/image_" << numImages << ".dot";
  ostringstream mapFName; mapFName << imgPath<<"/image_" << numImages << ".map";

  ofstream dotFile;
  dotFile.open(dotFName.str().c_str());
  dotFile << dot;
  dotFile.close();

  // Create the SVG file's picture of the dot file
  /*pid_t child = fork();
  // Child
  if(child==0) 
  {*/
          ostringstream cmd; cmd << "dot -Tsvg -o"<<imgPath<<"/image_" << numImages << ".svg "<<dotFName.str() << "&"; // -Tcmapx -o"<<mapFName.str()<<"
          cout << "Command \""<<cmd.str()<<"\"\n";
          system(cmd.str().c_str());
  /*      exit(0);
  // ERROR
  } else if(child<0) {
          cout << "dbgStream::addDOTStr() ERROR forking!";
          exit(-1);
  }*/

  //// Identify the names of the nodes in the dot file
  //ostringstream namesFName; namesFName << imgPath << "/image_" << numImages << ".names";
  //{
  //      ostringstream cmd; cmd << "~/Compilers/dot2Nodes.pl "<<dotFName.str()<<" > "<<namesFName.str(); // -Tcmapx -o"<<mapFName.str()<<"
  //      //cout << "Command \""<<cmd.str()<<"\"\n";
  //      system(cmd.str().c_str());
  //}
   //
  //// Read the names of the dot file's node
  //ifstream namesFile(namesFName.str().c_str());
  //string line;
  //set<string> nodes;
  //if(namesFile.is_open()) {
  //      while(namesFile.good()) {
  //              getline(namesFile, line);
  //              //*(this) << line << endl;
  //              if(line!="") nodes.insert(line);
  //      }
  //      namesFile.close();
  //}
  //else {
  //      cout << "dbgStream::addDOT() ERROR opening file \""<<namesFName.str()<<"\" for reading!"<<endl;
  //}

  ret << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"image_"<<numImages<<":<img src=\""<<imgFName<<"\" "; // <a href=\"image_" << numImages << ".dot\">
  //usemap=\""<<graphName.str()<<"\"
  ret << "><br>\n"; // </a>

  //ret << "<a href=\"javascript:showNodes("<<numImages<<", '";
  //for(set<string>::iterator n=nodes.begin(); n!=nodes.end(); ) {
  //      ret << "" << *n << "";
  //      n++;
  //      if(n!=nodes.end())
  //              ret << " ";
  //}
  //ret << "')\">Subset</a><iframe height=0 width=0 id=\"imgFrame_"<<numImages<<"\"></iframe><br>\n";

  //ret << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<embed height=\"100%\" width=\"100%\" pluginspage=\"http://www.adobe.com/svg/viewer/install\" src=\""<<imgFName<<"\">"<<endl;
  //ret << "\t\t\t"<<tabs(fileBufs.back()->funcs.size()+1)<<"<object height=\"100%\" width=\"100%\" data=\""<<imgFName<<"\" type=\"image/svg+xml\"></object>"<<endl;

  // Open the map file and copy its contents into the output stream
  /*ifstream mapFile(mapFName.str().c_str());
  string line;
  //cout << "opening file \""<<mapFName.str()<<"\" for reading!"<<endl;
  if(mapFile.is_open()) {
          while(mapFile.good()) {
                  getline(mapFile, line);
                  ret << line << endl;
          }
          mapFile.close();
  }
  else {
          cout << "dbgStream::addDOT() ERROR opening file \""<<mapFName.str()<<"\" for reading!"<<endl;
  }       */
  numImages++;
}

bool initialized=false;
dbgStream dbg;

/******************
 ***** indent *****
 ******************/
indent::indent(int curDebugLevel, int targetDebugLevel) {
  std::string space = "&nbsp;&nbsp;&nbsp;&nbsp;";
  if(curDebugLevel >= targetDebugLevel) {
    active = true;
    //dbg << "Entering indent space=\""<<space<<"\""<<std::endl;
    dbg.addIndent(space);
  }
  else
    active = false;
}
indent::indent(int curDebugLevel, int targetDebugLevel, std::string space) {
  if(curDebugLevel >= targetDebugLevel) {
    active = true;
    //dbg << "Entering indent space=\""<<space<<"\""<<std::endl;
    dbg.addIndent(space);
  }
  else
    active = false;
}
/*indent::indent() {
  std::string space = "&nbsp;&nbsp;&nbsp;&nbsp;";
  active = true;
  //dbg << "Entering indent space=\""<<space<<"\""<<std::endl;
  dbg.addIndent(space);
}*/
indent::indent(std::string space) {
  active = true;
  //dbg << "Entering indent space=\""<<space<<"\""<<std::endl;
  dbg.addIndent(space);
}
indent::~indent() {
  if(active) {
    //dbg << "Exiting indent"<<std::endl;
    dbg.remIndent();
  }
}

/******************
 ***** region *****
 ******************/
region::region(int curDebugLevel, int targetDebugLevel, regionLevel level, std::string label)
{
  if(curDebugLevel >= targetDebugLevel) {
    active = true;
    this->label = label;
    this->level = level;
    if(level == highLevel) {
      enterFile(label);
    } else if(level == midLevel)
      enterFunc(label);
  }
  else
    active = false;
  
}
region::region(regionLevel level, std::string label)
{
  active = true;
  this->level = level;
  this->label = label;
  if(level == highLevel) {
    exitFile(label);
  } else if(level == midLevel)
    enterFunc(label);
}
region::~region()
{
  if(active) {
    if(level == highLevel)
      exitFile(label);
    else if(level == midLevel)
      exitFunc(label);
  }
}

// Initializes the debug sub-system
void init(string title, string workDir, string fName)
{
  if(initialized) return;
  {
    ostringstream cmd; cmd << "mkdir -p "<<workDir;
    int ret = system(cmd.str().c_str());
    if(ret == -1) { cout << "Dbg::init() ERROR creating directory \""<<workDir<<"\"!"; exit(-1); }
  }
  
  // Directory where analysis-generated images will go
  ostringstream imgPath; imgPath << workDir << "/dbg_imgs";
  {
    ostringstream cmd; cmd << "mkdir -p "<<imgPath.str();
    //cout << "Command \""<<cmd.str()<<"\"\n";
    int ret = system(cmd.str().c_str());
    if(ret == -1) { cout << "Dbg::init() ERROR creating directory \""<<imgPath.str()<<"\"!"; exit(-1); }
  }
  
  // Directory where the default images go
  {
    ostringstream defImgPath; defImgPath << workDir << "/img";
    {
      ostringstream cmd; cmd << "mkdir -p "<<defImgPath.str();
      //cout << "Command \""<<cmd.str()<<"\"\n";
      int ret = system(cmd.str().c_str());
      if(ret == -1) { cout << "Dbg::init() ERROR creating directory \""<<defImgPath.str()<<"\"!"; exit(-1); }
    }
    
    // Copy the default images to this directory
    {
      ostringstream cmd; cmd << "cp -f "<<ROSE_SOURCE_TREE_PATH<<"/src/midend/programAnalysis/genericDataflow/img/* "<<defImgPath.str();
      //cout << "Command \""<<cmd.str()<<"\"\n";
      int ret = system(cmd.str().c_str());
      if(ret == -1) { cout << "Dbg::init() ERROR copying files from directory \"/g/g15/bronevet/Compilers/rose/src/midend/programAnalysis/genericDataflow/img\" directory \""<<defImgPath.str()<<"\"!"; exit(-1); }
    }
  }

  ostringstream dbgFileName; dbgFileName << workDir << "/" << fName;
  dbg.init(title, dbgFileName.str(), workDir, imgPath.str());
  initialized = true;
}

// Indicates that the application has entered or exited a file
void enterFile(std::string funcName/*, std::string indent="    "*/)
{
  if(!initialized) init("Debug Output", ".", "debug");
  dbg.enterFileLevel(funcName);
}

void exitFile(std::string funcName)
{
  if(!initialized) init("Debug Output", ".", "debug");
  dbg.exitFileLevel(funcName);
}


// Indicates that the application has entered or exited a function
void enterFunc(std::string funcName/*, std::string indent="    "*/)
{
  if(!initialized) init("Debug Output", ".", "debug");
  dbg.enterFunc(funcName);
}

void exitFunc(std::string funcName)
{
  if(!initialized) init("Debug Output", ".", "debug");
  dbg.exitFunc(funcName);
}

// Adds an image to the output with the given extension and returns the path of this image
// so that the caller can write to it.
std::string addImage(std::string ext)
{
        if(!initialized) init("Debug Output", ".", "debug");
        return dbg.addImage(ext);
}

// Given a representation of a graph in dot format, create an image from it and add it to the output.
// Return the path of the image.
std::string addDOT(dottable& obj)
{
        if(!initialized) init("Debug Output", ".", "debug");
        return dbg.addDOT(obj);
}

// Given a representation of a graph in dot format, create an image of it and return the string
// that must be added to the output to include this image.
std::string addDOTStr(dottable& obj)
{
        if(!initialized) init("Debug Output", ".", "debug");
        return dbg.addDOTStr(obj);
}

// Given a representation of a graph in dot format, create an image from it and add it to the output.
// Return the path of the image.
std::string addDOT(std::string dot) {
        if(!initialized) init("Debug Output", ".", "debug");
        return dbg.addDOT(dot);
}

// Given a string, returns a version of the string with all the control characters that may appear in the 
// string escaped to that the string can be written out to Dbg::dbg with no formatting issues.
// This function can be called on text that has already been escaped with no harm.
std::string escape(std::string s)
{
        string out;
        for(unsigned int i=0; i<s.length(); i++) {
                // Manage HTML tags
                     if(s[i] == '<') out += "&lt;";
                else if(s[i] == '>') out += "&gt;";
                // Manage hashes, since they confuse the C PreProcessor CPP
                else if(s[i] == '#') out += "&#35;";
                else                 out += s[i];
        }
        return out;
}


void dotGraphGenerator (dataflow::ComposedAnalysis *a) 
{
  analysisStatesToDOT eas(a);
  IntraAnalysisResultsToDotFiles upia_eas(eas);
  upia_eas.runAnalysis();
}

} // namespace Dbg
