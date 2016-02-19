//
//  untar.cpp
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



//  I learned about the tar format and the various representations in the wild
//  by reading through the code of the following fine projects:
//
//  https://github.com/abergmeier/tarlib
//
//  and
//
//  https://techoverflow.net/blog/2013/03/29/reading-tar-files-in-c/
//
//  The below code is another implementation, based on that information
//  but following neither of the above implementations in the resulting
//  code, particularly because this is a pure C++11 implementation for untar.
//  So please do not blame those for any errors this code may cause or have.


#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include "untar.hpp"
#include "helper.hpp"



class TarException : public std::runtime_error {
    using runtime_error::runtime_error;
};


void TarHeader::reset()
{
    if (!m_keep_members_once) {
        m_file_size = 0;
        m_modification_time = 0;
        m_filename.clear();
        m_linkname.clear();
        m_is_end = false;
        m_is_ustar = false;
        m_entrytype = Unknown;
    }
    else m_keep_members_once = false;
}

void TarHeader::clear()
{
    std::memset(raw.header, 0, HeaderLen);
    reset();
}

void TarHeader::analyze()
{
    // check for end header
    if (raw.header[0] == 0) {
        // check if full header is 0, then this is the end header of an archive
        m_is_end = true;
        for (auto ch : raw.header) {
            if (ch) {
                m_is_end = false;
                break;
            }
        }
        if (m_is_end) return;
    }

    // validate checksum
    {
        uint64_t header_checksum = std::strtoull(header.checksum, nullptr, 8);
        std::memset(header.checksum, ' ', 8);
        uint64_t sum = 0;
        for (auto ch : raw.header) sum += ch;
        if (header_checksum != sum) throw TarException("invalid header checksum");
    }

    m_is_ustar = (memcmp("ustar", header.extension.ustar.indicator, 5) == 0);

    // extract the filename
    m_filename.assign(header.file_name, std::min((size_t)100, ::strnlen(header.file_name, 100)));

    if (m_is_ustar) {
        // check if there is a filename prefix
        size_t plen = ::strnlen(header.extension.ustar.filename_prefix, 155);
        // yes, insert it before the existing filename
        if (plen) {
            m_filename = std::string(header.extension.ustar.filename_prefix, std::min((size_t)155, plen)) + "/" + m_filename;
        }
    }

    if (!m_filename.empty() && *m_filename.rbegin() == '/') {
        // make sure we detect also pre-1988 directory names :)
        if (!header.extension.ustar.type_flag) header.extension.ustar.type_flag = '5';
    }

    // now analyze the entry type

    switch (header.extension.ustar.type_flag) {
        case 0:
        case '0':
            // normal file
            if (m_entrytype == Longname1) {
                // this is a subsequent read on GNU tar long names
                // the current block contains only the filename and the next block contains metadata
                // set the filename from the current header
                m_filename.assign(header.file_name, std::min((size_t)HeaderLen, ::strnlen(header.file_name, HeaderLen)));
                // the next header contains the metadata, so replace the header before reading the metadata
                m_entrytype = Longname2;
                m_keep_members_once = true;
                return;
            }
            m_entrytype = File;

            header.file_bytes_terminator = 0;
            if ((header.file_bytes_octal[0] & 0x80) != 0) {
                m_file_size = std::strtoull(header.file_bytes_octal, nullptr, 256);
            } else {
                m_file_size = std::strtoull(header.file_bytes_octal, nullptr, 8);
            }
            
            header.modification_time_terminator = 0;
            m_modification_time = std::strtoull(header.modification_time_octal, nullptr, 8);
            break;

        case '1':
            // link
            m_entrytype = Link;
            m_linkname.assign(header.extension.ustar.linked_file_name, std::min((size_t)100, ::strnlen(header.file_name, 100)));
            break;

        case '2':
            // symlink
            m_entrytype = Symlink;
            m_linkname.assign(header.extension.ustar.linked_file_name, std::min((size_t)100, ::strnlen(header.file_name, 100)));
            break;

        case '5':
            // directory
            m_entrytype = Directory;
            m_file_size = 0;

            header.modification_time_terminator = 0;
            m_modification_time = std::strtoull(header.modification_time_octal, nullptr, 8);
            break;

        case '6':
            // fifo
            m_entrytype = Fifo;
            break;

        case 'L':
            // GNU long filename
            m_entrytype = Longname1;
            m_keep_members_once = true;
            return;

        default:
            m_entrytype = Unknown;
            break;
    }

    return;
}


UnTar::UnTar(const std::string& filename, bool use_bunzip)
: m_fd(-1)
{
    if (use_bunzip) m_bunzip = std::make_unique<UnBZip2>(filename);
    else {
        if (!filename.empty() && filename != "-") m_fd = ::open(filename.c_str(), O_RDONLY);
        else m_fd = STDIN_FILENO;
    }
}

UnTar::~UnTar()
{
    if (m_fd >= 0 && m_fd != STDIN_FILENO) ::close(m_fd);
}

void UnTar::read(void* buf, size_t len)
{
    ssize_t rb;
    if (m_bunzip) rb = m_bunzip->read(buf, len);
    else rb = ::read(m_fd, buf, len);
    if (rb != static_cast<ssize_t>(len)) throw TarException("unexpected end of file");
}

TarHeader::EntryType UnTar::entry(buf_t& buf, int accepted_types, bool skip_apple_resource_forks)
{
    do {

        m_header.reset();
        read(*m_header, TarHeader::HeaderLen);
        m_header.analyze();

        // this is the only valid exit condition from reading a tar archive - end header reached
        if (m_header.is_end()) return TarHeader::Unknown;

        if (m_header.type() == TarHeader::File) {

            // make sure we reserve space for at least one more character than the file size
            // (to cheaply add a 0 byte if the user wants to)
            buf.reserve(m_header.filesize() + 1);

            // resize the buffer to be able to read the file size
            buf.resize(m_header.filesize());

            // read the file into the buffer
            read(&buf[0], m_header.filesize());

            // check if we have to skip some padding bytes (tar files have a block size of 512)
            size_t padding = (TarHeader::HeaderLen - (m_header.filesize() % TarHeader::HeaderLen)) % TarHeader::HeaderLen;

            if (padding) {
                // this invalidates the (raw) header, but it is a handy buffer to read up to 512 bytes into here
                read(*m_header, padding);
            }

        }

    } while ((m_header.type() & accepted_types) == 0 || (skip_apple_resource_forks && CDDB::begins_with(m_header.filename(), ("./._"))));

    return m_header.type();
}

bool UnTar::file(std::string& name, buf_t& buf, bool skip_apple_resource_forks)
{
    if (entry(buf, TarHeader::File, skip_apple_resource_forks) == TarHeader::Unknown) return false;
    name = m_header.filename();
    return true;
}

