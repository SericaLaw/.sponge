#ifndef SPONGE_LIBSPONGE_TCP_SEGMENT_BUILDER_HH
#define SPONGE_LIBSPONGE_TCP_SEGMENT_BUILDER_HH

#include <utility>

#include "wrapping_integers.hh"
#include "tcp_segment.hh"

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

    TCPSegment build_segment() const;

};

#endif  // SPONGE_LIBSPONGE_TCP_SEGMENT_BUILDER_HH
