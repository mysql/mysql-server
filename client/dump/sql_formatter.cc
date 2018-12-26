/*
  Copyright (c) 2015, 2018 Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql_formatter.h"
#include "view.h"
#include "mysql_function.h"
#include "stored_procedure.h"
#include "privilege.h"
#include <boost/algorithm/string.hpp>
#include <boost/chrono.hpp>

using namespace Mysql::Tools::Dump;

void Sql_formatter::format_row_group(Row_group_dump_task* row_group)
{
  std::size_t row_data_length = 0;
  // Calculate total length of data to be formatted.
  for (std::vector<Row*>::iterator row_iterator= row_group->m_rows.begin();
    row_iterator != row_group->m_rows.end();
    ++row_iterator)
  {
    row_data_length+= 3; // Space for enclosing parentheses and comma.

    Row* row= *row_iterator;
    for (size_t column= row->m_row_data.size(); column-- > 0;)
    {
      // Space for escaped string, enclosing " and comma.
      row_data_length+= row->m_row_data.size_of_element(column) * 2 + 3;
    }
  }
  if (m_options->m_dump_column_names || row_group->m_has_generated_columns)
  {
    row_data_length+= 3; // Space for enclosing parentheses and space.
    const std::vector<Mysql_field>& fields= row_group->m_fields;
    for (std::vector<Mysql_field>::const_iterator
       field_iterator= fields.begin(); field_iterator != fields.end();
       ++field_iterator)
    {
      row_data_length+= field_iterator->get_name().size() * 2 + 3;
    }
  }
  std::string row_string;
  /*
    Space for constant strings "INSERT INTO ... VALUES ()" with
    reserve for comments, modificators and future changes.
    */
  const size_t INSERT_INTO_MAX_SIZE= 200;

  row_string.reserve(INSERT_INTO_MAX_SIZE
    + row_group->m_source_table->get_schema().size()
    + row_group->m_source_table->get_name().size()
    + row_data_length);

  if (m_options->m_insert_type_replace)
    row_string+= "REPLACE INTO ";
  else if (m_options->m_insert_type_ignore)
    row_string+= "INSERT IGNORE INTO ";
  else
    row_string+= "INSERT INTO ";
  row_string+= this->get_quoted_object_full_name(row_group->m_source_table);
  if (m_options->m_dump_column_names || row_group->m_has_generated_columns)
  {
    row_string+= " (";
    const std::vector<Mysql_field>& fields= row_group->m_fields;
    for (std::vector<Mysql_field>::const_iterator
      field_iterator= fields.begin(); field_iterator != fields.end();
      ++field_iterator)
    {
      if (field_iterator != fields.begin())
        row_string+= ',';
      row_string+= this->quote_name(field_iterator->get_name());
    }
    row_string+= ')';
  }
  row_string+= " VALUES ";

  CHARSET_INFO* charset_info= this->get_charset();

  std::vector<bool> is_blob;
  for (std::vector<Mysql_field>::const_iterator it=
    row_group->m_fields.begin(); it != row_group->m_fields.end(); ++it)
  {
    is_blob.push_back(
      it->get_character_set_nr() == my_charset_bin.number
      && (it->get_type() == MYSQL_TYPE_BIT
      || it->get_type() == MYSQL_TYPE_STRING
      || it->get_type() == MYSQL_TYPE_VAR_STRING
      || it->get_type() == MYSQL_TYPE_VARCHAR
      || it->get_type() == MYSQL_TYPE_BLOB
      || it->get_type() == MYSQL_TYPE_LONG_BLOB
      || it->get_type() == MYSQL_TYPE_MEDIUM_BLOB
      || it->get_type() == MYSQL_TYPE_TINY_BLOB
      || it->get_type() == MYSQL_TYPE_GEOMETRY));
  }

  for (std::vector<Row*>::const_iterator row_iterator=
    row_group->m_rows.begin(); row_iterator != row_group->m_rows.end();
    ++row_iterator)
  {
    Row* row= *row_iterator;

    if (row_iterator != row_group->m_rows.begin())
      row_string+= ',';
    row_string+= '(';

    size_t columns= row->m_row_data.size();
    for (size_t column= 0; column < columns; ++column)
    {
      if (column > 0)
        row_string+= ',';

      size_t column_length;
      const char* column_data=
        row->m_row_data.get_buffer(column, column_length);

      if (row->m_row_data.is_value_null(column))
        row_string+= "NULL";
      else if (column_length == 0)
        row_string+= "''";
      else if (row_group->m_fields[column].get_additional_flags()
        & NUM_FLAG)
      {
        if (column_length >= 1 && (my_isalpha(charset_info, column_data[0])
          || (column_length >= 2 && column_data[0] == '-'
          && my_isalpha(charset_info, column_data[1]))))
        {
          row_string+= "NULL";
        }
        else if (row_group->m_fields[column].get_type() == MYSQL_TYPE_DECIMAL)
        {
          row_string+= '\'';
          row_string.append(column_data, column_length);
          row_string+= '\'';
        }
        else
          row_string.append(column_data, column_length);
      }
      else if (m_options->m_hex_blob && is_blob[column])
      {
        row_string+= "0x";
        m_escaping_runner->append_hex_string(
          &row_string, column_data, column_length);
      }
      else
      {
        if (is_blob[column])
          row_string += "_binary ";
        row_string+= '\"';
        m_escaping_runner->append_escape_string(
          &row_string, column_data, column_length);
        row_string+= '\"';
      }
    }

    row_string+= ')';
  }

  row_string+= ";\n";

  this->append_output(row_string);
}

void Sql_formatter::format_table_indexes(
  Table_deferred_indexes_dump_task* table_indexes_dump_task)
{
  Table* table= table_indexes_dump_task->get_related_table();
  if (m_options->m_deffer_table_indexes)
  {
    /*
      Tables can have indexes  which can refer to columns from
      other tables (ex: foreign keys). In that case we need to
      emit 'USE db' statement as the referenced table may not have
      been created
    */
    bool use_added= false;
    std::string alter_base_string= "ALTER TABLE "
      + this->get_quoted_object_full_name(table) + " ADD ";
    for (std::vector<std::string>::const_iterator it=
      table->get_indexes_sql_definition().begin();
      it != table->get_indexes_sql_definition().end();
    ++it)
    {
      if (!use_added)
      {
        this->append_output("USE "
          + this->quote_name(table->get_schema()) + ";\n");
        use_added= true;
      }
      this->append_output(alter_base_string + (*it) + ";\n");
    }
  }
  if (m_options->m_add_locks)
    this->append_output("UNLOCK TABLES;\n");
}

void Sql_formatter::format_table_definition(
  Table_definition_dump_task* table_definition_dump_task)
{
  Table* table= table_definition_dump_task->get_related_table();
  bool use_added= false;
  if (m_options->m_drop_table)
    this->append_output("DROP TABLE IF EXISTS "
    + this->get_quoted_object_full_name(table) + ";\n");
  if (m_options->m_deffer_table_indexes == 0 && !use_added)
  {
    use_added= true;
    this->append_output("USE "
       + this->quote_name(table->get_schema()) + ";\n");
  }
  if (!m_options->m_suppress_create_table)
    this->append_output((m_options->m_deffer_table_indexes
    ? table->get_sql_definition_without_indexes()
    : table->get_sql_formatted_definition()) + ";\n");

  if (m_options->m_add_locks)
    this->append_output("LOCK TABLES "
    + this->get_quoted_object_full_name(table)
    + " WRITE;\n");
}

void Sql_formatter::format_database_start(
  Database_start_dump_task* database_definition_dump_task)
{
  Database* database= database_definition_dump_task
    ->get_related_database();
  if (m_options->m_drop_database)
    this->append_output("DROP DATABASE IF EXISTS " +
    this->quote_name(database->get_name()) + ";\n");
  if (!m_options->m_suppress_create_database)
    this->append_output(database->get_sql_formatted_definition() + ";\n");
}

void Sql_formatter::format_dump_end(Dump_end_dump_task* dump_start_dump_task)
{
  std::ostringstream out;
  std::time_t sys_time = boost::chrono::system_clock::to_time_t(
    boost::chrono::system_clock::now());
  // Convert to calendar time.
  std::string time_string = std::ctime(&sys_time);
  boost::trim(time_string);

  if (m_options->m_timezone_consistent)
    out << "SET TIME_ZONE=@OLD_TIME_ZONE;\n";
  if (m_options->m_charsets_consistent)
    out << "SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT;\n"
    "SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS;\n"
    "SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION;\n";
  out << "SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;\n"
    "SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;\n"
    "SET SQL_MODE=@OLD_SQL_MODE;\n";

  out << "-- Dump end time: " << time_string << "\n";

  this->append_output(out.str());
}

void Sql_formatter::format_dump_start(
  Dump_start_dump_task* dump_start_dump_task)
{
  // Convert to system time.
  std::time_t sys_time = boost::chrono::system_clock::to_time_t(
    boost::chrono::system_clock::now());
  // Convert to calendar time.
  std::string time_string = std::ctime(&sys_time);
  // Skip trailing newline
  boost::trim(time_string);

  std::ostringstream out;
  out << "-- Dump created by MySQL pump utility, version: "
    MYSQL_SERVER_VERSION ", " SYSTEM_TYPE " (" MACHINE_TYPE ")\n"
    << "-- Dump start time: " << time_string << "\n"
    << "-- Server version: " << this->get_server_version_string() << "\n\n"
    << "SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;\n"
    "SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, "
    "FOREIGN_KEY_CHECKS=0;\n" << "SET @OLD_SQL_MODE=@@SQL_MODE;\n"
    "SET SQL_MODE=\"NO_AUTO_VALUE_ON_ZERO\";\n";

  /* disable binlog */
  out << "SET @@SESSION.SQL_LOG_BIN= 0;\n";

  if (m_options->m_timezone_consistent)
    out << "SET @OLD_TIME_ZONE=@@TIME_ZONE;\n"
    "SET TIME_ZONE='+00:00';\n";
  if (m_options->m_charsets_consistent)
    out << "SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT;\n"
    "SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS;\n"
    "SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION;\n"
    "SET NAMES "
    << this->get_charset()->csname
    << ";\n";
  if (dump_start_dump_task->m_gtid_mode == "OFF" &&
      *((ulong*)&m_options->m_gtid_purged) == ((ulong)GTID_PURGED_ON))
  {
    m_options->m_mysql_chain_element_options->get_program()->error(
      Mysql::Tools::Base::Message_data(1, "Server has GTIDs disabled.\n",
      Mysql::Tools::Base::Message_type_error));
    return;
  }
  if (dump_start_dump_task->m_gtid_mode != "OFF")
  {
    /*
     value for m_gtid_purged is set by typecasting its address to ulong*
     however below conditions fails if we do direct comparison without
     typecasting on solaris sparc. Guessing that this is due to differnt
     endianess.
    */
    if (*((ulong*)&m_options->m_gtid_purged) == ((ulong)GTID_PURGED_ON) ||
        *((ulong*)&m_options->m_gtid_purged) == ((ulong)GTID_PURGED_AUTO))
    {
      if (!m_mysqldump_tool_options->m_dump_all_databases &&
          *((ulong*)&m_options->m_gtid_purged) == ((ulong)GTID_PURGED_AUTO))
      {
        m_options->m_mysql_chain_element_options->get_program()->error(
          Mysql::Tools::Base::Message_data(1,
          "A partial dump from a server that is using GTID-based replication "
          "requires the --set-gtid-purged=[ON|OFF] option to be specified. Use ON "
          "if the intention is to deploy a new replication slave using only some "
          "of the data from the dumped server. Use OFF if the intention is to "
          "repair a table by copying it within a topology, and use OFF if the "
          "intention is to copy a table between replication topologies that are "
          "disjoint and will remain so.\n",
          Mysql::Tools::Base::Message_type_error));
        return;
      }
      std::string gtid_output("SET @@GLOBAL.GTID_PURGED=/*!80000 '+'*/ '");
      gtid_output+= (dump_start_dump_task->m_gtid_executed + "';\n");
      out << gtid_output;
    }
  }

  this->append_output(out.str());
}

void Sql_formatter::format_plain_sql_object(
  Abstract_plain_sql_object_dump_task* plain_sql_dump_task)
{
  View* new_view_task=
     dynamic_cast<View*>(plain_sql_dump_task);
  if (new_view_task != NULL)
  {
     /*
      DROP VIEW statement followed by CREATE VIEW must be written to output
      as an atomic operation, else there is a possibility of bug#21399236.
      It happens when we DROP VIEW v1, and it uses column from view v2, which
      might get dropped before creation of real v1 view, and thus result in
      error during restore.
    */
    format_sql_objects_definer(plain_sql_dump_task, "VIEW");
    this->append_output("DROP VIEW IF EXISTS "
       + this->get_quoted_object_full_name(new_view_task) + ";\n"
       + plain_sql_dump_task->get_sql_formatted_definition() + ";\n");
    return;
  }

  Mysql_function* new_func_task=
     dynamic_cast<Mysql_function*>(plain_sql_dump_task);
  if (new_func_task != NULL)
    format_sql_objects_definer(plain_sql_dump_task, "FUNCTION");

  Stored_procedure* new_proc_task=
     dynamic_cast<Stored_procedure*>(plain_sql_dump_task);
  if (new_proc_task != NULL)
    format_sql_objects_definer(plain_sql_dump_task, "PROCEDURE");

  Privilege* new_priv_task=
     dynamic_cast<Privilege*>(plain_sql_dump_task);
  if (new_priv_task != NULL)
  {
    if (m_options->m_drop_user)
      this->append_output("DROP USER "
       + (dynamic_cast<Abstract_data_object*>(new_priv_task))->get_name()
       + ";\n");
  }

  this->append_output(plain_sql_dump_task->get_sql_formatted_definition()
    + ";\n");
}

void Sql_formatter::format_sql_objects_definer(
  Abstract_plain_sql_object_dump_task* plain_sql_dump_task, std::string object_type)
{
  if (m_options->m_skip_definer)
  {
    std::vector<std::string> object_ddl_lines;
    std::string object_ddl(plain_sql_dump_task->get_sql_formatted_definition());
    boost::split(object_ddl_lines, object_ddl,
                 boost::is_any_of("\n"), boost::token_compress_on);

    std::string new_sql_stmt;
    bool is_replaced= FALSE;
    for (std::vector<std::string>::iterator it= object_ddl_lines.begin();
         it != object_ddl_lines.end(); ++it)
    {
      std::string object_sql(*it);
      size_t object_pos= object_sql.find(object_type);
      size_t definer_pos= object_sql.find("DEFINER");
      if (object_pos != std::string::npos &&
          definer_pos != std::string::npos &&
          definer_pos <= object_pos &&
          !is_replaced)
      {
        object_sql.replace(definer_pos, (object_pos-definer_pos), "");
        new_sql_stmt+= object_sql + "\n";
        is_replaced= TRUE;
      }
      else
        new_sql_stmt+= object_sql + "\n";
    }
    plain_sql_dump_task->set_sql_formatted_definition(new_sql_stmt);
  }
}
void Sql_formatter::format_object(Item_processing_data* item_to_process)
{
  this->object_processing_starts(item_to_process);

  // format_row_group is placed first, as it is most occurring task.
  if (this->try_process_task<Row_group_dump_task>
    (item_to_process, &Sql_formatter::format_row_group)
    || this->try_process_task<Table_definition_dump_task>
    (item_to_process, &Sql_formatter::format_table_definition)
    || this->try_process_task<Table_deferred_indexes_dump_task>
    (item_to_process, &Sql_formatter::format_table_indexes)
    || this->try_process_task<Dump_start_dump_task>
    (item_to_process, &Sql_formatter::format_dump_start)
    || this->try_process_task<Dump_end_dump_task>
    (item_to_process, &Sql_formatter::format_dump_end)
    || this->try_process_task<Database_start_dump_task>
    (item_to_process, &Sql_formatter::format_database_start)
    /*
      Abstract_plain_sql_object_dump_task must be last, as so of above derive
      from it too.
      */
      || this->try_process_task<Abstract_plain_sql_object_dump_task>
      (item_to_process, &Sql_formatter::format_plain_sql_object))
  {
    // Item was processed. No further action required.
  }

  this->object_processing_ends(item_to_process);

  return;
}

Sql_formatter::Sql_formatter(I_connection_provider* connection_provider,
  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator,
  const Mysqldump_tool_chain_maker_options* mysqldump_tool_options,
  const Sql_formatter_options* options)
  : Abstract_output_writer_wrapper(message_handler, object_id_generator),
  Abstract_mysql_chain_element_extension(
  connection_provider, message_handler,
  options->m_mysql_chain_element_options),
  m_mysqldump_tool_options(mysqldump_tool_options),
  m_options(options)
{
  m_escaping_runner= this->get_runner();
}

Sql_formatter::~Sql_formatter()
{
  delete m_escaping_runner;
}
