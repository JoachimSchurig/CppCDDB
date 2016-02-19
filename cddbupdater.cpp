//
//  cddb.cpp
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

#include "cddbupdater.hpp"
#include "cddbexception.hpp"
#include "untar.hpp"
#include "diskrecord.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include "format.hpp"
#include "utf8.hpp"



using namespace CDDB;


SchemaInit::SchemaInit(const std::string& dbname)
{
    // create database if not exists, and set busy timeout to 100ms
    SQLite::Database sql(dbname, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 100);

    if (!sql.tableExists("CD")) {
        sql.exec("CREATE TABLE CD (cd INTEGER PRIMARY KEY, artist TEXT, title TEXT, genre INTEGER, year INTEGER, seconds INTEGER, revision INTEGER, tracks INTEGER)");
        sql.exec("CREATE TABLE NAMEHASH (hash INTEGER PRIMARY KEY, cd INTEGER)");
        sql.exec("CREATE TABLE TRACKS (cd INTEGER, track INTEGER, song TEXT, frames INTEGER)");
        sql.exec("CREATE TABLE DISCID (discid INTEGER, cd INTEGER)");
        sql.exec("CREATE TABLE FUZZYID (fuzzyid INTEGER, cd INTEGER)");
        sql.exec("CREATE TABLE GENRE (id INTEGER PRIMARY KEY, name TEXT)");
        sql.exec("CREATE TABLE ERRORS (reason TEXT, extended TEXT, file TEXT)");
        sql.exec("CREATE INDEX track_cd_idx ON TRACKS (cd)");
        sql.exec("CREATE INDEX discid_id_idx ON DISCID (discid)");
        sql.exec("CREATE INDEX fuzzyid_id_idx ON FUZZYID (fuzzyid)");
    }
}


CDDBSQLUpdater::CDDBSQLUpdater(const std::string& dbname)
: m_schema(dbname)
, m_sql(dbname, SQLITE_OPEN_READWRITE, 100) // set busy timeout to 100ms
, m_genres(m_sql, "GENRE")
, qcd       (m_sql, "INSERT INTO CD (artist, title, genre, year, seconds, revision, tracks) VALUES (?1,?2,?3,?4,?5,?6,?7)")
, qupdatecd (m_sql, "UPDATE CD SET artist=?2, title=?3, genre=?4, year=?5, seconds=?6, revision=?7, tracks=?8 WHERE cd=?1")
, qcd2      (m_sql, "SELECT cd, artist, title, genre, year, seconds, revision FROM CD WHERE cd=?1")
, qtracks   (m_sql, "INSERT INTO TRACKS (cd, track, song, frames) VALUES (?1,?2,?3,?4)")
, qdiscid   (m_sql, "INSERT INTO DISCID (discid, cd) VALUES (?1,?2)")
, qfdiscid  (m_sql, "INSERT INTO FUZZYID (fuzzyid, cd) VALUES (?1,?2)")
, qscrc     (m_sql, "SELECT cd FROM NAMEHASH WHERE hash=?1")
, qdhash    (m_sql, "SELECT cd FROM DISCID WHERE discid=?1")
, qicrc     (m_sql, "INSERT INTO NAMEHASH (hash, cd) VALUES (?1,?2)")
, qcoll1    (m_sql, "SELECT seconds FROM CD WHERE cd=?1")
, qcoll2    (m_sql, "SELECT song, frames FROM TRACKS WHERE cd=?1 ORDER BY track ASC")
, qcoll3    (m_sql, "SELECT artist, title, revision FROM CD WHERE cd=?1")
, qupdatetracks(m_sql, "UPDATE TRACKS SET song=?3, frames=?4 WHERE cd=?1 AND track=?2")
, qdelcd    (m_sql, "DELETE FROM CD WHERE cd=?1")
, qdeltracks(m_sql, "DELETE FROM TRACKS WHERE cd=?1")
, qdelhash  (m_sql, "DELETE FROM NAMEHASH WHERE hash=?1")
, qerror    (m_sql, "INSERT INTO ERRORS (reason, extended, file) VALUES (?1,?2,?3)")
{
}

void CDDBSQLUpdater::error(const std::string& error, const std::string& exterror, UnTar::buf_t& data)
{
    data.push_back(0);
    qerror.bind(1, error);
    qerror.bind(2, exterror);
    qerror.bind(3, &data[0]);
    qerror.exec();
    qerror.reset();
}

uint32_t CDDBSQLUpdater::check_title_hash(uint32_t hash)
{
    // check if we know that CD already by its CRC across title/tracks (as opposed to the discid)

    qscrc.bind(1, int64_t(hash));
    int32_t cdid = 0;
    // do we have a result?
    if (qscrc.executeStep()) {
        cdid = static_cast<uint32_t>(qscrc.getColumn(0).getInt64());
    }
    qscrc.reset();

    return cdid;
}

uint32_t CDDBSQLUpdater::write_record(const DiskRecord& rec, bool check_hash)
{
    // convert the genre string to an int
    int64_t genre = m_genres.map(m_sql, rec.genre());

    // write new CD to sql
    qcd.bind(1, rec.artist());
    qcd.bind(2, rec.title());
    qcd.bind(3, genre);
    qcd.bind(4, rec.year());
    qcd.bind(5, int64_t(rec.seconds()));
    qcd.bind(6, rec.revision());
    qcd.bind(7, int64_t(rec.songs().size()));
    // and execute
    qcd.exec();
    // get the id of the last written row
    uint32_t cdid = static_cast<uint32_t>(m_sql.getLastInsertRowid());
    // and reset the Statement for the next round
    qcd.reset();

    if (!check_hash || !check_title_hash(rec.normalized_hash())) {
        // write the hash record
        qicrc.bind(1, int64_t(rec.normalized_hash()));
        qicrc.bind(2, int64_t(cdid));
        qicrc.exec();
        qicrc.reset();
    }

    // now write all the songs of a disc
    int tct = 0;
    for (const auto& song : rec.songs()) {
        qtracks.bind(1, int64_t(cdid));
        qtracks.bind(2, tct);
        qtracks.bind(3, song);
        if (rec.frames().empty()) qtracks.bind(4, 0);
        else qtracks.bind(4, int64_t(rec.frames()[tct]));
        qtracks.exec();
        qtracks.reset();
        ++tct;
    }

    ++m_rep.added;

    return cdid;
}

void CDDBSQLUpdater::update_record(uint32_t cdid, const DiskRecord& rec)
{
    // convert the genre string to an int
    int64_t genre = m_genres.map(m_sql, rec.genre());

    // update CD to sql
    qupdatecd.bind(1, int64_t(cdid));
    qupdatecd.bind(2, rec.artist());
    qupdatecd.bind(3, rec.title());
    qupdatecd.bind(4, genre);
    qupdatecd.bind(5, rec.year());
    qupdatecd.bind(6, int64_t(rec.seconds()));
    qupdatecd.bind(7, rec.revision());
    qupdatecd.bind(8, int64_t(rec.songs().size()));
    // and execute
    qupdatecd.exec();
    // and reset the Statement for the next round
    qupdatecd.reset();

    // now write all the songs of a disc
    int tct = 0;
    for (const auto& song : rec.songs()) {
        qupdatetracks.bind(1, int64_t(cdid));
        qupdatetracks.bind(2, tct);
        qupdatetracks.bind(3, song);
        if (rec.frames().empty()) qupdatetracks.bind(4, 0);
        else qupdatetracks.bind(4, int64_t(rec.frames()[tct]));
        qupdatetracks.exec();
        qupdatetracks.reset();
        ++tct;
    }

    ++m_rep.updated;
}

uint32_t CDDBSQLUpdater::check_discid(uint32_t discid)
{
    qdhash.bind(1, int64_t(discid));
    uint32_t ecd = 0;
    if (qdhash.executeStep()) {
        ecd = static_cast<uint32_t>(qdhash.getColumn(0).getInt64());
    }
    qdhash.reset();

    return ecd;
}

void CDDBSQLUpdater::write_discid(uint32_t discid, uint32_t cdid)
{
    qdiscid.bind(1, int64_t(discid));
    qdiscid.bind(2, int64_t(cdid));
    qdiscid.exec();
    qdiscid.reset();
}

void CDDBSQLUpdater::write_fuzzy_discid(uint32_t fuzzyid, uint32_t cdid)
{
    qfdiscid.bind(1, int64_t(fuzzyid));
    qfdiscid.bind(2, int64_t(cdid));
    qfdiscid.exec();
    qfdiscid.reset();
}

void CDDBSQLUpdater::delete_record(uint32_t cdid, uint32_t hashvalue)
{
    qdelcd.bind(1, int64_t(cdid));
    qdelcd.exec();
    qdelcd.reset();
    qdeltracks.bind(1, int64_t(cdid));
    qdeltracks.exec();
    qdeltracks.reset();
    qdelhash.bind(1, int64_t(hashvalue));
    qdelhash.exec();
    qdelhash.reset();

    --m_rep.added;
}

DiskRecord CDDBSQLUpdater::read_record(uint32_t cdid, uint32_t discid)
{
    qcd2.bind(1, int64_t(cdid));

    if (!qcd2.executeStep()) {
        throw std::runtime_error("cannot read record");
    }

    int32_t cd          = static_cast<uint32_t>(qcd2.getColumn(0).getInt64());
    std::string artist  = qcd2.getColumn(1).getText();
    std::string title   = qcd2.getColumn(2).getText();
    int32_t genre_id    = static_cast<uint32_t>(qcd2.getColumn(3).getInt64());
    int32_t year        = static_cast<uint32_t>(qcd2.getColumn(4).getInt64());
    int32_t seconds     = static_cast<uint32_t>(qcd2.getColumn(5).getInt64());
    int32_t revision    = static_cast<uint32_t>(qcd2.getColumn(6).getInt64());

    qcd2.reset();

    std::string genre   = m_genres.map(genre_id);

    std::vector<std::string> songs;
    std::vector<uint32_t> frames;

    qcoll2.bind(1, cd);
    while (qcoll2.executeStep()) {
        songs.push_back(qcoll2.getColumn(0).getText());
        frames.push_back(static_cast<uint32_t>(qcoll2.getColumn(1).getInt64()));
    }
    qcoll2.reset();

    DiskRecord rec(discid, std::move(artist), std::move(title), year, std::move(genre), std::move(songs), std::move(frames), revision, seconds);
    return rec;
}

void CDDBSQLUpdater::import(const std::string& importfile, bool initial_import)
{
    m_rep.clear();
    
    Duration duration;

    // construct an untar object and tell it to use bz2 when the file has the
    // .bz2 suffix (it should always have..)
    UnTar tar(importfile, (importfile.rfind(".bz2") == importfile.length() - 4));

    m_sql.exec("PRAGMA synchronous=OFF");
    m_sql.exec("PRAGMA count_changes=OFF");
    m_sql.exec("PRAGMA journal_mode=MEMORY");
    m_sql.exec("PRAGMA temp_store=MEMORY");

    m_sql.exec("BEGIN TRANSACTION");

    if (initial_import) {
        m_sql.exec("DROP INDEX fuzzyid_id_idx");
    }

    UnTar::buf_t data;

    // get file after file
    while (tar.entry(data, TarHeader::File, true) != TarHeader::Unknown) {

        if (m_rep.rct && m_rep.rct % 100000 == 0) {
            duration.lap();
            std::cout << fmt::format("{0} - records read: {1}, rps: {2}",
                                     duration.to_string(Duration::Precision::Seconds),
                                     m_rep.rct,
                                     (100000*1000) / (duration.get_lap(Duration::Precision::Milliseconds)))
            << std::endl;
        }

        ++m_rep.rct;
        m_rep.bct += data.size();

        // following here is handling of normal files

        // construct DiskRecord from the data
        DiskRecord rec(data);

        // check if the record contains plausible data
        if (!rec.valid()) {

            if (m_debug) {
                std::string exterr = rec.artist() + " / " + rec.title();
                error("INVALID", exterr, data);
            }

            ++m_rep.frct;
            continue;
        }

        bool record_written = false;

        uint32_t cdid = check_title_hash(rec.normalized_hash());

        if (!cdid) {

            // this is a new record, write it
            cdid = write_record(rec, false);
            record_written = true;

        } else {

            ++m_rep.dcrcct;

            if (m_debug) {
                // this CD CRC is already known. For debug purposes, let's store them
                // to find out if they are legitimately so, or CRC collisions
                // (investigations showed they are legitimate dupes, but with differing discids due to
                // slightly different track offsets..)
                std::string exterr = fmt::format("hash duplicate: {0}", rec.normalized_hash());
                error("HASHDUP", exterr, data);
            }

            // on purpose, fall through to writing the discid links -
            // all needed data is valid: the cdid, and the rec.discid() is actually a new
            // valid discid for that already known cdid
            
        }
        
        // now write the discid link(s)
        {

            bool discid_valid = true;

            uint32_t ecd = check_discid(rec.discid());

            if (ecd) {

                // trouble - the discid is already known
                //
                // check if it is some sort of a collision
                //  a. hash collision, where different frame lengths yield the same hash value
                //  b. "real world" collision, where different CDs yield the same frame lengths
                //      (in this case we can not do a lot to resolve it automatically)
                //  c. actually same discid pointing to the same CD, which simply means that we
                //      had undetected dupes in the original CDDB database
                //
                // Case a. concerns about one record in 2800 with FNV hash computation on frames
                //      (which is really good, the original discid algorithm has a collision of
                //      one record in 3 (which renders it unusable if it were not checking for
                //      the frame lengths after fetching all the duplicate CDID records).
                // Case b. concerns about one in 146 records, in which the user needs to pick
                //      the right disc.
                // Case c. is the most frequent one (about one in 23 records). In most cases,
                // those are duplicates due to improved text content (like accents in titles, etc.)
                //
                // For case c. we should then check if the new version is preferrable over the existing
                // version (higher revision, or if equal revision higher entropy value), and update if it is.

                // for now - we check later down if we still want to write it
                discid_valid  = false;

                // now check if this really is a collision, that is, the
                // track sequences of the existing cd are different

                DiskRecord existing_rec = read_record(ecd, rec.discid());

                bool same_frames = existing_rec.seconds() == rec.seconds() && existing_rec.frames() == rec.frames();

                // now check if this is actually the same CD (by comparing the disc artist and title)
                bool same_title = (DiskRecord::compare_normalized(existing_rec.artist() + existing_rec.title(), rec.artist() + rec.title()) >= 25
                             || DiskRecord::compare_normalized(existing_rec.artist(), rec.artist()) >= 25
                             || DiskRecord::compare_normalized(existing_rec.title(), rec.title()) >= 25);

                if (!same_frames) {

                    // write the discid link to the CD. It is a collision, the user will have to pick the right choice.
                    discid_valid = true;

                    if (same_title) ++m_rep.realcddidcollct;
                    else ++m_rep.realdidcollct;

                    if (m_debug) {
                        std::string exterr = fmt::format("discid {0}, cd {1}, {2} / {3} - {4} / {5}",
                                                         rec.discid(), ecd, rec.artist(), rec.title(),
                                                         existing_rec.artist(), existing_rec.title());
                        if (same_title) error("SAMECDDID", exterr, data);
                        else error("SAMEDID", exterr, data);
                    }

                } else {

                    // same frames ->

                    std::string add_reason;

                    if (same_title) {

                        bool update_with_this = false;

                        ++m_rep.samecdframesct;
                        add_reason += "_REQ";

                        // now compare entropy - higher entropy is an indicator for more information and more
                        // accurate code points (think of accented chars vs. ASCII)
                        
                        if (rec.entropy() > existing_rec.entropy()) {

                            ++m_rep.entropy_gt;
                            add_reason += "_EGT";
                            // update the existing record with this one, and remove the record if we had written one
                            update_with_this = true;

                        }
                        else if (rec.entropy() == existing_rec.entropy()) {

                            // now check if the strings are EXACTLY the same
                            if (rec.equal_strings(existing_rec)) {

                                ++m_rep.duplicate;
                                add_reason += "_DUP";
                                // skip this, and remove the record if we had written one (not very probable)

                            } else if (rec.equal_lowercase_strings(existing_rec)) {

                                ++m_rep.duplicate_lower;
                                add_reason += "_DLP";

                                // check which of the strings contains more uppercase characters (which, if they
                                // are not all uppercase, is normally an indication of a more accurate record)

                                if (rec.charcount_upper() > existing_rec.charcount_upper()) {
                                    ++m_rep.upper_count_gt;
                                    // update the existing record with this one, and remove the record if we had written one
                                    update_with_this = true;
                                } else {
                                    ++m_rep.upper_count_eqlt;
                                }

                            } else {

                                // now check which one contains more characters (which we take as an indication
                                // of more complete information)
                                
                                if (rec.charcount() > existing_rec.charcount()) {
                                    ++m_rep.overall_count_gt;
                                    // update the existing record with this one, and remove the record if we had written one
                                    update_with_this = true;
                                } else {
                                    ++m_rep.overall_count_eqlt;
                                }

                                ++m_rep.entropy_eq;
                                add_reason += "_EEQ";

                            }

                        }
                        else {

                            ++m_rep.entropy_lt;
                            add_reason += "_ELT";
                            // skip this, and remove the record if we had written one (not very probable)

                        }

                        if (record_written) delete_record(cdid, rec.normalized_hash());
                        if (update_with_this) update_record(ecd, rec);

                    } else {
                        ++m_rep.sameframesct;
                    }

                    if (m_debug) {
                        std::string exterr = fmt::format("discid {0}, cd {1}, {2} / {3} - {4} / {5}",
                                                         rec.discid(), ecd, rec.artist(), rec.title(),
                                                         existing_rec.artist(), existing_rec.title());
                        if (same_title) error(std::string("SAMECDFRAMES") + add_reason, exterr, data); // these are duplicate CD titles (well, they vary slightly, but mean the same CD)
                        else error("SAMEFRAMES", exterr, data); // these are really same frames, but not same CDs
                    }
                    
                }

            }

            if (discid_valid) {
                write_discid(rec.discid(), cdid);
                write_fuzzy_discid(rec.fuzzy_discid(), cdid);
            }
        }
    }

    duration.lap();
    std::cout << fmt::format("{0} - records read: {1}, rps: {2}",
                             duration.to_string(Duration::Precision::Seconds),
                             m_rep.rct,
                             (m_rep.rct*1000) / (duration.get(Duration::Precision::Milliseconds)))
        << std::endl;

    std::cout << m_rep.to_string();

    if (initial_import) {
        Duration idxduration;
        m_sql.exec("CREATE INDEX fuzzyid_id_idx ON FUZZYID (fuzzyid)");
        idxduration.lap();
        std::cout << fmt::format("index creation took {0}", idxduration.to_string(Duration::Precision::Milliseconds)) << std::endl;
    }

    m_sql.exec("COMMIT TRANSACTION");

    duration.lap();

    std::cout << fmt::format("total time used: {0}", duration.to_string(Duration::Precision::Milliseconds)) << std::endl;
}

std::string CDDBSQLUpdater::report_t::to_string()
{
    std::string report;
    report = fmt::format("Read {0} bytes, {1} records, {2} invalid, {3} duplicate crc, {4} links\n"
                         ,bct ,rct ,frct ,dcrcct ,lct);
    report += fmt::format("Hash collisions (differing frames): same title: {0}, {1} real collisions\n"
                          ,realdidcollct ,realcddidcollct);
    report += fmt::format("Frame duplicates: {0}\n", sameframesct);
    report += fmt::format("Duplicate CD records: {0}\n", samecdframesct);
    report += fmt::format("    Entropy: gt {0}, eq {1}, lt {2}\n", entropy_gt, entropy_eq, entropy_lt);
    report += fmt::format("        Same strings: {0}, lowercase {1}\n", duplicate, duplicate_lower);
    report += fmt::format("            Uppercase chars: gt {0}, eq/lt {1}\n", upper_count_gt, upper_count_eqlt);
    report += fmt::format("        Total chars: gt {0}, eq/lt {1}\n", overall_count_gt, overall_count_eqlt);
    report += fmt::format("    Used for updates: {0}\n", entropy_gt + upper_count_gt + overall_count_gt);
    report += fmt::format("Total: added {0} CDs, updated {1} CDs\n", added, updated);
    return report;
}
