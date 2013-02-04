/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/data/ItemFileReadStore",["../_base/kernel","../_base/lang","../_base/declare","../_base/array","../_base/xhr","../Evented","../_base/window","./util/filter","./util/simpleFetch","../date/stamp"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var _b=_3("dojo.data.ItemFileReadStore",[_6],{constructor:function(_c){
this._arrayOfAllItems=[];
this._arrayOfTopLevelItems=[];
this._loadFinished=false;
this._jsonFileUrl=_c.url;
this._ccUrl=_c.url;
this.url=_c.url;
this._jsonData=_c.data;
this.data=null;
this._datatypeMap=_c.typeMap||{};
if(!this._datatypeMap["Date"]){
this._datatypeMap["Date"]={type:Date,deserialize:function(_d){
return _a.fromISOString(_d);
}};
}
this._features={"dojo.data.api.Read":true,"dojo.data.api.Identity":true};
this._itemsByIdentity=null;
this._storeRefPropName="_S";
this._itemNumPropName="_0";
this._rootItemPropName="_RI";
this._reverseRefMap="_RRM";
this._loadInProgress=false;
this._queuedFetches=[];
if(_c.urlPreventCache!==undefined){
this.urlPreventCache=_c.urlPreventCache?true:false;
}
if(_c.hierarchical!==undefined){
this.hierarchical=_c.hierarchical?true:false;
}
if(_c.clearOnClose){
this.clearOnClose=true;
}
if("failOk" in _c){
this.failOk=_c.failOk?true:false;
}
},url:"",_ccUrl:"",data:null,typeMap:null,clearOnClose:false,urlPreventCache:false,failOk:false,hierarchical:true,_assertIsItem:function(_e){
if(!this.isItem(_e)){
throw new Error("dojo.data.ItemFileReadStore: Invalid item argument.");
}
},_assertIsAttribute:function(_f){
if(typeof _f!=="string"){
throw new Error("dojo.data.ItemFileReadStore: Invalid attribute argument.");
}
},getValue:function(_10,_11,_12){
var _13=this.getValues(_10,_11);
return (_13.length>0)?_13[0]:_12;
},getValues:function(_14,_15){
this._assertIsItem(_14);
this._assertIsAttribute(_15);
return (_14[_15]||[]).slice(0);
},getAttributes:function(_16){
this._assertIsItem(_16);
var _17=[];
for(var key in _16){
if((key!==this._storeRefPropName)&&(key!==this._itemNumPropName)&&(key!==this._rootItemPropName)&&(key!==this._reverseRefMap)){
_17.push(key);
}
}
return _17;
},hasAttribute:function(_18,_19){
this._assertIsItem(_18);
this._assertIsAttribute(_19);
return (_19 in _18);
},containsValue:function(_1a,_1b,_1c){
var _1d=undefined;
if(typeof _1c==="string"){
_1d=_8.patternToRegExp(_1c,false);
}
return this._containsValue(_1a,_1b,_1c,_1d);
},_containsValue:function(_1e,_1f,_20,_21){
return _4.some(this.getValues(_1e,_1f),function(_22){
if(_22!==null&&!_2.isObject(_22)&&_21){
if(_22.toString().match(_21)){
return true;
}
}else{
if(_20===_22){
return true;
}
}
});
},isItem:function(_23){
if(_23&&_23[this._storeRefPropName]===this){
if(this._arrayOfAllItems[_23[this._itemNumPropName]]===_23){
return true;
}
}
return false;
},isItemLoaded:function(_24){
return this.isItem(_24);
},loadItem:function(_25){
this._assertIsItem(_25.item);
},getFeatures:function(){
return this._features;
},getLabel:function(_26){
if(this._labelAttr&&this.isItem(_26)){
return this.getValue(_26,this._labelAttr);
}
return undefined;
},getLabelAttributes:function(_27){
if(this._labelAttr){
return [this._labelAttr];
}
return null;
},_fetchItems:function(_28,_29,_2a){
var _2b=this,_2c=function(_2d,_2e){
var _2f=[],i,key;
if(_2d.query){
var _30,_31=_2d.queryOptions?_2d.queryOptions.ignoreCase:false;
var _32={};
for(key in _2d.query){
_30=_2d.query[key];
if(typeof _30==="string"){
_32[key]=_8.patternToRegExp(_30,_31);
}else{
if(_30 instanceof RegExp){
_32[key]=_30;
}
}
}
for(i=0;i<_2e.length;++i){
var _33=true;
var _34=_2e[i];
if(_34===null){
_33=false;
}else{
for(key in _2d.query){
_30=_2d.query[key];
if(!_2b._containsValue(_34,key,_30,_32[key])){
_33=false;
}
}
}
if(_33){
_2f.push(_34);
}
}
_29(_2f,_2d);
}else{
for(i=0;i<_2e.length;++i){
var _35=_2e[i];
if(_35!==null){
_2f.push(_35);
}
}
_29(_2f,_2d);
}
};
if(this._loadFinished){
_2c(_28,this._getItemsArray(_28.queryOptions));
}else{
if(this._jsonFileUrl!==this._ccUrl){
_1.deprecated("dojo.data.ItemFileReadStore: ","To change the url, set the url property of the store,"+" not _jsonFileUrl.  _jsonFileUrl support will be removed in 2.0");
this._ccUrl=this._jsonFileUrl;
this.url=this._jsonFileUrl;
}else{
if(this.url!==this._ccUrl){
this._jsonFileUrl=this.url;
this._ccUrl=this.url;
}
}
if(this.data!=null){
this._jsonData=this.data;
this.data=null;
}
if(this._jsonFileUrl){
if(this._loadInProgress){
this._queuedFetches.push({args:_28,filter:_2c});
}else{
this._loadInProgress=true;
var _36={url:_2b._jsonFileUrl,handleAs:"json-comment-optional",preventCache:this.urlPreventCache,failOk:this.failOk};
var _37=_5.get(_36);
_37.addCallback(function(_38){
try{
_2b._getItemsFromLoadedData(_38);
_2b._loadFinished=true;
_2b._loadInProgress=false;
_2c(_28,_2b._getItemsArray(_28.queryOptions));
_2b._handleQueuedFetches();
}
catch(e){
_2b._loadFinished=true;
_2b._loadInProgress=false;
_2a(e,_28);
}
});
_37.addErrback(function(_39){
_2b._loadInProgress=false;
_2a(_39,_28);
});
var _3a=null;
if(_28.abort){
_3a=_28.abort;
}
_28.abort=function(){
var df=_37;
if(df&&df.fired===-1){
df.cancel();
df=null;
}
if(_3a){
_3a.call(_28);
}
};
}
}else{
if(this._jsonData){
try{
this._loadFinished=true;
this._getItemsFromLoadedData(this._jsonData);
this._jsonData=null;
_2c(_28,this._getItemsArray(_28.queryOptions));
}
catch(e){
_2a(e,_28);
}
}else{
_2a(new Error("dojo.data.ItemFileReadStore: No JSON source data was provided as either URL or a nested Javascript object."),_28);
}
}
}
},_handleQueuedFetches:function(){
if(this._queuedFetches.length>0){
for(var i=0;i<this._queuedFetches.length;i++){
var _3b=this._queuedFetches[i],_3c=_3b.args,_3d=_3b.filter;
if(_3d){
_3d(_3c,this._getItemsArray(_3c.queryOptions));
}else{
this.fetchItemByIdentity(_3c);
}
}
this._queuedFetches=[];
}
},_getItemsArray:function(_3e){
if(_3e&&_3e.deep){
return this._arrayOfAllItems;
}
return this._arrayOfTopLevelItems;
},close:function(_3f){
if(this.clearOnClose&&this._loadFinished&&!this._loadInProgress){
if(((this._jsonFileUrl==""||this._jsonFileUrl==null)&&(this.url==""||this.url==null))&&this.data==null){
}
this._arrayOfAllItems=[];
this._arrayOfTopLevelItems=[];
this._loadFinished=false;
this._itemsByIdentity=null;
this._loadInProgress=false;
this._queuedFetches=[];
}
},_getItemsFromLoadedData:function(_40){
var _41=false,_42=this;
function _43(_44){
return (_44!==null)&&(typeof _44==="object")&&(!_2.isArray(_44)||_41)&&(!_2.isFunction(_44))&&(_44.constructor==Object||_2.isArray(_44))&&(typeof _44._reference==="undefined")&&(typeof _44._type==="undefined")&&(typeof _44._value==="undefined")&&_42.hierarchical;
};
function _45(_46){
_42._arrayOfAllItems.push(_46);
for(var _47 in _46){
var _48=_46[_47];
if(_48){
if(_2.isArray(_48)){
var _49=_48;
for(var k=0;k<_49.length;++k){
var _4a=_49[k];
if(_43(_4a)){
_45(_4a);
}
}
}else{
if(_43(_48)){
_45(_48);
}
}
}
}
};
this._labelAttr=_40.label;
var i,_4b;
this._arrayOfAllItems=[];
this._arrayOfTopLevelItems=_40.items;
for(i=0;i<this._arrayOfTopLevelItems.length;++i){
_4b=this._arrayOfTopLevelItems[i];
if(_2.isArray(_4b)){
_41=true;
}
_45(_4b);
_4b[this._rootItemPropName]=true;
}
var _4c={},key;
for(i=0;i<this._arrayOfAllItems.length;++i){
_4b=this._arrayOfAllItems[i];
for(key in _4b){
if(key!==this._rootItemPropName){
var _4d=_4b[key];
if(_4d!==null){
if(!_2.isArray(_4d)){
_4b[key]=[_4d];
}
}else{
_4b[key]=[null];
}
}
_4c[key]=key;
}
}
while(_4c[this._storeRefPropName]){
this._storeRefPropName+="_";
}
while(_4c[this._itemNumPropName]){
this._itemNumPropName+="_";
}
while(_4c[this._reverseRefMap]){
this._reverseRefMap+="_";
}
var _4e;
var _4f=_40.identifier;
if(_4f){
this._itemsByIdentity={};
this._features["dojo.data.api.Identity"]=_4f;
for(i=0;i<this._arrayOfAllItems.length;++i){
_4b=this._arrayOfAllItems[i];
_4e=_4b[_4f];
var _50=_4e[0];
if(!Object.hasOwnProperty.call(this._itemsByIdentity,_50)){
this._itemsByIdentity[_50]=_4b;
}else{
if(this._jsonFileUrl){
throw new Error("dojo.data.ItemFileReadStore:  The json data as specified by: ["+this._jsonFileUrl+"] is malformed.  Items within the list have identifier: ["+_4f+"].  Value collided: ["+_50+"]");
}else{
if(this._jsonData){
throw new Error("dojo.data.ItemFileReadStore:  The json data provided by the creation arguments is malformed.  Items within the list have identifier: ["+_4f+"].  Value collided: ["+_50+"]");
}
}
}
}
}else{
this._features["dojo.data.api.Identity"]=Number;
}
for(i=0;i<this._arrayOfAllItems.length;++i){
_4b=this._arrayOfAllItems[i];
_4b[this._storeRefPropName]=this;
_4b[this._itemNumPropName]=i;
}
for(i=0;i<this._arrayOfAllItems.length;++i){
_4b=this._arrayOfAllItems[i];
for(key in _4b){
_4e=_4b[key];
for(var j=0;j<_4e.length;++j){
_4d=_4e[j];
if(_4d!==null&&typeof _4d=="object"){
if(("_type" in _4d)&&("_value" in _4d)){
var _51=_4d._type;
var _52=this._datatypeMap[_51];
if(!_52){
throw new Error("dojo.data.ItemFileReadStore: in the typeMap constructor arg, no object class was specified for the datatype '"+_51+"'");
}else{
if(_2.isFunction(_52)){
_4e[j]=new _52(_4d._value);
}else{
if(_2.isFunction(_52.deserialize)){
_4e[j]=_52.deserialize(_4d._value);
}else{
throw new Error("dojo.data.ItemFileReadStore: Value provided in typeMap was neither a constructor, nor a an object with a deserialize function");
}
}
}
}
if(_4d._reference){
var _53=_4d._reference;
if(!_2.isObject(_53)){
_4e[j]=this._getItemByIdentity(_53);
}else{
for(var k=0;k<this._arrayOfAllItems.length;++k){
var _54=this._arrayOfAllItems[k],_55=true;
for(var _56 in _53){
if(_54[_56]!=_53[_56]){
_55=false;
}
}
if(_55){
_4e[j]=_54;
}
}
}
if(this.referenceIntegrity){
var _57=_4e[j];
if(this.isItem(_57)){
this._addReferenceToMap(_57,_4b,key);
}
}
}else{
if(this.isItem(_4d)){
if(this.referenceIntegrity){
this._addReferenceToMap(_4d,_4b,key);
}
}
}
}
}
}
}
},_addReferenceToMap:function(_58,_59,_5a){
},getIdentity:function(_5b){
var _5c=this._features["dojo.data.api.Identity"];
if(_5c===Number){
return _5b[this._itemNumPropName];
}else{
var _5d=_5b[_5c];
if(_5d){
return _5d[0];
}
}
return null;
},fetchItemByIdentity:function(_5e){
var _5f,_60;
if(!this._loadFinished){
var _61=this;
if(this._jsonFileUrl!==this._ccUrl){
_1.deprecated("dojo.data.ItemFileReadStore: ","To change the url, set the url property of the store,"+" not _jsonFileUrl.  _jsonFileUrl support will be removed in 2.0");
this._ccUrl=this._jsonFileUrl;
this.url=this._jsonFileUrl;
}else{
if(this.url!==this._ccUrl){
this._jsonFileUrl=this.url;
this._ccUrl=this.url;
}
}
if(this.data!=null&&this._jsonData==null){
this._jsonData=this.data;
this.data=null;
}
if(this._jsonFileUrl){
if(this._loadInProgress){
this._queuedFetches.push({args:_5e});
}else{
this._loadInProgress=true;
var _62={url:_61._jsonFileUrl,handleAs:"json-comment-optional",preventCache:this.urlPreventCache,failOk:this.failOk};
var _63=_5.get(_62);
_63.addCallback(function(_64){
var _65=_5e.scope?_5e.scope:_7.global;
try{
_61._getItemsFromLoadedData(_64);
_61._loadFinished=true;
_61._loadInProgress=false;
_5f=_61._getItemByIdentity(_5e.identity);
if(_5e.onItem){
_5e.onItem.call(_65,_5f);
}
_61._handleQueuedFetches();
}
catch(error){
_61._loadInProgress=false;
if(_5e.onError){
_5e.onError.call(_65,error);
}
}
});
_63.addErrback(function(_66){
_61._loadInProgress=false;
if(_5e.onError){
var _67=_5e.scope?_5e.scope:_7.global;
_5e.onError.call(_67,_66);
}
});
}
}else{
if(this._jsonData){
_61._getItemsFromLoadedData(_61._jsonData);
_61._jsonData=null;
_61._loadFinished=true;
_5f=_61._getItemByIdentity(_5e.identity);
if(_5e.onItem){
_60=_5e.scope?_5e.scope:_7.global;
_5e.onItem.call(_60,_5f);
}
}
}
}else{
_5f=this._getItemByIdentity(_5e.identity);
if(_5e.onItem){
_60=_5e.scope?_5e.scope:_7.global;
_5e.onItem.call(_60,_5f);
}
}
},_getItemByIdentity:function(_68){
var _69=null;
if(this._itemsByIdentity){
if(Object.hasOwnProperty.call(this._itemsByIdentity,_68)){
_69=this._itemsByIdentity[_68];
}
}else{
if(Object.hasOwnProperty.call(this._arrayOfAllItems,_68)){
_69=this._arrayOfAllItems[_68];
}
}
if(_69===undefined){
_69=null;
}
return _69;
},getIdentityAttributes:function(_6a){
var _6b=this._features["dojo.data.api.Identity"];
if(_6b===Number){
return null;
}else{
return [_6b];
}
},_forceLoad:function(){
var _6c=this;
if(this._jsonFileUrl!==this._ccUrl){
_1.deprecated("dojo.data.ItemFileReadStore: ","To change the url, set the url property of the store,"+" not _jsonFileUrl.  _jsonFileUrl support will be removed in 2.0");
this._ccUrl=this._jsonFileUrl;
this.url=this._jsonFileUrl;
}else{
if(this.url!==this._ccUrl){
this._jsonFileUrl=this.url;
this._ccUrl=this.url;
}
}
if(this.data!=null){
this._jsonData=this.data;
this.data=null;
}
if(this._jsonFileUrl){
var _6d={url:this._jsonFileUrl,handleAs:"json-comment-optional",preventCache:this.urlPreventCache,failOk:this.failOk,sync:true};
var _6e=_5.get(_6d);
_6e.addCallback(function(_6f){
try{
if(_6c._loadInProgress!==true&&!_6c._loadFinished){
_6c._getItemsFromLoadedData(_6f);
_6c._loadFinished=true;
}else{
if(_6c._loadInProgress){
throw new Error("dojo.data.ItemFileReadStore:  Unable to perform a synchronous load, an async load is in progress.");
}
}
}
catch(e){
throw e;
}
});
_6e.addErrback(function(_70){
throw _70;
});
}else{
if(this._jsonData){
_6c._getItemsFromLoadedData(_6c._jsonData);
_6c._jsonData=null;
_6c._loadFinished=true;
}
}
}});
_2.extend(_b,_9);
return _b;
});
