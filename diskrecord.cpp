//
//  diskrecord.cpp
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

#include <cstdlib>
#include <cstring>
#include <array>
#include <vector>
#include "diskrecord.hpp"
#include "helper.hpp"
#include "utf8.hpp"
#include "format.hpp"


using namespace CDDB;

/// construct a DiskRecord out of explicit parameters

DiskRecord::DiskRecord(uint32_t discid
                       , std::string&& artist
                       , std::string&& title
                       , uint16_t year
                       , std::string&& genre
                       , list_t&& songs
                       , frame_t&& frames
                       , uint16_t revision
                       , uint32_t seconds)
: m_discid(discid)
, m_artist(artist)
, m_title(title)
, m_year(year)
, m_genre(genre)
, m_songs(songs)
, m_frames(frames)
, m_revision(revision)
, m_seconds(seconds)
{
    m_discid_ready = true;
    verify_record();
}


/// construct a DiskRecord out of the raw cddb file data stream

DiskRecord::DiskRecord(const std::vector<char>& data)
{
    std::string key;
    std::string value;
    char lastch = 0;
    enum state_t { Skip, Start, Key, Limiter, Start2, Value, Comment };
    state_t state = Start;

    for (auto ch : data) {

        if (ch == '\n' || ch == '\r') {
            if (state == Value || state == Skip) {
                add_keyvalue(key, value);
            } else if (state == Comment) {
                add_comment(value);
            }

            state = Start;
            key.clear();
            value.clear();

            continue;
        }

        switch (state) {

            case Skip:
                break;

            case Start:
                if (ch == '#') {
                    state = Comment;
                }
                else if (!std::isspace(ch)) {
                    state = Key;
                    key = ch;
                }
                break;

            case Key:
                if (std::isspace(ch)) {
                    state = Limiter;
                } else if (ch == '=') {
                    state = Start2;
                } else {
                    key += ch;
                }
                break;

            case Limiter:
                if (std::isspace(ch)) break;
                else if (ch == '=') state = Start2;
                else state = Skip; // we only expect space or = at this point, everything else is an error
                break;

            case Start2:
                if (!std::isspace(ch)) {
                    state = Value;
                    value = ch;
                    lastch = ch;
                }
                break;

            case Value:
                if (lastch != ' ' || ch != ' ') value += ch;
                lastch = ch;
                break;

            case Comment:
                // skip leading spaces
                if (!value.empty() || !std::isspace(ch)) value += ch;
                break;
        }
    }

    // just in case the last line had no linefeed
    if (state == Value || state == Skip) {
        add_keyvalue(key, value);
    }

    cleanup();
    verify_record();
    if (valid()) m_valid = !bad_encoding();
    m_seconds = convert_frame_starts_in_frame_lengths(m_seconds, m_frames);
}

void DiskRecord::calc_discid() const
{
    if (m_valid) {
        // calculate an ID over track lengths and count
        m_discid = private_discid(m_seconds, m_frames);
    }
    m_discid_ready = true;
}

void DiskRecord::calc_fuzzy_discid() const
{
    if (m_valid) {
        // calculate an ID over track lengths and count
        m_fuzzy_discid = private_fuzzy_discid(m_seconds, m_frames);
    }
    m_fuzzy_discid_ready = true;
}

void DiskRecord::verify_record()
{
    // check if this is a valid record
    m_valid = !m_artist.empty()
            && !m_title.empty()
            && (m_frames.empty() || m_songs.size() == m_frames.size())
            && m_seconds;
}

inline uint32_t read_integer_from_string(const std::string& value, size_t label_len)
{
    return static_cast<uint32_t>(std::strtoul(value.c_str()+label_len, nullptr, 10));
}

void DiskRecord::add_comment(std::string& value)
{
    try {

        if (value.size() > 255) return;

        if (!m_read_tracks) {

            if (CDDB::begins_with(value, "Track frame offsets:")) m_read_tracks = true;
            else if (CDDB::begins_with(value, "Disc length: ")) m_seconds = read_integer_from_string(value, std::strlen("Disc length: "));
            else if (CDDB::begins_with(value, "Revision: ")) m_revision = read_integer_from_string(value, std::strlen("Revision: "));

        } else {

            trim(value);
            if (value.empty()) m_read_tracks = false;
            else if (CDDB::begins_with(value, "Disc length: ")) {
                // this is a record that misses an empty line between the track listing and the disc length
                m_read_tracks = false;
                m_seconds = read_integer_from_string(value, std::strlen("Disc length: "));
            }
            else {
                uint32_t i = read_integer_from_string(value, 0);
                if (!i) m_read_tracks = false;
                else m_frames.emplace_back(i);
            }
        }
            
    } catch (std::out_of_range& e) {
        // continue if value out of range - treat it as if it had not been there
    }
}

void DiskRecord::add_keyvalue(const std::string& key, std::string& value)
{
    try {
        // any non-comment certainly ends reading the track frame list
        m_read_tracks = false;

        trim_right(value);

        if (key.empty() || value.empty()) return;
        if (key.size() > 15 || value.size() > 255) return;

        // check encoding of value, could be either iso8859-1 (including ASCII) or utf8
        if (!Unicode::valid_utf8(value)) {
            std::string utfs;
            // convert to utf8, assuming that the string is in iso8859-1
            Unicode::to_utf8(value, utfs);
            value.swap(utfs);
        }

        if (key == "DISCID") {
            // we do not process the legacy discid anymore
        }
        else if (key == "DYEAR" ) m_year    = std::stoul(value);
        else if (key == "DGENRE") m_genre   = value;
        else if (key == "DTITLE") {
            auto p = value.find(" / ");
            if (p != std::string::npos) {
                m_artist  = value.substr(0, p);
                m_title   = value.substr(p+3, value.length() - p+3);
            } else {
                // title and artist are assumed to be the same if there is no /
                m_artist  = value;
                m_title   = value;
            }
        }
        else {
            if (CDDB::begins_with(key, "TTITLE")) {
                uint32_t lv = read_integer_from_string(key, std::strlen("TTITLE"));
                // some TTITLE lists start at 1, not at 0..
                if (m_songs.empty()) m_list_base = lv;
                if (m_songs.size() != lv - m_list_base) return;
                m_songs.emplace_back(value);
            }
        }
        
    } catch (std::out_of_range& e) {
        // continue if value out of range - treat it as if it had not been there
    }
}

inline bool DiskRecord::mostly_uppercase(const std::wstring str)
{
    std::wstring::size_type upperalpha = 0;
    std::wstring::size_type nonupperalpha = 0;

    for (auto ch : str) {
        if (std::iswalpha(ch)) {
            if (std::iswupper(ch)) ++upperalpha;
            else ++nonupperalpha;
        }
    }

    m_uppercasecount = upperalpha;

    return nonupperalpha < upperalpha;
}

void DiskRecord::convert_to_titlecase(std::string& value, Entropy<std::wstring::value_type>* entropy)
{
    std::wstring wide;
    if (Unicode::from_utf8(value, wide)) {
        if (mostly_uppercase(wide)) {
            if (to_title_case(wide, false) > 0) {
                value.erase();
                Unicode::to_utf8(wide, value);
            }
        }
        if (entropy) entropy->add(wide);
    }
}

void DiskRecord::calc_hash() const
{
    // calculate a hash across relevant data
    CDDB::FNVHash32 hash;
    hash.add(m_artist);
    hash.add(m_title);
    for (auto s : m_songs) hash.add(s);
    m_hash = hash.result();
    m_hash_ready = true;
}

void DiskRecord::calc_normalized_hash() const
{
    // calculate a normalized hash
    CDDB::FNVHash32 hash;
    hash.add(normalize(m_artist));
    hash.add(normalize(m_title));
    for (auto s : m_songs) hash.add(normalize(s));
    m_normalized_hash = hash.result();
    m_normalized_hash_ready = true;
}

void DiskRecord::calc_bad_encoding(const Entropy<std::wstring::value_type>& entropy) const
{
    // Examples for bad encoding
    //
    // Ãïíßäçò ÓôáìÜôçòÃïíßäçò Óôáìüôçò / ¼ëá Live CD2¼ëá Live CD2
    // ÄçìÞôñçò Ðáðáäçìçôñßïõ/Ëßíá Íéêïëáêïðïýëïõ/Åëåõèåñßá ÁñâáíéôÜêçÄçìþôñçò Ðáðáäçìçôñßïõ/ëßíá Íéêïëáêïðïýëïõ/åëåõèåñßá Áñâáíéôüêç / Áíáóôáóßá
    // ¤[¥ÛÅý¤[¥ûåý / Kids Return - Ãa«Ä¤lªº¤ÑªÅKids Return
    // µØ¯Ç¸s¬Pµø¯ç¸s¬p / µØ¯Ç91ª÷¦±¨µÂ§¤§¤@µø¯ç91ª÷¦±¨µâ§¤§¤@
    // ¥L¦b¨º¸Ì¥l¦b¨º¸ì
    // ÃÓµúÅïÃóµúåï / ¾y¤O¤dÁHºt°Û·|[Disc 1][¶Ì«Â¾ã²z]¾y¤o¤dáhºt°û·|[Disc 1][¶ì«â¾ã²z]
    // Sega / ????????and???2005???????????
    // Various / Ï Ç×ÏÓ ÔÇÓ ÏÈÏÍÇÓ CD 2Ï Ç×ïó Ôçó Ïèïíçó Cd 2
    // Í¯ÀöÍ¯àö / Ò»ÌýÖÓÇé-ÊÔÒôÍ¯ÀöDSD.×¨¼­Ò»ìýöóçé-êôòôí¯àödsd.×¨¼­
    // Various / 77 ëó÷øèõ ïåñåí äëÿ äåòåé, CD177 ëó÷øèõ ïåñåí äëÿ äåòåé, CD1

    m_bad_encoding = true;

    std::size_t ct = 0;
    // these are illegal unicode / ISO8859-1 values
    for (int ch = 0; ch < 0x20; ++ch) if (entropy.has_value(ch)) ++ct;
    // these are illegal unicode / ISO8859-1 values
    for (int ch = 0x7f; ch < 0xa0; ++ch) if (entropy.has_value(ch)) ++ct;
    // these are _very_ uncommon unicode / ISO8859-1 values
    for (int ch = 0xa0; ch < 0xc0; ++ch) if (entropy.has_value(ch)) ++ct;
    if (ct > 4) return;
    // not deleting ct here on purpose - let it add up with the next character class
    // these are unicode / ISO8859-1 values that should not make the majority of a string
    for (int ch = 0xc0; ch < 0x100; ++ch) if (entropy.has_value(ch)) ++ct;
    if (ct > entropy.size() / 3) return;

    m_bad_encoding = false;
}

void DiskRecord::calc_entropy() const
{
    if (m_entropy) return;
    Entropy<std::wstring::value_type> entropy;
    std::wstring wide;
    Unicode::from_utf8(m_title, wide);
    entropy += wide;
    wide.clear();
    Unicode::from_utf8(m_artist, wide);
    entropy += wide;
    for (auto& s : m_songs) {
        wide.clear();
        Unicode::from_utf8(s, wide);
        entropy += wide;
    }

    // store entropy
    m_entropy = entropy.size();

    // and count of characters
    m_charcount = entropy.count();

    // check for bad encoding
    calc_bad_encoding(entropy);
}

void DiskRecord::cleanup()
{
    // get rid of common reasons for duplicate entries:

    // 1) Artist named in front of the track titles again
    std::string begin = m_artist + " / ";
    for (auto& song : m_songs) {
        if (begins_with(song, begin)) song.erase(0, begin.size());
    }

    // 2) multiple spaces used instead of one, leading and trailing spaces
    trim_all(m_artist);
    trim_all(m_title);
    trim_all(m_genre);
    for (auto& song : m_songs) {
        trim_all(song);
    }

    // 3) uppercase used instead of mixed case
    // and calc entropy as we have to convert to wide string in the process anyway
    Entropy<std::wstring::value_type> entropy;
    convert_to_titlecase(m_artist, &entropy);
    convert_to_titlecase(m_title, &entropy);
    convert_to_titlecase(m_genre);
    for (auto& song : m_songs) {
        convert_to_titlecase(song, &entropy);
    }

    // store entropy
    m_entropy = entropy.size();

    // and count of characters
    m_charcount = entropy.count();

    // check for bad encoding
    calc_bad_encoding(entropy);
}

static const wchar_t substmap[] = {
    0x41, // As -> A
    0x41, // As -> A
    0x41, // As -> A
    0x41, // As -> A
    0x41, // Ä  -> A
    0x41, // As -> A
    0x41, // As -> A
    0x43, // C cedille -> C
    0x45, // Es -> E
    0x45, // Es -> E
    0x45, // Es -> E
    0x45, // Es -> E
    0x49, // Is -> I
    0x49, // Is -> I
    0x49, // Is -> I
    0x49, // Is -> I
    0x44, // D  -> D
    0x4e, // Ñ  -> N
    0x4f, // Os -> O
    0x4f, // Os -> O
    0x4f, // Os -> O
    0x4f, // Os -> O
    0x4f, // Ö  -> O
    0x20, // x  -> space
    0x4f, // Os -> O
    0x55, // Us -> U
    0x55, // Us -> U
    0x55, // Us -> U
    0x55, // Ü  -> U
    0x59, // Y -> Y
    0x20, //   -> space
    0x20, // ß -> space
    0x61, // as -> a
    0x61, // as -> a
    0x61, // as -> a
    0x61, // as -> a
    0x61, // ä  -> a
    0x61, // as -> a
    0x61, // as -> a
    0x63, // c cedille -> c
    0x65, // es -> e
    0x65, // es -> e
    0x65, // es -> e
    0x65, // es -> e
    0x69, // is -> i
    0x69, // is -> i
    0x69, // is -> i
    0x69, // is -> i
    0x64, // d -> d
    0x6e, // ñ -> n
    0x6f, // os -> o
    0x6f, // os -> o
    0x6f, // os -> o
    0x6f, // os -> o
    0x6f, // ö  -> o
    0x20, //    -> space
    0x6f, // os -> o
    0x75, // us -> u
    0x75, // us -> u
    0x75, // us -> u
    0x75, // ü  -> u
    0x79, // y -> y
    0x20, //   -> space
    0x79  // y -> y
};

static inline void normalize_accented_chars(std::wstring::value_type& ch)
{
    if (ch >= 0xC0 && ch <= 0xFF) {
        ch = substmap[ch - 0xC0];
    }
}

template <class String>
String normalize_(const std::string& str)
{
    std::wstring wide;
    Unicode::from_utf8(str, wide);
    String norm;
    for (auto ch : wide) {
        if (ch < '0') continue;
        if (ch > '9' && ch < 'A') continue;
        normalize_accented_chars(ch);
        if (std::iswpunct(ch)) continue;
        if (std::iswcntrl(ch)) continue;
        if (std::iswspace(ch)) continue;
        if (std::iswupper(ch)) ch = std::towlower(ch);
        if (ch > 'Z' && ch < 'a') continue;
        if (ch > 'z' && ch < 0xC0) continue;
        if (sizeof(typename String::value_type) == 1) Unicode::to_utf8(ch, norm);
        else norm += ch;
    }
    return norm;
}

std::string DiskRecord::normalize(const std::string& str)
{
    return normalize_<std::string>(str);
}

std::wstring DiskRecord::wnormalize(const std::string& str)
{
    return normalize_<std::wstring>(str);
}

uint16_t DiskRecord::compare(const std::wstring& left, const std::wstring& right)
{
    return NGrams<3, false>::compare(left, right);
}

uint16_t DiskRecord::compare(const std::string& left, const std::string& right)
{
    std::wstring wleft, wright;
    Unicode::from_utf8(left, wleft);
    Unicode::from_utf8(right, wright);
    return compare(wleft, wright);
}

uint16_t DiskRecord::compare_normalized(const std::string& left, const std::string& right)
{
    return compare(wnormalize(left), wnormalize(right));
}

std::string DiskRecord::cddb_file() const
{
    frame_t frames = m_frames;
    uint32_t seconds = convert_frame_lengths_in_frame_starts(m_seconds, frames);

    std::string file;
    file.reserve(1500);

    file  = "# xmcd 2.0 CD database file\n";
    file += "#\n";
    file += "# Track frame offsets:\n";
    for (const auto& frame : frames) file += fmt::format("#       {0}\n", frame);
    file += "#\n";
    file += fmt::format("# Disc length: {0} seconds\n", seconds);
    file += "#\n";
    file += fmt::format("# Revision: {0}\n", m_revision);
    file += "# Submitted via: xmcd 2.0\n";
    file += "#\n";
    file += fmt::format("DISCID={0:x}\n", m_discid);
    file += fmt::format("DTITLE={0} / {1}\n", m_artist, m_title);
    file += "DYEAR=";
    if (m_year) file += std::to_string(m_year);
    file += '\n';
    file += fmt::format("DGENRE={0}\n", m_genre);
    int tct = 0;
    for (const auto& song : m_songs) file += fmt::format("TTITLE{0}={1}\n", tct++, song);
    file += "EXTD=\n";
    for (std::size_t tct = 0; tct < m_songs.size(); ++tct) file += fmt::format("EXTT{0}=\n", tct);
    file += "PLAYORDER=\n";

    return file;
}

bool DiskRecord::equal_strings(const DiskRecord& other) const
{
    return artist() == other.artist()
    && title() == other.title()
    && songs() == other.songs();
}

bool compare_lower(const std::string& left, const std::string& right)
{
    std::wstring wl;
    std::wstring wr;
    Unicode::from_utf8(left, wl);
    Unicode::from_utf8(right, wr);
    for (auto& ch : wl) if (std::iswupper(ch)) ch = std::towlower(ch);
    for (auto& ch : wr) if (std::iswupper(ch)) ch = std::towlower(ch);
    return wl == wr;
}

bool DiskRecord::equal_lowercase_strings(const DiskRecord& other) const
{
    if (songs().size() != other.songs().size()) return false;
    if (!compare_lower(artist(), other.artist())) return false;
    if (!compare_lower(title(), other.title())) return false;
    for (std::size_t ct = 0; ct < songs().size(); ++ct) {
        if (!compare_lower(songs()[ct], other.songs()[ct])) return false;
    }
    return true;
}

