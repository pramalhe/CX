/******************************************************************************
 * Copyright (c) 2014-2017, Pedro Ramalhete, Andreia Correia
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

#ifndef _UNIVERSAL_CONSTRUCT_QUEUE_H_
#define _UNIVERSAL_CONSTRUCT_QUEUE_H_

#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <functional>

/**
 * <h1> Interface for Universal Constructs (Queues) </h1>
 *
 * UC is the Universal Construct
 * C is the queue class
 * CKey is the type of the item in the queue
 *
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */

// This class can be used to simplify the usage of queues with Universal Constructs
// For generic code you don't need it (and can't use it), but it can serve as an example of how to use lambdas.
// Q must be a queue where the items are of type QItem
template<typename UC, typename Q, typename QItem>
class UCQueue {
private:
    static const int MAX_THREADS = 128;
    const int maxThreads;
    UC uc{new Q(), maxThreads};

public:
    UCQueue(const int maxThreads=MAX_THREADS) : maxThreads{maxThreads} { }

    static std::string className() { return UC::className() + Q::className(); }

    bool enqueue(QItem* item, const int tid) {
        std::function<bool(Q*)> func = [item] (Q* q) { return q->enqueue(item); };
        return uc.applyUpdate(func, tid);
    }

    QItem* dequeue(const int tid) {
        std::function<bool(Q*)> func = [] (Q* q) { return q->dequeue(); };
        return (QItem*)uc.applyUpdate(func, tid);
    }
};

#endif /* _UNIVERSAL_CONSTRUCT_QUEUE_H_ */
