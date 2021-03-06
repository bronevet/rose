#include <iostream>
#include "abstract_object_map.h"
#include "analysisCommon.h"
#include "nodeState.h"
#include "functionState.h"
//#include "LatticeArithInstance.h"
#include "analysis.h"
#include "uniqueNameTraversal.h"
#include "defsAndUsesTraversal.h"
#include "iteratedDominanceFrontier.h"
#include "controlDependence.h"
//#include "heapDefsAndUsesTraversal.h"

/**     
 * This is an implementation of the abstract memory object --> lattice map 
 *           
 * author: jisheng zhao (jz10@rice.edu)   
 */

#define foreach BOOST_FOREACH
#define reverse_foreach BOOST_REVERSE_FOREACH

using namespace std;
using namespace boost;
using namespace ssa_private;
using namespace dataflow;

namespace dataflow {
int AbstractObjectMapDebugLevel=1;

bool MayEqualFunctor::mayEqual(AbstractObjectPtr objPtr1, AbstractObjectPtr objPtr2, PartEdgePtr pedge) {
  ROSE_ASSERT(objPtr1); ROSE_ASSERT(objPtr2);
  
  boost::shared_ptr<Pointer> ptr1 = boost::dynamic_pointer_cast<Pointer>(objPtr1);
  boost::shared_ptr<Pointer> ptr2 = boost::dynamic_pointer_cast<Pointer>(objPtr2);

  bool needDeref = false;
  if (ptr1) {
    objPtr1 = boost::static_pointer_cast<AbstractObject>(ptr1->getDereference(pedge));
    needDeref = true;
  } 
  if (ptr2) {
    objPtr2 = boost::static_pointer_cast<AbstractObject>(ptr2->getDereference(pedge));
    needDeref = true;
  }

  if (needDeref) 
    return mayEqual(objPtr1, objPtr2, pedge);
  else
    return objPtr1->mayEqual(objPtr2, pedge) || objPtr2->mayEqual(objPtr1, pedge);
}

bool MustEqualFunctor::mustEqual(AbstractObjectPtr objPtr1, AbstractObjectPtr objPtr2, PartEdgePtr pedge) {
  ROSE_ASSERT(objPtr1); ROSE_ASSERT(objPtr2);

  boost::shared_ptr<Pointer> ptr1 = boost::dynamic_pointer_cast<Pointer>(objPtr1);
  boost::shared_ptr<Pointer> ptr2 = boost::dynamic_pointer_cast<Pointer>(objPtr2);

  bool needDeref = false;
  if (ptr1) {
    objPtr1 = boost::static_pointer_cast<AbstractObject>(ptr1->getDereference(pedge));
    needDeref =true;
  }
  if (ptr2) {
    objPtr2 = boost::static_pointer_cast<AbstractObject>(ptr2->getDereference(pedge));
    needDeref = true;
  }

/*Dbg::dbg << "MustEqualFunctor::mustEqual\n";
Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<objPtr1->str("        ")<<"\n";
Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<objPtr2->str("        ")<<endl;*/

  if (needDeref)
    return mustEqual(objPtr1, objPtr2, pedge);
  else
    //return objPtr1->mustEqual(objPtr2, p) || objPtr2->mustEqual(objPtr1, p);
    return objPtr1->mustEqual(objPtr2, pedge);
}

// Set this Lattice object to represent the set of all possible execution prefixes.
// Return true if this causes the object to change and false otherwise.
bool AbstractObjectMap::setToFull()
{
  bool modified = !isFull;
  items.clear();
  isFull = true;
  return modified;
}

// Set this Lattice object to represent the of no execution prefixes (empty set).
// Return true if this causes the object to change and false otherwise.
bool AbstractObjectMap::setToEmpty()
{
  bool modified = !items.empty();
  items.clear();
  return modified;
}

std::string AbstractObjectMap::str(std::string indent) {
  return strp(latPEdge, indent);
}

// Variant of the str method that can produce information specific to the current Part.
// Useful since AbstractObjects can change from one Part to another.
std::string AbstractObjectMap::strp(PartEdgePtr pedge, std::string indent)
{
  ostringstream oss;
  oss << "[AbstractObjectMap ("<<pedge->str()<<"): ";
  
  //printf("[AbstractObjectMap: "); fflush(stdout);
  for(list<MapElement>::iterator it = items.begin();
       it != items.end(); it++) {
    //printf("\n%s%p => %p\n", indent.c_str(), it->first.get(), it->second.get()); fflush(stdout);
    oss << endl<<indent<<"&nbsp;&nbsp;&nbsp;&nbsp;"<<it->first->strp(pedge, indent+"&nbsp;&nbsp;&nbsp;&nbsp;")<<" => "<<it->second->str(indent+"&nbsp;&nbsp;&nbsp;&nbsp;");
  }
  oss << "]";
  return oss.str();
}

// Add a new memory object --> lattice pair to the frontier.
// Return true if this causes the map to change and false otherwise.
bool AbstractObjectMap::insert(AbstractObjectPtr o, LatticePtr lattice) {
  Dbg::region reg(AbstractObjectMapDebugLevel, 1, Dbg::region::topLevel, "AbstractObjectMap::insert()");
  if(AbstractObjectMapDebugLevel>=1) {
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;o="<<o->strp(latPEdge, "")<<" lattice="<<lattice->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<" isFull="<<isFull<<endl;
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  }
  
  // Do not insert mappings for dead keys
  if(!o->isLive(latPEdge)) { 
    if(AbstractObjectMapDebugLevel>=1) Dbg::dbg << "<b>AbstractObjectMap::insert() WARNING: attempt to insert dead mapping "<<o->strp(latPEdge)<<" => "<<lattice->str()<<"<\b>"<<endl;
    return false;
  }
  
  isFinite = isFinite && lattice->finiteLattice();
  // If this map corresponds to all possible mappings, all insertions are redundant
  if(isFull) { return false; }
  
  bool retVal = false;
  bool insertDone = false;
  bool mustEqualSeen = false;
  
  // First, check if there is a key on the frontier that must-equals o to determine
  // if we can just combine the new lattice with the old mapping
  list<MapElement>::iterator it;
  int i=0;
  for(it = items.begin(); it != items.end(); i++) {
    AbstractObjectPtr keyElement = it->first;
    if(AbstractObjectMapDebugLevel>=2) Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;keyElement="<<keyElement->str("            ")<<" mustEqual(o, keyElement, latPEdge)="<<mustEqual(o, keyElement, latPEdge)<<" insertDone="<<insertDone<<" mustEqualSeen="<<mustEqualSeen<<endl;
    // If we're done inserting, don't do it again
    if(insertDone) {
      // If o is mustEqual to this element and it is not the first match, remove this element
      if(mustEqual(o, keyElement, latPEdge)) {
        if(mustEqualSeen) {
          list<MapElement>::iterator itNext = it;
          itNext++;
          items.erase(it);
          it = itNext;
        } else 
          it++;
        mustEqualSeen = true;
      } else 
      it++;
      continue;
    }
  
    // If the o-frontier contains an object that must-equal to 
    if(mustEqual(o, keyElement, latPEdge)) {
      if(AbstractObjectMapDebugLevel==1) Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;keyElement="<<keyElement->str("            ")<<" mustEqual(o, keyElement, latPEdge)="<<mustEqual(o, keyElement, latPEdge)<<" insertDone="<<insertDone<<" mustEqualSeen="<<mustEqualSeen<<endl;
      if(AbstractObjectMapDebugLevel>=1) {
        Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;Must Equal"<<endl;
        Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;lattice="<<lattice->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
        Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;it="<<it->second->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
      }

      // If the old and new mappings of o are different,  we remove the old mapping and add a new one 
      if(!it->second->equiv(lattice.get()))
      {
        if(AbstractObjectMapDebugLevel==1) Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;keyElement="<<keyElement->str("            ")<<" mustEqual(o, keyElement, latPEdge)="<<mustEqual(o, keyElement, latPEdge)<<" insertDone="<<insertDone<<" mustEqualSeen="<<mustEqualSeen<<endl;
        if(AbstractObjectMapDebugLevel>=1) Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;Removing i="<<i<<", inserting "<<o->strp(latPEdge, "        ")<<"=>"<<lattice->str("        ")<<endl;
        list<MapElement>::iterator itNext = it;
        itNext++;
        items.erase(it);
        it = itNext;
        items.push_front(MapElement(o, lattice));
        retVal = true;
      } else {
        //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;No Change"<<endl;
        it++;
        // Otherwise, they're identical and thus there is no need to modify the map
        retVal = false;
      }
      insertDone = true;
      mustEqualSeen = true;
    }
    // If the element on the o-frontier may-equals o then insert a new o->lattice mapping 
    // since the new lattice cannot be combined with the mapping of *it
    else if(mayEqual(o, keyElement, latPEdge)) { 
      items.push_front(MapElement(o, lattice));
      it++;
      retVal = true;
      insertDone = true;
    } else
      it++;
  }
  
  if(!insertDone) {
    // There are no objects within this map on the o-frontier. As such, add an o->lattice mapping
    items.push_front(MapElement(o, lattice));
    
    retVal = true;
  }

  if(AbstractObjectMapDebugLevel>=1) {
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;retVal="<<retVal<<" insertDone="<<insertDone<<" mustEqualSeen="<<mustEqualSeen<<endl;
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  }
  return retVal;
};

// Removes the key matching the argument from the frontier.
// Return true if this causes the map to change and false otherwise.
bool AbstractObjectMap::remove(AbstractObjectPtr abstractObjectPtr) {
// If this map corresponds to all possible mappings, all removals are redundant
  if(isFull) { return false; }
  
  for (list<MapElement>::iterator it = items.begin();
       it != items.end(); it++) {
    AbstractObjectPtr keyElement = it->first;
    // For remove operation, we use must equal policy                                                               
    if (mustEqual(abstractObjectPtr, keyElement, latPEdge)) {
      it = items.erase(it);
      return true;
    }
  }
  return false;
};

// Get all x-frontier for a given abstract memory object                                                            
LatticePtr AbstractObjectMap::get(AbstractObjectPtr abstractObjectPtr) {
  if(AbstractObjectMapDebugLevel>=1) {
    Dbg::enterFunc("AbstractObjectMap::get()");
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;o="<<abstractObjectPtr->strp(latPEdge, "&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  }
  
  // If this map corresponds to all possible mappings, the only mapping that exists for any object is the full lattice
  if(isFull) { 
    LatticePtr fullLat(defaultLat->copy());
    fullLat->setToFull();
    if(AbstractObjectMapDebugLevel>=1) Dbg::exitFunc("AbstractObjectMap::get()");
    return fullLat;
  }
  
  LatticePtr ret;
  for (list<MapElement>::iterator it = items.begin();
       it != items.end(); it++) {
    AbstractObjectPtr keyElement = it->first;
    // For get operation, we can apply either must or may equal policy. This depends on the                         
    // functor
    bool eq = abstractObjectPtr->mayEqual(keyElement, latPEdge);
    if(AbstractObjectMapDebugLevel>=2 || (AbstractObjectMapDebugLevel>=1 && eq)) Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;keyElement(equal="<<eq<<")="<<keyElement->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    if(eq) {
      // If this is the first matching Lattice, copy this Lattice to ret
      if(!ret) ret = boost::shared_ptr<Lattice>(it->second->copy());
      // Otherwise, merge this latice into ret
      else     ret->meetUpdate(it->second.get());
      if(AbstractObjectMapDebugLevel>=1) Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;ret="<<ret->str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
    }
  }
  
  if(AbstractObjectMapDebugLevel>=1) {
    Dbg::dbg << "ret="<<(ret ? ret->str("&nbsp;&nbsp;&nbsp;&nbsp;"): "NULL")<<endl;
    Dbg::exitFunc("AbstractObjectMap::get()");
  }
  if(ret) return ret;
  // If there is no match for abstractObjectPtr, return a copy of the default lattice
  return LatticePtr(defaultLat->copy());
};

// initializes this Lattice to its default state, if it is not already initialized
void AbstractObjectMap::initialize()
{
  // Nothing to do here since Peter P's fixes will eliminate the need for lattices to maintain their own initialized state
}
 
// returns a copy of this lattice
Lattice* AbstractObjectMap::copy() const
{ return new AbstractObjectMap(*this); }

// overwrites the state of this Lattice with that of that Lattice
void AbstractObjectMap::copy(Lattice* thatL) {
  Lattice::copy(thatL);
  try {
    AbstractObjectMap * that = dynamic_cast <AbstractObjectMap*> (thatL);
    equalFunctor = that->equalFunctor;
    items = that->items;
  } catch (bad_cast & bc) { 
    ROSE_ASSERT(false);
  }
}

// Called by analyses to transfer this lattice's contents from across function scopes from a caller function 
//    to a callee's scope and vice versa. If this this lattice maintains any information on the basis of 
//    individual MemLocObjects these mappings must be converted, with MemLocObjects that are keys of the ml2ml 
//    replaced with their corresponding values. If a given key of ml2ml does not appear in the lattice, it must
//    be added to the lattice and assigned a default initial value. In many cases (e.g. over-approximate sets 
//    of MemLocObjects) this may not require any actual insertions. If the value of a given ml2ml mapping is 
//    NULL (empty boost::shared_ptr), any information for MemLocObjects that must-equal to the key should be 
//    deleted.
// The function takes newPEdge, the edge that points to the part within which the values of ml2ml should be 
//    interpreted. It corresponds to the code region(s) to which we are remapping.
// remapML must return a freshly-allocated object.
Lattice* AbstractObjectMap::remapML(const std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >& ml2ml, PartEdgePtr newPEdge)
{
  if(isFull) { return copy(); }
  
  if(AbstractObjectMapDebugLevel>=1) {
    // If either the key or the value of this mapping is dead within its respective part, we skip it.
    // Print notices of this skipping once
    for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::const_iterator m=ml2ml.begin(); m!=ml2ml.end(); m++)
      // If either the key or the value of this mapping is dead within its respective part, skip it
      if(!m->first->isLive(latPEdge) || (m->second && !m->second->isLive(newPEdge)))
        Dbg::dbg << "<b>AbstractObjectMap::remapML() WARNING: Skipping dead ml2ml mapping "<<m->first->strp(latPEdge)<<"(live="<<m->first->isLive(latPEdge)<<") => "<<(m->second ? m->second->strp(newPEdge) : "NULL")<<"(live="<<(m->second ? m->second->isLive(newPEdge) : -1)<<")"<<endl
                 << "&nbsp;&nbsp;&nbsp;&nbsp;latPEdge=["<<latPEdge->str()<<"]"<<endl
                 << "&nbsp;&nbsp;&nbsp;&nbsp;newPEdge=["<<newPEdge->str()<<"]</b>"<<endl;
  }
  
  AbstractObjectMap* newM = new AbstractObjectMap(equalFunctor, defaultLat, newPEdge);
  // Vector of flags that indicate whether a given key in ml2ml has been added to newM or not
  vector<bool> ml2mlAdded;
  // Initialize ml2mlAdded to all false
  for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::const_iterator m=ml2ml.begin(); m!=ml2ml.end(); m++)
    ml2mlAdded.push_back(false);
  
  for(std::list<MapElement>::iterator i=items.begin(); i!=items.end(); i++) {
    // Flags that indicate whether the current item is mustEqual to any keys in ml2ml
    bool existsMustEqual=false;
    //Dbg::dbg << "i="<<i->first->str()<<endl;
    
    int mIdx=0;
    for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::const_iterator m=ml2ml.begin(); m!=ml2ml.end(); m++, mIdx++) {
      //Dbg::indent ind(1, 1);
      //Dbg::dbg << mIdx << ": m="<<m->first->strp(latPEdge)<<endl;
      
      // If either the key or the value of this mapping is dead within its respective part, skip it
      if(!m->first->isLive(latPEdge) || (m->second && !m->second->isLive(newPEdge))) continue;
      
      // If the current item in this set may- or must-equals a key in ml2ml, record this and add the corresponding
      // value in ml2ml to be added to newS
      if(i->first->mustEqual(m->first, latPEdge)) {
        existsMustEqual = true;
        // Insert the corresponding value in ml2ml if it is not NULL
        if(m->second) newM->items.push_back(make_pair(boost::static_pointer_cast<AbstractObject>(m->second), i->second));
        // Remove the current pair from ml2mlCopy so we know that it doesn't need to be assigned the default lattice
        //ml2mlCopy.erase(m);
        ml2mlAdded[mIdx]=true;
      } else if(i->first->mayEqual(m->first, latPEdge)) {
        // Insert the corresponding value in ml2ml if it is not NULL
        if(m->second) newM->items.push_front(make_pair(boost::static_pointer_cast<AbstractObject>(m->second), i->second));
        // Remove the current pair from ml2mlCopy so we know that it doesn't need to be assigned the default lattice
        //ml2mlCopy.erase(m);
        ml2mlAdded[mIdx]=true;
      }
    }
    
    // If this item is not must-equal to some key(s) in ml2ml, copy it over to newS
    if(!existsMustEqual) {
      //Dbg::dbg << "!existsMustEqual, i live="<<i->first->isLive(newPEdge)<<", i->first="<<i->first->str("")<<"="<<i->first->strp(newPEdge)<<endl;
      
      // Skip items that are dead in newPEdge
      if(!i->first->isLive(newPEdge)) continue;
      newM->items.push_back(make_pair(i->first, i->second));
    }
    // Otherwise, we skip this item since it will be replaced by the value(s) of the key(s) it was must-equal to
  }
  
  // Iterate through the false mappings in ml2mlAdded (ml2ml keys that were not mapped to any items in this map)
  // and add to newM a mapping of their values to defaultLat (as long as the values are not NULL)
  int mIdx=0;
  for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::iterator m=ml2ml.begin(); m!=ml2ml.end(); m++, mIdx++) {
    // If either the key or the value of this mapping is dead within its respective part, skip it
    if(!m->first->isLive(latPEdge) || !m->second->isLive(newPEdge)) continue;
    
    if(!ml2mlAdded[mIdx] && m->second)
      newM->items.push_back(make_pair(m->second, defaultLat->copy()));
  }
  
  /*AbstractObjectMap* newM = new AbstractObjectMap(equalFunctor, defaultLat, latPEdge);
  std::set<pair<MemLocObjectPtr, MemLocObjectPtr> > ml2mlCopy = ml2ml;
  
  for(std::list<MapElement>::iterator i=items.begin(); i!=items.end(); i++)
  {
    // If the current item is to be transferred from the caller to the callee scopes
    bool remapped = false;
    for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::iterator m=ml2mlCopy.begin(); m!=ml2mlCopy.end(); m++) {
      if(i->first->mustEqual(m->first, latPEdge)) {
        // Add the corresponding value and the lattice mapping to the new map 
        // in the same order as they appear in the original map
        newM->items.push_back(make_pair(boost::static_pointer_cast<AbstractObject>(m->second), i->second));
        // Remove the current pair from ml2mlCopy so we know that it doesn't need to be assigned the default lattice
        ml2mlCopy.erase(m);
        // Record that the current key has been remapped
        remapped = true; 
        break;
      }
    }
    // If the current key was not remapped, add it under its current name
    if(!remapped)
      newM->items.push_back(make_pair(i->first, i->second));
  }
  
  // Iterate through the remaining mappings in ml2mlCopy and add to newM a mapping of their values to defaultLat
  for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::iterator m=ml2mlCopy.begin(); m!=ml2mlCopy.end(); m++) {
    newM->items.push_back(make_pair(m->second, defaultLat->copy()));
  }*/
  
  return newM;
}

// Adds information about the MemLocObjects in newL to this Lattice, overwriting any information previously 
//    maintained in this lattice about them.
// Returns true if the Lattice state is modified and false otherwise.
bool AbstractObjectMap::replaceML(Lattice* newL)
{
  AbstractObjectMap* calleeAOM = dynamic_cast<AbstractObjectMap*>(newL);
  ROSE_ASSERT(calleeAOM);
  
  bool modified = false;
  
  for(std::list<MapElement>::iterator i=calleeAOM->items.begin(); i!=calleeAOM->items.end(); i++) {
    // Do not copy over mappings with keys that are dead in this map's host part
    if(!i->first->isLive(latPEdge)) continue;
    modified = insert(i->first, i->second) || modified;
  }
  
  
  return modified;
}

// Computes the meet of this and that and saves the result in this
// returns true if this causes this to change and false otherwise
// The part of this object is to be used for AbstractObject comparisons.
bool AbstractObjectMap::meetUpdate(Lattice* thatL)
{
  /*Dbg::dbg << "AbstractObjectMap::meetUpdate()"<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;this="<<str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;thatL="<<thatL->str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;*/
  
  // Both incorporateVars() and meetUpdate currently call merge. This is clearly not
  // right but we'll postpone fixing it until we have the right algorithm for merges
  try {
    bool modified = false;
    AbstractObjectMap * that = dynamic_cast <AbstractObjectMap*> (thatL);
    
    // This algorithm is based on the following insights:
    // Given two AbstractObjectMaps: A: a_0 => a_1 => ... => a_n, and
    //                     B: b_0 => b_1 => ... => b_m
    //    where x => y if x is inserted after y
    // The only non-conservative thing that a merge algorithm can do is take a_i and b_j, where
    //   mustEqual(a_i, b_j) and place one before the other in the merged list. For these cases
    //   it is necessary to insert a single entry a_i -> A[a_i] merge B[b_j].
    // All elements that do not have a mustEqual partner in the same AbstractObjectMap or the other AbstractObjectMap 
    //   can be inserted into the new map in arbitrary order. AbstractObjectMap::insert ensures that no two elements
    //   that are mustEqual to each other may be in the same map. For elements across maps the following
    //   holds. If a_p and b_q are mayEqual to each other, then keeping them in the reverse order from
    //   their correct assignment order is conservative but may lose precision. This is because if 
    //   a_p => b_q but are stored in the opposite order, mayEqual(x, b_p), mustEqual(a_p)
    //   AbstractObjectMap::get(x) will merge the lattices stored at the two keys whereas the precise result would
    //   be to return the lattice stored under a_p. If a_p and b_q are not mayEqual to each other,
    //   either order is fine with no loss of precision.
    // The algorithm below chooses a simple order that is likely to work well in practice. It connects
    //   the pairs of elements in this->items(A) and that->items(B) that are mustEqual and then
    //   scans over each pair <a_i, b_j> them in the order they appear A, copying all the elements between
    //   the current b_j and the last b_j' if they've not already been copied over and if they don't have a
    //   mustEquals partner in A (these are handled by merging, as described above).
    //   A: a_0 => a_1 => r => a_2 => s => a_3 => t
    //   B: b_0 => r => b_1 => t => b_2 => b_3 => s
    //   merged: a_0 => a_1 => b_0 => r => a_2 => b_1 => b_2 => b_3 => s => a_3 => t
    
    // For each element x in this->items pointers that is mustEqual to an element y in
    // that->items, keeps the triple
    //    - iterator that points to x in this->items
    //    - iterator that points to y in that->items
    //    - index of y in that->items
    // Maintained in order of this->items.
    list<pair<list<MapElement>::iterator, pair<list<MapElement>::iterator, int> > > thisMustEq2thatMustEq;

    // For each element in that->items keeps true if this element is mustEquals to some
    // element in this->items and false otherwise.
    list<bool> thatMustEq;

    // Initialize thatMustEq to all false
    for(list<MapElement>::iterator itThat=that->items.begin(); itThat!=that->items.end(); itThat++)
      thatMustEq.push_back(false);

    // Determine which elements in this->items are mustEqual to elements in that->items
    // and for these pairs merge the lattices from that->items to this->items.
    for(list<MapElement>::iterator itThis=items.begin(); 
       itThis!=items.end(); itThis++) {
      int i=0;
      list<bool>::iterator thatMEIt=thatMustEq.begin();
      for(list<MapElement>::iterator itThat=that->items.begin(); 
         itThat!=that->items.end(); itThat++, i++, thatMEIt++) {
       // If we've found mustEqual pair of keys in this and that 
       if(mustEqual(itThis->first, itThat->first, latPEdge)) {
        // Record this pair
        thisMustEq2thatMustEq.push_back(make_pair(itThis, make_pair(itThat, i)));
        *thatMEIt = true;
        
        // Update the lattice at *itThis to incorporate information at *itThat
        {
          // First copy the lattice since it may change. We don't deep-copy lattices when we copy
          // AbstractObjectMaps, so multiple maps may contain references to the same lattice.
          // As such, instead of updating lattices in-place (this would update the same lattice
          // in other maps) we first copy them and update into the copy.
          itThis->second = LatticePtr(itThis->second->copy());
          modified = itThis->second->meetUpdate(itThat->second.get()) || modified;
        }
       }
      }
    }
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;thisMustEq2thatMustEq.size()="<<thisMustEq2thatMustEq.size()<<"\n";
    
    // Copy over the mappings of all the elements in that->items that were not mustEqual
    // to any elements in this->items. Although any order will work for these elements,
    // keep them their order in that->items.
    int thatIdx=0;
    list<MapElement>::iterator thatIt = that->items.begin();
    list<bool>::iterator thatMEIt=thatMustEq.begin();
    for(list<pair<list<MapElement>::iterator, pair<list<MapElement>::iterator, int> > >::iterator meIt=thisMustEq2thatMustEq.begin();
       meIt!=thisMustEq2thatMustEq.end(); meIt++) {
      // Copy over all the mappings from that->items from thatIt to meIt's partner in that->items
      // if they have not already been copied because elements that are mustEqual to each other were ordered
      // differently in this->items and that->items
      if(meIt->second.second >= thatIdx) {
        for(; thatIt!=meIt->second.first; thatIt++, thatIdx++, thatMEIt++)
          // Copy over the current element from that->items if it doesn't have a mustEqual 
          // partner in this->items (i.e. its already been handled)
          if(!(*thatMEIt)) {
            // NOTE: we do not currently update the part field in the lattice thatIt->second
            //       to refer to this->latPEdge. Perhaps we should make a copy of it and update it.
            items.insert(meIt->first, *thatIt);
            modified = true;
          }
        // Advance thatIt and thatIdx once more to account for the partner in that->items 
        // of the current entry in this->items
        thatIt++;
        thatIdx++;
      }
    }
    
    // Add all the elements from that->items that remain
    for(; thatIt!=that->items.end(); thatIt++) {
      // NOTE: we do not currently update the part field in the lattice thatIt->second
      //       to refer to this->latPEdge. Perhaps we should make a copy of it and update it.
      items.push_back(*thatIt);
      modified = true;
    }
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;items.size()="<<items.size()<<"\n";
    
    // Compress all the elements from that are now mustEqual to each other in this->latPEdge.
    // Note: the optimal way to do this is to compress that->mustEqual first and then
    //       merge but we're not allowed to modify that so the compression would need
    //       to be done non-destructively via some additional datastructure. We avoid
    //       this complication for now but should revisit this question if we identify
    //       this code region as a performance bottleneck.
    compressMustEq();
    
    // Remove all the dead keys
    compressDead();
    
    return modified;
  } catch (bad_cast & bc) { 
    ROSE_ASSERT(false);
  }
}

// Identify keys that are must-equal to each other and merge their lattices
// Return true if this causes the object to change and false otherwise.
bool AbstractObjectMap::compressMustEq()
{
  if(isFull) { return false; }
  
  /*Dbg::dbg << "compressMustEq()"<<endl;
  Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<str("&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;*/
  
  bool modified = false;
  int xIdx=0;
  for(list<MapElement>::iterator x = items.begin(); x != items.end(); x++, xIdx++) {
    //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;"<<xIdx<<" : x="<<x->first->str("")<<endl;
    // y starts from the element that follows x
    list<MapElement>::iterator y = x;
    y++;
    int yIdx = xIdx+1;
    for(; y != items.end(); yIdx++) {
      //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"<<yIdx<<" : y="<<y->first->str("")<<endl;
      // If x and y are equal, merge their lattices and remove the later one
      if(mustEqual(x->first, y->first, latPEdge)) {
        //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;MERGING and REMOVING"<<endl;
        // First copy the lattice since it may change. We don't deep-copy lattices when we copy
        // AbstractObjectMaps, so multiple maps may contain references to the same lattice.
        // As such, instead of updating lattices in-place (this would update the same lattice
        // in other maps) we first copy them and update into the copy.
        x->second = LatticePtr(x->second->copy());
        modified = x->second->meetUpdate(y->second.get()) || modified;
        
        list<MapElement>::iterator tmp = y;
        y++;
        items.erase(tmp);
        
        //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;map="<<str("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")<<endl;
        
        modified = true;
      } else
        y++;
    }
  }

  return modified;
};

// Remove all mappings with dead keys from this map.
// Return true if this causes the object to change and false otherwise.
bool AbstractObjectMap::compressDead()
{
  if(isFull) { return false; }
        
  bool modified = false;
  for(list<MapElement>::iterator i = items.begin(); i != items.end(); ) {
    // Remove mappings with dead keys
    if(!(i->first->isLive(latPEdge))) {
      list<MapElement>::iterator nextI = i;
      nextI++;
      
      items.erase(i);
      modified = true;
      
      i = nextI;
    } else
      i++;
  }
  
  return modified;
}

bool AbstractObjectMap::finiteLattice()
{
  return isFinite;
}

bool AbstractObjectMap::operator==(Lattice* that)
{
  ROSE_ASSERT(latPEdge == that->getPartEdge());
  // This will be written once we have the merging algorithm to test
  // these maps' frontiers for semantic equivalence
  return false;
}
}; // namespace dataflow
