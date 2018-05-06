/*
Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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
 ***                                User choices                            ***
 ***                                                                        ***
 ******************************************************************************
 *
 *  Module: 
 *      Name: mcc.userconfig.userconfigjs
 *
 *  Description:
 *      Setup global variables based on users choices.
 *
 *  External interface: 
 *      mcc.userconfig.userconfigjs.setConfigFile: Set the name of configuration user selected.
 *      mcc.userconfig.userconfigjs.getConfigFile: Get the name of configuration user selected.
 *      mcc.userconfig.userconfigjs.setConfigFileContents: Set contents of configuration file to variable.
 *      mcc.userconfig.userconfigjs.getConfigFileContents: Retrieve contents of configuration file from variable.
 *      mcc.userconfig.userconfigjs.getDefaultCfg: Provide default stores configuration.
 *      mcc.userconfig.userconfigjs.getConfKey: Return decryption key provided by user.
 *      mcc.userconfig.userconfigjs.setConfKey: Store decryption key provided by user.
 *      mcc.userconfig.userconfigjs.resetConfKey: Set passphrase *after* navigation.
 *      mcc.userconfig.userconfigjs.writeConfigFile: Write configuration to file;
 *
 *  External data: 
 *      TBD
 *
 *  Internal interface: 
 *      TBD
 *
 *  Internal data: 
 *      TBD
 *
 *  Unit test interface: 
 *      None
 *
 ******************************************************************************/

/****************************** Import/export  ********************************/

dojo.provide("mcc.userconfig.userconfigjs");

/**************************** External interface  *****************************/

mcc.userconfig.userconfigjs.setConfigFile = setConfigFile;
mcc.userconfig.userconfigjs.getConfigFile = getConfigFile;
mcc.userconfig.userconfigjs.setConfigFileContents = setConfigFileContents;
mcc.userconfig.userconfigjs.getConfigFileContents = getConfigFileContents;
mcc.userconfig.userconfigjs.getDefaultCfg = getDefaultCfg;
mcc.userconfig.userconfigjs.getConfKey = getConfKey;
mcc.userconfig.userconfigjs.setConfKey = setConfKey;
mcc.userconfig.userconfigjs.resetConfKey = resetConfKey;
mcc.userconfig.userconfigjs.writeConfigFile = writeConfigFile;

/****************************** Internal data   *******************************/
var confKey = "";
var configFile = "";
var configFileContents = "";
var defCfg1 = '{\n'+
    '\t"identifier": "id",\n'+
    '\t"label": "name",\n'+
    '\t"items": [\n'+
    '\t\t{\n'+
    '\t\t\t"id": 0,\n'+
    '\t\t\t"ssh_keybased": false,\n'+
    '\t\t\t"ssh_user": "",\n'+
    '\t\t\t"name": "';
var defCfg2 = '",\n'+
    '\t\t\t"apparea": "simple testing",\n'+
    '\t\t\t"writeload": "medium",\n'+
    '\t\t\t"installCluster": "NONE",\n'+
    '\t\t\t"openfw": false\n'+
    '\t\t}\n'+
    '\t]\n'+
    '},\n'+
    '{},\n'+
    '{},\n'+
    '{},\n'+
    '{}\n';

var defCfg = "";

/****************************** Implementation  *******************************/
// Get configuration key
function getConfKey() {
    return confKey;
}

// Set configuration key
function setConfKey(passp) {
    confKey = passp;
    console.log("Conf key set.");
    return ;
}

// Set configuration key again after navigation.
function resetConfKey(php) {
    confKey = php;
    return ;
}

//Wellcome screen provides user's choice for config file.
//This function saves it in globally available place.
function setConfigFile(fnm) {
    var cf = fnm;
    configFile = fnm;
    console.log("CF set to: " + configFile);
    if (defCfg == "") {
        defCfg = defCfg1 + configFile.slice(0, -4) + defCfg2; //Remove .mcc part.
    }
    return
}

//Tell others which file to use.
function getConfigFile() {
    return configFile;
}

//Tell others which file to use.
function getDefaultCfg() {
    return defCfg;
}

function setConfigFileContents(cfc) {
    configFileContents = cfc;
    console.log("[SET]CFC.length is: " + configFileContents.length);
    return
}

function getConfigFileContents() {
    console.log("[GET]CFC.length is: " + configFileContents.length);
    return configFileContents;
}

function do_post(msg) {
    // Convert to json string
    var jsonMsg = dojo.toJson(msg);
    // Return deferred from xhrPost
    return dojo.xhrPost({
        url: "/cmd",
        headers: { "Content-Type": "application/json" },
        postData: jsonMsg,
        handleAs: "json"
    });
}
// Generic error handler closure
function errorHandler(req, onError) {
    if (onError) {
        return onError;
    } else {
        return function (error) {
            console.log("An error occurred while executing '" + req.cmd + 
                    " (" + req.seq + ")': " + error);
        }
    }
}
// Generic reply handler closure
function replyHandler(onReply, onError) {
    return function (reply) {
        if (reply && reply.stat && reply.stat.errMsg != "OK") {
            if (onError) {
                onError(reply.stat.errMsg, reply);
            } else {
                alert(reply.stat.errMsg);
            }
        } else {
            onReply(reply);
        }
    }
}

function createCfgFileReq(mesg, onReply, onError) {
    console.log("Composing createCfgFileReq message for host localhost.");
    var ms = mesg;       
    // Call do_post, provide callbacks
    do_post(ms).then(replyHandler(onReply, onError), 
        errorHandler(ms.head, onError));
}

function writeConfigFile(contents, fileName) {
    var res2 = new dojo.Deferred();
    console.log("Writing to config file.");
    //Fill new config with predefined defaults:
    msg = {
        head: {cmd: "createCfgFileReq", seq: 1},
        body: {
            ssh: {keyBased: false, user: "", pwd: ""},
            hostName: 'localhost',
            path: "~",
            fname: fileName,
            contentString: contents,
            phr: getConfKey()
        }
    };
    createCfgFileReq(
            msg,
        function (resFNC) {
            console.log("Configuration written."); 
            res2.resolve(true);
            return res2;
        },  
        function (errMsgFNC) {
            alert("Unable to write config file " + fileName + " in HOME directory: " + errMsgFNC); 
            //This is bad, bail out and just work from tree.
            fileName = "";
            res2.resolve(false);
            return res2;
        }
    );
    res2.then(function(success1){
        console.log("createCfgFileReq success: ", success1);
        return true;
    }, function(error1){
        console.log("createCfgFileReq error: ", error1)
        return false;
    }).then(function(info1){
        console.log("createCfgFileReq has finished with success status: ", info1);
        if (info1) {
            console.log("writeConfigFile done.");
            return "OK"
        } else {
            return "FAILED"
        }
    });
}

/******************************** Initialize  *********************************/

dojo.ready(function() {
    console.log("Userconfig class module initialized");
});
