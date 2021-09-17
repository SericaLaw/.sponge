#ifndef SPONGE_LIBSPONGE_TRIE_HH
#define SPONGE_LIBSPONGE_TRIE_HH

#include "address.hh"

#include <optional>
#include <unordered_map>
#include <memory>

struct Entry {
    uint32_t route_prefix{0};
    uint8_t prefix_length{0};
    std::optional<Address> next_hop{};
    size_t interface_num{0};
};

class TrieNode {
  private:
    std::unordered_map<uint8_t, std::shared_ptr<TrieNode>> _children{};
    std::optional<Entry> _entry{};

  public:
    std::shared_ptr<TrieNode> get_or_create_child(uint8_t child);
    std::optional<std::shared_ptr<TrieNode>> get_child(uint8_t child);
    std::optional<Entry>& entry() { return _entry; }
};

class Trie {
  private:
    std::shared_ptr<TrieNode> _root;
    std::optional<Entry> _default{};
  public:
    Trie(): _root(std::make_shared<TrieNode>(TrieNode{})){};
    void insert(Entry entry);
    std::optional<Entry> longest_prefix_match(uint32_t ip);
};


#endif  // SPONGE_LIBSPONGE_TRIE_HH
