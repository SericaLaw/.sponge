#ifndef SPONGE_LIBSPONGE_ROUTER_HH
#define SPONGE_LIBSPONGE_ROUTER_HH

#include "network_interface.hh"

#include <optional>
#include <queue>
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
    std::shared_ptr<TrieNode> get_or_create_child(uint8_t child) {
        if (_children.count(child) == 0) {
            _children[child] = std::make_shared<TrieNode>();
        }
        return _children[child];
    };
    std::optional<std::shared_ptr<TrieNode>> get_child(uint8_t child) {
        if (_children.count(child) == 0) {
            return {};
        }
        return _children[child];
    };
    std::optional<Entry>& entry() { return _entry; }
};

class Trie {
  private:
    std::shared_ptr<TrieNode> _root;
    std::optional<Entry> _default{};
  public:
    Trie() : _root(std::make_shared<TrieNode>()) {};
    void insert(Entry entry) {
        uint32_t route_prefix = entry.route_prefix;
        uint8_t prefix_length = entry.prefix_length;
        uint32_t prefix;
        if (prefix_length == 0) {
            _default = entry;
            return;
        } else {
            prefix = ((0xffffffff << (32 - prefix_length)) & route_prefix);
        }
        std::shared_ptr<TrieNode> cur = _root;
        uint32_t mask = (1 << 31);
        while (prefix_length > 0) {
            uint8_t child = (prefix & mask) > 0 ? 1 : 0;
            cur = cur->get_or_create_child(child);
            prefix &= (mask - 1);
            mask >>= 1;
            --prefix_length;
        }
        cur->entry() = std::move(entry);
    };

    std::optional<Entry> longest_prefix_match(uint32_t ip) {
        std::optional<Entry> longest = {};
        std::optional<std::shared_ptr<TrieNode>> cur = _root;
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
    };
};

//! \brief A wrapper for NetworkInterface that makes the host-side
//! interface asynchronous: instead of returning received datagrams
//! immediately (from the `recv_frame` method), it stores them for
//! later retrieval. Otherwise, behaves identically to the underlying
//! implementation of NetworkInterface.
class AsyncNetworkInterface : public NetworkInterface {
    std::queue<InternetDatagram> _datagrams_out{};

  public:
    using NetworkInterface::NetworkInterface;

    //! Construct from a NetworkInterface
    AsyncNetworkInterface(NetworkInterface &&interface) : NetworkInterface(interface) {}

    //! \brief Receives and Ethernet frame and responds appropriately.

    //! - If type is IPv4, pushes to the `datagrams_out` queue for later retrieval by the owner.
    //! - If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
    //! - If type is ARP reply, learn a mapping from the "target" fields.
    //!
    //! \param[in] frame the incoming Ethernet frame
    void recv_frame(const EthernetFrame &frame) {
        auto optional_dgram = NetworkInterface::recv_frame(frame);
        if (optional_dgram.has_value()) {
            _datagrams_out.push(std::move(optional_dgram.value()));
        }
    };

    //! Access queue of Internet datagrams that have been received
    std::queue<InternetDatagram> &datagrams_out() { return _datagrams_out; }
};

//! \brief A router that has multiple network interfaces and
//! performs longest-prefix-match routing between them.
class Router {
    //! The router's collection of network interfaces
    std::vector<AsyncNetworkInterface> _interfaces{};
    Trie _route_table{};

    //! Send a single datagram from the appropriate outbound interface to the next hop,
    //! as specified by the route with the longest prefix_length that matches the
    //! datagram's destination address.
    void route_one_datagram(InternetDatagram &dgram);

  public:
    //! Add an interface to the router
    //! \param[in] interface an already-constructed network interface
    //! \returns The index of the interface after it has been added to the router
    size_t add_interface(AsyncNetworkInterface &&interface) {
        _interfaces.push_back(std::move(interface));
        return _interfaces.size() - 1;
    }

    //! Access an interface by index
    AsyncNetworkInterface &interface(const size_t N) { return _interfaces.at(N); }

    //! Add a route (a forwarding rule)
    void add_route(const uint32_t route_prefix,
                   const uint8_t prefix_length,
                   const std::optional<Address> next_hop,
                   const size_t interface_num);

    //! Route packets between the interfaces
    void route();
};

#endif  // SPONGE_LIBSPONGE_ROUTER_HH
