
#ifndef _WF_SET_H_
#define _WF_SET_H_

#include <iostream>
#include <functional>
#include <set>
#include "ucs/CXMutationWF.hpp"

// This is a wrapper to std::set using CX
template<typename K> class WFStdSet {

private:
    // Create a CX which wraps a std::set<K> and initializes it with a new (empty) instance
    CXMutationWF<std::set<K>> cx {new std::set<K>()};

public:
    // Used only by our benchmarks
    WFStdSet(int maxthreads=0) { };

    static std::string className() { return "WFStdSet"; }

    bool add(K key, const int tid) {
        return cx.applyUpdate([=] (auto *set) {
            if (set->find(key) == set->end()) {
                set->insert(key);
                return true;
            }
            return false;
        }, tid);
    }

    bool remove(K key, const int tid) {
        return cx.applyUpdate([=] (std::set<K>* set) {
            auto iter = set->find(key);
            if (iter == set->end()) return false;
            set->erase(iter);
            return true;
        }, tid);
    }

    bool contains(K key, const int tid) {
        return cx.applyRead([=] (std::set<K>* set) {
            return (set->find(key) != set->end());
        }, tid);
    }

    // Used only by our benchmarks
    bool addAll(K** keys, const int size, const int tid) {
        for (int i = 0; i < size; i++) add(*keys[i], tid);
        return true;
    }
};

#endif /* _WF_STD_SET_H_ */
