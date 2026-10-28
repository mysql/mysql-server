
# Metadata locking (MDL)

Extend the metadata locks to cover a new object type:

TYPE `schema_name` `object_name`

`TYPE` is a new mdl namespace

The `schema_name` part of a user defined type follows the same rules as
SCHEMA, for normalization (lower_case_table_names, my_casedn_str).

The `object_name` part of a user defined type follows the same rules as
FUNCTION, PROCEDURE, EVENT and RESOURCE_GROUPS:
names are case and accent insensitive.
