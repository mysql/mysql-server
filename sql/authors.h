#ifndef AUTHORS_INCLUDED
#define AUTHORS_INCLUDED

/* Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/* Structure of the name list */

struct show_table_authors_st {
  const char *name;
  const char *location;
  const char *comment;
};

/*
  Output from "SHOW AUTHORS"

  If you can update it, you get to be in it :)

  Don't be offended if your name is not in here, just add it!

  IMPORTANT: Names should be added in alphabetical order (by last name).

  Names should be encoded using UTF-8.
*/

struct show_table_authors_st show_table_authors[]= {
  { "Brian (Krow) Aker", "Seattle, WA, USA",
    "Architecture, archive, federated, bunch of little stuff :)" },
  { "Marc Alff", "Denver, CO, USA", "Signal, Resignal, Performance schema" },
  { "Venu Anuganti", "", "Client/server protocol (4.1)" },
  { "David Axmark", "Uppsala, Sweden",
    "Small stuff long time ago, Monty ripped it out!" },
  { "Alexander (Bar) Barkov", "Izhevsk, Russia",
    "Unicode and character sets (4.1)" },
  { "Omer BarNir", "Sunnyvale, CA, USA",
    "Testing (sometimes) and general QA stuff" },
  { "Guilhem Bichot", "Bordeaux, France", "Replication (since 4.0)" },
  { "John Birrell", "", "Emulation of pthread_mutex() for OS/2" },
  { "Andreas F. Bobak", "", "AGGREGATE extension to user-defined functions" },
  { "Alexey Botchkov (Holyfoot)", "Izhevsk, Russia",
    "GIS extensions (4.1), embedded server (4.1), precision math (5.0)"},
  { "Reggie Burnett", "Nashville, TN, USA", "Windows development, Connectors" },
  { "Oleksandr Byelkin", "Lugansk, Ukraine",
    "Query Cache (4.0), Subqueries (4.1), Views (5.0)" },
  { "Kent Boortz", "Orebro, Sweden", "Test platform, and general build stuff" },
  { "Tim Bunce", "", "mysqlhotcopy" },
  { "Yves Carlier", "", "mysqlaccess" },
  { "Joshua Chamas", "Cupertino, CA, USA",
    "Concurrent insert, extended date syntax" },
  { "Petr Chardin", "Moscow, Russia",
    "Instance Manager (5.0), Server log tables (5.1)" },
  { "Wei-Jou Chen", "", "Chinese (Big5) character set" },
  { "Albert Chin-A-Young", "",
    "Tru64 port, large file support, better TCP wrappers support" },
  { "Jorge del Conde", "Mexico City, Mexico", "Windows development" },
  { "Antony T. Curtis", "Norwalk, CA, USA",
    "Parser, port to OS/2, storage engines and some random stuff" },
  { "Yuri Dario", "", "OS/2 port" },
  { "Andrei Elkin", "Espoo, Finland", "Replication" },
  { "Patrick Galbraith", "Sharon, NH", "Federated Engine, mysqlslap" },
  { "Sergei Golubchik", "Kerpen, Germany",
    "Full-text search, precision math" },
  { "Lenz Grimmer", "Hamburg, Germany",
    "Production (build and release) engineering" },
  { "Nikolay Grishakin", "Austin, TX, USA", "Testing - Server" },
  { "Wei He", "", "Chinese (GBK) character set" },
  { "Eric Herman", "Amsterdam, Netherlands", "Bug fixing - federated" },
  { "Andrey Hristov", "Walldorf, Germany", "Event scheduler (5.1)" },
  { "Alexander (Alexi) Ivanov", "St. Petersburg, Russia", "Replication" },
  { "Mattias Jonsson", "Uppsala, Sweden", "Partitioning" },
  { "Alexander (Salle) Keremidarski", "Sofia, Bulgaria",
    "Bug fixing" },
  { "Mats Kindahl", "Storvreta, Sweden", "Replication" },
  { "Serge Kozlov", "Velikie Luki, Russia", "Testing - Cluster" },
  { "Hakan Küçükyılmaz", "Walldorf, Germany", "Testing - Server" },
  { "Greg (Groggy) Lehey", "Uchunga, SA, Australia", "Backup" },
  { "Matthias Leich", "Berlin, Germany", "Testing - Server" },
  { "Dmitri Lenev", "Moscow, Russia",
    "Time zones support (4.1), Triggers (5.0)" },
  { "Arjen Lentz", "Brisbane, Australia",
    "Documentation (2001-2004), Dutch error messages, LOG2()" },
  { "Marc Liyanage", "", "Created Mac OS X packages" },
  { "Kelly Long", "Denver, CO, USA", "Pool Of Threads" },
  { "Zarko Mocnik", "", "Sorting for Slovenian language" },
  { "Per-Erik Martin", "Uppsala, Sweden", "Stored Procedures (5.0)" },
  { "Alexis Mikhailov", "", "User-defined functions" },
  { "Sinisa Milivojevic", "Larnaca, Cyprus",
    "UNION (4.0), Subqueries in FROM clause (4.1), many other features" },
  { "Jonathan (Jeb) Miller", "Kyle, TX, USA",
    "Testing - Cluster, Replication" },
  { "Elliot Murphy", "Cocoa, FL, USA", "Replication and backup" },
  { "Kristian Nielsen", "Copenhagen, Denmark",
    "General build stuff" },
  { "Pekka Nouisiainen", "Stockholm, Sweden",
    "NDB Cluster: BLOB support, character set support, ordered indexes" },
  { "Alexander Nozdrin", "Moscow, Russia",
    "Bug fixing (Stored Procedures, 5.0)" },
  { "Per Eric Olsson", "", "Testing of dynamic record format" },
  { "Jonas Oreland", "Stockholm, Sweden",
    "NDB Cluster, Online Backup, lots of other things" },
  { "Konstantin Osipov", "Moscow, Russia",
    "Prepared statements (4.1), Cursors (5.0)" },
  { "Alexander (Sasha) Pachev", "Provo, UT, USA",
    "Statement-based replication, SHOW CREATE TABLE, mysql-bench" },
  { "Irena Pancirov", "", "Port to Windows with Borland compiler" },
  { "Jan Pazdziora", "", "Czech sorting order" },
  { "Benjamin Pflugmann", "",
    "Extended MERGE storage engine to handle INSERT" },
  { "Igor Romanenko", "",
    "mysqldump" },
  { "Mikael Ronström", "Stockholm, Sweden",
    "NDB Cluster, Partitioning (5.1), Optimizations" },
  { "Tõnu Samuel", "",
    "VIO interface, other miscellaneous features" },
  { "Carsten Segieth (Pino)", "Fredersdorf, Germany", "Testing - Server"},
  { "Martin Sköld", "Stockholm, Sweden",
    "NDB Cluster: Unique indexes, integration into MySQL" },
  { "Timothy Smith", "Auckland, New Zealand",
    "Dynamic character sets, parts of the build system, libmysqld"},
  { "Miguel Solorzano", "Florianopolis, Santa Catarina, Brazil",
    "Windows development, Windows NT service"},
  { "Punita Srivastava", "Austin, TX, USA", "Testing - Merlin"},
  { "Alexey Stroganov (Ranger)", "Lugansk, Ukraine", "Testing - Benchmarks"},
  { "Ingo Strüwing", "Berlin, Germany", "Bug fixing" },
  { "Magnus Svensson", "Öregrund, Sweden",
    "NDB Cluster: Integration into MySQL, test framework" },
  { "Zeev Suraski", "", "FROM_UNIXTIME(), ENCRYPT()" },
  { "TAMITO", "",
    "The _MB character set macros and UJIS and SJIS character sets" },
  { "Jani Tolonen", "Helsinki, Finland",
    "mysqlimport, extensions to command-line clients, PROCEDURE ANALYSE()" },
  { "Lars Thalmann", "Stockholm, Sweden",
    "Replication and cluster development" },
  { "Tomas Ulin", "Stockholm, Sweden",
    "NDB Cluster: Configuration, installation" },
  { "Gianmassimo Vigazzola", "", "Initial Windows port" },
  { "Sergey Vojtovich", "Izhevsk, Russia", "Plugins infrastructure (5.1)" },
  { "Matt Wagner", "Northfield, MN, USA", "Bug fixing" },
  { "Jim Winstead Jr.", "Los Angeles, CA, USA", "Bug fixing" },
  { "Michael (Monty) Widenius", "Tusby, Finland",
    "Lead developer and main author" },
  { "Peter Zaitsev", "Tacoma, WA, USA",
    "SHA1(), AES_ENCRYPT(), AES_DECRYPT(), bug fixing" },
  {NULL, NULL, NULL}
};

#endif /* AUTHORS_INCLUDED */
