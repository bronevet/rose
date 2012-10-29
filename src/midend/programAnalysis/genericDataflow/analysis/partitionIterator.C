//#include "variables.h"
#include "virtualCFG.h"
#include "partitionIterator.h"
#include <boost/make_shared.hpp>

#include <list>
using std::list;
#include <vector>
using std::vector;
#include <set>
using std::set;
#include <string>
using std::string;
#include <iostream>
using std::cout;
using std::ostringstream;

using namespace dataflow;

namespace dataflow{
  
/******************************
 ********** ITERATOR **********
 ******************************/
  
partIterator::partIterator() {
  initialized     = false;
}

partIterator::partIterator(const PartPtr start) 
{
  initialized     = true;
  remainingNodes.push_front(start);
  visited.insert(start);
}

void partIterator::init(const PartPtr start) {
  initialized     = true;
  remainingNodes.push_front(start);
  visited.insert(start);
}


// returns true if the given PartPtr  is in the remainingNodes list and false otherwise
bool partIterator::isRemaining(const PartPtr n)
{
  ROSE_ASSERT(initialized);
  for(list<PartPtr>::const_iterator it=remainingNodes.begin(); it!=remainingNodes.end(); it++)
  {
    // if in is currently n remainingNodes, say so
    if(*it == n) return true;
  }
  // n is not in remainingNodes
  return false;
}

// advances this iterator in the given direction. Forwards if fwDir=true and backwards if fwDir=false.
// if pushAllChildren=true, all of the current node's unvisited children (predecessors or successors, 
//    depending on fwDir) are pushed onto remainingNodes
// It is assumed that for a given iterator pushAllChildren either always = true or always = false.
void partIterator::advance(bool fwDir, bool pushAllChildren)
{
  ROSE_ASSERT(initialized);
  //Dbg::dbg << "partIterator::advance(fwDir="<<fwDir<<") #remainingNodes="<<remainingNodes.size()<<endl;
  /*cout<<"  visited=\n";
  for(set<PartPtr>::iterator it=visited.begin(); it!=visited.end(); it++)
    cout << "      ["<<it.getNode()->class_name()<<" | "<<it.getNode()<<" | "<<it.getNode()->unparseToString()<<"]\n";*/
  if(remainingNodes.size()>0)
  {
    // pop the next CFG node from the front of the list
    PartPtr cur = remainingNodes.front();
    remainingNodes.pop_front();
    //Dbg::dbg << "#remainingNodes="<<remainingNodes.size()<<" cur=["<<cur.getNode()->unparseToString()<<" | "<<cur.getNode()->class_name()<<"]"<<endl;
    
    if(pushAllChildren)
    {
      // find its followers (either successors or predecessors, depending on value of fwDir), push back 
      // those that have not yet been visited
      vector<PartEdgePtr> nextE;
      if(fwDir) nextE = cur->outEdges();
      else      nextE = cur->inEdges();
      for(vector<PartEdgePtr>::iterator it=nextE.begin(); it!=nextE.end(); it++)
      {
        PartPtr nextN = (fwDir ? (*it)->target() : nextN = (*it)->source());
        //Dbg::dbg << "nextN=["<<nextN.getNode()->unparseToString()<<" | "<<nextN.getNode()->class_name()<<"]"<<endl;
        
        /*cout << "      iterator::advance "<<(fwDir?"descendant":"predecessor")<<": "<<
               "<"<<nextN.getNode()->class_name()<<" | "<<nextN.getNode()<<" | "<<nextN.getNode()->unparseToString()<<">, "<<
               "visited="<<(visited.find(nextN) != visited.end())<<
               " remaining="<<isRemaining(nextN)<<"\n";*/
        
        // if we haven't yet visited this node and don't yet have it on the remainingNodes list
        if(visited.find(PartPtr(nextN)) == visited.end() &&
           !isRemaining(PartPtr(nextN)))
        {
          //printf("   pushing back node <%s: 0x%x: %s> visited=%d\n", nextN.getNode()->class_name().c_str(), nextN.getNode(), nextN.getNode()->unparseToString().c_str(), visited.find(nextN)!=visited.end());
          remainingNodes.push_back(nextN);
        }
      }
    
      // if we still have any nodes left remaining
      if(remainingNodes.size()>0)
      {
        // take the next node from the front of the list and mark it as visited
        visited.insert(remainingNodes.front());
      
        //Dbg::dbg << "remainingNodes.front()=["<<remainingNodes.front().getNode()->unparseToString()<<" | "<<remainingNodes.front().getNode()->class_name()<<"]"<<endl;
      }
      // Since pushAllChildren always = true or = false, we only need to worry about managing visited in the true case
    }
  }
}

void partIterator::operator ++ (int)
{
  ROSE_ASSERT(initialized);
  advance(true, true);
}

bool partIterator::eq(const partIterator& other_it) const
{
  if(initialized != other_it.initialized) return false;
    
  // If both iterators are not initialized, they're equal
  if(!initialized) return true;
    
  //printf("iterator::eq() remainingNodes.size()=%d  other_it.remainingNodes.size()=%d\n", remainingNodes.size(), other_it.remainingNodes.size());
  if(remainingNodes.size() != other_it.remainingNodes.size()) return false;
  
  list<PartPtr>::const_iterator it1, it2;
  // look to ensure that every CFG node in other_it.remainingNodes appears in remainingNodes
  
  for(it1=remainingNodes.begin(); it1!=remainingNodes.end(); it1++)
  {
    for(it2=other_it.remainingNodes.begin(); it2!=other_it.remainingNodes.end(); it2++)
    {
      // if we found *it1 inside other_it.remainingNodes
      if(*it1 == *it2)
      {
        //printf("        (*it1 == *it2)\n");
        break;
      }
    }
    
    // the two iterators are not equal if ...
    
    // the current node in remainingNodes was not found in other_it.remainingNodes
    if(it2!=other_it.remainingNodes.end()) 
    {
      //printf("        it2!=other_it.remainingNodes.end()\n");
      return false; 
    }
      
    // or the two nodes do not have the same visited status in both iterators
    if((visited.find(*it1) == visited.end()) !=
       (other_it.visited.find(*it1) == other_it.visited.end()))
    {
      //printf("        (visited.find(*it1) == visited.end()) != (other_it.visited.find(*it1) == other_it.visited.end())\n");
      return false;
    }
  }
  //printf("partIterator::eq: returning true\n");
  
  return true;
}

bool partIterator::operator==(const partIterator& other_it) const
{
  return eq(other_it);
}

bool partIterator::operator!=(const partIterator& it) const
{
  return !(*this == it);
}
  
PartPtr partIterator::operator * ()
{
  ROSE_ASSERT(initialized);
  return remainingNodes.front();
}

partIterator partIterator::begin(const PartPtr n)
{
  partIterator newIter(n);
  return newIter;
}

partIterator partIterator::end()
{
  partIterator blank;
  // Make sure that this iterator is initialized even though it is empty
  blank.initialized = true;
  return blank;
}

partIterator::checkpoint::checkpoint(const list<PartPtr>& remainingNodes, const set<PartPtr>& visited)
{
  this->remainingNodes = remainingNodes;
  this->visited  = visited;
}

partIterator::checkpoint::checkpoint(const partIterator::checkpoint& that)
{
  this->remainingNodes = that.remainingNodes;
  this->visited  = that.visited;
}

string partIterator::checkpoint::str(string indent)
{
  ostringstream outs;
  outs << indent << "[partIterator::checkpoint : "<<endl;
  for(list<PartPtr>::iterator it=remainingNodes.begin();
      it!=remainingNodes.end(); )
  {
    outs << indent << "&nbsp;&nbsp;&nbsp;&nbsp;"<<it->str();
    it++;
    if(it!=remainingNodes.end()) outs << endl;
  }
  outs << "]";
  return outs.str();
}

// Returns a checkpoint of this iterator's progress.
partIterator::checkpoint partIterator::getChkpt()
{
  ROSE_ASSERT(initialized);
  partIterator::checkpoint chkpt(remainingNodes, visited);
  return chkpt;
}

// Loads this iterator's state from the given checkpoint.
void partIterator::restartFromChkpt(partIterator::checkpoint& chkpt)
{  
  remainingNodes.clear();
  visited.clear();
  
  remainingNodes = chkpt.remainingNodes;
  visited = chkpt.visited;
  // The iterator must become initialized because it is only possible to take a checkpoints of initialized iterators
  initialized = true;
}

string partIterator::str(string indent)
{
  
  ostringstream outs;
  
  if(initialized) {
    outs << "[partIterator:"<<endl;
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;remainingNodes(#"<<remainingNodes.size()<<") = "<<endl;
    for(list<PartPtr>::iterator it=remainingNodes.begin(); it!=remainingNodes.end(); it++)
    { outs << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<it->str()<<endl; }
    
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;visited(#"<<visited.size()<<")  = \n";
    for(set<PartPtr>::iterator it=visited.begin(); it!=visited.end(); it++) { 
      PartPtr c = *it;
      outs << "&nbsp;&nbsp;&nbsp;&nbsp;"<<c.str()<<endl;
    }
    
    outs << "]";
  } else {
    outs << "[partIterator: Uninitialized]";
  }
    
  return outs.str();
}


/*****************************
 ******* BACK_ITERATOR *******
 *****************************/
  
void back_partIterator::operator ++ (int)
{
  ROSE_ASSERT(initialized);
  advance(false, true);
}

/******************************
 ********** DATAFLOW **********
 ******************************/

dataflowPartIterator::dataflowPartIterator(const PartPtr& terminator_arg) : terminator(terminator_arg)
{
  // Record that the terminator has been visited to ensure that it is never analyzed
  visited.insert(terminator);
}

dataflowPartIterator::dataflowPartIterator(const PartPtr& start, const PartPtr& terminator_arg): 
    partIterator(start), terminator(terminator_arg)
{
  // Record that the terminator has been visited to ensure that it is never analyzed
  visited.insert(terminator);
  
  ROSE_ASSERT(start!=terminator);
}

void dataflowPartIterator::init(const PartPtr& start_arg, const PartPtr& terminator_arg)
{
  ROSE_ASSERT(start_arg!=terminator_arg);
  // Use the init method to initialize the starting point to make sure that the object is recorded as being initialized
  partIterator::init(start_arg);
  terminator = terminator_arg;
}
 
void dataflowPartIterator::add(const PartPtr next)
{
  // If this dataflow iterator is not initialized, initialize it now since it will now have real state
  if(!initialized) initialized = true;
  
  // never add the terminator node
  if(next==terminator)
    return;
  
  // if next is not currently in remainingNodes, add it
  if(!isRemaining(next))
  {
    set<PartPtr>::iterator nextLoc = visited.find(next);
    if(nextLoc != visited.end())
      visited.erase(visited.find(next));
    remainingNodes.push_back(next);
  }
}

void dataflowPartIterator::operator ++ (int)
{
  ROSE_ASSERT(initialized);
  advance(true, false);
}

dataflowPartIterator::checkpoint::checkpoint(const partIterator::checkpoint& iChkpt, const PartPtr& terminator): 
  iChkpt(iChkpt), terminator(terminator) {}

dataflowPartIterator::checkpoint::checkpoint(const dataflowPartIterator::checkpoint &that): 
  iChkpt(that.iChkpt), terminator(that.terminator) {}

string dataflowPartIterator::checkpoint::str(string indent)
{
  ostringstream outs;
  outs << indent << "[dataflowPartIterator::checkpoint : \n"; 
  outs << indent << "&nbsp;&nbsp;&nbsp;&nbsp;iterator = \n"<<iChkpt.str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;")<<"\n";
  outs << indent << "&nbsp;&nbsp;&nbsp;&nbsp;terminator = "<<terminator.str()<<endl;
  return outs.str();
}

// Returns a checkpoint of this iterator's progress.
dataflowPartIterator::checkpoint dataflowPartIterator::getChkpt()
{
  ROSE_ASSERT(initialized);
  dataflowPartIterator::checkpoint chkpt(partIterator::getChkpt(), terminator);
  return chkpt;
}

// Loads this iterator's state from the given checkpoint.
void dataflowPartIterator::restartFromChkpt(dataflowPartIterator::checkpoint& chkpt)
{
  partIterator::restartFromChkpt(chkpt.iChkpt);
  terminator = chkpt.terminator;
}

string dataflowPartIterator::str(string indent)
{
  ostringstream outs;
  
  if(initialized) {
    outs << "[dataflowPartIterator:\n";
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;iterator = "<<partIterator::str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;")<<"\n";
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;terminator = "<< terminator.str()<<endl;
  } else {
    outs << "[dataflowPartIterator: Uninitialized]";
  }
    
  return outs.str();
}


/*****************************
******* BACK_DATAFLOW *******
*****************************/
back_dataflowPartIterator::back_dataflowPartIterator(const PartPtr end, const PartPtr terminator_arg): 
    partIterator(end), dataflowPartIterator(end, terminator_arg) {
}

void back_dataflowPartIterator::operator ++ (int)
{
  advance(false, false);
}

string back_dataflowPartIterator::str(string indent)
{
  ostringstream outs;
  
  if(initialized) {
    outs << "[back_dataflowPartIterator:\n";
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;iterator = "<<partIterator::str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;")<<"\n";
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;terminator = "<< terminator.str()<<endl;
  } else {
    outs << "[back_dataflowPartIterator: Uninitialized]";
  }
    
  return outs.str();
}

}; // namespace dataflow
