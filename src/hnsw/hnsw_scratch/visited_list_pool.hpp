#pragma once

#include <mutex>
#include <string.h>
#include <deque>
#include <vector>
#include <limits>
#include <algorithm>

/*
What this does:

If running many parallel searches over a dataset - each search MUST track which elements it has already visited - so we create this
Visited List Pool - a temporary bitmap for visited flags.

VisitedList: Tracks whether an element was visited in the current query iteration. 
visitedAt[i] holds when an element was last visited.
currentVisited is incremented after each search.
If visitedAt[i] == currentVisited. Current node was visited in this search.


VisitedListPool - preallocate N VisitedList elements.
Functions as an allocator and releaser of visited lists for memory reuse/efficiency
Concurrency is managed by dequeue and mutexes. 
*/

#pragma once

#include <mutex>
#include <string.h>
#include <deque>

typedef unsigned short int vl_type;


class VisitedList {
public:
    vl_type currentVisited; // curV
    vl_type* visitedAt;// mass
    unsigned int numElements; 

    VisitedList(int numElements_)
        : currentVisited(-1), numElements(numElements_) {
        visitedAt = new vl_type[numElements];
    }

    void reset() {
        currentVisited++;
        if (currentVisited == 0) {
            memset(visitedAt, 0, sizeof(vl_type) * numElements);
            currentVisited++;
        }
    }

    ~VisitedList() {
        delete[] visitedAt;
    }
};

///////////////////////////////////////////////////////////
//
// Class for multi-threaded pool-management of VisitTrackers
//
/////////////////////////////////////////////////////////

class VisitedListPool {
    std::deque<VisitedList *> pool;
    std::mutex poolguard;
    int numelements;

 public:
    VisitedListPool(int initmaxpools, int numelements1) {
        numelements = numelements1;
        for (int i = 0; i < initmaxpools; i++)
            pool.push_front(new VisitedList(numelements));
    }

    VisitedList *getFreeVisitedList() {
        VisitedList *rez;
        {
            std::unique_lock <std::mutex> lock(poolguard);
            if (pool.size() > 0) {
                rez = pool.front();
                pool.pop_front();
            } else {
                rez = new VisitedList(numelements);
            }
        }
        rez->reset();
        return rez;
    }

    void releaseVisitedList(VisitedList *vl) {
        std::unique_lock <std::mutex> lock(poolguard);
        pool.push_front(vl);
    }

    ~VisitedListPool() {
        while (pool.size()) {
            VisitedList *rez = pool.front();
            pool.pop_front();
            delete rez;
        }
    }
};
