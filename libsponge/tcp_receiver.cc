#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    WrappingInt32 seq = seg.header().seqno;
    string data = seg.payload().copy();
    if (!syn && !seg.header().syn)
        return;

    bool eof = false;
    if (seg.header().fin)
        fin = true, eof = true;
    if (seg.header().syn && !syn)
        syn = true, isn = seq;
    if (!data.empty()) {
        if (seg.header().syn || seq != isn) {
            size_t idx = unwrap(seq - (!(seg.header().syn)), isn, _reassembler.getExpectId());
            _reassembler.push_substring(data, idx, eof);
        }
    }
    if (fin && _reassembler.empty())
        _reassembler.stream_out().end_input();
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!syn)
        return nullopt;
    size_t ack = _reassembler.getExpectId();
    size_t bias = (fin && _reassembler.empty()) ? 1 : 0;
    return wrap(ack + 1 + bias, isn);  // "+1" because "SYN" occupies a No, "+bias" because finish reading "FIN"
}

size_t TCPReceiver::window_size() const { return _reassembler.getRemainCap(); }
