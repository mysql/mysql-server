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
 ***               Host tree details viewing and manipulation               ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module:
 *      Name: mcc.gui.hosttreedetails
 *
 *  Description:
 *      Functions for displaying and editing details for hosts and processes
 *
 *  External interface:
 *      mcc.gui.hosttreedetails.hostTreeSelectionDetailsSetup: Setup gui
 *      mcc.gui.hosttreedetails.updateHostTreeSelectionDetails: Update details
 *
 *  External data:
 *      None
 *
 *  Internal interface:
 *      hostDefaultsSetup: Assign default values for the attributes
 *      hostSelectionDetailsSetup: Setup widgets for host details
 *      processSelectionDetailsSetup: Setup widgets for process details
 *
 *  Internal data:
 *      None
 *
 *  Unit test interface:
 *      None
 *
 *  Todo:
 *      Implement unit tests.
 *
 ******************************************************************************/

/******************************* Import/export ********************************/
dojo.provide('mcc.gui.hosttreedetails');

dojo.require('dijit.form.FilteringSelect');
dojo.require('dijit.form.Button');
dojo.require('dijit.form.ToggleButton');
dojo.require('dijit.form.NumberSpinner');
dojo.require('dijit.form.TextBox');
dojo.require('dijit.form.CheckBox');
dojo.require('dijit.Tooltip');

dojo.require('mcc.gui');
dojo.require('mcc.util');
dojo.require('mcc.storage');

/***************************** External interface *****************************/
mcc.gui.hosttreedetails.hostTreeSelectionDetailsSetup = hostTreeSelectionDetailsSetup;
mcc.gui.hosttreedetails.updateHostTreeSelectionDetails = updateHostTreeSelectionDetails;

/******************************* Implementation *******************************/
/**
 *Create tooltips for DOM nodes.
 *
 * @param {[String]} cId DOM nodes tooltip connects to.
 * @param {String} lbl text to show.
 * @returns fake
 */
function createTT (cId, lbl) {
    return new dijit.Tooltip({
        connectId: cId,
        label: lbl
    });
}

// Update details for current tree selection
function updateHostTreeSelectionDetails () {
    var hostTreeItem = mcc.gui.getCurrentHostTreeItem();
    if (!hostTreeItem.treeItem || !mcc.storage.hostTreeStorage().isItem(hostTreeItem.treeItem) ||
        hostTreeItem.treeItem.isType('anyHost') || hostTreeItem.treeItem.isType('host')) {
        // If host or no tree item is selected, no details are shown
        dijit.byId('hostTreeSelectionDetails').selectChild(dijit.byId('noHostTreeSelectionDetails'));
        // But we still need to set focus accordingly if a host is selected
        if (hostTreeItem.treeItem &&
            (hostTreeItem.treeItem.isType('anyHost') || hostTreeItem.treeItem.isType('host'))) {
            mcc.gui.hostTreeSetPath(['root', '' + hostTreeItem.storageItem.getId()]);
        } else {
            console.debug('[DBG]No tree item selected');
        }
    } else if (hostTreeItem.treeItem.isType('process')) {
        // Focus on the selected tree item
        mcc.gui.hostTreeSetPath(['root', '' + hostTreeItem.storageItem.getValue('host'),
            '' + hostTreeItem.storageItem.getId()]);

        // Get process type family and restrict type selection
        mcc.storage.processTypeStorage().getItem(
            hostTreeItem.storageItem.getValue('processtype')).then(function (processtype) {
            dijit.byId('pd_types').set('query', { family: processtype.getValue('family') });
        });

        // Fill in values
        dijit.byId('pd_name').setValue(hostTreeItem.storageItem.getValue('name'));
        dijit.byId('pd_types').setValue(hostTreeItem.storageItem.getValue('processtype'));
        dijit.byId('hostTreeSelectionDetails').selectChild(dijit.byId('processSelectionDetails'));
    }
}

// Setup widgets for process details
function processSelectionDetailsSetup () {
    // Header for details pane
    var processSelectionDetailsHeader = new dojox.grid.DataGrid({
        structure: [{
            name: 'Process property',
            width: '37%'
        },
        {
            name: 'Value',
            width: '63%'
        }]
    }, 'processDetailsHeader');
    processSelectionDetailsHeader.startup();

    // Contents of the details pane
    dijit.byId('processDetails').setContent("\
        <div style=\"height: 20px;\">\
            <span style=\"width: 38%; \">\
                <label for='pd_name'>Process name</label>\
                <span class='helpIcon' id='pd_name_qm'>\
                    [?]\
                </span>\
            </span>\
            <span style=\"width: 62%; float: right\">\
                <div id=\"pd_name\"></div>\
            </span>\
        </div>\
        <div style=\"height: 20px;\">\
            <span style=\"width: 38%; \">\
                <label for='pd_types'>Process type</label>\
                <span class='helpIcon' id='pd_types_qm'>\
                    [?]\
                </span>\
            </span>\
            <span style=\"width: 62%; float: right\">\
                <div id=\"pd_types\"></div>\
            </span>\
        </div>"
    );

    var pdName = new dijit.form.TextBox({ style: 'width: 150px', intermediateChanges: true }, 'pd_name');
    createTT(['pd_name', 'pd_name_qm'], 'Enter a process name for easy recognintion of the process.\
            This is used only within the configurator.');
    dojo.connect(pdName, 'onChange', function () {
        mcc.gui.getCurrentHostTreeItem().storageItem.setValue('name', pdName.getValue());
        mcc.storage.processStorage().save();
    });

    var pdTypes = new dijit.form.FilteringSelect({
        style: 'width: 150px',
        store: mcc.storage.processTypeStorage().store(),
        searchAttr: 'nodeLabel'
    }, 'pd_types');
    createTT(['pd_types', 'pd_types_qm'], 'The process type can be changed to other types that are \
            compatible with the one initially chosen.');
    dojo.connect(pdTypes, 'onChange', function () {
        // Get current selection
        var hostTreeItem = mcc.gui.getCurrentHostTreeItem();
        // Just return if no change
        if (String(hostTreeItem.storageItem.getValue('processtype')) === String(pdTypes.getValue())) {
            return;
        }
        // Get target type
        mcc.storage.processTypeStorage().getItem(pdTypes.getValue()).then(function (ptype) {
            var pname = ptype.getValue('nodeLabel');
            var pseq = hostTreeItem.storageItem.getValue('seqno');
            // Reset process name widget
            dijit.byId('pd_name').set('value', pname + ' ' + pseq);
            // Take care to store the type as integer
            hostTreeItem.storageItem.setValue('processtype', +ptype.getId());
            mcc.storage.processStorage().save();
        });
    });
}

// Setup gui for defining host and process details
function hostTreeSelectionDetailsSetup () {
    processSelectionDetailsSetup();
}

/********************************* Initialize *********************************/
dojo.ready(function initialize () {
    console.info('[INF]Host tree selection details module initialized');
});
