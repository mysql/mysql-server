/*
Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/******************************************************************************
 ***                                                                        ***
 ***                    GUI unit tests: HTML utilities                      ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.tests.html_gui
 *
 *  Test cases: 
 *      Positive: 
 *          posVerifyWidgets: Verify correct widgets are setup for a table row
 *          posVerifyOverride: Verify correct overriding behavior
 *          posVerifyNonOverridable: Verify correct setup of a non-overridable
 *          posVerifyRevert: Verify correct revert behavior
 *
 *      Negative: 
 *
 *  Todo: 
 *      Implement negative test cases
 *
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.tests.html_gui");

dojo.require("dijit.form.NumberSpinner");
dojo.require("dijit.form.TextBox");

dojo.require("mcc.util");

/******************************* Test cases  **********************************/

// Verify that the correct widgets are setup for a table row
function posVerifyWidgets() {
    var pre = "pre1_";
    var attr = "my_attr"; 

    // Create a content string
    var contentString = mcc.util.startTable();
    contentString += mcc.util.tableRow(pre, "url", "label", attr, "tooltip");
    contentString += mcc.util.endTable();
    dojo.byId("posVerifyWidgets").innerHTML = contentString;

    // Setup widgets to fill the row
    mcc.util.setupWidgets(null, null, pre, attr, dijit.form.NumberSpinner, 
            "50%", true);

    // Dijit should be a NumberSpinner
    mcc.util.tst("Check edit field: " + dijit.byId(pre + attr).declaredClass);
    doh.t(dijit.byId(pre + attr).declaredClass == "dijit.form.NumberSpinner");

    // Dijit on ctrl id should be a ToggleButton
    mcc.util.tst("Check ctrl button: " + 
            dijit.byId(pre + attr + "_ctrl").declaredClass);
    doh.t(dijit.byId(pre + attr + "_ctrl").declaredClass == 
            "dijit.form.ToggleButton");
}

// Verify that the widgets interact correctly when overriding a value
function posVerifyOverride() {
    var pre = "pre2_";
    var attr = "my_attr";
    var enabledVal = "enabled_val";
    var disabledVal = "disabled_val";
    var defaultVal = "default_val";

    // Return postcondition for doh to check
    var postCondition = new doh.Deferred();

    // First expect defaultVal, then enabledVal
    var expectedVal = defaultVal;
    function setAttr(upAttr, upVal) {
        mcc.util.tst("Set attr received: " + upAttr + "=" + upVal);
        if (upAttr == attr && upVal == expectedVal) {
            if (expectedVal == defaultVal) expectedVal = enabledVal;
            else if (expectedVal == enabledVal) postCondition.callback(true);
            else postCondition.errback(false);
        } else {
            postCondition.errback(false);
        }
    }

    // Create content string, add to document
    var contentString = mcc.util.startTable();
    contentString += mcc.util.tableRow(pre, "url", "label", attr, "tooltip");
    contentString += mcc.util.endTable();
    dojo.byId("posVerifyOverride").innerHTML= contentString;

    // Setup widgets to fill the row
    mcc.util.setupWidgets(setAttr, null, pre, attr, 
            dijit.form.TextBox, "50%", true);

    // Set default value which will be assigned when ctrl checked
    mcc.util.html.setPropertyDefault(pre + attr, defaultVal);

    // Verify ctrl button not checked
    mcc.util.tst("Verify control button not checked"); 
    doh.f(dijit.byId(pre + attr + "_ctrl").get("checked"));

    // Connect to onChange, resolve deferred to synchronize
    var afterOnChange = new dojo.Deferred();
    dojo.connect(dijit.byId(pre + attr), "onChange", function (val) {
        afterOnChange.resolve(val);
    });

    // Update value of editable field
    dijit.byId(pre + attr).set("value", disabledVal);
    mcc.util.tst("Update and check value of editable field: " + 
            dijit.byId(pre + attr).get("value")); 
    doh.t(dijit.byId(pre + attr).get("value") == disabledVal); 

    // Wait for onChange to be triggered
    mcc.util.tst("Wait for first onChange event to be processed");
    afterOnChange.then(function (val) {
        mcc.util.tst("OnChange done for " + val);

        // Reset deferred
        afterOnChange = new dojo.Deferred();

        // Check button - will implicitly set default
        mcc.util.tst("Check and verify control button"); 
        dijit.byId(pre + attr + "_ctrl").set("checked", true);
        doh.t(dijit.byId(pre + attr + "_ctrl").get("checked"));

        mcc.util.tst("Wait for second onChange event to be processed");
        afterOnChange.then(function (val) {
            mcc.util.tst("OnChange done for " + val);

            // Reset deferred
            afterOnChange = new dojo.Deferred();

            // Update value of editable field
            dijit.byId(pre + attr).set("value", enabledVal);

            // Wait for onChange to be triggered
            mcc.util.tst("Wait for third onChange event to be processed");
            afterOnChange.then(function (val) {
                mcc.util.tst("OnChange done for " + val);
            });
        });
    });
    // Probably already resolved
    return postCondition;
}

// Verify that the correct widgets are set up for a non-overridable field
function posVerifyNonOverridable() {
    var pre = "pre3_";
    var attr = "my_attr";

    // Create content string, add to document
    var contentString = mcc.util.startTable();
    contentString += mcc.util.tableRow(pre, "url", "label", attr, "tooltip");
    contentString += mcc.util.endTable();
    dojo.byId("posVerifyNonOverridable").innerHTML= contentString;

    // Setup widgets to fill the row
    mcc.util.setupWidgets(null, null, pre, attr, 
            dijit.form.NumberSpinner, "50%", false);

    // Verify that the widget is a disabled text box
    mcc.util.tst("Widget " + pre + attr + " should be a TextBox");
    doh.t(dijit.byId(pre + attr).declaredClass == "dijit.form.TextBox");
    mcc.util.tst("Widget " + pre + attr + " should be disabled");
    doh.t(dijit.byId(pre + attr).get("disabled"));

    // Verify that the control widget is undefined
    mcc.util.tst("Widget " + pre + attr + "_ctrl should be undefined");
    doh.t(dijit.byId(pre + attr + "_ctrl") === undefined);
}

// Verify that the widgets interact correctly when reverting a value
function posVerifyRevert() {
    var pre = "pre4_";
    var attr = "my_attr";
    var enabledVal = "enabled_val";
    var defaultVal = "default_val";

    // Return postcondition for doh to check
    var postCondition = new doh.Deferred();

    // Sycnh point for delete attr
    var afterDelete = new dojo.Deferred();

    // Callback to delete attribute
    function deleteAttr(delAttr) {
        mcc.util.tst("Delete attr received: " + delAttr);
        if (delAttr == attr) {
            afterDelete.resolve(true);
        } else {
            afterDelete.resolve(false);
        }
    }

    // Create content string, add to document
    var contentString = mcc.util.startTable();
    contentString += mcc.util.tableRow(pre, "url", "label", attr, "tooltip");
    contentString += mcc.util.endTable();
    dojo.byId("posVerifyRevert").innerHTML= contentString;

    // Setup widgets to fill the row
    mcc.util.setupWidgets(null, deleteAttr, pre, attr, 
            dijit.form.TextBox, "50%", true);

    // Set default value which will be assigned when ctrl checked
    mcc.util.html.setPropertyDefault(pre + attr, defaultVal);

    // Check button - will implicitly set field to default
    mcc.util.tst("Check and verify control button"); 
    dijit.byId(pre + attr + "_ctrl").set("checked", true);
    doh.t(dijit.byId(pre + attr + "_ctrl").get("checked"));

    // Temporarily set different value and verify
    mcc.util.tst("Edit field"); 
    dijit.byId(pre + attr).set("value", enabledVal);
    doh.t(dijit.byId(pre + attr).get("value") == enabledVal);

    // Uncheck button - should delete attribute
    mcc.util.tst("Uncheck and verify control button"); 
    dijit.byId(pre + attr + "_ctrl").set("checked", false);
    doh.f(dijit.byId(pre + attr + "_ctrl").get("checked"));

    // Verify field value is now default
    afterDelete.then(function (val) {
        mcc.util.tst("Field value after uncheck: " + 
                dijit.byId(pre + attr).get("value"));
        if (val && (dijit.byId(pre + attr).get("value") == defaultVal)) {
            postCondition.callback(true);
        } else {
            postCondition.errback(false);
        } 
    });
    return postCondition; 
}

// Verify that updating widgets behave correctly
function posVerifyUpdateOverridable() {
    var pre = "pre5_";
    var attr = "my_attr";
    var overridingVal = "overriding_val";
    var defaultVal = "default_val";

    // Return postcondition for doh to check
    var postCondition = new doh.Deferred();

    var currVal = undefined;
    var getAttrCalled = false;
    var setAttrCalled = false;

    // Get attribute
    function getAttr() {
        mcc.util.tst("getAttr returning: " + currVal);
        getAttrCalled = true; 
        return currVal;
    }

    // Set attribute
    function setAttr(upAttr, upVal) {
        mcc.util.tst("setAttr receiving: " + upAttr + "=" + upVal);
        setAttrCalled = true; 
    }

    // Create content string, add to document
    var contentString = mcc.util.startTable();
    contentString += mcc.util.tableRow(pre, "url", "label", attr, "tooltip");
    contentString += mcc.util.endTable();
    dojo.byId("posVerifyUpdateOverridable").innerHTML= contentString;

    // Setup widgets to fill the row
    mcc.util.setupWidgets(setAttr, null, pre, attr, 
            dijit.form.TextBox, "50%", true);

    // Connect to onChange, resolve deferred to synchronize
    var afterOnChange = new dojo.Deferred();
    dojo.connect(dijit.byId(pre + attr), "onChange", function (val) {
        afterOnChange.resolve(val);
    });

    // Update overridable with undefined val should set field to default
    mcc.util.updateWidgets(getAttr, pre, attr, defaultVal);

    // Wait for onChange to be triggered
    mcc.util.tst("Wait for first onChange event to be processed");
    afterOnChange.then(function (val) {
        mcc.util.tst("OnChange done for " + val);

        // Verify control button unchecked
        mcc.util.tst("Verify control button unchecked"); 
        doh.f(dijit.byId(pre + attr + "_ctrl").get("checked"));

        // Verify field value is default
        mcc.util.tst("Verify field value is default"); 
        doh.t(dijit.byId(pre + attr).get("value") == defaultVal);

        // Verify set attr not called
        mcc.util.tst("Verify setAttr not called"); 
        doh.f(setAttrCalled);

        // Verify get attr called
        mcc.util.tst("Verify getAttr called"); 
        doh.t(getAttrCalled);

        // Reset control variables
        getAttrCalled = false; 
        setAttrCalled = false; 
        afterOnChange = new dojo.Deferred();

        // Update overridable with defined val should set field to val
        currVal = overridingVal; 
        mcc.util.updateWidgets(getAttr, pre, attr, defaultVal);

        // Wait for onChange to be triggered
        mcc.util.tst("Wait for second onChange event to be processed");
        afterOnChange.then(function (val) {
            mcc.util.tst("OnChange done for " + val);

            // Verify control button checked
            mcc.util.tst("Verify control button checked"); 
            doh.t(dijit.byId(pre + attr + "_ctrl").get("checked"));

            // Verify field value is updated
            mcc.util.tst("Verify field value is overriding val"); 
            doh.t(dijit.byId(pre + attr).get("value") == overridingVal);

            // Verify set attr called
            mcc.util.tst("Verify setAttr called"); 
            doh.t(setAttrCalled);

            // Verify get attr called
            mcc.util.tst("Verify getAttr called"); 
            doh.t(getAttrCalled);

            // If we got this far, it's ok
            postCondition.callback(true);
        });
    });
    return postCondition; 
}

// Verify that updating widgets behave correctly
function posVerifyUpdateNonOverridable() {
    var pre = "pre6_";
    var attr = "my_attr";
    var overridingVal = "overriding_val";
    var defaultVal = "default_val";

    // Return postcondition for doh to check
    var postCondition = new doh.Deferred();

    var currVal = undefined;
    var getAttrCalled = false;
    var setAttrCalled = false;

    // Get attribute
    function getAttr() {
        mcc.util.tst("getAttr returning: " + currVal);
        getAttrCalled = true; 
        return currVal;
    }

    // Set attribute
    function setAttr(upAttr, upVal) {
        mcc.util.tst("setAttr receiving: " + upAttr + "=" + upVal);
        setAttrCalled = true; 
    }

    // Create content string, add to document
    var contentString = mcc.util.startTable();
    contentString += mcc.util.tableRow(pre, "url", "label", attr, "tooltip");
    contentString += mcc.util.endTable();
    dojo.byId("posVerifyUpdateNonOverridable").innerHTML= contentString;

    // Setup widgets to fill the row
    mcc.util.setupWidgets(setAttr, null, pre, attr, 
                      dijit.form.TextBox, "50%", false);

    // Connect to onChange, resolve deferred to synchronize
    var afterOnChange = new dojo.Deferred();
    dojo.connect(dijit.byId(pre + attr), "onChange", function (val) {
        afterOnChange.resolve(val);
    });

    // Update non-overridable with undefined default should set field to current
    currVal = "current_value";
    mcc.util.updateWidgets(getAttr, pre, attr, undefined);

    // Wait for onChange to be triggered
    mcc.util.tst("Wait for first onChange event to be processed");
    afterOnChange.then(function (val) {
        mcc.util.tst("OnChange done for " + val);

        // Verify field value is current value
        mcc.util.tst("Verify field value is default"); 
        doh.t(dijit.byId(pre + attr).get("value") == currVal);

        // Verify set attr not called
        mcc.util.tst("Verify setAttr not called"); 
        doh.f(setAttrCalled);

        // Verify get attr called
        mcc.util.tst("Verify getAttr called"); 
        doh.t(getAttrCalled);

        // Reset control variables
        getAttrCalled = false; 
        setAttrCalled = false; 
        afterOnChange = new dojo.Deferred();

        // Update non-overridable with defined default should set field default
        mcc.util.updateWidgets(getAttr, pre, attr, defaultVal);

        // Wait for onChange to be triggered
        mcc.util.tst("Wait for second onChange event to be processed");
        afterOnChange.then(function (val) {
            mcc.util.tst("OnChange done for " + val);

            // Verify field value is updated
            mcc.util.tst("Verify field value is default"); 
            doh.t(dijit.byId(pre + attr).get("value") == defaultVal);

            // Verify set attr not called
            mcc.util.tst("Verify setAttr not called"); 
            doh.f(setAttrCalled);

            // Verify get attr called
            mcc.util.tst("Verify getAttr called"); 
            doh.t(getAttrCalled);

            // If we got this far, it's ok
            postCondition.callback(true);
        });
    });
    return postCondition; 
}

/*************************** Register test cases  *****************************/

doh.register("mcc.util.tests.html_gui", [
    posVerifyWidgets,
    posVerifyOverride,
    posVerifyNonOverridable,
    posVerifyRevert,
    posVerifyUpdateOverridable,
    posVerifyUpdateNonOverridable
]);

