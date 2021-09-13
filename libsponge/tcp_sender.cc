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
    , _retransmission_timeout{retx_timeout}{}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t bytes = 0;
    for (auto& tuple : _segments_pending) {
        bytes += get<0>(tuple).length_in_sequence_space();
    }
    return bytes;
}

void TCPSender::fill_window() {
    TCPSegmentBuilder builder;
    if (!_syned) {
        builder.with_seqno(next_seqno()).with_syn();
        _send(builder);
        _syned = true;
        return;
    }
    size_t receiver_window_remaining = _receiver_window_right - next_seqno_absolute();
    size_t payload_len_limit = min(receiver_window_remaining, TCPConfig::MAX_PAYLOAD_SIZE);
    while (payload_len_limit > 0 && !_fined) {
        // this will move _stream's head
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
                builder.with_fin();
                _fined = true;   // notify fin while carry payload; payload can be empty
            } else if (receiver_window_remaining > payload_len_limit) {
                builder.with_fin();
                _fined = true;
            }
            _send(builder);
        }
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
    if (absolute_ackno > _receiver_window_left) {
        _receiver_window_size = zero_window_size ? 1 : window_size;
        _receiver_window_left = absolute_ackno;
        _receiver_window_right = absolute_ackno + _receiver_window_size;
        _retransmission_timeout = _initial_retransmission_timeout;
        while (not _segments_pending.empty()) {
            TCPSegment &seg = get<0>(_segments_pending.front());
            auto absolute_seqno = unwrap(seg.header().seqno, _isn, _receiver_window_left);
            if (absolute_seqno + seg.length_in_sequence_space() <= _receiver_window_left) {
                _segments_pending.pop_front();
            } else {
                break;
            }
        }
    } else if (absolute_ackno == _receiver_window_left) {
        // processed this ackno before
        _receiver_window_size = zero_window_size ? 1 : window_size;
        _receiver_window_left = absolute_ackno;
        _receiver_window_right = absolute_ackno + _receiver_window_size;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (not _segments_pending.empty()) {
        if (get<1>(_segments_pending.front()) < ms_since_last_tick) {
            get<1>(_segments_pending.front()) = 0;
        } else {
            get<1>(_segments_pending.front()) -= ms_since_last_tick;
        }
    }
    if (not _segments_pending.empty()) {
        auto [seg, time_left, retransmission_count] = _segments_pending.front();
        if (time_left == 0) {
            _segments_out.push(seg);
            if (not zero_window_size)
                _retransmission_timeout *= 2;
            get<1>(_segments_pending.front()) = _retransmission_timeout;
            get<2>(_segments_pending.front())++;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    if (not _segments_pending.empty()) {
        return get<2>(_segments_pending.front());
    }
    return 0;
}

void TCPSender::send_empty_segment() {
    TCPSegmentBuilder builder;
    builder.with_seqno(next_seqno());
    _send(builder);
}

void TCPSender::_send(TCPSegmentBuilder &builder) {
    TCPSegment seg = builder.build_segment();
    _segments_out.push(seg);
    _segments_pending.emplace_back(seg, _retransmission_timeout, 0);
    _next_seqno += seg.length_in_sequence_space();
}
