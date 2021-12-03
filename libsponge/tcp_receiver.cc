#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto seqno = seg.header().seqno;
    auto syn = seg.header().syn;
    auto fin = seg.header().fin;
    const auto &payload = seg.payload();

    [[unlikely]] if (syn) {
        // SYN received
        _sender_isn = seqno;
    }

    if (in_syn_recv()) {
        auto abs_seqno = unwrap(seqno, _sender_isn.value(), _reassembler.stream_out().bytes_written());
        if (abs_seqno == 0) {
            // SYN segment may also contain data payload
            abs_seqno = 1;
            if (!syn) {
                return; // invalid stream with index 0
            }
        }

        size_t stream_index = abs_seqno - 1;
        _reassembler.push_substring(payload.copy(), stream_index, fin);
    } // otherwise, it's in LISTEN
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (not _sender_isn.has_value()) {
        return {};
    }

    auto stream_index = _reassembler.stream_out().bytes_written();
    // calculate absolute seqno (syn and eof should take space)
    auto abs_seqno = stream_index + 1;
    if (_reassembler.stream_out().input_ended()) {
        ++abs_seqno;
    }
    return wrap(abs_seqno, _sender_isn.value());
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
