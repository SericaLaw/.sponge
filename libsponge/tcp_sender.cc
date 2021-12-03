#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timeout{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t bytes = 0;
    for (auto& seg : _segments_pending) {
        bytes += seg.length_in_sequence_space();
    }
    return bytes;
}

void TCPSender::fill_window() {
    TCPSegmentBuilder builder;
    if (in_closed()) {   // no syn sent
        // start 3-way handshake
        builder.with_seqno(next_seqno()).with_syn();
        _send(builder);
        return;
    }

    size_t receiver_window_remaining = _receiver_window_right - next_seqno_absolute();
    size_t payload_len_limit = min(receiver_window_remaining, TCPConfig::MAX_PAYLOAD_SIZE);

    // fill receiver's window as much as possible, may send out multiple segments
    while (payload_len_limit > 0 and not _fined) {
        string payload = _stream.read(payload_len_limit);
        builder.with_seqno(next_seqno()).with_data(payload);

        // _stream buffer empty
        if (payload.empty() and not _stream.eof()) {
            break;
        }

        if (not _stream.eof() and not payload.empty()) {
            // no need to set fin
            _send(builder);
        } else if (_stream.eof()) {
            // if length is limited by the receiver window, don't send fin
            // else, receiver has enough room for fin, but length is limited by MAX_PAYLOAD_SIZE, send fin
            if (payload.size() < payload_len_limit) {
                builder.with_fin(); // notify fin while carry payload; payload can be empty
                _fined = true;
            } else if (receiver_window_remaining > payload_len_limit) {
                builder.with_fin(); // fin flag won't take payload's space, though take 1 for abs_seqno
                _fined = true;
            }
            _send(builder);
        }

        // update remaining receiver window
        receiver_window_remaining = _receiver_window_right - next_seqno_absolute();
        payload_len_limit = min(receiver_window_remaining, TCPConfig::MAX_PAYLOAD_SIZE);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto absolute_ackno = unwrap(ackno, _isn, _receiver_window_left);
    if (window_size == 0) {
        zero_window_size = true;
    } else {
        zero_window_size = false;
    }
    if (absolute_ackno > _receiver_window_right) {
        return;
    }

    if (absolute_ackno > _receiver_window_left) {
        // update receiver window
        _receiver_window_size = zero_window_size ? 1 : window_size;
        _receiver_window_left = absolute_ackno;
        _receiver_window_right = absolute_ackno + _receiver_window_size;

        // reset timeout
        _retransmission_timeout = _initial_retransmission_timeout;
        _consecutive_retransmissions = 0;

        // receiver has received all the segments on the left of _receiver_window_left
        while (not _segments_pending.empty()) {
            TCPSegment &seg = _segments_pending.front();
            auto absolute_seqno = unwrap(seg.header().seqno, _isn, _receiver_window_left);
            if (absolute_seqno + seg.length_in_sequence_space() <= _receiver_window_left) {
                _segments_pending.pop_front();
            } else {
                break;
            }
        }

        // reset timer
        if (_segments_pending.empty()) {
            _timer.stop();
        } else {
            _timer.start(_retransmission_timeout);
        }

        // now receiver (may) have more room to receive, fill the window
        fill_window();
    } else if (absolute_ackno == _receiver_window_left) {
        // processed this ackno before, just update window size
        _receiver_window_size = zero_window_size ? 1 : window_size;
        _receiver_window_left = absolute_ackno;
        _receiver_window_right = absolute_ackno + _receiver_window_size;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);
    if (not _segments_pending.empty() and _timer.timeout()) {
        // timeout, retrans first pending segment
        _segments_out.push(_segments_pending.front());
        ++_consecutive_retransmissions;
        if (not zero_window_size) {
            _retransmission_timeout *= 2;
        }
        _timer.start(_retransmission_timeout);
    }
    if (_segments_pending.empty()) {
        _timer.stop();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions;; }

void TCPSender::send_empty_segment() {
    TCPSegmentBuilder builder;
    builder.with_seqno(next_seqno());
    _send(builder);
}

void TCPSender::_send(TCPSegmentBuilder &builder) {
    TCPSegment seg = builder.build_segment();
    _segments_out.push(seg);
    // don't re-trans empty ACKs?
    if (seg.length_in_sequence_space() > 0) {
        _segments_pending.push_back(seg);
        // Every time a segment containing data (nonzero length in sequence space) is sent
        // (whether it’s the first time or a retransmission), if the timer is not running, start it
        if (not _timer.running()) {
            _timer.start(_retransmission_timeout);
        }
    }
    _next_seqno += seg.length_in_sequence_space();
}