.. Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

Comparison to MySQL C/S Protocol
================================

============================= ======================= ===============
Feature                       mysql C/S protocol      mysqlx protocol
============================= ======================= ===============
plaintext auth                |yes|                   |yes| via SASL
extentisible auth             |yes| (5.6)             |yes| via SASL
TLS extension                 |yes|                   |yes|
max message size >= 1Gb       |yes|                   |yes|
compression extension         |yes|                   |yes|
resettable sessions           `COM_RESET_CONNECTION`_ :protobuf:msg:`Mysqlx.Session::Reset`
multiple, sequential sessions `COM_CHANGE_USER`_      :protobuf:msg:`Mysqlx.Session::AuthenticateStart`
multiple, parallel sessions   |no|                    :doc:`mysqlx-protocol-multiplexing`
out-of-band notifications     |no|                    :protobuf:msg:`Mysqlx.Notice::`
extensible messages           |no|                    via protobuf
extensible protocol           via capability flags    :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`
prepared SQL                  |yes|                   :protobuf:msg:`Mysqlx.Sql::`
prepared CRUD                 |no|                    :protobuf:msg:`Mysqlx.Crud::`
multi-statement               |yes| (5.0)             |no|
multi-resultset               |yes| (5.0)             :protobuf:msg:`Mysqlx.Resultset::FetchDoneMoreResultsets`
OUT-paramset                  |yes| (5.5)             :protobuf:msg:`Mysqlx.Resultset::FetchDoneMoreOutParams`
============================= ======================= ===============

.. |yes| unicode:: U+2713 .. yes
.. |no| unicode:: U+2715  .. no

Mapping of MySQL C/S ``COM_`` to Mysqlx Messages
------------------------------------------------

============================= ===============
Command                       mysqlx protocol
============================= ===============
`COM_QUIT`_                   :protobuf:msg:`Mysqlx.Connection::Close`
`COM_INIT_DB`_                .. todo:: add way to set session variables
`COM_QUERY`_                  no, use prepared statements instead
`COM_FIELD_LIST`_             no, use SQL commands instead
`COM_CREATE_DB`_              .. todo:: add CRUD messages to create collections
`COM_DROP_DB`_                .. todo:: add CRUD messages to drop collections
`COM_REFRESH`_                no, use SQL commands instead
`COM_SHUTDOWN`_               .. todo:: document how to execute a ``SHUTDOWN``
`COM_STATISTICS`_             no, use SQL commands instead
`COM_PROCESS_INFO`_           no, use SQL commands instead
`COM_PROCESS_KILL`_           no, use SQL commands instead
`COM_DEBUG`_                  no
`COM_PING`_                   .. todo:: add a ``PING`` message
`COM_CHANGE_USER`_            :protobuf:msg:`Mysqlx.Session::AuthenticateStart`
`COM_RESET_CONNECTION`_       :protobuf:msg:`Mysqlx.Session::Reset`
`COM_STMT_PREPARE`_           :protobuf:msg:`Mysqlx.PreparedStmt::Prepare`
`COM_STMT_SEND_LONG_DATA`_    not implemented
`COM_STMT_EXECUTE`_           :protobuf:msg:`Mysqlx.PreparedStmt::Execute`
`COM_STMT_CLOSE`_             :protobuf:msg:`Mysqlx.PreparedStmt::Close`
`COM_STMT_RESET`_             not implemented
`COM_SET_OPTION`_             not needed
`COM_STMT_FETCH`_             :protobuf:msg:`Mysqlx.Cursor::FetchResultset`
============================= ===============

.. _COM_QUIT: http://dev.mysql.com/doc/internals/en/com-quit.html
.. _COM_INIT_DB: http://dev.mysql.com/doc/internals/en/com-init-db.html
.. _COM_QUERY: http://dev.mysql.com/doc/internals/en/com-query.html
.. _COM_FIELD_LIST: http://dev.mysql.com/doc/internals/en/com-field-list.html
.. _COM_CREATE_DB: http://dev.mysql.com/doc/internals/en/com-create-db.html
.. _COM_DROP_DB: http://dev.mysql.com/doc/internals/en/com-drop-db.html
.. _COM_REFRESH: http://dev.mysql.com/doc/internals/en/com-refresh.html
.. _COM_SHUTDOWN: http://dev.mysql.com/doc/internals/en/com-shutdown.html
.. _COM_STATISTICS: http://dev.mysql.com/doc/internals/en/com-statistics.html
.. _COM_PROCESS_INFO: http://dev.mysql.com/doc/internals/en/com-process-info.html
.. _COM_PROCESS_KILL: http://dev.mysql.com/doc/internals/en/com-process-kill.html
.. _COM_DEBUG: http://dev.mysql.com/doc/internals/en/com-debug.html
.. _COM_PING: http://dev.mysql.com/doc/internals/en/com-ping.html
.. _COM_CHANGE_USER: http://dev.mysql.com/doc/internals/en/com-change-user.html
.. _COM_RESET_CONNECTION: http://dev.mysql.com/doc/internals/en/com-reset-connection.html
.. _COM_STMT_PREPARE: http://dev.mysql.com/doc/internals/en/com-stmt-prepare.html
.. _COM_STMT_SEND_LONG_DATA: http://dev.mysql.com/doc/internals/en/com-stmt-send-long-data.html
.. _COM_STMT_EXECUTE: http://dev.mysql.com/doc/internals/en/com-stmt-execute.html
.. _COM_STMT_CLOSE: http://dev.mysql.com/doc/internals/en/com-stmt-close.html
.. _COM_STMT_RESET: http://dev.mysql.com/doc/internals/en/com-stmt-reset.html
.. _COM_SET_OPTION: http://dev.mysql.com/doc/internals/en/com-set-option.html
.. _COM_STMT_FETCH: http://dev.mysql.com/doc/internals/en/com-stmt-fetch.html

