//
// Created by serica on 9/12/21.
//

#include "tcp_segment_builder.hh"

TCPSegment TCPSegmentBuilder::build_segment() const {
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