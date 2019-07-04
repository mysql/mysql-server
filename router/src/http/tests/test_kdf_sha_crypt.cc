/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "../src/kdf_sha_crypt.h"

#include <tuple>

#include <gtest/gtest.h>

// give the constexpr' of ShaCrypt storage to keep googletest happy
// which wants to take a address of them
//
// if someone else needs to take an address of them, these should be moved
// to sha_crypt.cc directly
constexpr unsigned long ShaCryptMcfAdaptor::kDefaultRounds;
constexpr unsigned long ShaCryptMcfAdaptor::kMinRounds;
constexpr unsigned long ShaCryptMcfAdaptor::kMaxRounds;

class ShaCryptTest : public ::testing::Test,
                     public ::testing::WithParamInterface<
                         std::tuple<std::string,  // MCF string
                                    int,          // rounds
                                    std::string,  // salt
                                    std::string,  // checksum
                                    const char *  // password
                                    >> {};

TEST_P(ShaCryptTest, decode) {
  auto hash_info = ShaCryptMcfAdaptor::from_mcf(std::get<0>(GetParam()));
  EXPECT_EQ(hash_info.rounds(), std::get<1>(GetParam()));
  EXPECT_EQ(hash_info.salt(), std::get<2>(GetParam()));
  EXPECT_EQ(hash_info.checksum(), std::get<3>(GetParam()));
}

TEST_P(ShaCryptTest, verify) {
  auto hash_info = ShaCryptMcfAdaptor::from_mcf(std::get<0>(GetParam()));
  if (nullptr != std::get<4>(GetParam())) {
    EXPECT_EQ(hash_info.checksum(),
              ShaCrypt::derive(hash_info.digest(), hash_info.rounds(),
                               hash_info.salt(), std::get<4>(GetParam())));
  }
}

// only the MCF compliant variants are supported
//
// the original code also allows without $6$ prefix
//
// - rounds=1000$salt$checksum
// - salt$checksum
//
// Only if rounds= is followed by a positive integral number (incl 0) and a "$"
// it is not treated as salt:
//
//     $5$rounds=$...
//        ^^^ salt
//     $5$rounds=-1$...
//        ^^^ salt
//     $5$rounds=foobar1$...
//        ^^^ salt
//

INSTANTIATE_TEST_CASE_P(
    Foo, ShaCryptTest,
    ::testing::Values(
        std::make_tuple(  // sha512, no rounds
            "$6$saltstring$svn8UoSVapNtMuq1ukKS4tPQd8iKwSMHWjl/"
            "O817G3uBnIFNjnQJu"
            "esI68u4OTLiBFdcbYEdFCoEOfaS35inz1",
            5000, "saltstring",
            "svn8UoSVapNtMuq1ukKS4tPQd8iKwSMHWjl/"
            "O817G3uBnIFNjnQJuesI68u4OTLiBFdcbYEdFCoEOfaS35inz1",
            "Hello world!"),
        std::make_tuple(  // sha512, salt truncated
            "$6$rounds=10000$saltstringsaltstring$OW1/"
            "O6BYHV6BcXZu8QVeXbDWra3Oeqh0sb"
            "HbbMCVNSnCM/UrjmM0Dp8vOuZeHBy/YTBmSK6H9qs/y3RnOaw5v.",
            10000, "saltstringsaltst",
            "OW1/O6BYHV6BcXZu8QVeXbDWra3Oeqh0sbHbbMCVNSnCM/UrjmM0Dp8vOuZeHBy/"
            "YTBmSK6H9qs/y3RnOaw5v.",
            "Hello world!"),
        std::make_tuple(  // sha512, salt too long
            "$6$rounds=5000$toolongsaltstring$"
            "lQ8jolhgVRVhY4b5pZKaysCLi0QBxGoNeKQ"
            "zQ3glMhwllF7oGDZxUhx1yxdYcz/e1JSbq3y6JMxxl8audkUEm0",
            5000, "toolongsaltstrin",
            "lQ8jolhgVRVhY4b5pZKaysCLi0QBxGoNeKQ"
            "zQ3glMhwllF7oGDZxUhx1yxdYcz/e1JSbq3y6JMxxl8audkUEm0",
            "This is just a test"),

        std::make_tuple(  // sha512, salt too long
            "$6$rounds=1400$anotherlongsaltstring$POfYwTEok97VWcjxIiSOjiykti.o/"
            "pQs.wP"
            "vMxQ6Fm7I6IoYN3CmLs66x9t0oSwbtEW7o7UmJEiDwGqd8p4ur1",
            1400, "anotherlongsalts",
            "POfYwTEok97VWcjxIiSOjiykti.o/pQs.wP"
            "vMxQ6Fm7I6IoYN3CmLs66x9t0oSwbtEW7o7UmJEiDwGqd8p4ur1",
            "a very much longer text to encrypt.  This one even stretches over "
            "more"
            "than one line."),
        std::make_tuple(  // sha512, salt short
            "$6$rounds=77777$short$WuQyW2YR.hBNpjjRhpYD/"
            "ifIw05xdfeEyQoMxIXbkvr0g"
            "ge1a1x3yRULJ5CCaUeOxFmtlcGZelFl5CxtgfiAc0",
            77777, "short",
            "WuQyW2YR.hBNpjjRhpYD/ifIw05xdfeEyQoMxIXbkvr0g"
            "ge1a1x3yRULJ5CCaUeOxFmtlcGZelFl5CxtgfiAc0",
            "we have a short salt string but not a short password"),
        std::make_tuple(  // sha512, short password
            "$6$rounds=123456$asaltof16chars..$"
            "BtCwjqMJGx5hrJhZywWvt0RLE8uZ4oPwc"
            "elCjmw2kSYu.Ec6ycULevoBK25fs2xXgMNrCzIMVcgEJAstJeonj1",
            123456, "asaltof16chars..",
            "BtCwjqMJGx5hrJhZywWvt0RLE8uZ4oPwc"
            "elCjmw2kSYu.Ec6ycULevoBK25fs2xXgMNrCzIMVcgEJAstJeonj1",
            "a short string"),
        std::make_tuple(  // sha512, small rounds
            "$6$rounds=10$roundstoolow$kUMsbe306n21p9R.FRkW3IGn.S9NPN0x50YhH1x"
            "hLsPuWGsUSklZt58jaTfF4ZEQpyUNGc0dqbpBYYBaHHrsX.",
            1000, "roundstoolow",
            "kUMsbe306n21p9R.FRkW3IGn.S9NPN0x50YhH1x"
            "hLsPuWGsUSklZt58jaTfF4ZEQpyUNGc0dqbpBYYBaHHrsX.",
            "the minimum number is still observed"),
        std::make_tuple(  // sha256, no rounds
            "$5$saltstring$5B8vYYiY.CVt1RlTTf8KbXBH3hsxY/GNooZaBBGWEc5", 5000,
            "saltstring", "5B8vYYiY.CVt1RlTTf8KbXBH3hsxY/GNooZaBBGWEc5",
            "Hello world!"),
        std::make_tuple(  // sha256
            "$5$rounds=10000$saltstringsaltst$3xv."
            "VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA",
            10000, "saltstringsaltst",
            "3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA", "Hello world!"),
        std::make_tuple(  // sha256
            "$5$rounds=1400$anotherlongsaltstring$"
            "Rx.j8H.h8HjEDGomFU8bDkXm3XIUnzyxf12oP84Bnq1",
            1400, "anotherlongsalts",
            "Rx.j8H.h8HjEDGomFU8bDkXm3XIUnzyxf12oP84Bnq1",
            "a very much longer text to encrypt.  This one even stretches over "
            "more"
            "than one line."),
        std::make_tuple(  // sha256
            "$5$rounds=77777$short$JiO1O3ZpDAxGJeaDIuqCoEFysAe1mZNJRs3pw0KQRd/",
            77777,                                          // rounds
            "short",                                        // salt
            "JiO1O3ZpDAxGJeaDIuqCoEFysAe1mZNJRs3pw0KQRd/",  // checksum
            "we have a short salt string but not a short password"),
        std::make_tuple(  // sha256, short password
            "$5$rounds=123456$asaltof16chars..$gP3VQ/"
            "6X7UUEW3HkBn2w1/Ptq2jxPyzV/cZKmF/wJvD",
            123456,                                         // rounds
            "asaltof16chars..",                             // salt
            "gP3VQ/6X7UUEW3HkBn2w1/Ptq2jxPyzV/cZKmF/wJvD",  // checksum
            "a short string"),
        std::make_tuple(  // sha256, small rounds
            "$5$rounds=10$roundstoolow$yfvwcWrQ8l/"
            "K0DAWyuPMDNHpIVlTQebY9l/gL972bIC",
            1000,                                           // rounds
            "roundstoolow",                                 // salt
            "yfvwcWrQ8l/K0DAWyuPMDNHpIVlTQebY9l/gL972bIC",  // checksum
            "the minimum number is still observed"),

        // empty checksum signals the 'verify' not skip the verification

        std::make_tuple(  // sha256, no $ after founds -> salt
            "$5$rounds=1001",
            ShaCryptMcfAdaptor::kDefaultRounds,  // rounds
            "rounds=1001",                       // salt
            "",                                  // checksum
            nullptr),
        std::make_tuple(  // sha256, rounds set, empty salt
            "$5$rounds=1001$",
            1001,  // rounds
            "",    // salt
            "",    // checksum
            nullptr),
        std::make_tuple(  // sha256, negative integer -> salt
            "$5$rounds=-1$",
            ShaCryptMcfAdaptor::kDefaultRounds,  // rounds
            "rounds=-1",                         // salt
            "",                                  // checksum
            nullptr),
        std::make_tuple(  // sha256, no integral number after rounds -> salt
            "$5$rounds=foobar$checksum",
            ShaCryptMcfAdaptor::kDefaultRounds,  // rounds
            "rounds=foobar",                     // salt
            "checksum",                          // checksum
            nullptr)));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
