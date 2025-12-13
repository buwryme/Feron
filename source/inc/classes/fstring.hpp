#pragma once

#include <cstdint>
#include <cstddef>
#include "../runtime/impl/mm/malloc.hpp"
#include "../runtime/impl/mm/free.hpp"
#include <cstring>

namespace feron{

class string {
public:
    // --- constructors / destructor ---
    inline string() noexcept : data_(nullptr), bytes_(0), owned_(false) {}
    inline string(const char* s) noexcept { init_from_cstr(s); }
    inline string(const char* s, std::size_t n) noexcept { init_from_bytes(s, n); }
    inline string(const string& o) noexcept { copy_from(o); }
    inline string(string&& o) noexcept { steal_from(o); }
    inline ~string() noexcept { release(); }

    inline string& operator=(const string& o) noexcept { if (this != &o) { release(); copy_from(o); } return *this; }
    inline string& operator=(string&& o) noexcept { if (this != &o) { release(); steal_from(o); } return *this; }

    // --- creation helpers ---
    inline static string from_cstr(const char* s) noexcept { return string(s); }
    // inline static string empty() noexcept { return string(); }

    // --- basic queries ---
    inline std::size_t size_bytes() const noexcept { return bytes_; }
    inline bool empty() const noexcept { return bytes_ == 0; }

    // Returns number of Unicode code points (O(n) cached on demand)
    inline std::size_t length() const noexcept {
        if (!data_) return 0;
        std::size_t count = 0;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data_);
        const uint8_t* end = p + bytes_;
        while (p < end) {
            std::uint8_t lead = *p;
            std::size_t adv = utf8_advance_bytes(lead);
            if (adv == 0) { ++p; continue; }
            p += adv;
            ++count;
        }
        return count;
    }

    // --- indexing by code point (like JS .charAt / .codePointAt) ---
    // charAt returns a new string containing the single code point (or empty)
    inline string charAt(std::size_t index) const noexcept {
        std::size_t byte_off = codepoint_index_to_byte(index);
        if (byte_off == SIZE_MAX) return string();
        std::size_t adv = utf8_char_bytes_at(byte_off);
        return string(data_ + byte_off, adv);
    }

    // codePointAt returns the Unicode code point value or 0xFFFFFFFF if out of range
    inline uint32_t codePointAt(std::size_t index) const noexcept {
        std::size_t byte_off = codepoint_index_to_byte(index);
        if (byte_off == SIZE_MAX) return 0xFFFFFFFFu;
        return decode_codepoint(reinterpret_cast<const uint8_t*>(data_) + byte_off);
    }

    // at supports negative indexing (like JS proposal)
    inline string at(int64_t index) const noexcept {
        if (index < 0) {
            std::size_t len = length();
            if (static_cast<std::size_t>(-index) > len) return string();
            return charAt(len + index);
        }
        return charAt(static_cast<std::size_t>(index));
    }

    // --- concatenation ---
    inline string concat(const string& other) const noexcept {
        if (!data_) return other;
        if (!other.data_) return *this;
        std::size_t nb = bytes_ + other.bytes_;
        char* buf = static_cast<char*>(malloc(nb + 1));
        if (!buf) return string();
        memcpy(buf, data_, bytes_);
        memcpy(buf + bytes_, other.data_, other.bytes_);
        buf[nb] = '\0';
        return string(buf, nb, true);
    }

    // --- search / contains ---
    inline bool includes(const string& needle, std::size_t fromIndex = 0) const noexcept {
        return indexOf(needle, fromIndex) != SIZE_MAX;
    }

    inline std::size_t indexOf(const string& needle, std::size_t fromIndex = 0) const noexcept {
        if (!data_ || !needle.data_) return SIZE_MAX;
        std::size_t start_byte = codepoint_index_to_byte(fromIndex);
        if (start_byte == SIZE_MAX) return SIZE_MAX;
        const char* hay = data_ + start_byte;
        const char* found = strstr(hay, needle.data_);
        if (!found) return SIZE_MAX;
        return byte_to_codepoint_index(static_cast<std::size_t>(found - data_));
    }

    inline std::size_t lastIndexOf(const string& needle) const noexcept {
        if (!data_ || !needle.data_) return SIZE_MAX;
        // naive reverse search by scanning
        std::size_t last = SIZE_MAX;
        std::size_t i = 0;
        std::size_t cpcount = length();
        for (; i < cpcount; ++i) {
            std::size_t idx = indexOf(needle, i);
            if (idx == SIZE_MAX) break;
            last = idx;
            i = idx; // continue after this index
        }
        return last;
    }

    inline bool startsWith(const string& prefix) const noexcept {
        if (!data_ || !prefix.data_) return false;
        if (prefix.bytes_ > bytes_) return false;
        return memcmp(data_, prefix.data_, prefix.bytes_) == 0;
    }

    inline bool endsWith(const string& suffix) const noexcept {
        if (!data_ || !suffix.data_) return false;
        if (suffix.bytes_ > bytes_) return false;
        return memcmp(data_ + (bytes_ - suffix.bytes_), suffix.data_, suffix.bytes_) == 0;
    }

    // --- slicing and substring ---
    // slice(start, end) with negative indices allowed
    inline string slice(int64_t start, int64_t end = INT64_MAX) const noexcept {
        std::size_t cp = length();
        int64_t s = normalize_index(start, cp);
        int64_t e = (end == INT64_MAX) ? static_cast<int64_t>(cp) : normalize_index(end, cp);
        if (s < 0) s = 0;
        if (e < s) return string();
        std::size_t sb = codepoint_index_to_byte(static_cast<std::size_t>(s));
        std::size_t eb = codepoint_index_to_byte(static_cast<std::size_t>(e));
        if (sb == SIZE_MAX || eb == SIZE_MAX || eb < sb) return string();
        return string(data_ + sb, eb - sb);
    }

    // substring(a,b) like JS (swaps if a>b, negative treated as 0)
    inline string substring(int64_t a, int64_t b = INT64_MAX) const noexcept {
        std::size_t cp = length();
        int64_t aa = (a < 0) ? 0 : a;
        int64_t bb = (b == INT64_MAX) ? static_cast<int64_t>(cp) : ((b < 0) ? 0 : b);
        if (aa > bb) { int64_t t = aa; aa = bb; bb = t; }
        return slice(aa, bb);
    }

    // substr(start, length)
    inline string substr(int64_t start, int64_t len) const noexcept {
        std::size_t cp = length();
        int64_t s = (start < 0) ? static_cast<int64_t>(static_cast<int64_t>(cp) + start) : start;
        if (s < 0) s = 0;
        if (s >= static_cast<int64_t>(cp)) return string();
        int64_t e = s + ((len < 0) ? (static_cast<int64_t>(cp) - s) : len);
        return slice(s, e);
    }

    // --- repeat ---
    inline string repeat(std::size_t count) const noexcept {
        if (!data_) return string();
        if (count == 0) return string();
        std::size_t nb = bytes_ * count;
        char* buf = static_cast<char*>(malloc(nb + 1));
        if (!buf) return string();
        char* p = buf;
        for (std::size_t i = 0; i < count; ++i) {
            memcpy(p, data_, bytes_);
            p += bytes_;
        }
        buf[nb] = '\0';
        return string(buf, nb, true);
    }

    // --- trim (whitespace ASCII only) ---
    inline string trim() const noexcept {
        if (!data_) return string();
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data_);
        std::size_t i = 0, j = bytes_;
        // trim left
        while (i < j && is_ascii_space(p[i])) ++i;
        // trim right
        while (j > i && is_ascii_space(p[j - 1])) --j;
        return string(data_ + i, j - i);
    }

    // --- padStart / padEnd (pad string with padStr to reach target length in code points) ---
    inline string padStart(std::size_t targetLength, const string& padStr = string(" ")) const noexcept {
        std::size_t cur = length();
        if (cur >= targetLength) return *this;
        std::size_t need = targetLength - cur;
        return pad_impl(need, padStr, true);
    }

    inline string padEnd(std::size_t targetLength, const string& padStr = string(" ")) const noexcept {
        std::size_t cur = length();
        if (cur >= targetLength) return *this;
        std::size_t need = targetLength - cur;
        return pad_impl(need, padStr, false);
    }

    // --- case conversions (ASCII only) ---
    inline string toUpperCase() const noexcept {
        if (!data_) return string();
        char* buf = static_cast<char*>(malloc(bytes_ + 1));
        if (!buf) return string();
        for (std::size_t i = 0; i < bytes_; ++i) {
            unsigned char c = static_cast<unsigned char>(data_[i]);
            if (c >= 'a' && c <= 'z') buf[i] = static_cast<char>(c - 32);
            else buf[i] = data_[i];
        }
        buf[bytes_] = '\0';
        return string(buf, bytes_, true);
    }

    inline string toLowerCase() const noexcept {
        if (!data_) return string();
        char* buf = static_cast<char*>(malloc(bytes_ + 1));
        if (!buf) return string();
        for (std::size_t i = 0; i < bytes_; ++i) {
            unsigned char c = static_cast<unsigned char>(data_[i]);
            if (c >= 'A' && c <= 'Z') buf[i] = static_cast<char>(c + 32);
            else buf[i] = data_[i];
        }
        buf[bytes_] = '\0';
        return string(buf, bytes_, true);
    }

    // --- replace (first occurrence) and replaceAll ---
    inline string replace(const string& search, const string& replaceWith) const noexcept {
        std::size_t idx = indexOf(search, 0);
        if (idx == SIZE_MAX) return *this;
        std::size_t bidx = codepoint_index_to_byte(idx);
        std::size_t before = bidx;
        std::size_t after = bytes_ - (bidx + search.bytes_);
        std::size_t nb = before + replaceWith.bytes_ + after;
        char* buf = static_cast<char*>(malloc(nb + 1));
        if (!buf) return string();
        char* p = buf;
        memcpy(p, data_, before); p += before;
        memcpy(p, replaceWith.data_, replaceWith.bytes_); p += replaceWith.bytes_;
        memcpy(p, data_ + bidx + search.bytes_, after); p += after;
        buf[nb] = '\0';
        return string(buf, nb, true);
    }

    inline string replaceAll(const string& search, const string& replaceWith) const noexcept {
        if (!data_ || !search.data_) return *this;
        // naive repeated replace
        string cur = *this;
        std::size_t pos = 0;
        while (true) {
            std::size_t idx = cur.indexOf(search, pos);
            if (idx == SIZE_MAX) break;
            cur = cur.replace(search, replaceWith);
            pos = idx + replaceWith.length();
        }
        return cur;
    }

    // --- split by single-char or string delimiter (returns simple dynamic array via caller-managed buffer) ---
    // For freestanding simplicity, provide a split that writes parts into a preallocated array of string.
    // Returns number of parts written (maxParts). If more parts exist, they are truncated.
    inline std::size_t split(const string& delim, string* outParts, std::size_t maxParts) const noexcept {
        if (!data_) return 0;
        if (!delim.data_) {
            // split into code points
            std::size_t cp = length();
            std::size_t written = 0;
            for (std::size_t i = 0; i < cp && written < maxParts; ++i) {
                outParts[written++] = charAt(i);
            }
            return written;
        }
        std::size_t start_cp = 0;
        std::size_t written = 0;
        while (true && written < maxParts) {
            std::size_t idx = indexOf(delim, start_cp);
            if (idx == SIZE_MAX) {
                // last part
                outParts[written++] = slice(start_cp, INT64_MAX);
                break;
            }
            outParts[written++] = slice(start_cp, idx);
            start_cp = idx + delim.length();
        }
        return written;
    }

    // --- iterator over code points ---
    struct Iterator {
        const char* base;
        const char* cur;
        const char* end;
        inline Iterator(const char* b, const char* c, const char* e) : base(b), cur(c), end(e) {}
        inline uint32_t operator*() const noexcept { return decode_codepoint(reinterpret_cast<const uint8_t*>(cur)); }
        inline Iterator& operator++() noexcept {
            std::size_t adv = utf8_char_bytes_at_ptr(reinterpret_cast<const uint8_t*>(cur), reinterpret_cast<const uint8_t*>(end));
            if (adv == 0) ++cur; else cur += adv;
            return *this;
        }
        inline bool operator!=(const Iterator& o) const noexcept { return cur != o.cur; }
    };

    inline Iterator begin() const noexcept { return Iterator(data_, data_, data_ ? data_ + bytes_ : nullptr); }
    inline Iterator end() const noexcept { return Iterator(data_, data_ ? data_ + bytes_ : nullptr, data_ ? data_ + bytes_ : nullptr); }

    // --- raw access ---
    inline const char* c_str() const noexcept { return data_ ? data_ : ""; }

private:
    const char* data_ = nullptr;
    std::size_t bytes_ = 0;
    bool owned_ = false;

    // private constructor for owned buffer
    inline string(char* buf, std::size_t n, bool owned) noexcept : data_(buf), bytes_(n), owned_(owned) {}

    inline void init_from_cstr(const char* s) noexcept {
        if (!s) { data_ = nullptr; bytes_ = 0; owned_ = false; return; }
        std::size_t n = strlen(s);
        init_from_bytes(s, n);
    }

    inline void init_from_bytes(const char* s, std::size_t n) noexcept {
        if (!s || n == 0) { data_ = nullptr; bytes_ = 0; owned_ = false; return; }
        char* buf = static_cast<char*>(malloc(n + 1));
        if (!buf) { data_ = nullptr; bytes_ = 0; owned_ = false; return; }
        memcpy(buf, s, n);
        buf[n] = '\0';
        data_ = buf; bytes_ = n; owned_ = true;
    }

    inline void copy_from(const string& o) noexcept {
        if (!o.data_) { data_ = nullptr; bytes_ = 0; owned_ = false; return; }
        char* buf = static_cast<char*>(malloc(o.bytes_ + 1));
        if (!buf) { data_ = nullptr; bytes_ = 0; owned_ = false; return; }
        memcpy(buf, o.data_, o.bytes_);
        buf[o.bytes_] = '\0';
        data_ = buf; bytes_ = o.bytes_; owned_ = true;
    }

    inline void steal_from(string& o) noexcept {
        data_ = o.data_; bytes_ = o.bytes_; owned_ = o.owned_;
        o.data_ = nullptr; o.bytes_ = 0; o.owned_ = false;
    }

    inline void release() noexcept {
        if (owned_ && data_) free(const_cast<char*>(data_));
        data_ = nullptr; bytes_ = 0; owned_ = false;
    }

    // --- helpers ---
    inline static bool is_ascii_space(uint8_t c) noexcept {
        return c == 0x20 || (c >= 0x09 && c <= 0x0D);
    }

    // Return number of bytes for a UTF-8 char given lead byte (0 if invalid)
    inline static std::size_t utf8_advance_bytes(uint8_t lead) noexcept {
        if ((lead & 0x80) == 0) return 1;
        if ((lead & 0xE0) == 0xC0) return 2;
        if ((lead & 0xF0) == 0xE0) return 3;
        if ((lead & 0xF8) == 0xF0) return 4;
        return 0;
    }

    inline std::size_t utf8_char_bytes_at(std::size_t byte_off) const noexcept {
        if (!data_ || byte_off >= bytes_) return 0;
        return utf8_advance_bytes(static_cast<uint8_t>(data_[byte_off]));
    }

    inline static std::size_t utf8_char_bytes_at_ptr(const uint8_t* p, const uint8_t* end) noexcept {
        if (!p || p >= end) return 0;
        return utf8_advance_bytes(*p);
    }

    // decode codepoint from pointer (assumes valid UTF-8; returns replacement 0xFFFD on invalid)
    inline static uint32_t decode_codepoint(const uint8_t* p) noexcept {
        if (!p) return 0xFFFDu;
        uint8_t b0 = p[0];
        if ((b0 & 0x80) == 0) return b0;
        if ((b0 & 0xE0) == 0xC0) {
            uint8_t b1 = p[1];
            return ((b0 & 0x1F) << 6) | (b1 & 0x3F);
        }
        if ((b0 & 0xF0) == 0xE0) {
            uint8_t b1 = p[1], b2 = p[2];
            return ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        }
        if ((b0 & 0xF8) == 0xF0) {
            uint8_t b1 = p[1], b2 = p[2], b3 = p[3];
            return ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        }
        return 0xFFFDu;
    }

    // convert codepoint index to byte offset; returns SIZE_MAX if out of range
    inline std::size_t codepoint_index_to_byte(std::size_t idx) const noexcept {
        if (!data_) return SIZE_MAX;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data_);
        const uint8_t* end = p + bytes_;
        std::size_t i = 0;
        while (p < end) {
            if (i == idx) return static_cast<std::size_t>(p - reinterpret_cast<const uint8_t*>(data_));
            std::size_t adv = utf8_advance_bytes(*p);
            if (adv == 0) { ++p; continue; }
            p += adv;
            ++i;
        }
        if (idx == i) return bytes_; // allow index == length -> end
        return SIZE_MAX;
    }

    // convert byte offset to codepoint index
    inline std::size_t byte_to_codepoint_index(std::size_t byte_off) const noexcept {
        if (!data_ || byte_off > bytes_) return SIZE_MAX;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data_);
        const uint8_t* end = p + byte_off;
        std::size_t i = 0;
        while (p < end) {
            std::size_t adv = utf8_advance_bytes(*p);
            if (adv == 0) { ++p; continue; }
            p += adv;
            ++i;
        }
        return i;
    }

    inline std::size_t normalize_index(int64_t idx, std::size_t cp_len) const noexcept {
        if (idx < 0) {
            int64_t v = static_cast<int64_t>(cp_len) + idx;
            return v < 0 ? 0 : static_cast<std::size_t>(v);
        }
        return static_cast<std::size_t>(idx);
    }

    inline string pad_impl(std::size_t need, const string& padStr, bool start) const noexcept {
        if (!padStr.data_ || padStr.bytes_ == 0) return *this;
        // build pad by repeating padStr until need reached (in code points)
        // naive: repeat padStr bytes until codepoint count >= need
        string acc = string();
        while (acc.length() < need) {
            acc = acc.concat(padStr);
        }
        // trim acc to exact code points
        acc = acc.slice(0, static_cast<int64_t>(need));
        if (start) return acc.concat(*this);
        return this->concat(acc);
    }
};

} // namespace feron