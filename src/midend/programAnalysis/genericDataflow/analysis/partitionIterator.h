#ifndef PARTITION_ITERATOR
#define PARTITION_ITERATOR

#include "virtualCFG.h"
#include "DataflowCFG.h"
#include "partitions.h"
//#include "baseCFGIterator.h"

#include <list>
#include <set>
#include <string>

namespace dataflow{

// Iterates over Parts in a Partition, respecting dependences in the graph.
// Supports both forward and backward iteration.
class partIterator
{
  //protected:
  public:
          
  std::list<PartPtr> remainingNodes;
  std::set<PartPtr> visited;
  bool initialized;

  public:
  partIterator();
  
  partIterator(const PartPtr start);
  virtual ~partIterator() { }
  
  void init(const PartPtr start);

  protected:
  // returns true if the given PartPtr is in the remainingNodes list and false otherwise
  bool isRemaining(const PartPtr p);
          
  // advances this iterator in the given direction. Forwards if fwDir=true and backwards if fwDir=false.
  // if pushAllChildren=true, all of the current node's unvisited children (predecessors or successors, 
  //    depending on fwDir) are pushed onto remainingNodes
  void advance(bool fwDir, bool pushAllChildren);
  
  public:
  virtual void operator ++ (int);
  
  bool eq(const partIterator& other_it) const;
  
  bool operator==(const partIterator& other_it) const;
  
  bool operator!=(const partIterator& it) const;
          
  PartPtr operator * ();
  
  // Returns a fresh iterator that starts at node n
  static partIterator begin(const PartPtr n);
  
  // Returns an empty iterator that can be compared to any other iterator to 
  // check if it has completed passing over its iteration space
  static partIterator end();
  
  // Contains the state of an iterator, allowing iterators to be 
  // checkpointed and restarted.
  class checkpoint/* : public virtual BaseCFG::iterator::checkpoint*/
  {
    std::list<PartPtr> remainingNodes;
    std::set<PartPtr>  visited;
  
    public:
    checkpoint(const std::list<PartPtr>& remainingNodes, const std::set<PartPtr>& visited);
          
    checkpoint(const checkpoint& that);
    
    std::string str(std::string indent="");
    
    friend class partIterator;
  };
  
  // Returns a checkpoint of this iterator's progress.
  checkpoint getChkpt();
  
  // Loads this iterator's state from the given checkpoint.
  void restartFromChkpt(checkpoint& chkpt);
  
  std::string str(std::string indent="");
};

class back_partIterator : public virtual partIterator
{
  public:
  back_partIterator(): partIterator() {}
  
  back_partIterator(const PartPtr end): partIterator(end) { }
  
  void operator ++ (int);
};

class dataflowPartIterator : public virtual partIterator
{
  protected:
  PartPtr terminator;
  public:
  //dataflow(): iterator() {}
  
  dataflowPartIterator(const PartPtr& terminator_arg);
  dataflowPartIterator(const PartPtr& start, const PartPtr& terminator_arg);
  
  void init(const PartPtr& start_arg, const PartPtr& terminator_arg);
  
  // Initializes this iterator's terminator node
  /*void setTerminator(const PartPtr terminator) {
          initialized = true;
          this->terminator = terminator;
  }*/
  
  void add(const PartPtr next);
  
  void operator ++ (int);
  
  // Contains the state of an dataflow iterator, allowing dataflow 
  // iterators to be checkpointed and restarted.
  class checkpoint/* : public virtual BaseCFG::checkpoint*/
  {
    protected:
    partIterator::checkpoint iChkpt;
    PartPtr terminator;
    
    public:
    checkpoint(const partIterator::checkpoint& iChkpt, const PartPtr& terminator);
    
    checkpoint(const checkpoint &that);
            
    std::string str(std::string indent="");
    
    friend class dataflowPartIterator;
  };
  
  // Returns a checkpoint of this dataflow iterator's progress.
  checkpoint getChkpt();
  
  // Loads this dataflow iterator's state from the given checkpoint.
  void restartFromChkpt(checkpoint& chkpt);
  
  std::string str(std::string indent="");
};

class back_dataflowPartIterator: public virtual dataflowPartIterator
{
  public: 
  back_dataflowPartIterator(const PartPtr terminator_arg) : dataflowPartIterator(terminator_arg) {}
  back_dataflowPartIterator(const PartPtr end, const PartPtr terminator_arg);
          
  void operator ++ (int);
  
  std::string str(std::string indent="");
};
}; // namespace dataflow
#endif
