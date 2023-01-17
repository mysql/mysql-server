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
  "title": "MySQL Router dynamic state file",
      "description": "JSON Schema of the MySQLRouter dynamic state file",
      "$schema": "http://json-schema.org/draft-04/schema#",

      "type": "object", "additionalProperties": false,

      "properties": {
        "version": {
          "description": "State file version in the MAJOR.MINOR.PATCH format",
          "type": "string"
        },
        "metadata-cache": {
          "description": "metadata-cache section definition",
          "type": "object",
          "additionalProperties": false,
          "required": ["cluster-metadata-servers"],
          "properties": {
            "group-replication-id": {
              "description":
                  "In case of the Group Replication ID the metadata cache module was bootstrapped against, in case of Async Replicaset cluster ID the metadata cache module was bootstrapped against",
              "type": "string"
            },
            "clusterset-id": {
              "description":
                  "Id of the ClusterSet the metadata cache module was bootstrapped against",
              "type": "string"
            },
            "oneOf":
                [
                  {"required": ["group-replication-id"]},
                  {"required": ["clusterset-id"]}
                ],

            "cluster-metadata-servers": {
              "description":
                  "List of the metadata servers that metadata cache module uses for fetching metadata",
              "type": "array",
              "uniqueItems": true,
              "items": {"$ref": "#/definitions/nodeAddress"},
              "definitions": {
                "nodeAddress": {
                  "description": "URI of the metadata server",
                  "type": "string"
                }
              }
            },
            "view-id": {
              "description":
                  "Last seen view_id of the Replicaset Cluster or ClusterSet metadata",
              "type": "number"
            }
          }
        }
      },

      "required": ["version", "metadata-cache"]
}
