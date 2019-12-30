//>>built
define("dojox/data/HtmlTableStore",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/dom","dojo/_base/array","dojo/_base/xhr","dojo/_base/sniff","dojo/data/util/simpleFetch","dojo/data/util/filter","dojox/xml/parser"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var _b=_2("dojox.data.HtmlTableStore",null,{constructor:function(_c){
_1.deprecated("dojox.data.HtmlTableStore","Please use dojox.data.HtmlStore");
if(_c.url){
if(!_c.tableId){
throw new Error("dojo.data.HtmlTableStore: Cannot instantiate using url without an id!");
}
this.url=_c.url;
this.tableId=_c.tableId;
}else{
if(_c.tableId){
this._rootNode=_4.byId(_c.tableId);
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
this._headings.push(_a.textContent(th));
}));
},_getAllItems:function(){
var _d=[];
for(var i=1;i<this._rootNode.rows.length;i++){
_d.push(this._rootNode.rows[i]);
}
return _d;
},_assertIsItem:function(_e){
if(!this.isItem(_e)){
throw new Error("dojo.data.HtmlTableStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_f){
if(typeof _f!=="string"){
throw new Error("dojo.data.HtmlTableStore: a function was passed an attribute argument that was not an attribute name string");
}
return _5.indexOf(this._headings,_f);
},getValue:function(_10,_11,_12){
var _13=this.getValues(_10,_11);
return (_13.length>0)?_13[0]:_12;
},getValues:function(_14,_15){
this._assertIsItem(_14);
var _16=this._assertIsAttribute(_15);
if(_16>-1){
return [_a.textContent(_14.cells[_16])];
}
return [];
},getAttributes:function(_17){
this._assertIsItem(_17);
var _18=[];
for(var i=0;i<this._headings.length;i++){
if(this.hasAttribute(_17,this._headings[i])){
_18.push(this._headings[i]);
}
}
return _18;
},hasAttribute:function(_19,_1a){
return this.getValues(_19,_1a).length>0;
},containsValue:function(_1b,_1c,_1d){
var _1e=undefined;
if(typeof _1d==="string"){
_1e=_9.patternToRegExp(_1d,false);
}
return this._containsValue(_1b,_1c,_1d,_1e);
},_containsValue:function(_1f,_20,_21,_22){
var _23=this.getValues(_1f,_20);
for(var i=0;i<_23.length;++i){
var _24=_23[i];
if(typeof _24==="string"&&_22){
return (_24.match(_22)!==null);
}else{
if(_21===_24){
return true;
}
}
}
return false;
},isItem:function(_25){
if(_25&&_25.store&&_25.store===this){
return true;
}
return false;
},isItemLoaded:function(_26){
return this.isItem(_26);
},loadItem:function(_27){
this._assertIsItem(_27.item);
},_fetchItems:function(_28,_29,_2a){
if(this._rootNode){
this._finishFetchItems(_28,_29,_2a);
}else{
if(!this.url){
this._rootNode=_4.byId(this.tableId);
this._getHeadings();
for(var i=0;i<this._rootNode.rows.length;i++){
this._rootNode.rows[i].store=this;
}
}else{
var _2b={url:this.url,handleAs:"text"};
var _2c=this;
var _2d=_6.get(_2b);
_2d.addCallback(function(_2e){
var _2f=function(_30,id){
if(_30.id==id){
return _30;
}
if(_30.childNodes){
for(var i=0;i<_30.childNodes.length;i++){
var _31=_2f(_30.childNodes[i],id);
if(_31){
return _31;
}
}
}
return null;
};
var d=document.createElement("div");
d.innerHTML=_2e;
_2c._rootNode=_2f(d,_2c.tableId);
_2c._getHeadings.call(_2c);
for(var i=0;i<_2c._rootNode.rows.length;i++){
_2c._rootNode.rows[i].store=_2c;
}
_2c._finishFetchItems(_28,_29,_2a);
});
_2d.addErrback(function(_32){
_2a(_32,_28);
});
}
}
},_finishFetchItems:function(_33,_34,_35){
var _36=null;
var _37=this._getAllItems();
if(_33.query){
var _38=_33.queryOptions?_33.queryOptions.ignoreCase:false;
_36=[];
var _39={};
var _3a;
var key;
for(key in _33.query){
_3a=_33.query[key]+"";
if(typeof _3a==="string"){
_39[key]=_9.patternToRegExp(_3a,_38);
}
}
for(var i=0;i<_37.length;++i){
var _3b=true;
var _3c=_37[i];
for(key in _33.query){
_3a=_33.query[key]+"";
if(!this._containsValue(_3c,key,_3a,_39[key])){
_3b=false;
}
}
if(_3b){
_36.push(_3c);
}
}
_34(_36,_33);
}else{
if(_37.length>0){
_36=_37.slice(0,_37.length);
}
_34(_36,_33);
}
},getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Identity":true};
},close:function(_3d){
},getLabel:function(_3e){
if(this.isItem(_3e)){
return "Table Row #"+this.getIdentity(_3e);
}
return undefined;
},getLabelAttributes:function(_3f){
return null;
},getIdentity:function(_40){
this._assertIsItem(_40);
if(!_7("opera")){
return _40.sectionRowIndex;
}else{
return (_5.indexOf(this._rootNode.rows,_40)-1);
}
},getIdentityAttributes:function(_41){
return null;
},fetchItemByIdentity:function(_42){
var _43=_42.identity;
var _44=this;
var _45=null;
var _46=null;
if(!this._rootNode){
if(!this.url){
this._rootNode=_4.byId(this.tableId);
this._getHeadings();
for(var i=0;i<this._rootNode.rows.length;i++){
this._rootNode.rows[i].store=this;
}
_45=this._rootNode.rows[_43+1];
if(_42.onItem){
_46=_42.scope?_42.scope:_1.global;
_42.onItem.call(_46,_45);
}
}else{
var _47={url:this.url,handleAs:"text"};
var _48=_6.get(_47);
_48.addCallback(function(_49){
var _4a=function(_4b,id){
if(_4b.id==id){
return _4b;
}
if(_4b.childNodes){
for(var i=0;i<_4b.childNodes.length;i++){
var _4c=_4a(_4b.childNodes[i],id);
if(_4c){
return _4c;
}
}
}
return null;
};
var d=document.createElement("div");
d.innerHTML=_49;
_44._rootNode=_4a(d,_44.tableId);
_44._getHeadings.call(_44);
for(var i=0;i<_44._rootNode.rows.length;i++){
_44._rootNode.rows[i].store=_44;
}
_45=_44._rootNode.rows[_43+1];
if(_42.onItem){
_46=_42.scope?_42.scope:_1.global;
_42.onItem.call(_46,_45);
}
});
_48.addErrback(function(_4d){
if(_42.onError){
_46=_42.scope?_42.scope:_1.global;
_42.onError.call(_46,_4d);
}
});
}
}else{
if(this._rootNode.rows[_43+1]){
_45=this._rootNode.rows[_43+1];
if(_42.onItem){
_46=_42.scope?_42.scope:_1.global;
_42.onItem.call(_46,_45);
}
}
}
}});
_3.extend(_b,_8);
return _b;
});
