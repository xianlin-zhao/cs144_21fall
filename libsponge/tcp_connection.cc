#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return timeSinceRecv; }

void TCPConnection::uncleanShut() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    actFlag = false;
}

void TCPConnection::sendRstAndAbort() {
    _sender.send_empty_segment(true);
    sendQueueSeg();
    uncleanShut();
}

void TCPConnection::sendQueueSeg() {
    while (!_sender.segments_out().empty()) {
        TCPSegment &nowSeg = _sender.segments_out().front();
        uint16_t maxWin = numeric_limits<uint16_t>::max();
        if (_receiver.window_size() > maxWin)
            nowSeg.header().win = maxWin;
        else
            nowSeg.header().win = _receiver.window_size();

        if (_receiver.ackno().has_value()) {
            nowSeg.header().ack = true;
            nowSeg.header().ackno = _receiver.ackno().value();
        }

        _segments_out.push(nowSeg);
        _sender.segments_out().pop();
    }
}

bool TCPConnection::shouldShut() {
    if (!(unassembled_bytes() == 0 && _receiver.stream_out().input_ended()))
        return false;
    if (!(_sender.stream_in().input_ended() &&
          _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2))
        return false;
    if (!(bytes_in_flight() == 0))
        return false;
    if (!_linger_after_streams_finish)
        return true;
    return timeSinceRecv >= 10 * _cfg.rt_timeout;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    timeSinceRecv = 0;
    if (seg.header().rst) {
        uncleanShut();
        return;
    }

    _receiver.segment_received(seg);
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        if (_sender.segments_out().empty())
            _sender.send_empty_segment();
    }

    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0 &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }

    sendQueueSeg();
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof())
        _linger_after_streams_finish = false;
}

bool TCPConnection::active() const { return actFlag; }

size_t TCPConnection::write(const string &data) {
    size_t realLen = _sender.stream_in().write(data);
    _sender.fill_window();
    sendQueueSeg();
    return realLen;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    timeSinceRecv += ms_since_last_tick;

    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        sendRstAndAbort();
        return;
    }
    _sender.tick(ms_since_last_tick);

    if (shouldShut())
        actFlag = false;
    else
        sendQueueSeg();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    sendQueueSeg();
}

void TCPConnection::connect() {
    _sender.fill_window();
    sendQueueSeg();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            sendRstAndAbort();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
