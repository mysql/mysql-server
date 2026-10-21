
# Requirements

## Functional requirements

### MySQL Editions

Support for User Data Type (UDT) is available in all editions of the server:

* Community edition
* Enterprise edition
* Cloud editions

When a party provides a specific User Data Type implementation, the type delivered is available, at the provider choice, to either:

* All editions (community, enterprise, cloud)
* Enterprise and cloud only
* Cloud only

### Deployment

Deployment consist of:

* adding a specific User Data Type to a MySQL server instance
* removing a specific User Data Type from a MySQL server instance

Deployment is dynamic, and does not require a server restart.

Deployment is a privileged operation.

### Discoverability

Because User Data Types installed in a server are optional, users need to discover what is available.

An interface in SQL is exposed by the server to list all available User Data Types.

An interface in SQL is exposed by the server to list all operations available for a given User Data Type.

An interface in SQL is exposed by the server to describe the full operation signature for a given operation on a given User Data Type.


## Non functional requirements

TODO

