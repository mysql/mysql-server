/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

template<class C>
mysqlx::Message *create()
{
  return new C();
}

static struct init_message_factory
{
  init_message_factory()
  {
    server_msgs_by_name["NOTICE"] = std::make_pair(&create<Mysqlx::Notice::Frame>, Mysqlx::ServerMessages::NOTICE);
    server_msgs_by_id[Mysqlx::ServerMessages::NOTICE] = std::make_pair(&create<Mysqlx::Notice::Frame>, "NOTICE");
    server_msgs_by_full_name["Mysqlx.Notice.Frame"] = "NOTICE";
    server_msgs_by_name["RESULTSET_FETCH_DONE_MORE_RESULTSETS"] = std::make_pair(&create<Mysqlx::Resultset::FetchDoneMoreResultsets>, Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS);
    server_msgs_by_id[Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS] = std::make_pair(&create<Mysqlx::Resultset::FetchDoneMoreResultsets>, "RESULTSET_FETCH_DONE_MORE_RESULTSETS");
    server_msgs_by_full_name["Mysqlx.Resultset.FetchDoneMoreResultsets"] = "RESULTSET_FETCH_DONE_MORE_RESULTSETS";
    server_msgs_by_name["CONN_CAPABILITIES"] = std::make_pair(&create<Mysqlx::Connection::Capabilities>, Mysqlx::ServerMessages::CONN_CAPABILITIES);
    server_msgs_by_id[Mysqlx::ServerMessages::CONN_CAPABILITIES] = std::make_pair(&create<Mysqlx::Connection::Capabilities>, "CONN_CAPABILITIES");
    server_msgs_by_full_name["Mysqlx.Connection.Capabilities"] = "CONN_CAPABILITIES";
    server_msgs_by_name["RESULTSET_ROW"] = std::make_pair(&create<Mysqlx::Resultset::Row>, Mysqlx::ServerMessages::RESULTSET_ROW);
    server_msgs_by_id[Mysqlx::ServerMessages::RESULTSET_ROW] = std::make_pair(&create<Mysqlx::Resultset::Row>, "RESULTSET_ROW");
    server_msgs_by_full_name["Mysqlx.Resultset.Row"] = "RESULTSET_ROW";
    server_msgs_by_name["RESULTSET_FETCH_DONE"] = std::make_pair(&create<Mysqlx::Resultset::FetchDone>, Mysqlx::ServerMessages::RESULTSET_FETCH_DONE);
    server_msgs_by_id[Mysqlx::ServerMessages::RESULTSET_FETCH_DONE] = std::make_pair(&create<Mysqlx::Resultset::FetchDone>, "RESULTSET_FETCH_DONE");
    server_msgs_by_full_name["Mysqlx.Resultset.FetchDone"] = "RESULTSET_FETCH_DONE";
    server_msgs_by_name["SQL_STMT_EXECUTE_OK"] = std::make_pair(&create<Mysqlx::Sql::StmtExecuteOk>, Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK);
    server_msgs_by_id[Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK] = std::make_pair(&create<Mysqlx::Sql::StmtExecuteOk>, "SQL_STMT_EXECUTE_OK");
    server_msgs_by_full_name["Mysqlx.Sql.StmtExecuteOk"] = "SQL_STMT_EXECUTE_OK";
    server_msgs_by_name["ERROR"] = std::make_pair(&create<Mysqlx::Error>, Mysqlx::ServerMessages::ERROR);
    server_msgs_by_id[Mysqlx::ServerMessages::ERROR] = std::make_pair(&create<Mysqlx::Error>, "ERROR");
    server_msgs_by_full_name["Mysqlx.Error"] = "ERROR";
    server_msgs_by_name["OK"] = std::make_pair(&create<Mysqlx::Ok>, Mysqlx::ServerMessages::OK);
    server_msgs_by_id[Mysqlx::ServerMessages::OK] = std::make_pair(&create<Mysqlx::Ok>, "OK");
    server_msgs_by_full_name["Mysqlx.Ok"] = "OK";
    server_msgs_by_name["RESULTSET_COLUMN_META_DATA"] = std::make_pair(&create<Mysqlx::Resultset::ColumnMetaData>, Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA);
    server_msgs_by_id[Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA] = std::make_pair(&create<Mysqlx::Resultset::ColumnMetaData>, "RESULTSET_COLUMN_META_DATA");
    server_msgs_by_full_name["Mysqlx.Resultset.ColumnMetaData"] = "RESULTSET_COLUMN_META_DATA";
    server_msgs_by_name["SESS_AUTHENTICATE_OK"] = std::make_pair(&create<Mysqlx::Session::AuthenticateOk>, Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK);
    server_msgs_by_id[Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK] = std::make_pair(&create<Mysqlx::Session::AuthenticateOk>, "SESS_AUTHENTICATE_OK");
    server_msgs_by_full_name["Mysqlx.Session.AuthenticateOk"] = "SESS_AUTHENTICATE_OK";
    client_msgs_by_full_name["Mysqlx.Sql.CursorFetchMetaData"] = "SQL_CURSOR_FETCH_META_DATA";
    client_msgs_by_name["CON_CAPABILITIES_GET"] = std::make_pair(&create<Mysqlx::Connection::CapabilitiesGet>, Mysqlx::ClientMessages::CON_CAPABILITIES_GET);
    client_msgs_by_id[Mysqlx::ClientMessages::CON_CAPABILITIES_GET] = std::make_pair(&create<Mysqlx::Connection::CapabilitiesGet>, "CON_CAPABILITIES_GET");
    client_msgs_by_full_name["Mysqlx.Connection.CapabilitiesGet"] = "CON_CAPABILITIES_GET";
    client_msgs_by_name["CRUD_UPDATE"] = std::make_pair(&create<Mysqlx::Crud::Update>, Mysqlx::ClientMessages::CRUD_UPDATE);
    client_msgs_by_id[Mysqlx::ClientMessages::CRUD_UPDATE] = std::make_pair(&create<Mysqlx::Crud::Update>, "CRUD_UPDATE");
    client_msgs_by_full_name["Mysqlx.Crud.Update"] = "CRUD_UPDATE";
    client_msgs_by_name["SESS_AUTHENTICATE_CONTINUE"] = std::make_pair(&create<Mysqlx::Session::AuthenticateContinue>, Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE);
    client_msgs_by_id[Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE] = std::make_pair(&create<Mysqlx::Session::AuthenticateContinue>, "SESS_AUTHENTICATE_CONTINUE");
    client_msgs_by_full_name["Mysqlx.Session.AuthenticateContinue"] = "SESS_AUTHENTICATE_CONTINUE";
    client_msgs_by_name["CON_CAPABILITIES_SET"] = std::make_pair(&create<Mysqlx::Connection::CapabilitiesSet>, Mysqlx::ClientMessages::CON_CAPABILITIES_SET);
    client_msgs_by_id[Mysqlx::ClientMessages::CON_CAPABILITIES_SET] = std::make_pair(&create<Mysqlx::Connection::CapabilitiesSet>, "CON_CAPABILITIES_SET");
    client_msgs_by_full_name["Mysqlx.Connection.CapabilitiesSet"] = "CON_CAPABILITIES_SET";
    client_msgs_by_name["CRUD_DELETE"] = std::make_pair(&create<Mysqlx::Crud::Delete>, Mysqlx::ClientMessages::CRUD_DELETE);
    client_msgs_by_id[Mysqlx::ClientMessages::CRUD_DELETE] = std::make_pair(&create<Mysqlx::Crud::Delete>, "CRUD_DELETE");
    client_msgs_by_full_name["Mysqlx.Crud.Delete"] = "CRUD_DELETE";
    client_msgs_by_name["EXPECT_CLOSE"] = std::make_pair(&create<Mysqlx::Expect::Close>, Mysqlx::ClientMessages::EXPECT_CLOSE);
    client_msgs_by_id[Mysqlx::ClientMessages::EXPECT_CLOSE] = std::make_pair(&create<Mysqlx::Expect::Close>, "EXPECT_CLOSE");
    client_msgs_by_full_name["Mysqlx.Expect.Close"] = "EXPECT_CLOSE";
    client_msgs_by_name["CRUD_INSERT"] = std::make_pair(&create<Mysqlx::Crud::Insert>, Mysqlx::ClientMessages::CRUD_INSERT);
    client_msgs_by_id[Mysqlx::ClientMessages::CRUD_INSERT] = std::make_pair(&create<Mysqlx::Crud::Insert>, "CRUD_INSERT");
    client_msgs_by_full_name["Mysqlx.Crud.Insert"] = "CRUD_INSERT";
    client_msgs_by_name["SESS_CLOSE"] = std::make_pair(&create<Mysqlx::Session::Close>, Mysqlx::ClientMessages::SESS_CLOSE);
    client_msgs_by_id[Mysqlx::ClientMessages::SESS_CLOSE] = std::make_pair(&create<Mysqlx::Session::Close>, "SESS_CLOSE");
    client_msgs_by_full_name["Mysqlx.Session.Close"] = "SESS_CLOSE";
    client_msgs_by_name["SQL_STMT_EXECUTE"] = std::make_pair(&create<Mysqlx::Sql::StmtExecute>, Mysqlx::ClientMessages::SQL_STMT_EXECUTE);
    client_msgs_by_id[Mysqlx::ClientMessages::SQL_STMT_EXECUTE] = std::make_pair(&create<Mysqlx::Sql::StmtExecute>, "SQL_STMT_EXECUTE");
    client_msgs_by_full_name["Mysqlx.Sql.StmtExecute"] = "SQL_STMT_EXECUTE";
    client_msgs_by_name["SESS_RESET"] = std::make_pair(&create<Mysqlx::Session::Reset>, Mysqlx::ClientMessages::SESS_RESET);
    client_msgs_by_id[Mysqlx::ClientMessages::SESS_RESET] = std::make_pair(&create<Mysqlx::Session::Reset>, "SESS_RESET");
    client_msgs_by_full_name["Mysqlx.Session.Reset"] = "SESS_RESET";
    client_msgs_by_name["CON_CLOSE"] = std::make_pair(&create<Mysqlx::Connection::Close>, Mysqlx::ClientMessages::CON_CLOSE);
    client_msgs_by_id[Mysqlx::ClientMessages::CON_CLOSE] = std::make_pair(&create<Mysqlx::Connection::Close>, "CON_CLOSE");
    client_msgs_by_full_name["Mysqlx.Connection.Close"] = "CON_CLOSE";
    client_msgs_by_name["EXPECT_OPEN"] = std::make_pair(&create<Mysqlx::Expect::Open>, Mysqlx::ClientMessages::EXPECT_OPEN);
    client_msgs_by_id[Mysqlx::ClientMessages::EXPECT_OPEN] = std::make_pair(&create<Mysqlx::Expect::Open>, "EXPECT_OPEN");
    client_msgs_by_full_name["Mysqlx.Expect.Open"] = "EXPECT_OPEN";
    client_msgs_by_name["CRUD_FIND"] = std::make_pair(&create<Mysqlx::Crud::Find>, Mysqlx::ClientMessages::CRUD_FIND);
    client_msgs_by_id[Mysqlx::ClientMessages::CRUD_FIND] = std::make_pair(&create<Mysqlx::Crud::Find>, "CRUD_FIND");
    client_msgs_by_full_name["Mysqlx.Crud.Find"] = "CRUD_FIND";
    client_msgs_by_name["SESS_AUTHENTICATE_START"] = std::make_pair(&create<Mysqlx::Session::AuthenticateStart>, Mysqlx::ClientMessages::SESS_AUTHENTICATE_START);
    client_msgs_by_id[Mysqlx::ClientMessages::SESS_AUTHENTICATE_START] = std::make_pair(&create<Mysqlx::Session::AuthenticateStart>, "SESS_AUTHENTICATE_START");
    client_msgs_by_full_name["Mysqlx.Session.AuthenticateStart"] = "SESS_AUTHENTICATE_START";
  }
} init_message_factory;


