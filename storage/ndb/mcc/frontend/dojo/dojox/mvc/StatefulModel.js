//>>built
define("dojox/mvc/StatefulModel",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/Stateful","./getStateful","./getPlainValue","./StatefulArray"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_1.deprecated("dojox/mvc/StatefulModel","Use dojox/mvc/getStateful, dojox/mvc/getPlainValue, dojox/mvc/StatefulArray or one of the dojox/mvc/*RefControllers instead");
var _9=_4("dojox.mvc.StatefulModel",[_5],{data:null,store:null,valid:true,value:"",reset:function(){
if(_2.isObject(this.data)&&!(this.data instanceof Date)&&!(this.data instanceof RegExp)){
for(var x in this){
if(this[x]&&_2.isFunction(this[x].reset)){
this[x].reset();
}
}
}else{
this.set("value",this.data);
}
},commit:function(_a){
this._commit();
var ds=_a||this.store;
if(ds){
this._saveToStore(ds);
}
},toPlainObject:function(){
return _7(this,_9.getPlainValueOptions);
},splice:function(_b,n){
var a=(new _8([])).splice.apply(this,_2._toArray(arguments));
for(var i=0;i<a.length;i++){
(this._removals=this._removals||[]).push(a[i].toPlainObject());
}
return a;
},add:function(_c,_d){
if(typeof this.get("length")==="number"&&/^[0-9]+$/.test(_c.toString())){
if(this.get("length")<(_c-0)){
throw new Error("Out of bounds insert attempted, must be contiguous.");
}
this.splice(_c-0,0,_d);
}else{
this.set(_c,_d);
}
},remove:function(_e){
if(typeof this.get("length")==="number"&&/^[0-9]+$/.test(_e.toString())){
if(!this.get(_e)){
throw new Error("Out of bounds delete attempted - no such index: "+n);
}else{
this.splice(_e-0,1);
}
}else{
var _f=this.get(_e);
if(!_f){
throw new Error("Illegal delete attempted - no such property: "+_e);
}else{
this._removals=this._removals||[];
this._removals.push(_f.toPlainObject());
this.set(_e,undefined);
delete this[_e];
}
}
},valueOf:function(){
return this.toPlainObject();
},toString:function(){
return this.value===""&&this.data?this.data.toString():this.value.toString();
},constructor:function(_10){
var _11=(_10&&"data" in _10)?_10.data:this.data;
this._createModel(_11);
},_createModel:function(_12){
if(_12!=null){
_12=_6(_12,_9.getStatefulOptions);
if(_2.isArray(_12)){
this.length=0;
[].splice.apply(this,_12);
}else{
if(_2.isObject(_12)){
for(var s in _12){
if(_12.hasOwnProperty(s)){
this[s]=_12[s];
}
}
}else{
this.set("value",_12);
}
}
}
},_commit:function(){
for(var x in this){
if(this[x]&&_2.isFunction(this[x]._commit)){
this[x]._commit();
}
}
this.data=this.toPlainObject();
},_saveToStore:function(_13){
if(this._removals){
_3.forEach(this._removals,function(d){
_13.remove(_13.getIdentity(d));
},this);
delete this._removals;
}
var _14=this.toPlainObject();
if(_2.isArray(_14)){
_3.forEach(_14,function(d){
_13.put(d);
},this);
}else{
_13.put(_14);
}
}});
_2.mixin(_9,{getStatefulOptions:{getType:function(v){
return _2.isArray(v)?"array":v!=null&&{}.toString.call(v)=="[object Object]"?"object":"value";
},getStatefulArray:function(a){
var _15=this,_16=_2.mixin(new _8(_3.map(a,function(_17){
return _6(_17,_15);
})));
for(var s in _9.prototype){
if(s!="set"){
_16[s]=_9.prototype[s];
}
}
_16.data=a;
return _16;
},getStatefulObject:function(o){
var _18=new _9();
_18.data=o;
for(var s in o){
_18.set(s,_6(o[s],this));
}
return _18;
},getStatefulValue:function(v){
var _19=new _9();
_19.data=v;
_19.set("value",v);
return _19;
}},getPlainValueOptions:{getType:function(v){
if(_2.isArray(v)){
return "array";
}
if(_2.isObject(v)){
for(var s in v){
if(v.hasOwnProperty(s)&&s!="value"&&(v[s]||{}).get&&(v[s]||{}).watch){
return "object";
}
}
}
return "value";
},getPlainArray:function(a){
return _3.map(a,function(_1a){
return _7(_1a,this);
},this);
},getPlainObject:function(o){
var _1b={};
for(var s in o){
if(s=="_watchCallbacks"||(s in _9.prototype)){
continue;
}
_1b[s]=_7(o[s],this);
}
return _1b;
},getPlainValue:function(v){
return (v||{}).set&&(v||{}).watch?_7(v.value,this):v;
}}});
return _9;
});
