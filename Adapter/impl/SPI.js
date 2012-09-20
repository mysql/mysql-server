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

/* 1: Require adapter_config.js. 
      This imports path, fs, assert, util, and unified_debug modules globally.
      It also sets some global location variables: adapter_dir, spi_dir, etc.
      
      adapter_config is required here, rather than at the top level of the api,
      so that the spi can be used independently of the api.  
*/
require("../adapter_config.js");

exports.getDBServiceProvider = function(impl_name) {
  var module = path.join(spi_dir, impl_name, impl_name + "_service_provider.js");
  var service;

  /* If the module file does not exist, throw an error.
  */  
  if(! path.existsSync(module)) {
    throw new Error('Unknown service provider "' + impl_name + '"');
  }

  
  


  return spi;
};
