# High Level Description

This project is to implement, in MySQL, support for User Defined Types,
where the type implementation is provided by a third party to MySQL.

This project is also to be conducted with the MySQL community, in public.

# Executive summary

MySQL's user defined type (UDT) initiative is to let developers add first-class
SQL data types to MySQL.
The intent is to make a type such as UUID, or a domain-specific identifier
usable in table definitions and SQL while its behaviour -- conversion, validation, and operations -- is supplied by an installable component.
This would move MySQL toward a more extensible type system,
reducing pressure to add every emerging or domain-specific data type as a built-in server feature.
The feature will leverage and extend MySQL's component infrastructure to make this possible.

The enhancement is likely going to impact entire server codebase rather than a narrow SQL enhancement.
It will impact parser and DDL, privileges and metadata locking,
data dictionary and Information Schema, UDF/service APIs, optimizer and prepared statements,
stored programs, client protocol/connectors, indexing and constraints, replication,
backup/restore, tooling, and observability.

Initial thinking is to keep the storage format explicit and stable in the data dictionary,
while dynamically loaded components register the type's native operations.
This deliberately confines type-aware logic to the SQL layer so storage engines
and replication can handle the serialized representation rather than requiring
every lower layer to understand every third-party type.
