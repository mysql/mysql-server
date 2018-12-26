/*
Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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
 ***                           HTML utilities                               ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.util.html
 *
 *  Description:
 *      Utilities to generate various html tags and corresponding widgets
 *
 *  External interface: 
 *      mcc.util.html.startTable: Return string for an html table opening tag
 *      mcc.util.html.tableRow: Return string for a 3-col table row
 *      mcc.util.html.endTable: Return string for an html table closing tag
 *      mcc.util.html.setupWidgets: Setup widgets for a table row
 *      mcc.util.html.updateWidgets: Update widgets for a table row
 *      mcc.util.html.getDocUrlRoot: Return the root url for documentation links
 *
 *  External data: 
 *      None
 *
 *  Internal interface: 
 *      setPropertyDefault: Save default value hashed on widget id
 *      getPropertyDefault: Get default value hashed on widget id
 *
 *  Internal data: 
 *      backgroundColor: Alternate background color for rows
 *      propertyDefault: Keep track of default values for overridden fields
 * 
 *  Unit test interface: 
 *      mcc.util.html.setPropertyDefault: See setPropertyDefault
 *
 *  Todo:
 *      Exception handling
 * 
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.util.html");

dojo.require("dijit.form.ToggleButton");
dojo.require("dijit.form.TextBox");
dojo.require("dijit.form.CheckBox");
dojo.require("dijit.form.NumberSpinner");
dojo.require("dijit.Tooltip");
dojo.require("dijit.form.FilteringSelect");
dojo.require("dojox.validate");

dojo.require("mcc.util");

/**************************** External interface  *****************************/

mcc.util.html.startTable = startTable;
mcc.util.html.tableRow = tableRow; 
mcc.util.html.endTable = endTable;
mcc.util.html.setupWidgets = setupWidgets;
mcc.util.html.updateWidgets = updateWidgets; 
mcc.util.html.getDocUrlRoot = getDocUrlRoot;

/******************************* Internal data ********************************/

var propertyDefault = [];
var backgroundColor = false;

/****************************** Implementation  *******************************/

// Get root url to documentation
function getDocUrlRoot() {
    return "https://dev.mysql.com/doc/refman/5.7/en/"; 
}

// Set property default
function setPropertyDefault(key, value) {
    propertyDefault[key] = value;
}

// Get property default
function getPropertyDefault(key) {
    return propertyDefault[key];
}

// Start an html table. Reset background color
function startTable() {
    backgroundColor = false;
    return "<table width=\"100%\" cellspacing=\"0\">";
}

// Add a 3-col row. Generate ids for all fields. Link to url, assign tooltip
function tableRow(prefix, label, url, attribute, tooltip) {
    
    backgroundColor = !backgroundColor;
    return "<tr style=\"" + (backgroundColor?"background-color: #f4f6f8" : "") +
           "; \"><td width=\"28%\">" + 
           (url ? 
               "<label for='" + prefix + attribute + "'><a href=\"" + 
               url + "\" target=\"_blank\">" + 
               label + "</a></label>" : label) + "\
                <span class='helpIcon' id=\"" + prefix + attribute + "_qm\">\
                    " + (tooltip ? "[?]" : "") + "\
                </span>\
           </td>\
           <td width=\"62%\">\
                <div id=\"" + prefix + attribute + "\"></div>\
            </td>\
           <td width=\"10%\"><div id=\"" + prefix + attribute + "_ctrl\">\
           </div></td></tr>";
}

// End an html table
function endTable() {
    return "</table>";
}

// Setup widgets for a table row: Editable field, toggle button for control
function setupWidgets(setAttribute, deleteAttribute, prefix, attribute, widget, 
                      width, overridable, tooltip, constraints) {

    // Create a tooltip widget and connect it to the question mark
    var tt = new dijit.Tooltip({
        connectId: [prefix + attribute + "_qm"],
        label: tooltip
    });

    // If the field is overridable, generate widgets
    if (overridable === undefined || overridable) {
        // Setup the editable field
        var editableField = new widget({
            style: "width: " + width + "; ", 
            intermediateChanges: true,
            disabled: true
        }, prefix + attribute);

        // Add constraints if NumberSpinner
        if (widget == dijit.form.NumberSpinner) {
            if (constraints) {
                editableField.set("constraints", constraints);
            } else {
                editableField.set("constraints", 
                        {min:0, places:0, pattern:"#"});
            }
        }

        // Add constraints if FilteringSelect
        if (widget == dijit.form.FilteringSelect) {
            if (constraints) {
				var re = /\s*,\s*/;
				var splitConstraints = constraints.split(re);
				var options=[];
				for (var j = 0; j < splitConstraints.length; j++) {  
					var val = splitConstraints[j];  
					var lab = splitConstraints[j];  
					options.push({label: lab, value: val, selected:false});
				}  
                editableField.set("labelAttr", "label")
                editableField.set("searchAttr", "value");
                editableField.set("idProperty", "value");
                editableField.store.setData(options);
            }
        }

        // Setup an edit control button
        var controlButton = new dijit.form.ToggleButton({
            baseClass: "iconButton",
            checked: false,
            iconClass: "dijitIconAdd",
            showLabel: false
        }, prefix + attribute + "_ctrl");

        // Create a tooltip widget and connect it to control button
        var control_tt = new dijit.Tooltip({
            connectId: [prefix + attribute + "_ctrl"],
            label: "Override predefined setting"
        });

        // Handle onChange events for the editable field
        dojo.connect(editableField, "onChange", function (val) {
            // The edit control must be set to true
            if (controlButton.get("checked") && setAttribute) {
                var value = this.get("value");
				if (widget == dijit.form.FilteringSelect) {
					value = dijit.byId(prefix + attribute).getValue();
					setAttribute(attribute, value);
				} else {
					// For checkboxes, replace "on" by true
					if (widget == dijit.form.CheckBox && value == "on") {
						value = true;
					}
					if (!editableField.get("constraints") || 
						  editableField.validate()) {
					  setAttribute(attribute, value);
					}
				}
            }
        });

        // Handle onChange events for the edit control button
        dojo.connect(controlButton, "onChange", function (val) {
            if (!val) {
                // Editing disabled - button indicates clicking enables edit
                this.set("iconClass", "dijitIconAdd");
                control_tt.set("label", "Override predefined setting");
                // Blur to remove tooltip
                dojo.byId(this.id).blur();
                // Disable the edit field
                editableField.set("disabled", true);
                // Revert value to predefined
				if (widget == dijit.form.FilteringSelect) {
					var defv = getPropertyDefault(prefix + attribute);
					dijit.byId(prefix + attribute).textbox.value=defv;
				} else {
					editableField.set("value", 
                       getPropertyDefault(prefix + attribute));
				}
                // Unset the previously edited attribute and save
                if (deleteAttribute) {
                    deleteAttribute(attribute);
                }
            } else {
                // Editing enabled - button shows that clicking reverts
                this.set("iconClass", "dijitIconDelete");
                control_tt.set("label", "Revert to predefined setting");
                // Blur to remove tooltip
                dojo.byId(this.id).blur();
                // If field disabled, enable and set value to predefined
                if (editableField.get("disabled")) {
                    editableField.set("disabled", false);
					if (widget == dijit.form.FilteringSelect) {
						var defv = getPropertyDefault(prefix + attribute);
						dijit.byId(prefix + attribute).textbox.value=defv;
					} else {
						editableField.set("value", 
                            getPropertyDefault(prefix + attribute));
					}
                    // Must trigger onchange in case val already set
                    editableField.onChange();
                }
            }
        });
    // For a non-overridable field, just generate a plain text box
    } else {
        var textBox = new dijit.form.TextBox({
            style: "width: " + width + "; ", 
            disabled: true
        }, prefix + attribute);
    }
}

// Update widgets based on selection
function updateWidgets(getAttribute, prefix, attribute, defaultValue) {

    // Get the stored attribute value
    var storedVal = getAttribute(attribute); 
	var fs_wid = dijit.byId(prefix + attribute);

    // The field is overridable if we have a ctrl id. If so, update widgets
    if (dijit.byId(prefix + attribute + "_ctrl")) {
        defaultValue = (defaultValue !== undefined ? defaultValue : "");

		if (fs_wid.declaredClass == "dijit.form.FilteringSelect") {
			dijit.byId(prefix + attribute).textbox.value=defaultValue;
		}
        // Save default value
        setPropertyDefault(prefix + attribute, defaultValue);

        // If the item has attr value, display it, enable check ctrl
        if (storedVal !== undefined) {
            dijit.byId(prefix + attribute + "_ctrl").set("checked", true);
            dijit.byId(prefix + attribute).set("disabled", false);
			if (fs_wid.declaredClass == "dijit.form.FilteringSelect") {
				dijit.byId(prefix + attribute).textbox.value=storedVal;
			} else {			
				dijit.byId(prefix + attribute).set("value", storedVal);
			}
        // If the item has no attr value, show default, disable, uncheck ctrl
        } else {
            dijit.byId(prefix + attribute + "_ctrl").set("checked", false);
            dijit.byId(prefix + attribute).set("disabled", true);
			if (fs_wid.declaredClass == "dijit.form.FilteringSelect") {
				dijit.byId(prefix + attribute).textbox.value=defaultValue;
			} else {			
				dijit.byId(prefix + attribute).set("value", defaultValue);
			}
        }
    // Show attr value for a non-overridable field without default (e.g. nodeid)
    } else if (defaultValue === undefined) {
		if (fs_wid.declaredClass == "dijit.form.FilteringSelect") {
			dijit.byId(prefix + attribute).textbox.value=storedVal;
		} else {			
			dijit.byId(prefix + attribute).set("value", storedVal);
		}
    // Show default value for a non-overridable field
    } else {
		if (fs_wid.declaredClass == "dijit.form.FilteringSelect") {
			dijit.byId(prefix + attribute).textbox.value=defaultValue;
		} else {
			dijit.byId(prefix + attribute).set("value", defaultValue);
		}
    }
}

/**************************** Unit test interface  ****************************/

if (mcc.util.tests) {
    // Export function to set property defaults
    mcc.util.html.setPropertyDefault = setPropertyDefault;
}

/******************************** Initialize  *********************************/

dojo.ready(function () {
    mcc.util.dbg("HTML module initialized");
});


