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

"use strict";
/*global integraltypes,verify_integraltypes,fail_verify_integraltypes*/

var udebug = unified_debug.getLogger("integraltypes/lib.js");

/** The integraltypes domain object */
global.integraltypes = function(id, ttinyint, tsmallint, tmediumint, tint, tbigint) {
  if (typeof(id) !== 'undefined') this.id = id;
  if (typeof(ttinyint) !== 'undefined') this.ttinyint = ttinyint;
  if (typeof(tsmallint) !== 'undefined') this.tsmallint = tsmallint;
  if (typeof(tmediumint) !== 'undefined') this.tmediumint = tmediumint;
  if (typeof(tint) !== 'undefined') this.tint = tint;
  if (typeof(tbigint) !== 'undefined') this.tbigint = tbigint;
};

/** The integraltypes key */
global.integraltypes_key = function(id) {
  this.id = id;
};

/** map integraltypes domain object */
var tablemapping = new mynode.TableMapping("test.integraltypes");
tablemapping.mapField("id");
tablemapping.mapField("ttinyint", "ttinyint");
tablemapping.mapField("tsmallint", "tsmallint");
tablemapping.mapField("tmediumint", "tmediumint");
tablemapping.mapField("tint", "tint");
tablemapping.mapField("tbigint", "tbigint");
tablemapping.applyToClass(global.integraltypes);

global.integraltypes.prototype.getId = function() {return this.id;};

/** Verify the instance or append an error message to the test case */
global.verify_integraltypes = function(err, instance, id, testCase, domainObject) {
  if (err) {
    testCase.appendErrorMessage(err);
    return;
  }
  if (typeof(instance) !== 'object') {
    testCase.appendErrorMessage('Result for id ' + id + ' is not an object; actual type: ' + typeof(instance));
  }
  if (instance === null) {
    testCase.appendErrorMessage('Result for id ' + id + ' is null.');
    return;
  }
  if (domainObject) {
    if (typeof(instance.getId) !== 'function') {
      testCase.appendErrorMessage('Result for id ' + id + ' is not a domain object');
      return;
    }
  }
  udebug.log_detail('instance for id ', id, ':', instance);
  testCase.errorIfNotEqual('fail to verify id', id, instance.id);
  testCase.errorIfNotEqual('fail to verify ttinyint', id, instance.ttinyint);
  testCase.errorIfNotEqual('fail to verify tsmallint', id, instance.tsmallint);
  testCase.errorIfNotEqual('fail to verify tmediumint', id, instance.tmediumint);
  testCase.errorIfNotEqual('fail to verify tint', id, instance.tint);
  testCase.errorIfNotEqual('fail to verify tbigint of type ' + typeof(instance.tbigint), id + '', instance.tbigint);
};

global.fail_verify_integraltypes_array = function(err, instances, ids, testCase, domainObject) {
  var i;
  if (err) {
    testCase.appendErrorMessage(err);
  } else if (instances.length !== ids.length) {
    testCase.appendErrorMessage(
        'Mismatch in length of result; expected: ' + ids.length + '; actual: ' + instances.length);
  } else {
    for (i = 0; i < ids.length; ++i) {
      // verify each instance in the result array
      verify_integraltypes(err, instances[i], ids[i], testCase, domainObject);
    }
  }
  // fail if any failures
  testCase.failOnError();
};

/** Verify the instance or fail the test case */
global.fail_verify_integraltypes = function(err, instance, id, testCase, domainObject) {
  if (err) {
    testCase.fail(err);
    return;
  }
  if (typeof(instance) !== 'object') {
    testCase.fail(new Error('Result for id ' + id + ' is not an object; actual type: ' + typeof(instance)));
    return;
  }
  if (instance === null) {
    testCase.fail(new Error('Result for id ' + id + ' is null.'));
    return;
  }
  if (domainObject) {
    if (typeof(instance.getId) !== 'function') {
      testCase.fail(new Error('Result for id ' + id + ' is not a domain object'));
      return;
    }
  }
  udebug.log_detail('instance:', instance);
  var message = '';
  if (instance.id != id) {
    message += 'fail to verify id: expected: ' + id + ', actual: ' + instance.id + '\n';
  }
  if (instance.ttinyint != id) {
    message += 'fail to verify ttinyint: expected: ' + id + ', actual: ' + instance.ttinyint + '\n';
  }
  if (instance.tsmallint != id) {
    message += 'fail to verify tsmallint: expected: ' + id + ', actual: ' + instance.tsmallint + '\n';
  }
  if (instance.tmediumint != id) {
    message += 'fail to verify tmediumint: expected: ' + id + ', actual: ' + instance.tmediumint + '\n';
  }
  if (instance.tint != id) {
    message += 'fail to verify tint: expected: ' + id + ', actual: ' + instance.tint + '\n';
  }
  if (instance.tbigint != id) {
    message += 'fail to verify tbigint: expected: ' + id + ', actual: ' + instance.tbigint + '\n';
  }
  if (message !== '') {
    testCase.appendErrorMessage(message);
  }
  if (testCase.session) {
    testCase.session.close(function(err) {
      if (err) {
        testCase.appendErrorMessage(err);
      }
      testCase.session = null;
      testCase.failOnError();
    });
  }
};
