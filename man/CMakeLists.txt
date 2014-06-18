# Copyright (c) 2012, Monty Program Ab
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

SET(MAN1_SERVER innochecksum.1 my_print_defaults.1 myisam_ftdump.1 myisamchk.1
                aria_chk.1 aria_dump_log.1 aria_ftdump.1 aria_pack.1 aria_read_log.1
		myisamlog.1 myisampack.1 mysql.server.1
		mysql_convert_table_format.1 mysql_fix_extensions.1
		mysql_install_db.1
		mysql_secure_installation.1 mysql_setpermission.1
		mysql_tzinfo_to_sql.1 mysql_upgrade.1 mysql_zap.1
		mysqld_multi.1 mysqld_safe.1 mysqldumpslow.1 mysqlhotcopy.1
		mysqltest.1 perror.1 replace.1 resolve_stack_dump.1
		resolveip.1 mysqlbug.1)
SET(MAN8_SERVER mysqld.8)
SET(MAN1_CLIENT msql2mysql.1 mysql.1 mysql_find_rows.1 mysql_waitpid.1
		mysqlaccess.1 mysqladmin.1 mysqlbinlog.1 mysqlcheck.1
		mysqldump.1 mysqlimport.1 mysqlshow.1 mysqlslap.1
                mysql_plugin.1)
SET(MAN1_DEVEL mysql_config.1)
SET(MAN1_TEST mysql-stress-test.pl.1 mysql-test-run.pl.1 mysql_client_test.1
              mysqltest_embedded.1 mysql_client_test_embedded.1)

INSTALL(FILES ${MAN1_SERVER} DESTINATION ${INSTALL_MANDIR}/man1 COMPONENT ManPagesServer)
INSTALL(FILES ${MAN8_SERVER} DESTINATION ${INSTALL_MANDIR}/man8 COMPONENT ManPagesServer)
INSTALL(FILES ${MAN1_CLIENT} DESTINATION ${INSTALL_MANDIR}/man1 COMPONENT ManPagesClient)
INSTALL(FILES ${MAN1_DEVEL} DESTINATION ${INSTALL_MANDIR}/man1 COMPONENT ManPagesDevelopment)
INSTALL(FILES ${MAN1_TEST} DESTINATION ${INSTALL_MANDIR}/man1 COMPONENT ManPagesTest)

