
TWS (Table With Strings)
------------------------

A benchmark that measures the round-trip latency (and a few other metrics)
of basic lookup/insert/update/delete operations with MySQL Cluster APIs
on a predominantly string/varchar-based table.

Currently supported APIs: NDB API, NDB JTie, ClusterJ, and JDBC.

This benchmark code is mostly self-contained (and prepared but not yet
integrated with CRUND).

Please, see
-> tws_java/README.txt
-> tws_c++/README.txt
on how to build and run the client code.

Questions or comments appreciated: martin.zaun@oracle.com
