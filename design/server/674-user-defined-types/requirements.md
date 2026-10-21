
# Requirements

## MySQL Editions

FR:

Support for User Data Type (UDT) is available in all editions of the server:

* Community edition
* Enterprise edition
* Cloud editions

FR:

When a party provides a specific User Data Type implementation, the type delivered is available, at the provider choice, to either:

* All editions (community, enterprise, cloud)
* Enterprise and cloud only
* Cloud only

## MySQL Versions

The target for this project is the main branch only, release 27.x

No backports are planned to older releases.

NFR:

If at all possible, this project should try to mitigate disruptions
when porting fixes from older releases to the main branch.

Design constraints include:

* to isolate User Defined Type specific code to dedicated files,
  to prevent merge collisions.

## Deployment

Deployment consist of:

* adding a specific User Data Type to a MySQL server instance
* removing a specific User Data Type from a MySQL server instance

FR:

Deployment is dynamic, and does not require a server restart.

FR:

Deployment is a privileged operation.

## Discoverability

### Type definition discoverability

Because User Data Types installed in a server are optional, users need to discover what is available.

FR:

An interface in SQL is exposed by the server to list all available User Data Types.

FR:

An interface in SQL is exposed by the server to list all operations available for a given User Data Type.

FR:

An interface in SQL is exposed by the server to describe the full operation signature for a given operation on a given User Data Type.

### Type usage discoverability

Users need to find out where a given type is in use, to remove references before removing a type.

FR: 

The INFORMATION_SCHEMA is extended to expose:

* the User Data Type used by a column in a table, if applicable
* the User Data Type used by a parameter in a stored program, if applicable
* the User Data Type used by the return type of a stored function, if applicable

FR:

The INFORMATION_SCHEMA exposes a new view, to find all usages of a given User Data Type in stored programs.

## SQL Language

### Type DDL

FR:

A TYPE is a SQL object.

FR:

The syntax for DDL operations on a type is a subset of the ANSI SQL syntax.

New statements supported are:

* CREATE TYPE
* DROP TYPE

### Type DML

FR:

Invoking an operation on a type is a valid SQL expression.

The syntax for method invocations on a type is a subset of the ANSI SQL
syntax.

New expressions supported are:

* Invoking a static method on a TYPE
* Invoking a dynamic method on a TYPE

### Type usage

FR:

A User Defined Type can be used everywhere a builtin MySQL type is used,
except for the following restrictions:

* User Defined Types can not be used in an INDEX
* User Defined Types can not be used in an PRIMARY KEY
* User Defined Types can not be used in an FOREIGN KEY

### Type expression validity

FR:

Expressions invoking a method on a type are checked against the type,
method, and parameters.

Invalid invocations:

* using a type that does not exists
* invoking a method on a SQL object that is not typed with a User Defined Type
* using a method that does not exists
* invoking a static method with the dynamic syntax
* invoking a dynamic method with the static syntax
* using parameters that do not comply with the method signature

are detected and reported as errors.

FR:

Verification of the expression validity is performed during parsing,
not during runtime.

As a consequence:

* an invalid expression that is not reachable (IFNULL, CASE expressions,
  impossible WHERE clauses) is detected at parse time
* an invalid expression in a prepared statement is detected during PREPARE
* in invalid expression in a stored program is detected during CREATE

## Type implementation

### Components

NFR:

Types are implemented as a COMPONENT, implemented in C++.

This choice is driven by performance considerations.

Note that a component is considered trusted code when loaded into the server.

There are no safeguards such as an internal container to constrain code
execution, to prevent failures caused by third party code.

FR:

The server supports adding a type:

* from a component dynamically installed, using INSTALL COMPONENT.
  * This allows a third party to provide optional types.
  * This allows MySQL to provide optional types.

NFR:

The server supports adding a type:

* from a component statically linked with the server.
  * This allows MySQL to provide additional builtin types.

## Services

NFR:

The server exposes the services needed for a third party to implement a
type.

NFR:

An implementation of a type in a component should depend only on exposed
services. Dependencies on server internals are forbidden.

NFR:

As new features for User Defined Types are implemented over time:

* new services may be added in future releases
* existing services maybe extended in future releases

Service definitions are versioned and provide a stable interface,
for backward compatibility (once a service is declared stable).

## No registration authority

NFR:

A third party implementing a type MUST NOT be required to register a name or
identifier with a central authority to provide a type implementation.

NFR:

The doxygen technical documentation, for type implementers,
describes guidelines and best practices to avoid naming collisions,
to favor a healthy ecosystem of types provided by various vendors.

## Documentation

### Reference Manual

NFR:

Every aspect related to using a type in an application (Installing a type, DDL, DML, GRANT, REVOKE)
is documented in the reference manual.

For reference: https://dev.mysql.com/doc/refman/26.10/en/

### Doxygen documentation

NFR:

Every aspect related to providing a type to an application (implementing
a component) is documented in the internal doxygen documentation.

For reference: https://dev.mysql.com/doc/dev/mysql-server/latest/

