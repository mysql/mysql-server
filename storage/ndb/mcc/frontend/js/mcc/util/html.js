/*
Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
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
 *      mcc.util.html.getClusterUrlRoot: Return the root url for download links
 *      mcc.util.html.getClusterDockerUrl: Return the url for downloading
 *          Docker image.
 *      mcc.util.html.setClusterVersion: Set to 7.6, 8.0, ... from ClusterSetup page.
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
 *      clusterver: "8.0", "7.6" etc for forming URL's (help and download links)
 *
 *  Unit test interface:
 *      mcc.util.html.setPropertyDefault: See setPropertyDefault
 *
 *  Todo:
 *      Exception handling
 *
 ******************************************************************************/

/******************************* Import/export ********************************/
dojo.provide('mcc.util.html');

dojo.require('dijit.form.ToggleButton');
dojo.require('dijit.form.TextBox');
dojo.require('dijit.form.CheckBox');
dojo.require('dijit.form.NumberSpinner');
dojo.require('dijit.Tooltip');
dojo.require('dijit.form.FilteringSelect');
dojo.require('dojox.validate');

dojo.require('mcc.util');

/***************************** External interface *****************************/
mcc.util.html.startTable = startTable;
mcc.util.html.tableRow = tableRow;
mcc.util.html.endTable = endTable;
mcc.util.html.setupWidgets = setupWidgets;
mcc.util.html.updateWidgets = updateWidgets;
mcc.util.html.getDocUrlRoot = getDocUrlRoot;
mcc.util.html.getClusterUrlRoot = getClusterUrlRoot;
mcc.util.html.getClusterDockerUrl = getClusterDockerUrl;
mcc.util.html.setClusterVersion = setClusterVersion;

/******************************* Internal data ********************************/
var propertyDefault = [];
var backgroundColor = false;
var clusterVer;

/******************************* Implementation *******************************/
function setClusterVersion (ver) {
    clusterVer = ver;
    console.debug('[DBG]Cluster version set to ' + clusterVer);
}

// Get root url to documentation
function getDocUrlRoot () {
    if (clusterVer === '8.0') {
        return 'https://dev.mysql.com/doc/refman/8.0/en/';
    } else {
        if (clusterVer === '7.6') {
            return 'https://dev.mysql.com/doc/refman/5.7/en/';
        } else {
            // 5.7-7.5
            return 'https://dev.mysql.com/doc/refman/5.7/en/';
        }
    }
}

function getClusterUrlRoot () {
    if (clusterVer === '8.0') {
        return 'http://repo.mysql.com/mysql80-community-release';
    } else {
        if (clusterVer === '7.6') {
            return 'http://repo.mysql.com/mysql57-community-release';
        } else {
            // 5.7-7.5
            return 'http://repo.mysql.com/mysql57-community-release';
        }
    }
}

function getClusterDockerUrl () {
    if (clusterVer === '8.0') {
        return 'mysql/mysql-cluster:8.0';
    } else {
        if (clusterVer === '7.6') {
            return 'mysql/mysql-cluster:7.6';
        } else {
            // 5.7-7.5
            return 'mysql/mysql-cluster:7.6';
        }
    }
}

// Set property default
function setPropertyDefault (key, value) {
    propertyDefault[key] = value;
}

// Get property default
function getPropertyDefault (key) {
    return propertyDefault[key];
}

// Start an html table. Reset background color
function startTable () {
    backgroundColor = false;
    return '<table width="100%" cellspacing="0">';
}

// Add a 3-col row. Generate ids for all fields. Link to url, assign tooltip
function tableRow (prefix, label, url, attribute, tooltip) {
    backgroundColor = !backgroundColor;
    return '<tr style="' + (backgroundColor ? 'background-color: #f4f6f8' : '') + '; "><td width="28%">' +
        (url ? "<label for='" + prefix + attribute + "'><a href=\"" + url + '" target="_blank">' +
        label + '</a></label>' : label) + "\
        <span class='helpIcon' id=\"" + prefix + attribute + '_qm">\
            ' + (tooltip ? '[?]' : '') + '</span>\
        </td>\
        <td width="62%">\
            <div id="' + prefix + attribute + '"></div>\
        </td>\
        <td width="10%"><div id="' + prefix + attribute + '_ctrl">\
        </div></td></tr>';
}

// End an html table
function endTable () {
    return '</table>';
}

// Setup widgets for a table row: Editable field, toggle button for control
function setupWidgets (setAttribute, deleteAttribute, prefix, attribute, widget,
    width, overridable, tooltip, constraints, started, running) {
    // Create a tooltip widget and connect it to the question mark
    var tt = new dijit.Tooltip({
        connectId: [prefix + attribute + '_qm'],
        label: tooltip
    });
    // If the field is overridable, generate widgets
    if (overridable === undefined || overridable) {
        // Setup the editable field
        var editableField = new widget({
            style: 'width: ' + width + '; ',
            intermediateChanges: true,
            disabled: true
        }, prefix + attribute);

        // Add constraints if NumberSpinner
        if (widget === dijit.form.NumberSpinner) {
            if (constraints) {
                editableField.set('constraints', constraints);
            } else {
                editableField.set('constraints', { min: 0, places: 0, pattern: '#' });
            }
        }

        // Add constraints if FilteringSelect
        if (widget === dijit.form.FilteringSelect) {
            if (constraints) {
                var re = /\s*,\s*/;
                var splitConstraints = constraints.split(re);
                var options = [];
                for (var j = 0; j < splitConstraints.length; j++) {
                    var val = splitConstraints[j];
                    var lab = splitConstraints[j];
                    options.push({ label: lab, value: val, selected: false });
                }
                editableField.set('labelAttr', 'label');
                editableField.set('searchAttr', 'value');
                editableField.set('idProperty', 'value');
                editableField.store.setData(options);
            }
        }

        // Setup an edit control button
        var controlButton = new dijit.form.ToggleButton({
            baseClass: 'iconButton',
            checked: false,
            iconClass: 'dijitIconAdd',
            showLabel: false
        }, prefix + attribute + '_ctrl');

        // Create a tooltip widget and connect it to control button
        var control_tt = new dijit.Tooltip({
            connectId: [prefix + attribute + '_ctrl'],
            label: 'Override predefined setting'
        });

        // Handle onChange events for the editable field
        dojo.connect(editableField, 'onChange', function (val) {
            // The edit control must be set to true
            if (controlButton.get('checked') && setAttribute) {
                var value = this.get('value');
                if (widget === dijit.form.FilteringSelect) {
                    value = dijit.byId(prefix + attribute).getValue();
                    setAttribute(attribute, value);
                } else {
                    // For checkboxes, replace "on" by true
                    if (widget === dijit.form.CheckBox && value === 'on') {
                        value = true;
                    }
                    if (!editableField.get('constraints') || editableField.validate()) {
                        setAttribute(attribute, value);
                    }
                }
            }
        });

        // Handle onChange events for the edit control button
        dojo.connect(controlButton, 'onChange', function (val) {
            var defV = getPropertyDefault(prefix + attribute) || '';
            if (!val) {
                // Editing disabled - button indicates clicking enables edit
                this.set('iconClass', 'dijitIconAdd');
                control_tt.set('label', 'Override predefined setting');
                // Blur to remove tooltip
                dojo.byId(this.id).blur();
                // Disable the edit field
                editableField.set('disabled', true);
                // Revert value to predefined
                if (widget === dijit.form.FilteringSelect) {
                    dijit.byId(prefix + attribute).textbox.value = defV;
                    // Unset the previously edited attribute and save
                    if (deleteAttribute) {
                        deleteAttribute(attribute);
                    }
                } else {
                    // there is no value to begin with
                    editableField.set('value', defV);
                    // Unset the previously edited attribute and save
                    if (deleteAttribute) {
                        deleteAttribute(attribute);
                    }
                }
            } else {
                // Editing enabled - button shows that clicking reverts. we have "val" and "defV"
                // in case we need comparison
                this.set('iconClass', 'dijitIconDelete');
                control_tt.set('label', 'Revert to predefined setting');
                // Blur to remove tooltip
                dojo.byId(this.id).blur();
                // If field disabled AND Cluster is not running, enable and set value to predefined
                if (editableField.get('disabled') && !running) {
                    editableField.set('disabled', false);
                    // there could be special ones among these in the future...
                    if (widget === dijit.form.FilteringSelect) {
                        dijit.byId(prefix + attribute).textbox.value = defV;
                        // Must trigger onchange in case val already set
                        editableField.onChange();
                    } else {
                        // force defaults on all, let rest of code figure out how dangerous this is
                        editableField.set('value', defV);
                        // Must trigger onchange in case val already set
                        editableField.onChange();
                    }
                }
            }
        });
    // For a non-overridable field, just generate a plain text box
    } else {
        var textBox = new dijit.form.TextBox({
            style: 'width: ' + width + '; ',
            disabled: true
        }, prefix + attribute);
    }
}

// Update widgets based on selection
function updateWidgets (getAttribute, prefix, attribute, defaultValue, started, running) {
    // Get the stored attribute value
    var storedVal = getAttribute(attribute);
    var fsWid = dijit.byId(prefix + attribute);
    // The field is overridable if we have a ctrl id. If so, upd. widgets belonging to _ctrl button.
    if (dijit.byId(prefix + attribute + '_ctrl')) {
        defaultValue = defaultValue || '';

        // Save default value
        setPropertyDefault(prefix + attribute, defaultValue);
        // If the item has attr value, display it, enable check ctrl
        if (storedVal) { // !== undefined
            // this is where "special" parameters will end up since they are always saved
            dijit.byId(prefix + attribute + '_ctrl').set('checked', true);
            dijit.byId(prefix + attribute).set('disabled', false);
            if (fsWid.declaredClass === 'dijit.form.FilteringSelect') {
                // console.debug('[DBG]' + (prefix + attribute) + ' as FiltSel, setting value to ' + defaultValue);
                dijit.byId(prefix + attribute).textbox.value = storedVal;
            } else {
                // console.debug('[DBG]' + (prefix + attribute) + ' as something, setting value to ' + defaultValue);
                dijit.byId(prefix + attribute).set('value', storedVal);
            }
            // console.debug('[DBG]There was value for ' + attribute + ', val:' + storedVal + ', default:' + defaultValue);
            //  && String(attribute) !== 'NoOfFragmentLogFiles' && String(attribute) !== 'FragmentLogFileSize' || String(attribute) === 'NoOfFragmentLogParts'
            if (String(attribute) === 'NoOfReplicas') {
                // if special, just warn
                // pt_data_NoOfReplicas
                if (String(storedVal) !== String(defaultValue)) {
                    dojo.style(fsWid.domNode, 'background', 'red');
                    dojo.style('pt_data_' + attribute, 'color', 'white'); //.domNode
                } else {
                    dojo.style(fsWid.domNode, 'background', 'inherit');
                    dojo.style('pt_data_' + attribute, 'color', 'inherit'); // .domNode
                }
                // do not bother unless absolutely necessary
                if (String(storedVal) !== String(defaultValue) && started) {
                    var msg = 'For ' + attribute + ' actual value(' + storedVal + ') is different ' +
                        'from recommended value (' + (defaultValue || '-') + ')!';
                    console.warn('[WRN]' + msg);
                    mcc.util.displayModal('H', 3, '<span style="font-size:135%;color:orangered;">' +
                        msg + '</span>', '<span style="font-size:150%;color:red">Configuration ' +
                        'might not be valid.</span>', '');
                }
            }
        } else {
            // No user-modified value.
            // If the item has no attr value, show default, disable, uncheck ctrl
            // There are values which should be written into config at all times
            // to, possibly, prevent user from changing them thus invalidating
            // Cluster configuration. Ports should also go here...
            // removed last 2, && String(attribute) !== 'NoOfFragmentLogFiles'
            // && String(attribute) !== 'FragmentLogFileSize'
            if (fsWid.declaredClass === 'dijit.form.FilteringSelect') {
                // console.debug('[DBG]' + (prefix + attribute) + ' as FilteringSelect, setting value to ' + defaultValue);
                dijit.byId(prefix + attribute).textbox.value = defaultValue;
            } else {
                dijit.byId(prefix + attribute).set('value', defaultValue);
            }
            // all we need to do here is make sure special params end up written to configuration
            // file by not disabling CheckButton. && String(attribute) !== 'NoOfFragmentLogParts'
            if (String(attribute) !== 'NoOfReplicas') {
                dijit.byId(prefix + attribute + '_ctrl').set('checked', false);
                dijit.byId(prefix + attribute).set('disabled', true);
            } else {
                setPropertyDefault(prefix + attribute, null);
                dijit.byId(prefix + attribute + '_ctrl').set('checked', true);
                dijit.byId(prefix + attribute).set('disabled', false);
            }
        }
    // Show attr value for a non-overridable field without default (e.g. nodeid)
    } else if (defaultValue === undefined) {
        if (fsWid.declaredClass === 'dijit.form.FilteringSelect') {
            dijit.byId(prefix + attribute).textbox.value = storedVal;
        } else {
            dijit.byId(prefix + attribute).set('value', storedVal);
        }
    // Show default value for a non-overridable field
    } else {
        // console.log(attribute + ', default:' + (defaultValue || '-') + ', actual:' + (storedVal || '-'));
        if (fsWid.declaredClass === 'dijit.form.FilteringSelect') {
            dijit.byId(prefix + attribute).textbox.value = defaultValue;
        } else {
            dijit.byId(prefix + attribute).set('value', defaultValue);
        }
    }
}

/**************************** Unit test interface *****************************/
if (mcc.util.tests) {
    // Export function to set property defaults
    mcc.util.html.setPropertyDefault = setPropertyDefault;
}

/********************************* Initialize *********************************/
dojo.ready(function () {
    console.info('[INF]HTML module initialized');
});
