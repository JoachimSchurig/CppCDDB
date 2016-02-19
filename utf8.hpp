//
//  utf8.hpp
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

#ifndef utf8_hpp_EUIHDSIUSDZGALKHSBDLFUZDHSBKJDSLIVZSDJHBSKDVSDKVS
#define utf8_hpp_EUIHDSIUSDZGALKHSBDLFUZDHSBKJDSLIVZSDJHBSKDVSDKVS

#include <cstdint>
#include <string>


namespace CDDB {
namespace Unicode {

    template<typename Ch>
    uint32_t codepoint_cast(Ch sch)
    {
        // All this code gets completely eliminated during
        // compilation. All it does is to make sure we can
        // expand any char type to a uint32_t without signed
        // bit expansion, treating all char types as unsigned.

        if (sizeof(Ch) == 1) {
            return static_cast<uint8_t>(sch);
        }
        else if (sizeof(Ch) == 2) {
            return static_cast<uint16_t>(sch);
        }
        else {
            return static_cast<uint32_t>(sch);
        }
    }

    template<typename Ch, typename N>
    void to_utf8(Ch sch, std::basic_string<N>& narrow)
    {
        uint32_t ch = codepoint_cast(sch);

        if (ch < 0x0080) {
            narrow += N(ch);
        }
        else if (ch < 0x0800) {
            narrow += N(0xc0 | ((ch >> 6) & 0x1f));
            narrow += N(0x80 | (ch & 0x3f));
        }
        else if (ch < 0x010000) {
            narrow += N(0xe0 | ((ch >> 12) & 0x0f));
            narrow += N(0x80 | ((ch >> 6) & 0x3f));
            narrow += N(0x80 | (ch & 0x3f));
        }
        else if (ch < 0x0110000) {
            narrow += N(0xf0 | ((ch >> 18) & 0x07));
            narrow += N(0x80 | ((ch >> 12) & 0x3f));
            narrow += N(0x80 | ((ch >> 6) & 0x3f));
            narrow += N(0x80 | (ch & 0x3f));
        }
        else {
            narrow += '?';
        }
    }

    template<typename W, typename N>
    void to_utf8(const std::basic_string<W>& wide, std::basic_string<N>& narrow)
    {
        for (auto ch : wide) to_utf8(ch, narrow);
    }

    template<typename N>
    bool valid_utf8(const std::basic_string<N>& narrow)
    {
        uint16_t remaining { 0 };
        uint32_t codepoint { 0 };
        uint32_t lower_limit { 0 };

        for (const auto sch : narrow) {

            uint32_t ch = codepoint_cast(sch);

            if (sizeof(N) > 1 && ch > 0x0ff) {
                return false;
            }

            switch (remaining) {

                default:
                case 0: {
                    codepoint = 0;
                    if (ch < 128) {
                        break;
                    }
                    else if ((ch & 0x0e0) == 0x0c0) {
                        remaining = 1;
                        lower_limit = 0x080;
                        codepoint = ch & 0x01f;
                    }
                    else if ((ch & 0x0f0) == 0x0e0) {
                        remaining = 2;
                        lower_limit = 0x0800;
                        codepoint = ch & 0x0f;
                    }
                    else if ((ch & 0x0f8) == 0x0f0) {
                        remaining = 3;
                        lower_limit = 0x010000;
                        codepoint = ch & 0x07;
                    }
                    else {
                        return false;
                    }
                    break;
                }

                case 5:
                case 4:
                case 3:
                case 2:
                case 1: {
                    if ((ch & 0x0c0) != 0x080) {
                        return false;
                    }
                    codepoint <<= 6;
                    codepoint |= (ch & 0x03f);
                    --remaining;
                    if (!remaining) {
                        if (codepoint < lower_limit) {
                            return false;
                        }
                    }
                    break;
                }
                    
            }
            
        }
        return true;
    }
    
    template<typename N, typename W>
    bool from_utf8(const std::basic_string<N>& narrow, std::basic_string<W>& wide)
    {
        uint16_t remaining { 0 };
        uint32_t codepoint { 0 };
        uint32_t lower_limit { 0 };

        for (const auto sch : narrow) {

            uint32_t ch = codepoint_cast(sch);

            if (sizeof(N) > 1 && ch > 0x0ff) {
                return false;
            }

            switch (remaining) {

                default:
                case 0: {
                    codepoint = 0;
                    if (ch < 128) {
                        wide += static_cast<W>(ch);
                        break;
                    }
                    else if ((ch & 0x0e0) == 0x0c0) {
                        remaining = 1;
                        lower_limit = 0x080;
                        codepoint = ch & 0x01f;
                    }
                    else if ((ch & 0x0f0) == 0x0e0) {
                        remaining = 2;
                        lower_limit = 0x0800;
                        codepoint = ch & 0x0f;
                    }
                    else if ((ch & 0x0f8) == 0x0f0) {
                        remaining = 3;
                        lower_limit = 0x010000;
                        codepoint = ch & 0x07;
                    }
                    else {
                        return false;
                    }
                    break;
                }

                case 5:
                case 4:
                case 3:
                case 2:
                case 1: {
                    if ((ch & 0x0c0) != 0x080) {
                        return false;
                    }
                    codepoint <<= 6;
                    codepoint |= (ch & 0x03f);
                    --remaining;
                    if (!remaining) {
                        if (codepoint < lower_limit) {
                            return false;
                        }

                        // take care - this code is written for platforms with 32 bit wide chars -
                        // if you compile this code on Windows, or want to explicitly support 16
                        // bit wide chars on other platforms, you need to supply surrogate pair
                        // replacements for characters > 0x0ffffffff at exactly this point before
                        // adding a new codepoint to 'wide'

                        wide += W(codepoint);
                    }
                    break;
                }

            }
            
        }
        
        return true;
    }

} // namespace Unicode
} // namespace CDDB


#endif /* utf8_hpp */
