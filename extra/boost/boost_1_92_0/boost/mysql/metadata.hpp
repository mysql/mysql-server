//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_METADATA_HPP
#define BOOST_MYSQL_METADATA_HPP

#include <boost/mysql/column_type.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/coldef_view.hpp>
#include <boost/mysql/detail/flags.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace boost {
namespace mysql {

/**
 * \brief Metadata about a column in a SQL query.
 * \details This is a regular, value type. Instances of this class are not created by the user
 * directly, but by the library.
 */
class metadata
{
public:
    /**
     * \brief Default constructor.
     * \details The constructed metadata object has undefined
     * values for all of its members.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    metadata() = default;

    /**
     * \brief Move constructor.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * `string_view`s obtained by calling accessor functions on `other` are invalidated.
     */
    metadata(metadata&& other) = default;

    /**
     * \brief Copy constructor.
     *
     * \par Exception safety
     * Strong guarantee. Internal allocations may throw.
     */
    metadata(const metadata& other) = default;

    /**
     * \brief Move assignment.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * `string_view`s obtained by calling accessor functions on both `*this` and `other`
     * are invalidated.
     */
    metadata& operator=(metadata&& other) = default;

    /**
     * \brief Copy assignment.
     *
     * \par Exception safety
     * Basic guarantee. Internal allocations may throw.
     *
     * \par Object lifetimes
     * `string_view`s obtained by calling accessor functions on `*this`
     * are invalidated.
     */
    metadata& operator=(const metadata& other) = default;

    /// Destructor.
    ~metadata() = default;

    /**
     * \brief Returns the name of the database (schema) the column belongs to.
     * \details
     * This is optional information - it won't be populated unless
     * the connection executing the query has `meta_mode() == metadata_mode::full`.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` is alive and hasn't been
     * assigned to or moved from.
     */
    string_view database() const noexcept { return substring(0, table_offset_); }

    /**
     * \brief Returns the name of the virtual table the column belongs to.
     * \details If the table was aliased, this will be the name of the alias
     * (e.g. in `"SELECT * FROM employees emp"`, `table()` will be `"emp"`).
     *\n
     * This is optional information - it won't be populated unless
     * the connection executing the query has `meta_mode() == metadata_mode::full`.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` is alive and hasn't been
     * assigned to or moved from.
     */
    string_view table() const noexcept { return substring(table_offset_, org_table_offset_); }

    /**
     * \brief Returns the name of the physical table the column belongs to.
     * \details E.g. in `"SELECT * FROM employees emp"`,
     * `original_table()` will be `"employees"`.
     * \n
     * This is optional information - it won't be populated unless
     * the connection executing the query has `meta_mode() == metadata_mode::full`.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` is alive and hasn't been
     * assigned to or moved from.
     */
    string_view original_table() const noexcept { return substring(org_table_offset_, name_offset_); }

    /**
     * \brief Returns the actual name of the column.
     * \details If the column was aliased, this will be the name of the alias
     * (e.g. in `"SELECT id AS employee_id FROM employees"`,
     * `column_name()` will be `"employee_id"`).
     *\n
     * This is optional information - it won't be populated unless
     * the connection executing the query has `meta_mode() == metadata_mode::full`.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` is alive and hasn't been
     * assigned to or moved from.
     */
    string_view column_name() const noexcept { return substring(name_offset_, org_name_offset_); }

    /**
     * \brief Returns the original (physical) name of the column.
     * \details E.g. in `"SELECT id AS employee_id FROM employees"`,
     * `original_column_name()` will be `"id"`.
     * \n
     * This is optional information - it won't be populated unless
     * the connection executing the query has `meta_mode() == metadata_mode::full`.
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` is alive and hasn't been
     * assigned to or moved from.
     */
    string_view original_column_name() const noexcept { return substring(org_name_offset_, strings_.size()); }

    /**
     * \brief Returns the ID of the collation that fields belonging to this column use.
     * \details This is <b>not</b> the collation used when defining the column
     * in a `CREATE TABLE` statement, but the collation that fields that belong to
     * this column and are sent to the client have. It usually matches the connection's collation.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    std::uint16_t column_collation() const noexcept { return character_set_; }

    /**
     * \brief Returns the maximum length of the column.
     * \par Exception safety
     * No-throw guarantee.
     */
    unsigned column_length() const noexcept { return column_length_; }

    /**
     * \brief  Returns the type of the column (see \ref column_type for more info).
     * \par Exception safety
     * No-throw guarantee.
     */
    column_type type() const noexcept { return type_; }

    /**
     * \brief  Returns the number of decimals of the column.
     * \par Exception safety
     * No-throw guarantee.
     */
    unsigned decimals() const noexcept { return decimals_; }

    /**
     * \brief Returns `true` if the column is not allowed to be NULL, `false` if it is nullable.
     * \par Exception safety
     * No-throw guarantee.
     */
    bool is_not_null() const noexcept { return flag_set(detail::column_flags::not_null); }

    /**
     * \brief Returns `true` if the column is part of a `PRIMARY KEY`.
     * \par Exception safety
     * No-throw guarantee.
     */
    bool is_primary_key() const noexcept { return flag_set(detail::column_flags::pri_key); }

    /**
     * \brief Returns `true` if the column is part of a `UNIQUE KEY` (but not a `PRIMARY KEY`).
     * \par Exception safety
     * No-throw guarantee.
     */
    bool is_unique_key() const noexcept { return flag_set(detail::column_flags::unique_key); }

    /**
     * \brief Returns `true` if the column is part of a `KEY` (but not a `UNIQUE KEY` or `PRIMARY KEY`).
     * \par Exception safety
     * No-throw guarantee.
     */
    bool is_multiple_key() const noexcept { return flag_set(detail::column_flags::multiple_key); }

    /**
     * \brief Returns `true` if the column has no sign (is `UNSIGNED`).
     * \par Exception safety
     * No-throw guarantee.
     */
    bool is_unsigned() const noexcept { return flag_set(detail::column_flags::unsigned_); }

    /**
     * \brief Returns `true` if the column is defined as `ZEROFILL` (padded to its maximum length by
     *        zeros).
     * \par Exception safety
     * No-throw guarantee.
     */
    bool is_zerofill() const noexcept { return flag_set(detail::column_flags::zerofill); }

    /**
     * \brief Returns `true` if the column is defined as `AUTO_INCREMENT`.
     * \par Exception safety
     * No-throw guarantee.
     */
    bool is_auto_increment() const noexcept { return flag_set(detail::column_flags::auto_increment); }

    /**
     * \brief Returns `true` if the column does not have a default value.
     * \par Exception safety
     * No-throw guarantee.
     */
    bool has_no_default_value() const noexcept { return flag_set(detail::column_flags::no_default_value); }

    /**
     * \brief Returns `true` if the column is defined as `ON UPDATE CURRENT_TIMESTAMP`.
     * \par Exception safety
     * No-throw guarantee.
     */
    bool is_set_to_now_on_update() const noexcept { return flag_set(detail::column_flags::on_update_now); }

private:
    // All strings together: schema, table, org table, name, org name
    std::vector<char> strings_;
    std::size_t table_offset_{};      // virtual table
    std::size_t org_table_offset_{};  // physical table
    std::size_t name_offset_{};       // virtual column name
    std::size_t org_name_offset_{};   // physical column name
    std::uint16_t character_set_{};
    std::uint32_t column_length_{};  // maximum length of the field
    column_type type_{};             // type of the column
    std::uint16_t flags_{};          // Flags as defined in Column Definition Flags
    std::uint8_t decimals_{};        // max shown decimal digits. 0x00 for int/static strings; 0x1f for
                                     // dynamic strings, double, float

    static std::size_t total_string_size(const detail::coldef_view& coldef)
    {
        return coldef.database.size() + coldef.table.size() + coldef.org_table.size() + coldef.name.size() +
               coldef.org_name.size();
    }

    static char* copy_string(string_view from, char* to)
    {
        if (!from.empty())
            std::memcpy(to, from.data(), from.size());
        return to + from.size();
    }

    metadata(const detail::coldef_view& coldef, bool copy_strings)
        : strings_(copy_strings ? total_string_size(coldef) : 0u, '\0'),
          character_set_(coldef.collation_id),
          column_length_(coldef.column_length),
          type_(coldef.type),
          flags_(coldef.flags),
          decimals_(coldef.decimals)
    {
        if (copy_strings)
        {
            // Offsets
            table_offset_ = coldef.database.size();
            org_table_offset_ = table_offset_ + coldef.table.size();
            name_offset_ = org_table_offset_ + coldef.org_table.size();
            org_name_offset_ = name_offset_ + coldef.name.size();

            // Values. The packet points into a network packet, so it's guaranteed to
            // not overlap with
            char* it = strings_.data();
            it = copy_string(coldef.database, it);
            it = copy_string(coldef.table, it);
            it = copy_string(coldef.org_table, it);
            it = copy_string(coldef.name, it);
            it = copy_string(coldef.org_name, it);
            BOOST_ASSERT(it == strings_.data() + strings_.size());
        }
    }

    bool flag_set(std::uint16_t flag) const noexcept { return flags_ & flag; }

    string_view substring(std::size_t first, std::size_t last) const
    {
        return {strings_.data() + first, static_cast<std::size_t>(last - first)};
    }

#ifndef BOOST_MYSQL_DOXYGEN
    friend struct detail::access;
#endif
};

}  // namespace mysql
}  // namespace boost

#endif
