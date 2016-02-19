//
//  main.cpp
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
#include <unistd.h>
#include "cddbupdater.hpp"
#include "cddbserver.hpp"
#include "format.hpp"
#include "helper.hpp"




int main(int argc, char *argv[]) {

    std::string lname = CDDB::set_unicode_locale("", true);

    try {

        std::string database = "cddb.sqlite";
        std::string importfile;
        std::string updatefile;
        uint16_t port = 8880;
        bool expect_http = true;
        bool print_protocol = false;
        uint16_t max_diff = 4;

        {
            int opt;

            while ((opt = ::getopt(argc, argv, "cd:f:i:hp:u:v")) != -1) {
                switch (opt) {
                    case 'c':
                        expect_http = false;
                        break;
                    case 'd':
                        database = optarg;
                        break;
                    case 'f':
                        max_diff = ::strtoul(optarg, nullptr, 10);
                        break;
                    default:
                    case 'h':
                        std::cout << argv[0] << " - help:" << std::endl;
                        std::cout << std::endl;
                        std::cout << " -c       : send cddb protocol welcome message on connect (would disturb HTTP)" << std::endl;
                        std::cout << " -d file  : database file (default 'cddb.sqlite')" << std::endl;
                        std::cout << " -f sec   : difference in seconds to allow for relaxed track matching (1..8)" << std::endl;
                        std::cout << " -i file  : import from file ('-' for stdin)" << std::endl;
                        std::cout << " -p port  : CDDB port to use (default 8880)" << std::endl;
                        std::cout << " -u file  : update from file ('-' for stdin)" << std::endl;
                        std::cout << " -v       : print protocol log on stderr" << std::endl;
                        std::cout << std::endl;
                        exit(0);
                    case 'i':
                        importfile = optarg;
                        break;
                    case 'p':
                        port = ::strtoul(optarg, nullptr, 10);
                        break;
                    case 'u':
                        updatefile = optarg;
                        break;
                    case 'v':
                        print_protocol = true;
                        break;
                }
            }
        }

            database = "/Users/joachim/raspi/cddb.sqlite";
        //    importfile = "/Users/joachim/Downloads/freedb-complete-20160101.tar.bz2";
        //    updatefile = "/Users/joachim/Downloads/freedb-update-20160101-20160201.tar.bz2";

        if (!importfile.empty()) {

            // create the CDDB updater object
            CDDB::CDDBSQLUpdater cddbupdater(database);

            // check if we shall import some data
            cddbupdater.import(importfile, true);

        }

        if (!updatefile.empty()) {

            // create the CDDB updater object
            CDDB::CDDBSQLUpdater cddbupdater(database);

            // check if there are update data to an existing database
            // (import and update only differ by the latter keeping the indexes up during import)
            cddbupdater.import(updatefile, false);

        }

        // construct a cddb server
        CDDB::CDDBSQLServer cddbserver(database, port, expect_http, print_protocol, max_diff);

        // and run it with 30 seconds IO timeout, in blocking mode
        cddbserver.start(30, true);
            
        return 0;
        
    } catch (std::exception& e) {

        std::cerr << "Exception: " << e.what() << std::endl;
        
    } catch (...) {

        std::cerr << "Unknown exception " << std::endl;
        throw;

    }

    return 1;
}
