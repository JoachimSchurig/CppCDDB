//
//  cddbserver.cpp
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

#include <iostream>
#include <vector>
#include <unordered_map>

#include "cddbserver.hpp"
#include "cddbexception.hpp"
#include "helper.hpp"
#include "format.hpp"
#include "diskrecord.hpp"



using namespace CDDB;



CDDBSQLServer::CDList::CDList(SQLite::Database& sql)
: m_query2(sql,  "SELECT artist, title, seconds, tracks FROM CD WHERE cd=?1")
, m_frames2(sql, "SELECT frames FROM TRACKS WHERE cd=?1 ORDER BY track ASC")
{
}

void CDDBSQLServer::CDList::sort()
{
    // sort by best frames match
    std::sort(cdvec.begin(), cdvec.end(), [](const cd_t& a, const cd_t& b)
              {
                  return a.diff < b.diff;
              });
}

bool CDDBSQLServer::CDList::has(uint32_t cdid) const
{
    auto it = std::find_if(cdvec.cbegin(), cdvec.cend(), [cdid](const cd_t& a)
                           {
                               return a.cd == cdid;
                           });
    return it != cdvec.cend();
}

bool CDDBSQLServer::CDList::get(uint32_t cdid, cd_t& cd)
{
    frames_t frames;
    m_frames2.bind(1, int64_t(cdid));
    while (m_frames2.executeStep()) {
        frames.push_back(static_cast<uint32_t>(m_frames2.getColumn(0).getInt64()));
    }
    m_frames2.reset();

    bool found = false;

    // now get the cd information
    m_query2.bind(1, int64_t(cdid));

    if (m_query2.executeStep()) {

        found = true;
        cd.cd = cdid;
        cd.artist = m_query2.getColumn(0).getText();
        cd.title = m_query2.getColumn(1).getText();
        cd.seconds = static_cast<uint32_t>(m_query2.getColumn(2).getInt64());
        cd.tracks = static_cast<uint32_t>(m_query2.getColumn(3).getInt64());
        cd.frames = std::move(frames);

    }
    m_query2.reset();

    return found;
}

bool CDDBSQLServer::CDList::add(uint32_t cdid)
{
    if (has(cdid)) return true;

    cd_t cd;

    if (!get(cdid, cd)) return false;
    cdvec.push_back(std::move(cd));
    return true;
}

bool CDDBSQLServer::CDList::add_if(uint32_t cdid, const frames_t& tracks, uint32_t max_trackdiff)
{
    if (has(cdid)) return true;

    cd_t cd;

    if (!get(cdid, cd)) return false;

    // do sanity checks

    if (cd.tracks != tracks.size()) return false;
    if (cd.frames.size() != tracks.size()) return false;

    // compare frames and drop if too different

    auto left = cd.frames.cbegin();
    auto right = tracks.cbegin();
    uint32_t diff = 0;

    for (; left != cd.frames.cend();) {
        auto v_left = *left;
        auto v_right = *right;
        uint32_t d;
        if (v_left > v_right) d = v_left - v_right;
        else d = v_right - v_left;
        if (d > max_trackdiff) return false;
        diff += d;
        ++left;
        ++right;
    }

    // note diff score for later sorting of all elements

    cd.diff = diff;

    // finally add to the list

    cdvec.push_back(std::move(cd));
    
    return true;
}


std::string CDDBSQLServer::cddb_query_by_discid(uint32_t discid, const frames_t& tracks, uint32_t seconds)
{
    // protect sql (or map) access by a lock
    std::lock_guard<std::mutex> lock(m_sqlmutex);

    CDList cdlist(m_sql);

    m_query.bind(1, int64_t(discid));
    while (m_query.executeStep()) {
        uint32_t cdid = static_cast<uint32_t>(m_query.getColumn(0).getInt64());
        cdlist.add_if(cdid, tracks, m_max_trackdiff);
    }
    m_query.reset();

    // sort by best match if there are multiple results
    cdlist.sort();

    std::string reply;

    if (cdlist.size() > 1) {

        reply = "210 Found exact matches, list follows (until terminating `.')\n";
        for (const auto& cd : cdlist) {
            reply += fmt::format("generic {0:x} {1} / {2}\n", discid, cd.artist, cd.title);
        }
        reply += ".\n";

    } else if (cdlist.size() == 1) {

        reply = fmt::format("200 generic {0:x} {1} / {2}\n", discid, cdlist.begin()->artist, cdlist.begin()->title);

    }
    
    return reply;
}

std::string CDDBSQLServer::cddb_query_by_fuzzy_discid(uint32_t discid, const frames_t& tracks, uint32_t seconds)
{
    // protect sql (or map) access by a lock
    std::lock_guard<std::mutex> lock(m_sqlmutex);

    CDList cdlist(m_sql);

    m_fquery.bind(1, int64_t(discid));
    while (m_fquery.executeStep()) {
        uint32_t cdid = static_cast<uint32_t>(m_fquery.getColumn(0).getInt64());
        cdlist.add_if(cdid, tracks, m_max_trackdiff);
    }
    m_fquery.reset();

    // sort by best match if there are multiple results
    cdlist.sort();

    std::string reply;

    if (!cdlist.empty()) {

        reply = "211 Found close matches, list follows (until terminating `.')\n";
        int ct = 0;
        for (const auto& it : cdlist) {
            // calculate private discid
            uint32_t discid = private_discid(it.seconds, it.frames);
            reply += fmt::format("generic {0:x} {1} / {2}\n", discid, it.artist, it.title);
            // only show the first some best matches if many
            if (++ct == 10) break;
        }
        reply += ".\n";

    }

    return reply;
}

std::string CDDBSQLServer::cddb_query(uint32_t discid, const frames_t& tracks, uint32_t seconds)
{
    // calculate private discid
    discid = private_discid(seconds, tracks);
    // try exact discid
    std::string reply = cddb_query_by_discid(discid, tracks, seconds);

    // try fuzzy discid if no result
    if (reply.empty()) {
        // calculate private fuzzy discid
        discid = private_fuzzy_discid(seconds, tracks);
        reply = cddb_query_by_fuzzy_discid(discid, tracks, seconds);
    }

    if (reply.empty()) reply = "202\n";

    return reply;
}

std::string CDDBSQLServer::build_cddb_file(uint32_t discid, const std::string& category)
{
    // protect sql (or map) access by a lock
    std::lock_guard<std::mutex> lock(m_sqlmutex);

    std::string file;

    m_qcd.bind(1, int64_t(discid));
    if (m_qcd.executeStep()) {

        int32_t cd          = static_cast<uint32_t>(m_qcd.getColumn(0).getInt64());
        std::string artist  = m_qcd.getColumn(1).getText();
        std::string title   = m_qcd.getColumn(2).getText();
        int32_t genre_id    = static_cast<uint32_t>(m_qcd.getColumn(3).getInt64());
        int32_t year        = static_cast<uint32_t>(m_qcd.getColumn(4).getInt64());
        int32_t seconds     = static_cast<uint32_t>(m_qcd.getColumn(5).getInt64());
        int32_t revision    = static_cast<uint32_t>(m_qcd.getColumn(6).getInt64());
        std::string genre   = m_genres.map(genre_id);

        std::vector<std::string> songs;
        std::vector<uint32_t> frames;

        m_qtracks.bind(1, cd);
        while (m_qtracks.executeStep()) {
            songs.push_back(m_qtracks.getColumn(0).getText());
            frames.push_back(static_cast<uint32_t>(m_qtracks.getColumn(1).getInt64()));
        }
        m_qtracks.reset();
        
        DiskRecord rec(discid, std::move(artist), std::move(title), year, std::move(genre),
                       std::move(songs), std::move(frames), revision, seconds);

        file = rec.cddb_file();
    }
    m_qcd.reset();

    return file;
}

std::string CDDBSQLServer::register_user(std::vector<std::string>::const_iterator it, std::vector<std::string>::const_iterator end)
{
    std::string reply;

    enum Data { User = 0, Host, Client, Version, End };
    std::vector<std::string> data;

    for (;it != end; ++it) data.emplace_back(*it);
    data.resize(End);

    reply = fmt::format("200 hello and welcome {0}@{1} running {2} {3}\n",
                        data[User], data[Host], data[Client], data[Version]);

    return reply;
}

std::string CDDBSQLServer::cddb_request(const std::string& qstr, Parameters& parameters)
{
    std::string reply;
    std::vector<std::string> words;

    CDDB::StringTokenizer<std::string> tokenizer(qstr, " \t\r\n");
    tokenizer.split(words);
    
    auto wordct = words.size();

    if (wordct) {
        tolower(words[0]);
        if (wordct > 1 && words[0] == "cddb") tolower(words[1]);
    }

    auto it = words.cbegin();

    if (wordct < 1) {

        if (!parameters.handshake) reply = "201 hostname C++CDDB server v1.0 ready at date\n";
        else {
            reply = "530\n";
            parameters.terminate = true;
        }

    } else {

        if (wordct > 1 && *it == "cddb") {

            ++it;

            if (*it == "hello") {

                // cddb hello username hostname clientname version

                reply = register_user(++it, words.cend());

                parameters.handshake = true;

            } else if (!parameters.handshake) {

                reply = "530 no handshake\n";
                parameters.terminate = true;

            } else {

                // here starts the main query parsing

                if (*it == "lscat") {

                    reply = "200 Okay category list follows (until terminating marker)\ngeneric\n.\n";
                }

                else if (*it == "query") {

                    if (wordct < 6) {

                        reply = "530 insufficient parameters\n";
                        parameters.terminate = true;

                    } else {

                        // cddb query discid ntrks off1 off2 ... nsecs

                        typedef std::vector<uint32_t> tracks_t;
                        tracks_t tracks;

                        uint32_t ntrks = static_cast<uint32_t>(std::stoul(words[3]));

                        if (ntrks && ntrks + 5 == wordct) {

                            for (tracks_t::size_type ct = 0; ct < ntrks; ++ct) {
                                tracks.emplace_back(std::stoul(words[4 + ct]));
                            }

                            uint32_t seconds = static_cast<uint32_t>(std::stoul(words[4 + ntrks]));

                            uint32_t discid = static_cast<uint32_t>(std::stoul(words[2], nullptr, 16));

                            seconds = convert_frame_starts_in_frame_lengths(seconds, tracks);

                            reply = cddb_query(discid, tracks, seconds);

                        } else {

                            reply = "530 track count does not match parameter count\n";
                            parameters.terminate = true;
                            
                        }


                    }

                }

                else if (*it == "read") {

                    if (wordct != 4) {

                        reply = "530 invalid parameter count\n";
                        parameters.terminate = true;

                    } else {

                        // cddb read categ discid
                        std::string rec = build_cddb_file(static_cast<uint32_t>(std::stoul(words[3], nullptr, 16)), words[2]);

                        if (!rec.empty()) {

                            reply = fmt::format("210 {0} {1}\n", words[2], words[3]);
                            reply += rec;
                            reply += ".\n";

                        } else {

                            reply = fmt::format("{0} {1} {2} No such CD entry in database.\n", 401, words[2], words[3]);
                            
                        }
                    }

                }

                else {

                    reply = "530 unsupported cddb command\n";
                    parameters.terminate = true;
                    
                }

            }
        }

        else if (*it == "hello") {

            // hello username hostname clientname version

            reply = register_user(++it, words.cend());

            parameters.handshake = true;
        }
        
        else if (!parameters.handshake) {

            reply = "530 no handshake\n";
            parameters.terminate = true;

        }

        else if (*it == "stat") {

            reply = "210 OK, status information follows (until terminating `.')\n";
            reply += "current proto: 6\n";
            reply += "max proto: 6\n";
            reply += "gets: no\n";
            reply += "updates: no\n";
            reply += "posting: no\n";
            reply += "quotes: no\n";
            reply += "current users: 1\n";
            reply += "max users: 1000\n";
            reply += "strip ext: yes\n";
            reply += "Database entries: 3565787\n";
            reply += ".\n";

        }
        
        else if (*it == "proto") {

            // proto [level]
            int level = 0;
            if (++it != words.end()) level = std::stoi(*it);
            if (level == 6) reply = "502 Protocol level already 6\n";
            else if (level > 0 && level != 6) reply = "501 Illegal protocol level\n";
            else reply = "200 CDDB protocol level: current 6, supported 6\n";

        }

        else if (*it == "ver") {

            reply = "200 hostname C++CDDB v1.0 (c) Joachim Schurig 2016.\n";

        }

        else if (*it == "quit") {

            reply = "230 hostname Closing connection. Goodbye.\n";
            parameters.terminate = true;
            
        }

        else {

            reply = "530 unsupported command\n";
            parameters.terminate = true;

        }

    }

    return reply;
}

inline uint16_t hex_digit(std::string::value_type ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';

    throw std::runtime_error("illegal hex character in query");
}

std::size_t split_http_cddb(const std::string qstr, std::vector<std::string>& svec)
{
    // GET /?cmd=cddb+query+6809330a+10+150+20753+41510+53268+75958+91735+103165+120710+144018+160108+2357&hello=joachim+client+cddb-tool+0.4.7&proto=6 HTTP/1.1

    enum State { Preamble, QueryCmd, QueryReplaceEqual, Query, HttpVersion };
    State state = Preamble;

    uint16_t hex_count = 0;
    uint16_t hex_char = 0;

    std::string query;

    for (auto ch : qstr) {
        switch (state) {
            case Preamble:
                if (ch == '?') state = QueryCmd;
                break;

            case QueryCmd:
                if (ch == '=') state = Query;
                break;

            case QueryReplaceEqual:
            case Query:
                if (hex_count) {
                    hex_char *= 16;
                    hex_char += hex_digit(ch);
                    if (--hex_count == 0) query += std::string::value_type(hex_char);
                }
                else if (ch == '%') {
                    hex_count = 2;
                    hex_char = 0;
                }
                else if (ch == '+') {
                    query += ' ';
                }
                else if (ch == '&') {
                    svec.push_back(std::move(query));
                    state = QueryReplaceEqual;
                }
                else if (ch == ' ') {
                    svec.push_back(std::move(query));
                    state = HttpVersion;
                }
                else if (ch == '=' && state == QueryReplaceEqual) {
                    query += ' ';
                    state = Query;
                }
                else {
                    query += ch;
                }
                break;

            case HttpVersion:
                break;
        }
    }

    if (hex_count) throw std::runtime_error("incomplete hex char");
    if (state != HttpVersion) throw std::runtime_error("malformed HTTP request");

    return svec.size();
}

std::string CDDBSQLServer::request(const std::string& qstr, ASIOServer::param_t parameters)
{
    param_t par = std::dynamic_pointer_cast<Parameters>(parameters);
    if (m_print_protocol) std::cerr << qstr << std::endl;

    if (CDDB::begins_with(qstr, "GET ")) {
        par->is_http = true;
        std::vector<std::string> cmds;
        if (split_http_cddb(qstr, cmds) == 3) {
            // parse the cddb hello
            cddb_request(cmds[1], *par);
            // parse the proto command
            cddb_request(cmds[2], *par);
            // and finally parse the query
            if (m_print_protocol) std::cerr << cmds[0] << std::endl;
            std::string cddbres = cddb_request(cmds[0], *par);
            std::string res;
            res.reserve(cddbres.size() + 80);
            res = fmt::format("HTTP/1.1 200 OK\r\nContent-Length: {0}\r\n\r\n", cddbres.size());
            res += cddbres;
            if (m_print_protocol) std::cerr << res << std::endl;
            return res;
        }
        throw std::runtime_error("invalid query");
    }
    else if (par->is_http) {
        // simply ignore all client headers
        return "";
    }

    // assume CDDB protocol
    return cddb_request(qstr, *par);
}

std::string CDDBSQLServer::init(ASIOServer::param_t parameters)
{
    // do not send the welcome message if we expect HTTP on this port (it would destroy the first HTTP response)
    if (m_expect_http) return std::string();
    else return request("", parameters);
}

CDDBSQLServer::CDDBSQLServer(const std::string& dbname, uint16_t port, bool expect_http, bool print_protocol, uint16_t max_trackdiff)
: ASIOServer(port)
, m_sql(dbname, SQLITE_OPEN_READONLY, 1000) // set busy timeout to 1000ms
, m_qcd(m_sql,     "SELECT CD.cd, CD.artist, CD.title, CD.genre, CD.year, CD.seconds, CD.revision"
                   " FROM DISCID,CD WHERE DISCID.discid=?1 AND CD.cd=DISCID.cd")
, m_qtracks(m_sql, "SELECT song, frames FROM TRACKS WHERE cd=?1 ORDER BY track ASC")
, m_query(m_sql,   "SELECT cd FROM DISCID WHERE discid=?1")
, m_fquery(m_sql,  "SELECT cd FROM FUZZYID WHERE fuzzyid=?1")
, m_frames(m_sql,  "SELECT cd FROM TRACKS WHERE frames>?1 AND frames<?2 AND track=?3")
, m_genres(m_sql,  "GENRE")
, m_expect_http(expect_http)
, m_print_protocol(print_protocol)
, m_max_trackdiff(max_trackdiff * 75)
{
}


