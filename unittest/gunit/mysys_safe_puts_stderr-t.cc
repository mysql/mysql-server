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
  string to STDERR from a signal handler as a sequence of self-describing
  lines of the form

    <label> +<offset>: <chunk>\n

  The tests redirect STDERR into a pipe, call the function, and parse the
  captured lines.
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

class SafePutsStderrTest : public ::testing::Test {
 protected:
  /*
    Redirect STDERR into a pipe, call my_safe_puts_stderr(), restore STDERR
    and return everything the function wrote. The write end of the pipe is
    closed before reading so that read() reports EOF; inputs are kept far
    below the pipe capacity. May be called several times per test.
  */
  std::string capture(const char *label, const char *val, size_t max_len) {
    int fds[2];
    fflush(stderr);
    EXPECT_EQ(0, pipe(fds));
    const int saved_stderr = dup(STDERR_FILENO);
    EXPECT_GE(saved_stderr, 0);
    EXPECT_GE(dup2(fds[1], STDERR_FILENO), 0);

    my_safe_puts_stderr(label, val, max_len);

    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);
    close(fds[1]);

    std::string out;
    char buf[1024];
    for (;;) {
      const ssize_t n = read(fds[0], buf, sizeof(buf));
      if (n <= 0) break;
      out.append(buf, static_cast<size_t>(n));
    }
    close(fds[0]);
    return out;
  }

  /*
    Split the raw output into lines and check the invariants every line has
    to satisfy: ends with '\n', is at most kMaxLine bytes, starts with the
    "<label> +" prefix, and contains no control characters.
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

/* 5. Non-printable bytes are replaced by ' ' (parse() also checks that no
      line contains a control character). */
TEST_F(SafePutsStderrTest, NonPrintableReplaced) {
  const std::string in("ab\ncd\tef\x01gh\x7fij", 14);
  const std::vector<Line> lines = dump("Query", in.c_str(), in.size());
  ASSERT_EQ(1U, lines.size());
  EXPECT_EQ("ab cd ef gh ij", lines[0].payload);

  /* The printable-ASCII range is [0x20, 0x7e]: 0x20 and 0x7e are kept,
     while 0x7f and any byte >= 0x80 are replaced by ' '. */
  const std::string edges("\x20\x7e\x7f\x80\xff", 5);
  const std::vector<Line> edge_lines = dump("Query", edges.c_str(), 5);
  ASSERT_EQ(1U, edge_lines.size());
  EXPECT_EQ(" ~   ", edge_lines[0].payload);
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

}  // namespace mysys_safe_puts_stderr_unittest

#endif /* HAVE_STACKTRACE && !_WIN32 */
