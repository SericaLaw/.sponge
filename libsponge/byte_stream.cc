#include "byte_stream.hh"
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) : cap(capacity) {
    buf = std::vector<char>(capacity + 1);
}

// writer
size_t ByteStream::write(const string &data) {
    size_t i = 0, n = data.size();
    while ((tail + 1) % (cap + 1) != head && i < n && not input_ended()) {
        buf[tail] = data[i];
        tail = (tail + 1) % (cap + 1);
        ++i;
        ++_bytes_written;
    }
    return i;
}

size_t ByteStream::remaining_capacity() const {
    if (head <= tail) {
        return cap - (tail - head);
    } else {
        return head - tail - 1;
    }
}

void ByteStream::end_input() { _input_ended = true; }

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string res;
    size_t i = head;
    size_t remain = len;
    while (remain > 0 && i != tail) {
        res += buf[i];
        i = (i + 1) % (cap + 1);
        --remain;
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t remain = len;
    while (remain > 0 && head != tail) {
        head = (head + 1) % (cap + 1);
        --remain;
        _bytes_read++;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);
    pop_output(len);
    return res;
}

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const {
    return cap - remaining_capacity();
}

bool ByteStream::buffer_empty() const {
    return remaining_capacity() == cap;
}

bool ByteStream::eof() const { return input_ended() && head == tail; }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }