#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), mp(), expectId(0), eofId(-1), eofFlag(false) {}

void StreamReassembler::putToUnAssemble(const std::string &data, const uint64_t index) {
    uint64_t realId = index;
    auto it = mp.begin();
    string toIns(data);
    while (it != mp.end()) {
        size_t insLeft = realId;
        size_t insRight = insLeft + toIns.length() - 1;
        size_t elemLeft = it->first;
        string &tmp = it->second;
        size_t elemRight = elemLeft + tmp.length() - 1;
        if (insLeft > elemRight || insRight < elemLeft) {  // no overlap
            it++;
            continue;
        }
        if (insLeft <= elemLeft && insRight >= elemRight)  // cover
            it = mp.erase(it);
        else if (insLeft >= elemLeft && insRight <= elemRight)  // useless
            return;
        else if (insRight > elemRight) {
            realId = elemLeft;
            toIns.insert(0, tmp.substr(0, insLeft - elemLeft));
            it = mp.erase(it);
        } else {
            toIns += tmp.substr(insRight - elemLeft + 1, elemRight - insRight);
            it = mp.erase(it);
        }
    }
    mp[realId] = toIns;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t remainCap = _output.remaining_capacity();
    size_t windowRight = expectId + remainCap;
    size_t startId = index;
    string comeStr(data);
    if (eofFlag)
        return;

    if (index < expectId) {
        if (index + data.length() <= expectId)
            return;
        comeStr.erase(0, expectId - index);
        startId = expectId;
    }
    if (startId + comeStr.length() > windowRight)
        comeStr.erase(windowRight - startId, startId + comeStr.length() - windowRight);
    if (eofId >= 0 && startId + comeStr.length() > static_cast<size_t>(eofId))
        comeStr.erase(eofId - startId, startId + comeStr.length() - eofId);
    if (eofId == -1 && eof)
        eofId = index + data.length();
    if (expectId == static_cast<size_t>(eofId)) {
        eofFlag = true;
        _output.end_input();
    }
    if (comeStr.length() == 0)
        return;

    putToUnAssemble(comeStr, startId);
    while (!mp.empty()) {
        if (eofFlag)
            break;
        auto it = mp.begin();
        string &tmp = it->second;
        size_t nowStart = it->first;
        if (nowStart != expectId)
            break;
        size_t len = _output.write(tmp);
        expectId += len;
        mp.erase(mp.begin());
        if (expectId == static_cast<size_t>(eofId)) {
            eofFlag = true;
            _output.end_input();
        }
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t ans = 0;
    for (auto &it : mp)
        ans += it.second.length();
    return ans;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
