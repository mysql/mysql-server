/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
*/

global.path            = require("path");
global.fs              = require("fs");
global.assert          = require("assert");
global.util            = require("util");

global.adapter_dir     = __dirname;

global.build_dir       = path.join(adapter_dir, "impl", "build", "Release");
global.spi_doc_dir     = path.join(adapter_dir, "impl", "SPI-documentation");
global.spi_module      = path.join(adapter_dir, "impl", "SPI.js");
global.api_module      = path.join(adapter_dir, "api", "mynode.js");
global.udebug_module   = path.join(adapter_dir, "api", "unified_debug.js");


