/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/tests/driver/connector/mysqlx_all_msgs.h"

#include "plugin/x/client/mysqlxclient/xmessage.h"


Message_by_full_name server_msgs_by_full_name;
Message_by_full_name client_msgs_by_full_name;

Message_server_by_name server_msgs_by_name;
Message_client_by_name client_msgs_by_name;

Message_server_by_id server_msgs_by_id;
Message_client_by_id client_msgs_by_id;

static struct init_message_factory {
  template <class C>
  static xcl::XProtocol::Message *create() {
    return new C();
  }

  template <typename T, typename Message_type_id>
  void server_message(Message_type_id id, const std::string &name,
                      const std::string &full_name) {
    server_msgs_by_name[name] = std::make_pair(&create<T>, id);
    server_msgs_by_id[id] = std::make_pair(&create<T>, name);
    server_msgs_by_full_name[full_name] = name;
  }

  template <typename T, typename Message_type_id>
  void client_message(Message_type_id id, const std::string &name,
                      const std::string &full_name) {
    client_msgs_by_name[name] = std::make_pair(&create<T>, id);
    client_msgs_by_id[id] = std::make_pair(&create<T>, name);
    client_msgs_by_full_name[full_name] = name;
  }

  init_message_factory() {
    server_message<Mysqlx::Connection::Capabilities>(
        Mysqlx::ServerMessages::CONN_CAPABILITIES, "CONN_CAPABILITIES",
        "Mysqlx.Connection.Capabilities");
    server_message<Mysqlx::Error>(Mysqlx::ServerMessages::ERROR, "ERROR",
                                  "Mysqlx.Error");
    server_message<Mysqlx::Notice::Frame>(Mysqlx::ServerMessages::NOTICE,
                                          "NOTICE", "Mysqlx.Notice.Frame");
    server_message<Mysqlx::Ok>(Mysqlx::ServerMessages::OK, "OK", "Mysqlx.Ok");
    server_message<Mysqlx::Resultset::ColumnMetaData>(
        Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA,
        "RESULTSET_COLUMN_META_DATA", "Mysqlx.Resultset.ColumnMetaData");
    server_message<Mysqlx::Resultset::FetchDone>(
        Mysqlx::ServerMessages::RESULTSET_FETCH_DONE, "RESULTSET_FETCH_DONE",
        "Mysqlx.Resultset.FetchDone");
    server_message<Mysqlx::Resultset::FetchDoneMoreResultsets>(
        Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS,
        "RESULTSET_FETCH_DONE_MORE_RESULTSETS",
        "Mysqlx.Resultset.FetchDoneMoreResultsets");
    server_message<Mysqlx::Resultset::Row>(
        Mysqlx::ServerMessages::RESULTSET_ROW, "RESULTSET_ROW",
        "Mysqlx.Resultset.Row");
    server_message<Mysqlx::Session::AuthenticateOk>(
        Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK, "SESS_AUTHENTICATE_OK",
        "Mysqlx.Session.AuthenticateOk");
    server_message<Mysqlx::Sql::StmtExecuteOk>(
        Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK, "SQL_STMT_EXECUTE_OK",
        "Mysqlx.Sql.StmtExecuteOk");

    client_message<Mysqlx::Connection::CapabilitiesGet>(
        Mysqlx::ClientMessages::CON_CAPABILITIES_GET, "CON_CAPABILITIES_GET",
        "Mysqlx.Connection.CapabilitiesGet");
    client_message<Mysqlx::Connection::CapabilitiesSet>(
        Mysqlx::ClientMessages::CON_CAPABILITIES_SET, "CON_CAPABILITIES_SET",
        "Mysqlx.Connection.CapabilitiesSet");
    client_message<Mysqlx::Connection::Close>(Mysqlx::ClientMessages::CON_CLOSE,
                                              "CON_CLOSE",
                                              "Mysqlx.Connection.Close");
    client_message<Mysqlx::Crud::Delete>(Mysqlx::ClientMessages::CRUD_DELETE,
                                         "CRUD_DELETE", "Mysqlx.Crud.Delete");
    client_message<Mysqlx::Crud::Find>(Mysqlx::ClientMessages::CRUD_FIND,
                                       "CRUD_FIND", "Mysqlx.Crud.Find");
    client_message<Mysqlx::Crud::Insert>(Mysqlx::ClientMessages::CRUD_INSERT,
                                         "CRUD_INSERT", "Mysqlx.Crud.Insert");
    client_message<Mysqlx::Crud::Update>(Mysqlx::ClientMessages::CRUD_UPDATE,
                                         "CRUD_UPDATE", "Mysqlx.Crud.Update");
    client_message<Mysqlx::Crud::CreateView>(
        Mysqlx::ClientMessages::CRUD_CREATE_VIEW, "CRUD_CREATE_VIEW",
        "Mysqlx.Crud.CreateView");
    client_message<Mysqlx::Crud::ModifyView>(
        Mysqlx::ClientMessages::CRUD_MODIFY_VIEW, "CRUD_MODIFY_VIEW",
        "Mysqlx.Crud.ModifyView");
    client_message<Mysqlx::Crud::DropView>(
        Mysqlx::ClientMessages::CRUD_DROP_VIEW, "CRUD_DROP_VIEW",
        "Mysqlx.Crud.DropView");
    client_message<Mysqlx::Expect::Close>(Mysqlx::ClientMessages::EXPECT_CLOSE,
                                          "EXPECT_CLOSE",
                                          "Mysqlx.Expect.Close");
    client_message<Mysqlx::Expect::Open>(Mysqlx::ClientMessages::EXPECT_OPEN,
                                         "EXPECT_OPEN", "Mysqlx.Expect.Open");
    client_message<Mysqlx::Session::AuthenticateContinue>(
        Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE,
        "SESS_AUTHENTICATE_CONTINUE", "Mysqlx.Session.AuthenticateContinue");
    client_message<Mysqlx::Session::AuthenticateStart>(
        Mysqlx::ClientMessages::SESS_AUTHENTICATE_START,
        "SESS_AUTHENTICATE_START", "Mysqlx.Session.AuthenticateStart");
    client_message<Mysqlx::Session::Close>(Mysqlx::ClientMessages::SESS_CLOSE,
                                           "SESS_CLOSE",
                                           "Mysqlx.Session.Close");
    client_message<Mysqlx::Session::Reset>(Mysqlx::ClientMessages::SESS_RESET,
                                           "SESS_RESET",
                                           "Mysqlx.Session.Reset");
    client_message<Mysqlx::Sql::StmtExecute>(
        Mysqlx::ClientMessages::SQL_STMT_EXECUTE, "SQL_STMT_EXECUTE",
        "Mysqlx.Sql.StmtExecute");
  }
} init_message_factory;

