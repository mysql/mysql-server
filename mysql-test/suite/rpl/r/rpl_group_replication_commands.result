call mtr.add_suppression("Group Replication plugin is not installed.");
START GROUP_REPLICATION;
ERROR HY000: The server is not configured properly to be an active member of the group. Please see more details on error log.
STOP GROUP_REPLICATION;
ERROR HY000: The server is not configured properly to be an active member of the group. Please see more details on error log.
CREATE USER 'group_rpl_user' IDENTIFIED BY '';
START GROUP_REPLICATION;
ERROR 42000: Access denied; you need (at least one of) the SUPER or GROUP_REPLICATION_ADMIN privilege(s) for this operation
STOP GROUP_REPLICATION;
ERROR 42000: Access denied; you need (at least one of) the SUPER or GROUP_REPLICATION_ADMIN privilege(s) for this operation
GRANT GROUP_REPLICATION_ADMIN ON *.* TO group_rpl_user;
START GROUP_REPLICATION;
ERROR HY000: The server is not configured properly to be an active member of the group. Please see more details on error log.
STOP GROUP_REPLICATION;
ERROR HY000: The server is not configured properly to be an active member of the group. Please see more details on error log.
DROP USER group_rpl_user;
