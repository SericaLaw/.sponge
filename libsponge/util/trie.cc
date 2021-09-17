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
    uint32_t route_prefix = entry.route_prefix;
    uint8_t prefix_length = entry.prefix_length;
    uint32_t prefix;
    if (prefix_length == 0) {
        _default = entry;
        return;
    } else {
        prefix = ((0xffffffff << (32 - prefix_length)) & route_prefix);
    }
    shared_ptr<TrieNode> cur = _root;
    uint32_t mask = (1 << 31);
    while (prefix_length > 0) {
        uint8_t child = (prefix & mask) > 0 ? 1 : 0;
        cur = cur->get_or_create_child(child);
        prefix &= (mask - 1);
        mask >>= 1;
        --prefix_length;
    }
    cur->entry() = entry;
}

std::optional<Entry> Trie::longest_prefix_match(uint32_t ip) {
    optional<Entry> longest = {};
    optional<shared_ptr<TrieNode>> cur = _root;
    uint32_t mask = (1 << 31);
    while (cur.has_value()) {
        uint8_t child = (ip & mask) > 0 ? 1 : 0;
        cur = cur.value()->get_child(child);
        if (cur.has_value() and cur.value()->entry().has_value()) {
            longest = cur.value()->entry();
        }
        ip &= (mask - 1);
        mask >>= 1;
    }
    if (not longest.has_value() and _default.has_value())
        return _default;

    return longest;
}
