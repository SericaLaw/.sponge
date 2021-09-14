#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto seqno = seg.header().seqno;
    auto syn = seg.header().syn;
    auto fin = seg.header().fin;
    const auto& payload = seg.payload();
    if (syn) {
        // SYN received
        _sender_isn = seqno;
    }
    if (_sender_isn.has_value() and not _reassembler.stream_out().input_ended()) {
        auto abs_seqno = unwrap(seqno, _sender_isn.value(), _reassembler.stream_out().bytes_written());
        if (abs_seqno == 0) {   // SYN
            if (!syn) return;   // an invalid segment
            abs_seqno = 1;
        }
        _reassembler.push_substring(payload.copy(), abs_seqno - 1, fin);
    } // otherwise, it's in LISTEN
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (not _sender_isn.has_value())
        return {};
    auto stream_index = _reassembler.stream_out().bytes_written();
    auto abs_seqno = stream_index + 1;
    if (_reassembler.stream_out().input_ended()) {
        ++abs_seqno;
    }
    return wrap(abs_seqno, _sender_isn.value());
}

size_t TCPReceiver::window_size() const {
    return _reassembler.stream_out().remaining_capacity();
}
