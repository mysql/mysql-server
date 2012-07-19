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

var spi = require("../impl/SPI.js");
var annotations = require("./Annotations.js");
var sessionfactory = require("./SessionFactory.js");

var factories = {};

exports.Annotations = function() {
  return new annotations.Annotations();
};

function spi_connect_sync(properties) {
  var db = spi.getDBServiceProvider(properties.implementation);
  return db.connectSync(properties);  
}

exports.ConnectionProperties = function(name) {
  var sp = spi.getDBServiceProvider(name);
  return sp.getDefaultConnectionProperties();
};


//connect(Properties, Annotations, Function(err, SessionFactory[, ...]) callback[, ...]);
exports.connect = function(properties, annotations, user_callback) {
  var mynode = this;
  var sp = spi.getDBServiceProvider(properties.implementation);
  var factoryKey = sp.getFactoryKey(properties);
  
  var factory = factories[factoryKey];

  var mycallback = function(error, dbconnection) {
    if(! error) {
      factory = new sessionfactory.SessionFactory(factoryKey);
      
      factory.dbconnection = dbconnection;
      factory.properties = properties;
      factory.annotations = annotations;
      factories[factoryKey] = factory;
      console.dir(factories);
    }
    else {
      console.log("Error is: " + error);
    } 
    user_callback(error, factory);
  };

  if(typeof(factory) == 'undefined') {
    sp.connect(properties, mycallback);
  }
  else { 
    user_callback(null, factory);   //todo: extra parameters
  }
};


exports.openSession = function(properties, annotations, callback) {
  exports.connect(properties, annotations, function(err, factory) {
    if(! err) {
      var session = factory.openSession();
    } 
    callback(err, session);   // todo: extra parameters
  });
};





