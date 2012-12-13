/** Galois Distributed Directory -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2012, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @author Manoj Dhanapal <madhanap@cs.utexas.edu>
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */

#ifndef GALOIS_RUNTIME_DIRECTORY_H
#define GALOIS_RUNTIME_DIRECTORY_H

#include "Galois/Runtime/NodeRequest.h"
#include <unordered_map>
#include "Galois/Runtime/ll/TID.h"
#include "Galois/Runtime/MethodFlags.h"
#include <unistd.h>

#include "Galois/Runtime/ll/SimpleLock.h"
#include "Galois/Runtime/Network.h"

#define PLACEREQ 10000

namespace Galois {
namespace Runtime {

// the default value is false
extern bool distributed_foreach;

static inline void set_distributed_foreach(bool val) {
   distributed_foreach = val;
   return;
}

static inline bool get_distributed_foreach() {
   return distributed_foreach;
}

namespace DIR {

extern NodeRequest nr;

static inline int getTaskRank() {
   return nr.taskRank;
}

static inline int getNoTasks() {
   return nr.numTasks;
}

static inline void comm() {
   nr.Communicate();
   return;
}

   static void *resolve (void *ptr, int owner, size_t size) {
      Lockable *L;
      int count;
      static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   pthread_mutex_lock(&mutex);
      void *tmp = nr.remoteAccess(ptr,owner);
      if (!tmp) {
         // go get a copy of the object
         nr.PlaceRequest (owner, ptr, size);
         count = 0;
         do {
            if (count++ == PLACEREQ) {
               // reqs may be lost as remote tasks may ask for the same node
               count = 0;
               // may be the only running thread - req a call to comm
               nr.Communicate();
               // sleep so that the caller is not flooded with requests
               usleep(10000);
               nr.PlaceRequest (owner, ptr, size);
            }
            // another thread might have got the same data
            nr.checkRequest(ptr,owner,&tmp,size);
         } while(!tmp);
      }
   pthread_mutex_unlock(&mutex);
      // lock the object if locked for use by the directory (setLockValue)
      L = reinterpret_cast<Lockable*>(tmp);
 //   if (get_distributed_foreach() && (getNoTasks() != 1))
      if (get_distributed_foreach())
        lockAcquire(L,Galois::MethodFlag::ALL);
      else
        acquire(L,Galois::MethodFlag::ALL);
      return tmp;
   }

} // end of DIR namespace
}
} // end namespace


namespace Galois {
namespace Runtime {
namespace Distributed {

//entry point for remote fetch messages
template<typename T>
void serializeLandingPad(Galois::Runtime::Distributed::RecvBuffer &) {
  //  T* ptr = reinterpret_cast<T*>(ptr);
  //I have ptr;
  //acquire(ptr);
}

class RemoteDirectory {

  struct objstate {
    enum ObjStates { ObjValid, ObjIncoming };

    uintptr_t localobj;
    enum ObjStates state;
  };

  struct ohash : public unary_function<std::pair<uintptr_t, uint32_t>, size_t> {
    size_t operator()(const std::pair<uintptr_t, uint32_t>& v) const {
      return std::hash<uintptr_t>()(v.first) ^ std::hash<uint32_t>()(v.second);
    }
  };

  std::unordered_map<std::pair<uintptr_t, uint32_t>, objstate, ohash> curobj;
  Galois::Runtime::LL::SimpleLock<true> Lock;

  //returns a valid local pointer to the object if it exists
  //or returns null
  uintptr_t haveObject(uintptr_t ptr, uint32_t owner);

  //returns a valid local pointer to the object if it exists
  //or returns null
  uintptr_t fetchObject(uintptr_t ptr, uint32_t owner, recvFuncTy pad );

public:
  //resolve a pointer, owner pair
  //precondition: owner != networkHostID
  template<typename T>
  T* resolve(uintptr_t ptr, uint32_t owner) {
    assert(owner != networkHostID);
    uintptr_t p = haveObject(ptr, owner);
    if (!p)
      p = fetchObject(ptr, owner, &serializeLandingPad<T>);
    return reinterpret_cast<T*>(p);
  }
};

RemoteDirectory& getSystemRemoteDirectory();

class LocalDirectory {
};

LocalDirectory& getSystemLocalDirectory();

} //Distributed
} //Runtime
} //Galois
#endif
