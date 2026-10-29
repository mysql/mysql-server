/* Copyright (c) 2026, Amazon.com, Inc. or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Unit tests for my_safe_puts_stderr(), which dumps a (possibly untrusted)
  string to STDERR from a signal handler, in one of two formats selected by
  my_safe_puts_tagged_lines:

  - single line (default): the label, ": ", the string and a newline;
  - tagged lines: a sequence of self-describing lines, each made of the
    label, " +", the byte offset of the chunk, ": ", the chunk and a
    newline.

  The tests redirect STDERR into a temporary file, call the function, and
  check or parse the captured output.
*/

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "my_config.h"
#include "my_inttypes.h"
#include "my_stacktrace.h"

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef __linux__
#include <sys/mman.h>
#endif

#if defined(HAVE_STACKTRACE) && !defined(_WIN32)

namespace mysys_safe_puts_stderr_unittest {

/* Must match MY_SAFE_PUTS_CHUNK in mysys/stacktrace.cc. */
constexpr size_t kChunk = 132;
/* Size of the line buffer used by mysys/stacktrace.cc. */
constexpr size_t kMaxLine = 256;

/* One parsed output line. */
struct Line {
  size_t offset;
  std::string payload;
};

/*
  True if s is well-formed utf8mb4, by the rules of my_charset_utf8mb4
  (strings/mb_wc.h): no stray continuation bytes, no 0xc0, 0xc1 or
  0xf5-0xff, no overlong forms, no surrogates (U+D800-U+DFFF), nothing
  above U+10FFFF and no sequence cut by the end of s.
*/
static bool is_valid_utf8(const std::string &s) {
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    size_t need;
    unsigned char lo = 0x80, hi = 0xbf; /* Range of the second byte. */
    if (c < 0x80)
      need = 1;
    else if (c >= 0xc2 && c <= 0xdf)
      need = 2;
    else if (c >= 0xe0 && c <= 0xef) {
      need = 3;
      if (c == 0xe0) lo = 0xa0;
      if (c == 0xed) hi = 0x9f;
    } else if (c >= 0xf0 && c <= 0xf4) {
      need = 4;
      if (c == 0xf0) lo = 0x90;
      if (c == 0xf4) hi = 0x8f;
    } else
      return false;
    if (i + need > s.size()) return false;
    for (size_t k = 1; k < need; ++k) {
      const unsigned char n = static_cast<unsigned char>(s[i + k]);
      if (k == 1 ? (n < lo || n > hi) : (n & 0xc0) != 0x80) return false;
    }
    i += need;
  }
  return true;
}

/*
  Inputs with bytes that are not part of a well-formed utf8mb4 sequence,
  and the expected output: each such byte replaced by one ' ', everything
  else kept. Used by both formats.
*/
struct Utf8Case {
  std::string in;
  std::string out;
};
static const std::vector<Utf8Case> &utf8_cases() {
  static const std::vector<Utf8Case> cases = {
      /* A lone Latin-1 byte, e.g. from a latin1 client. */
      {"\xe9MARK", " MARK"},
      {"caf\xe9 MARK", "caf  MARK"},
      /* Lone continuation byte. */
      {"a\x80z", "a z"},
      {"a\xbfz", "a z"},
      /* Overlong forms. */
      {std::string("\xc0\x80", 2), "  "},
      {"\xc1\xbf", "  "},
      {"\xe0\x80\x80", "   "},
      {"\xe0\x9f\xbf", "   "},
      {"\xf0\x80\x80\x80", "    "},
      {"\xf0\x8f\xbf\xbf", "    "},
      /* Surrogates. */
      {"\xed\xa0\x80", "   "},
      {"\xed\xbf\xbf", "   "},
      /* Above U+10FFFF, and bytes that never occur in UTF-8. */
      {"\xf4\x90\x80\x80", "    "},
      {"\xf5", " "},
      {"x\xf5\x80\x80\x80y", "x    y"},
      {"\xf8\xfc\xfe\xff", "    "},
      /* A lead byte followed by too few continuation bytes. */
      {"\xc3"
       "A",
       " A"},
      {"\xe2\x82"
       "A",
       "  A"},
      {"\xf0\x9f\x98"
       "A",
       "   A"},
      /* ... or by another character. */
      {"\xe2\xc3\xa9", " \xc3\xa9"},
      /* A sequence cut by the end of the string. */
      {"ab\xc3", "ab "},
      {"ab\xe2\x82", "ab  "},
      {"ab\xf0\x9f\x98", "ab   "},
      /* Well-formed characters at the edges of the ranges are kept. */
      {"\xc2\x80\xdf\xbf", "\xc2\x80\xdf\xbf"},
      {"\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80\xef\xbf\xbf",
       "\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80\xef\xbf\xbf"},
      {"\xf0\x90\x80\x80\xf3\xbf\xbf\xbf\xf4\x8f\xbf\xbf",
       "\xf0\x90\x80\x80\xf3\xbf\xbf\xbf\xf4\x8f\xbf\xbf"},
      {"SELECT '\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80'",
       "SELECT '\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80'"},
  };
  return cases;
}

/* Captures what my_safe_puts_stderr() writes, in the given format. */
class SafePutsStderrBase : public ::testing::Test {
 protected:
  explicit SafePutsStderrBase(bool tagged) : m_tagged(tagged) {}
  void SetUp() override { my_safe_puts_tagged_lines = m_tagged; }
  void TearDown() override { my_safe_puts_tagged_lines = false; }

  /*
    Redirect STDERR into a temporary file, call my_safe_puts_stderr(),
    restore STDERR and return everything the function wrote. A file rather
    than a pipe: the output is read only after the call returns, and a pipe
    smaller than the output would block the writer forever. May be called
    several times per test.
  */
  std::string capture(const char *label, const char *val, size_t max_len) {
    std::string out;
    fflush(stderr);
    FILE *tmp = tmpfile();
    EXPECT_NE(nullptr, tmp);
    if (tmp == nullptr) return out;
    const int fd = fileno(tmp);
    const int saved_stderr = dup(STDERR_FILENO);
    EXPECT_GE(saved_stderr, 0);
    EXPECT_GE(dup2(fd, STDERR_FILENO), 0);

    my_safe_puts_stderr(label, val, max_len);

    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);

    EXPECT_EQ(0, lseek(fd, 0, SEEK_SET));
    char buf[1024];
    for (;;) {
      const ssize_t n = read(fd, buf, sizeof(buf));
      if (n <= 0) break;
      out.append(buf, static_cast<size_t>(n));
    }
    fclose(tmp);
    return out;
  }

 private:
  const bool m_tagged;
};

/* Tagged-lines format. */
class SafePutsStderrTest : public SafePutsStderrBase {
 protected:
  SafePutsStderrTest() : SafePutsStderrBase(true) {}

  /*
    Split the raw output into lines and check the invariants every line has
    to satisfy: ends with '\n', is at most kMaxLine bytes, starts with the
    label followed by " +", and contains no control characters.
  */
  static std::vector<Line> parse(const std::string &out, const char *label) {
    std::vector<Line> lines;
    const std::string prefix = std::string(label) + " +";
    EXPECT_FALSE(out.empty());
    EXPECT_EQ('\n', out.back()) << "Output does not end with a newline";

    size_t start = 0;
    while (start < out.size()) {
      const size_t nl = out.find('\n', start);
      EXPECT_NE(std::string::npos, nl) << "Unterminated line";
      if (nl == std::string::npos) break;
      const std::string line = out.substr(start, nl - start + 1);
      start = nl + 1;

      EXPECT_LE(line.size(), kMaxLine);
      EXPECT_EQ(0, line.compare(0, prefix.size(), prefix)) << line;
      for (size_t i = 0; i + 1 < line.size(); ++i)
        EXPECT_GE(static_cast<unsigned char>(line[i]), 0x20) << line;

      Line parsed;
      const size_t colon = line.find(": ", prefix.size());
      EXPECT_NE(std::string::npos, colon) << line;
      if (colon == std::string::npos) break;
      parsed.offset =
          std::stoul(line.substr(prefix.size(), colon - prefix.size()));
      parsed.payload = line.substr(colon + 2, line.size() - colon - 3);
      /* Every line is imported on its own as utf8mb4. */
      EXPECT_TRUE(is_valid_utf8(line)) << line;
      lines.push_back(parsed);
    }
    return lines;
  }

  /* Convenience: capture + parse. */
  std::vector<Line> dump(const char *label, const char *val, size_t max_len) {
    return parse(capture(label, val, max_len), label);
  }
};

/* A printable string of the given length, with a recognisable pattern. */
static std::string printable(size_t len) {
  std::string s;
  for (size_t i = 0; i < len; ++i) s.push_back('A' + static_cast<char>(i % 26));
  return s;
}

static std::string join(const std::vector<Line> &lines) {
  std::string s;
  for (const Line &l : lines) s += l.payload;
  return s;
}

/* 1. Long string is split into chunks of kChunk bytes. */
TEST_F(SafePutsStderrTest, ThreeChunks) {
  const std::string in = printable(299);
  const std::vector<Line> lines = dump("Query", in.c_str(), in.size());
  ASSERT_EQ(3U, lines.size());
  EXPECT_EQ(0U, lines[0].offset);
  EXPECT_EQ(kChunk, lines[1].offset);
  EXPECT_EQ(2 * kChunk, lines[2].offset);
  EXPECT_EQ(in.substr(0, kChunk), lines[0].payload);
  EXPECT_EQ(in.substr(kChunk, kChunk), lines[1].payload);
  EXPECT_EQ(in.substr(2 * kChunk), lines[2].payload);
  EXPECT_EQ(in, join(lines));
}

/* 2a. Exactly one chunk gives exactly one line. */
TEST_F(SafePutsStderrTest, ExactlyOneChunk) {
  const std::string in = printable(kChunk);
  const std::vector<Line> lines = dump("Query", in.c_str(), in.size());
  ASSERT_EQ(1U, lines.size());
  EXPECT_EQ(0U, lines[0].offset);
  EXPECT_EQ(in, lines[0].payload);
}

/* 2b. One byte more than a chunk gives a second line with one byte. */
TEST_F(SafePutsStderrTest, OneChunkPlusOne) {
  const std::string in = printable(kChunk + 1);
  const std::vector<Line> lines = dump("Query", in.c_str(), in.size());
  ASSERT_EQ(2U, lines.size());
  EXPECT_EQ(kChunk, lines[1].offset);
  EXPECT_EQ(1U, lines[1].payload.size());
  EXPECT_EQ(in.substr(kChunk), lines[1].payload);
  EXPECT_EQ(in, join(lines));
}

/* 3. A zero-length string still yields exactly one (empty) line. */
TEST_F(SafePutsStderrTest, ZeroLength) {
  const char *in = "";
  const std::string out = capture("Label", in, 0);
  EXPECT_EQ("Label +0: \n", out);
}

/* 4. max_len smaller than the string truncates the dump. */
TEST_F(SafePutsStderrTest, MaxLenTruncates) {
  const std::string in = printable(400);
  const std::vector<Line> lines = dump("Query", in.c_str(), 150);
  ASSERT_EQ(2U, lines.size());
  EXPECT_EQ(0U, lines[0].offset);
  EXPECT_EQ(kChunk, lines[1].offset);
  EXPECT_EQ(150U - kChunk, lines[1].payload.size());
  EXPECT_EQ(in.substr(0, 150), join(lines));

  const std::vector<Line> short_lines = dump("Query", in.c_str(), 7);
  ASSERT_EQ(1U, short_lines.size());
  EXPECT_EQ(in.substr(0, 7), short_lines[0].payload);
}

/* 5. Control bytes are replaced by ' ' (parse() also checks that no line
      contains a control character). */
TEST_F(SafePutsStderrTest, NonPrintableReplaced) {
  const std::string in("ab\ncd\tef\x01gh\x7fij", 14);
  const std::vector<Line> lines = dump("Query", in.c_str(), in.size());
  ASSERT_EQ(1U, lines.size());
  EXPECT_EQ("ab cd ef gh ij", lines[0].payload);

  /* Only 0x00-0x1f and 0x7f are replaced among the ASCII bytes: 0x20 and
     0x7e are kept. 0x80 and 0xff are not part of a well-formed UTF-8
     sequence and are replaced as well. */
  const std::string edges("\x1f\x20\x7e\x7f\x80\xff", 6);
  const std::vector<Line> edge_lines = dump("Query", edges.c_str(), 6);
  ASSERT_EQ(1U, edge_lines.size());
  EXPECT_EQ("  ~   ", edge_lines[0].payload);
}

/*
  True if s consists of complete UTF-8 sequences only: every lead byte is
  followed by as many continuation bytes as it announces, and there are no
  stray continuation bytes. Overlong forms and surrogates are not checked,
  the tests only use well-formed characters.
*/
static bool complete_utf8(const std::string &s) {
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    size_t need;
    if (c < 0x80)
      need = 1;
    else if ((c & 0xe0) == 0xc0)
      need = 2;
    else if ((c & 0xf0) == 0xe0)
      need = 3;
    else if ((c & 0xf8) == 0xf0)
      need = 4;
    else
      return false;
    if (i + need > s.size()) return false;
    for (size_t k = 1; k < need; ++k)
      if ((static_cast<unsigned char>(s[i + k]) & 0xc0) != 0x80) return false;
    i += need;
  }
  return true;
}

/*
  Check the lines of a tagged dump of in: each line has at most kChunk
  bytes of payload, starts at the offset where the previous one ended, is
  valid UTF-8 on its own, and the payloads joined give in.
*/
static void expect_whole_utf8_lines(const std::vector<Line> &lines,
                                    const std::string &in) {
  size_t expected_offset = 0;
  for (const Line &l : lines) {
    EXPECT_EQ(expected_offset, l.offset);
    EXPECT_LE(l.payload.size(), kChunk);
    EXPECT_FALSE(l.payload.empty());
    EXPECT_TRUE(complete_utf8(l.payload)) << "Line at +" << l.offset;
    EXPECT_TRUE(is_valid_utf8(l.payload)) << "Line at +" << l.offset;
    expected_offset = l.offset + l.payload.size();
  }
  EXPECT_EQ(in, join(lines));
}

/* 5b. UTF-8 text is kept, and a multi-byte character is never split
       between two lines: a line ends before a character that does not fit,
       and the next line starts with it, at its exact byte offset. */
TEST_F(SafePutsStderrTest, Utf8Kept) {
  const std::string in = "SELECT '\xc3\xa9'";
  const std::vector<Line> lines = dump("Query", in.c_str(), in.size());
  ASSERT_EQ(1U, lines.size());
  EXPECT_EQ(in, lines[0].payload);

  /* U+00E9 at offsets kChunk - 1 and kChunk. */
  std::string split = printable(kChunk - 1) + "\xc3\xa9" + printable(5);
  const std::vector<Line> split_lines =
      dump("Query", split.c_str(), split.size());
  ASSERT_EQ(2U, split_lines.size());
  EXPECT_EQ(printable(kChunk - 1), split_lines[0].payload);
  EXPECT_EQ(kChunk - 1, split_lines[1].offset);
  EXPECT_EQ("\xc3\xa9" + printable(5), split_lines[1].payload);
  expect_whole_utf8_lines(split_lines, split);
}

/* 5c. 2-, 3- and 4-byte characters at every position where they would
       straddle the end of the first line, and where they end exactly at
       it (no shortening then). */
TEST_F(SafePutsStderrTest, Utf8StraddlesChunk) {
  const char *chars[] = {"\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x98\x80"};
  for (const char *c : chars) {
    const std::string ch(c);
    /* lead is the offset of the lead byte; the last value of lead puts
       the whole character at the end of the first chunk. */
    for (size_t lead = kChunk - ch.size(); lead < kChunk; ++lead) {
      SCOPED_TRACE("char of " + std::to_string(ch.size()) +
                   " bytes, lead byte at " + std::to_string(lead));
      const std::string in = printable(lead) + ch + printable(200);
      const std::vector<Line> lines = dump("Query", in.c_str(), in.size());
      ASSERT_EQ(3U, lines.size());
      if (lead + ch.size() == kChunk) {
        EXPECT_EQ(kChunk, lines[1].offset);
        EXPECT_EQ(ch, lines[0].payload.substr(lead));
      } else {
        EXPECT_EQ(lead, lines[1].offset);
        EXPECT_EQ(printable(lead), lines[0].payload);
        EXPECT_EQ(ch, lines[1].payload.substr(0, ch.size()));
      }
      expect_whole_utf8_lines(lines, in);
    }
  }
}

/* 5d. Characters straddling the single-line read size, and a long text of
       mixed multi-byte characters: all lines stay whole and contiguous. */
TEST_F(SafePutsStderrTest, Utf8LongerThanRun) {
  const std::string in = printable(4095) + "\xc3\xa9" + printable(300);
  expect_whole_utf8_lines(dump("Query", in.c_str(), in.size()), in);

  const std::string in4 = printable(4094) + "\xf0\x9f\x98\x80" + printable(9);
  expect_whole_utf8_lines(dump("Query", in4.c_str(), in4.size()), in4);

  std::string mixed;
  while (mixed.size() < 10000) mixed += "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80";
  const std::vector<Line> lines = dump("Query", mixed.c_str(), mixed.size());
  EXPECT_GT(lines.size(), mixed.size() / kChunk);
  expect_whole_utf8_lines(lines, mixed);
}

/* 5e. max_len cutting a character: the truncated end is not a well-formed
       sequence and is printed as one ' ' per byte, also after a line that
       was ended early. */
TEST_F(SafePutsStderrTest, Utf8TruncatedByMaxLen) {
  const std::string in = printable(kChunk - 1) + "\xc3\xa9" + printable(200);
  const std::vector<Line> lines = dump("Query", in.c_str(), kChunk);
  ASSERT_EQ(1U, lines.size());
  EXPECT_EQ(printable(kChunk - 1) + " ", lines[0].payload);

  /* The first line ends early at kChunk - 1, the second line holds U+00E9,
     kChunk - 2 letters and the lead byte of U+20AC cut by max_len. */
  const std::string in2 = printable(kChunk - 1) + "\xc3\xa9" +
                          printable(kChunk - 2) + "\xe2\x82\xac" +
                          printable(10);
  const size_t max_len2 = 2 * kChunk;
  const std::vector<Line> lines2 = dump("Query", in2.c_str(), max_len2);
  ASSERT_EQ(3U, lines2.size());
  EXPECT_EQ(kChunk - 1, lines2[1].offset);
  EXPECT_EQ(kChunk, lines2[1].payload.size());
  EXPECT_EQ(2 * kChunk - 1, lines2[2].offset);
  EXPECT_EQ(" ", lines2[2].payload);
  EXPECT_EQ(in2.substr(0, max_len2 - 1) + " ", join(lines2));

  /* Cut after two of the four bytes of U+1F600. */
  const std::string in3 = "ab\xf0\x9f\x98\x80";
  const std::vector<Line> lines3 = dump("Query", in3.c_str(), 4);
  ASSERT_EQ(1U, lines3.size());
  EXPECT_EQ("ab  ", lines3[0].payload);
}

/* 5f. Bytes that are not part of a well-formed UTF-8 sequence become one
       ' ' each, so offsets and lengths are unchanged; a line is ended early
       only before a lead byte, and never made empty. */
TEST_F(SafePutsStderrTest, InvalidBytesBecomeSpaces) {
  /* A lone continuation byte at the end of a chunk: no shortening. */
  const std::string cont = printable(kChunk - 1) + "\x80" + printable(10);
  const std::vector<Line> cont_lines = dump("Query", cont.c_str(), cont.size());
  ASSERT_EQ(2U, cont_lines.size());
  EXPECT_EQ(kChunk, cont_lines[1].offset);
  EXPECT_EQ(printable(kChunk - 1) + " " + printable(10), join(cont_lines));

  /* Only continuation bytes: full chunks of spaces, nothing shortened or
     dropped. */
  const std::string conts(300, '\x80');
  const std::vector<Line> conts_lines =
      dump("Query", conts.c_str(), conts.size());
  ASSERT_EQ(3U, conts_lines.size());
  EXPECT_EQ(kChunk, conts_lines[1].offset);
  EXPECT_EQ(2 * kChunk, conts_lines[2].offset);
  EXPECT_EQ(std::string(300, ' '), join(conts_lines));

  /* A lead byte followed by too many continuation bytes: the lead byte is
     too far back to matter, the chunk is not shortened. The first two
     bytes form U+00C0, the other continuation bytes are spaces. */
  const std::string longrun = "\xc3" + std::string(200, '\x80');
  const std::vector<Line> longrun_lines =
      dump("Query", longrun.c_str(), longrun.size());
  ASSERT_EQ(2U, longrun_lines.size());
  EXPECT_EQ(kChunk, longrun_lines[1].offset);
  EXPECT_EQ("\xc3\x80" + std::string(199, ' '), join(longrun_lines));

  /* A lone lead byte at the end of a chunk, not followed by a continuation
     byte: it starts the next line, where it becomes a space. */
  const std::string lone = printable(kChunk - 1) + "\xc3" + printable(10);
  const std::vector<Line> lone_lines = dump("Query", lone.c_str(), lone.size());
  ASSERT_EQ(2U, lone_lines.size());
  EXPECT_EQ(kChunk - 1, lone_lines[1].offset);
  EXPECT_EQ(" " + printable(10), lone_lines[1].payload);
  EXPECT_EQ(printable(kChunk - 1) + " " + printable(10), join(lone_lines));

  /* A lone Latin-1 byte followed by an ASCII marker. */
  const std::string latin1 = "SELECT 'caf\xe9' /* MARKER */";
  const std::vector<Line> latin1_lines =
      dump("Query", latin1.c_str(), latin1.size());
  ASSERT_EQ(1U, latin1_lines.size());
  EXPECT_EQ("SELECT 'caf ' /* MARKER */", latin1_lines[0].payload);

  /* 0xe9 where it would straddle the end of the first line: it is moved to
     the next line like a lead byte, and becomes a space there. */
  const std::string latin1b = printable(kChunk - 1) + "\xe9MARKER";
  const std::vector<Line> latin1b_lines =
      dump("Query", latin1b.c_str(), latin1b.size());
  ASSERT_EQ(2U, latin1b_lines.size());
  EXPECT_EQ(kChunk - 1, latin1b_lines[1].offset);
  EXPECT_EQ(" MARKER", latin1b_lines[1].payload);
}

/* 5g. Every kind of byte that is not part of a well-formed sequence. */
TEST_F(SafePutsStderrTest, InvalidSequences) {
  const std::vector<Utf8Case> &cases = utf8_cases();
  for (size_t i = 0; i < cases.size(); ++i) {
    const Utf8Case &c = cases[i];
    SCOPED_TRACE("case " + std::to_string(i));
    const std::vector<Line> lines = dump("Query", c.in.data(), c.in.size());
    ASSERT_EQ(1U, lines.size());
    EXPECT_EQ(c.out, lines[0].payload);
  }
}

/* 5h. 0xc0, 0xc1 and 0xf5-0xff are not lead bytes: at the end of a chunk
       they do not end the line early. A lead byte that can only start an
       invalid sequence still does, which only moves the line break. */
TEST_F(SafePutsStderrTest, NotLeadBytesDoNotShorten) {
  for (const char *b : {"\xc0", "\xc1", "\xf5", "\xf8", "\xff"}) {
    SCOPED_TRACE(std::string("byte ") +
                 std::to_string(static_cast<unsigned char>(*b)));
    const std::string in = printable(kChunk - 1) + b + "\x80" + printable(10);
    const std::vector<Line> lines = dump("Query", in.c_str(), in.size());
    ASSERT_EQ(2U, lines.size());
    EXPECT_EQ(kChunk, lines[1].offset);
    EXPECT_EQ(printable(kChunk - 1) + "  " + printable(10), join(lines));
  }

  /* 0xe0 0x80 is not well-formed (0xe0 needs 0xa0-0xbf next), but it is a
     lead byte followed only by a continuation byte: the line still ends
     before it, and both bytes become spaces on the next line. */
  const std::string bad = printable(kChunk - 1) + "\xe0\x80" + "MARKER";
  const std::vector<Line> bad_lines = dump("Query", bad.c_str(), bad.size());
  ASSERT_EQ(2U, bad_lines.size());
  EXPECT_EQ(kChunk - 1, bad_lines[1].offset);
  EXPECT_EQ(printable(kChunk - 1) + "  MARKER", join(bad_lines));
}

/* 6. Printing stops at an embedded NUL even if max_len is larger. */
TEST_F(SafePutsStderrTest, StopsAtNul) {
  const char in[] = "prefix\0suffix";
  const std::vector<Line> lines = dump("Query", in, sizeof(in));
  ASSERT_EQ(1U, lines.size());
  EXPECT_EQ("prefix", lines[0].payload);

  /* NUL in the second chunk. */
  std::string in2 = printable(200);
  in2[150] = '\0';
  const std::vector<Line> lines2 = dump("Query", in2.c_str(), in2.size());
  ASSERT_EQ(2U, lines2.size());
  EXPECT_EQ(kChunk, lines2[1].offset);
  EXPECT_EQ(in2.substr(0, 150), join(lines2));
}

#ifdef __linux__
/* These cases rely on the Linux /proc-based ptr_sane() range check; on other
   platforms ptr_sane() trusts the pointer and the dereference would fault. */
/* Only used by the Linux-only cases below. */
/* Must match MY_SAFE_PUTS_INVALID in mysys/stacktrace.cc. */
const char kInvalid[] = "<is an invalid pointer>";
/* 7. An entirely invalid pointer yields exactly one diagnostic line. */
TEST_F(SafePutsStderrTest, InvalidPointer) {
  const char *in = reinterpret_cast<const char *>(16);
  const std::vector<Line> lines = dump("Query", in, 100);
  ASSERT_EQ(1U, lines.size());
  EXPECT_EQ(0U, lines[0].offset);
  EXPECT_EQ(kInvalid, lines[0].payload);
}

/* 8. A string running off the end of mapped memory: the readable part is
      printed, followed by the diagnostic at the offset where reading
      failed. */
TEST_F(SafePutsStderrTest, StraddlesUnmappedPage) {
  const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  ASSERT_GT(page, 0U);
  void *mem = mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, mem);
  char *first = static_cast<char *>(mem);
  ASSERT_EQ(0, munmap(first + page, page));

  constexpr size_t kTail = 20;
  char *in = first + page - kTail;
  const std::string expected = printable(kTail);
  memcpy(in, expected.data(), kTail);

  const std::vector<Line> lines = dump("Query", in, 100);
  ASSERT_EQ(2U, lines.size());
  EXPECT_EQ(0U, lines[0].offset);
  EXPECT_EQ(expected, lines[0].payload);
  EXPECT_EQ(kTail, lines[1].offset);
  EXPECT_EQ(kInvalid, lines[1].payload);

  EXPECT_EQ(0, munmap(first, page));
}
#endif /* __linux__ */

/* 9. Every line is newline-terminated and at most kMaxLine bytes long,
      even for a long label. */
TEST_F(SafePutsStderrTest, LineFormatInvariants) {
  const std::string in = printable(3 * kChunk + 5);
  const std::string label(40, 'L');
  const std::string out = capture(label.c_str(), in.c_str(), in.size());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ('\n', out.back());
  size_t start = 0, count = 0;
  while (start < out.size()) {
    const size_t nl = out.find('\n', start);
    ASSERT_NE(std::string::npos, nl);
    EXPECT_LE(nl - start + 1, kMaxLine);
    start = nl + 1;
    ++count;
  }
  EXPECT_EQ(4U, count);
  EXPECT_EQ(in, join(parse(out, label.c_str())));
}

/* 9b. A string longer than the single-line read size is still printed as
       lines of at most kChunk bytes of payload each. */
TEST_F(SafePutsStderrTest, LongerThanRun) {
  const std::string in = printable(10000);
  const std::vector<Line> lines = dump("Query", in.c_str(), in.size());
  ASSERT_EQ((in.size() + kChunk - 1) / kChunk, lines.size());
  for (size_t i = 0; i < lines.size(); ++i) {
    EXPECT_EQ(i * kChunk, lines[i].offset);
    EXPECT_LE(lines[i].payload.size(), kChunk);
  }
  EXPECT_EQ(in, join(lines));
}

/* 10. The label is used verbatim. */
TEST_F(SafePutsStderrTest, LabelVerbatim) {
  const char *in = "hello";
  EXPECT_EQ("Query (deadbeef) +0: hello\n",
            capture("Query (deadbeef)", in, strlen(in)));
}

TEST_F(SafePutsStderrTest, LabelVerbatimFoo) {
  const char *in = "world";
  EXPECT_EQ("Foo +0: world\n", capture("Foo", in, strlen(in)));
}

/* Single-line format (the default). */
class SafePutsStderrSingleLineTest : public SafePutsStderrBase {
 protected:
  SafePutsStderrSingleLineTest() : SafePutsStderrBase(false) {}

  /* Capture, and check that the output is well-formed UTF-8 as a whole. */
  std::string capture(const char *label, const char *val, size_t max_len) {
    const std::string out = SafePutsStderrBase::capture(label, val, max_len);
    EXPECT_TRUE(is_valid_utf8(out)) << out;
    return out;
  }
};

/* The default format is single line. */
TEST(SafePutsStderrDefault, SingleLineIsDefault) {
  EXPECT_FALSE(my_safe_puts_tagged_lines);
}

/* 11. The label, ": ", the string and one newline. */
TEST_F(SafePutsStderrSingleLineTest, Short) {
  const char *in = "hello";
  EXPECT_EQ("Query (deadbeef): hello\n",
            capture("Query (deadbeef)", in, strlen(in)));
}

/* 12. A string longer than a chunk is still printed as one line. */
TEST_F(SafePutsStderrSingleLineTest, LongStringOneLine) {
  const std::string in = printable(3 * kChunk + 5);
  EXPECT_EQ("Query: " + in + "\n", capture("Query", in.c_str(), in.size()));
}

/* 13. max_len truncates the string. */
TEST_F(SafePutsStderrSingleLineTest, MaxLenTruncates) {
  const std::string in = printable(400);
  EXPECT_EQ("Query: " + in.substr(0, 150) + "\n",
            capture("Query", in.c_str(), 150));
}

/* 14. A zero-length string: the Linux /proc reader prints nothing after
       the label, the direct-memory reader prints an empty string. */
TEST_F(SafePutsStderrSingleLineTest, ZeroLength) {
#ifdef __linux__
  EXPECT_EQ("Label: ", capture("Label", "", 0));
#else
  EXPECT_EQ("Label: \n", capture("Label", "", 0));
#endif
}

/* 15. Control bytes are replaced by ' ', so the output stays on one line. */
TEST_F(SafePutsStderrSingleLineTest, NonPrintableReplaced) {
  const std::string in("ab\ncd\tef\x01gh\x7fij", 14);
  EXPECT_EQ("Query: ab cd ef gh ij\n", capture("Query", in.c_str(), 14));

  /* A newline or carriage return can never end the line early. */
  const std::string nl("a\nb\rc\n", 6);
  EXPECT_EQ("Query: a b c \n", capture("Query", nl.c_str(), nl.size()));
}

/* 15b. UTF-8 identifiers and literals are printed unchanged; other bytes
        >= 0x80 that are not part of a well-formed sequence become ' '. */
TEST_F(SafePutsStderrSingleLineTest, Utf8Kept) {
  const std::string in = "SELECT '\xc3\xa9' FROM t\xc3\xa4";
  EXPECT_EQ("Query: " + in + "\n", capture("Query", in.c_str(), in.size()));

  const std::string edges("\x1f\x20\x7e\x7f\x80\xff", 6);
  EXPECT_EQ("Query:   ~   \n", capture("Query", edges.c_str(), edges.size()));
}

/* 15d. Every kind of byte that is not part of a well-formed sequence, and
        a lone Latin-1 byte followed by an ASCII marker. */
TEST_F(SafePutsStderrSingleLineTest, InvalidBytesBecomeSpaces) {
  const std::vector<Utf8Case> &cases = utf8_cases();
  for (size_t i = 0; i < cases.size(); ++i) {
    const Utf8Case &c = cases[i];
    SCOPED_TRACE("case " + std::to_string(i));
    EXPECT_EQ("Query: " + c.out + "\n",
              capture("Query", c.in.data(), c.in.size()));
  }

  const std::string latin1 = "SELECT 'caf\xe9' /* MARKER */";
  EXPECT_EQ("Query: SELECT 'caf ' /* MARKER */\n",
            capture("Query", latin1.c_str(), latin1.size()));

  const std::string conts(300, '\x80');
  EXPECT_EQ("Query: " + std::string(300, ' ') + "\n",
            capture("Query", conts.c_str(), conts.size()));

  /* Truncated by max_len. */
  const std::string cut = "ab\xe2\x82\xac";
  EXPECT_EQ("Query: ab  \n", capture("Query", cut.c_str(), 4));
}

/* 15e. Characters of 2, 3 and 4 bytes straddling the end of the first
        4096-byte run are kept whole: the run is ended before the
        character and the next one starts with it. A lone 0xe9 there still
        becomes a single ' '. */
TEST_F(SafePutsStderrSingleLineTest, Utf8StraddlesRun) {
  const std::string in4 = printable(4094) + "\xf0\x9f\x98\x80" + printable(9);
  EXPECT_EQ("Query: " + in4 + "\n", capture("Query", in4.c_str(), in4.size()));

  const char *chars[] = {"\xc3\xa9", "\xe2\x82\xac", "\xf0\x9f\x98\x80"};
  for (const char *c : chars) {
    const std::string ch(c);
    for (const size_t boundary : {kChunk, size_t{4096}, size_t{8192}}) {
      for (size_t lead = boundary - ch.size(); lead < boundary + 1; ++lead) {
        SCOPED_TRACE("char of " + std::to_string(ch.size()) +
                     " bytes, lead byte at " + std::to_string(lead));
        const std::string in = printable(lead) + ch + printable(5000);
        EXPECT_EQ("Query: " + in + "\n",
                  capture("Query", in.c_str(), in.size()));
      }
    }
  }

  const std::string latin1 = printable(4095) + "\xe9MARKER";
  EXPECT_EQ("Query: " + printable(4095) + " MARKER\n",
            capture("Query", latin1.c_str(), latin1.size()));

  std::string mixed;
  while (mixed.size() < 20000) mixed += "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80";
  EXPECT_EQ("Query: " + mixed + "\n",
            capture("Query", mixed.c_str(), mixed.size()));
}

/* 15c. A string longer than the single-line read size, with a multi-byte
        character across the boundary of a read, is printed intact on one
        line. */
TEST_F(SafePutsStderrSingleLineTest, LongerThanRun) {
  std::string in = printable(10000);
  in[4095] = '\xc3';
  in[4096] = '\xa9';
  EXPECT_EQ("Query: " + in + "\n", capture("Query", in.c_str(), in.size()));
}

/* 16. Printing stops at the first NUL byte, also when more than a chunk
       of data follows it. */
TEST_F(SafePutsStderrSingleLineTest, StopsAtNul) {
  const char in[] = "prefix\0suffix";
  EXPECT_EQ("Query: prefix\n", capture("Query", in, sizeof(in)));

  std::string in2 = printable(700);
  in2[150] = '\0';
  EXPECT_EQ("Query: " + in2.substr(0, 150) + "\n",
            capture("Query", in2.c_str(), in2.size()));
}

#ifdef __linux__
/* The address as printed by the my_safe_* formatters. */
static std::string address(const void *p) {
  char buf[32];
  my_safe_snprintf(buf, sizeof(buf), "%p", p);
  return buf;
}

/* 17. An entirely invalid pointer: the diagnostic follows the label. */
TEST_F(SafePutsStderrSingleLineTest, InvalidPointer) {
  const char *in = reinterpret_cast<const char *>(16);
  EXPECT_EQ("Query: Can't read from address " + address(in) + "\n",
            capture("Query", in, 100));
}

/* 18. A string running off the end of mapped memory: the readable part is
       printed on one line. Reading used to be done 256 bytes at a time and
       an unreadable address was reported only when such a read started at
       it; the output is unchanged. */
TEST_F(SafePutsStderrSingleLineTest, StraddlesUnmappedPage) {
  const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  ASSERT_GT(page, 1024U);
  void *mem = mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, mem);
  char *first = static_cast<char *>(mem);
  ASSERT_EQ(0, munmap(first + page, page));

  for (const size_t tail : {size_t{20}, kChunk, size_t{256}, size_t{512}}) {
    char *in = first + page - tail;
    const std::string expected = printable(tail);
    memcpy(in, expected.data(), tail);
    std::string want = "Query: " + expected + "\n";
    if (tail % 256 == 0)
      want += "Can't read from address " + address(first + page) + "\n";
    EXPECT_EQ(want, capture("Query", in, 1000)) << "tail " << tail;
  }

  EXPECT_EQ(0, munmap(first, page));
}
#endif /* __linux__ */

}  // namespace mysys_safe_puts_stderr_unittest

#endif /* HAVE_STACKTRACE && !_WIN32 */
