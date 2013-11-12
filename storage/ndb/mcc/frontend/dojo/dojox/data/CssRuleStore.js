//>>built
define("dojox/data/CssRuleStore",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/json","dojo/_base/window","dojo/_base/sniff","dojo/data/util/sorter","dojo/data/util/filter","./css"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.data.CssRuleStore",null,{_storeRef:"_S",_labelAttribute:"selector",_cache:null,_browserMap:null,_cName:"dojox.data.CssRuleStore",constructor:function(_a){
if(_a){
_1.mixin(this,_a);
}
this._cache={};
this._allItems=null;
this._waiting=[];
this.gatherHandle=null;
var _b=this;
function _c(){
try{
_b.context=_9.determineContext(_b.context);
if(_b.gatherHandle){
clearInterval(_b.gatherHandle);
_b.gatherHandle=null;
}
while(_b._waiting.length){
var _d=_b._waiting.pop();
_9.rules.forEach(_d.forFunc,null,_b.context);
_d.finishFunc();
}
}
catch(e){
}
};
this.gatherHandle=setInterval(_c,250);
},setContext:function(_e){
if(_e){
this.close();
this.context=_9.determineContext(_e);
}
},getFeatures:function(){
return {"dojo.data.api.Read":true};
},isItem:function(_f){
if(_f&&_f[this._storeRef]==this){
return true;
}
return false;
},hasAttribute:function(_10,_11){
this._assertIsItem(_10);
this._assertIsAttribute(_11);
var _12=this.getAttributes(_10);
if(_3.indexOf(_12,_11)!=-1){
return true;
}
return false;
},getAttributes:function(_13){
this._assertIsItem(_13);
var _14=["selector","classes","rule","style","cssText","styleSheet","parentStyleSheet","parentStyleSheetHref"];
var _15=_13.rule.style;
if(_15){
var key;
for(key in _15){
_14.push("style."+key);
}
}
return _14;
},getValue:function(_16,_17,_18){
var _19=this.getValues(_16,_17);
var _1a=_18;
if(_19&&_19.length>0){
return _19[0];
}
return _18;
},getValues:function(_1b,_1c){
this._assertIsItem(_1b);
this._assertIsAttribute(_1c);
var _1d=null;
if(_1c==="selector"){
_1d=_1b.rule["selectorText"];
if(_1d&&_1.isString(_1d)){
_1d=_1d.split(",");
}
}else{
if(_1c==="classes"){
_1d=_1b.classes;
}else{
if(_1c==="rule"){
_1d=_1b.rule.rule;
}else{
if(_1c==="style"){
_1d=_1b.rule.style;
}else{
if(_1c==="cssText"){
if(_6("ie")){
if(_1b.rule.style){
_1d=_1b.rule.style.cssText;
if(_1d){
_1d="{ "+_1d.toLowerCase()+" }";
}
}
}else{
_1d=_1b.rule.cssText;
if(_1d){
_1d=_1d.substring(_1d.indexOf("{"),_1d.length);
}
}
}else{
if(_1c==="styleSheet"){
_1d=_1b.rule.styleSheet;
}else{
if(_1c==="parentStyleSheet"){
_1d=_1b.rule.parentStyleSheet;
}else{
if(_1c==="parentStyleSheetHref"){
if(_1b.href){
_1d=_1b.href;
}
}else{
if(_1c.indexOf("style.")===0){
var _1e=_1c.substring(_1c.indexOf("."),_1c.length);
_1d=_1b.rule.style[_1e];
}else{
_1d=[];
}
}
}
}
}
}
}
}
}
if(_1d!==undefined){
if(!_1.isArray(_1d)){
_1d=[_1d];
}
}
return _1d;
},getLabel:function(_1f){
this._assertIsItem(_1f);
return this.getValue(_1f,this._labelAttribute);
},getLabelAttributes:function(_20){
return [this._labelAttribute];
},containsValue:function(_21,_22,_23){
var _24=undefined;
if(typeof _23==="string"){
_24=_8.patternToRegExp(_23,false);
}
return this._containsValue(_21,_22,_23,_24);
},isItemLoaded:function(_25){
return this.isItem(_25);
},loadItem:function(_26){
this._assertIsItem(_26.item);
},fetch:function(_27){
_27=_27||{};
if(!_27.store){
_27.store=this;
}
var _28=_27.scope||_5.global;
if(this._pending&&this._pending.length>0){
this._pending.push({request:_27,fetch:true});
}else{
this._pending=[{request:_27,fetch:true}];
this._fetch(_27);
}
return _27;
},_fetch:function(_29){
var _2a=_29.scope||_5.global;
if(this._allItems===null){
this._allItems={};
try{
if(this.gatherHandle){
this._waiting.push({"forFunc":_1.hitch(this,this._handleRule),"finishFunc":_1.hitch(this,this._handleReturn)});
}else{
_9.rules.forEach(_1.hitch(this,this._handleRule),null,this.context);
this._handleReturn();
}
}
catch(e){
if(_29.onError){
_29.onError.call(_2a,e,_29);
}
}
}else{
this._handleReturn();
}
},_handleRule:function(_2b,_2c,_2d){
var _2e=_2b["selectorText"];
var s=_2e.split(" ");
var _2f=[];
for(var j=0;j<s.length;j++){
var tmp=s[j];
var _30=tmp.indexOf(".");
if(tmp&&tmp.length>0&&_30!==-1){
var _31=tmp.indexOf(",")||tmp.indexOf("[");
tmp=tmp.substring(_30,((_31!==-1&&_31>_30)?_31:tmp.length));
_2f.push(tmp);
}
}
var _32={};
_32.rule=_2b;
_32.styleSheet=_2c;
_32.href=_2d;
_32.classes=_2f;
_32[this._storeRef]=this;
if(!this._allItems[_2e]){
this._allItems[_2e]=[];
}
this._allItems[_2e].push(_32);
},_handleReturn:function(){
var _33=[];
var _34=[];
var _35=null;
for(var i in this._allItems){
_35=this._allItems[i];
for(var j in _35){
_34.push(_35[j]);
}
}
var _36;
while(this._pending.length){
_36=this._pending.pop();
_36.request._items=_34;
_33.push(_36);
}
while(_33.length){
_36=_33.pop();
this._handleFetchReturn(_36.request);
}
},_handleFetchReturn:function(_37){
var _38=_37.scope||_5.global;
var _39=[];
var _3a="all";
var i;
if(_37.query){
_3a=_4.toJson(_37.query);
}
if(this._cache[_3a]){
_39=this._cache[_3a];
}else{
if(_37.query){
for(i in _37._items){
var _3b=_37._items[i];
var _3c=(_37.queryOptions?_37.queryOptions.ignoreCase:false);
var _3d={};
var key;
var _3e;
for(key in _37.query){
_3e=_37.query[key];
if(typeof _3e==="string"){
_3d[key]=_8.patternToRegExp(_3e,_3c);
}
}
var _3f=true;
for(key in _37.query){
_3e=_37.query[key];
if(!this._containsValue(_3b,key,_3e,_3d[key])){
_3f=false;
}
}
if(_3f){
_39.push(_3b);
}
}
this._cache[_3a]=_39;
}else{
for(i in _37._items){
_39.push(_37._items[i]);
}
}
}
var _40=_39.length;
if(_37.sort){
_39.sort(_7.createSortFunction(_37.sort,this));
}
var _41=0;
var _42=_39.length;
if(_37.start>0&&_37.start<_39.length){
_41=_37.start;
}
if(_37.count&&_37.count){
_42=_37.count;
}
var _43=_41+_42;
if(_43>_39.length){
_43=_39.length;
}
_39=_39.slice(_41,_43);
if(_37.onBegin){
_37.onBegin.call(_38,_40,_37);
}
if(_37.onItem){
if(_1.isArray(_39)){
for(i=0;i<_39.length;i++){
_37.onItem.call(_38,_39[i],_37);
}
if(_37.onComplete){
_37.onComplete.call(_38,null,_37);
}
}
}else{
if(_37.onComplete){
_37.onComplete.call(_38,_39,_37);
}
}
return _37;
},close:function(){
this._cache={};
this._allItems=null;
},_assertIsItem:function(_44){
if(!this.isItem(_44)){
throw new Error(this._cName+": Invalid item argument.");
}
},_assertIsAttribute:function(_45){
if(typeof _45!=="string"){
throw new Error(this._cName+": Invalid attribute argument.");
}
},_containsValue:function(_46,_47,_48,_49){
return _3.some(this.getValues(_46,_47),function(_4a){
if(_4a!==null&&!_1.isObject(_4a)&&_49){
if(_4a.toString().match(_49)){
return true;
}
}else{
if(_48===_4a){
return true;
}
}
return false;
});
}});
});
