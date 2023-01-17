// Copyright (c) 2018, 2023, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

{
  "title": "MySQL Server Mock Data",
      "description": "JSON Schema of the input files of the mysql-server-mock",
      "$schema": "http://json-schema.org/draft-04/schema#",

      "type": "object", "additionalProperties": false,

      "definitions": {
        "Ok": {
          "description": "OK response",
          "type": "object",
          "additionalProperties": false,
          "properties": {
            "last_insert_id": {
              "description": "last insert id",
              "type": "integer",
              "minimum": 0,
              "default": 0
            },
            "warning_count": {
              "description": "number of warnings",
              "type": "integer",
              "minimum": 0,
              "default": 0
            }
          }
        },

        "Error": {
          "description": "Error response",
          "type": "object",
          "additionalProperties": false,
          "required": ["code", "message"],
          "properties": {
            "message": {"type": "string"},
            "sql_state": {
              "description": "SQL state",
              "type": "string",
              "minLength": 5,
              "maxLength": 5,
              "default": "HY000"
            },
            "code":
                {"description": "error code", "type": "integer", "minimum": 0}
          }
        },

        "ExecTime": {
          "description": "Execution time in milliseconds",
          "type": "number",
          "minimum": 0,
          "default": 0
        },

        "ResultsetColumn": {
          "description": "column description",
          "type": "object",
          "additionalProperties": false,
          "required": ["type", "name"],

          "properties": {
            "type": {
              "description": "datatype of the column",
              "enum":
                  [
                    "TINY",       "SHORT",    "LONG",       "INT24",
                    "LONGLONG",   "DECIMAL",  "NEWDECIMAL", "FLOAT",
                    "DOUBLE",     "BIT",      "TIMESTAMP",  "DATE",
                    "TIME",       "DATETIME", "YEAR",       "STRING",
                    "VAR_STRING", "BLOB",     "SET",        "ENUM",
                    "GEOMETRY",   "NULL",     "TINYBLOB",   "LONGBLOB",
                    "MEDIUMBLOB"
                  ]
            },
            "name": {"description": "name of the column", "type": "string"},
            "orig_name": {
              "description": "original name of the column",
              "type": "string"
            },
            "table":
                {"description": "name of the column table", "type": "string"},
            "orig_table": {
              "description": "original name of the column table",
              "type": "string"
            },
            "schema": {
              "description": "name of the column table schema",
              "type": "string"
            },
            "catalog": {"description": "name of the catalog", "type": "string"},
            "flags": {
              "description": "column flags",
              "type": "integer",
              "minimum": 0,
              "maximum": 65535,
              "default": 0
            },
            "decimals": {
              "description": "column decimals",
              "type": "integer",
              "minimum": 0,
              "maximum": 255,
              "default": 0
            },
            "length": {
              "description": "column length",
              "type": "integer",
              "minimum": 0
            },
            "character_set": {
              "description": "column character set",
              "type": "integer",
              "minimum": 0,
              "maximum": 65535,
              "default": 63
            },
            "comment": {
              "description":
                  "additional information/comment about the column; ignored by the server mock, for self-documenting only",
              "type": "string"
            },
            "repeat": {
              "description":
                  "if set to positive number n, the data for given field will be duplicated n times; useful for generating big packets; supported only for columns with type STRING",
              "type": "integer",
              "minimum": 1,
              "default": 1
            }
          }
        },

        "ResultsetRow": {
          "description": "resultset row",
          "type": "array",
          "minItems": 1,
          "items": {
            "title": "ResultsetField",
            "description": "field of a resultset row",
            "type": ["number", "string", "boolean", "null"]
          }
        },

        "Result": {
          "description": "resultset for a statement",
          "type": "object",
          "additionalProperties": false,

          "properties": {
            "columns": {
              "description": "column descriptions",
              "type": "array",
              "minItems": 1,
              "items": {"$ref": "#/definitions/ResultsetColumn"}
            },
            "rows": {
              "description": "resultset rows",
              "type": "array",
              "items": {"$ref": "#/definitions/ResultsetRow"}
            }
          },
          "required": ["columns"]
        },

        "RPC": {
          "description": "statement and its response",
          "type": "object",
          "additionalProperties": false,

          "properties": {
            "stmt": {"description": "statement text", "type": "string"},
            "exec_time": {"$ref": "#/definitions/ExecTime"},
            "stmt.regex": {
              "description": "regular expression matching the statement text",
              "type": "string"
            },
            "result": {"$ref": "#/definitions/Result"},
            "ok": {"$ref": "#/definitions/Ok"},
            "error": {"$ref": "#/definitions/Error"}
          },
          "allOf":
              [
                {
                  "oneOf":
                      [{"required": ["stmt"]}, {"required": ["stmt.regex"]}]
                },
                {
                  "oneOf": [
                    {"required": ["ok"]}, {"required": ["result"]},
                    {"required": ["error"]}
                  ]
                }
              ]
        },

        "Greeting": {
          "description": "First packet of the MySQL Classic Protocol Handshake",
          "type": "object",
          "properties": {"exec_time": {"$ref": "#/definitions/ExecTime"}}
        },

        "Notice": {
          "description": "X-protocol notice definition",
          "type": "object",
          "properties": {
            "type": {
              "description": "type (id) of the notice message",
              "type": "integer"
            },
            "scope": {
              "description": "scope of the notice (LOCAL or GLOBAL)",
              "type": "string"
            },
            "payload": {
              "description": "Notice protobuf message as json object",
              "type": "object"
            }
          }
        }
      },

      "properties": {
        "defaults": {
          "description": "Default options",
          "type": "object",
          "properties": {"exec_time": {"$ref": "#/definitions/ExecTime"}}
        },
        "handshake": {
          "description": "Handshake Configuration",
          "type": "object",
          "properties": {"greeting": {"$ref": "#/definitions/Greeting"}}
        },
        "stmts": {
          "description":
              "RPCs that the mock will expect and process one after the other",
          "type": "array",
          "items": {"$ref": "#/definitions/RPC"}
        },
        "notices": {
          "description":
              "X-protocol notices to be sent to the connected client asynchronously",
          "type": "array",
          "items": {"$ref": "#/definitions/Notice"}
        }
      },
      "required": ["stmts"]
}
