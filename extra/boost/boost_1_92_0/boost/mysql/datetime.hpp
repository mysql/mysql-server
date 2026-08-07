//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DATETIME_HPP
#define BOOST_MYSQL_DATETIME_HPP

#include <boost/mysql/days.hpp>

#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/datetime.hpp>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/throw_exception.hpp>

#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <ratio>
#include <stdexcept>

namespace boost {
namespace mysql {

/**
 * \brief Type representing MySQL `DATETIME` and `TIMESTAMP` data types.
 * \details Represents a Gregorian date and time broken by its year, month, day, hour, minute, second and
 * microsecond components, without a time zone.
 * \n
 * This type is close to the protocol and should not be used as a vocabulary type.
 * Instead, cast it to a `std::chrono::time_point` by calling \ref as_time_point,
 * \ref get_time_point, \ref as_local_time_point or \ref get_local_time_point.
 * \n
 * Datetimes retrieved from MySQL don't include any time zone information. Determining the time zone
 * is left to the application. Thus, any time point obtained from this class should be
 * interpreted as a local time in an unspecified time zone, like `std::chrono::local_time`.
 * For compatibility with older compilers, \ref as_time_point and \ref get_time_point return
 * `system_clock` time points. These should be interpreted as local times rather
 * than UTC. Prefer using \ref as_local_time_point or \ref get_local_time_point
 * if your compiler supports them, as they provide more accurate semantics.
 * \n
 * As opposed to `time_point`, this type allows representing MySQL invalid and zero datetimes.
 * These values are allowed by MySQL but don't represent real time points.
 * \n
 * Note: using `std::chrono` time zone functionality under MSVC may cause memory leaks to be reported.
 * See <a href="https://github.com/microsoft/STL/issues/2047">this issue</a> for an explanation and
 * <a href="https://github.com/microsoft/STL/issues/2504">this other issue</a> for a workaround.
 */
class datetime
{
public:
    /**
     * \brief A `std::chrono::time_point` that can represent any valid datetime, with microsecond resolution.
     * \details
     * Time points used by this class are always local times, even if defined
     * to use the system clock. Prefer using \ref local_time_point, if your compiler
     * supports it.
     */
    using time_point = std::chrono::
        time_point<std::chrono::system_clock, std::chrono::duration<std::int64_t, std::micro>>;

#ifdef BOOST_MYSQL_HAS_LOCAL_TIME

    /**
     * \brief A `std::chrono::local_time` that can represent any valid datetime, with microsecond resolution.
     * \details Requires C++20 calendar types.
     */
    using local_time_point = std::chrono::local_time<std::chrono::duration<std::int64_t, std::micro>>;
#endif

    /**
     * \brief Constructs a zero datetime.
     * \details Results in a datetime with all of its components set to zero.
     * The resulting object has `this->valid() == false`.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr datetime() noexcept = default;

    /**
     * \brief Constructs a datetime from its individual components.
     * \details
     * Component values that yield invalid datetimes (like zero or out-of-range
     * values) are allowed, resulting in an object with `this->valid() == false`.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr datetime(
        std::uint16_t year,
        std::uint8_t month,
        std::uint8_t day,
        std::uint8_t hour = 0,
        std::uint8_t minute = 0,
        std::uint8_t second = 0,
        std::uint32_t microsecond = 0
    ) noexcept
        : year_(year),
          month_(month),
          day_(day),
          hour_(hour),
          minute_(minute),
          second_(second),
          microsecond_(microsecond)
    {
    }

    /**
     * \brief Constructs a datetime from a `time_point`.
     * \par Exception safety
     * Strong guarantee. Throws on invalid input.
     * \throws std::out_of_range If the resulting `datetime` object would be
     * out of the [\ref min_datetime, \ref max_datetime] range.
     */
    BOOST_CXX14_CONSTEXPR inline explicit datetime(time_point tp);

#ifdef BOOST_MYSQL_HAS_LOCAL_TIME
    /**
     * \brief Constructs a datetime from a `local_time_point`.
     * \details
     * Equivalent to constructing a `date` from a `time_point` with the same
     * `time_since_epoch()` as `tp`.
     * \n
     * Requires C++20 calendar types.
     *
     * \par Exception safety
     * Strong guarantee. Throws on invalid input.
     * \throws std::out_of_range If the resulting `datetime` object would be
     * out of the [\ref min_datetime, \ref max_datetime] range.
     */
    constexpr explicit datetime(local_time_point tp) : datetime(time_point(tp.time_since_epoch())) {}
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
     * \brief Retrieves the hour component.
     * \details If `this->valid() == true`, this value is within the `[0, 23]` range.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint8_t hour() const noexcept { return hour_; }

    /**
     * \brief Retrieves the minute component.
     * \details If `this->valid() == true`, this value is within the `[0, 59]` range.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint8_t minute() const noexcept { return minute_; }

    /**
     * \brief Retrieves the second component.
     * \details If `this->valid() == true`, this value is within the `[0, 59]` range.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint8_t second() const noexcept { return second_; }

    /**
     * \brief Retrieves the microsecond component.
     * \details If `this->valid() == true`, this value is within the `[0, 999999]` range.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr std::uint32_t microsecond() const noexcept { return microsecond_; }

    /**
     * \brief Returns `true` if `*this` represents a valid `time_point`.
     * \details If any of the individual components is out of range, the datetime
     * doesn't represent an actual `time_point` (e.g. `datetime(2020, 2, 30)`) or
     * the datetime is not in the [\ref min_date, \ref max_date] validity range,
     * returns `false`. Otherwise, returns `true`.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr bool valid() const noexcept
    {
        return detail::is_valid(year_, month_, day_) && hour_ <= detail::max_hour &&
               minute_ <= detail::max_min && second_ <= detail::max_sec && microsecond_ <= detail::max_micro;
    }

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
        return time_point(unch_get_micros());
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
    BOOST_CXX14_CONSTEXPR inline time_point as_time_point() const
    {
        if (!valid())
            BOOST_THROW_EXCEPTION(std::invalid_argument("datetime::as_time_point: invalid datetime"));
        return time_point(unch_get_micros());
    }

#ifdef BOOST_MYSQL_HAS_LOCAL_TIME
    /**
     * \brief Converts `*this` into a `local_time_point` (unchecked access).
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
    constexpr local_time_point get_local_time_point() const noexcept
    {
        BOOST_ASSERT(valid());
        return local_time_point(unch_get_micros());
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
    constexpr local_time_point as_local_time_point() const
    {
        if (!valid())
            BOOST_THROW_EXCEPTION(std::invalid_argument("date::as_local_time_point: invalid date"));
        return local_time_point(unch_get_micros());
    }
#endif

    /**
     * \brief Tests for equality.
     * \details Two datetimes are considered equal if all of its individual components
     * are equal. This function works for invalid datetimes, too.
     *
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr bool operator==(const datetime& rhs) const noexcept
    {
        return year_ == rhs.year_ && month_ == rhs.month_ && day_ == rhs.day_ && hour_ == rhs.hour_ &&
               minute_ == rhs.minute_ && second_ == rhs.second_ && microsecond_ == rhs.microsecond_;
    }

    /**
     * \brief Tests for inequality.
     * \par Exception safety
     * No-throw guarantee.
     */
    constexpr bool operator!=(const datetime& rhs) const noexcept { return !(*this == rhs); }

    /**
     * \brief Returns the current system time as a datetime object.
     * \par Exception safety
     * Strong guarantee. Only throws if obtaining the current time throws.
     */
    static datetime now()
    {
        auto now = time_point::clock::now();
        return datetime(std::chrono::time_point_cast<time_point::duration>(now));
    }

private:
    std::uint16_t year_{};
    std::uint8_t month_{};
    std::uint8_t day_{};
    std::uint8_t hour_{};
    std::uint8_t minute_{};
    std::uint8_t second_{};
    std::uint32_t microsecond_{};

    BOOST_CXX14_CONSTEXPR inline time_point::duration unch_get_micros() const
    {
        // Doing time of day independently to prevent overflow
        days d(detail::ymd_to_days(year_, month_, day_));
        auto time_of_day = std::chrono::hours(hour_) + std::chrono::minutes(minute_) +
                           std::chrono::seconds(second_) + std::chrono::microseconds(microsecond_);
        return time_point::duration(d) + time_of_day;
    }
};

/**
 * \relates datetime
 * \brief Streams a datetime.
 * \details This function works for invalid datetimes, too.
 */
BOOST_MYSQL_DECL
std::ostream& operator<<(std::ostream& os, const datetime& v);

/// The minimum allowed value for \ref datetime.
BOOST_INLINE_CONSTEXPR datetime min_datetime(0u, 1u, 1u);

/// The maximum allowed value for \ref datetime.
BOOST_INLINE_CONSTEXPR datetime max_datetime(9999u, 12u, 31u, 23u, 59u, 59u, 999999u);

}  // namespace mysql
}  // namespace boost

// Implementations
BOOST_CXX14_CONSTEXPR boost::mysql::datetime::datetime(time_point tp)
{
    using std::chrono::duration_cast;
    using std::chrono::hours;
    using std::chrono::microseconds;
    using std::chrono::minutes;
    using std::chrono::seconds;

    // Avoiding using -= for durations as it's not constexpr until C++17
    auto input_dur = tp.time_since_epoch();
    auto rem = input_dur % days(1);
    auto num_days = duration_cast<days>(input_dur);
    if (rem.count() < 0)
    {
        rem = rem + days(1);
        num_days = num_days - days(1);
    }
    auto num_hours = duration_cast<hours>(rem);
    rem = rem - num_hours;
    auto num_minutes = duration_cast<minutes>(rem);
    rem = rem - num_minutes;
    auto num_seconds = duration_cast<seconds>(rem);
    rem = rem - num_seconds;
    auto num_microseconds = duration_cast<microseconds>(rem);

    BOOST_ASSERT(num_hours.count() >= 0 && num_hours.count() <= detail::max_hour);
    BOOST_ASSERT(num_minutes.count() >= 0 && num_minutes.count() <= detail::max_min);
    BOOST_ASSERT(num_seconds.count() >= 0 && num_seconds.count() <= detail::max_sec);
    BOOST_ASSERT(num_microseconds.count() >= 0 && num_microseconds.count() <= detail::max_micro);

    bool ok = detail::days_to_ymd(num_days.count(), year_, month_, day_);
    if (!ok)
        BOOST_THROW_EXCEPTION(std::out_of_range("datetime::datetime: time_point was out of range"));

    microsecond_ = static_cast<std::uint32_t>(num_microseconds.count());
    second_ = static_cast<std::uint8_t>(num_seconds.count());
    minute_ = static_cast<std::uint8_t>(num_minutes.count());
    hour_ = static_cast<std::uint8_t>(num_hours.count());
}

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/datetime.ipp>
#endif

#endif
