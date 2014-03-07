//>>built
define("dojox/data/OpmlStore",["dojo/_base/declare","dojo/_base/lang","dojo/_base/xhr","dojo/data/util/simpleFetch","dojo/data/util/filter","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6){
var _7=_1("dojox.data.OpmlStore",null,{constructor:function(_8){
this._xmlData=null;
this._arrayOfTopLevelItems=[];
this._arrayOfAllItems=[];
this._metadataNodes=null;
this._loadFinished=false;
this.url=_8.url;
this._opmlData=_8.data;
if(_8.label){
this.label=_8.label;
}
this._loadInProgress=false;
this._queuedFetches=[];
this._identityMap={};
this._identCount=0;
this._idProp="_I";
if(_8&&"urlPreventCache" in _8){
this.urlPreventCache=_8.urlPreventCache?true:false;
}
},label:"text",url:"",urlPreventCache:false,_assertIsItem:function(_9){
if(!this.isItem(_9)){
throw new Error("dojo.data.OpmlStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_a){
if(!_2.isString(_a)){
throw new Error("dojox.data.OpmlStore: a function was passed an attribute argument that was not an attribute object nor an attribute name string");
}
},_removeChildNodesThatAreNotElementNodes:function(_b,_c){
var _d=_b.childNodes;
if(_d.length===0){
return;
}
var _e=[];
var i,_f;
for(i=0;i<_d.length;++i){
_f=_d[i];
if(_f.nodeType!=1){
_e.push(_f);
}
}
for(i=0;i<_e.length;++i){
_f=_e[i];
_b.removeChild(_f);
}
if(_c){
for(i=0;i<_d.length;++i){
_f=_d[i];
this._removeChildNodesThatAreNotElementNodes(_f,_c);
}
}
},_processRawXmlTree:function(_10){
this._loadFinished=true;
this._xmlData=_10;
var _11=_10.getElementsByTagName("head");
var _12=_11[0];
if(_12){
this._removeChildNodesThatAreNotElementNodes(_12);
this._metadataNodes=_12.childNodes;
}
var _13=_10.getElementsByTagName("body");
var _14=_13[0];
if(_14){
this._removeChildNodesThatAreNotElementNodes(_14,true);
var _15=_13[0].childNodes;
for(var i=0;i<_15.length;++i){
var _16=_15[i];
if(_16.tagName=="outline"){
this._identityMap[this._identCount]=_16;
this._identCount++;
this._arrayOfTopLevelItems.push(_16);
this._arrayOfAllItems.push(_16);
this._checkChildNodes(_16);
}
}
}
},_checkChildNodes:function(_17){
if(_17.firstChild){
for(var i=0;i<_17.childNodes.length;i++){
var _18=_17.childNodes[i];
if(_18.tagName=="outline"){
this._identityMap[this._identCount]=_18;
this._identCount++;
this._arrayOfAllItems.push(_18);
this._checkChildNodes(_18);
}
}
}
},_getItemsArray:function(_19){
if(_19&&_19.deep){
return this._arrayOfAllItems;
}
return this._arrayOfTopLevelItems;
},getValue:function(_1a,_1b,_1c){
this._assertIsItem(_1a);
this._assertIsAttribute(_1b);
if(_1b=="children"){
return (_1a.firstChild||_1c);
}else{
var _1d=_1a.getAttribute(_1b);
return (_1d!==undefined)?_1d:_1c;
}
},getValues:function(_1e,_1f){
this._assertIsItem(_1e);
this._assertIsAttribute(_1f);
var _20=[];
if(_1f=="children"){
for(var i=0;i<_1e.childNodes.length;++i){
_20.push(_1e.childNodes[i]);
}
}else{
if(_1e.getAttribute(_1f)!==null){
_20.push(_1e.getAttribute(_1f));
}
}
return _20;
},getAttributes:function(_21){
this._assertIsItem(_21);
var _22=[];
var _23=_21;
var _24=_23.attributes;
for(var i=0;i<_24.length;++i){
var _25=_24.item(i);
_22.push(_25.nodeName);
}
if(_23.childNodes.length>0){
_22.push("children");
}
return _22;
},hasAttribute:function(_26,_27){
return (this.getValues(_26,_27).length>0);
},containsValue:function(_28,_29,_2a){
var _2b=undefined;
if(typeof _2a==="string"){
_2b=_5.patternToRegExp(_2a,false);
}
return this._containsValue(_28,_29,_2a,_2b);
},_containsValue:function(_2c,_2d,_2e,_2f){
var _30=this.getValues(_2c,_2d);
for(var i=0;i<_30.length;++i){
var _31=_30[i];
if(typeof _31==="string"&&_2f){
return (_31.match(_2f)!==null);
}else{
if(_2e===_31){
return true;
}
}
}
return false;
},isItem:function(_32){
return (_32&&_32.nodeType==1&&_32.tagName=="outline"&&_32.ownerDocument===this._xmlData);
},isItemLoaded:function(_33){
return this.isItem(_33);
},loadItem:function(_34){
},getLabel:function(_35){
if(this.isItem(_35)){
return this.getValue(_35,this.label);
}
return undefined;
},getLabelAttributes:function(_36){
return [this.label];
},_fetchItems:function(_37,_38,_39){
var _3a=this;
var _3b=function(_3c,_3d){
var _3e=null;
if(_3c.query){
_3e=[];
var _3f=_3c.queryOptions?_3c.queryOptions.ignoreCase:false;
var _40={};
for(var key in _3c.query){
var _41=_3c.query[key];
if(typeof _41==="string"){
_40[key]=_5.patternToRegExp(_41,_3f);
}
}
for(var i=0;i<_3d.length;++i){
var _42=true;
var _43=_3d[i];
for(var key in _3c.query){
var _41=_3c.query[key];
if(!_3a._containsValue(_43,key,_41,_40[key])){
_42=false;
}
}
if(_42){
_3e.push(_43);
}
}
}else{
if(_3d.length>0){
_3e=_3d.slice(0,_3d.length);
}
}
_38(_3e,_3c);
};
if(this._loadFinished){
_3b(_37,this._getItemsArray(_37.queryOptions));
}else{
if(this._loadInProgress){
this._queuedFetches.push({args:_37,filter:_3b});
}else{
if(this.url!==""){
this._loadInProgress=true;
var _44={url:_3a.url,handleAs:"xml",preventCache:_3a.urlPreventCache};
var _45=_3.get(_44);
_45.addCallback(function(_46){
_3a._processRawXmlTree(_46);
_3b(_37,_3a._getItemsArray(_37.queryOptions));
_3a._handleQueuedFetches();
});
_45.addErrback(function(_47){
throw _47;
});
}else{
if(this._opmlData){
this._processRawXmlTree(this._opmlData);
this._opmlData=null;
_3b(_37,this._getItemsArray(_37.queryOptions));
}else{
throw new Error("dojox.data.OpmlStore: No OPML source data was provided as either URL or XML data input.");
}
}
}
}
},getFeatures:function(){
var _48={"dojo.data.api.Read":true,"dojo.data.api.Identity":true};
return _48;
},getIdentity:function(_49){
if(this.isItem(_49)){
for(var i in this._identityMap){
if(this._identityMap[i]===_49){
return i;
}
}
}
return null;
},fetchItemByIdentity:function(_4a){
if(!this._loadFinished){
var _4b=this;
if(this.url!==""){
if(this._loadInProgress){
this._queuedFetches.push({args:_4a});
}else{
this._loadInProgress=true;
var _4c={url:_4b.url,handleAs:"xml"};
var _4d=_3.get(_4c);
_4d.addCallback(function(_4e){
var _4f=_4a.scope?_4a.scope:_6.global;
try{
_4b._processRawXmlTree(_4e);
var _50=_4b._identityMap[_4a.identity];
if(!_4b.isItem(_50)){
_50=null;
}
if(_4a.onItem){
_4a.onItem.call(_4f,_50);
}
_4b._handleQueuedFetches();
}
catch(error){
if(_4a.onError){
_4a.onError.call(_4f,error);
}
}
});
_4d.addErrback(function(_51){
this._loadInProgress=false;
if(_4a.onError){
var _52=_4a.scope?_4a.scope:_6.global;
_4a.onError.call(_52,_51);
}
});
}
}else{
if(this._opmlData){
this._processRawXmlTree(this._opmlData);
this._opmlData=null;
var _53=this._identityMap[_4a.identity];
if(!_4b.isItem(_53)){
_53=null;
}
if(_4a.onItem){
var _54=_4a.scope?_4a.scope:_6.global;
_4a.onItem.call(_54,_53);
}
}
}
}else{
var _53=this._identityMap[_4a.identity];
if(!this.isItem(_53)){
_53=null;
}
if(_4a.onItem){
var _54=_4a.scope?_4a.scope:_6.global;
_4a.onItem.call(_54,_53);
}
}
},getIdentityAttributes:function(_55){
return null;
},_handleQueuedFetches:function(){
if(this._queuedFetches.length>0){
for(var i=0;i<this._queuedFetches.length;i++){
var _56=this._queuedFetches[i];
var _57=_56.args;
var _58=_56.filter;
if(_58){
_58(_57,this._getItemsArray(_57.queryOptions));
}else{
this.fetchItemByIdentity(_57);
}
}
this._queuedFetches=[];
}
},close:function(_59){
}});
_2.extend(_7,_4);
return _7;
});
