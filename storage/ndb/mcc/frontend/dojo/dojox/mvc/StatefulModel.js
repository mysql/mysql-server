//>>built
define("dojox/mvc/StatefulModel",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/Stateful"],function(_1,_2,_3,_4){
var _5=_3("dojox.mvc.StatefulModel",[_4],{data:null,store:null,valid:true,value:"",reset:function(){
if(_1.isObject(this.data)&&!(this.data instanceof Date)&&!(this.data instanceof RegExp)){
for(var x in this){
if(this[x]&&_1.isFunction(this[x].reset)){
this[x].reset();
}
}
}else{
this.set("value",this.data);
}
},commit:function(_6){
this._commit();
var ds=_6||this.store;
if(ds){
this._saveToStore(ds);
}
},toPlainObject:function(){
var _7={};
var _8=false;
for(var p in this){
if(this[p]&&_1.isFunction(this[p].toPlainObject)){
if(!_8&&typeof this.get("length")==="number"){
_7=[];
}
_8=true;
_7[p]=this[p].toPlainObject();
}
}
if(!_8){
if(this.get("length")===0){
_7=[];
}else{
_7=this.value;
}
}
return _7;
},add:function(_9,_a){
var n,n1,_b,_c,_d=new _5({data:""});
if(typeof this.get("length")==="number"&&/^[0-9]+$/.test(_9.toString())){
n=_9;
if(!this.get(n)){
if(this.get("length")==0&&n==0){
this.set(n,_a);
}else{
n1=n-1;
if(!this.get(n1)){
throw new Error("Out of bounds insert attempted, must be contiguous.");
}
this.set(n,_a);
}
}else{
n1=n-0+1;
_b=_a;
_c=this.get(n1);
if(!_c){
this.set(n1,_b);
}else{
do{
this._copyStatefulProperties(_c,_d);
this._copyStatefulProperties(_b,_c);
this._copyStatefulProperties(_d,_b);
this.set(n1,_c);
_c=this.get(++n1);
}while(_c);
this.set(n1,_b);
}
}
this.set("length",this.get("length")+1);
}else{
this.set(_9,_a);
}
},remove:function(_e){
var n,_f,_10;
if(typeof this.get("length")==="number"&&/^[0-9]+$/.test(_e.toString())){
n=_e;
_f=this.get(n);
if(!_f){
throw new Error("Out of bounds delete attempted - no such index: "+n);
}else{
this._removals=this._removals||[];
this._removals.push(_f.toPlainObject());
n1=n-0+1;
_10=this.get(n1);
if(!_10){
this.set(n,undefined);
delete this[n];
}else{
while(_10){
this._copyStatefulProperties(_10,_f);
_f=this.get(n1++);
_10=this.get(n1);
}
this.set(n1-1,undefined);
delete this[n1-1];
}
this.set("length",this.get("length")-1);
}
}else{
_f=this.get(_e);
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
},constructor:function(_11){
var _12=(_11&&"data" in _11)?_11.data:this.data;
this._createModel(_12);
},_createModel:function(obj){
if(_1.isObject(obj)&&!(obj instanceof Date)&&!(obj instanceof RegExp)&&obj!==null){
for(var x in obj){
var _13=new _5({data:obj[x]});
this.set(x,_13);
}
if(_1.isArray(obj)){
this.set("length",obj.length);
}
}else{
this.set("value",obj);
}
},_commit:function(){
for(var x in this){
if(this[x]&&_1.isFunction(this[x]._commit)){
this[x]._commit();
}
}
this.data=this.toPlainObject();
},_saveToStore:function(_14){
if(this._removals){
_2.forEach(this._removals,function(d){
_14.remove(_14.getIdentity(d));
},this);
delete this._removals;
}
var _15=this.toPlainObject();
if(_1.isArray(_15)){
_2.forEach(_15,function(d){
_14.put(d);
},this);
}else{
_14.put(_15);
}
},_copyStatefulProperties:function(src,_16){
for(var x in src){
var o=src.get(x);
if(o&&_1.isObject(o)&&_1.isFunction(o.get)){
_16.set(x,o);
}
}
}});
return _5;
});
