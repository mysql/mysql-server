/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_ROUTING_GUIDELINES_SCHEMA_H_
#define ROUTER_SRC_ROUTING_GUIDELINES_SCHEMA_H_

constexpr const char *const k_routing_guidelines_schema =
    R"({
  "$schema":"https://json-schema.org/draft/2020-12/schema",
  "title":"MySQL Router routing guidelines engine document schema",
  "type":"object",
  "properties":{
    "name":{
      "description":"Name of the routing guidelines document",
      "type":"string"
    },
    "version":{
      "description":"Version of the routing guidelines document",
      "type":"string"
    },
    "destinations":{
      "description":"Entries representing set of MySQL server instances",
      "type":"array",
      "items":{
        "type":"object",
        "properties":{
          "name":{
            "description":"Unique name of the given destinations entry",
            "type":"string"
          },
          "match":{
            "description":"Matching criteria for destinations class",
            "type":"string"
          }
        },
        "required":[
          "name",
          "match"
        ]
      },
      "minItems":1,
      "uniqueItems":true
    },
    "routes":{
      "description":"Routes entries that are binding destinations with connection matching criteria",
      "type":"array",
      "items":{
        "type":"object",
        "properties":{
          "name":{
            "description":"Name of the route",
            "type":"string"
          },
          "connectionSharingAllowed":{
            "type":"boolean"
          },
          "enabled":{
            "type":"boolean"
          },
          "match":{
            "description":"Connection matching criteria",
            "type":"string"
          },
          "destinations":{
            "description":"Destination groups used for routing, by order of preference",
            "type":"array",
            "items":{
              "type":"object",
              "properties":{
                "classes":{
                  "description":"Destination group",
                  "type":"array",
                  "items":{
                    "description":"Reference to 'name' entries in the 'destinations' section",
                    "type":"string"
                  }
                },
                "strategy":{
                  "description":"Routing strategy that will be used for this route",
                  "type":"string",
                  "enum":[
                    "round-robin",
                    "first-available"
                  ]
                },
                "priority":{
                  "description":"Priority of the given group",
                  "type":"integer",
                  "minimum":0
                }
              },
              "required":[
                "classes",
                "strategy",
                "priority"
              ],
              "minItems":1,
              "uniqueItems":true
            }
          }
        },
        "required":[
          "name",
          "match",
          "destinations"
        ],
        "minItems":1,
        "uniqueItems":true
      }
    }
  },
  "required":[
    "version",
    "destinations",
    "routes"
  ],
  "additionalProperties": false,

  "match_rules":{
    "keywords":{
      "type": "array",
      "items":{
          "type": "string",
          "enum": %s
      }
    },
    "functions":{
      "type": "array",
      "items":{
          "type": "string",
          "enum": %s
      }
    },
    "variables":{
      "type": "array",
      "items":{
          "type": "string",
          "enum": %s
      }
    }
  }
})";

#endif  // ROUTER_SRC_ROUTING_GUIDELINES_SCHEMA_H_
