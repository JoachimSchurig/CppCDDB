//
//  cddbupdater.hpp
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

#ifndef cddbupdater_hpp_ALIHKDYHJVDGSOICULKHAISOULKGZUCVTKADZBK
#define cddbupdater_hpp_ALIHKDYHJVDGSOICULKHAISOULKGZUCVTKADZBK

#include "sqlitecpp/SQLiteCpp.h"
#include "helper.hpp"
#include "cddbstringintmap.hpp"
#include "cddbdefines.hpp"
#include "diskrecord.hpp"
#include "untar.hpp"



namespace CDDB {

class SchemaInit {
public:
    SchemaInit(const std::string& dbname);
    ~SchemaInit() {}
};

class CDDBSQLUpdater {
public:
    CDDBSQLUpdater(const std::string& dbname);
    ~CDDBSQLUpdater() {}

    void import(const std::string& importfile, bool initial_import);
    void add_fuzzy_table();

private:
    struct report_t {
        // read count
        uint64_t rct = 0;
        // link count
        uint64_t lct = 0;
        // duplicate crc count
        uint64_t dcrcct = 0;
        // failed record count
        uint64_t frct = 0;
        // total unpacked bytes
        uint64_t bct = 0;
        // real discid collisions of the same CD, but with different frames
        uint64_t realcddidcollct = 0;
        // real discid collisions of the same CD
        uint64_t samecdframesct = 0;
        // real discid collisions
        uint64_t realdidcollct = 0;
        // real discid collisions
        uint64_t sameframesct = 0;
        // does the new record have higher entropy
        uint64_t entropy_gt = 0;
        // or same
        uint64_t entropy_eq = 0;
        // or lower
        uint64_t entropy_lt = 0;
        // records with exactly the same strings
        uint64_t duplicate = 0;
        uint64_t duplicate_lower = 0;
        // uppercase charcount
        uint64_t upper_count_gt = 0;
        uint64_t upper_count_eqlt = 0;
        // overall charcount
        uint64_t overall_count_gt = 0;
        uint64_t overall_count_eqlt = 0;
        // absolute counts
        uint64_t added = 0;
        uint64_t updated = 0;
        // clear counters
        void clear() { std::memset(this, 0, sizeof(report_t)); }
        std::string to_string();
    };

    SchemaInit m_schema;
    SQLite::Database m_sql;
    StringIntMapCache m_genres;
    report_t m_rep;

    SQLite::Statement qcd;
    SQLite::Statement qupdatecd;
    SQLite::Statement qcd2;
    SQLite::Statement qtracks;
    SQLite::Statement qdiscid;
    SQLite::Statement qfdiscid;
    SQLite::Statement qscrc;
    SQLite::Statement qdhash;
    SQLite::Statement qicrc;
    SQLite::Statement qcoll1;
    SQLite::Statement qcoll2;
    SQLite::Statement qcoll3;
    SQLite::Statement qupdatetracks;
    SQLite::Statement qdelcd;
    SQLite::Statement qdeltracks;
    SQLite::Statement qdelhash;
    SQLite::Statement qerror;

    bool m_debug = false;

    CDDBSQLUpdater(const CDDBSQLUpdater&) = delete;
    CDDBSQLUpdater& operator=(const CDDBSQLUpdater&) = delete;

    uint32_t check_title_hash(uint32_t hash);
    uint32_t write_record(const DiskRecord& rec, bool check_hash);
    uint32_t check_discid(uint32_t discid);
    void write_fuzzy_discid(uint32_t fuzzyid, uint32_t cdid);
    void write_discid(uint32_t discid, uint32_t cdid);
    DiskRecord read_record(uint32_t cdid, uint32_t discid);
    void update_record(uint32_t cdid, const DiskRecord& rec);
    void delete_record(uint32_t cdid, uint32_t hashvalue);
    void error(const std::string& error, const std::string& exterror, UnTar::buf_t& data);
};

}

#endif /* cddbupdater_hpp */
