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

    EthernetFrame sendFrame;
    sendFrame.payload() = dgram.serialize();
    sendFrame.header().type = EthernetHeader::TYPE_IPv4;
    sendFrame.header().src = _ethernet_address;

    auto it = ip2Ether.find(next_hop_ip);
    if (it != ip2Ether.end()) {
        sendFrame.header().dst = it->second;
        _frames_out.push(sendFrame);
    } else {
        waitList.push_back(make_pair(next_hop_ip, sendFrame));
        if (arpTime.find(next_hop_ip) != arpTime.end())
            return;
        arpTime[next_hop_ip] = 0;

        ARPMessage arpMsg;
        arpMsg.opcode = ARPMessage::OPCODE_REQUEST;
        arpMsg.sender_ethernet_address = _ethernet_address;
        arpMsg.sender_ip_address = _ip_address.ipv4_numeric();

        arpMsg.target_ip_address = next_hop_ip;

        EthernetFrame arpFrame;
        arpFrame.header().type = EthernetHeader::TYPE_ARP;
        arpFrame.header().src = _ethernet_address;
        arpFrame.header().dst = ETHERNET_BROADCAST;
        arpFrame.payload() = BufferList(arpMsg.serialize());

        _frames_out.push(arpFrame);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST)
        return nullopt;
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram interGram;
        if (interGram.parse(Buffer(frame.payload())) == ParseResult::NoError)
            return interGram;
    } else {
        ARPMessage arpMsg;
        if (arpMsg.parse(Buffer(frame.payload())) == ParseResult::NoError) {
            uint32_t senderIp = arpMsg.sender_ip_address;
            EthernetAddress senderEtherAddr = arpMsg.sender_ethernet_address;
            ip2Ether[senderIp] = senderEtherAddr;
            ip2duraTime[senderIp] = 0;

            for (auto it = waitList.begin(); it != waitList.end();) {
                if (it->first == senderIp) {
                    EthernetFrame &readyFrame = it->second;
                    readyFrame.header().dst = senderEtherAddr;
                    _frames_out.push(readyFrame);
                    it = waitList.erase(it);
                } else {
                    it++;
                }
            }

            if (arpMsg.opcode == ARPMessage::OPCODE_REQUEST && arpMsg.target_ip_address == _ip_address.ipv4_numeric()) {
                ARPMessage arpReply;
                arpReply.opcode = ARPMessage::OPCODE_REPLY;
                arpReply.sender_ethernet_address = _ethernet_address;
                arpReply.sender_ip_address = _ip_address.ipv4_numeric();
                arpReply.target_ethernet_address = senderEtherAddr;
                arpReply.target_ip_address = senderIp;

                EthernetFrame replyFrame;
                replyFrame.header().type = EthernetHeader::TYPE_ARP;
                replyFrame.header().src = _ethernet_address;
                replyFrame.header().dst = senderEtherAddr;
                replyFrame.payload() = BufferList(arpReply.serialize());

                _frames_out.push(replyFrame);
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto it = ip2duraTime.begin(); it != ip2duraTime.end();) {
        it->second += ms_since_last_tick;
        if (it->second > 30000) {
            uint32_t ipAddr = it->first;
            it = ip2duraTime.erase(it);
            ip2Ether.erase(ipAddr);
        } else {
            it++;
        }
    }

    for (auto it = arpTime.begin(); it != arpTime.end();) {
        it->second += ms_since_last_tick;
        if (it->second > 5000)
            it = arpTime.erase(it);
        else
            it++;
    }
}
