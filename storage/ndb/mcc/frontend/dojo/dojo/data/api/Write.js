/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/data/api/Write",["../..","./Read"],function(_1){
_1.declare("dojo.data.api.Write",_1.data.api.Read,{getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Write":true};
},newItem:function(_2,_3){
throw new Error("Unimplemented API: dojo.data.api.Write.newItem");
},deleteItem:function(_4){
throw new Error("Unimplemented API: dojo.data.api.Write.deleteItem");
},setValue:function(_5,_6,_7){
throw new Error("Unimplemented API: dojo.data.api.Write.setValue");
},setValues:function(_8,_9,_a){
throw new Error("Unimplemented API: dojo.data.api.Write.setValues");
},unsetAttribute:function(_b,_c){
throw new Error("Unimplemented API: dojo.data.api.Write.clear");
},save:function(_d){
throw new Error("Unimplemented API: dojo.data.api.Write.save");
},revert:function(){
throw new Error("Unimplemented API: dojo.data.api.Write.revert");
},isDirty:function(_e){
throw new Error("Unimplemented API: dojo.data.api.Write.isDirty");
}});
return _1.data.api.Write;
});
