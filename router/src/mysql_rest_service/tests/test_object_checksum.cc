/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <vector>

#include "mrs/database/helper/object_checksum.h"
#include "test_mrs_database_rest_table.h"
#include "test_mrs_object_utils.h"

using namespace mrs::database;

using testing::Test;

class TestDigest : public IDigester {
 public:
  void update(std::string_view data) override {
    size_t start = 0, p = data.find('\0');
    while (p < data.size()) {
      updates_.append(data.substr(start, p - start));
      updates_.append("\\0");
      start = p + 1;
      p = data.find('\0', start);
    }
    updates_.append(data.substr(start));
  }

  std::string finalize() override { return updates_; }

 private:
  std::string updates_;
};

TEST(ObjectChecksum, plain) {
  auto root = DualityViewBuilder("mrstestdb", "actor")
                  .field("field1")
                  .field("field2")
                  .field("field3")
                  .field("field4")
                  .field("field5")
                  .field("field6")
                  .resolve();

  std::string doc = R"*({
            "field1": 1,
            "field2": "text",
            "field3": null,
            "field4": 0.3,
            "field5": true,
            "field6": {
                "nested": "json",
                "object": {"another": [{"something":123}, {}, []]}
            }
    })*";

  TestDigest visited_fields;
  mrs::database::digest_object(root, doc, &visited_fields);
  EXPECT_EQ(
      "{\"field1\":\x1\\0\\0\\0\"field2\":\"text\"\"field3\":null\"field4\":"
      "333333\xD3?\"field5\":true\"field6\":{\"nested\":\"json\"\"object\":{"
      "\"another\":[{\"something\":{\\0\\0\\0}{}[]]}}}",
      visited_fields.finalize());

  std::string tmp1 = mrs::database::post_process_json(root, {}, {}, doc);
  EXPECT_EQ(
      R"*({"field1":1,"field2":"text","field3":null,"field4":0.3,"field5":true,"field6":{"nested":"json","object":{"another":[{"something":123},{},[]]}},"_metadata":{"etag":"1F4204272C93FD5F5F6BB6E8E3221C6F35C81961E4335C4328E3E916E6614D6A"}})*",
      tmp1);

  std::string tmp2 = mrs::database::post_process_json(
      root, {}, {{"testmd", "testvalue"}}, doc);
  EXPECT_EQ(
      R"*({"field1":1,"field2":"text","field3":null,"field4":0.3,"field5":true,"field6":{"nested":"json","object":{"another":[{"something":123},{},[]]}},"_metadata":{"etag":"1F4204272C93FD5F5F6BB6E8E3221C6F35C81961E4335C4328E3E916E6614D6A","testmd":"testvalue"}})*",
      tmp2);

  EXPECT_EQ(make_json(tmp1)["_metadata"]["etag"],
            make_json(tmp2)["_metadata"]["etag"]);
}

TEST(ObjectChecksum, object) {
  auto root =
      DualityViewBuilder("mrstestdb", "object")
          .field("field1")
          .field("field2")
          .field_to_one(
              "nested1",
              ViewBuilder("object1")
                  .field("field3")
                  .field("field4")
                  .field_to_one(
                      "nested2",
                      ViewBuilder("object2").field("field5").field("field6")))
          .field("field7")
          .resolve();

  {
    std::string doc = R"*({
        "field1": 123,
        "field2": true,
        "nested1": {
            "field3": "hello",
            "field4": 321.345,
            "nested2": {
                "field5": null,
                "field6": "{string string string}"
            }
        },
        "field7": "hello"
    })*";

    doc = mrs::database::post_process_json(root, {}, {}, doc);

    EXPECT_EQ(
        R"*({"field1":123,"field2":true,"nested1":{"field3":"hello","field4":321.345,"nested2":{"field5":null,"field6":"{string string string}"}},"field7":"hello","_metadata":{"etag":"961677B781AA86E0BD2BF3F1B4CEE9C827D98948F9A062C1B29AE16BD7524969"}})*",
        doc);
  }
  {
    std::string doc = R"*({
        "nested1": {"nested2": {}}
        }
    })*";

    doc = mrs::database::post_process_json(root, {}, {}, doc);

    EXPECT_EQ(
        R"*({"nested1":{"nested2":{}},"_metadata":{"etag":"584B8199EBC1E37A7DC4E29AC291EFE6C5D6B033B0D8DE4561EE364849EF9C5A"}})*",
        doc);
  }
}

TEST(ObjectChecksum, array) {
  auto root =
      DualityViewBuilder("mrstestdb", "object")
          .field("field1")
          .field_to_many(
              "nested1",
              ViewBuilder("object1").field("field3").field_to_one(
                  "nested2",
                  ViewBuilder("object2").field("field5").field("field6")))
          .field("field7")
          .resolve();
  {
    std::string doc = R"*({
        "field1": 123,
        "nested1": [{
                "field3": "hello",
                "nested2": {
                    "field5": null,
                    "field6": "{string string string}"
                }
            },
            {
                "field3": "world",
                "nested2": {
                    "field5": 1,
                    "field6": 2
                }
            }
        ],
        "field7": "hello"
    })*";

    doc = mrs::database::post_process_json(root, {}, {}, doc);

    EXPECT_EQ(
        R"*({"field1":123,"nested1":[{"field3":"hello","nested2":{"field5":null,"field6":"{string string string}"}},{"field3":"world","nested2":{"field5":1,"field6":2}}],"field7":"hello","_metadata":{"etag":"5DC7C15748D9AB467CC3D61E772655A30F090CE133E6FCC8FDF9110E53B1A65E"}})*",
        doc);
  }

  {
    std::string doc = R"*({
        "nested1": [{
                "field3": ["x", [], {}],
                "nested2": {}
            },
            {
                "field3": "world",
                "nested2": {
                    "field5": [123, 456, [[[]]]],
                    "field6": {"a":{}, "": 123456}
                }
            }
        ],
        "field7": [888,999]
    })*";

    doc = mrs::database::post_process_json(root, {}, {}, doc);

    EXPECT_EQ(
        R"*({"nested1":[{"field3":["x",[],{}],"nested2":{}},{"field3":"world","nested2":{"field5":[123,456,[[[]]]],"field6":{"a":{},"":123456}}}],"field7":[888,999],"_metadata":{"etag":"6168622C90D2AEF1DFA08534210C25758B340223691047EA381FE84262C2C919"}})*",
        doc);
  }
}

TEST(ObjectChecksum, nocheck_disabled) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor")
          .field("field")
          .field("field2", FieldFlag::WITH_NOCHECK)
          .field("field3", FieldFlag::DISABLED)
          .field_to_one(
              "nest",
              ViewBuilder("nested")
                  .field("field", FieldFlag::WITH_NOCHECK)
                  .field("field5", FieldFlag::DISABLED)
                  .field("field6")
                  .field_to_many("list",
                                 ViewBuilder("nestlist")
                                     .field("field", FieldFlag::WITH_NOCHECK)
                                     .field("fieldx", FieldFlag::DISABLED)
                                     .field("fieldy")))
          .field_to_one(
              "nest2",
              ViewBuilder("nested", TableFlag::WITH_NOCHECK).field("field7"))
          .field_to_one("nest3", ViewBuilder("nested").field(
                                     "field8", FieldFlag::DISABLED))
          .resolve();
  {
    std::string orig_doc = R"*({
        "field": 1234,
        "field2": false,
        "field3": {"x": 32},
        "nest": {
            "field": [],
            "field5": "text",
            "field6": "more text",
            "list": [
                {
                    "field": 123,
                    "fieldx": "abc",
                    "fieldy": null
                },
                {
                    "field": 678,
                    "fieldx": "xyz",
                    "fieldy": []
                }
            ]
        },
        "nest2": {
            "field7": null
        },
        "nest3": {
            "field8": null
        }
    })*";
    std::string doc = orig_doc;

    doc = mrs::database::post_process_json(root, {}, {}, doc);

    std::string expected =
        R"*({"field":1234,"field2":false,"nest":{"field":[],"field6":"more text","list":[{"field":123,"fieldy":null},{"field":678,"fieldy":[]}]},"nest2":{"field7":null},"nest3":{},"_metadata":{"etag":"A32F45D33DE989D9260297459B8A084CBDC8BB097077BA80811B9236F40947D9"}})*";

    EXPECT_EQ(expected, doc);

    TestDigest visited_fields;
    mrs::database::digest_object(root, orig_doc, &visited_fields);
    // should have visited all fields that are not nocheck or disabled
    EXPECT_EQ(
        //"field field3 x nest field5 field6 field3 field8 ",
        "{\"field\":\xD2\x4\\0\\0\"field3\":{\"x\": "
        "\\0\\0\\0}\"nest\":{\"field5\":\"text\"\"field6\":\"more "
        "text\"\"list\":[{\"fieldx\":\"abc\"\"fieldy\":null}{\"fieldx\":"
        "\"xyz\"\"fieldy\":[]}]}\"nest3\":{\"field8\":null}}",
        visited_fields.finalize());

    // try again completely omitting the disabled fields
    // output JSON should be identical, but not the etag
    doc = R"*({
        "field": 1234,
        "field2": false,
        "nest": {
            "field": [],
            "field6": "more text",
            "list": [
                {
                    "field": 123,
                    "fieldy": null
                },
                {
                    "field": 678,
                    "fieldy": []
                }
            ]
        },
        "nest2": {
            "field7": null
        },
        "nest3": {
        }
    })*";

    doc = mrs::database::post_process_json(root, {}, {}, doc);

    EXPECT_EQ(expected.substr(0, expected.find("_metadata")),
              doc.substr(0, doc.find("_metadata")));

    // try again completely omitting the no-check fields
    // output JSON will change, but the etag should match
    doc = R"*({
        "field": 1234,
        "field3": {"x": 32},
        "nest": {
            "field5": "text",
            "field6": "more text",
            "list": [
                {
                    "fieldx": "abc",
                    "fieldy": null
                },
                {
                    "fieldx": "xyz",
                    "fieldy": []
                }
            ]
        },
        "nest3": {
            "field8": null
        }
    })*";

    doc = mrs::database::post_process_json(root, {}, {}, doc);
    EXPECT_EQ(expected.substr(expected.find("_metadata")),
              doc.substr(doc.find("_metadata")))
        << doc;
  }
  {
    // potentially ambiguous
    std::string doc = R"*({
        "field": 1234,
        "nest": {
        }
    })*";

    doc = mrs::database::post_process_json(root, {}, {}, doc);

    EXPECT_EQ(
        R"*({"field":1234,"nest":{},"_metadata":{"etag":"DC8336A1B1135723F59CBCBB068F167016080B3FCE36B000EECBD171655C2285"}})*",
        doc);

    doc = R"*({
        "nest": {
            "field": 1234
        }
    })*";
    doc = mrs::database::post_process_json(root, {}, {}, doc);

    EXPECT_EQ(
        R"*({"nest":{"field":1234},"_metadata":{"etag":"B626CFC21129922857AD78E49C5D1951E3185D692CC3FBAD07E641F95DC6997E"}})*",
        doc);
  }
}

TEST(ObjectChecksum, column_filter) {
  auto root =
      DualityViewBuilder("mrstestdb", "actor")
          .field("field")
          .field("field2", FieldFlag::WITH_NOCHECK)
          .field("field3", FieldFlag::DISABLED)
          .field_to_one(
              "nest",
              ViewBuilder("nested")
                  .field("field", FieldFlag::WITH_NOCHECK)
                  .field("field5", FieldFlag::DISABLED)
                  .field("field6")
                  .field_to_many("list",
                                 ViewBuilder("nestlist")
                                     .field("field", FieldFlag::WITH_NOCHECK)
                                     .field("fieldx", FieldFlag::DISABLED)
                                     .field("fieldy")))
          .field_to_one(
              "nest2",
              ViewBuilder("nested", TableFlag::WITH_NOCHECK).field("field7"))
          .field_to_one("nest3", ViewBuilder("nested").field(
                                     "field8", FieldFlag::DISABLED))
          .resolve();

  {
    std::string orig_doc = R"*({
        "field": 1234,
        "field2": false,
        "field3": {"x": 32},
        "nest": {
            "field": [],
            "field5": "text",
            "field6": "more text",
            "list": [
                {
                    "field": 123,
                    "fieldx": "abc",
                    "fieldy": null
                },
                {
                    "field": 678,
                    "fieldx": "xyz",
                    "fieldy": []
                }
            ]
        },
        "nest2": {
            "field7": null
        },
        "nest3": {
            "field8": null
        }
    })*";
    std::string doc;

    auto exclude_filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"!field", "!nest2", "!nest.list.fieldy"});
    std::string exclude_expected =
        R"*({"field2":false,"nest":{"field":[],"field6":"more text","list":[{"field":123},{"field":678}]},"nest3":{},"_metadata":{"etag":"A32F45D33DE989D9260297459B8A084CBDC8BB097077BA80811B9236F40947D9"}})*";

    auto include_filter = mrs::database::dv::ObjectFieldFilter::from_url_filter(
        *root, {"field", "nest2", "nest.list.fieldy"});

    // checksum should ignore column filter so etags should be the same as in
    // exclude_expected
    std::string include_expected =
        R"*({"field":1234,"nest":{"list":[{"fieldy":null},{"fieldy":[]}]},"nest2":{"field7":null},"_metadata":{"etag":"A32F45D33DE989D9260297459B8A084CBDC8BB097077BA80811B9236F40947D9"}})*";

    doc = mrs::database::post_process_json(root, exclude_filter, {}, orig_doc);
    EXPECT_EQ(exclude_expected, doc);

    doc = mrs::database::post_process_json(root, include_filter, {}, orig_doc);
    EXPECT_EQ(include_expected, doc);

    TestDigest visited_fields1;
    mrs::database::digest_object(root, orig_doc, &visited_fields1);
    // should have visited all fields that are not nocheck
    EXPECT_EQ(
        // field field3 x nest field5 field6 list.fieldx list.fieldy
        // nest3.field8
        "{\"field\":\xD2\x4\\0\\0\"field3\":{\"x\": "
        "\\0\\0\\0}\"nest\":{\"field5\":\"text\"\"field6\":\"more "
        "text\"\"list\":[{\"fieldx\":\"abc\"\"fieldy\":null}{\"fieldx\":"
        "\"xyz\"\"fieldy\":[]}]}\"nest3\":{\"field8\":null}}",
        visited_fields1.finalize());

    // try again completely omitting the disabled fields
    // output JSON should be identical, but not etags
    doc = R"*({
        "field": 1234,
        "field2": false,
        "nest": {
            "field": [],
            "field6": "more text",
            "list": [
                {
                    "field": 123,
                    "fieldy": null
                },
                {
                    "field": 678,
                    "fieldy": []
                }
            ]
        },
        "nest2": {
            "field7": null
        },
        "nest3": {
        }
    })*";

    doc = mrs::database::post_process_json(root, exclude_filter, {}, doc);

    EXPECT_EQ(exclude_expected.substr(0, exclude_expected.find("_metadata")),
              doc.substr(0, doc.find("_metadata")));

    // try again completely omitting the no-check fields
    // output JSON will change, but the etag should match
    doc = R"*({
        "field": 1234,
        "field3": {"x": 32},
        "nest": {
            "field5": "text",
            "field6": "more text",
            "list": [
                {
                    "fieldx": "abc",
                    "fieldy": null
                },
                {
                    "fieldx": "xyz",
                    "fieldy": []
                }
            ]
        },
        "nest3": {
            "field8": null
        }
    })*";
    TestDigest visited_fields2;
    mrs::database::digest_object(root, doc, &visited_fields2);
    EXPECT_EQ(visited_fields1.finalize(), visited_fields2.finalize());

    doc = mrs::database::post_process_json(root, exclude_filter, {}, doc);
    EXPECT_EQ(exclude_expected.substr(exclude_expected.find("_metadata")),
              doc.substr(doc.find("_metadata")))
        << doc;
  }
}
