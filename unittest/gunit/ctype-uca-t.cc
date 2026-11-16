#include "my_global.h"
#include "m_ctype.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

TEST(CtypeUcaTest, StrnxfrmlenIsPessimisticForUdfa) {
  const CHARSET_INFO *cs = &my_charset_utf8mb4_0900_as_cs;

  // U+FDFA is a three-byte UTF-8 character that expands to eight
  // collation elements in the UCA 9.0.0 data.
  const char src[] = "\xEF\xB7\xBA\xEF\xB7\xBA\xEF\xB7\xBA\xEF\xB7\xBA";
  const size_t srclen = sizeof(src) - 1;

  const size_t max_len = cs->coll->strnxfrmlen(cs, srclen);

  // Use a buffer that is definitely large enough to hold the full key.
  std::vector<uchar> dst(srclen * 8 * 4 + 64);
  const size_t actual_len = cs->coll->strnxfrm(
      cs, dst.data(), dst.size(), reinterpret_cast<const uchar *>(src), srclen,
      0);

  EXPECT_GE(max_len, actual_len)
      << "strnxfrmlen() must be a pessimistic upper bound for strnxfrm()";
}

}  // namespace
