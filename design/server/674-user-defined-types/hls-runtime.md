
# MySQL Runtime

## UDT cache

An internal memory cache maintains an image of user defined types.

Runtime operations operate primarily on the cache,
not on the persistent data dictionary metadata representing a type.

### UDT cache states

A TYPE can be in two states:

* UNBOUND, there is no implementation attached to the type
* BOUND, there is an implementation attached to the type

### UDT cache reference counts

The UDT cache maintains reference counts for:

* types
* methods

### UDT cache transitions

#### INSTALL COMPONENT

Upon INSTALL COMPONENT, a component implementing a type:

* invokes services to register a type implementation
* invoke services to register a method implementation on a type

Registering a type:

* validates that the UDT exists in the data dictionary
* creates an entry in the UDT cache if necessary
* validates that the UDT cache is in UNBOUND state,
  thereby enforcing that the same type can not be claimed multiple
  times
* validates that the type metadata provided by the component
  is consistent with the type metadata stored in the data dictionary
* updates the cache to a BOUND state

Registering a method:

* validates that the UDT method exists in the data dictionary
* creates an entry for the method in the UDT cache if necessary
* validates that the UDT method is in UNBOUND state,
  thereby enforcing that the same method can not be claimed multiple
  times
* validates that the method metadata provided by the component
  is consistent with the method metadata stored in the data dictionary
* updates the cache method to a BOUND state

#### UNINSTALL COMPONENT

Upon UNINSTALL COMPONENT, a component implementing a type:

* invoke services to unregister a method implementation from a type
* invokes services to unregister a type implementation

Unregistering a method:

* updates the cache method to a UNBOUND state

Unregistering a type:

* updates the cache to a UNBOUND state

#### Statement start

When a statement starts, any type or method needed during runtime evaluation
is looked up in the UDT runtime cache.

Execution verifies the UDT is in BOUND state, or fails with an error if
UNBOUND: there is no implementation loaded to support the operation.

Execution then increments the reference counters.

#### Statement end

During statement cleanup, reference counters incremented during the
statement execution are decremented.

#### TYPE DDL

Any type DDL such as:

* CREATE TYPE
* ALTER TYPE
* DROP TYPE
* CREATE ORDERING
* DROP ORDERING

must first verify that the type is not already present in the UDT cache,
with a BOUND state or active reference counts.

The DDL operation fails with an error in these cases (TYPE in use).

Users must first UNINSTALL COMPONENT to remove a type implementation,
to perform DDL on a type.

