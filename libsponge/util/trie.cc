#include "trie.hh"

#include <iostream>
using namespace std;
std::shared_ptr<TrieNode> TrieNode::get_or_create_child(uint8_t child) {
    if (_children.count(child) == 0) {
        _children[child] = make_shared<TrieNode>(TrieNode{});
    }
    return _children[child];
}
std::optional<std::shared_ptr<TrieNode>> TrieNode::get_child(uint8_t child) {
    if (_children.count(child) == 0) {
        return {};
    }
    return _children[child];
}

void Trie::insert(Entry entry) {
    uint32_t routePrefix = entry.routePrefix;
    uint8_t prefixLength = entry.prefixLength;
    uint32_t prefix = ((0xffffffff << (32 - prefixLength)) & routePrefix);
    shared_ptr<TrieNode> cur = _root;
    uint32_t mask = (1 << 31);
    while (prefix > 0) {
        uint8_t child = (prefix & mask) > 0 ? 1 : 0;
        cur = cur->get_or_create_child(child);
        prefix &= (mask - 1);
        mask >>= 1;
    }
    cur->entry() = entry;
}

std::optional<Entry> Trie::longest_prefix_match(uint32_t ip) {
    optional<Entry> longest = {};
    optional<shared_ptr<TrieNode>> cur = _root;
    uint32_t mask = (1 << 31);
    while (cur.has_value() and ip > 0) {
        uint8_t child = (ip & mask) > 0 ? 1 : 0;
        cur = cur.value()->get_child(child);
        if (cur.has_value() and cur.value()->entry().has_value()) {
            longest = cur.value()->entry();
        }
        ip &= (mask - 1);
        mask >>= 1;
    }
    return longest;
}
