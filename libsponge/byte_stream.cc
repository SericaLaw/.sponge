#include <algorithm>

#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity{capacity} {}

size_t ByteStream::write(const string &data) {
    auto data_size = data.size();
    auto wc = min(data_size, remaining_capacity());
    _bytes_written += wc;
    if (wc == data_size) {
        _buffer.append(Buffer{string{data}});
    } else {
        _buffer.append(Buffer{string{string_view(data.c_str(), wc)}});
    }
    return wc;
}

size_t ByteStream::write(string &&data) {
    auto data_size = data.size();
    auto wc = min(data_size, remaining_capacity());
    _bytes_written += wc;
    data.resize(wc);
    _buffer.append(Buffer{move(data)});

    return wc;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    auto remain = min(len, buffer_size());
    string ret;
    ret.reserve(remain);
    for (const auto &segment : _buffer.buffers()) {
        auto segment_size = segment.size();
        if (remain >= segment_size) {
            ret.append(segment);
            remain -= segment_size;
            if (remain == 0) {
                break;
            }
        } else {
            ret.append(segment.str(remain));
            break;
        }
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    auto rc = min(len, buffer_size());
    _bytes_read += rc;
    _buffer.remove_prefix(rc);
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res{peek_output(len)};
    pop_output(len);
    return res;
}

void ByteStream::end_input() { _input_ended = true; }

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return _bytes_written - _bytes_read; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _input_ended && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
