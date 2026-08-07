#ifndef BOOST_PARSER_DETAIL_CASE_FOLD_HPP
#define BOOST_PARSER_DETAIL_CASE_FOLD_HPP

#include <boost/parser/config.hpp>
#include <boost/parser/detail/text/transcode_iterator.hpp>
#include <boost/parser/detail/case_fold_data_generated.hpp>

#include <algorithm>


namespace boost::parser::detail {

    template<typename I>
    std::optional<I> do_short_mapping(
        short_mapping_range const * first,
        short_mapping_range const * last,
        char32_t cp,
        I out)
    {
        auto it = std::lower_bound(
            first,
            last,
            cp,
            [](short_mapping_range const & range, char32_t cp) {
                return range.cp_first_ < cp;
            });
        if (it != first) {
            auto const prev = it - 1;
            if (prev->cp_first_ <= cp && cp < prev->cp_last_)
                it = prev;
        }
        if (it != last && it->cp_first_ <= cp && cp < it->cp_last_) {
            auto const offset = cp - it->cp_first_;
            if (offset % it->stride_ == 0) {
                *out++ =
                    single_mapping_cps[it->first_idx_ + offset / it->stride_];
                return out;
            }
        }

        return std::nullopt;
    }

    template<typename I>
    I case_fold(char32_t cp, I out)
    {
        // One-byte fast path.
        if (cp < 0x100) {
            // ASCII letter fast path.
            if (0x61 <= cp && cp <= 0x7a) {
                *out++ = cp;
                return out;
            } else if (0x41 <= cp && cp <= 0x5a) {
                *out++ = cp + 0x20;
                return out;
            } else if (cp == 0x00DF) {
                // The lone multi-mapping below 0x100.
                *out++ = 0x0073;
                *out++ = 0x0073;
                return out;
            } else {
                // Skip [0x41, 0x5a), handled above.
                auto const first = text::detail::begin(mapping_ranges) + 1;
                // 7th entry starts with 0x100.
                auto const last = text::detail::begin(mapping_ranges) + 7;
                if (auto out_opt = do_short_mapping(first, last, cp, out))
                    return *out_opt;
            }
            *out++ = cp;
            return out;
        }

        // Single-cp-mapping path (next most common case).
        {
            auto const first = text::detail::begin(mapping_ranges);
            auto const last = text::detail::end(mapping_ranges);
            if (auto out_opt = do_short_mapping(first, last, cp, out))
                return *out_opt;
        }

        // Multi-cp mapping path.
        {
            auto const last = detail::text::detail::end(long_mappings);
            auto const it = std::lower_bound(
                detail::text::detail::begin(long_mappings),
                last,
                cp,
                [](long_mapping const & mapping, char32_t cp) {
                    return mapping.cp_ < cp;
                });
            if (it != last && it->cp_ == cp) {
#if BOOST_PARSER_USE_CONCEPTS
                return std::ranges::copy(it->mapping_, text::null_sentinel, out)
                    .out;
#else
                return std::copy(
                    it->mapping_,
                    std::find(
                        text::detail::begin(it->mapping_),
                        text::detail::end(it->mapping_),
                        0),
                    out);
#endif
            }
        }

        *out++ = cp;
        return out;
    }
}

#endif
