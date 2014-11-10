/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

/** This file is copied from http://wiki.ecmascript.org/doku.php?id=harmony:proxies.
 * The page is based on Harmony proxies that are considered obsolete. Once proxies
 * are standardized in ECMAScript 6, this file will need to be rewritten.
 */
/*global Proxy, assert, path, api_doc_dir, unified_debug */

"use strict";

var udebug       = unified_debug.getLogger("ProxyFactory.js");

function handlerMaker (obj) {
  return {
   getOwnPropertyDescriptor: function(name) {
     var desc = Object.getOwnPropertyDescriptor(obj, name);
     // a trapping proxy's properties must always be configurable
     if (desc !== undefined) { desc.configurable = true; }
     return desc;
   },
   getPropertyDescriptor:  function(name) {
     var desc = Object.getPropertyDescriptor(obj, name); // not in ES5
     // a trapping proxy's properties must always be configurable
     if (desc !== undefined) { desc.configurable = true; }
     return desc;
   },
   getOwnPropertyNames: function() {
     return Object.getOwnPropertyNames(obj);
   },
   getPropertyNames: function() {
     return Object.getPropertyNames(obj);                // not in ES5
   },
   defineProperty: function(name, desc) {
     Object.defineProperty(obj, name, desc);
   },
   delete:       function(name) { return delete obj[name]; },   
   fix:          function() {
     if (Object.isFrozen(obj)) {
       var result = {};
       Object.getOwnPropertyNames(obj).forEach(function(name) {
         result[name] = Object.getOwnPropertyDescriptor(obj, name);
       });
       return result;
     }
     // As long as obj is not frozen, the proxy won't allow itself to be fixed
     return undefined; // will cause a TypeError to be thrown
   },

   /* ignore jslint error; 'in' really is what we want */
   has:          function(name) { return name in obj; },
   hasOwn:       function(name) { return ({}).hasOwnProperty.call(obj, name); },
   get:          function(receiver, name) {
     if (!obj.hasOwnProperty(name)) {
       udebug.log('ProxyFactory.handlerMaker.get fail to get ', name);
       obj.failGet(name);
     }
     return obj[name];
   },
   set:          function(receiver, name, val) { obj[name] = val; return true; }, // bad behavior when set fails in non-strict mode
   /* ignore jslint error; 'in' really is what we want */
   enumerate:    function() {
     var result = [], name;
     for (name in obj) { result.push(name); }
     return result;
   },
   keys: function() { return Object.keys(obj); }

  };
}
/* usage:
var proxyFactory = require("ProxyFactory.js");
var proxy = proxyFactory.createProxy(obj);
 */
function createProxy(obj) {
  var handler, proxy;
  handler = handlerMaker(obj);
  try {
    return Proxy.create(handler);
  } catch (e) {
    console.log('ProxyFactory.createProxy Proxy.createProxy threw', e.message);
    throw new Error('Proxy is not defined. Please start node with --harmony option.');
  }
}

module.exports = createProxy;
