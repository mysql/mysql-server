//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_PIPELINE_HPP
#define BOOST_MYSQL_PIPELINE_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/pipeline.hpp>
#include <boost/mysql/detail/writable_field_traits.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {

/**
 * \brief (EXPERIMENTAL) A variant-like type holding the response of a single pipeline stage.
 * \details
 * This is a variant-like type, similar to `boost::system::result`. At any point in time,
 * it can contain: \n
 *   \li A \ref statement. Will happen if the stage was a prepare statement that succeeded.
 *   \li A \ref results. Will happen if the stage was a query or statement execution that succeeded.
 *   \li An \ref error_code, \ref diagnostics pair. Will happen if the stage failed, or if it succeeded but
 *       it doesn't yield a value (as in close statement, reset connection and set character set).
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class stage_response
{
#ifndef BOOST_MYSQL_DOXYGEN
    struct errcode_with_diagnostics
    {
        error_code ec;
        diagnostics diag;
    };

    struct
    {
        variant2::variant<errcode_with_diagnostics, statement, results> value;

        void emplace_results() { value.emplace<results>(); }
        void emplace_error() { value.emplace<errcode_with_diagnostics>(); }
        detail::execution_processor& get_processor()
        {
            return detail::access::get_impl(variant2::unsafe_get<2>(value));
        }
        void set_result(statement s) { value = s; }
        void set_error(error_code ec, diagnostics&& diag)
        {
            value.emplace<0>(errcode_with_diagnostics{ec, std::move(diag)});
        }
    } impl_;

    friend struct detail::access;
#endif

    bool has_error() const { return impl_.value.index() == 0u; }

    BOOST_MYSQL_DECL
    void check_has_results() const;

public:
    /**
     * \brief Default constructor.
     * \details
     * Constructs an object containing an empty error code and diagnostics.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    stage_response() = default;

    /**
     * \brief Returns true if the object contains a statement.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    bool has_statement() const noexcept { return impl_.value.index() == 1u; }

    /**
     * \brief Returns true if the object contains a results.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    bool has_results() const noexcept { return impl_.value.index() == 2u; }

    /**
     * \brief Retrieves the contained error code.
     * \details
     * If `*this` contains an error, retrieves it.
     * Otherwise (if `this->has_statement() || this->has_results()`),
     * returns an empty (default-constructed) error code.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    error_code error() const noexcept
    {
        return has_error() ? variant2::unsafe_get<0>(impl_.value).ec : error_code();
    }

    /**
     * \brief Retrieves the contained diagnostics (lvalue reference accessor).
     * \details
     * If `*this` contains an error, retrieves the associated diagnostic information
     * by copying it.
     * Otherwise (if `this->has_statement() || this->has_results()`),
     * returns an empty diagnostics object.
     *
     * \par Exception safety
     * Strong guarantee: memory allocations may throw.
     */
    diagnostics diag() const&
    {
        return has_error() ? variant2::unsafe_get<0>(impl_.value).diag : diagnostics();
    }

    /**
     * \brief Retrieves the contained diagnostics (rvalue reference accessor).
     * \details
     * If `*this` contains an error, retrieves the associated diagnostic information
     * by moving it.
     * Otherwise (if `this->has_statement() || this->has_results()`),
     * returns an empty (default-constructed) error.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    diagnostics diag() && noexcept
    {
        return has_error() ? variant2::unsafe_get<0>(std::move(impl_.value)).diag : diagnostics();
    }

    /**
     * \brief Retrieves the contained statement or throws an exception.
     * \details
     * If `*this` contains an statement (`this->has_statement() == true`),
     * retrieves it. Otherwise, throws an exception.
     *
     * \par Exception safety
     * Strong guarantee. Throws on invalid input.
     * \throws std::invalid_argument If `*this` does not contain a statement.
     */
    BOOST_MYSQL_DECL
    statement as_statement() const;

    /**
     * \brief Retrieves the contained statement (unchecked accessor).
     * \details
     * If `*this` contains an statement,
     * retrieves it. Otherwise, the behavior is undefined.
     *
     * \par Preconditions
     * `this->has_statement() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    statement get_statement() const noexcept
    {
        BOOST_ASSERT(has_statement());
        return variant2::unsafe_get<1>(impl_.value);
    }

    /**
     * \brief Retrieves the contained results or throws an exception.
     * \details
     * If `*this` contains a `results` object (`this->has_results() == true`),
     * retrieves a reference to it. Otherwise, throws an exception.
     *
     * \par Exception safety
     * Strong guarantee. Throws on invalid input.
     * \throws std::invalid_argument If `this->has_results() == false`
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` is alive
     * and hasn't been assigned to.
     */
    const results& as_results() const&
    {
        check_has_results();
        return variant2::unsafe_get<2>(impl_.value);
    }

    /// \copydoc as_results
    results&& as_results() &&
    {
        check_has_results();
        return variant2::unsafe_get<2>(std::move(impl_.value));
    }

    /**
     * \brief Retrieves the contained results (unchecked accessor).
     * \details
     * If `*this` contains a `results` object, retrieves a reference to it.
     * Otherwise, the behavior is undefined.
     *
     * \par Preconditions
     * `this->has_results() == true`
     *
     * \par Exception safety
     * No-throw guarantee.
     *
     * \par Object lifetimes
     * The returned reference is valid as long as `*this` is alive
     * and hasn't been assigned to.
     */
    const results& get_results() const& noexcept
    {
        BOOST_ASSERT(has_results());
        return variant2::unsafe_get<2>(impl_.value);
    }

    /// \copydoc get_results
    results&& get_results() && noexcept
    {
        BOOST_ASSERT(has_results());
        return variant2::unsafe_get<2>(std::move(impl_.value));
    }
};

/**
 * \brief (EXPERIMENTAL) A pipeline request.
 * \details
 * Contains a collection of pipeline stages, fully describing the work to be performed
 * by a pipeline operation.
 * Call any of the `add_xxx` functions to append new stages to the request.
 *
 * \par Experimental
 * This part of the API is experimental, and may change in successive
 * releases without previous notice.
 */
class pipeline_request
{
#ifndef BOOST_MYSQL_DOXYGEN
    struct impl_t
    {
        std::vector<std::uint8_t> buffer_;
        std::vector<detail::pipeline_request_stage> stages_;
    } impl_;

    friend struct detail::access;
#endif

public:
    /**
     * \brief Default constructor.
     * \details Constructs an empty pipeline request, with no stages.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    pipeline_request() = default;

    /**
     * \brief Adds a stage that executes a text query.
     * \details
     * Creates a stage that will run `query` as a SQL query,
     * like \ref any_connection::execute.
     *
     * \par Exception safety
     * Strong guarantee. Memory allocations may throw.
     *
     * \par Object lifetimes
     * query is copied into the request and need not be kept alive after this function returns.
     */
    BOOST_MYSQL_DECL
    pipeline_request& add_execute(string_view query);

    /**
     * \brief Adds a stage that executes a prepared statement.
     * \details
     * Creates a stage that runs
     * `stmt` bound to any parameters passed in `params`, like \ref any_connection::execute
     * and \ref statement::bind. For example, `add_execute(stmt, 42, "John")` has
     * effects equivalent to `conn.execute(stmt.bind(42, "John"))`.
     *
     * \par Exception safety
     * Strong guarantee. Throws if the supplied number of parameters doesn't match the number
     * of parameters expected by the statement. Additionally, memory allocations may throw.
     * \throws std::invalid_argument If `sizeof...(params) != stmt.num_params()`
     *
     * \par Preconditions
     * The passed statement should be valid (`stmt.valid() == true`).
     *
     * \par Object lifetimes
     * Any objects pointed to by `params` are copied into the request and
     * need not be kept alive after this function returns.
     *
     * \par Type requirements
     * Any type satisfying `WritableField` can be used as a parameter.
     * This includes all types that can be used with \ref statement::bind,
     * including scalar types, strings, blobs and optionals.
     */
    template <BOOST_MYSQL_WRITABLE_FIELD... WritableField>
    pipeline_request& add_execute(statement stmt, const WritableField&... params)
    {
        std::array<field_view, sizeof...(WritableField)> params_arr{{detail::to_field(params)...}};
        return add_execute_range(stmt, params_arr);
    }

    /**
     * \brief Adds a stage that executes a prepared statement.
     * \details
     * Creates a stage that runs
     * `stmt` bound to any parameters passed in `params`, like \ref any_connection::execute
     * and \ref statement::bind. For example, `add_execute_range(stmt, params)` has
     * effects equivalent to `conn.execute(stmt.bind(params.begin(), params.end()))`.
     * \n
     * This function can be used instead of \ref add_execute when the number of actual parameters
     * of a statement is not known at compile time.
     *
     * \par Exception safety
     * Strong guarantee. Throws if the supplied number of parameters doesn't match the number
     * of parameters expected by the statement. Additionally, memory allocations may throw.
     * \throws std::invalid_argument If `params.size() != stmt.num_params()`
     *
     * \par Preconditions
     * The passed statement should be valid (`stmt.valid() == true`).
     *
     * \par Object lifetimes
     * The `params` range is copied into the request and
     * needs not be kept alive after this function returns.
     */
    BOOST_MYSQL_DECL
    pipeline_request& add_execute_range(statement stmt, span<const field_view> params);

    /**
     * \brief Adds a prepare statement stage.
     * \details
     * Creates a stage that prepares a statement server-side. The resulting
     * stage has effects equivalent to `conn.prepare_statement(stmt_sql)`.
     *
     * \par Exception safety
     * Strong guarantee. Memory allocations may throw.
     *
     * \par Object lifetimes
     * stmt_sql is copied into the request and need not be kept alive after this function returns.
     */
    BOOST_MYSQL_DECL
    pipeline_request& add_prepare_statement(string_view stmt_sql);

    /**
     * \brief Adds a close statement stage.
     * \details
     * Creates a stage that closes a prepared statement. The resulting
     * stage has effects equivalent to `conn.close_statement(stmt)`.
     *
     * \par Exception safety
     * Strong guarantee. Memory allocations may throw.
     *
     * \par Preconditions
     * The passed statement should be valid (`stmt.valid() == true`).
     */
    BOOST_MYSQL_DECL pipeline_request& add_close_statement(statement stmt);

    /**
     * \brief Adds a reset connection stage.
     * \details
     * Creates a stage that resets server-side session state. The resulting
     * stage has effects equivalent to `conn.reset_connection()`.
     *
     * \par Exception safety
     * Strong guarantee. Memory allocations may throw.
     */
    BOOST_MYSQL_DECL pipeline_request& add_reset_connection();

    /**
     * \brief Adds a set character set stage.
     * \details
     * Creates a stage that sets the connection's character set.
     * The resulting stage has effects equivalent to `conn.set_character_set(charset)`.
     *
     * \par Exception safety
     * Strong guarantee. Throws if the supplied character set name is not valid
     * (i.e. `charset.name` contains non-ASCII characters).
     * The check is performed as a hardening measure, and never happens with
     * the character sets provided by this library.
     *
     * Additionally, memory allocations may throw.
     * \throws std::invalid_argument If `charset.name` contains non-ASCII characters.
     *
     * \par Preconditions
     * The passed character set should not be default-constructed
     * (`charset.name != nullptr`).
     */
    BOOST_MYSQL_DECL pipeline_request& add_set_character_set(character_set charset);

    /**
     * \brief Removes all stages in the pipeline request, making the object empty again.
     * \details
     * Can be used to re-use a single request object for multiple pipeline operations.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    void clear() noexcept
    {
        impl_.buffer_.clear();
        impl_.stages_.clear();
    }
};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/pipeline.ipp>
#endif

#endif
