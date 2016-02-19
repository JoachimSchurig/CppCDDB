//
//  cddbserver.hpp
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

#ifndef cddbserver_hpp_DUJHSDJVBJASCTSJZUKCHJDVJZDVUJSDHUVJ
#define cddbserver_hpp_DUJHSDJVBJASCTSJZUKCHJDVJZDVUJSDHUVJ

#include <string>
#include <mutex>

#include "sqlitecpp/SQLiteCpp.h"
#include "cddbstringintmap.hpp"
#include "asioserver.hpp"
#include "cddbdefines.hpp"


namespace CDDB {

class CDDBSQLServer : public ASIOServer {
public:
    CDDBSQLServer(const std::string& dbname, uint16_t port = 8880, bool expect_http = true, bool print_protocol = false, uint16_t max_trackdiff = 4);

protected:
    struct Parameters : public ASIOServer::Parameters {
        bool handshake = false;
        bool is_http = false;
    };
    typedef std::vector<uint32_t> frames_t;
    typedef std::shared_ptr<Parameters> param_t;
    
    virtual std::string init(ASIOServer::param_t parameters) override;
    virtual std::string request(const std::string& qstr, ASIOServer::param_t parameters) override;
    virtual ASIOServer::param_t get_parameters() override { return std::make_shared<Parameters>(); }

private:
    class CDList {
    public:
        struct cd_t {
            uint32_t cd = 0;
            std::string artist;
            std::string title;
            uint32_t seconds = 0;
            uint32_t tracks = 0;
            frames_t frames;
            uint32_t diff = 0;
        };
        typedef std::vector<cd_t> cdvec_t;

        CDList(SQLite::Database& sql);

        bool add(uint32_t cdid);
        bool add_if(uint32_t cdid, const frames_t& tracks, uint32_t max_trackdiff = 4 * 75);
        void clear() { cdvec.clear(); }
        void sort();
        bool empty() const { return cdvec.empty(); }
        std::size_t size() const { return cdvec.size(); }
        bool has(uint32_t cdid) const;

        cdvec_t::const_iterator cbegin() const { return cdvec.cbegin(); }
        cdvec_t::const_iterator cend() const { return cdvec.cend(); }
        cdvec_t::iterator begin() { return cdvec.begin(); }
        cdvec_t::iterator end() { return cdvec.end(); }

    private:
        bool get(uint32_t cdid, cd_t& cd);
        SQLite::Statement m_query2;
        SQLite::Statement m_frames2;
        cdvec_t cdvec;
    };

    SQLite::Database m_sql;
    SQLite::Statement m_qcd;
    SQLite::Statement m_qtracks;
    SQLite::Statement m_query;
    SQLite::Statement m_fquery;
    SQLite::Statement m_frames;
    StringIntMapCache m_genres;
    std::mutex m_sqlmutex;
    bool m_expect_http = true;
    bool m_print_protocol = false;
    uint32_t m_max_trackdiff = 4 * 75;

    CDDBSQLServer(const CDDBSQLServer&) = delete;
    CDDBSQLServer& operator=(const CDDBSQLServer&) = delete;

    std::string cddb_request(const std::string& qstr, Parameters& parameters);
    std::string build_cddb_file(uint32_t discid, const std::string& category);
    std::string cddb_query_by_discid(uint32_t discid, const frames_t& tracks, uint32_t seconds);
    std::string cddb_query_by_fuzzy_discid(uint32_t discid, const frames_t& tracks, uint32_t seconds);
    std::string cddb_query(uint32_t discid, const frames_t& tracks, uint32_t seconds);
    std::string register_user(std::vector<std::string>::const_iterator it, std::vector<std::string>::const_iterator end);
};


}

#endif /* cddbserver_hpp */
