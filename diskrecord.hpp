//
//  diskrecord.hpp
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

#ifndef diskrecord_hpp_DSAHGFCVBUZTHFZDGHIUCJZT
#define diskrecord_hpp_DSAHGFCVBUZTHFZDGHIUCJZT

#include <vector>
#include <string>
#include <cinttypes>
#include "cddbdefines.hpp"


namespace CDDB {

class DiskRecord {
public:
    typedef std::vector<std::string> list_t;
    typedef uint32_t discid_t;
    typedef std::vector<uint32_t> frame_t;

    DiskRecord(const std::vector<char>& data);
    DiskRecord(uint32_t discid
               , std::string&& artist
               , std::string&& title
               , uint16_t year
               , std::string&& genre
               , list_t&& songs
               , frame_t&& frames
               , uint16_t revision
               , uint32_t seconds);
    ~DiskRecord() {}

    discid_t discid() const { if (!m_discid_ready) calc_discid(); return m_discid; }
    discid_t fuzzy_discid() const { if (!m_fuzzy_discid_ready) calc_fuzzy_discid(); return m_fuzzy_discid; }
    const std::string& artist() const { return m_artist; }
    const std::string& title() const { return m_title; }
    uint16_t year() const { return m_year; }
    const std::string& genre() const { return m_genre; }
    const list_t& songs() const { return m_songs; }
    const frame_t& frames() const { return m_frames; }
    uint32_t seconds() const { return m_seconds; }
    uint16_t revision() const { return m_revision; }
    uint32_t hash() const { if (!m_hash_ready) calc_hash(); return m_hash; }
    uint32_t normalized_hash() const { if (!m_normalized_hash_ready) calc_normalized_hash(); return m_normalized_hash; }
    bool valid() const { return m_valid; }
    std::size_t entropy() const { if (!m_entropy) calc_entropy(); return m_entropy; }
    std::size_t charcount() const { if (!m_entropy) calc_entropy(); return m_charcount; }
    std::size_t charcount_upper() const { if (!m_entropy) calc_entropy(); return m_uppercasecount; }
    bool bad_encoding() const { if (!m_entropy) calc_entropy(); return m_bad_encoding; }
    std::string cddb_file() const;

    static std::string normalize(const std::string& str);
    static std::wstring wnormalize(const std::string& str);
    static uint16_t compare(const std::wstring& left, const std::wstring& right);
    static uint16_t compare(const std::string& left, const std::string& right);
    static uint16_t compare_normalized(const std::string& left, const std::string& right);

    bool equal_strings(const DiskRecord& other) const;
    bool equal_lowercase_strings(const DiskRecord& other) const;

private:
    void add_keyvalue(const std::string& key, std::string& value);
    void add_comment(std::string& value);
    void cleanup();
    void calc_entropy() const;
    void calc_hash() const;
    void calc_normalized_hash() const;
    void calc_discid() const;
    void calc_fuzzy_discid() const;
    void calc_bad_encoding(const Entropy<std::wstring::value_type>& entropy) const;
    void verify_record();
    bool mostly_uppercase(const std::wstring str);
    void convert_to_titlecase(std::string& value, Entropy<std::wstring::value_type>* entropy = nullptr);

    mutable discid_t m_discid = 0;
    mutable discid_t m_fuzzy_discid = 0;
    std::string m_artist;
    std::string m_title;
    uint16_t m_year = 0;
    std::string m_genre;
    list_t m_songs;
    frame_t m_frames;
    uint16_t m_revision = 0;
    uint32_t m_seconds = 0;
    mutable uint32_t m_hash = 0;
    mutable uint32_t m_normalized_hash = 0;
    mutable std::size_t m_entropy = 0;
    mutable std::size_t m_charcount = 0;
    mutable std::size_t m_uppercasecount = 0;
    mutable bool m_bad_encoding = false;

    mutable bool m_hash_ready = false;
    mutable bool m_normalized_hash_ready = false;
    mutable bool m_discid_ready = false;
    mutable bool m_fuzzy_discid_ready = false;

    list_t::size_type m_list_base = 0;
    bool m_valid = false;
    bool m_read_tracks = false;
};

}

#endif /* diskrecord_hpp */
