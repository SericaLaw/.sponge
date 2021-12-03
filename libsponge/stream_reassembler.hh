#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cassert>
#include <cstdint>
#include <set>
#include <string>

class Segment {
  private:
    // initial
    size_t _index{};
    Buffer _data{};

    // for trim prefix and suffix
    size_t _start_index{};
    size_t _end_index{};

  public:
    Segment(const size_t index, const std::string_view &data) : _index{index}, _data{std::string{data}}, _start_index{index}, _end_index{index + data.size()} {}

    // we don't actually operate on the data
    void remove_prefix(size_t n) {
        _start_index += n;
        assert(_start_index <= _end_index);
    }
    void remove_suffix(size_t n) {
        _end_index -= n;
        assert(_start_index <= _end_index);
    }

    size_t size() const { return str().size(); }
    size_t start_index() const { return _start_index; }
    size_t end_index() const { return _end_index; }
    const Buffer &buffer() const { return _data; }

    // return the string view which shadows the trimmed part
    std::string_view str() const {
        return { _data.str().begin() + _start_index - _index, _end_index - _start_index};
    }
    bool operator < (const size_t &rhs) const { return end_index() < rhs; }
    // ensure no overlapping when insert, so we can simply compare the start index
    bool operator < (const Segment &rhs) const { return start_index() < rhs.start_index(); }

};

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    size_t _end_index{0};
    size_t _unassembled_bytes{0};
    bool _got_eof{false};
    std::set<Segment> _segments{};

    void push_interval(Segment &segment);

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
