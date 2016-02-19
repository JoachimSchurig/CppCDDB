//
//  unbzip2.cpp
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


#include "unbzip2.hpp"
#include <stdio.h>
#include <stdexcept>
#include <errno.h>
#include <cstring>


class BZip2Exception : public std::runtime_error {
    using runtime_error::runtime_error;
};


UnBZip2::UnBZip2(const std::string& file)
: m_fp(nullptr)
, m_bzfp(nullptr)
{
    if (!file.empty() && file != "-") m_fp = ::fopen(file.c_str(), "r");
    else m_fp = ::stdin;
    if (!m_fp) throw BZip2Exception(file + ": cannot open: " + strerror(errno));

    int bzerror;
    m_bzfp = BZ2_bzReadOpen(&bzerror, m_fp, 0, 0, nullptr, 0);
    if (bzerror != BZ_OK) throw BZip2Exception(file + ": cannot open: " + bzstrerror(bzerror));
}

UnBZip2::~UnBZip2()
{
    int bzerror;
    if (m_bzfp) BZ2_bzReadClose(&bzerror, m_bzfp);
    if (m_fp && m_fp != ::stdin) fclose(m_fp);
}

ssize_t UnBZip2::read(void* buf, size_t len)
{
    int bzerror;
    ssize_t rb = BZ2_bzRead(&bzerror, m_bzfp, buf, static_cast<int>(len));
    if (bzerror == BZ_STREAM_END) return rb;
    if (bzerror != BZ_OK) throw BZip2Exception(std::string("read error: ") + bzstrerror(bzerror));
    return rb;
}

const char* UnBZip2::bzstrerror(int errcode)
{
    switch (errcode) {
        default:
            return "success (or unknown error code)";
        case BZ_OK:
            return "success";
        case BZ_SEQUENCE_ERROR:
            return "sequence error";
        case BZ_PARAM_ERROR:
            return "parameter error";
        case BZ_MEM_ERROR:
            return "memory error";
        case BZ_DATA_ERROR:
            return "data error";
        case BZ_DATA_ERROR_MAGIC:
            return "data error magic";
        case BZ_IO_ERROR:
            return "IO error";
        case BZ_UNEXPECTED_EOF:
            return "unexpected EOF";
        case BZ_OUTBUFF_FULL:
            return "outbuf full";
        case BZ_CONFIG_ERROR:
            return "configuration error";
    }
}





