#ifndef C4_YML_PARSER_STATE_HPP_
#define C4_YML_PARSER_STATE_HPP_

#ifndef C4_YML_ERROR_HPP_
#include "c4/yml/error.hpp"
#endif

// NOLINTBEGIN(hicpp-signed-bitwise)

namespace c4 {
namespace yml {

/** data type for @ref ParserState_e */
using ParserFlag_t = int;

/** Enumeration of the state flags for the parser */
typedef enum : ParserFlag_t { // NOLINT
    RTOP = 0x01 <<  0,   ///< reading at top level
    RUNK = 0x01 <<  1,   ///< reading unknown state (when starting): must determine whether scalar, map or seq
    RMAP = 0x01 <<  2,   ///< reading a map
    RSEQ = 0x01 <<  3,   ///< reading a seq
    RFLOW = 0x01 <<  4,  ///< reading is inside explicit flow chars: [] or {}
    RBLCK = 0x01 <<  5,  ///< reading in block mode
    QMRK = 0x01 <<  6,   ///< reading an explicit key (`? key`)
    RKEY = 0x01 <<  7,   ///< reading a key
    RVAL = 0x01 <<  9,   ///< reading a val
    RKCL = 0x01 <<  8,   ///< reading the key colon (ie the : after the key in the map)
    RNXT = 0x01 << 10,   ///< read next sibling
    SSCL = 0x01 << 11,   ///< there's a stored scalar
    QSCL = 0x01 << 12,   ///< stored scalar was quoted
    RSET = 0x01 << 13,   ///< the (implicit) map being read is a !!set. @see https://yaml.org/type/set.html
    RDOC = 0x01 << 14,   ///< reading a document
    NDOC = 0x01 << 15,   ///< no document mode. a document has ended and another has not started yet.
    USTY = 0x01 << 16,   ///< reading in unknown style mode - must determine FLOW or BLCK
    //! reading an implicit map nested in an explicit seq.
    //! eg,          {key: [key2: value2, key3: value3]}
    //! is parsed as {key: [{key2: value2}, {key3: value3}]}
    RSEQIMAP = 0x01 << 17,
} ParserState_e;


/** @cond dev */
#ifdef RYML_DBG
namespace detail {
RYML_EXPORT csubstr _parser_flags_to_str(substr buf, ParserFlag_t flags);
} // namespace detail
#endif
/** @endcond */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** Helper to control the line contents while parsing a buffer */
struct LineContents
{
    substr  rem;         ///< current line remainder, without newline characters
    substr  full;        ///< full line, including newline characters `\n` and `\r`
    size_t  num_cols;    ///< number of columns in the line, excluding newline
                         ///< characters (ie the initial size of rem)
    size_t  indentation; ///< number of spaces on the beginning of the line.

    LineContents() RYML_NOEXCEPT = default;

    void reset_with_next_line(substr buf, size_t start) RYML_NOEXCEPT
    {
        RYML_ASSERT_BASIC_(start <= buf.len);
        size_t end = start;
        // get the current line stripped of newline chars
        while((end < buf.len) && (buf.str[end] != '\n'))
            ++end;
        if(end < buf.len)
        {
            RYML_ASSERT_BASIC_(buf[end] == '\n');
            full = buf.range(start, end + 1);
            rem = buf.range(start, end);
        }
        else
        {
            // buffer ends without newline
            full = rem = buf.sub(start);
        }
        size_t pos = rem.last_not_of('\r');
        rem.len = (pos != npos) ? pos + 1 : 0;
        num_cols = rem.len;
        RYML_ASSERT_BASIC_(rem.find('\r') == npos);
        // TODO move this to the parser state
        indentation = rem.first_not_of(' ');  // find the first column where the character is not a space
    }

    C4_ALWAYS_INLINE size_t current_col() const RYML_NOEXCEPT
    {
        RYML_ASSERT_BASIC_(rem.str >= full.str);
        return static_cast<size_t>(rem.str - full.str);
    }

    C4_ALWAYS_INLINE size_t current_col(csubstr s) const RYML_NOEXCEPT
    {
        RYML_ASSERT_BASIC_(s.str >= full.str);
        RYML_ASSERT_BASIC_(s.str <= rem.end());
        return static_cast<size_t>(s.str - full.str);
    }
};
static_assert(std::is_standard_layout<LineContents>::value, "LineContents not standard");


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

struct ParserState
{
    LineContents line_contents;
    Location     pos;
    ParserFlag_t flags;
    size_t       indref;  ///< the reference indentation in the current block scope
    id_type      level;
    id_type      node_id; ///< don't hold a pointer to the node as it will be relocated during tree resizes
    size_t       scalar_col; // the column where the scalar (or its quotes) begin
    bool         more_indented;
    bool         has_children;

    ParserState() = default;

    void start_parse(const char *file, id_type node_id_)
    {
        level = 0;
        pos.name = to_csubstr(file);
        pos.offset = 0;
        pos.line = 1;
        pos.col = 1;
        node_id = node_id_;
        more_indented = false;
        scalar_col = 0;
        indref = 0;
        has_children = false;
    }

    void reset_after_push()
    {
        node_id = NONE;
        indref = npos;
        more_indented = false;
        ++level;
        has_children = false;
    }

    C4_ALWAYS_INLINE void reset_before_pop(ParserState const& to_pop)
    {
        pos = to_pop.pos;
        line_contents = to_pop.line_contents;
    }

public:

    C4_ALWAYS_INLINE bool at_line_beginning() const noexcept
    {
        return line_contents.rem.str == line_contents.full.str;
    }
    C4_ALWAYS_INLINE bool at_first_token() const noexcept
    {
        RYML_ASSERT_BASIC_(line_contents.indentation != npos);
        return pos.col == line_contents.indentation + 1;
    }
    C4_ALWAYS_INLINE bool indentation_eq() const noexcept
    {
        RYML_ASSERT_BASIC_(indref != npos);
        return line_contents.indentation != npos
            && line_contents.indentation == indref;
    }
    C4_ALWAYS_INLINE bool indentation_eq_extra() const noexcept
    {
        RYML_ASSERT_BASIC_(indref != npos);
        return line_contents.indentation != npos
            && line_contents.indentation == indref + 1u;
    }
    C4_ALWAYS_INLINE bool indentation_ge() const noexcept
    {
        RYML_ASSERT_BASIC_(indref != npos);
        return line_contents.indentation != npos
            && line_contents.indentation >= indref;
    }
    C4_ALWAYS_INLINE bool indentation_ge_extra() const noexcept
    {
        RYML_ASSERT_BASIC_(indref != npos);
        return line_contents.indentation != npos
            && line_contents.indentation >= indref + 1u;
    }
    C4_ALWAYS_INLINE bool indentation_gt() const noexcept
    {
        RYML_ASSERT_BASIC_(indref != npos);
        return line_contents.indentation != npos
            && line_contents.indentation > indref;
    }
    C4_ALWAYS_INLINE bool indentation_gt_extra() const noexcept
    {
        RYML_ASSERT_BASIC_(indref != npos);
        return line_contents.indentation != npos
            && line_contents.indentation > indref + 1u;
    }
    C4_ALWAYS_INLINE bool indentation_lt() const noexcept
    {
        RYML_ASSERT_BASIC_(indref != npos);
        return line_contents.indentation != npos && line_contents.indentation < indref;
    }
    C4_ALWAYS_INLINE bool indentation_lt_extra() const noexcept
    {
        RYML_ASSERT_BASIC_(indref != npos);
        return line_contents.indentation != npos
            && line_contents.indentation < indref + 1u;
    }
};
static_assert(std::is_standard_layout<ParserState>::value, "ParserState not standard");


} // namespace yml
} // namespace c4

// NOLINTEND(hicpp-signed-bitwise)

#endif /* C4_YML_PARSER_STATE_HPP_ */
