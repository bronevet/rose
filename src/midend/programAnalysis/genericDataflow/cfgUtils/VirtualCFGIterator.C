//#include "variables.h"
#include "virtualCFG.h"
#include "VirtualCFGIterator.h"
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

namespace VirtualCFG{
  
/******************************
 ********** ITERATOR **********
 ******************************/
  
iterator::iterator() {
  initialized     = false;
}

iterator::iterator(const CFGNode start) 
{
  initialized     = true;
  remainingNodes.push_front(start);
  visited.insert(start);
}

void iterator::init(const CFGNode start) {
  initialized     = true;
  remainingNodes.push_front(start);
  visited.insert(start);
}


// returns true if the given CFGNode  is in the remainingNodes list and false otherwise
bool iterator::isRemaining(const CFGNode n)
{
  ROSE_ASSERT(initialized);
  for(list<CFGNode>::const_iterator it=remainingNodes.begin(); it!=remainingNodes.end(); it++)
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
void iterator::advance(bool fwDir, bool pushAllChildren)
{
  ROSE_ASSERT(initialized);
  //Dbg::dbg << "iterator::advance(fwDir="<<fwDir<<") #remainingNodes="<<remainingNodes.size()<<endl;
  /*cout<<"  visited=\n";
  for(set<CFGNode>::iterator it=visited.begin(); it!=visited.end(); it++)
    cout << "      ["<<it.getNode()->class_name()<<" | "<<it.getNode()<<" | "<<it.getNode()->unparseToString()<<"]\n";*/
  if(remainingNodes.size()>0)
  {
    // pop the next CFG node from the front of the list
    CFGNode cur = remainingNodes.front();
    remainingNodes.pop_front();
    //Dbg::dbg << "#remainingNodes="<<remainingNodes.size()<<" cur=["<<cur.getNode()->unparseToString()<<" | "<<cur.getNode()->class_name()<<"]"<<endl;
    
    if(pushAllChildren)
    {
      // find its followers (either successors or predecessors, depending on value of fwDir), push back 
      // those that have not yet been visited
      vector<CFGEdge> nextE;
      if(fwDir) nextE = cur.outEdges();
      else      nextE = cur.inEdges();
      for(vector<CFGEdge>::iterator it=nextE.begin(); it!=nextE.end(); it++)
      {
        CFGNode nextN = (fwDir ? it->target() : nextN = it->source());
        //Dbg::dbg << "nextN=["<<nextN.getNode()->unparseToString()<<" | "<<nextN.getNode()->class_name()<<"]"<<endl;
        
        /*cout << "      iterator::advance "<<(fwDir?"descendant":"predecessor")<<": "<<
               "<"<<nextN.getNode()->class_name()<<" | "<<nextN.getNode()<<" | "<<nextN.getNode()->unparseToString()<<">, "<<
               "visited="<<(visited.find(nextN) != visited.end())<<
               " remaining="<<isRemaining(nextN)<<"\n";*/
        
        // if we haven't yet visited this node and don't yet have it on the remainingNodes list
        if(visited.find(nextN) == visited.end() &&
           !isRemaining(nextN))
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

void iterator::operator ++ (int)
{
  ROSE_ASSERT(initialized);
  advance(true, true);
}

bool iterator::eq(const iterator& other_it) const
{
  if(initialized != other_it.initialized) return false;
    
  // If both iterators are not initialized, they're equal
  if(!initialized) return true;
    
  //printf("iterator::eq() remainingNodes.size()=%d  other_it.remainingNodes.size()=%d\n", remainingNodes.size(), other_it.remainingNodes.size());
  if(remainingNodes.size() != other_it.remainingNodes.size()) return false;
  
  list<CFGNode>::const_iterator it1, it2;
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
  //printf("iterator::eq: returning true\n");
  
  return true;
}

bool iterator::operator==(const iterator& other_it) const
{
  return eq(other_it);
}

bool iterator::operator!=(const iterator& it) const
{
  return !(*this == it);
}
  
CFGNode iterator::operator * ()
{
  ROSE_ASSERT(initialized);
  return remainingNodes.front();
}

iterator iterator::begin(const CFGNode n)
{
  iterator newIter(n);
  return newIter;
}

iterator iterator::end()
{
  iterator blank;
  // Make sure that this iterator is initialized even though it is empty
  blank.initialized = true;
  return blank;
}

iterator::checkpoint::checkpoint(const list<CFGNode>& remainingNodes, const set<CFGNode>& visited)
{
  this->remainingNodes = remainingNodes;
  this->visited  = visited;
}

iterator::checkpoint::checkpoint(const iterator::checkpoint& that)
{
  this->remainingNodes = that.remainingNodes;
  this->visited  = that.visited;
}

string iterator::checkpoint::str(string indent)
{
  ostringstream outs;
  outs << indent << "[VirtualCFG::iterator::checkpoint : "<<endl;
  for(list<CFGNode>::iterator it=remainingNodes.begin();
      it!=remainingNodes.end(); )
  {
    outs << indent << "&nbsp;&nbsp;&nbsp;&nbsp;["<<it->getNode()->unparseToString()<<" | "<<it->getNode()->class_name()<<"]";
    it++;
    if(it!=remainingNodes.end()) outs << endl;
  }
  outs << "]";
  return outs.str();
}

// Returns a checkpoint of this iterator's progress.
iterator::checkpoint iterator::getChkpt()
{
  ROSE_ASSERT(initialized);
  iterator::checkpoint chkpt(remainingNodes, visited);
  return chkpt;
  //return new iterator::checkpoint::checkpoint(remainingNodes, visited);
}

// Loads this iterator's state from the given checkpoint.
void iterator::restartFromChkpt(iterator::checkpoint& chkpt)
{  
  remainingNodes.clear();
  visited.clear();
  
  remainingNodes = chkpt.remainingNodes;
  visited = chkpt.visited;
  // The iterator must become initialized because it is only possible to take a checkpoints of initialized iterators
  initialized = true;
}

string iterator::str(string indent)
{
  
  ostringstream outs;
  
  if(initialized) {
    outs << "[iterator:"<<endl;
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;remainingNodes(#"<<remainingNodes.size()<<") = "<<endl;
    for(list<CFGNode>::iterator it=remainingNodes.begin(); it!=remainingNodes.end(); it++)
    { outs << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;["<<it->getNode()->unparseToString()<<" | "<<it->getNode()->class_name()<<"]"<<endl; }
    
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;visited(#"<<visited.size()<<")  = \n";
    for(set<CFGNode>::iterator it=visited.begin(); it!=visited.end(); it++)
    { outs << "&nbsp;&nbsp;&nbsp;&nbsp;["<<it->getNode()->unparseToString()<<" | "<<it->getNode()->class_name()<<"]"<<endl; }
    
    outs << "]";
  } else {
    outs << "[iterator: Uninitialized]";
  }
    
  return outs.str();
}


/*****************************
 ******* BACK_ITERATOR *******
 *****************************/
  
void back_iterator::operator ++ (int)
{
  ROSE_ASSERT(initialized);
  advance(false, true);
}

/******************************
 ********** DATAFLOW **********
 ******************************/

dataflowIterator::dataflowIterator(const CFGNode terminator_arg) : terminator(terminator_arg)
{
  // Record that the terminator has been visited to ensure that it is never analyzed
  visited.insert(terminator);
}

dataflowIterator::dataflowIterator(const CFGNode start, const CFGNode terminator_arg): 
    iterator(start), terminator(terminator_arg)
{
  // Record that the terminator has been visited to ensure that it is never analyzed
  visited.insert(terminator);
  
  ROSE_ASSERT(start!=terminator);
}

void dataflowIterator::init(const CFGNode start_arg, const CFGNode terminator_arg)
{
  ROSE_ASSERT(start_arg!=terminator_arg);
  // Use the init method to initialize the starting point to make sure that the object is recorded as being initialized
  iterator::init(start_arg);
  terminator = terminator_arg;
}
 
void dataflowIterator::add(const CFGNode next)
{
  // If this dataflow iterator is not initialized, initialize it now since it will now have real state
  if(!initialized) initialized = true;
  
  // never add the terminator node
  if(next==terminator)
    return;
  
  // if next is not currently in remainingNodes, add it
  if(!isRemaining(next))
  {
    set<CFGNode>::iterator nextLoc = visited.find(next);
    if(nextLoc != visited.end())
      visited.erase(visited.find(next));
    remainingNodes.push_back(next);
  }
}

void dataflowIterator::operator ++ (int)
{
  ROSE_ASSERT(initialized);
  advance(true, false);
}

dataflowIterator::checkpoint::checkpoint(const iterator::checkpoint& iChkpt, const CFGNode terminator): 
  iChkpt(iChkpt), terminator(terminator) {}

dataflowIterator::checkpoint::checkpoint(const dataflowIterator::checkpoint &that): 
  iChkpt(that.iChkpt), terminator(that.terminator) {}

string dataflowIterator::checkpoint::str(string indent)
{
  ostringstream outs;
  outs << indent << "[VirtualCFG::dataflowIterator::checkpoint : \n"; 
  outs << indent << "&nbsp;&nbsp;&nbsp;&nbsp;iterator = \n"<<iChkpt.str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;")<<"\n";
  outs << indent << "&nbsp;&nbsp;&nbsp;&nbsp;terminator = ["<<terminator.getNode()->unparseToString()<<" | "<<terminator.getNode()->class_name()<<"]"<<endl;
  return outs.str();
}

// Returns a checkpoint of this iterator's progress.
dataflowIterator::checkpoint dataflowIterator::getChkpt()
{
  ROSE_ASSERT(initialized);
  dataflowIterator::checkpoint chkpt(iterator::getChkpt(), terminator);
  return chkpt;
  //return new dataflowIterator::checkpoint::checkpoint(iterator::getChkpt(), terminator);
}

// Loads this iterator's state from the given checkpoint.
void dataflowIterator::restartFromChkpt(dataflowIterator::checkpoint& chkpt)
{
  iterator::restartFromChkpt(chkpt.iChkpt);
  terminator = chkpt.terminator;
}

string dataflowIterator::str(string indent)
{
  ostringstream outs;
  
  if(initialized) {
    outs << "[dataflowIterator:\n";
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;iterator = "<<iterator::str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;")<<"\n";
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;terminator = ["<<terminator.getNode()->unparseToString()<<" | "<<terminator.getNode()->class_name()<<"]"<<endl;
  } else {
    outs << "[dataflowIterator: Uninitialized]";
  }
    
  return outs.str();
}


/*****************************
******* BACK_DATAFLOW *******
*****************************/
back_dataflowIterator::back_dataflowIterator(const CFGNode end, const CFGNode terminator_arg): 
    iterator(end), dataflowIterator(end, terminator_arg) {
}

void back_dataflowIterator::operator ++ (int)
{
  advance(false, false);
}

string back_dataflowIterator::str(string indent)
{
  ostringstream outs;
  
  if(initialized) {
    outs << "[back_dataflowIterator:\n";
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;iterator = "<<iterator::str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;")<<"\n";
    outs << "&nbsp;&nbsp;&nbsp;&nbsp;terminator = ["<<terminator.getNode()->unparseToString()<<" | "<<terminator.getNode()->class_name()<<"]"<<endl;
  } else {
    outs << "[back_dataflowIterator: Uninitialized]";
  }
    
  return outs.str();
}

}
