//>>built
define("dojox/data/HtmlTableStore",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/dom","dojo/_base/array","dojo/_base/xhr","dojo/_base/sniff","dojo/_base/window","dojo/data/util/simpleFetch","dojo/data/util/filter","dojox/xml/parser"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_2("dojox.data.HtmlTableStore",null,{constructor:function(_d){
_1.deprecated("dojox.data.HtmlTableStore","Please use dojox.data.HtmlStore");
if(_d.url){
if(!_d.tableId){
throw new Error("dojo.data.HtmlTableStore: Cannot instantiate using url without an id!");
}
this.url=_d.url;
this.tableId=_d.tableId;
}else{
if(_d.tableId){
this._rootNode=_4.byId(_d.tableId);
this.tableId=this._rootNode.id;
}else{
this._rootNode=_4.byId(this.tableId);
}
this._getHeadings();
for(var i=0;i<this._rootNode.rows.length;i++){
this._rootNode.rows[i].store=this;
}
}
},url:"",tableId:"",_getHeadings:function(){
this._headings=[];
_5.forEach(this._rootNode.tHead.rows[0].cells,_3.hitch(this,function(th){
this._headings.push(_b.textContent(th));
}));
},_getAllItems:function(){
var _e=[];
for(var i=1;i<this._rootNode.rows.length;i++){
_e.push(this._rootNode.rows[i]);
}
return _e;
},_assertIsItem:function(_f){
if(!this.isItem(_f)){
throw new Error("dojo.data.HtmlTableStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_10){
if(typeof _10!=="string"){
throw new Error("dojo.data.HtmlTableStore: a function was passed an attribute argument that was not an attribute name string");
return -1;
}
return _5.indexOf(this._headings,_10);
},getValue:function(_11,_12,_13){
var _14=this.getValues(_11,_12);
return (_14.length>0)?_14[0]:_13;
},getValues:function(_15,_16){
this._assertIsItem(_15);
var _17=this._assertIsAttribute(_16);
if(_17>-1){
return [_b.textContent(_15.cells[_17])];
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
_1f=_a.patternToRegExp(_1e,false);
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
if(_26&&_26.store&&_26.store===this){
return true;
}
return false;
},isItemLoaded:function(_27){
return this.isItem(_27);
},loadItem:function(_28){
this._assertIsItem(_28.item);
},_fetchItems:function(_29,_2a,_2b){
if(this._rootNode){
this._finishFetchItems(_29,_2a,_2b);
}else{
if(!this.url){
this._rootNode=_4.byId(this.tableId);
this._getHeadings();
for(var i=0;i<this._rootNode.rows.length;i++){
this._rootNode.rows[i].store=this;
}
}else{
var _2c={url:this.url,handleAs:"text"};
var _2d=this;
var _2e=_6.get(_2c);
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
_2d._rootNode=_30(d,_2d.tableId);
_2d._getHeadings.call(_2d);
for(var i=0;i<_2d._rootNode.rows.length;i++){
_2d._rootNode.rows[i].store=_2d;
}
_2d._finishFetchItems(_29,_2a,_2b);
});
_2e.addErrback(function(_33){
_2b(_33,_29);
});
}
}
},_finishFetchItems:function(_34,_35,_36){
var _37=null;
var _38=this._getAllItems();
if(_34.query){
var _39=_34.queryOptions?_34.queryOptions.ignoreCase:false;
_37=[];
var _3a={};
var _3b;
var key;
for(key in _34.query){
_3b=_34.query[key]+"";
if(typeof _3b==="string"){
_3a[key]=_a.patternToRegExp(_3b,_39);
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
return "Table Row #"+this.getIdentity(_3f);
}
return undefined;
},getLabelAttributes:function(_40){
return null;
},getIdentity:function(_41){
this._assertIsItem(_41);
if(!_7("opera")){
return _41.sectionRowIndex;
}else{
return (_5.indexOf(this._rootNode.rows,_41)-1);
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
this._rootNode=_4.byId(this.tableId);
this._getHeadings();
for(var i=0;i<this._rootNode.rows.length;i++){
this._rootNode.rows[i].store=this;
}
_46=this._rootNode.rows[_44+1];
if(_43.onItem){
_47=_43.scope?_43.scope:_8.global;
_43.onItem.call(_47,_46);
}
}else{
var _48={url:this.url,handleAs:"text"};
var _49=_6.get(_48);
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
_45._rootNode=_4b(d,_45.tableId);
_45._getHeadings.call(_45);
for(var i=0;i<_45._rootNode.rows.length;i++){
_45._rootNode.rows[i].store=_45;
}
_46=_45._rootNode.rows[_44+1];
if(_43.onItem){
_47=_43.scope?_43.scope:_8.global;
_43.onItem.call(_47,_46);
}
});
_49.addErrback(function(_4e){
if(_43.onError){
_47=_43.scope?_43.scope:_8.global;
_43.onError.call(_47,_4e);
}
});
}
}else{
if(this._rootNode.rows[_44+1]){
_46=this._rootNode.rows[_44+1];
if(_43.onItem){
_47=_43.scope?_43.scope:_8.global;
_43.onItem.call(_47,_46);
}
}
}
}});
_3.extend(_c,_9);
return _c;
});
