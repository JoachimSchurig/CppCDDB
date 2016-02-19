//
//  asioserver.cpp
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
#include "asioserver.hpp"


using asio::ip::tcp;



std::string ASIOServer::init(param_t parameters)
{
    return std::string();
}

std::string ASIOServer::request(const std::string& qstr, param_t parameters)
{
    return std::string();
}

void ASIOServer::session(std::unique_ptr<asio::ip::tcp::iostream> stream)
{
    try {

        param_t parameters = get_parameters();

        stream->expires_from_now(std::chrono::seconds(m_timeout));
        *stream << init(parameters);

        std::string line;

        while (!parameters->terminate && !stream->bad()) {

            stream->expires_from_now(std::chrono::seconds(m_timeout));

            if (!std::getline(*stream, line)) return;

            if (line != "\r") {
                *stream << request(line, parameters);
            }

        }

    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unknown exception" << std::endl;
    }
}

void ASIOServer::server(bool ipv6)
{
    // do not catch exceptions here - if one gets triggered, we want to terminate
    // the application as we do not know how to proceed with the listen sockets
    // otherwise!

    asio::ip::v6_only v6_only(false);

    tcp::endpoint endpoint((ipv6) ? tcp::v6() : tcp::v4(), m_port);
    tcp::acceptor acceptor(m_asio, endpoint, true); // true means reuse_addr

    if (ipv6) {
        // check if we listen on both v4 and v6, or only on v6
        acceptor.get_option(v6_only);
        // if acceptor is not open this computer does not support v6
        // if v6_only then this computer does not use a dual stack
        if (!acceptor.is_open() || v6_only) {
            // check if we can stay in this thread (because the v6 acceptor
            // is not working, and blocking construction is requested)
            if (!acceptor.is_open() && m_block) server(false);
            // else open v4 explicitly in another thread
            m_ipv4_server = std::make_unique<std::thread>(&ASIOServer::server, this, false);
        }
    }

    while (acceptor.is_open() && !m_quit) {
        auto ustream = std::make_unique<tcp::iostream>();
        asio::error_code ec;
        acceptor.accept(*ustream->rdbuf());
        std::thread(&ASIOServer::session, this, std::move(ustream)).detach();
    }
}

bool ASIOServer::start(uint16_t timeout_seconds, bool block)
{
    if (is_running()) throw std::runtime_error("server is already running");
    m_timeout = timeout_seconds;
    m_block = block;
    if (m_block) server(true);
    else m_ipv6_server = std::make_unique<std::thread>(&ASIOServer::server, this, true);
    return is_running();
}


ASIOServer::~ASIOServer()
{
    // needed to send some signal to stop the running threads.
    // currently they only quit after an accept on the listen sockets.
    
    m_quit = true;

    // now wait for completion
    if (m_ipv4_server) m_ipv4_server->join();
    if (m_ipv6_server) m_ipv6_server->join();
}


