#include <ucs/CXMutationWF>
#include <set>

int main(void) {
    // Create a CX which wraps a std::set<K> and initializes it with a new (empty) instance
    CXMutationWF<std::set<int>> cx {new std::set<int>()};

    int key = 33;

    // Insert a key into the set (wait-free progress)
    cx.applyUpdate([=] (std::set<int>* set) { set->insert(key); return true; }, tid);

    // Lookup key 33 in the set (wait-free progress)
    bool foundit = cx.applyRead([=] (std::set<int>* set) {
        return (set->find(key) != set->end());
    }, tid);

    // Will never print out "error"
    if (foundit) {
        std::cout << "Found the key\n";
    } else {
        std::cout << "error\n";
    }

    return 0;
}
