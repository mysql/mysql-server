
# Architecture

This section documents the key architectural decisions
for the User Defined Type project, with rationale.

## User Defined Types

User defined types can be expressed in various ways.
There is not one form, but several, for different purposes.

### Distinct types

The SQL-2023 spec defines a concept of 'distinct type'.

A distinct type is an encapsulated implementation of a given type,
stored in the database using an underlying storage representation.

The internal structure of a distinct type is by nature opaque.

This form is the most powerful concept, as it allows to represent
types with any internal complexity, for example:

* a geometry in a GIS system
* a vector index
* a matrix
* an audio fingerprint

Distinct types can be also used to totally encapsulate basic data,
for example:

* an IPv6 address
* a phone number
* a book USBN13 number

### Structured types

The SQL-2023 spec also defines a concept of 'structured type'.

A structured type is a list of fields.

For example, representing a postal address as:

* a street number
* a street name
* a zip code
* a city name

in a structure allows direct access to each sub field,
while simply defining a column in a table as a postal address.

While this is convenient to avoid repetition, this is not expressive enough,
as not all kind of data can fit into a fixed flat list of fields.

### Other user defined types

Various database vendor also provide alternate forms
for user defined types.

#### Enumerated types

PostgreSQL allows to define a alias on an enum,
to avoid repetition of each elements.

https://www.postgresql.org/docs/current/sql-createtype.html

#### Table types

SQL-Server allows TABLE TYPE.

https://learn.microsoft.com/en-us/sql/t-sql/statements/create-type-transact-sql?view=sql-server-ver17

### Architectural decision: distinct types

MySQL implements support for distinct types.

From a functionality point of view, this is the most expressive,
and allows to implement anything.

From a stability point of view, it is desirable to aim
for a form actually defined in the SQL standard,
rather than have a bias towards a specific vendor and its proprietary
syntax.

## Type Metadata

A user defined type implementation as a whole consist of:

* metadata, describing the type and operations available
* code, implementing the operations

A key decision is to define where the metadata is available.

### No metadata

If no metadata is available at all, only the type implementation knows
what operations are supported and which parameter they expect.

Integrity checks are deferred at runtime invocation, which is highly
undesirable.

### Data dictionary

Storing metadata about a user defined type in the internal
data dictionary allows the server itself (mysqld) to make
integrity checks before invoking an operation implementation.

This allows to consider operations properties that are needed by the
optimizer (deterministic / non deterministic).

Metadata stored in the data dictionary only is not visible externally.

### INFORMATION_SCHEMA

With INFORMATION_SCHEMA views built on top of data dictionary tables,
the user defined type metadata is also visible to users.

This allows external tooling to reason on user defined types, like:

* a query editor that supports in tool assistance for operations invocations
* an AI agent that supports SQL code generation to invoke user data types

### Architectural decision: INFORMATION_SCHEMA

Metadata for user data types must be available in the INFORMATION_SCHEMA,
exposing the content stored in the data dictionary.

The SQL-2023 specification defines views for this:

* USER_DEFINED_TYPES
* METHOD_SPECIFICATIONS
* METHOD_SPECIFICATION_PARAMETERS

as well as additional supporting columns to other views.

## Type implementation

The logic to implement a type can be expressed in various languages,
with trade-offs.

### SQL

When expressing an implementation in SQL, runtime evaluation of third party
code is guaranteed to be safe and not cause failures in the server itself.

A significant challenge for development is the lack of supporting SQL libraries.
Logic needs to be implemented from zero from the ground up, which is
likely to be an overwhelming burden to developers implementing a type.

Another concern is runtime performances, because the SQL code is interpreted and
not compiled.

### Native

With a native implementation, there is a technical risk associated with
including third party code in the server process, as there are no mechanisms
in place to prevent damage possibly caused by a poor quality, or even
malicious, type implementation.

This is mitigated during deployment; the decision to install -- or not -- a
third party implementation is deferred to the database administrator or service
provider.

In terms of development burden, a native implementation can benefit from
existing software libraries available in the native language. For example,
implementing a C++ native IPv6 type is trivial when reusing `inet_ntop(3)` from
the existing C library.

In terms of runtime performances, compiled native code, reusing existing
libraries which are already well optimized for production, is the best solution.

### Architectural decision: native implementation

MySQL will support native implementation only, and not SQL, to implement a
user defined type.

Native is synonym to C++ currently.

Implementing native types in a different language (RUST) is not in scope,
but is in theory possible as long as the compiled code complies with the ABI
exposed by the server to implement a user defined type.

## Extension point

A third party implementation for a user defined type is installed
dynamically, the server build is unchanged.

A type implementation is therefore provided as a shared library.

### MySQL component

The MySQL component framework already provides the necessary infrastructure
to implement extensions, using services provided by the server itself, or
other components.

### MySQL plugin

Plugins also allows to install a shared library in the server.
Building a plugin however relies on server symbols, and is too
coupled with the server internal implementation to be distributed
independently.

Plugins are abandoned for this very reason, replaced by components.

Using a MySQL PLUGIN for a user defined type is not a viable
solution.

### Other packaging

Defining a different kind of extension point, beside COMPONENT, introduces
limitations and creates duplication in the code base.

Assuming there is a technical reason to have a different extension, a user
defined type implementation will still need to benefit from existing server
features, such as:

* performance schema instrumentation for mutexes, memory
* performance schema custom tables, provided by a UDT implementation
* system variables to configure a UDT implementation, for example to turn on
  a debugging trace
* existing server services, for example to print a debugging trace to the
  server logs.

### Architectural decision: MySQL component

User defined types are implemented using a MySQL COMPONENT.

Any limitation found in the component framework that impedes implementing
user defined types as components, is to be fixed in the component framework.

