/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/data/api/Notification",["../..","./Read"],function(_1){
_1.declare("dojo.data.api.Notification",_1.data.api.Read,{getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Notification":true};
},onSet:function(_2,_3,_4,_5){
throw new Error("Unimplemented API: dojo.data.api.Notification.onSet");
},onNew:function(_6,_7){
throw new Error("Unimplemented API: dojo.data.api.Notification.onNew");
},onDelete:function(_8){
throw new Error("Unimplemented API: dojo.data.api.Notification.onDelete");
}});
return _1.data.api.Notification;
});
