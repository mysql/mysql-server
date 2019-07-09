/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
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
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */


var conf = require("../adapter_config.js"),
    path = require("path"),
    fs   = require("fs");


/*  getDBServiceProvider()
    The internal DBServiceProvider modules are located in spi_dir.
    An external DBServiceProvider module "x" must be loadable using: 
      require("x/x_service_provider.js")
*/

exports.getDBServiceProvider = function(impl_name) {
  var existsSync = fs.existsSync || path.existsSync;
  var impl_module_file = path.basename(impl_name) + "_service_provider.js";
  var externalModule = path.join(impl_name, impl_module_file);
  var internalModule = path.join(conf.spi_dir, impl_name, impl_module_file);
  var isInternalImpl = existsSync(internalModule);
  var service, error;
  
  if(isInternalImpl) {
    service = require(internalModule);
  }
  else {
    try {
      service = require(externalModule);
    }
    catch(e) {
      error = new Error("getDBServiceProvider: provider " + impl_name + 
                         " does not exist.");
      error.cause = e;
      throw error;
    }
  }

  /* Now verify that the module can load its dependencies.  
     This will throw an exception if it fails.
  */
  service.loadRequiredModules();  
  
  return service;
};
