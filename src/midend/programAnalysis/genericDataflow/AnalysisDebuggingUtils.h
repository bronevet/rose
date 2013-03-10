#ifndef ROSE_ANALYSIS_DEBUGGING_UTILS_H
#define ROSE_ANALYSIS_DEBUGGING_UTILS_H

#include <list>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

class printable
{
        public:
        virtual ~printable() {}
        virtual std::string str(std::string indent="")=0;
};

class dottable
{
        public:
        virtual ~dottable() {}
        // Returns a string that containts the representation of the object as a graph in the DOT language
        // that has the given name
        virtual std::string toDOT(std::string graphName)=0;
};

namespace dataflow {
class Analysis;
class ComposedAnalysis;
};

namespace Dbg {
// Generate dot graphs for an analysis: it handles intra-procedural analysis for now. 
// For each function, a dot graph file will be generated. The CFG node will contain lattices information.
// The dot file will have a name like: original_full_filename_managed_func_name_cfg.dot
void dotGraphGenerator(dataflow::ComposedAnalysis *a);

class dbgStream;

// Adopted from http://wordaligned.org/articles/cpp-streambufs
class dbgBuf: public std::streambuf
{
  friend class dbgStream;
  // True immediately after a new line
  bool synched;
  // True if the owner dbgStream is writing text and false if the user is
  bool ownerAccess;
  std::streambuf* baseBuf;
  std::list<std::string> funcs;
  //std::list<std::string> indents;

  // The number of observed '<' characters that have not yet been balanced out by '>' characters.
  //      numOpenAngles = 1 means that we're inside an HTML tag
  //      numOpenAngles > 1 implies an error or text inside a comment
  int numOpenAngles;

  // The number of divs that have been inserted into the output
  std::list<int> parentDivs;
  // For each div a list of the strings that need to be concatenated to form the indent for each line
  std::list<std::list<std::string> > indents;

  /*// Flag that indicates synch was just called to start a new line (if so then the next time 
  // we get a new level of indent, it will add to print out the extra indent characters on top
  // of what was already printed in synch).
  bool justSynched;*/

  // Flag that indicates that a new line has begun and thus before the next printed character
  // we need to print the indent
  bool needIndent;
public:
        
  virtual ~dbgBuf() {};
  // Construct a streambuf which tees output to both input
  // streambufs.
  dbgBuf();
  dbgBuf(std::streambuf* baseBuf);
  void init(std::streambuf* baseBuf);
        
private:
  // This dbgBuf has no buffer. So every character "overflows"
  // and can be put directly into the teed buffers.
  virtual int overflow(int c);

  // Prints the indent to the stream buffer, returns 0 on success non-0 on failure
  //int printIndent();

  // Prints the given string to the stream buffer
  int printString(std::string s);

  //virtual int sputc(char c);

  // Get the current indentation depth within the current div
  int getIndentDepth();
  // Get the current indentation within the current div
  std::string getIndent();
  // Add more indentation within the current div
  void addIndent(std::string indent);
  // Remove the most recently added indent within the current div
  void remIndent();

  virtual std::streamsize xsputn(const char * s, std::streamsize n);

  // Sync buffer.
  virtual int sync();
        
  // Switch between the owner class and user code writing text
protected:
  void userAccessing();
  void ownerAccessing();

  // Indicates that the application has entered or exited a function
  void enterFunc(std::string funcName);
  void exitFunc(std::string funcName);
  // Returns the depth of enterFunc calls that have not yet been matched by exitFuncCalls
  int funcDepth();
};

// Stream that uses dbgBuf
class dbgStream : public std::ostream
{
  std::list<std::ofstream*> indexFiles;
  std::list<std::ofstream*> dbgFiles;
  std::list<std::string>    detailFileRelFNames; // Relative names of all the dbg files on the stack
  std::list<std::ofstream*> summaryFiles;
  std::list<std::string>   flTitles;
  std::list<int>           fileLevel;
  std::list<dbgBuf*>       fileBufs;
  int colorIdx; // The current index into the list of colors 
  dbgBuf defaultFileBuf;
  
  std::vector<std::string> colors;
  // The title of the file
  std::string title;
  // The root working directory
  std::string workDir;
  // The directory where all images will be stored
  std::string imgPath;
  // The name of the output debug file
  std::string dbgFileName;
  // The total number of images in the output file
  int numImages;
  
  
  bool initialized;
  
  

public:
  // Construct an ostream which tees output to the supplied
  // ostreams.
  dbgStream();
  dbgStream(std::string title, std::string dbgFileName, std::string workDir, std::string imgPath);
  void init(std::string title, std::string dbgFileName, std::string workDir, std::string imgPath);
  ~dbgStream();

  // Returns the string representation of the current file level
  std::string fileLevelStr();

  // Enter a new file level
  void enterFileLevel(std::string flTitle, bool topLevel=false);
  
  // Exit a current file level
  void exitFileLevel(std::string flTitle, bool topLevel=false);

  void printSummaryFileContainerHTML(std::string absoluteFileName, std::string relativeFileName, std::string title);
  void printDetailFileContainerHTML(std::string absoluteFileName, std::string relativeFileName, std::string title);
  /*void printDetailFileHeader(std::string title);
  void printDetailFileTrailer();*/
  
  // Indicates that the application has entered or exited a function
  void enterFunc(std::string funcName, std::string detailContentURL="", std::string summaryContentURL="");
  void exitFunc(std::string funcName);

  // Adds an image to the output with the given extension and returns the path of this image
  // so that the caller can write to it.
  std::string addImage(std::string ext=".gif");

  // Given a reference to an object that can be represented as a dot graph, create an image from it and add it to the output.
  // Return the path of the image.
  std::string addDOT(dottable& obj);
  // Given a reference to an object that can be represented as a dot graph, create an image of it and return the string
  // that must be added to the output to include this image.
  std::string addDOTStr(dottable& obj);
  // Given a representation of a graph in dot format, create an image from it and add it to the output.
  // Return the path of the image.
  std::string addDOT(std::string dot);
  // The common work code for all the addDOT methods
  void addDOT(std::string imgFName, std::string graphName, std::string dot, std::ostream& ret);

  // Add a given amount of indent space to all subsequent text within the current function
  void addIndent(std::string indent);
  // Remove the most recently added indent
  void remIndent();
};

extern bool initialized;
extern dbgStream dbg;

class indent {
public:
  bool active;
  indent(int curDebugLevel, int targetDebugLevel);
  indent(int curDebugLevel, int targetDebugLevel, std::string space);
  indent(std::string space="&nbsp;&nbsp;&nbsp;&nbsp;");
  ~indent();
};

class region {
public:
  bool active;
  std::string label;
  typedef enum {highLevel, midLevel, lowLevel} regionLevel;
  regionLevel level;
  
  region(int curDebugLevel, int targetDebugLevel, regionLevel level, std::string label);
  region(regionLevel level, std::string label);
  ~region();
};

        
// Initializes the debug sub-system
void init(std::string title, std::string workDir, std::string fName="debug");

// Indicates that the application has entered or exited a file
void enterFile(std::string funcName);
void exitFile(std::string funcName);
        
// Indicates that the application has entered or exited a function
void enterFunc(std::string funcName);
void exitFunc(std::string funcName);

// Adds an image to the output with the given extension and returns the path of this image
// so that the caller can write to it.
std::string addImage(std::string ext=".gif");

// Given a reference to an object that can be represented as a dot graph,  create an image from it and add it to the output.
// Return the path of the image.
std::string addDOT(dottable& obj);

// Given a reference to an object that can be represented as a dot graph, create an image of it and return the string
// that must be added to the output to include this image.
std::string addDOTStr(dottable& obj);

// Given a representation of a graph in dot format, create an image from it and add it to the output.
// Return the path of the image.
std::string addDOT(std::string dot);

// Given a string, returns a version of the string with all the control characters that may appear in the 
// string escaped to that the string can be written out to Dbg::dbg with no formatting issues.
// This function can be called on text that has already been escaped with no harm.
std::string escape(std::string s);
}

#endif
