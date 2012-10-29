#ifndef VIRTUAL_CFG_ITERATOR
#define VIRTUAL_CFG_ITERATOR

#include "virtualCFG.h"
#include "DataflowCFG.h"
#include "partitions.h"
//#include "baseCFGIterator.h"

#include <list>
#include <set>
#include <string>

namespace VirtualCFG{

// Iterates over Parts in a VirtualCFG, respecting dependences in the graph.
// Supports both forward and backward iteration.
class iterator/* : public virtual BaseCFG::iterator*/
{
  //protected:
  public:
          
  std::list<CFGNode> remainingNodes;
  std::set<CFGNode> visited;
  bool initialized;

  public:
  iterator();
  
  iterator(const CFGNode start);
  virtual ~iterator() { }
  
  void init(const CFGNode start);

  protected:
  // returns true if the given CFGNode is in the remainingNodes list and false otherwise
  bool isRemaining(const CFGNode p);
          
  // advances this iterator in the given direction. Forwards if fwDir=true and backwards if fwDir=false.
  // if pushAllChildren=true, all of the current node's unvisited children (predecessors or successors, 
  //    depending on fwDir) are pushed onto remainingNodes
  void advance(bool fwDir, bool pushAllChildren);
  
  public:
  virtual void operator ++ (int);
  
  bool eq(const iterator& other_it) const;
  
  bool operator==(const iterator& other_it) const;
  
  bool operator!=(const iterator& it) const;
          
  CFGNode operator * ();
  
  // Returns a fresh iterator that starts at node n
  static iterator begin(const CFGNode n);
  
  // Returns an empty iterator that can be compared to any other iterator to 
  // check if it has completed passing over its iteration space
  static iterator end();
  
  // Contains the state of an iterator, allowing iterators to be 
  // checkpointed and restarted.
  class checkpoint/* : public virtual BaseCFG::iterator::checkpoint*/
  {
    std::list<CFGNode> remainingNodes;
    std::set<CFGNode>  visited;
  
    public:
    checkpoint(const std::list<CFGNode>& remainingNodes, const std::set<CFGNode>& visited);
          
    checkpoint(const checkpoint& that);
    
    std::string str(std::string indent="");
    
    friend class iterator;
  };
  
  // Returns a checkpoint of this iterator's progress.
  checkpoint getChkpt();
  
  // Loads this iterator's state from the given checkpoint.
  void restartFromChkpt(checkpoint& chkpt);
  
  std::string str(std::string indent="");
};

class back_iterator : /*public virtual BaseCFG::backiterator, */public virtual iterator
{
  public:
  back_iterator(): iterator() {}
  
  back_iterator(const CFGNode end): iterator(end) { }
  
  void operator ++ (int);
};

class dataflowIterator : /*public virtual BaseCFG::dataflow, */public virtual iterator
{
  protected:
  CFGNode terminator;
  public:
  //dataflow(): iterator() {}
  
  dataflowIterator(const CFGNode terminator_arg);
  
  //dataflow(const CFGNode &start) : iterator(start) {}
  dataflowIterator(const CFGNode start, const CFGNode terminator_arg);
  
  void init(const CFGNode start_arg, const CFGNode terminator_arg);
  
  // Initializes this iterator's terminator node
  /*void setTerminator(const CFGNode terminator) {
          initialized = true;
          this->terminator = terminator;
  }*/
  
  void add(const CFGNode next);
  
  void operator ++ (int);
  
  // Contains the state of an dataflow iterator, allowing dataflow 
  // iterators to be checkpointed and restarted.
  class checkpoint/* : public virtual BaseCFG::dataflow::checkpoint*/
  {
          protected:
          iterator::checkpoint iChkpt;
          CFGNode terminator;
          
          public:
          checkpoint(const iterator::checkpoint& iChkpt, const CFGNode terminator);
          
          checkpoint(const checkpoint &that);
                  
          std::string str(std::string indent="");
          
          friend class dataflowIterator;
  };
  
  // Returns a checkpoint of this dataflow iterator's progress.
  checkpoint getChkpt();
  
  // Loads this dataflow iterator's state from the given checkpoint.
  void restartFromChkpt(checkpoint& chkpt);
  
  std::string str(std::string indent="");
};

class back_dataflowIterator: /*public virtual BaseCFG::back_dataflow,*/ public virtual dataflowIterator
{
  public: 
  //back_dataflow(): back_iterator() {}
  
  back_dataflowIterator(const CFGNode terminator_arg) : dataflowIterator(terminator_arg) {}
  
  //back_dataflow(const CFGNode &end) : iterator(end) {}
  
  back_dataflowIterator(const CFGNode end, const CFGNode terminator_arg);
          
  void operator ++ (int);
  
  std::string str(std::string indent="");
};
}
#endif
