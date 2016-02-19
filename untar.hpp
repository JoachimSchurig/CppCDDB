//
//  untar.hpp
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

#ifndef untar_hpp_DKHGJGCVHBNLMDSKVNJBHVSDHBJNKMADLMKLSDUBV
#define untar_hpp_DKHGJGCVHBNLMDSKVNJBHVSDHBJNKMADLMKLSDUBV

#include <cinttypes>
#include <vector>
#include <memory>
#include "unbzip2.hpp"


class TarHeader {
public:
    enum { HeaderLen = 512 };
    enum EntryType { Unknown = 0, File = 1, Directory = 2, Link = 4, Symlink = 8, Fifo = 16, Longname1 = 32, Longname2 = 64 };

    TarHeader() : m_keep_members_once(false) { clear(); }
    ~TarHeader() {}

    void clear();
    void reset();
    char* operator*() { return raw.header; }
    void analyze();
    bool is_end() const { return m_is_end; }
    EntryType type() const { return m_entrytype; }
    bool is_file() const { return type() == File; }
    bool is_directory() const { return type() == Directory; }
    const std::string& filename() const { return m_filename; }
    const std::string& linkname() const { return m_linkname; }
    size_t filesize() const { return m_file_size; }

private:
    /// the header structure
    union {
        struct {
            char header[HeaderLen];
        } raw;
        struct {
            char file_name[100];
            char mode[8];
            struct {
                char user[8];
                char group[8];
            } owner_ids;

            char file_bytes_octal[11];
            char file_bytes_terminator;
            char modification_time_octal[11];
            char modification_time_terminator;

            char checksum[8];

            union {
                struct {
                    char link_indicator;
                    char linked_file_name[100];
                } legacy;
                struct {
                    char type_flag;
                    char linked_file_name[100];
                    char indicator[6];
                    char version[2];
                    struct {
                        char user[32];
                        char group[32];
                    } owner_names;
                    struct {
                        char major[8];
                        char minor[8];
                    } device;
                    char filename_prefix[155];
                } ustar;
            } extension;
        } header;
    };

    uint64_t m_file_size;
    uint64_t m_modification_time;
    std::string m_filename;
    std::string m_linkname;
    bool m_is_end;
    bool m_is_ustar;
    EntryType m_entrytype;
    bool m_keep_members_once;
};


class UnTar {
public:
    typedef std::vector<char> buf_t;

    UnTar(const std::string& filename, bool use_bunzip = false);
    ~UnTar();

    /// simple interface: call for subsequent real files, with buf getting filled with the file's data
    /// returns false if end of archive
    bool file(std::string& name, buf_t& buf, bool skip_apple_resource_forks = false);

    /// extended interface, permitting to receive files, but also directory, link, and symlink entries from a tar archive
    /// returns type of the entry
    TarHeader::EntryType entry(buf_t& buf, int accepted_types = TarHeader::File, bool skip_apple_resource_forks = false);

    /// for the extended interface: get the current file type (after a call to entry() )
    TarHeader::EntryType type() const { return m_header.type(); }

    /// for the extended interface: get the current file name (after a call to entry(), and if the type is File, Link, or Symlink )
    const std::string& filename() const { return m_header.filename(); }

    /// for the extended interface: get the current link name (after a call to entry(), and if the type is Link or Symlink)
    const std::string& linkname() const { return m_header.linkname(); }

private:
    TarHeader m_header;
    std::unique_ptr<UnBZip2> m_bunzip;
    int m_fd;

    void read(void* buf, size_t len);
};



#endif /* untar_hpp */
