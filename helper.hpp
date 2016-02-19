//
//  helper.hpp
//
//  Copyright © 2016 Joachim Schurig. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice, this
//  list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//  this list of conditions and the following disclaimer in the documentation
//  and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
//  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef helper_hpp_DUFGDSKJVGJSHBDCHJZDBJVHDSTCVSCZJBH
#define helper_hpp_DUFGDSKJVGJSHBDCHJZDBJVHDSTCVSCZJBH


#include <unordered_map>
#include <unordered_set>
#include <cinttypes>
#include <string>
#include <cstring>
#include <chrono>
#include <vector>
#include <mutex>
#include <cwctype>


#if defined(__APPLE__) && defined(__MACH__)
#if !defined(__OSX__)
#define __OSX__
#endif
#endif


namespace CDDB {


/// switch from C to Unicode encoding

std::string set_unicode_locale(std::string name = std::string(), bool throw_on_error = false);


template<class String>
inline String get_filename(const String& str)
{
    String name;
    auto b = str.rfind('/');
    if (b == String::npos) return str;
    if (b < str.length()) name = str.substr(b + 1, str.length() - b + 1);
    return name;
}

inline std::string get_filename(const char* str)
{
    return get_filename(std::string(str));
}

template<class String>
inline String get_last_path(const String& str)
{
    String path;
    auto b = str.rfind('/');
    if (b == String::npos || b == 0) return path;

    auto a = --b;
    for(;;) {
        a = str.rfind('/', a);
        if (a < b) break;
        else if (a == String::npos) return str.substr(0, b + 1);
        --a;
        --b;
    }
    return str.substr(a + 1, b - a);
}

inline std::string get_last_path(const char* str)
{
    return get_last_path(std::string(str));
}
    
template <class String>
bool begins_with(const String& str, const String& begin)
{
    if (begin.size() > str.size()) return false;
    auto si = str.begin();
    for (auto bi : begin) {
        if (bi != *si) return false;
        ++si;
    }
    return true;
}

template <class String>
bool begins_with(const String& str, const typename String::value_type* begin)
{
    if (begin == nullptr) return false;
    if (str.empty() && *begin) return false;
    for (auto si : str) {
        if (si != *begin) return !*begin;
        ++begin;
    }
    return !*begin;
}

template <class String>
bool ends_with(const String& str, const String& end)
{
    if (end.size() > str.size()) return false;
    auto si = str.end() - end.size();
    for (auto bi : end) {
        if (bi != *si) return false;
        ++si;
    }
    return true;
}
    
template <class String>
bool ends_with(const String& str, const typename String::value_type* end)
{
    if (end == nullptr) return false;
    if (str.empty() && *end) return false;
    std::size_t clen = ::strnlen(end, str.length()+2);
    if (clen > str.length()) return false;
    for (auto it = str.end() - clen; it != str.end(); ++it) {
        if (*it != *end) return !*end;
        ++end;
    }
    return !*end;
}

class Duration {
public:
    enum Precision { Days = 0, Hours = 1, Minutes = 2, Seconds = 3, Milliseconds = 4, Microseconds = 5, Nanoseconds = 6 };
    Duration()
    : m_start(std::chrono::steady_clock::now())
    , m_last_lap(m_start)
    , m_this_lap(m_start) {}
    ~Duration() {}
    
    void lap() { m_last_lap = m_this_lap; m_this_lap = std::chrono::steady_clock::now(); }
    std::string print_lap(Precision precision = Precision::Milliseconds) const { return to_string(m_last_lap, m_this_lap, precision); }
    std::string to_string(Precision precision = Precision::Milliseconds) const { return to_string(m_start, m_this_lap, precision); }
    uint64_t get_lap(Precision precision = Precision::Nanoseconds) const;
    uint64_t get(Precision precision = Precision::Nanoseconds) const;

    static std::string to_string(uint64_t nanoseconds, Precision precision);
    static std::string to_string(std::chrono::steady_clock::time_point since, std::chrono::steady_clock::time_point now, Precision precision)
    { return Duration::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(now - since).count(), precision); }

private:
    struct divider_t {
        uint64_t divider;
        const char* unit;
        const char separator;
        uint16_t digits;
    };
    typedef std::vector<divider_t> dividers_t;
    static dividers_t dividers;

    std::chrono::steady_clock::time_point m_start;
    std::chrono::steady_clock::time_point m_last_lap;
    std::chrono::steady_clock::time_point m_this_lap;

    static uint64_t print_unit(std::string& out, uint64_t nanoseconds, dividers_t::const_iterator it);
};


/// This template class tokenizes C++ strings (splits between configurable separator characters). It
/// also understands the notion of quoted strings, so, sequences in the input which are delimited by
/// doublequotes. If a doublequote is preceeded by (an odd count of) backslashes, it is not counted
/// as a doublequote.

template <class T>
class StringTokenizer
{
private:
    // last start to have fast access to last delimiter if requested
    typename T::size_type m_start, m_laststart, m_end;
    const T& m_string;
    const T m_delimit;
    const bool m_doublequoted;

protected:
    /// checks if an even number of backslashes precedes the position
    /// (and 0 counts as even, too :)
    bool unescaped(typename T::size_type pos)
    {
        typename T::size_type ct = 0;
        while (pos-- > 0) {
            if (m_string[pos] == '\\') ++ct;
            else break;
        }
        return (ct & 1) == 0;
    }

public:
    /// get the next string token
    T get()
    {
        // skip leading delimiters
        while (m_start < m_string.size() && m_delimit.find(m_string[m_start], 0) != T::npos) ++m_start;
        if (m_start >= m_string.size()) return T();
        typename T::size_type pos;
        // save start position to recover delimiter for left_delimiter()
        m_laststart = m_start;
        // the compiler will optimize this condition out if m_doublequoted == false
        if (m_doublequoted && m_string[m_start] == '"') {
            // token starts with an unescaped quote - find the next unescaped quote regardless
            // of delimiters and return the substring between
            ++m_start;
            pos = m_start;
            while ((pos = m_string.find('"', pos)) != T::npos) {
                // check if this one is unescaped
                if (unescaped(pos)) {
                    typename T::size_type s = m_start;
                    // pos + 1 goes behind the closing ", and we expect a delimiter thereafter
                    // as with no quoted string, therefore + 1
                    m_start = pos + 1 + 1;
                    return m_string.substr(s, pos - s);
                } else {
                    pos += 1;
                    if (pos == m_string.size()) {
                        // this is an error - no closing doublequote
                        typename T::size_type s = m_start;
                        m_start = pos + 1;
                        return m_string.substr(s, pos - s - 1);
                    }
                }
            }
            // this is an error, an open quoted string
            pos = m_start;
            m_start = m_string.size() + 1;
            return m_string.substr(pos, m_string.size() - pos);
        }
        while ((pos = m_string.find_first_of(m_delimit, m_start)) != T::npos) {
            typename T::size_type s = m_start;
            m_start = pos + 1;
            return m_string.substr(s, pos - s);
        }
        // this is the last fraction after a (last) delimiter
        if (m_start < m_string.size()) {
            pos = m_start;
            m_start = m_string.size() + 1;
            return m_string.substr(pos, m_string.size() - pos);
        }
        // and this is an empty string, when the last delimiter
        // was at last position
        return T();
    }
    /// assign the next token from the class
    operator T()
    {
        return get();
    }
    /// provide a stream output (not yet functional)
    std::ostream& operator << (std::ostream& os) {
        os << get();
        return os;
    }
    bool eol() const
    {
        return m_start >= m_string.size();
    }
    /// valid for last get(), returns the left (starting) delimiter
    typename T::value_type left_delimiter() const
    {
        if (!m_laststart) return 0;
        return m_string[m_laststart - 1];
    }
    /// valid for last get(), returns the right (ending) delimiter
    typename T::value_type right_delimiter() const
    {
        if (!m_start || m_start > m_string.size()) return 0;
        return m_string[m_start - 1];
    }
    void reset()
    {
        m_start = 0;
    }
    template <class Container>
    Container split()
    {
        Container container;
        while (!eol()) container.emplace_back(get());
        return container;
    }
    template <class Container>
    typename Container::size_type split(Container& container)
    {
        container = split<Container>();
        return container.size();
    }
    StringTokenizer(const T& input_p, const T& delimit_p, bool doublequoted = true)
    : m_start(0)
    , m_laststart(0)
    , m_string(input_p)
    , m_delimit(delimit_p)
    , m_doublequoted(doublequoted)
    {
    }
};


template <class String>
typename String::size_type trim_right(String& str, typename String::value_type ch = ' ')
{
    auto rit = str.rbegin();
    for (; rit != str.rend() && *rit == ch; ++rit);
    str.erase(str.rend() - rit, String::npos);
    return str.size();
}

template <class String>
typename String::size_type trim_left(String& str, typename String::value_type ch = ' ')
{
    auto it = str.begin();
    for (; it != str.end() && *it == ch; ++it);
    str.erase(0, it - str.begin());
    return str.size();
}

template <class String>
typename String::size_type trim_multiple(String& str, typename String::value_type ch = ' ')
{
    auto rit = str.rbegin();
    for (;;) {
        for (; rit != str.rend() && *rit != ch; ++rit);
        if (rit == str.rend()) return str.size();
        auto begin = rit;
        ++rit;
        for (; rit != str.rend() && *rit == ch; ++rit);
        typename String::size_type n = rit - begin;
        if (n > 1) str.erase(str.rend() - rit, n - 1);
    }
    return str.size();
}

template <class String>
typename String::size_type trim(String& str, typename String::value_type ch = ' ')
{
    if (!trim_right(str, ch)) return 0;
    return trim_left(str, ch);
}

template <class String>
typename String::size_type trim_all(String& str, typename String::value_type ch = ' ')
{
    trim(str, ch);
    return trim_multiple(str, ch);
}

template <class String>
size_t to_title_case(String& str, bool force_upcase)
{
    bool was_space = true;
    // special for CDDB - do not Titlecase the word CD
    bool was_C = false;
    std::size_t changed = 0;

    for (auto& ch : str) {
        if (!std::iswalnum(ch)) {
            was_space = true;
        } else {
            if (was_space) {
                was_C = ch == 'C';
                if (force_upcase) {
                    if (std::iswlower(ch)) {
                        ch = std::towupper(ch);
                        ++changed;
                    }
                }
            } else {
                if (std::iswupper(ch)) {
                    if (!(was_C && ch == 'D')) {
                        ch = std::towlower(ch);
                        ++changed;
                    }
                }
                was_C = false;
            }
            was_space = false;
        }
    }
    return changed;
}

template <class String>
std::size_t tolower(String& str)
{
    std::size_t changed = 0;
    for (auto& ch : str) {
        if (sizeof(ch) == 1) {
            if (std::isupper(ch)) {
                ch = std::tolower(ch);
                ++changed;
            }
        } else {
            if (std::iswupper(ch)) {
                ch = std::towlower(ch);
                ++changed;
            }
        }
    }
    return changed;
}

template <class String>
std::size_t toupper(String& str)
{
    std::size_t changed = 0;
    for (auto& ch : str) {
        if (sizeof(ch) == 1) {
            if (std::islower(ch)) {
                ch = std::toupper(ch);
                ++changed;
            }
        } else {
            if (std::iswlower(ch)) {
                ch = std::towupper(ch);
                ++changed;
            }
        }
    }
    return changed;
}

#ifdef __clang__
constexpr inline bool is_big_endian()
#else
inline bool is_big_endian()
#endif
{
    union {
        uint32_t i;
        char c[4];
    } endian = {0x01020304};

    return (endian.c[0] == 1);
}

inline void swap_bytes(void* p, std::size_t len)
{
    if (len > 1) {
        uint8_t* cp = (uint8_t*)p;
        for (std::size_t i = 0, e = len-1; i < len/2; ++i, --e) {
            std::swap(cp[i], cp[e]);
        }
    }
}

inline void swap_bytes(char* p)
{
    std::size_t x = std::strlen(p);
    swap_bytes(p, x);
}

template <class VALUE>
void swap_bytes(VALUE& value)
{
    if (!std::is_scalar<VALUE>::value) throw std::runtime_error("operation only supported for scalar type");
    swap_bytes(&value, sizeof(VALUE));
}

template <class VALUE>
void to_big_endian(VALUE& value)
{
    if (!std::is_scalar<VALUE>::value) throw std::runtime_error("operation only supported for scalar type");
    if (!is_big_endian()) swap_bytes(value);
}

template <class VALUE>
void to_little_endian(VALUE& value)
{
    if (!std::is_scalar<VALUE>::value) throw std::runtime_error("operation only supported for scalar type");
    if (is_big_endian()) swap_bytes(value);
}

class FNVHash32 {
public:
    FNVHash32() : m_result(2166136261) {}

    template <class VALUE>
    void add(VALUE value) {
        if (!std::is_scalar<VALUE>::value) throw std::runtime_error("operation only supported for scalar type");
        uint8_t* p = (uint8_t*)&value;
        for (uint16_t i = 0; i < sizeof(VALUE); ++i) add(p[i]);
    }

    template <class VALUE>
    void add(VALUE value, bool big_endian) {
        if (!std::is_scalar<VALUE>::value) throw std::runtime_error("operation only supported for scalar type");
        if (is_big_endian() != big_endian) swap_bytes(value);
        add(value);
    }

    void add(const char* p) {
        for (; *p != 0; ++p) add(*p);
    }

    void add(const std::string& s) {
        for (auto ch : s) add(ch);
    }

    void add(uint8_t value) {
        m_result *= 16777619;
        m_result ^= value;
    }

    uint32_t result() const { return m_result; }

    operator uint32_t const() { return result(); }

private:
    uint32_t m_result;
};


template<class Element>
class Entropy {
public:
    void clear()
    {
        m_elements.clear();
        m_count = 0;
    }

    void operator+(Element e)
    {
        add(e);
    }

    template<class Container>
    void operator+(const Container& container)
    {
        add(container);
    }

    Entropy& operator+=(Element e)
    {
        add(e);
        return *this;
    }

    template<class Container>
    Entropy& operator+=(const Container& container)
    {
        add(container);
        return *this;
    }

    void add(Element e)
    {
        m_elements.insert(e);
        ++m_count;
    }

    template<class Container>
    void add(const Container& container)
    {
        for (auto& e : container) add(static_cast<Element>(e));
    }

    std::size_t size() const
    {
        return m_elements.size();
    }

    std::size_t count() const
    {
        return m_count;
    }

    bool operator==(const Entropy& other) const
    {
        return size() == other.size();
    }
    
    bool operator<(const Entropy& other) const
    {
        return size() < other.size();
    }

    bool has_value(Element e) const
    {
        return m_elements.find(e) != m_elements.end();
    }
    
private:
    typedef std::unordered_set<Element> elements_t;
    elements_t m_elements;
    std::size_t m_count = 0;
};


template <uint16_t nglen, bool added_spaces>
class NGrams {
public:
    NGrams(const std::wstring& str = L"")
    {
        if (nglen < 2) throw std::runtime_error("NGram minimum len is 2");
        init(str);
    }

    std::size_t init(const std::wstring& str)
    {
        m_ngrams.clear();

        // the if and else branches all get optimized
        // away during compilation as they depend solely
        // on compile time static template parameters

        std::size_t minlen;
        if (added_spaces) {
            if (nglen == 2) minlen = 1;
            else minlen = nglen-2;
        } else {
            minlen = nglen;
        }
        if (str.size() < minlen) return 0;

        if (added_spaces) {
            m_count = str.size() - (nglen-3);
        } else {
            m_count = str.size() - (nglen-1);
        }

        if (added_spaces) add_char(' ');
        for (auto ch : str) add_char(ch);
        if (added_spaces) add_char(' ');
        return m_ngrams.size();
    }

    uint16_t compare(const NGrams& other)
    {
        if (!m_count) return 0;

        uint16_t found = 0;
        std::vector<bool> used;
        used.insert(used.begin(), m_ngrams.size(), false);

        for (auto& o : other.m_ngrams) {
            auto u = used.begin();
            for (auto& g : m_ngrams) {
                if (!*u && o == g) {
                    ++found;
                    *u = true;
                    break;
                }
                ++u;
            }
        }

        return found * 100 / m_count;
    }

    uint16_t compare(const std::wstring& str)
    {
        NGrams<nglen, added_spaces> other(str);
        return compare(other);
    }

    static uint16_t compare(const std::wstring& left, const std::wstring& right)
    {
        NGrams<nglen, added_spaces> g_left(left);
        return g_left.compare(right);
    }

private:
    typedef std::string ngram_t;
    typedef std::vector<ngram_t> ngrams_t;

    void add_char(std::wstring::value_type ch)
    {
        auto size = m_ngrams.size();
        if (size < m_count) {
            m_ngrams.emplace_back("");
            ++size;
        }
        for (uint16_t ct = 0; ct < nglen && ct < size; ++ct) {
            m_ngrams[size - ct - 1] += ch;
        }
    }
    
    ngrams_t m_ngrams;
    std::size_t m_count = 0;
};



} // of namespace CDDB


#endif /* helper_hpp */
