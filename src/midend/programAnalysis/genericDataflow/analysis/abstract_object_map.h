#pragma once
#include "dataflow.h"
#include "compose.h"
#include <rose.h>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <ostream>
#include <fstream>
#include <sstream>
#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/operators.hpp>
#include <filteredCFG.h>
#include "staticSingleAssignment.h"
#include <boost/unordered_map.hpp>
#include "dataflowCfgFilter.h"
#include "CallGraph.h"
#include "uniqueNameTraversal.h"
#include "abstract_object.h"
#include "latticeFull.h"
#include "data_structures.h"

#include <list>
#include <iostream>

namespace dataflow {
  extern int AbstractObjectMapDebugLevel;
  
  using namespace boost;
  using namespace std;
  //using namespace AbstractMemoryObject;
  
  typedef boost::shared_ptr<Lattice> LatticePtr;
  typedef std::pair<AbstractObjectPtr, LatticePtr> MapElement;

  class AbstractObjectMap : public Lattice {
    
  protected: 
    EqualFunctorPtr    equalFunctor;
    MustEqualFunctor   mustEqual;
    MayEqualFunctor    mayEqual;
    list<MapElement>   items;
    // Pointer to a default instance of the given lattice type.
    // AbstractObjectMap::get() can return copies of this object to indicate that
    // there is no mapping for a given AbstractObject.
    LatticePtr         defaultLat;
    // True if all the lattices inserted into this map are finite
    // and false otherwise
    bool isFinite;
    // True if this map has been set to represent the set of all possible AbstractObject->Lattice mappings.
    bool mapIsFull;
    
    // The composer and analysis within which this map is being maintained
    Composer* comp;
    ComposedAnalysis* analysis;
    
  public:
    // GB: Removed the empty constructor since an object constructed this 
    // way would not have a valid equalFunctor and thus would not function
    //AbstractObjectMap() : /*composer(NULL), */isFinite(true) {}
    AbstractObjectMap(const AbstractObjectMap& that) : Lattice(that.latPEdge),
                                   equalFunctor(that.equalFunctor),
                                   items       (that.items),
                                   defaultLat  (that.defaultLat),
                                   isFinite    (that.isFinite),
                                   mapIsFull   (that.mapIsFull),
                                   comp        (that.comp),
                                   analysis    (that.analysis)
    {}
    AbstractObjectMap(EqualFunctor& ef, LatticePtr defaultLat_, PartEdgePtr pedge, Composer* comp, ComposedAnalysis* analysis) : 
      Lattice(pedge), equalFunctor(EqualFunctorPtr(&ef)), defaultLat(defaultLat_), isFinite(true), mapIsFull(false), comp(comp), analysis(analysis) {}
    AbstractObjectMap(EqualFunctor* ef, LatticePtr defaultLat_, PartEdgePtr pedge, Composer* comp, ComposedAnalysis* analysis) : 
      Lattice(pedge), equalFunctor(EqualFunctorPtr(ef)), defaultLat(defaultLat_), isFinite(true), mapIsFull(false), comp(comp), analysis(analysis) {}
    AbstractObjectMap(EqualFunctorPtr efPtr, LatticePtr defaultLat_, PartEdgePtr pedge, Composer* comp, ComposedAnalysis* analysis) :
      Lattice(pedge), equalFunctor(efPtr), defaultLat(defaultLat_), isFinite(true), mapIsFull(false), comp(comp), analysis(analysis) {}
    ~AbstractObjectMap() {}

  public:
    // Add a new memory object --> lattice pair to the frontier.
    // Return true if this causes the map to change and false otherwise.
    // It is assumed that the given Lattice is now owned by the AbstractObjectMap and can be modified and deleted by it.
    bool insert(AbstractObjectPtr abstractObjectPtr, LatticePtr lattice);
    
    // Removes the key matching the argument from the frontier.
    // Return true if this causes the map to change and false otherwise.
    bool remove(AbstractObjectPtr abstractObjectPtr);
    
    // Get all x-frontier for a given abstract memory object
    LatticePtr get(AbstractObjectPtr abstractObjectPtr);
        
    // Set this Lattice object to represent the set of all possible execution prefixes.
    // Return true if this causes the object to change and false otherwise.
    bool setToFull();
    
    // Set this Lattice object to represent the of no execution prefixes (empty set).
    // Return true if this causes the object to change and false otherwise.
    bool setToEmpty();

    // Returns whether this lattice denotes the set of all possible execution prefixes.
    bool isFull();
    // Returns whether this lattice denotes the empty set.
    bool isEmpty();
    
    std::string str(std::string indent="");
    // Variant of the str method that can produce information specific to the current Part.
    // Useful since AbstractObjects can change from one Part to another.
    std::string strp(PartEdgePtr pedge, std::string indent="");
    
    // -----------------
    // Lattice methods
    // initializes this Lattice to its default state, if it is not already initialized
    void initialize();
    
    // returns a copy of this lattice
    Lattice* copy() const;
    
    // overwrites the state of this Lattice with that of that Lattice
    void copy(Lattice* thatL);
    
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
    //    remapML must return a freshly-allocated object.
    // remapML must return a freshly-allocated object.
    Lattice* remapML(const std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >& ml2ml, PartEdgePtr newPEdge);

    // Adds information about the MemLocObjects in newL to this Lattice, overwriting any information previously 
    //    maintained in this lattice about them.
    // Returns true if the Lattice state is modified and false otherwise.
    bool replaceML(Lattice* newL);
    
    // computes the meet of this and that and saves the result in this
    // returns true if this causes this to change and false otherwise
    bool meetUpdate(Lattice* that);
    
    // Identify keys that are must-equal to each other and merge their lattices
    // Return true if this causes the object to change and false otherwise.
    bool compressMustEq();

    // Remove all mappings with dead keys from this map.
    // Return true if this causes the object to change and false otherwise.
    bool compressDead();
    
    bool finiteLattice();
    
    // !!! UNIMPLEMENTED
    bool operator==(Lattice* that);
  };
}; // namespace dataflow;
