#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , RTO(retx_timeout)
    , elapsed(0) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t ans = 0;
    for (auto &it : outSegs)
        ans += it.length_in_sequence_space();
    return ans;
}

void TCPSender::fill_window() {
    uint16_t realWindow = (recvWindow != 0) ? recvWindow : 1;
    uint64_t flightSize = bytes_in_flight();
    if (flightSize >= realWindow)
        return;
    realWindow -= flightSize;

    while (realWindow > 0) {
        uint64_t buildSegLen = 0;
        TCPSegment segment;
        TCPHeader &header = segment.header();
        if (!synSent) {
            header.syn = true;
            synSent = true;
            buildSegLen += 1;
        }

        size_t readLen = (realWindow - buildSegLen < TCPConfig::MAX_PAYLOAD_SIZE) ? realWindow - buildSegLen
                                                                                  : TCPConfig::MAX_PAYLOAD_SIZE;
        string segData = _stream.read(readLen);
        readLen = segData.size();
        if (readLen > 0)
            segment.payload() = Buffer(move(segData));
        buildSegLen += readLen;

        if (_stream.eof() && !finSent && buildSegLen < realWindow) {
            header.fin = true;
            finSent = true;
            buildSegLen += 1;
        }

        if (buildSegLen != 0) {
            header.seqno = wrap(_next_seqno, _isn);
            _segments_out.push(segment);
            outSegs.push_back(segment);
            realWindow -= buildSegLen;
            _next_seqno += buildSegLen;

            if (!timeFlag) {
                timeFlag = true;
                elapsed = 0;
            }
        } else
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    recvWindow = window_size;
    recvAckNo = unwrap(ackno, _isn, _next_seqno);
    if (recvAckNo > _next_seqno)
        return;

    while (!outSegs.empty()) {
        auto segFront = outSegs.begin();
        uint64_t frontSeq = unwrap((*segFront).header().seqno, _isn, _next_seqno);
        if (frontSeq + (*segFront).length_in_sequence_space() <= recvAckNo) {
            outSegs.erase(outSegs.begin());
            RTO = _initial_retransmission_timeout;
            timeFlag = true;
            elapsed = 0;
            consecutNum = 0;
        } else
            break;
    }

    if (outSegs.empty()) {
        timeFlag = false;
        elapsed = 0;
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (timeFlag) {
        elapsed += ms_since_last_tick;
        if (elapsed >= RTO) {
            if (outSegs.size() > 0)
                _segments_out.push(outSegs.front());
            if (recvWindow > 0) {
                consecutNum++;
                RTO *= 2;
            }
            elapsed = 0;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return consecutNum; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(segment);
}
