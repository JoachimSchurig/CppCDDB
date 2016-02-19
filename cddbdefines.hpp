//
//  cddbdefines.hpp
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

#ifndef cddbdefines_hpp_CDJBJHSDVBSLDVMDSVLJBVKJNDKAMVDSV
#define cddbdefines_hpp_CDJBJHSDVBSLDVMDSVLJBVKJNDKAMVDSV



#include "helper.hpp"


inline uint32_t private_discid_fnv(uint32_t seconds, std::vector<uint32_t> frames)
{
    // calculate a FNV based ID over track lengths and count
    CDDB::FNVHash32 discid;
    discid.add(seconds, true);
    discid.add(static_cast<uint32_t>(frames.size(), true));
    for (auto& track : frames) {
        discid.add(track, true);
    }
    return discid;
}

inline uint32_t private_fuzzy_discid_fnv(uint32_t seconds, std::vector<uint32_t> frames)
{
    // calculate a FNV based ID over normalized track lengths and count
    CDDB::FNVHash32 discid;
    // do not add the seconds (it's actually the start frame of the CD in the private implementation
    discid.add(static_cast<uint32_t>(frames.size(), true));
    for (auto& track : frames) {
        // go down to 8 second resolution
        uint32_t normalized = (((track + 38) / 75) + 4) / 8;
        discid.add(normalized, true);
    }
    return discid;
}

inline uint32_t private_discid(uint32_t seconds, std::vector<uint32_t> frames)
{
    return private_discid_fnv(seconds, frames);
}

inline uint32_t private_fuzzy_discid(uint32_t seconds, std::vector<uint32_t> frames)
{
    return private_fuzzy_discid_fnv(seconds, frames);
}

inline uint32_t convert_frame_starts_in_frame_lengths(uint32_t seconds, std::vector<uint32_t>& frames)
{
    if (frames.empty()) return 0;

    uint32_t startframe = *frames.begin();

    for (auto it = frames.begin(); it != frames.end()-1; ++it) {
        *it = *(it+1) - *it;
    }

    *frames.rbegin() = (seconds*75 - *frames.rbegin()) - startframe;

    return startframe;
}

inline uint32_t convert_frame_lengths_in_frame_starts(uint32_t seconds, std::vector<uint32_t>& frames)
{
    if (frames.empty()) return 0;

    std::vector<uint32_t> f2;
    f2.reserve(frames.size());
    // this is the first start
    f2.push_back(seconds);
    for (auto it = frames.begin(); it != frames.end()-1; ++it) {
        f2.push_back(f2.back() + *it);
    }

    uint32_t sec = (*f2.rbegin() + *frames.rbegin() - seconds) / 75;

    frames = std::move(f2);

    return sec;
}



#endif /* cddbdefines_hpp */
