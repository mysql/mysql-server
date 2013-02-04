/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/data/api/Identity",["../..","./Read"],function(_1){
_1.declare("dojo.data.api.Identity",_1.data.api.Read,{getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Identity":true};
},getIdentity:function(_2){
throw new Error("Unimplemented API: dojo.data.api.Identity.getIdentity");
},getIdentityAttributes:function(_3){
throw new Error("Unimplemented API: dojo.data.api.Identity.getIdentityAttributes");
},fetchItemByIdentity:function(_4){
if(!this.isItemLoaded(_4.item)){
throw new Error("Unimplemented API: dojo.data.api.Identity.fetchItemByIdentity");
}
}});
return _1.data.api.Identity;
});
