/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "../src/content_type.h"

#include <string>

#include <gtest/gtest.h>

/**
 * @test ensure file-extension matching works.
 */

struct ContentTypeParam {
  std::string extension;
  std::string content_type;
};

void PrintTo(const ContentTypeParam &me, ::std::ostream *os) {
  *os << "(" << me.extension << ", " << me.content_type << ")";
}

class ContentTypeTest : public ::testing::Test,
                        public ::testing::WithParamInterface<ContentTypeParam> {
};

TEST_P(ContentTypeTest, ensure) {
  EXPECT_EQ(GetParam().content_type,
            ContentType::from_extension(GetParam().extension));
}

ContentTypeParam content_type_values[] = {
    {"css", MimeType::TextCss},
    {"jpg", MimeType::ImageJpeg},
    {"jpeg", MimeType::ImageJpeg},
    {"htm", MimeType::TextHtml},
    {"html", MimeType::TextHtml},
    {"Html", MimeType::TextHtml},
    {"HTML", MimeType::TextHtml},
    {"js", MimeType::ApplicationJavascript},
    {"json", MimeType::ApplicationJson},
    {"png", MimeType::ImagePng},
    {"svg", MimeType::ImageSvgXML},

    {"htn", MimeType::ApplicationOctetStream},
    {"unknown", MimeType::ApplicationOctetStream},
};

INSTANTIATE_TEST_SUITE_P(Spec, ContentTypeTest,
                         ::testing::ValuesIn(content_type_values),
                         [](const auto &tp) { return tp.param.extension; });

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
