# CppCDDB
A CDDB server implementation in C++14, using SQLite, reading freedb.org bz2 archive files without previous unpacking.

### What this application does
CppCDDB is a fast local server for the CDDB protocol [as documented at ftp.freedb.org](http://ftp.freedb.org/pub/freedb/latest/CDDBPROTO). The CDDB protocol is used by clients like CD player apps or CD ripper apps to find information about artist, title, and song names.

CppCDDB maintains a copy of the freedb database on a local computer, in an SQLite database. This way it can be queried in situations without internet access, or if a user does not want to share which CD she plays at what moment with the rest of the world.

CppCDDB is capable of fast fuzzy searches against the database. CD lenghts often vary slightly between releases, and because the CDDB discid is based on frame positions on the CD, a slightly varying release would not be found without fuzzy searches. 

The full freedb.org database with all 3.5 million CDs and more than 44 milion track titles (as of February 2016) needs 2.9GB disc space in an SQLite database - a size today easily manageable on embedded devices. The difficulty with the freedb format is however to get the data into an SQL database - as the download consists of, well, 3.6 million files in only a handful of directories. On most file systems you should not even start the unpacking, as it will eventually fail after some days of trying hard.

CppCDDB therefore includes a bzip2 tar reader (the archive format used by freedb.org) that reads all data directly from the compressed archive format, checks the data under some sanity aspects, and writes the approved records into its own SQLite database.

This process takes about 15 minutes on a relatively recent Macbook. I would not start it though on a Raspberry. But fortunately the SQLite database format is binary compatible across architectures, and the hash values that CppCDDB uses for the database search are big endian on all platforms, therefore it is easy to simply copy a database from a OSX or Linux desktop to an embedded device, and use it there for lookups (which is read only mode).

The original [CDDB discid algorithm](https://en.wikipedia.org/wiki/CDDB#Example_calculation_of_a_CDDB1_.28FreeDB.29_disc_ID) produces an astounding amount of collisions (same discid value for different CDs) - nearly 1.2 million for the 3.5 million CDs. By using a simple [Fowler-Noll-Vo hash](https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function) straight across the frame positions the collision risk could be reduced by nearly three magnitudes (6000 collisions for 3.5 million).

The reason why the original CDDB discid works at all is - that it is not used. Or only for a first lookup. The final decision which record to present to a requester is not based on the discid, but on a direct comparison of the frame lengths of the involved tracks (which are luckily sent alongside the discid in the client requests).

CppCDDB therefore does not use the original discid, but an own, private value, based on FNV hashing. This 32-bit-id looks as the original id, and is therefore accepted by the cddb client applications. Additionally, CppCDDB creates an index with reduced accuracy of the track lengths (rounded to the nearest 8 seconds), which is used for the fuzzy search.

Both indexes respond in microseconds, even on a Raspberry.

###Supported Operating Systems
CppCDDB is written in standard C++14. Basically all platforms with a compiler for that standard could run it. It includes support for big (e.g. ARM) and low endian (386) systems.

You need sqlite3 and libbz2 on your platform. For Windows this means that you need to get them yourself, on other systems they are either preinstalled or configurable by the respective package manager.

CppCDDB is currently tested on OSX and Debian Linux, on a Mac and a Raspberry Pi.

###Used Third-Party software
CppCDDB uses [SQLite3](http://sqlite.org) and [bzip2](http://www.bzip.org). It also uses the formidable C++ string formatting library [CppFormat](http://cppformat.github.io/latest/index.html) from Victor Zverovich. For networking, (Boost::)[ASIO](http://think-async.com/Asio/) is used (as standalone headers, Boost is not needed). And it uses Sebastien Rombauts's nice [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) C++ wrapper classes for SQLite - which helped to reduce greatly the implementation time for this project. Because his project is larger than what I needed for this project, I have copied his core database classes into the folder sqlitecpp/ in this project. Please always use his project here on GitHub directly for your own work.

Thank you all for your valuable work!

###How to install
For the time being simply clone or checkout the repository locally, and enter its directory, then enter into the sqlitecpp directory and type `make`.

Go back down into the CppCDDB directory and edit the Makefile. At the beginning it contains a section which tells where to find the ASIO library headers (it is a header-only library). Point it to where you downloaded and unpacked ASIO. Then compile with `make`.

Start the application as follows: `cppcddbd -d database-file`. This opens up port 8880 in ipv4 and ipv6 mode (if available) and waits for your client requests in either the native cddb protocol or via http (but on this port).

Of course you should first make sure that you have a database with the CD data: [Download](http://www.freedb.org/en/download__database.10.html) a snapshot from freedb.org, and start like `cppcddbd -d database-file -i import-file.tar.bz2`. This will start the import, and every 100.000 records you will get a status message on stderr.

For subsequent starts make sure to not again import the file (it does not damage the data, but you would have to wait until the import has ended (with a lot of hash collisions) to avoid damaging the sql file.

`cppcddbd -h` gives a brief option explanation.

`-u` allows you to update an existing database with incremental update files from freedb.org.

###Copyright and License
CppCDDB is licensed under the permissive terms of the BSD license.
(c) 2016 Joachim Schurig
