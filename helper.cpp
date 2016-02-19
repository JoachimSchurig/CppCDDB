//
//  helper.cpp
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

#include <locale>
#include <stdexcept>
#include "clocale"
#include "helper.hpp"

using namespace CDDB;

const char DefaultLocale[] = "en_US.UTF-8";

std::string CDDB::set_unicode_locale(std::string name, bool throw_on_error)
{
    try {
#ifdef __OSX__
        // no way to get the user's locale in OSX with C++. So simply set to en_US if not given as a parameter.
        if (name.empty() || name == "C" || name == "C.UTF-8") name = DefaultLocale;
        std::setlocale(LC_ALL, name.c_str());
        if (ends_with(name, ".UTF-8")) name.erase(name.end() - 6, name.end());
        std::locale::global(std::locale(name.c_str()));
        // make the name compatible to the rest of the world
        name += ".UTF-8";
#else
        // on other platforms, query the user's locale
        if (name.empty()) name = std::locale("").name();
        // set to a fully defined locale if only the C locale is setup. This is also needed for C.UTF-8, as
        // that one does not permit character conversions outside the ASCII range.
        if (name == "C" || name == "C.UTF-8") name = DefaultLocale;
        std::setlocale(LC_ALL, name.c_str());
        std::locale::global(std::locale(name.c_str()));
#endif
    } catch (std::exception& e) {
        if (throw_on_error) throw std::runtime_error(std::string("cannot set unicode locale ") + name);
        name.erase();
    } catch (...) {
        if (throw_on_error) throw std::runtime_error(std::string("cannot set unicode locale ") + name);
        name.erase();
    }
    return name;
}



Duration::dividers_t Duration::dividers = {
    { uint64_t(1000)*1000*1000*60*60*24, "d", '\0', 0 },
    { uint64_t(1000)*1000*1000*60*60,    "h",  ',', 2 },
    { uint64_t(1000)*1000*1000*60,       "m",  ':', 2 },
    { uint64_t(1000)*1000*1000,          "s",  ':', 2 },
    { uint64_t(1000)*1000,               "ms", '.', 3 },
    { uint64_t(1000),                    "us", '.', 3 },
    { uint64_t(1),                       "ns", '.', 3 }
};

uint64_t Duration::print_unit(std::string& out, uint64_t nanoseconds, dividers_t::const_iterator it)
{
    if (nanoseconds >= it->divider) {
        uint64_t val = nanoseconds / it->divider;
        nanoseconds -= val * it->divider;
        if (!out.empty()) {
            // only left-pad with zeroes if this is not the first unit printed
            out += it->separator;
            std::string sval = std::to_string(val);
            auto len = sval.length();
            while (len++ < it->digits) out += '0';
            out += sval;
        } else {
            out += std::to_string(val);
        }
        out += it->unit;
    }
    return nanoseconds;
}

std::string Duration::to_string(uint64_t nanoseconds, Precision precision)
{
    std::string out;

    // 4d,07h:22m:08s.660ms.252us.881ns

    auto ie = dividers.begin() + precision + 1;
    for (auto it = dividers.begin(); it != ie; ++it) {
        nanoseconds = print_unit(out, nanoseconds, it);
    }
    if (out.empty()) {
        out += '0';
        out += (--ie)->unit;
    }
    return out;
}

uint64_t Duration::get(Precision precision) const
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(m_this_lap - m_start).count() / dividers[precision].divider;
}

uint64_t Duration::get_lap(Precision precision) const
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(m_this_lap - m_last_lap).count() / dividers[precision].divider;
}

