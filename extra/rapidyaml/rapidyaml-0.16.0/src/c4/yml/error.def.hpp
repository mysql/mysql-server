#ifndef C4_YML_ERROR_DEF_HPP_
#define C4_YML_ERROR_DEF_HPP_

/** @file error.def.hpp Definitions of error utilities used by ryml. */

#ifndef C4_YML_ERROR_HPP_
#include <c4/yml/error.hpp>
#endif

#include <cassert>

// NOLINTBEGIN(bugprone-use-after-move,hicpp-invalid-access-moved)

namespace c4 {
namespace yml {

template<class DumpFn>
C4_NO_INLINE size_t location_format(DumpFn &&dumpfn, Location const& loc)
{
    if(!loc)
        return 0;
    char buf_[32];
    substr buf(buf_);
    size_t count = 0;
    if(!loc.name.empty())
    {
        std::forward<DumpFn>(dumpfn)(loc.name);
        std::forward<DumpFn>(dumpfn)(":");
        count += loc.name.len + 1;
    }
    if(loc.line != npos)
    {
        csubstr val = detail::_to_chars_limited(buf, loc.line);
        if(loc.name.empty())
        {
            std::forward<DumpFn>(dumpfn)("line=");
            std::forward<DumpFn>(dumpfn)(val);
            if(loc.col == npos)
            {
                std::forward<DumpFn>(dumpfn)(":");
                ++count;
            }
            count += val.len + 5;
        }
        else
        {
            std::forward<DumpFn>(dumpfn)(val);
            std::forward<DumpFn>(dumpfn)(":");
            count += val.len + 1;
        }
    }
    if(loc.col != npos)
    {
        csubstr val = detail::_to_chars_limited(buf, loc.col);
        if(loc.line != npos || !loc.name.empty())
        {
            std::forward<DumpFn>(dumpfn)(" ");
            ++count;
        }
        std::forward<DumpFn>(dumpfn)("col=");
        std::forward<DumpFn>(dumpfn)(val);
        count += val.len + 4;
        if(loc.offset == npos)
        {
            std::forward<DumpFn>(dumpfn)(":");
            ++count;
        }
    }
    if(loc.offset != npos)
    {
        csubstr val = detail::_to_chars_limited(buf, loc.offset);
        if(loc.line != npos || loc.col != npos || !loc.name.empty())
        {
            std::forward<DumpFn>(dumpfn)(" ");
            ++count;
        }
        std::forward<DumpFn>(dumpfn)("(");
        std::forward<DumpFn>(dumpfn)(val);
        std::forward<DumpFn>(dumpfn)("B):");
        count += val.len + 5;
    }
    return count;
}

template<class DumpFn>
C4_NO_INLINE void location_format_with_context(DumpFn &&dumpfn,
                                               Location const &location,
                                               csubstr source_buffer,
                                               csubstr call,
                                               size_t num_lines_before,
                                               size_t num_lines_after,
                                               size_t first_col_highlight,
                                               size_t last_col_highlight,
                                               size_t maxlen)
{
    if(!location)
        return;
    char buf_[32];
    substr buf(buf_);
    auto pr = [&](csubstr s){ std::forward<DumpFn>(dumpfn)(s); };
    auto prn = [&](csubstr s, size_t num_times){
        for(size_t i = 0; i < num_times; ++i)
            std::forward<DumpFn>(dumpfn)(s);
    };
    csubstr line = detail::_get_text_region(source_buffer, location.offset, 0, 0);
    size_t target_col = location.col != npos ? location.col : (last_col_highlight > first_col_highlight ? first_col_highlight : npos);
    size_t first_col_to_show = 0;
    if(target_col != npos && target_col > maxlen)
        first_col_to_show = target_col - maxlen + 1;
    auto print_line_maybe_truncated = [&](csubstr contents){
        if(contents.len <= maxlen)
        {
            if(first_col_to_show == 0)
            {
                pr(contents);
            }
            else if(first_col_to_show < contents.len)
            {
                csubstr show = contents.sub(first_col_to_show);
                pr("[...]");
                pr(show);
                if(maxlen > show.len)
                    prn(" ", maxlen - show.len + 5);
                pr(" (showing columns ");
                pr(detail::_to_chars_limited(buf, first_col_to_show));
                pr("-");
                pr(detail::_to_chars_limited(buf, contents.len));
                pr("/");
                pr(detail::_to_chars_limited(buf, contents.len));
                pr(")");
            }
            else
            {
                pr("[...]");
                prn(" ", maxlen + 5);
                pr(" (not showing, columns=");
                pr(detail::_to_chars_limited(buf, contents.len));
                pr(")");
            }
        }
        else
        {
            if(first_col_to_show == 0)
            {
                csubstr show = contents.first(maxlen);
                pr(show);
                pr("[...] (showing columns 0-");
                pr(detail::_to_chars_limited(buf, show.len));
                pr("/");
                pr(detail::_to_chars_limited(buf, contents.len));
                pr(")");
            }
            else if(first_col_to_show < contents.len && first_col_to_show + maxlen <= contents.len)
            {
                csubstr show = contents.sub(first_col_to_show, maxlen);
                pr("[...]");
                pr(show);
                pr("[...] (showing columns ");
                pr(detail::_to_chars_limited(buf, first_col_to_show));
                pr("-");
                pr(detail::_to_chars_limited(buf, first_col_to_show + maxlen));
                pr("/");
                pr(detail::_to_chars_limited(buf, contents.len));
                pr(")");
            }
            else if(first_col_to_show < contents.len)
            {
                csubstr show = contents.sub(first_col_to_show);
                pr("[...]");
                pr(show);
                if(maxlen > show.len)
                    prn(" ", maxlen - show.len + 5);
                pr(" (showing columns ");
                pr(detail::_to_chars_limited(buf, first_col_to_show));
                pr("-");
                pr(detail::_to_chars_limited(buf, contents.len));
                pr("/");
                pr(detail::_to_chars_limited(buf, contents.len));
                pr(")");
            }
            else
            {
                pr("[...]");
                prn(" ", maxlen + 5);
                pr(" (not showing, columns=");
                pr(detail::_to_chars_limited(buf, contents.len));
                pr(")");
            }
        }
    };
    // print the location, and compute how many cols it took
    size_t locsize = location_format(pr, location);
    // print line
    if(locsize)
    {
        pr(" ");
        //++locsize;
    }
    auto print_call = [&](csubstr after){
        pr(call);
        pr(":");
        if(after.len)
            pr(after);
    };
    size_t jump;
    if(call.empty())
    {
        print_line_maybe_truncated(line);
        pr("\n");
        jump = locsize + location.col - first_col_to_show;
    }
    else
    {
        print_call("\n");
        print_call("\n");
        print_call(" ");
        pr("    ");
        print_line_maybe_truncated(line);
        pr("\n");
        jump = call.len + 2;
    }
    // when skipping to the first col, add 5 to adjust for the [...]
    // leading the line as shown
    const size_t first_col_jump = first_col_to_show == 0 ? 0 : 5;
    // print a cursor pointing at the column on the previous printed line
    auto print_cursor = [&](size_t nocall_jump){
        if(location.offset == npos)
            return;
        if(call.empty())
        {
            if(nocall_jump != npos)
            {
                prn(" ", nocall_jump + first_col_jump);
                pr("|\n");
                prn(" ", nocall_jump + first_col_jump);
                pr("(here)\n");
            }
        }
        else if(location.col != npos)
        {
            print_call(" ");
            pr("    ");
            prn(" ", location.col - first_col_to_show + first_col_jump);
            pr("|\n");
            print_call(" ");
            pr("    ");
            prn(" ", location.col - first_col_to_show + first_col_jump);
            pr("(here)\n");
        }
    };
    // maybe highlighted zone
    size_t firstcol = first_col_highlight < line.len ? first_col_highlight : line.len;
    size_t lastcol = last_col_highlight < line.len ? last_col_highlight : line.len;
    firstcol = firstcol < maxlen ? firstcol : maxlen;
    lastcol = lastcol < maxlen ? lastcol : maxlen;
    if(firstcol < lastcol)
    {
        if(!call.empty())
        {
            print_call(" ");
            pr("    ");
        }
        else
        {
            for(size_t i = 0; i < locsize + firstcol; ++i)
                pr(" ");
        }
        for(size_t i = locsize + firstcol; i < locsize + lastcol; ++i)
            pr("~");
        pr("  (cols ");
        pr(detail::_to_chars_limited(buf, firstcol));
        pr("-");
        pr(detail::_to_chars_limited(buf, lastcol));
        pr("/");
        pr(detail::_to_chars_limited(buf, line.len));
        pr(")\n");
    }
    if(location.col != npos)
    {
        print_cursor(jump);
    }
    // maybe print the region
    if(num_lines_before || num_lines_after)
    {
        if(!call.empty())
        {
            print_call("\n");
            print_call(" ");
            pr("see region:\n");
            print_call("\n");
        }
        else
        {
            if(location)
            {
                location_format(pr, location);
                pr(" ");
            }
            pr("see region:\n");
        }
        csubstr region = detail::_get_text_region(source_buffer, location.offset, num_lines_before, num_lines_after);
        for(csubstr contents : region.split('\n'))
        {
            if(!call.empty())
            {
                print_call("     ");
            }
            print_line_maybe_truncated(contents);
            pr("\n");
        }
        assert(location.col == npos || location.col >= first_col_to_show);
        print_cursor(location.col - first_col_to_show);
    }
}


template<class DumpFn>
C4_NO_INLINE void err_basic_format(DumpFn &&dumpfn, csubstr msg, ErrorDataBasic const& errdata)
{
    if(errdata.location)
    {
        location_format(dumpfn, errdata.location);
        std::forward<DumpFn>(dumpfn)(" ");
    }
    std::forward<DumpFn>(dumpfn)("ERROR: [basic] ");
    std::forward<DumpFn>(dumpfn)(msg);
}


template<class DumpFn>
C4_NO_INLINE void err_parse_format(DumpFn &&dumpfn, csubstr msg, ErrorDataParse const& errdata)
{
    if(errdata.ymlloc)
    {
        location_format(std::forward<DumpFn>(dumpfn), errdata.ymlloc);
        std::forward<DumpFn>(dumpfn)(" ");
    }
    std::forward<DumpFn>(dumpfn)("ERROR: [parse] ");
    std::forward<DumpFn>(dumpfn)(msg);
    if(errdata.cpploc)
    {
        std::forward<DumpFn>(dumpfn)("\n");
        location_format(std::forward<DumpFn>(dumpfn), errdata.cpploc);
        std::forward<DumpFn>(dumpfn)(" (detected here)");
    }
}


template<class DumpFn>
C4_NO_INLINE void err_visit_format(DumpFn &&dumpfn, csubstr msg, ErrorDataVisit const& errdata)
{
    char buf_[32];
    substr buf(buf_);
    if(errdata.cpploc)
    {
        location_format(dumpfn, errdata.cpploc);
        std::forward<DumpFn>(dumpfn)(" ");
    }
    std::forward<DumpFn>(dumpfn)("ERROR: [visit] ");
    std::forward<DumpFn>(dumpfn)(msg);
    if(errdata.node != NONE && errdata.tree != nullptr)
    {
        if(errdata.cpploc)
        {
            std::forward<DumpFn>(dumpfn)("\n");
            location_format(dumpfn, errdata.cpploc);
            std::forward<DumpFn>(dumpfn)(" ");
        }
        std::forward<DumpFn>(dumpfn)("ERROR: (");
        if(errdata.node != NONE)
        {
            std::forward<DumpFn>(dumpfn)("node=");
            std::forward<DumpFn>(dumpfn)(detail::_to_chars_limited(buf, errdata.node));
            if(errdata.tree != nullptr)
                std::forward<DumpFn>(dumpfn)(" ");
        }
        if(errdata.tree != nullptr)
        {
            std::forward<DumpFn>(dumpfn)("tree=");
            std::forward<DumpFn>(dumpfn)(detail::_to_chars_limited(buf, static_cast<void const*>(errdata.tree)));
        }
        std::forward<DumpFn>(dumpfn)(")");
    }
}

} // namespace yml
} // namespace c4

// NOLINTEND(bugprone-use-after-move,hicpp-invalid-access-moved)

#endif /* C4_YML_ERROR_HPP_ */
