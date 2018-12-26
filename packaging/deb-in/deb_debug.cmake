# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

SET (DEB_RULES_DEBUG_CMAKE
"
	mkdir debug && cd debug && \\
	cmake .. \\
		-DBUILD_CONFIG=mysql_release \\
		-DCMAKE_INSTALL_PREFIX=/usr \\
		-DCMAKE_BUILD_TYPE=Debug \\
		-DINSTALL_DOCDIR=share/mysql/docs \\
		-DINSTALL_DOCREADMEDIR=share/mysql \\
		-DINSTALL_INCLUDEDIR=include/mysql \\
		-DINSTALL_INFODIR=share/mysql/docs \\
		-DINSTALL_LIBDIR=lib/$(DEB_HOST_MULTIARCH) \\
		-DINSTALL_MANDIR=share/man \\
		-DINSTALL_MYSQLSHAREDIR=share/mysql \\
		-DINSTALL_MYSQLTESTDIR=lib/mysql-test \\
		-DINSTALL_PLUGINDIR=lib/mysql/plugin \\
		-DINSTALL_SBINDIR=sbin \\
		-DINSTALL_SUPPORTFILESDIR=share/mysql \\
		-DSYSCONFDIR=/etc/mysql \\
		-DMYSQL_UNIX_ADDR=/var/run/mysqld/mysqld.sock \\
		-DWITH_SSL=bundled \\
		-DWITH_INNODB_MEMCACHED=1 \\
		-DWITH_MECAB=system \\
		-DWITH_NUMA=ON \\
		-DCOMPILATION_COMMENT=\"MySQL ${DEB_PRODUCTNAMEC} Server (${DEB_LICENSENAME})\" \\
		-DINSTALL_LAYOUT=DEB \\
		-DDEB_PRODUCT=${DEB_PRODUCT} \\
		${DEB_CMAKE_EXTRAS}
")

SET (DEB_RULES_DEBUG_MAKE
"
	cd debug && \\
	$(MAKE) -j8 VERBOSE=1
")

SET (DEB_RULES_DEBUG_EXTRA
"
	# The ini file isn't built for debug, so copy over from the standard build
	install -g root -o root -m 0755 debian/tmp/usr/lib/mysql/plugin/daemon_example.ini debian/tmp/usr/lib/mysql/plugin/debug
")

SET (DEB_INSTALL_DEBUG_SERVER
"
# debug binary
usr/sbin/mysqld-debug
")

SET (DEB_INSTALL_DEBUG_SERVER_PLUGINS
"
# debug plugins
usr/lib/mysql/plugin/debug/adt_null.so
usr/lib/mysql/plugin/debug/auth_socket.so
usr/lib/mysql/plugin/debug/authentication_ldap_sasl_client.so
usr/lib/mysql/plugin/debug/group_replication.so
usr/lib/mysql/plugin/debug/connection_control.so
usr/lib/mysql/plugin/debug/innodb_engine.so
usr/lib/mysql/plugin/debug/keyring_file.so
usr/lib/mysql/plugin/debug/keyring_udf.so
usr/lib/mysql/plugin/debug/libmemcached.so
usr/lib/mysql/plugin/debug/libpluginmecab.so
usr/lib/mysql/plugin/debug/locking_service.so
usr/lib/mysql/plugin/debug/mypluglib.so
usr/lib/mysql/plugin/debug/mysql_no_login.so
usr/lib/mysql/plugin/debug/mysqlx.so
usr/lib/mysql/plugin/debug/rewriter.so
usr/lib/mysql/plugin/debug/semisync_master.so
usr/lib/mysql/plugin/debug/semisync_slave.so
usr/lib/mysql/plugin/debug/validate_password.so
usr/lib/mysql/plugin/debug/version_token.so
")

SET (DEB_INSTALL_DEBUG_TEST_PLUGINS
"
# debug plugins
usr/lib/mysql/plugin/debug/auth.so
usr/lib/mysql/plugin/debug/auth_test_plugin.so
usr/lib/mysql/plugin/debug/daemon_example.ini
usr/lib/mysql/plugin/debug/ha_example.so
usr/lib/mysql/plugin/debug/libdaemon_example.so
usr/lib/mysql/plugin/debug/libtest_framework.so
usr/lib/mysql/plugin/debug/libtest_services.so
usr/lib/mysql/plugin/debug/libtest_services_threaded.so
usr/lib/mysql/plugin/debug/libtest_session_detach.so
usr/lib/mysql/plugin/debug/libtest_session_info.so
usr/lib/mysql/plugin/debug/libtest_session_in_thd.so
usr/lib/mysql/plugin/debug/libtest_sql_2_sessions.so
usr/lib/mysql/plugin/debug/libtest_sql_all_col_types.so
usr/lib/mysql/plugin/debug/libtest_sql_cmds_1.so
usr/lib/mysql/plugin/debug/libtest_sql_commit.so
usr/lib/mysql/plugin/debug/libtest_sql_complex.so
usr/lib/mysql/plugin/debug/libtest_sql_errors.so
usr/lib/mysql/plugin/debug/libtest_sql_lock.so
usr/lib/mysql/plugin/debug/libtest_sql_processlist.so
usr/lib/mysql/plugin/debug/libtest_sql_replication.so
usr/lib/mysql/plugin/debug/libtest_sql_shutdown.so
usr/lib/mysql/plugin/debug/libtest_sql_sqlmode.so
usr/lib/mysql/plugin/debug/libtest_sql_stored_procedures_functions.so
usr/lib/mysql/plugin/debug/libtest_sql_views_triggers.so
usr/lib/mysql/plugin/debug/libtest_x_sessions_deinit.so
usr/lib/mysql/plugin/debug/libtest_x_sessions_init.so
usr/lib/mysql/plugin/debug/qa_auth_client.so
usr/lib/mysql/plugin/debug/qa_auth_interface.so
usr/lib/mysql/plugin/debug/qa_auth_server.so
usr/lib/mysql/plugin/debug/replication_observers_example_plugin.so
usr/lib/mysql/plugin/debug/rewrite_example.so
usr/lib/mysql/plugin/debug/test_security_context.so
usr/lib/mysql/plugin/debug/test_udf_services.so
")
