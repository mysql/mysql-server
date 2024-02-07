/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "sql_parser_state.h"
#include "sql_splitting_allowed.h"

#include <gtest/gtest.h>

#include "mysql/harness/stdx/expected_ostream.h"
#include "mysql/harness/tls_context.h"
#include "sql_parser.h"

using Allowed = SplittingAllowedParser::Allowed;

static stdx::expected<SplittingAllowedParser::Allowed, std::string>
splitting_allowed(SqlLexer &&lexer) {
  return SplittingAllowedParser(lexer.begin(), lexer.end()).parse();
}

struct SharingAllowedParam {
  std::string_view stmt;

  stdx::expected<SplittingAllowedParser::Allowed, std::string> expected_result;
};

std::ostream &operator<<(std::ostream &os, Allowed allowed) {
  switch (allowed) {
    case Allowed::Always:
      os << "always";
      break;
    case Allowed::Never:
      os << "never";
      break;
    case Allowed::InTransaction:
      os << "in-transaction";
      break;
    case Allowed::OnlyReadOnly:
      os << "read-only";
      break;
    case Allowed::OnlyReadWrite:
      os << "read-write";
      break;
  }

  return os;
}

std::ostream &operator<<(std::ostream &os, const SharingAllowedParam &param) {
  os << param.stmt << ", " << param.expected_result;

  return os;
}

class SharingAllowedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<SharingAllowedParam> {};

TEST_P(SharingAllowedTest, works) {
  SqlParserState sql_parser_state;

  // set the statement in the parser.
  sql_parser_state.statement(GetParam().stmt);

  ASSERT_EQ(splitting_allowed(sql_parser_state.lexer()),
            GetParam().expected_result)
      << GetParam().stmt;
}

const SharingAllowedParam data_definition[] = {
    {"alter database", Allowed::Always},                       //
    {"alter event", Allowed::Always},                          //
    {"alter function", Allowed::Always},                       //
    {"alter instance", Allowed::Never},                        //
    {"alter logfile group", Allowed::Never},                   //
    {"alter procedure", Allowed::Always},                      //
    {"alter schema", Allowed::Always},                         // alias DATABASE
    {"alter server", Allowed::Never},                          //
    {"alter table", Allowed::Always},                          //
    {"alter tablespace", Allowed::Always},                     //
    {"alter undo tablespace", Allowed::Always},                //
    {"alter view", Allowed::Always},                           //
    {"create database", Allowed::Always},                      //
    {"create event", Allowed::Always},                         //
    {"create definer=current_user() event", Allowed::Always},  //
    {"create function", Allowed::Always},                      //
    {"create definer=current_user() function", Allowed::Always},      //
    {"create instance", Allowed::Never},                              //
    {"create index", Allowed::Always},                                //
    {"create unique index", Allowed::Always},                         //
    {"create fulltext index", Allowed::Always},                       //
    {"create spatial index", Allowed::Always},                        //
    {"create logfile group", Allowed::Never},                         //
    {"create procedure", Allowed::Always},                            //
    {"create definer=current_user() procedure", Allowed::Always},     //
    {"create schema", Allowed::Always},                               //
    {"create server", Allowed::Never},                                //
    {"create spatial reference system", Allowed::Always},             //
    {"create or replace spatial reference system", Allowed::Always},  //
    {"create table", Allowed::Always},                                //
    {"create temporary table", Allowed::Always},                      //
    {"create tablespace", Allowed::Always},                           //
    {"create undo tablespace", Allowed::Always},                      //
    {"create view", Allowed::Always},                                 //
    {"create or replace view", Allowed::Always},                      //
    {"create algorithm=undefined view", Allowed::Always},             //
    {"create definer=current_user() view", Allowed::Always},          //
    {"create sql security defined view", Allowed::Always},            //
    {"drop database", Allowed::Always},                               //
    {"drop event", Allowed::Always},                                  //
    {"drop function", Allowed::Always},                               //
    {"drop instance", Allowed::Never},                                //
    {"drop index", Allowed::Always},                                  //
    {"drop logfile group", Allowed::Never},                           //
    {"drop procedure", Allowed::Always},                              //
    {"drop role", Allowed::Always},                                   //
    {"drop schema", Allowed::Always},                                 //
    {"drop server", Allowed::Never},                                  //
    {"drop spatial reference system", Allowed::Always},               //
    {"drop table", Allowed::Always},                                  //
    {"drop temporary table", Allowed::Always},                        //
    {"drop tablespace", Allowed::Always},                             //
    {"drop view", Allowed::Always},                                   //
    {"RENAME TABLE tbl", Allowed::Always},                            //
    {"TRUNCATE TABLE tbl", Allowed::Always},                          //
    {"TRUNCATE tbl", Allowed::Always},  // TABLE is optional
};

INSTANTIATE_TEST_SUITE_P(Ddl, SharingAllowedTest,
                         ::testing::ValuesIn(data_definition));

const SharingAllowedParam data_manipulation[] = {
    {"call p1()", Allowed::Always},                  //
    {"delete from tbl", Allowed::Always},            //
    {"DO 1", Allowed::Always},                       //
    {"HANDLER open", Allowed::Never},                //
    {"IMPORT TABLE ", Allowed::Always},              //
    {"INSERt into tbl VALUES ()", Allowed::Always},  //
    {"load data", Allowed::Always},                  //
    {"load xml", Allowed::Always},                   //
    {"(SELECT 1)", Allowed::Always},                 //
    {"REPLACE", Allowed::Always},                    //
    {"SELECT 1", Allowed::Always},                   //
    {"TABLE tbl", Allowed::Always},                  //
    {"UPDATE tbl set foo = 1", Allowed::Always},     //
    {"values ROW(1,1)", Allowed::Always},            //
    {"WITH cte () SELECT 1", Allowed::Always},       //
};

INSTANTIATE_TEST_SUITE_P(Dml, SharingAllowedTest,
                         ::testing::ValuesIn(data_manipulation));

const SharingAllowedParam transaction_and_locking[] = {
    {"begin", Allowed::Always},                        //
    {"begin work", Allowed::Always},                   //
    {"start transaction", Allowed::Always},            //
    {"start transaction read only", Allowed::Always},  //
                                                       //
    {"commit", Allowed::Always},                       //
    {"rollback", Allowed::Always},                     //
    {"savepoint", Allowed::Always},                    //
    {"release savepoint", Allowed::Always},            //
    {"rollback to", Allowed::Always},                  //
    {"rollback to savepoint", Allowed::Always},        //
    {"rollback work to savepoint", Allowed::Always},   //
                                                       //
    {"lock tables", Allowed::Never},                   // instance
    {"unlock tables", Allowed::Never},                 // instance
                                                       //
    {"lock instance for backup", Allowed::Never},      // instance
    {"unlock instance", Allowed::Never},               // instance
                                                       //
    {"set transaction read only", Allowed::Always},    //
    {"xa begin", Allowed::Always},                     //
    {"xa start", Allowed::Always},                     //
    {"xa prepare", Allowed::Always},                   //
    {"xa commit", Allowed::Always},                    //
    {"xa rollback", Allowed::Always},                  //
    {"xa recover", Allowed::Always},                   //
};

INSTANTIATE_TEST_SUITE_P(Trx, SharingAllowedTest,
                         ::testing::ValuesIn(transaction_and_locking));

const SharingAllowedParam replication_source[] = {
    {"purge binary logs", Allowed::Never},             // instance
    {"reset binary logs and gtids ", Allowed::Never},  // instance
                                                       //
    {"set sql_log_bin = 1", Allowed::Always},          // session
};

INSTANTIATE_TEST_SUITE_P(ReplicationSource, SharingAllowedTest,
                         ::testing::ValuesIn(replication_source));

const SharingAllowedParam replication_replica[] = {
    {"change replication filter", Allowed::Never},     // instance
    {"change replication source to", Allowed::Never},  // instance
    {"reset replica", Allowed::Never},                 // instance
    {"start replica", Allowed::Never},                 // instance
    {"start group_replication", Allowed::Never},       // instance
    {"stop replica", Allowed::Never},                  // instance
    {"stop group_replication", Allowed::Never},        // instance
};

INSTANTIATE_TEST_SUITE_P(ReplicationReplica, SharingAllowedTest,
                         ::testing::ValuesIn(replication_replica));

const SharingAllowedParam prepared_stmt[] = {
    {"prepare", Allowed::Never},             // session, not tracked
    {"execute", Allowed::Never},             // session, not tracked
    {"deallocate prepare", Allowed::Never},  // session, not tracked
};

INSTANTIATE_TEST_SUITE_P(PreparedStatement, SharingAllowedTest,
                         ::testing::ValuesIn(prepared_stmt));

// Database Admin Statements

const SharingAllowedParam account_management_statements[] = {
    {"alter user", Allowed::Always},        //
    {"create role", Allowed::Always},       //
    {"create user", Allowed::Always},       //
    {"drop user", Allowed::Always},         //
    {"drop role", Allowed::Always},         //
    {"grant", Allowed::Always},             //
    {"revoke", Allowed::Always},            //
    {"set password", Allowed::Always},      //
    {"set default role", Allowed::Always},  //
    {"set role", Allowed::Always},          //
};

INSTANTIATE_TEST_SUITE_P(AccountManagement, SharingAllowedTest,
                         ::testing::ValuesIn(account_management_statements));

const SharingAllowedParam resource_group_management_statements[] = {
    {"alter resource group", Allowed::Never},   //
    {"create resource group", Allowed::Never},  //
    {"drop resource group", Allowed::Never},    //
    {"set resource group", Allowed::Never},     //
};

INSTANTIATE_TEST_SUITE_P(
    ResourceGroupManagement, SharingAllowedTest,
    ::testing::ValuesIn(resource_group_management_statements));

const SharingAllowedParam table_maintenance_statements[] = {
    {"ANAlyze table", Allowed::Always},   //
    {"CHECK table", Allowed::Always},     //
    {"CHECKSUM table", Allowed::Always},  //
    {"OPTIMIZE table", Allowed::Always},  //
    {"REPAIR table", Allowed::Always},    //
};

INSTANTIATE_TEST_SUITE_P(TableMaintenance, SharingAllowedTest,
                         ::testing::ValuesIn(table_maintenance_statements));

const SharingAllowedParam component_statements[] = {
    {"create function", Allowed::Always},  //
    {"create aggregate function if not exists foo returns string soname foo.so",
     Allowed::Always},                        //
    {"drop function", Allowed::Always},       //
    {"install component", Allowed::Never},    // instance
    {"uninstall component", Allowed::Never},  // instance
    {"install plugin", Allowed::Never},       // instance
    {"uninstall plugin", Allowed::Never},     // instance
};

INSTANTIATE_TEST_SUITE_P(Component, SharingAllowedTest,
                         ::testing::ValuesIn(component_statements));

const SharingAllowedParam clone_statements[] = {
    {"clone", Allowed::Never},  // instance
};

INSTANTIATE_TEST_SUITE_P(Clone, SharingAllowedTest,
                         ::testing::ValuesIn(clone_statements));

const SharingAllowedParam set_statements[] = {
    {"set default role", Allowed::Always},        //
    {"set global", Allowed::Never},               //
    {"set local", Allowed::Always},               //
    {"set names utf8", Allowed::Always},          //
    {"set persist", Allowed::Never},              //
    {"set persist_only", Allowed::Never},         //
    {"set resource group", Allowed::Never},       //
    {"set role", Allowed::Always},                //
    {"set session", Allowed::Always},             //
    {"set transaction", Allowed::Always},         //
    {"set sql_bin_log=0", Allowed::Always},       //
    {"set @u=0", Allowed::Always},                //
    {"set @@var=@@global.var", Allowed::Always},  //
};

INSTANTIATE_TEST_SUITE_P(Set, SharingAllowedTest,
                         ::testing::ValuesIn(set_statements));

const SharingAllowedParam show_statements[] = {
    {"SHOW", Allowed::Never},                               //
    {"SHOW binary logs", Allowed::OnlyReadWrite},           //
    {"SHOW binlog events", Allowed::OnlyReadOnly},          //
    {"SHOW character set", Allowed::Always},                //
    {"SHOW charset", Allowed::Always},                      //
    {"SHOW collation", Allowed::Always},                    //
    {"SHOW columns from tbl", Allowed::Always},             //
    {"SHOW full columns from tbl", Allowed::Always},        //
    {"SHOW create database db", Allowed::Always},           //
    {"SHOW create event ev", Allowed::Always},              //
    {"SHOW create function f", Allowed::Always},            //
    {"SHOW create procedure p", Allowed::Always},           //
    {"SHOW create table tbl", Allowed::Always},             //
    {"SHOW create trigger t", Allowed::Always},             //
    {"SHOW create view v", Allowed::Always},                //
    {"SHOW databases", Allowed::Always},                    //
    {"SHOW engine innodb status", Allowed::InTransaction},  //
    {"SHOW engines", Allowed::Always},                      //
    {"SHOW storage engines", Allowed::Always},              //
    {"SHOW errors", Allowed::Always},                       //
    {"SHOW events", Allowed::Always},                       //
    {"SHOW function code testing.t1", Allowed::Always},     //
    {"SHOW function status", Allowed::Always},              //
    {"SHOW grants for user", Allowed::Always},              //
    {"SHOW index from tbl", Allowed::Always},               //
    {"SHOW binary log status", Allowed::OnlyReadWrite},     //
    {"SHOW open tables", Allowed::InTransaction},           //
    {"SHOW plugins", Allowed::Always},                      //
    {"SHOW procedure code", Allowed::Always},               //
    {"SHOW procedure status", Allowed::Always},             //
    {"SHOW privileges", Allowed::Always},                   //
    {"SHOW processlist", Allowed::InTransaction},           //
    {"SHOW full processlist", Allowed::InTransaction},      //
    {"SHOW profile", Allowed::InTransaction},               //
    {"SHOW profiles", Allowed::InTransaction},              //
    {"SHOW relaylog", Allowed::OnlyReadOnly},               //
    {"SHOW replicas", Allowed::OnlyReadWrite},              //
    {"SHOW replica status", Allowed::OnlyReadOnly},         //
    {"SHOW global status", Allowed::InTransaction},         //
    {"SHOW session status", Allowed::Always},               //
    {"SHOW TABLES", Allowed::Always},                       //
    {"SHOW full TABLES", Allowed::Always},                  //
    {"SHOW TABLE status", Allowed::Always},                 //
    {"SHOW triggers", Allowed::Always},                     //
    {"SHOW global variables", Allowed::Always},             //
    {"SHOW session variables", Allowed::Always},            //
    {"SHOW warnings", Allowed::Always},                     //
};

INSTANTIATE_TEST_SUITE_P(Show, SharingAllowedTest,
                         ::testing::ValuesIn(show_statements));

const SharingAllowedParam other_admin_statements[] = {
    {"binlog", Allowed::Always},                              // binlog event
    {"cache index", Allowed::Never},                          //
    {"flush", Allowed::Always},                               // replicated
    {"flush privileges", Allowed::Always},                    // replicated
    {"flush binary logs", Allowed::Always},                   // binlog event
    {"flush local privileges", Allowed::Never},               // not replicated
    {"flush no_write_to_binlog privileges", Allowed::Never},  // not replicated
    {"flush logs", Allowed::Never},                           // not replicated
    {"kill", Allowed::Never},                                 //
    {"load index into cache", Allowed::Never},                //
    {"reset", Allowed::Never},                                //
    {"reset persist", Allowed::Never},                        //
    {"restart", Allowed::Never},                              //
    {"shutdown", Allowed::Never},                             //
};

INSTANTIATE_TEST_SUITE_P(OtherAdmin, SharingAllowedTest,
                         ::testing::ValuesIn(other_admin_statements));

const SharingAllowedParam utility_stmt[] = {
    {"describe tbl", Allowed::Always},                         //
    {"desc tbl", Allowed::Always},                             //
    {"explain select", Allowed::Always},                       //
    {"explain analyze select", Allowed::Always},               //
    {"explain format=tree select 1", Allowed::Always},         //
    {"explain format=tree table tbl", Allowed::Always},        //
    {"explain format=tree delete from tbl", Allowed::Always},  //
    {"explain format=tree insert into 1", Allowed::Always},    //
    {"explain format=tree replace into 1", Allowed::Always},   //
    {"explain format=tree update into 1", Allowed::Always},    //
    {"help foo", Allowed::Always},                             //
    {"use db", Allowed::Always},                               //
};

INSTANTIATE_TEST_SUITE_P(Utility, SharingAllowedTest,
                         ::testing::ValuesIn(utility_stmt));

const SharingAllowedParam fail_stmts[] = {
    {"select '", Allowed::Always},   // SELECT, '
    {"select \"", Allowed::Always},  // SELECT, "
    {"select `", Allowed::Always},   // SELECT, `
};

INSTANTIATE_TEST_SUITE_P(Fail, SharingAllowedTest,
                         ::testing::ValuesIn(fail_stmts));

int main(int argc, char *argv[]) {
  TlsLibraryContext lib_ctx;

  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
