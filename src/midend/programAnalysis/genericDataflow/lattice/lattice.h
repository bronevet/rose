#ifndef LATTICE_H
#define LATTICE_H

#include "CallGraphTraverse.h"
//#include "variables.h"
#include "partitions.h"
#include "abstract_object.h"
#include <string>
#include <map>

namespace dataflow {
class Lattice : public printable
{
        public:
        PartPtr part;
        //Lattice() {}
        Lattice(PartPtr p) : part(p) {}
        
        public:
        // Sets the Part that this Lattice's information corresponds to
        void setPart(PartPtr p) { this->part = p; }
        PartPtr getPart()       { return this->part; }
        
        // initializes this Lattice to its default state, if it is not already initialized
        virtual void initialize()=0;
        // returns a copy of this lattice
        virtual Lattice* copy() const=0;
        // overwrites the state of this Lattice with that of that Lattice
        virtual void copy(Lattice* that)=0;
        
        // Called by analyses to transfer this lattice's contents from across function scopes from a caller function 
        //    to a callee's scope and vice versa. If this this lattice maintains any information on the basis of 
        //    individual MemLocObjects these mappings must be converted, with MemLocObjects that are keys of the ml2ml 
        //    replaced with their corresponding values. If a given key of ml2ml does not appear in the lattice, it must
        //    be added to the lattice and assigned a default initial value. In many cases (e.g. over-approximate sets 
        //    of MemLocObjects) this may not require any actual insertions. If the value of a given ml2ml mapping is 
        //    NULL (empty boost::shared_ptr), any information for MemLocObjects that must-equal to the key should be 
        //    deleted.
        // The function takes newPart, the part within which the values of ml2ml should be interpreted. It corresponds
        //    to the code region(s) to which we are remapping.
        virtual Lattice* remapML(const std::set<pair<MemLocObjectPtr, MemLocObjectPtr> >& ml2ml, PartPtr newPart) {
          return false;
        }
        
        // Adds information about the MemLocObjects in newL to this Lattice, overwriting any information previously 
        //    maintained in this lattice about them.
        // Returns true if the Lattice state is modified and false otherwise.
        virtual bool replaceML(Lattice* newL)
        {
          return false;
        }
        
        // Computes the meet of this and that and saves the result in this
        // returns true if this causes this to change and false otherwise
        // The part of this object is to be used for AbstractObject comparisons.
        virtual bool meetUpdate(Lattice* that)=0;
        
        // Returns true if this Lattice implies that lattice (its constraints are equal to or tighter than those of 
        // that Lattice) and false otherwise.
        virtual bool implies(Lattice* that) {
          // this is tighter than that if meeting that into this causes this to change (that contains possibilities 
          // not already in this) but not vice versa (all the possibilities in this already exist in that)
          Lattice* thisCopy = copy();
          if(!thisCopy->meetUpdate(that)) { 
            delete thisCopy;
            return false;
          }
          delete thisCopy;
          
          Lattice* thatCopy = that->copy();
          if(thatCopy->meetUpdate(this)) {
            delete thatCopy;
            return false;
          }
          delete thatCopy;
          return true;
        }
        
        // Returns true if this Lattice is semantically equivalent to that lattice (both correspond to the same set
        // of application executions).
        virtual bool equiv(Lattice* that) {
          // this and that are equivalent if meeting either one with the other causes no changes
          Lattice* thisCopy = copy();
          if(thisCopy->meetUpdate(that)) { 
            delete thisCopy;
            return false;
          }
          delete thisCopy;
          
          Lattice* thatCopy = that->copy();
          if(thatCopy->meetUpdate(this)) {
            delete thatCopy;
            return false;
          }
          delete thatCopy;
          return true;
        }
        
        // Computes the meet of this and that and returns the result
        virtual bool finiteLattice()=0;
        
        virtual bool operator==(Lattice* that) /*const*/=0;
        bool operator!=(Lattice* that) {
                return !(*this == that);
        }
        bool operator==(Lattice& that) {
                return *this == &that;
        }
        bool operator!=(Lattice& that) {
                return !(*this == that);
        }
        
        // Set this Lattice object to represent the set of all possible execution prefixes.
        // Return true if this causes the object to change and false otherwise.
        virtual bool setToFull()=0;
        // Set this Lattice object to represent the of no execution prefixes (empty set).
        // Return true if this causes the object to change and false otherwise.
        virtual bool setToEmpty()=0;
        
        // Functions used to inform this lattice that a given variable is now in use (e.g. a variable has entered 
        //    scope or an expression is being analyzed) or is no longer in use (e.g. a variable has exited scope or
        //    an expression or variable is dead).
        // It is assumed that a newly-added variable has not been added before and that a variable that is being
        //    removed was previously added
        /*virtual void addVar(varID var)=0;
        virtual void remVar(varID var)=0;*/
                        
        // The string that represents ths object
        // If indent!="", every line of this string must be prefixed by indent
        // The last character of the returned string should not be '\n', even if it is a multi-line string.
        //virtual string str(string indent="") /*const*/=0;
};

class FiniteLattice : public virtual Lattice
{
        public:
        //FiniteLattice() {}
        FiniteLattice(PartPtr p) : Lattice(p) {}
        
        bool finiteLattice()
        { return true;  }
};

class InfiniteLattice : public virtual Lattice
{
        public:
        //InfiniteLattice() {}
        InfiniteLattice(PartPtr p) : Lattice(p) {}
        
        bool finiteLattice()
        { return false; }
        
        // widens this from that and saves the result in this
        // returns true if this causes this to change and false otherwise
        virtual bool widenUpdate(InfiniteLattice* that)=0;
};

}; // namespace dataflow
#endif
