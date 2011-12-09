
CRUND
-----

A benchmark that measures the performance of MySQL Server/Cluster APIs for
basic database operations.

The database operations are variations of: Creates, Reads, Updates,
Navigates, and Deletes ("CRUND").

The MySQL Cluster client APIs currently tested by the benchmark are:
- NDB API (C++)
- NDB JTie (Java)
- Cluster/J
- JDBC (MySQL Server)
- OpenJPA on JDBC (MySQL Server)
- OpenJPA on ClusterJ and JDBC (MySQL Server)
[- ndb-bindings/java (referred to as NDB/J in this benchmark)]
[- JDBC (Apache Derby)]
[- OpenJPA on JDBC (Apache Derby)]

See INSTALL.txt for how to build, configure, and run the CRUND benchmark.

Comments or questions appreciated: martin.zaun@oracle.com


Urban Dictionary: "crund"
* used to debase people who torture others with their illogical attempts to
  make people laugh;
* reference to cracking obsolete jokes;
* a dance form;
* to hit hard or smash. 
http://www.urbandictionary.com/define.php?term=crund


TWS (Table With Strings)
------------------------

A self-contained benchmark that measures the performance of selected MySQL
Server/Cluster APIs for basic operations on a string/varchar-based schema.

See file tws_benchmark/README.txt for how to build and run the benchmark.

This benchmark is standalone and not yet integrated into CRUND.
