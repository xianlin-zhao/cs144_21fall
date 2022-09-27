#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    RouteEntry newEntry(route_prefix, prefix_length, next_hop, interface_num);
    routeTable.push_back(newEntry);
    // Your code here.
}

bool Router::matchPrefix(uint32_t dstIp, uint32_t prefix, uint8_t preLen) {
    if (preLen == 0)
        return true;
    return (dstIp >> (32 - preLen)) == (prefix >> (32 - preLen));
}

int Router::findInTable(uint32_t dstIp) {
    int ansId = -1;
    int sz = routeTable.size();
    uint32_t nowMaxLen = 0;

    for (int i = 0; i < sz; i++) {
        uint32_t prefix = routeTable[i].prefix;
        uint8_t preLen = routeTable[i].preLen;
        if (matchPrefix(dstIp, prefix, preLen)) {
            if (preLen >= nowMaxLen) {
                nowMaxLen = preLen;
                ansId = i;
            }
        }
    }
    return ansId;
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    uint32_t dstIp = dgram.header().dst;
    uint8_t ttl = dgram.header().ttl;
    if (ttl <= 1)
        return;
    int goalEntryId = findInTable(dstIp);
    if (goalEntryId < 0)
        return;
    dgram.header().ttl -= 1;
    if (routeTable[goalEntryId].nextHop.has_value())
        interface(routeTable[goalEntryId].interId).send_datagram(dgram, routeTable[goalEntryId].nextHop.value());
    else
        interface(routeTable[goalEntryId].interId).send_datagram(dgram, Address::from_ipv4_numeric(dstIp));
    // Your code here.
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
