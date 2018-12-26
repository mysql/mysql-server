/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "program.h"
#include "i_connection_provider.h"
#include "thread_specific_connection_provider.h"
#include "single_transaction_connection_provider.h"
#include "simple_id_generator.h"
#include "i_progress_watcher.h"
#include "standard_progress_watcher.h"
#include "i_crawler.h"
#include "mysql_crawler.h"
#include "i_chain_maker.h"
#include "mysqldump_tool_chain_maker.h"
#include <boost/chrono.hpp>

using namespace Mysql::Tools::Dump;

void Program::close_redirected_stderr()
{
  if (m_stderr != NULL)
    fclose(m_stderr);
}

void Program::error_log_file_callback(char*)
{
  if (!m_error_log_file.has_value())
    return;
  this->close_redirected_stderr();
  m_stderr= freopen(m_error_log_file.value().c_str(), "a", stderr);
  if (m_stderr == NULL)
  {
    this->error(Mysql::Tools::Base::Message_data(errno,
      "Cannot append error log to specified file: \""
      + m_error_log_file.value() + "\"",
      Mysql::Tools::Base::Message_type_error));
  }
}

bool Program::message_handler(const Mysql::Tools::Base::Message_data& message)
{
  this->error(message);
  return false;
}

void Program::error(const Mysql::Tools::Base::Message_data& message)
{
  std::cerr << this->get_name() << ": [" << message.get_message_type_string()
    << "] (" << message.get_code() << ") " << message.get_message()
    << std::endl;

  if (message.get_message_type() == Mysql::Tools::Base::Message_type_error)
  {
    std::cerr << "Dump process encountered error and will not continue."
      << std::endl;
    m_error_code.store((int)message.get_code());
  }
}

void Program::create_options()
{
  this->create_new_option(&m_error_log_file, "log-error-file",
    "Append warnings and errors to specified file.")
    ->add_callback(new Mysql::Instance_callback<void, char*, Program>(
    this, &Program::error_log_file_callback));
  this->create_new_option(&m_watch_progress, "watch-progress",
    "Shows periodically dump process progress information on error output. "
    "Progress information include both completed and total number of "
    "tables, rows and other objects collected.")
    ->set_value(true);
  this->create_new_option(&m_single_transaction, "single-transaction",
    "Creates a consistent snapshot by dumping all tables in a single "
    "transaction. Works ONLY for tables stored in storage engines which "
    "support multiversioning (currently only InnoDB does); the dump is NOT "
    "guaranteed to be consistent for other storage engines. "
    "While a --single-transaction dump is in process, to ensure a valid "
    "dump file (correct table contents and binary log position), no other "
    "connection should use the following statements: ALTER TABLE, DROP "
    "TABLE, RENAME TABLE, TRUNCATE TABLE, as consistent snapshot is not "
    "isolated from them. This option is mutually exclusive with "
    "--add-locks option.");
}

void  Program::check_mutually_exclusive_options()
{
  /*
    In case of --add-locks we dont allow parallelism
  */
  if (m_mysqldump_tool_chain_maker_options->m_default_parallelism ||
     m_mysqldump_tool_chain_maker_options->get_parallel_schemas_thread_count())
  {
    if (m_mysqldump_tool_chain_maker_options->m_formatter_options->m_add_locks)
      m_mysql_chain_element_options->get_program()->error(
        Mysql::Tools::Base::Message_data(1, "Usage of --add-locks "
        "is mutually exclusive with parallelism.",
        Mysql::Tools::Base::Message_type_error));
  }
}

int Program::get_total_connections()
{
  /*
    total thread count for mysqlpump would be as below:
     1 main thread +
     default queues thread (specified by default parallelism) +
     total parallel-schemas without threads specified * dp +
     total threads mentioned in parallel-schemas
  */
 
   int dp= m_mysqldump_tool_chain_maker_options->m_default_parallelism;
  return (1 + dp +
    m_mysqldump_tool_chain_maker_options->get_parallel_schemas_thread_count() +
    (m_mysqldump_tool_chain_maker_options->
     get_parallel_schemas_with_default_thread_count() * dp));
}

int Program::get_error_code()
{
  return m_error_code.load();
}

int Program::execute(std::vector<std::string> positional_options)
{
  I_connection_provider* connection_provider= NULL;
  int num_connections= get_total_connections();

  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler= new Mysql::Instance_callback
    <bool, const Mysql::Tools::Base::Message_data&, Program>(
    this, &Program::message_handler);

  try
  {
    connection_provider=
      m_single_transaction ?
      new Single_transaction_connection_provider(this, num_connections, message_handler)
      : new Thread_specific_connection_provider(this);
  }
  catch (const std::exception &e)
  {
    this->error(Mysql::Tools::Base::Message_data(
      0, "Error during creating connection.",
      Mysql::Tools::Base::Message_type_error));
  }

  Mysql::Tools::Base::Mysql_query_runner* runner= connection_provider
         ->get_runner(message_handler);
  if (mysql_get_server_version(runner->get_low_level_connection()) < 50708)
  {
    std::cerr << "Server version is not compatible. Server version should "
                 "be 5.7.8 or above.";
    delete runner;
    delete message_handler;
    delete connection_provider;
    return 0;
  }

  Simple_id_generator* id_generator= new Simple_id_generator();

  boost::chrono::high_resolution_clock::time_point start_time=
    boost::chrono::high_resolution_clock::now();

  I_progress_watcher* progress_watcher= NULL;

  if (m_watch_progress)
  {
    progress_watcher= new Standard_progress_watcher(
      message_handler, id_generator);
  }
  I_crawler* crawler= new Mysql_crawler(
    connection_provider, message_handler, id_generator,
    m_mysql_chain_element_options, this);
  m_mysqldump_tool_chain_maker_options->process_positional_options(
    positional_options);
  check_mutually_exclusive_options();
  I_chain_maker* chain_maker= new Mysqldump_tool_chain_maker(
    connection_provider, message_handler, id_generator,
    m_mysqldump_tool_chain_maker_options, this);

  crawler->register_chain_maker(chain_maker);
  if (progress_watcher != NULL)
  {
    crawler->register_progress_watcher(progress_watcher);
    chain_maker->register_progress_watcher(progress_watcher);
  }

  crawler->enumerate_objects();

  delete runner;
  delete crawler;
  if (progress_watcher != NULL)
    delete progress_watcher;
  delete id_generator;
  delete connection_provider;
  delete message_handler;
  delete chain_maker;

  if (!get_error_code())
  {
    std::cerr << "Dump completed in " <<
      boost::chrono::duration_cast<boost::chrono::milliseconds>(
      boost::chrono::high_resolution_clock::now() - start_time) << std::endl;
  }
  return get_error_code();
}

std::string Program::get_description()
{
  return "MySQL utility for dumping data from databases to external file.";
}

int Program::get_first_release_year()
{
  return 2014;
}

std::string Program::get_version()
{
  return "1.0.0";
}

Program::~Program()
{
  delete m_mysql_chain_element_options;
  delete m_mysqldump_tool_chain_maker_options;
  this->close_redirected_stderr();
}

void Program::short_usage()
{
  std::cout << "Usage: " << get_name() <<" [OPTIONS] [--all-databases]"
            << std::endl;
  std::cout << "OR     " << get_name() <<" [OPTIONS] --databases DB1 [DB2 DB3...]"
            << std::endl;
  std::cout << "OR     " << get_name() <<" [OPTIONS] database [tables]"
            << std::endl;
}

Program::Program()
  : Abstract_connection_program(),
  m_stderr(NULL),
  m_error_code(0)
{
  m_mysql_chain_element_options= new Mysql_chain_element_options(this);
  m_mysqldump_tool_chain_maker_options=
    new Mysqldump_tool_chain_maker_options(m_mysql_chain_element_options);

  this->add_provider(m_mysql_chain_element_options);
  this->add_provider(m_mysqldump_tool_chain_maker_options);
}

const char *load_default_groups[]=
{
  "client", /* Read settings how to connect to server. */
  "mysql_dump", /* Read special settings for mysql_dump. */
  0
};

static Program program;

int main(int argc, char **argv)
{
  ::program.run(argc, argv);
  return 0;
}
