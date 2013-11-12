//>>built
define("dojox/data/HtmlStore",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/dom","dojo/_base/xhr","dojo/_base/window","dojo/data/util/simpleFetch","dojo/data/util/filter","dojox/xml/parser"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_1("dojox.data.HtmlStore",null,{constructor:function(_b){
if(_b&&"urlPreventCache" in _b){
this.urlPreventCache=_b.urlPreventCache?true:false;
}
if(_b&&"trimWhitespace" in _b){
this.trimWhitespace=_b.trimWhitespace?true:false;
}
if(_b.url){
if(!_b.dataId){
throw new Error("dojo.data.HtmlStore: Cannot instantiate using url without an id!");
}
this.url=_b.url;
this.dataId=_b.dataId;
}else{
if(_b.dataId){
this.dataId=_b.dataId;
}
}
if(_b&&"fetchOnCreate" in _b){
this.fetchOnCreate=_b.fetchOnCreate?true:false;
}
if(this.fetchOnCreate&&this.dataId){
this.fetch();
}
},url:"",dataId:"",trimWhitespace:false,urlPreventCache:false,fetchOnCreate:false,_indexItems:function(){
this._getHeadings();
if(this._rootNode.rows){
if(this._rootNode.tBodies&&this._rootNode.tBodies.length>0){
this._rootNode=this._rootNode.tBodies[0];
}
var i;
for(i=0;i<this._rootNode.rows.length;i++){
this._rootNode.rows[i]._ident=i+1;
}
}else{
var c=1;
for(i=0;i<this._rootNode.childNodes.length;i++){
if(this._rootNode.childNodes[i].nodeType===1){
this._rootNode.childNodes[i]._ident=c;
c++;
}
}
}
},_getHeadings:function(){
this._headings=[];
if(this._rootNode.tHead){
_2.forEach(this._rootNode.tHead.rows[0].cells,_3.hitch(this,function(th){
var _c=_9.textContent(th);
this._headings.push(this.trimWhitespace?_3.trim(_c):_c);
}));
}else{
this._headings=["name"];
}
},_getAllItems:function(){
var _d=[];
var i;
if(this._rootNode.rows){
for(i=0;i<this._rootNode.rows.length;i++){
_d.push(this._rootNode.rows[i]);
}
}else{
for(i=0;i<this._rootNode.childNodes.length;i++){
if(this._rootNode.childNodes[i].nodeType===1){
_d.push(this._rootNode.childNodes[i]);
}
}
}
return _d;
},_assertIsItem:function(_e){
if(!this.isItem(_e)){
throw new Error("dojo.data.HtmlStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_f){
if(typeof _f!=="string"){
throw new Error("dojo.data.HtmlStore: a function was passed an attribute argument that was not an attribute name string");
return -1;
}
return _2.indexOf(this._headings,_f);
},getValue:function(_10,_11,_12){
var _13=this.getValues(_10,_11);
return (_13.length>0)?_13[0]:_12;
},getValues:function(_14,_15){
this._assertIsItem(_14);
var _16=this._assertIsAttribute(_15);
if(_16>-1){
var _17;
if(_14.cells){
_17=_9.textContent(_14.cells[_16]);
}else{
_17=_9.textContent(_14);
}
return [this.trimWhitespace?_3.trim(_17):_17];
}
return [];
},getAttributes:function(_18){
this._assertIsItem(_18);
var _19=[];
for(var i=0;i<this._headings.length;i++){
if(this.hasAttribute(_18,this._headings[i])){
_19.push(this._headings[i]);
}
}
return _19;
},hasAttribute:function(_1a,_1b){
return this.getValues(_1a,_1b).length>0;
},containsValue:function(_1c,_1d,_1e){
var _1f=undefined;
if(typeof _1e==="string"){
_1f=_8.patternToRegExp(_1e,false);
}
return this._containsValue(_1c,_1d,_1e,_1f);
},_containsValue:function(_20,_21,_22,_23){
var _24=this.getValues(_20,_21);
for(var i=0;i<_24.length;++i){
var _25=_24[i];
if(typeof _25==="string"&&_23){
return (_25.match(_23)!==null);
}else{
if(_22===_25){
return true;
}
}
}
return false;
},isItem:function(_26){
return _26&&_4.isDescendant(_26,this._rootNode);
},isItemLoaded:function(_27){
return this.isItem(_27);
},loadItem:function(_28){
this._assertIsItem(_28.item);
},_fetchItems:function(_29,_2a,_2b){
if(this._rootNode){
this._finishFetchItems(_29,_2a,_2b);
}else{
if(!this.url){
this._rootNode=_4.byId(this.dataId);
this._indexItems();
this._finishFetchItems(_29,_2a,_2b);
}else{
var _2c={url:this.url,handleAs:"text",preventCache:this.urlPreventCache};
var _2d=this;
var _2e=_5.get(_2c);
_2e.addCallback(function(_2f){
var _30=function(_31,id){
if(_31.id==id){
return _31;
}
if(_31.childNodes){
for(var i=0;i<_31.childNodes.length;i++){
var _32=_30(_31.childNodes[i],id);
if(_32){
return _32;
}
}
}
return null;
};
var d=document.createElement("div");
d.innerHTML=_2f;
_2d._rootNode=_30(d,_2d.dataId);
_2d._indexItems();
_2d._finishFetchItems(_29,_2a,_2b);
});
_2e.addErrback(function(_33){
_2b(_33,_29);
});
}
}
},_finishFetchItems:function(_34,_35,_36){
var _37=[];
var _38=this._getAllItems();
if(_34.query){
var _39=_34.queryOptions?_34.queryOptions.ignoreCase:false;
_37=[];
var _3a={};
var key;
var _3b;
for(key in _34.query){
_3b=_34.query[key]+"";
if(typeof _3b==="string"){
_3a[key]=_8.patternToRegExp(_3b,_39);
}
}
for(var i=0;i<_38.length;++i){
var _3c=true;
var _3d=_38[i];
for(key in _34.query){
_3b=_34.query[key]+"";
if(!this._containsValue(_3d,key,_3b,_3a[key])){
_3c=false;
}
}
if(_3c){
_37.push(_3d);
}
}
_35(_37,_34);
}else{
if(_38.length>0){
_37=_38.slice(0,_38.length);
}
_35(_37,_34);
}
},getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Identity":true};
},close:function(_3e){
},getLabel:function(_3f){
if(this.isItem(_3f)){
if(_3f.cells){
return "Item #"+this.getIdentity(_3f);
}else{
return this.getValue(_3f,"name");
}
}
return undefined;
},getLabelAttributes:function(_40){
if(_40.cells){
return null;
}else{
return ["name"];
}
},getIdentity:function(_41){
this._assertIsItem(_41);
if(this.hasAttribute(_41,"name")){
return this.getValue(_41,"name");
}else{
return _41._ident;
}
},getIdentityAttributes:function(_42){
return null;
},fetchItemByIdentity:function(_43){
var _44=_43.identity;
var _45=this;
var _46=null;
var _47=null;
if(!this._rootNode){
if(!this.url){
this._rootNode=_4.byId(this.dataId);
this._indexItems();
if(_45._rootNode.rows){
_46=this._rootNode.rows[_44+1];
}else{
for(var i=0;i<_45._rootNode.childNodes.length;i++){
if(_45._rootNode.childNodes[i].nodeType===1&&_44===_9.textContent(_45._rootNode.childNodes[i])){
_46=_45._rootNode.childNodes[i];
}
}
}
if(_43.onItem){
_47=_43.scope?_43.scope:_6.global;
_43.onItem.call(_47,_46);
}
}else{
var _48={url:this.url,handleAs:"text"};
var _49=_5.get(_48);
_49.addCallback(function(_4a){
var _4b=function(_4c,id){
if(_4c.id==id){
return _4c;
}
if(_4c.childNodes){
for(var i=0;i<_4c.childNodes.length;i++){
var _4d=_4b(_4c.childNodes[i],id);
if(_4d){
return _4d;
}
}
}
return null;
};
var d=document.createElement("div");
d.innerHTML=_4a;
_45._rootNode=_4b(d,_45.dataId);
_45._indexItems();
if(_45._rootNode.rows&&_44<=_45._rootNode.rows.length){
_46=_45._rootNode.rows[_44-1];
}else{
for(var i=0;i<_45._rootNode.childNodes.length;i++){
if(_45._rootNode.childNodes[i].nodeType===1&&_44===_9.textContent(_45._rootNode.childNodes[i])){
_46=_45._rootNode.childNodes[i];
break;
}
}
}
if(_43.onItem){
_47=_43.scope?_43.scope:_6.global;
_43.onItem.call(_47,_46);
}
});
_49.addErrback(function(_4e){
if(_43.onError){
_47=_43.scope?_43.scope:_6.global;
_43.onError.call(_47,_4e);
}
});
}
}else{
if(this._rootNode.rows[_44+1]){
_46=this._rootNode.rows[_44+1];
if(_43.onItem){
_47=_43.scope?_43.scope:_6.global;
_43.onItem.call(_47,_46);
}
}
}
}});
_3.extend(_a,_7);
return _a;
});
