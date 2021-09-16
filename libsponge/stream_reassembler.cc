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
    if (eof) {
        got_eof = true;
        eof_tail_index = index + data.size();
    }
    size_t wc = write_slice(data, index, _output);
    // update interval, in the form of [left, right], rather than [left, right) (may be modified later)
    pair<size_t, size_t> interval = make_pair(index, index + wc - 1);

    push_interval(interval);
    // reassemble
    while (!intervals.empty()) {
        auto [st, ed] = intervals.front();
        if (_output.bytes_written() >= st) {
            set_tail(ed + 1, _output);
            intervals.pop_front();
        } else {
            break;
        }
    }
    if (got_eof and _output.bytes_written() == eof_tail_index) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t sum = 0;
    for (auto it = intervals.begin(); it != intervals.end(); it++) {
        auto [st, ed] = *it;
        sum += ed - st + 1;
    }
    return sum;
}

bool StreamReassembler::empty() const {
    return unassembled_bytes() == 0;
}

size_t StreamReassembler::write_slice(const string &data, const size_t index, ByteStream &bs) {
    size_t tail_index = bs.bytes_written();
    bool overlap = index <= tail_index;
    size_t i = 0, n = data.size();

    if (overlap) {
        // has overlap
        i = tail_index - index;
    }
    size_t temp_tail = bs.tail;
    if (!overlap)
        temp_tail = bs.advance(bs.tail, index - tail_index);
    // out of window
    if (index > tail_index + bs.remaining_capacity()) return 0;
    while ((temp_tail + 1) % (bs.cap + 1) != bs.head && i < n) {
        bs.buf[temp_tail] = data[i];
        temp_tail = bs.advance(temp_tail, 1);
        ++i;
    }
    if (overlap) {
        set_tail(index + i, bs);
    }
    return i;
}

size_t StreamReassembler::set_tail(const size_t new_tail_index, ByteStream &bs) {
    // note that "tail_index" is the "last_written_index" + 1, i.e. bytes_written
    // the written bytes reside in [head, tail)
    size_t cur_tail_index = bs.bytes_written();
    while (cur_tail_index < new_tail_index) {
        bs.tail = bs.advance(bs.tail,1);
        ++cur_tail_index;
        ++bs._bytes_written;
    }
    return 0;
}

void StreamReassembler::push_interval(pair<size_t, size_t> &interval) {
    auto [st, ed] = interval;
    list<pair<size_t, size_t>>::iterator it, squash_from;
    for (it = intervals.begin(); it != intervals.end(); ++it) {
        auto [cur_st, cur_ed] = *it;
        // involved in current interval
        if (cur_st <= st && cur_ed >= ed) {
            return;
        }
        // involve current interval
        if (st < cur_st && ed > cur_ed) {
            *it = interval;
            squash_from = it;   // need squash
            break;
        }
        // check subsequent intervals
        if (cur_ed < st) {
            continue;
        }
        // should reside before current interval
        if (cur_st > ed) {
            intervals.insert(it, interval);
            return;
        }
        // have overlap, should merge two intervals
        if (cur_ed >= ed) {
            *it = make_pair(st, cur_ed);
            return;
        }
        *it = make_pair(cur_st, ed);
        squash_from = it;   // need squash
        break;
    }
    if (it == intervals.end()) {
        intervals.push_back(interval);
        return;
    }
    // squash
    auto [new_st, new_ed] = *squash_from;
    for (++it; it != intervals.end();) {
        auto [cur_st, cur_ed] = *it;
        if (cur_st <= new_ed) {
            new_ed = max(cur_ed, ed);
            it = intervals.erase(it);
        } else {
            break;
        }
    }
    squash_from->second = new_ed;
}
