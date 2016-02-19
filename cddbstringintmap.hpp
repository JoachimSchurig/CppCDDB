//
//  ccdbstringintmap.hpp
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

#ifndef cddbstringintmap_hpp_SDKJHVDSJHBNKJHDSBSDLVKDSJVLDSKBHVJCDIU
#define cddbstringintmap_hpp_SDKJHVDSJHBNKJHDSBSDLVKDSJVLDSKBHVJCDIU

#include <unordered_map>
#include <string>
#include "sqlitecpp/SQLiteCpp.h"

namespace CDDB {

/// helper class to avoid joining all the time on the Category table,
/// and to automatically append to it when new values appear

class StringIntMapCache {
public:
    StringIntMapCache(SQLite::Database& sql, const std::string& tablename);
    virtual ~StringIntMapCache() {}
    int64_t map(SQLite::Database& sql, const std::string& str);
    std::string map(int64_t);
    std::string operator[](int64_t id) { return map(id); }
    std::string printall() const;
    int64_t map(const std::string& str) const;
    void load(SQLite::Database& sql, const std::string& tablename);
    void unload();

private:
    typedef std::unordered_map<std::string, int64_t> stringintmap_t;
    stringintmap_t m_map;
    SQLite::Statement m_insert_new;
    SQLite::Statement m_find_id;
};

}

#endif /* cddbstringintmap_hpp */
