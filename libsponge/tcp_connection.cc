  
#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;
    if (seg.header().rst) {
        if (_receiver.in_listen() and _sender.in_closed())
            return;
        if (_sender.in_syn_sent() and not seg.header().ack)
            return;
        // set both inbound and outbound streams to error state
        _reset(false);
        return;
    }
    if (_receiver.in_listen() and not seg.header().syn) return;
    if (_sender.in_syn_sent() and seg.header().ack and seg.payload().size() > 0) return;

    // give the segment to receiver
    _receiver.segment_received(seg);
    if (seg.header().ack and _sender.next_seqno_absolute() > 0) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
    }

    // CLOSED -> SYN_SENT
    if (seg.header().syn and _sender.next_seqno_absolute() == 0) { // not syned
        // in listening state, a remote peer connected, need to ack with syn
        _sender.fill_window();
    } else if (seg.length_in_sequence_space() > 0 and _sender.next_seqno_absolute() > 0) {
        // should syn first
        _sender.send_empty_segment();
    }

    _send_outbound_segments();
    if (_receiver.stream_out().eof() and not _sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    _check_done();
}

bool TCPConnection::active() const {
    if (not _done()) {
        return true;
    }
    if (not _linger_after_streams_finish) {
        return false;
    } else {
        if (_time_done.has_value())
            return _time_since_last_segment_received - _time_done.value() < 10 * _cfg.rt_timeout;
        return false;
    }
}

size_t TCPConnection::write(const string &data) {
    auto wc = _sender.stream_in().write(data);
    _sender.fill_window();
    _send_outbound_segments();
    return wc;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _reset(true);
    }
    _send_outbound_segments();

    _check_done();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _send_outbound_segments();

    _check_done();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _send_outbound_segments();

    _check_done();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _reset(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_send_outbound_segments() {
    while (not _sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        auto receiver_ackno = _receiver.ackno();
        // set ack, ackno and window_size
        if (receiver_ackno.has_value()) {
            seg.header().ack = true;
            seg.header().ackno = receiver_ackno.value();
        }
        if (_receiver.window_size() > numeric_limits<char16_t>::max()) {
            seg.header().win = numeric_limits<char16_t>::max();
        } else {
            seg.header().win = _receiver.window_size();
        }
        if (_rst_set) {
            seg.header().rst = true;
        }
        _segments_out.push(std::move(seg));
    }
}

bool TCPConnection::_done() const {
    if (not (_receiver.stream_out().eof() or _receiver.stream_out().error())) {
        return false;
    }
    if (not _sender.stream_in().error() and not (_sender.stream_in().eof() and _sender.bytes_in_flight() == 0 and _sender.fined())) {
        return false;
    }
    return true;
}

void TCPConnection::_check_done() {
    if (_done() and not _time_done.has_value()) {
        _time_done = _time_since_last_segment_received;
    }
}
void TCPConnection::_reset(bool send_rst) {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _sender.stream_in().end_input();
    while (not _sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    _linger_after_streams_finish = false;
    if (send_rst) {
        _rst_set = true;
        _sender.send_empty_segment();
        _send_outbound_segments();
    }
}