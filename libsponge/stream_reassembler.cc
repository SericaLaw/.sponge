#include <iostream>

#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`


using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (_output.input_ended()) {
        return;
    }

    auto expected = _output.bytes_written();
    if (index + data.size() < expected) {
        return;
    }

    if (eof and not _got_eof) {
        _got_eof = true;
        _end_index = index + data.size();
    }

    auto len = data.size();

    // trim the overflow data
    if (index + data.size() > expected + _capacity) {
        len = expected + _capacity - index;
    }

    Segment segment{index, string_view{data.c_str(), len}};
    if (segment.start_index() < expected) {
        segment.remove_prefix(expected - segment.start_index());
    }

    push_interval(segment);

    // reassemble
    while (not _segments.empty()) {
        auto it = _segments.begin();
        // index start from 0
        if (it->start_index() == _output.bytes_written()) {
            size_t wc = _output.write(string{it->str()});
            if (wc == 0) {
                break;
            }
            _unassembled_bytes -= wc;
            if (wc < it->size()) {
                Segment trimmed = *it;
                trimmed.remove_prefix(wc);
                _segments.erase(_segments.begin());
                _segments.insert(trimmed);
                break;
            }

            _segments.erase(_segments.begin());
        } else {
            break;
        }
    }

    if (_got_eof and _end_index == _output.bytes_written()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }

void StreamReassembler::push_interval(Segment &segment) {
    if (segment.size() == 0) {
        return;
    }

    // ensure no overlap with prev
    auto it = lower_bound(_segments.begin(), _segments.end(), segment.start_index());
    auto remove_begin = it;
    if (it != _segments.end()) {
        assert(it->end_index() >= segment.start_index());
        if (it->start_index() <= segment.start_index()) {
            size_t remove_n = it->end_index() - segment.start_index();
            if (remove_n > segment.size()) {
                return;
            }
            segment.remove_prefix(remove_n);
            ++remove_begin;
        }
    }

    // ensure no overlap with behind
    it = lower_bound(_segments.begin(), _segments.end(), segment.end_index());
    auto remove_end = it;
    if (it != _segments.end()) {
        assert(it->end_index() >= segment.end_index());
        if (it->start_index() <= segment.end_index()) {
            size_t remove_n = segment.end_index() - it->start_index();
            if (remove_n > segment.size()) {
                return;
            }
            segment.remove_suffix(remove_n);
        }
    }

    // remove overlapping segments
    for (it = remove_begin; it != remove_end; it++) {
        _unassembled_bytes -= it->size();
    }
    _segments.erase(remove_begin, remove_end);
    _segments.insert(segment);
    _unassembled_bytes += segment.size();
}