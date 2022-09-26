#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : dq(), cap(capacity), inputEndFlag(false), readTotal(0), writeTotal(0) {}

size_t ByteStream::write(const string &data) {
    size_t sz = data.length();
    size_t canWrite = remaining_capacity();
    sz = (sz > canWrite) ? canWrite : sz;
    for (size_t i = 0; i < sz; i++) {
        dq.push_back(data[i]);
        writeTotal += 1;
    }
    return sz;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ans = "";
    size_t canRead = buffer_size();
    size_t realRead = (len > canRead) ? canRead : len;
    auto it = dq.begin();
    for (size_t i = 0; i < realRead; i++) {
        ans.push_back(*it);
        it++;
    }
    return ans;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t canPop = buffer_size();
    size_t realPop = (len > canPop) ? canPop : len;
    for (size_t i = 0; i < realPop; i++) {
        dq.pop_front();
        readTotal += 1;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ans = peek_output(len);
    pop_output(len);
    return ans;
}

void ByteStream::end_input() { inputEndFlag = true; }

bool ByteStream::input_ended() const { return inputEndFlag; }

size_t ByteStream::buffer_size() const { return dq.size(); }

bool ByteStream::buffer_empty() const { return dq.empty(); }

bool ByteStream::eof() const { return inputEndFlag && dq.empty(); }

size_t ByteStream::bytes_written() const { return writeTotal; }

size_t ByteStream::bytes_read() const { return readTotal; }

size_t ByteStream::remaining_capacity() const { return cap - dq.size(); }
