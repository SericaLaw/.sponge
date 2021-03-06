#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>
#include <list>

//! \brief The "sender" part of a TCP implementation.
class RetransmissionTimer {
  private:
    size_t _timeout{0};
    size_t _time_elapsed{0};
    bool _running{false};
  public:
    void start(size_t timeout) {
        reset(timeout);
        _running = true;
    }
    bool timeout() const {
        return _running and _time_elapsed >= _timeout;
    }
    bool running() const { return _running; }
    void tick(size_t ms_since_last_tick) {
        if (_running)
            _time_elapsed += ms_since_last_tick;
    }
    void reset(size_t timeout) {
        _timeout = timeout;
        _time_elapsed = 0;
        _running = false;
    }
    void stop() {
        _running = false;
    }
};

class TCPSegmentBuilder {
  private:
    bool ack{false};
    bool rst{false};
    bool syn{false};
    bool fin{false};
    WrappingInt32 seqno{0};
    WrappingInt32 ackno{0};
    std::string data{};

  public:
    TCPSegmentBuilder &with_ack(WrappingInt32 ackno_) {
        ack = true;
        ackno = ackno_;
        return *this;
    }

    TCPSegmentBuilder &with_ack(uint32_t ackno_) { return with_ack(WrappingInt32{ackno_}); }

    TCPSegmentBuilder &with_rst() {
        rst = true;
        return *this;
    }

    TCPSegmentBuilder &with_syn() {
        syn = true;
        return *this;
    }

    TCPSegmentBuilder &with_fin() {
        fin = true;
        return *this;
    }

    TCPSegmentBuilder &with_seqno(WrappingInt32 seqno_) {
        seqno = seqno_;
        return *this;
    }

    TCPSegmentBuilder &with_seqno(uint32_t seqno_) { return with_seqno(WrappingInt32{seqno_}); }

    TCPSegmentBuilder &with_data(std::string data_) {
        data = std::move(data_);
        return *this;
    }

    TCPSegment build_segment() const {
        TCPSegment seg;
        seg.payload() = std::string(data);
        seg.header().ack = ack;
        seg.header().fin = fin;
        seg.header().syn = syn;
        seg.header().rst = rst;
        seg.header().ackno = ackno;
        seg.header().seqno = seqno;
        return seg;
    }
};

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    bool zero_window_size{false};
    uint16_t _receiver_window_size{1};
    uint64_t _receiver_window_left{0};  // == last ackno from remote receiver
    uint64_t _receiver_window_right{1}; // == last ackno + window_size of remote receiver
    unsigned int _retransmission_timeout;

    std::list<TCPSegment> _segments_pending{};
    bool _syned{false};
    bool _fined{false};

    RetransmissionTimer _timer{};
    unsigned int _consecutive_retransmissions{0};

    // only use this method when sending a segment at its first time
    void _send(TCPSegmentBuilder& builder);
  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}

    bool fined() const { return _fined; }
    bool in_closed() const { return next_seqno_absolute() == 0; }
    bool in_syn_sent() const {
        return next_seqno_absolute() > 0
               and next_seqno_absolute() == bytes_in_flight();
    }
    bool in_syn_acked() const {
        return (next_seqno_absolute() > bytes_in_flight() and not stream_in().eof())
               or (stream_in().eof() and next_seqno_absolute() < stream_in().bytes_written() + 2);
    }
    bool in_fin_sent() const {
        return stream_in().eof()
               and next_seqno_absolute() == (stream_in().bytes_written() + 2)
               and bytes_in_flight() > 0;
    }
    bool in_fin_acked() const {
        return stream_in().eof()
               and next_seqno_absolute() == (stream_in().bytes_written() + 2)
               and bytes_in_flight() == 0;
    }
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
