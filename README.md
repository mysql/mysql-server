# ASQL: An Attribute-Based Access Control extension for MySQL

This repository contains the source code for ASQL, an attribute-based access control (ABAC) extension for MySQL. ASQL has been natively incorporated into the existing authorization framework in MySQL, which has pre-existing support for Role-Based Access Control (RBAC) and Discretionary Access Control (DAC).

### Installation
ASQL is natively incorporated into MySQL and is available on a recompiled instance of MySQL-server.

### Syntax and Semantics
ASQL provides a syntax for policy specification and management. The syntax has been kept close to that for DAC and RBAC, thus making migration and re-training easier.
- CREATE RULE rule_name FOR priv_type [, priv_type] ... OF [USER ATTRIBUTE {user_attribute = value|ANY, user_attribute = value| ANY ...}] [AND RESOURCE ATTRIBUTE {resource_attribute = value| ANY, resource_attribute
= value | ANY ...}]
- DROP RULE rule_name
- CREATE USER ATTRIBUTE user_attribute_name
- CREATE RESOURCE ATTRIBUTE resource_attribute_name
- DROP USER ATTRIBUTE user_attribute_name
- DROP RESOURCE ATTRIBUTE resource_attribute_name
- GRANT USER ATTRIBUTE user_attribute = value TO user [, user] ...
- GRANT RESOURCE ATTRIBUTE resource_attribute = value TO resource [, resource] ...
- REVOKE USER ATTRIBUTE user_attribute [= value | ALL] FROM user [, user] ...
- REVOKE RESOURCE ATTRIBUTE resource_attribute [= value | ALL] FROM resource [, resource] ...

The semantics for the statements are briefly explained below: 
- CREATE RULE statement: Used to create a rule in the ABAC policy. If a particular user or resource attribute is omitted from the expression, it is considered to match any value. Various privileges can be specified depending on the desired granularity of the ABAC instance in the database. For example, privileges like INSERT, DELETE, SELECT and UPDATE can be applied on tables or columns. On the other hand, certain
privileges like SHOW DATABASES are applicable only at a global level.
- DROP RULE statement: Deletes a rule from the policy.
- CREATE USER (RESOURCE) ATTRIBUTE statement: Allows us to create a new user (resource) attribute.
- DROP USER (RESOURCE) ATTRIBUTE statement: Deletes a user (resource) attribute from the system. It also removes any user (resource) condition in the policy that involves the given attribute.
- GRANT USER (RESOURCE) ATTRIBUTE statement: Assigns an attribute value pair to the given list of users (resources).
- REVOKE USER (RESOURCE) ATTRIBUTE statement: If no value is specified, then it revokes the value of the given attribute from the given list of users (resources). However, if a particular value is mentioned in the statement, the revoke occurs only for users (resources) who have been granted the given value for the attribute.

### Limitations
- The current version of ASQL doesn't provide support for environment variables. This was left out of the scope of our implementation since the development would require a major overhaul of the existing authorization support in MySQL.

### Further reading
Our work on ASQL has been described in [this](https://dl.acm.org/doi/abs/10.1145/3532105.3535033) paper, which was published in ACM SACMAT 2022.
