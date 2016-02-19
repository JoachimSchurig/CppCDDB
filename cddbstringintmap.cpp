//
//  ccdbstringintmap.cpp
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

#include "format.hpp"
#include "cddbstringintmap.hpp"
#include "cddbexception.hpp"


using namespace CDDB;

std::string StringIntMapCache::printall() const
{
    std::string ret;
    ret.reserve(m_map.size() * 15);
    for (const auto& it : m_map) {
        if (!it.first.empty()) ret += it.first + '\n';
    }
    return ret;
}

int64_t StringIntMapCache::map(const std::string& str) const
{
    const auto it = m_map.find(str);
    if (it != m_map.end()) return it->second;
    return 0;
}

StringIntMapCache::StringIntMapCache(SQLite::Database& sql, const std::string& tablename)
: m_insert_new(sql, fmt::format("INSERT INTO {0} (name) VALUES (?1)", tablename))
, m_find_id(sql, fmt::format("SELECT name FROM {0} WHERE id=?1", tablename))
{
    // prepare the query statement
    SQLite::Statement query(sql, fmt::format("SELECT * FROM {0}", tablename));

    // and loop until all results are fetched
    while (query.executeStep()) {
        m_map.insert(std::make_pair(query.getColumn(1).getText(), query.getColumn(0).getInt()));
    }
}

/// map() is currently not threadsafe. Add locks if you will access it from multiple threads!

int64_t StringIntMapCache::map(SQLite::Database& sql, const std::string& str)
{
    const auto it = m_map.find(str);
    if (it != m_map.end()) return it->second;

    m_insert_new.bind(1, str);

    // execute the prepared statement
    if (m_insert_new.exec() != 1) throw CDDBException("StringIntMap::map(): inserting new value failed");

    // query the last rowid
    int64_t id = sql.getLastInsertRowid();

    // reset the prepared query
    m_insert_new.reset();

    // insert rowid into map
    m_map.insert(make_pair(str, id));

    // and return the value
    return id;
}

std::string StringIntMapCache::map(int64_t id)
{
    std::string name;

    m_find_id.bind(1, id);

    if (m_find_id.executeStep()) {
        name = m_find_id.getColumn(0).getText();
    }

    m_find_id.reset();

    return name;
}


