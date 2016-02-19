//
//  asioserver.hpp
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

#ifndef asioserver_hpp_ASUCFGVCDUUJZUCNILASDKBUALSIVB
#define asioserver_hpp_ASUCFGVCDUUJZUCNILASDKBUALSIVB

// This class needs the (Boost::)ASIO headers accessible in the include path.
// You can download a standalone version of ASIO from http://think-async.com/Asio/
// Modify the Makefile to indicate the path to the headers.
// You do not need to install Boost to use ASIO.

#define ASIO_STANDALONE
#include <asio.hpp>
#include <memory>
#include <thread>


class ASIOServer {
public:
    ASIOServer(uint16_t port)
    : m_port(port) {}
    virtual ~ASIOServer();

    bool start(uint16_t timeout_seconds = 5 * 60, bool block = false);
    void stop() { m_quit = true; }
    bool is_running() const { return m_ipv6_server || m_ipv4_server; }

protected:
    struct Parameters {
        virtual ~Parameters() {}
        bool terminate = false;
    };
    typedef std::shared_ptr<Parameters> param_t;
    
    /// virtual hook to override with a completely new session management logic
    /// (either calling init() and request() below, or anything else)
    virtual void session(std::unique_ptr<asio::ip::tcp::iostream> stream);
    /// virtual hook to send a init message to the client
    virtual std::string init(param_t parameters);
    /// virtual hook to process one line of client requests
    virtual std::string request(const std::string& qstr, param_t parameters);
    /// request the stream timeout requested for this instance (set it in own
    /// session handlers for reading and writing on the stream)
    uint16_t get_timeout() const { return m_timeout; }
    /// if the derived class needs addtional per-thread control parameters,
    /// define a Parameters class to accomodate those, and return a storage
    /// model of this class in a call to get_parameters()
    virtual param_t get_parameters() { return std::make_shared<Parameters>(); }

private:
    asio::io_service m_asio;
    uint16_t m_port;
    bool m_block;
    bool m_quit = false;
    uint16_t m_timeout = 5*60;
    std::unique_ptr<std::thread> m_ipv4_server;
    std::unique_ptr<std::thread> m_ipv6_server;

    ASIOServer(const ASIOServer&) = delete;
    ASIOServer& operator=(const ASIOServer&) = delete;

    void server(bool ipv6);
};


#endif /* asioserver_hpp */
