#include "src/hello.h"

#include "gtest/gtest.h"

class HelloTest : public ::testing::Test {};

TEST_F(HelloTest, CborVersion) {
  EXPECT_EQ(cbor_version(), 0);
}

