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
currentMarker is incremented after each search.
If visitedAt[i] == currentMarker. Current node was visited in this search.


VisitedListPool - preallocate N VisitedList elements.
Functions as an allocator and releaser of visited lists for memory reuse/efficiency
Concurrency is managed by dequeue and mutexes. 
*/

class VisitedList {
public:
    unsigned short int currentMarker_;
    std::vector<unsigned short int> visitedAt_;

    explicit VisitedList(int numberElements)
        : currentMarker_(std::numeric_limits<unsigned short int>::max()),
          visitedAt_(numberElements, 0) {}

    void reset() {
        currentMarker_++;
        if (currentMarker_ == 0) {
            std::fill(visitedAt_.begin(), visitedAt_.end(), 0);
            currentMarker_++;
        }
    }
};

///////////////////////////////////////////////////////////
//
// Class for multi-threaded pool-management of VisitedLists
//
/////////////////////////////////////////////////////////

class VisitedListPool {
    std::deque<VisitedList *> pool_;
    std::mutex poolguard_;
    int numberElements_;

 public:
    VisitedListPool(int maxPool, int numberElements) {
        numberElements_ = numberElements;
        for (int i = 0; i < maxPool; i++)
            pool_.push_front(new VisitedList(numberElements_));
    }

    VisitedList *getFreeVisitedList() {
        VisitedList *visitedList;
        {
            std::unique_lock <std::mutex> lock(poolguard_);
            if (pool_.size() > 0) {
                visitedList = pool_.front();
                pool_.pop_front();
            } else {
                visitedList = new VisitedList(numberElements_);
            }
        }
        visitedList->reset();
        return visitedList;
    }

    void releaseVisitedList(VisitedList *visitedList) {
        std::unique_lock <std::mutex> lock(poolguard_);
        pool_.push_front(visitedList);
    }

    ~VisitedListPool() {
        while (pool_.size()) {
            VisitedList *VisitedList = pool_.front();
            pool_.pop_front();
            delete VisitedList;
        }
    }
};
