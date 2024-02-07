/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CONSTANTS_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CONSTANTS_H_

#include <bitset>
#include <cstdint>

namespace classic_protocol {

namespace capabilities {

namespace pos {
using value_type = uint8_t;
constexpr value_type long_password{0};
constexpr value_type found_rows{1};
constexpr value_type long_flag{2};
constexpr value_type connect_with_schema{3};
constexpr value_type no_schema{4};
constexpr value_type compress{5};
constexpr value_type odbc{6};
constexpr value_type local_files{7};
constexpr value_type ignore_space{8};
constexpr value_type protocol_41{9};
constexpr value_type interactive{10};
constexpr value_type ssl{11};
// 12 is unused
constexpr value_type transactions{13};
// 14 is unused
constexpr value_type secure_connection{15};
constexpr value_type multi_statements{16};
constexpr value_type multi_results{17};
constexpr value_type ps_multi_results{18};
constexpr value_type plugin_auth{19};
constexpr value_type connect_attributes{20};
constexpr value_type client_auth_method_data_varint{21};
constexpr value_type expired_passwords{22};
constexpr value_type session_track{23};
constexpr value_type text_result_with_session_tracking{24};
constexpr value_type optional_resultset_metadata{25};
constexpr value_type compress_zstd{26};
constexpr value_type query_attributes{27};
//
// 29 is an extension flag for >32 bit
// 30 is client only
// 31 is client only

}  // namespace pos

using value_type = std::bitset<32>;

// old_password instead of older_password
// version_added: 3.21
constexpr value_type long_password{1 << pos::long_password};
// found rows, instead of affected rows.
// version_added: 3.21
constexpr value_type found_rows{1 << pos::found_rows};
// version_added: 3.21
// get all column flags
constexpr value_type long_flag{1 << pos::long_flag};
// connect with schema
// version_added: 3.21
constexpr value_type connect_with_schema{1 << pos::connect_with_schema};
// don't allow schema.table.column
// version_added: 3.21
constexpr value_type no_schema{1 << pos::no_schema};
// use deflate compression
// version_added: 3.22
constexpr value_type compress{1 << pos::compress};
// odbc client
// version_added: 3.22
constexpr value_type odbc{1 << pos::odbc};
// can use LOCAL INFILE
// version_added: 3.22
constexpr value_type local_files{1 << pos::local_files};
// ignore space before (
// version_added: 3.22
constexpr value_type ignore_space{1 << pos::ignore_space};
// protocol_version 10 + more fields in server::Greeting
// version_added: 4.1
constexpr value_type protocol_41{1 << pos::protocol_41};
// interactive
// version_added: 3.22
constexpr value_type interactive{1 << pos::interactive};
// switch to SSL
// version_added: 3.23
constexpr value_type ssl{1 << pos::ssl};
// status-field in Ok message
// version_added: 3.23
constexpr value_type transactions{1 << pos::transactions};
// mysql_native_password
// version_added: 4.1
constexpr value_type secure_connection{1 << pos::secure_connection};
// multi-statement support
// version_added: 4.1
constexpr value_type multi_statements{1 << pos::multi_statements};
// multi-result support
// version_added: 4.1
constexpr value_type multi_results{1 << pos::multi_results};
// version_added: 5.5
constexpr value_type ps_multi_results{1 << pos::ps_multi_results};
// version_added: 5.5
constexpr value_type plugin_auth{1 << pos::plugin_auth};
// version_added: 5.6
constexpr value_type connect_attributes{1 << pos::connect_attributes};
// version_added: 5.6
constexpr value_type client_auth_method_data_varint{
    1 << pos::client_auth_method_data_varint};
// version_added: 5.6
constexpr value_type expired_passwords{1 << pos::expired_passwords};
// version_added: 5.7
constexpr value_type session_track{1 << pos::session_track};
// version_added: 5.7
constexpr value_type text_result_with_session_tracking{
    1 << pos::text_result_with_session_tracking};
// version_added: 8.0
constexpr value_type compress_zstd{1 << pos::compress_zstd};
// version_added: 8.0
constexpr value_type optional_resultset_metadata{
    1 << pos::optional_resultset_metadata};
// version_added: 8.0
constexpr value_type query_attributes{1 << pos::query_attributes};
}  // namespace capabilities

namespace status {
namespace pos {
using value_type = uint8_t;
constexpr value_type in_transaction{0};
constexpr value_type autocommit{1};
// 2 is unused (more-results in 4.1.22)
constexpr value_type more_results_exist{3};
constexpr value_type no_good_index_used{4};
constexpr value_type no_index_used{5};
constexpr value_type cursor_exists{6};
constexpr value_type last_row_sent{7};
constexpr value_type schema_dropped{8};
constexpr value_type no_backslash_escapes{9};
constexpr value_type metadata_changed{10};
constexpr value_type query_was_slow{11};
constexpr value_type ps_out_params{12};
constexpr value_type in_transaction_readonly{13};
constexpr value_type session_state_changed{14};

}  // namespace pos
using value_type = std::bitset<16>;

// transaction is open
// version_added: 3.23
constexpr value_type in_transaction{1 << pos::in_transaction};
// autocommit
// version_added: 3.23
constexpr value_type autocommit{1 << pos::autocommit};
// multi-statement more results
// version_added: 4.1
constexpr value_type more_results_exist{1 << pos::more_results_exist};
// no good index used
// version_added: 4.1
constexpr value_type no_good_index_used{1 << pos::no_good_index_used};
// no index used
// version_added: 4.1
constexpr value_type no_index_used{1 << pos::no_index_used};
// cursor exists
// version_added: 5.0
constexpr value_type cursor_exists{1 << pos::cursor_exists};
// last row sent
// version_added: 5.0
constexpr value_type last_row_sent{1 << pos::last_row_sent};
// schema dropped
// version_added: 4.1
constexpr value_type schema_dropped{1 << pos::schema_dropped};
// no backslash escapes
// version_added: 5.0
constexpr value_type no_backslash_escapes{1 << pos::no_backslash_escapes};
// metadata changed
// version_added: 5.1
constexpr value_type metadata_changed{1 << pos::metadata_changed};
// version_added: 5.5
constexpr value_type query_was_slow{1 << pos::query_was_slow};
// version_added: 5.5
constexpr value_type ps_out_params{1 << pos::ps_out_params};
// version_added: 5.7
constexpr value_type in_transaction_readonly{1 << pos::in_transaction_readonly};
// version_added: 5.7
constexpr value_type session_state_changed{1 << pos::session_state_changed};
}  // namespace status

namespace cursor {
namespace pos {
using value_type = uint8_t;
constexpr value_type read_only{0};
constexpr value_type for_update{1};
constexpr value_type scrollable{2};
constexpr value_type param_count_available{3};

constexpr value_type _bitset_size{param_count_available + 1};
}  // namespace pos
using value_type = std::bitset<pos::_bitset_size>;

constexpr value_type no_cursor{0};
constexpr value_type read_only{1 << pos::read_only};
constexpr value_type for_update{1 << pos::for_update};
constexpr value_type scrollable{1 << pos::scrollable};
constexpr value_type param_count_available{1 << pos::param_count_available};
}  // namespace cursor

namespace field_type {
using value_type = uint8_t;
constexpr value_type Decimal{0x00};
constexpr value_type Tiny{0x01};
constexpr value_type Short{0x02};
constexpr value_type Long{0x03};
constexpr value_type Float{0x04};
constexpr value_type Double{0x05};
constexpr value_type Null{0x06};
constexpr value_type Timestamp{0x07};
constexpr value_type LongLong{0x08};
constexpr value_type Int24{0x09};
constexpr value_type Date{0x0a};
constexpr value_type Time{0x0b};
constexpr value_type DateTime{0x0c};
constexpr value_type Year{0x0d};
// not used in protocol: constexpr value_type NewDate{0x0e};
constexpr value_type Varchar{0x0f};
constexpr value_type Bit{0x10};
constexpr value_type Timestamp2{0x11};
// not used in protocol: constexpr value_type Datetime2{0x12};
// not used in protocol: constexpr value_type Time2{0x13};
// not used in protocol: constexpr value_type TypedArray{0x14};
constexpr value_type Json{0xf5};
constexpr value_type NewDecimal{0xf6};
constexpr value_type Enum{0xf7};
constexpr value_type Set{0xf8};
constexpr value_type TinyBlob{0xf9};
constexpr value_type MediumBlob{0xfa};
constexpr value_type LongBlob{0xfb};
constexpr value_type Blob{0xfc};
constexpr value_type VarString{0xfd};
constexpr value_type String{0xfe};
constexpr value_type Geometry{0xff};
}  // namespace field_type

namespace column_def {
namespace pos {
using value_type = uint8_t;
constexpr value_type not_null{0};
constexpr value_type primary_key{1};
constexpr value_type unique_key{2};
constexpr value_type multiple_key{3};
constexpr value_type blob{4};
constexpr value_type is_unsigned{5};
constexpr value_type zerofill{6};
constexpr value_type binary{7};
constexpr value_type is_enum{8};
constexpr value_type auto_increment{9};
constexpr value_type timestamp{10};
constexpr value_type set{11};
constexpr value_type no_default_value{12};
constexpr value_type on_update{13};
constexpr value_type numeric{14};

constexpr value_type _bitset_size{numeric + 1};
}  // namespace pos
using value_type = std::bitset<pos::_bitset_size>;

constexpr value_type not_null{1 << pos::not_null};
constexpr value_type primary_key{1 << pos::primary_key};
constexpr value_type unique_key{1 << pos::unique_key};
constexpr value_type multiple_key{1 << pos::multiple_key};
constexpr value_type blob{1 << pos::blob};
constexpr value_type is_unsigned{1 << pos::is_unsigned};
constexpr value_type zerofill{1 << pos::zerofill};
constexpr value_type binary{1 << pos::binary};
constexpr value_type is_enum{1 << pos::is_enum};
constexpr value_type auto_increment{1 << pos::auto_increment};
constexpr value_type timestamp{1 << pos::timestamp};
constexpr value_type set{1 << pos::set};
constexpr value_type no_default_value{1 << pos::no_default_value};
constexpr value_type on_update{1 << pos::on_update};
constexpr value_type numeric{1 << pos::numeric};
}  // namespace column_def

namespace reload_cmds {
namespace pos {
using value_type = uint8_t;
constexpr value_type flush_privileges{0};
constexpr value_type flush_logs{1};
constexpr value_type flush_tables{2};
constexpr value_type flush_hosts{3};
constexpr value_type flush_status{4};
constexpr value_type flush_threads{5};
constexpr value_type reset_slave{6};
constexpr value_type reset_master{7};

constexpr value_type _bitset_size{reset_master + 1};
}  // namespace pos
using value_type = std::bitset<pos::_bitset_size>;

constexpr value_type flush_privileges{1 << pos::flush_privileges};
constexpr value_type flush_logs{1 << pos::flush_logs};
constexpr value_type flush_tables{1 << pos::flush_tables};
constexpr value_type flush_hosts{1 << pos::flush_hosts};
constexpr value_type flush_status{1 << pos::flush_status};
constexpr value_type flush_threads{1 << pos::flush_threads};
constexpr value_type reset_slave{1 << pos::reset_slave};
constexpr value_type reset_master{1 << pos::reset_master};

}  // namespace reload_cmds

namespace collation {
using value_type = uint8_t;
constexpr value_type Latin1SwedishCi{0x08};
constexpr value_type Utf8GeneralCi{0x21};
constexpr value_type Binary{0x3f};
}  // namespace collation

}  // namespace classic_protocol

#endif
