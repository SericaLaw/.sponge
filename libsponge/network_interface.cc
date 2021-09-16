#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame frame;
    frame.header().src = _ethernet_address;
    if (_arp_table.count(next_hop_ip) > 0) {
        auto [hw_address, ttl] = _arp_table[next_hop_ip];
        if (ttl > 0) {
            frame.header().dst = hw_address;
            frame.header().type = EthernetHeader::TYPE_IPv4;
            frame.payload() = dgram.serialize();
            _frames_out.push(frame);
            return;
        }
    }

    // queue the IP datagram and send ARP
    _dgram_pending.emplace(dgram, next_hop);

    if (_last_broadcast_time.count(next_hop_ip) > 0
        and _clock - _last_broadcast_time[next_hop_ip] < _broadcast_interval_ms) {
        return;
    }
    _last_broadcast_time[next_hop_ip] = _clock;
    frame.header().dst = ETHERNET_BROADCAST;
    frame.header().type = EthernetHeader::TYPE_ARP;
    ARPMessage arp_req;
    arp_req.opcode = ARPMessage::OPCODE_REQUEST;
    arp_req.sender_ethernet_address = _ethernet_address;
    arp_req.sender_ip_address = _ip_address.ipv4_numeric();
    arp_req.target_ip_address = next_hop_ip;
    frame.payload() = arp_req.serialize();
    _frames_out.push(frame);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    EthernetAddress dst = frame.header().dst;
    if (dst != _ethernet_address and dst != ETHERNET_BROADCAST) {
        return {};
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        ParseResult res = dgram.parse(frame.payload());
        if (res == ParseResult::NoError) {
            return dgram;
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        cerr << "ARP MSG received\n";
        ARPMessage arp_msg;
        ParseResult res = arp_msg.parse(frame.payload());
        if (res == ParseResult::NoError) {

            // learn arp mapping
            uint32_t sender_ip_address = arp_msg.sender_ip_address;
            EthernetAddress sender_ethernet_address = arp_msg.sender_ethernet_address;
            _arp_table[sender_ip_address] = make_pair(sender_ethernet_address, _cache_ttl_ms);

            uint32_t target_ip_address = arp_msg.target_ip_address;

            if (target_ip_address == _ip_address.ipv4_numeric() and arp_msg.opcode == ARPMessage::OPCODE_REQUEST) {
                EthernetFrame arp_reply_frame;
                arp_reply_frame.header().src = _ethernet_address;
                arp_reply_frame.header().dst = sender_ethernet_address;
                arp_reply_frame.header().type = EthernetHeader::TYPE_ARP;
                ARPMessage arp_reply;
                arp_msg.opcode = ARPMessage::OPCODE_REPLY;
                arp_msg.sender_ethernet_address = _ethernet_address;
                arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
                arp_msg.target_ethernet_address = sender_ethernet_address;
                arp_msg.target_ip_address = sender_ip_address;
                arp_reply_frame.payload() = arp_msg.serialize();
                _frames_out.push(arp_reply_frame);
            }
        }

        // polling queued dgrams
        while (not _dgram_pending.empty()) {
            auto size = _dgram_pending.size();
            auto& [dgram, next_hop] = _dgram_pending.front();
            send_datagram(dgram, next_hop);
            if (size < _dgram_pending.size()) break;
            _dgram_pending.pop();
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _clock += ms_since_last_tick;
    // refresh
    for (auto it = _arp_table.begin(); it != _arp_table.end();) {
        auto [hw_address, ttl] = it->second;
        if (ttl <= ms_since_last_tick) {
            _arp_table.erase(it++);
        } else {
            it->second.second -= ms_since_last_tick;
            ++it;
        }
    }
}
