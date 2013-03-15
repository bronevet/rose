#include "abstract_object_set.h"
#include <ostream>
#include <typeinfo>

using namespace std;
namespace dataflow
{  
int AbstractObjectSetDebugLevel=0;
// returns true only if the list grows
// false implies element is already present
// insert(o) : In both modes we insert o unless o is must-equal to another AbstractObject already inside the set.
//             This ensures that we account for the entire sets of concrete objects that are in the set, only 
//             saving space when two AbstractObjects definitely correspond to the same concrete object (it would also 
//             be ok if the newly added AbstractObject was a subset of a previously added one). In may mode insert 
//             would also be conservative if we replaced multiple AbstractObject with a single AbstractObject that 
//             over-approximates their union. In must mode insert would also be conservative if we removed any 
//             AbstractObject from the set.
bool AbstractObjectSet::insert(AbstractObjectPtr that)
{
  ROSE_ASSERT(that);
  /*ostringstream label; label << "AbstractObjectSet::insert("<<that->str()<<")";
  Dbg::region reg(AbstractObjectSetDebugLevel, 1, Dbg::region::midLevel, label.str());
  
  Dbg::dbg << "this="<<str()<<endl;*/
  
  // Do not insert mappings for dead keys
  if(!that->isLive(latPEdge, comp, analysis)) { 
    if(AbstractObjectSetDebugLevel>=1) Dbg::dbg << "<b>AbstractObjectSet::insert() WARNING: attempt to insert dead element "<<that->strp(latPEdge)<<"<\b>"<<endl;
    return false;
  }
    
  bool retval = false;
  if(!containsEqualSet(that)) {
    Dbg::indent(AbstractObjectSetDebugLevel, 1);
    if(AbstractObjectSetDebugLevel>=1) Dbg::dbg << "Object not contained."<<endl;// Inserting into"<<endl<<str("")<<endl;
    
    items.push_back(that);
    retval = true;
  }
  
  // Having inserted the new item we need to clean up the map to ensure that it stays bounded in size
  // Step 1: call isEmpty to check for any keys mapped to empty sets
  isEmpty();
  // Step 2: if the map is larger than some fixed bound, merge some key->value mappings together
  // !!! TODO !!!
  
  /*if(!containsMust(that)) {
    // The similarity between that and each AbstractObject currently in items
    map<AbstractObjectPtr, simType> thatSimilarity;
    
    // Compute the similarity between this item and all the items currently in the set
    for(std::list<AbstractObjectPtr>::iterator it = items.begin(); it != items.end(); it++) {
      if((*it)->mayEqual(that, latPEdge)) thatSimilarity[*it] = mayEqual;
      else                                thatSimilarity[*it] = notMayEqual;
    }
    
    // If adding this item will not push this set beyond its maximum size
    if(items.size()<maxElements) {
      items.push_back(that);
      
      // Incorporate that's similarity information into itemSimilarity
      for(map<AbstractObjectPtr, simType>::iterator item=thatSimilarity.begin(); item!=thatSimilarity.end(); item++) {
        itemSimilarity[*item][that] = mayEqual;
        itemSimilarity[that][*item] = mayEqual;
      }
    // Otherwise, find an existing item to merge this item into
    } else {
      // Records the similarity between each current item and the new item
      map<AbstractObjectPtr, int> simScore;
      for(std::list<AbstractObjectPtr>::iterator it = items.begin(); it != items.end(); it++) {
        simScore[*it]=0;
        for(std::map<AbstractObjectPtr, simType>::iterator itSim=items[*it].begin(); itSim!=items[*it].end(); itSim++) {
          if(itSim->first != that) {
          if(itSim->second == )
        }
      }
    }
    retval = true;
  }*/
  
  return retval;
}

// return true on successfull removal
// throws exception if element not found
// remove(o) : In may mode removes any AbstractObject in the set that is mustEqual to o to ensure that we still keep an 
//             over-approximation of the set of concrete objects in the AbstractObjectSet. In must mode uses mayEqual 
//             to ensure that all concrete objects that may equal to o are removed to keep the AbstractObjectSet an 
//             under-approximation.
bool AbstractObjectSet::remove(const AbstractObjectPtr that)
{
    Dbg::indent ind(AbstractObjectSetDebugLevel, 1);
    bool retval = false;
    bool found = false;
    ROSE_ASSERT(that);
    std::list<AbstractObjectPtr>::iterator it = items.begin();
    //Dbg::dbg << "AbstractObjectSet::remove("<<that->str("")<<")"<<endl;
    for(; it != items.end(); it++) {
        /*Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;it="<<(*it)->str("")<<" "<<(mode==may? "mustEqual": "mayEqual")<<"="<<((mode==may  && (*it)->mustEqual(that, latPEdge, comp, analysis)) ||
           (mode==must && (*it)->mayEqual(that, latPEdge, comp, analysis)))<<endl;*/
        if((mode==may  && (*it)->mustEqual(that, latPEdge, comp, analysis)) ||
           (mode==must && (*it)->mayEqual(that, latPEdge, comp, analysis))) {
            items.erase(it);
            found = true;
            break; // only one object that mustEqual(that) should be present
        }
    }

    // if element not found issue a notice
    if(!found) {
        //try {
        if(AbstractObjectSetDebugLevel>=1) {
          Dbg::dbg << "<b>AbstractObjectSet::remove()"<<endl;
          Dbg::indent ind(AbstractObjectSetDebugLevel, 1);
          Dbg::dbg << "Cannot find "<<that->str("")<<endl;
          Dbg::dbg << str("&nbsp;&nbsp;&nbsp;&nbsp;")<<"</b>"<<endl;
        }
        // throw "element not found";
    }
    // end for
    return retval;
}

// Returns true if a mustEqual is present; false otherwise
bool AbstractObjectSet::containsMust(const AbstractObjectPtr that) 
{
  /*Dbg::dbg << "AbstractObjectSet::containsMust("<<that->str("")<<")"<<endl;
  Dbg::indent(1, 1);*/
  ROSE_ASSERT(that);
  bool retval = false;
  std::list<AbstractObjectPtr>::iterator it = items.begin();
  for( ; it != items.end(); it++) {
    //Dbg::dbg << "it="<<(*it)->str()<<" mustEqual="<<(*it)->mustEqual(that, latPEdge, comp, analysis)<<endl;
    if((*it)->mustEqual(that, latPEdge, comp, analysis)) {
      retval = true;
      break;
    } // end if
  } // end for

  return retval;
}

bool AbstractObjectSet::containsMay(const AbstractObjectPtr that)
{
    ROSE_ASSERT(that);
    bool retval = false;
    //Dbg::dbg << "AbstractObjectSet::containsMay("<<that->str("")<<")"<<endl;
    std::list<AbstractObjectPtr>::iterator it = items.begin();
    for( ; it != items.end(); it++) {
        //Dbg::dbg << "&nbsp;&nbsp;&nbsp;&nbsp;it="<<(*it)->str("")<<")"<<endl;
        if((*it)->mayEqual(that, latPEdge, comp, analysis)) {
            retval = true;
            break;
        } // end if
    } // end for
    return retval;
}

// Returns true if this set contains an AbstractObject that denotes the same set as that; false otherwise
bool AbstractObjectSet::containsEqualSet(const AbstractObjectPtr that) 
{
  /*Dbg::dbg << "AbstractObjectSet::containsEqualSet("<<that->str("")<<")"<<endl;
  Dbg::indent(1, 1);*/
  ROSE_ASSERT(that);
  bool retval = false;
  std::list<AbstractObjectPtr>::iterator it = items.begin();
  for( ; it != items.end(); it++) {
    /*Dbg::dbg << "it="<<(*it)->str()<<endl;
    Dbg::dbg << "equalSet="<<(*it)->equalSet(that, latPEdge, comp, analysis)<<endl;*/
    if((*it)->equalSet(that, latPEdge, comp, analysis)) {
      retval = true;
      break;
    } // end if
  } // end for

  return retval;
}

// Set this Lattice object to represent the set of all possible execution prefixes.
// Return true if this causes the object to change and false otherwise.
bool AbstractObjectSet::setToFull()
{
  bool modified = !setIsFull;
  items.clear();
  setIsFull = true;
  return modified;
}

// Set this Lattice object to represent the of no execution prefixes (empty set).
// Return true if this causes the object to change and false otherwise.
bool AbstractObjectSet::setToEmpty()
{
  bool modified = !items.empty();
  items.clear();
  return modified;
}

// Set all the information associated Lattice object with this MemLocObjectPtr to full.
// Return true if this causes the object to change and false otherwise.
// This function does nothing because it is a set of abstract objects rather than a map from some abstract objects
// to others. 
bool AbstractObjectSet::setMLValueToFull(MemLocObjectPtr ml)
{
  return false;
}

// Returns whether this lattice denotes the set of all possible execution prefixes.
bool AbstractObjectSet::isFull()
{
  return setIsFull;
}

// Returns whether this lattice denotes the empty set.
bool AbstractObjectSet::isEmpty()
{
  // Check if all items are empty
  for(std::list<AbstractObjectPtr>::iterator i=items.begin(); i!=items.end();) {
    // If at least one is not empty, return false
    if(!(*i)->isEmpty(getPartEdge(), comp, analysis)) return false;
    
    // If this item is empty, remove it from the items list
    Dbg::dbg << "AbstractObjectSet::isEmpty() removing "<<(*i)->str()<<endl;
    items.erase(i++);
  }
  // If all are empty, return true
  ROSE_ASSERT(items.size()==0);
  return true;
}

// debug: prints the elements of set as string
std::string AbstractObjectSet::str(std::string indent)
{
    return strp(latPEdge, indent);
}

// Variant of the str method that can produce information specific to the current Part.
// Useful since AbstractObjects can change from one Part to another.
std::string AbstractObjectSet::strp(PartEdgePtr pedge, std::string indent)
{
    ostringstream oss;
  
    std::list<AbstractObjectPtr>::iterator it = items.begin();
    oss << "[AbstractObjectSet ("<<items.size()<<")"<<endl;
    while(it != items.end()) {
        /*if(it != items.begin()) */oss << indent;
        oss << (*it)->strp(pedge, "&nbsp;&nbsp;&nbsp;&nbsp;");
        
        it++;
        if(it!=items.end())
          oss << std::endl;
    }
    oss << "]";
    
    return oss.str();
}
    
// -----------------
// Lattice methods
// initializes this Lattice to its default state, if it is not already initialized
void AbstractObjectSet::initialize()
{
  // Nothing to do here since Peter P's fixes will eliminate the need for lattices to maintain their own initialized state
}

// returns a copy of this lattice
Lattice* AbstractObjectSet::copy() const
{
  AbstractObjectSet *n = new AbstractObjectSet(*this);
  ROSE_ASSERT(n);
  return n;
}

// overwrites the state of this Lattice with that of that Lattice
void AbstractObjectSet::copy(Lattice* thatL)
{
  Lattice::copy(thatL);
  
  try {
    AbstractObjectSet *that = dynamic_cast <AbstractObjectSet*> (thatL);
    setIsFull = that->setIsFull;
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
// In must mode for each MemLocObject o in the set, if there exist any pairs <old, new> in ml2ml such that 
//    o mustEquals old, then new will be included in the final set.
// May mode is the same, except if for some pair <old, new> old mayEquals o but not mustEquals o then new is 
//    included in the final set but o is not removed.
Lattice* AbstractObjectSet::remapML(const std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >& ml2ml, PartEdgePtr newPEdge)
{
  if(AbstractObjectSetDebugLevel>=1) {
    // If either the key or the value of this mapping is dead within its respective part, we skip it.
    // Print notices of this skipping once
    for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::const_iterator m=ml2ml.begin(); m!=ml2ml.end(); m++)
      // If either the key or the value of this mapping is dead within its respective part, skip it
      if(!m->first->isLive(latPEdge, comp, analysis) || (m->second && !m->second->isLive(newPEdge, comp, analysis)))
        Dbg::dbg << "AbstractObjectSet::remapML() WARNING: Skipping dead ml2ml mapping "<<m->first->strp(latPEdge)<<"(live="<<m->first->isLive(latPEdge, comp, analysis)<<") => "<<(m->second ? m->second->strp(newPEdge) : "NULL")<<"(live="<<(m->second ? m->second->isLive(newPEdge, comp, analysis) : -1)<<")"<<endl;
  }
  
  Dbg::region reg(AbstractObjectSetDebugLevel, 1, Dbg::region::midLevel, "AbstractObjectSet::remapML");
  
  AbstractObjectSet* newS = new AbstractObjectSet(newPEdge, comp, analysis, mode);
  // Set of ml2ml values that need to be added to newS because they match (may-equal or must-equal)
  // MemLocObjects currently in items
  set<MemLocObjectPtr> vals2add;
  for(std::list<AbstractObjectPtr>::iterator i=items.begin(); i!=items.end(); i++) {
    // Flags that indicate whether the current item is mayEqual or mustEqual any keys in ml2ml
    bool existsMustEqual=false;
    bool existsMayEqual=false;

    for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::const_iterator m=ml2ml.begin(); m!=ml2ml.end(); m++) {
      // If either the key or the value of this mapping is dead within its respective part, skip it
      if(!m->first->isLive(latPEdge, comp, analysis) || (m->second && !m->second->isLive(newPEdge, comp, analysis))) continue;
      
      // If the current item in this set may- or must-equals a key in ml2ml, record this and add the corresponding
      // value in ml2ml to be added to newS
      if((*i)->mustEqual(m->first, latPEdge, comp, analysis)) {
        //Dbg::dbg << "i="<<(*i)->str("")<<" mustEqual m->first="<<m->first->str("")<<" m->second="<<(m->second ? m->second->str("") : "NULL")<<endl;
        existsMustEqual = true;
        // Insert the corresponding value in ml2ml if it is not NULL
        if(m->second) vals2add.insert(m->second);
      } else if(mode == may && (*i)->mayEqual(m->first, latPEdge, comp, analysis)) {
        if(AbstractObjectSetDebugLevel>=1) Dbg::dbg << "i="<<(*i)->str("")<<" mayEqual m->first="<<m->first->str("")<<" m->second="<<(m->second ? m->second->str("") : "NULL")<<endl;
        existsMayEqual = true;
        // Insert the corresponding value in ml2ml if it is not NULL
        vals2add.insert(m->second);
      }
    }
    if(AbstractObjectSetDebugLevel>=1) Dbg::dbg << "existsMustEqual="<<existsMustEqual<<" existsMayEqual="<<existsMayEqual<<endl;
    
    // If this item is not must-equal to some key(s) in ml2ml, copy it over to newS
    if(!existsMustEqual) {
      // Skip items that are dead in newPEdge
      if(!(*i)->isLive(newPEdge, comp, analysis)) continue;
      newS->items.push_back(*i);
    }
    // Otherwise, we skip this item since it will be replaced by the value(s) of the key(s) it was must-equal to
  }
  
  // Now add the values of all the keys in ml2ml that got matched to this set's items
  if(AbstractObjectSetDebugLevel>=1) Dbg::dbg << "vals2add=" << endl;
  for(set<MemLocObjectPtr>::iterator v=vals2add.begin(); v!=vals2add.end(); v++) {
    Dbg::indent ind(AbstractObjectSetDebugLevel,1);
    if(AbstractObjectSetDebugLevel>=1) Dbg::dbg << (*v)->str("") << endl;
    newS->items.push_back(*v);
  }
  
  /*AbstractObjectSet* newS = new AbstractObjectSet(latPEdge, mode);
  Dbg::dbg << "AbstractObjectSet::remapML(#items="<<items.size()<<" #ml2ml="<<ml2ml.size()<<endl;
  Dbg::indent ind(AbstractObjectSetDebugLevel, 1);
  
  for(std::list<AbstractObjectPtr>::iterator i=items.begin(); i!=items.end(); i++)
  {
    Dbg::indent ind(AbstractObjectSetDebugLevel, 1);
    MemLocObjectPtr ml = boost::dynamic_pointer_cast<MemLocObject>(*i);
    ROSE_ASSERT(ml);
    Dbg::dbg << "ml="<<ml.get()->str("")<<endl;
    
    // If the current item is to be transferred from the caller to the callee scopes
    bool remapped = false;
    for(std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >::const_iterator m=ml2ml.begin(); m!=ml2ml.end(); m++) {
      Dbg::indent ind(AbstractObjectSetDebugLevel, 1);
      Dbg::dbg << "Observed ("<<ml->mustEqual(m->first, latPEdge)<<") "<<m->first.get()->str("")<<" => "<<m->second.get()->str("")<<endl;
      if(ml->mustEqual(m->first, latPEdge)) {
        // Add the corresponding value to the new set
        newS->items.push_back(m->second);
        remapped = true; // Record that the current item has been remapped
        break;
      }
    }
    // If the current item was not remapped, add it under its current name
    if(!remapped)
      newS->items.push_back(ml);
  }
  Dbg::dbg<<"newS->items.size()="<<newS->items.size()<<endl;*/
  
  return newS;
}

// Adds information about the MemLocObjects in newL to this Lattice, overwriting any information previously 
//    maintained in this lattice about them.
// Returns true if the Lattice state is modified and false otherwise.
bool AbstractObjectSet::replaceML(Lattice* newL)
{
  AbstractObjectSet* calleeAOS = dynamic_cast<AbstractObjectSet*>(newL);
  ROSE_ASSERT(calleeAOS);
  
  bool modified = false;
  
  for(std::list<AbstractObjectPtr>::iterator i=calleeAOS->items.begin(); i!=calleeAOS->items.end(); i++) {
    MemLocObjectPtr ml = boost::dynamic_pointer_cast<MemLocObject>(*i);
    ROSE_ASSERT(ml);
    // Do not copy over mappings with keys that are dead in this set's host part
    if(!ml->isLive(latPEdge, comp, analysis)) continue;
    
    modified = insert(ml) || modified;
  }
  
  return modified;
}

// computes the meet of this and that and saves the result in this
// returns true if this causes this to change and false otherwise
// The part of this object is to be used for AbstractObject comparisons.
// meet(s1, s2) : In may mode uses insert() to add AbstractObjects from both s1 and s2 to the meet. In must mode only
//             inserts objects into the meet that must exist in both AbstractObjectSets.
bool AbstractObjectSet::meetUpdate(Lattice* thatL)
{
  try {
    Dbg::region reg(AbstractObjectSetDebugLevel, 2, Dbg::region::midLevel, "AbstractObjectSet::meetUpdate");
    AbstractObjectSet *that = dynamic_cast <AbstractObjectSet*> (thatL);
    if(setIsFull) return false;
    if(that->setIsFull) {
      setToFull();
      return true;
    }
    
    // Copy over from that all the elements that don't already exist in this
    bool modified = false;
    if(mode == may) {
      if(AbstractObjectSetDebugLevel>=2) Dbg::dbg << "that->items="<<endl;
      Dbg::indent ind(AbstractObjectSetDebugLevel, 2);
      for(std::list<AbstractObjectPtr>::iterator it=that->items.begin(); it!=that->items.end(); it++) {
        // Do not copy over mappings with keys that are dead in this set's host part
        if(!(*it)->isLive(latPEdge, comp, analysis)) continue;
        
        if(AbstractObjectSetDebugLevel>=2) Dbg::dbg << "it="<<it->get()->str()<<endl;
        modified = insert(*it) || modified;
        if(AbstractObjectSetDebugLevel>=2) Dbg::dbg << "modified = "<<modified<<endl;
      }
    } else if(mode==must) {
      // Remove all the AbstractObjects in this that do not also appear in that
      for(std::list<AbstractObjectPtr>::iterator it=that->items.begin(); it!=that->items.end(); it++) {
        // Do not copy over mappings with keys that are dead in this set's host part
        if(!(*it)->isLive(latPEdge, comp, analysis)) continue;
        
        if(!containsMust(*it))
          modified = remove(*it) || modified;
      }
    }

    return modified;
  } catch (bad_cast & bc) { 
    ROSE_ASSERT(false);
  }
}

bool AbstractObjectSet::operator==(Lattice* thatL)
{
  ROSE_ASSERT(latPEdge == thatL->getPartEdge());
  try {
    AbstractObjectSet *that = dynamic_cast <AbstractObjectSet*> (thatL);
    ROSE_ASSERT(comp     == that->comp);
    ROSE_ASSERT(analysis == that->analysis);
    // GB: This is a quadratic time comparison. Can make it linear if we sort the objects somehow.
    
    // Iterate through this->items and confirm that all its elements are in that->items
    for(std::list<AbstractObjectPtr>::iterator it=items.begin(); it!=items.end(); it++)
    if(!that->containsMust(*it)) return false;
    
    // Iterate through that->items and confirm that all its elements are in this->items
    for(std::list<AbstractObjectPtr>::iterator it=that->items.begin(); it!=that->items.end(); it++)
    if(!that->containsMust(*it)) return false;

    // Everything aligns perfectly
    return true;
  } catch (bad_cast & bc) { 
    ROSE_ASSERT(false);
  }
}

}; // end namespace
