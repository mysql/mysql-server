//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DATE_HPP
#define BOOST_MYSQL_DATE_HPP

#include <boost/mysql/days.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/datetime.hpp>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/throw_exception.hpp>

#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <stdexcept>

namespace boost {
namespace mysql {

/**
 * \brief Type representing MySQL `DATE` data type.
 * \details
 * Represents a Gregorian date broken by its year, month and day components, without a time zone.
 * \n
 * This type is close to the protocol and should not be used as a vocabulary type.
 * Instead, cast it to a `std::chrono::time_point` by calling \ref as_time_point,
 * \ref get_time_point, \ref as_local_time_point or \ref get_local_time_point.
 * \n
 * Dates retrieved from MySQL don't include any time zone information. Determining the time zone
 * is left to the application. Thus, any time point obtained from this class should be
 * interpreted as a local time in an unspecified time zone, like `std::chrono::local_time`.
 * For compatibility with older compilers, \ref as_time_point and \ref get_time_point return
 * `system_clock` time points. These should be interpreted as local times rather
 * than UTC. Prefer using \ref as_local_time_point or \ref get_local_time_point
 * if your compiler supports them, as they provide more accurate semantics.
 * \n
 * As opposed to `time_point`, this type allows representing MySQL invalid and zero dates.
 * These values are allowed by MySQL but don't represent real dates.
 * \n
 * Note: using `std::chrono` time zone functionality under MSVC may cause memory leaks to be reported.
 * See <a href="https://github.com/microsoft/STL/issues/2047">this issue</a> for an explanation and
 * <a href="https://github.com/microsoft/STL/issues/2504">this other issue</a> for a workaround.
 */
class date
{
public:
    /**
     * \brief A `std::chrono::time_point` that can represent any valid `date`.
     * \details
     * Time points used by this class are always local times, even if defined
     * to use the system clock.
     */
    using time_point = std::chrono::time_point<std::chrono::system_clock, days>;

    /**
     * \brief Constructs a zero date.
     * \details
     * Results in a date with all of its components set to zero.
     * The resulting object has `this->valid() == false`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr date() noexcept = default;

    /**
     * \brief Constructs a date from its year, month and date components.
     * \details
     * Component values that yield invalid dates (like zero or out-of-range
     * values) are allowed, resulting in an object with `this->valid() == false`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr date(std::uint16_t year, std::uint8_t month, std::uint8_t day) noexcept
        : year_(year), month_(month), day_(day)
    {
    }

    /**
     * \brief Constructs a date from a `time_point`.
     * \details
     * The time point is interpreted as a local time. No time zone conversion is performed.
     *
     * \par Exception safety
     * Strong guarantee. Throws on invalid input.
     * \throws std::out_of_range If the resulting `date` would be
     * out of the [\ref min_date, \ref max_date] range.
     */
    BOOST_CXX14_CONSTEXPR explicit date(time_point tp)
    {
        bool ok = detail::days_to_ymd(tp.time_since_epoch().count(), year_, month_, day_);
        if (!ok)
            BOOST_THROW_EXCEPTION(std::out_of_range("date::date: time_point was out of range"));
    }

#ifdef BOOST_MYSQL_HAS_LOCAL_TIME
    /**
     * \brief Constructs a date from a local time point.
     * \details
     * Equivalent to constructing a `date` from a `time_point` with the same
     * `time_since_epoch()` as `tp`.
     * \n
     * Requires C++20 calendar types.
     *
     * \par Exception safety
     * Strong guarantee. Throws on invalid input.
     * \throws std::out_of_range If the resulting `date` would be
     * out of the [\ref min_date, \ref max_date] range.
     */
    constexpr explicit date(std::chrono::local_days tp) : date(time_point(tp.time_since_epoch())) {}
#endif

    /**
     * \brief Retrieves the year component.
     * \details
     * Represents the year number in the Gregorian calendar.
     * If `this->valid() == true`, this value is within the `[0, 9999]` range.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint16_t year() const noexcept { return year_; }

    /**
     * \brief Retrieves the month component (1-based).
     * \details
     * A value of 1 represents January.
     * If `this->valid() == true`, this value is within the `[1, 12]` range.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint8_t month() const noexcept { return month_; }

    /**
     * \brief Retrieves the day component (1-based).
     * \details
     * A value of 1 represents the first day of the month.
     * If `this->valid() == true`, this value is within the `[1, last_month_day]` range
     * (where `last_month_day` is the last day of the month).
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint8_t day() const noexcept { return day_; }

    /**
     * \brief Returns `true` if `*this` represents a valid `time_point`.
     * \details If any of the individual components is out of range, the date
     * doesn't represent an actual `time_point` (e.g. `date(2020, 2, 30)`) or
     * the date is not in the [\ref min_date, \ref max_date] validity range,
     * returns `false`. Otherwise, returns `true`.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr bool valid() const noexcept { return detail::is_valid(year_, month_, day_); }

    /**
     * \brief Converts `*this` into a `time_point` (unchecked access).
     * \details
     * If your compiler supports it, prefer using \ref get_local_time_point,
     * as it provides more accurate semantics.
     *
     * \par Preconditions
     * `this->valid() == true` (if violated, results in undefined behavior).
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    BOOST_CXX14_CONSTEXPR time_point get_time_point() const noexcept
    {
        BOOST_ASSERT(valid());
        return time_point(unch_get_days());
    }

    /**
     * \brief Converts `*this` into a `time_point` (checked access).
     * \details
     * If your compiler supports it, prefer using \ref as_local_time_point,
     * as it provides more accurate semantics.
     *
     * \par Exception safety
     * Strong guarantee.
     * \throws std::invalid_argument If `!this->valid()`.
     */
    BOOST_CXX14_CONSTEXPR time_point as_time_point() const
    {
        if (!valid())
            BOOST_THROW_EXCEPTION(std::invalid_argument("date::as_time_point: invalid date"));
        return time_point(unch_get_days());
    }

#ifdef BOOST_MYSQL_HAS_LOCAL_TIME
    /**
     * \brief Converts `*this` into a local time point (unchecked access).
     * \details
     * The returned object has the same `time_since_epoch()` as `this->get_time_point()`,
     * but uses the `std::chrono::local_t` pseudo-clock to better represent
     * the absence of time zone information.
     * \n
     * Requires C++20 calendar types.
     *
     * \par Preconditions
     * `this->valid() == true` (if violated, results in undefined behavior).
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::chrono::local_days get_local_time_point() const noexcept
    {
        BOOST_ASSERT(valid());
        return std::chrono::local_days(unch_get_days());
    }

    /**
     * \brief Converts `*this` into a local time point (checked access).
     * \details
     * The returned object has the same `time_since_epoch()` as `this->as_time_point()`,
     * but uses the `std::chrono::local_t` pseudo-clock to better represent
     * the absence of time zone information.
     * \n
     * Requires C++20 calendar types.
     *
     * \par Exception safety
     * Strong guarantee.
     * \throws std::invalid_argument If `!this->valid()`.
     */
    constexpr std::chrono::local_days as_local_time_point() const
    {
        if (!valid())
            BOOST_THROW_EXCEPTION(std::invalid_argument("date::as_local_time_point: invalid date"));
        return std::chrono::local_days(unch_get_days());
    }
#endif

    /**
     * \brief Tests for equality.
     * \details Two dates are considered equal if all of its individual components
     * are equal. This function works for invalid dates, too.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr bool operator==(const date& rhs) const noexcept
    {
        return year_ == rhs.year_ && month_ == rhs.month_ && day_ == rhs.day_;
    }

    /**
     * \brief Tests for inequality.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr bool operator!=(const date& rhs) const noexcept { return !(rhs == *this); }

    /**
     * \brief Returns the current system time as a date object.
     * \par Exception safety
     * Strong guarantee. Only throws if obtaining the current time throws.
     */
    static date now()
    {
        auto now = time_point::clock::now();
        return date(std::chrono::time_point_cast<time_point::duration>(now));
    }

private:
    std::uint16_t year_{};
    std::uint8_t month_{};
    std::uint8_t day_{};

    BOOST_CXX14_CONSTEXPR days unch_get_days() const
    {
        return days(detail::ymd_to_days(year_, month_, day_));
    }
};

/**
 * \relates date
 * \brief Streams a date.
 * \details This function works for invalid dates, too.
 */
BOOST_MYSQL_DECL
std::ostream& operator<<(std::ostream& os, const date& v);

/// The minimum allowed value for \ref date.
BOOST_INLINE_CONSTEXPR date min_date{0u, 1u, 1u};

/// The maximum allowed value for \ref date.
BOOST_INLINE_CONSTEXPR date max_date{9999u, 12u, 31u};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/date.ipp>
#endif

#endif
