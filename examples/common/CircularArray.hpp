/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.

 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */

#ifndef _CIRCULARARRAY_H_
#define _CIRCULARARRAY_H_

#include <vector>
#include <iostream>

#include "../common/HazardErasCX.hpp"
#include "../common/HazardPointersCX.hpp"


/**
 * This is storing the pointers to the T instances, not the actual T instances.
 */
template<typename TNode, class HP = HazardPointersCX<TNode>>
class CircularArray {

private:
    static const int max_size = 2000;
    int min_size = 1000;
    TNode** preRetiredMutNodes;
    int begin = 0;
    int size = 0;
    HP& hp;
    int tid;

    void clean(TNode* node) {
        int pos = begin;
        int initialSize = size;
        TNode* lnext = nullptr;
        for(int i = 0;i<initialSize;i++){
            if(pos==max_size)pos=0;
            TNode* mNode = preRetiredMutNodes[pos];
            if(mNode->ticket.load() > node->ticket.load()-min_size) {
                begin = pos;
                return;
            }
            lnext = mNode->next.load();
            mNode->next.store(mNode, std::memory_order_release);
            hp.retire(lnext,tid);
            pos++;
            size--;
        }
    }


public:
    CircularArray(HP& hp, int tid):hp{hp},tid{tid} {
        preRetiredMutNodes = new TNode*[max_size];
        static_assert(std::is_same<decltype(TNode::ticket), std::atomic<uint64_t>>::value, "TNode::ticket must exist");
        static_assert(std::is_same<decltype(TNode::next), std::atomic<TNode*>>::value, "TNode::next must exist");
    }

    ~CircularArray() {
        int pos = begin;
        for(int i = 0;i<size;i++){
            if (pos == max_size) pos = 0;
            hp.retire(preRetiredMutNodes[pos]->next.load(),tid);
            pos++;
        }
        delete[] preRetiredMutNodes;
    }


    bool add(TNode* node) {
        if (size == max_size) clean(node);
        int pos = (begin+size)%max_size;
        preRetiredMutNodes[pos] = node;
        size++;
        return true;
    }
};

#endif /* _CIRCULARARRAY_H_ */
