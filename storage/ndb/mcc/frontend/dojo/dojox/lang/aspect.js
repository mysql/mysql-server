//>>built
define("dojox/lang/aspect",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.aspect");
(function(){
var d=_1,_4=_3.lang.aspect,ap=Array.prototype,_5=[],_6;
var _7=function(){
this.next_before=this.prev_before=this.next_around=this.prev_around=this.next_afterReturning=this.prev_afterReturning=this.next_afterThrowing=this.prev_afterThrowing=this;
this.counter=0;
};
d.extend(_7,{add:function(_8){
var _9=d.isFunction(_8),_a={advice:_8,dynamic:_9};
this._add(_a,"before","",_9,_8);
this._add(_a,"around","",_9,_8);
this._add(_a,"after","Returning",_9,_8);
this._add(_a,"after","Throwing",_9,_8);
++this.counter;
return _a;
},_add:function(_b,_c,_d,_e,_f){
var _10=_c+_d;
if(_e||_f[_c]||(_d&&_f[_10])){
var _11="next_"+_10,_12="prev_"+_10;
(_b[_12]=this[_12])[_11]=_b;
(_b[_11]=this)[_12]=_b;
}
},remove:function(_13){
this._remove(_13,"before");
this._remove(_13,"around");
this._remove(_13,"afterReturning");
this._remove(_13,"afterThrowing");
--this.counter;
},_remove:function(_14,_15){
var _16="next_"+_15,_17="prev_"+_15;
if(_14[_16]){
_14[_16][_17]=_14[_17];
_14[_17][_16]=_14[_16];
}
},isEmpty:function(){
return !this.counter;
}});
var _18=function(){
return function(){
var _19=arguments.callee,_1a=_19.advices,ret,i,a,e,t;
if(_6){
_5.push(_6);
}
_6={instance:this,joinPoint:_19,depth:_5.length,around:_1a.prev_around,dynAdvices:[],dynIndex:0};
try{
for(i=_1a.prev_before;i!=_1a;i=i.prev_before){
if(i.dynamic){
_6.dynAdvices.push(a=new i.advice(_6));
if(t=a.before){
t.apply(a,arguments);
}
}else{
t=i.advice;
t.before.apply(t,arguments);
}
}
try{
ret=(_1a.prev_around==_1a?_19.target:_4.proceed).apply(this,arguments);
}
catch(e){
_6.dynIndex=_6.dynAdvices.length;
for(i=_1a.next_afterThrowing;i!=_1a;i=i.next_afterThrowing){
a=i.dynamic?_6.dynAdvices[--_6.dynIndex]:i.advice;
if(t=a.afterThrowing){
t.call(a,e);
}
if(t=a.after){
t.call(a);
}
}
throw e;
}
_6.dynIndex=_6.dynAdvices.length;
for(i=_1a.next_afterReturning;i!=_1a;i=i.next_afterReturning){
a=i.dynamic?_6.dynAdvices[--_6.dynIndex]:i.advice;
if(t=a.afterReturning){
t.call(a,ret);
}
if(t=a.after){
t.call(a);
}
}
var ls=_19._listeners;
for(i in ls){
if(!(i in ap)){
ls[i].apply(this,arguments);
}
}
}
finally{
for(i=0;i<_6.dynAdvices.length;++i){
a=_6.dynAdvices[i];
if(a.destroy){
a.destroy();
}
}
_6=_5.length?_5.pop():null;
}
return ret;
};
};
_4.advise=function(obj,_1b,_1c){
if(typeof obj!="object"){
obj=obj.prototype;
}
var _1d=[];
if(!(_1b instanceof Array)){
_1b=[_1b];
}
for(var j=0;j<_1b.length;++j){
var t=_1b[j];
if(t instanceof RegExp){
for(var i in obj){
if(d.isFunction(obj[i])&&t.test(i)){
_1d.push(i);
}
}
}else{
if(d.isFunction(obj[t])){
_1d.push(t);
}
}
}
if(!d.isArray(_1c)){
_1c=[_1c];
}
return _4.adviseRaw(obj,_1d,_1c);
};
_4.adviseRaw=function(obj,_1e,_1f){
if(!_1e.length||!_1f.length){
return null;
}
var m={},al=_1f.length;
for(var i=_1e.length-1;i>=0;--i){
var _20=_1e[i],o=obj[_20],ao=new Array(al),t=o.advices;
if(!t){
var x=obj[_20]=_18();
x.target=o.target||o;
x.targetName=_20;
x._listeners=o._listeners||[];
x.advices=new _7;
t=x.advices;
}
for(var j=0;j<al;++j){
ao[j]=t.add(_1f[j]);
}
m[_20]=ao;
}
return [obj,m];
};
_4.unadvise=function(_21){
if(!_21){
return;
}
var obj=_21[0],_22=_21[1];
for(var _23 in _22){
var o=obj[_23],t=o.advices,ao=_22[_23];
for(var i=ao.length-1;i>=0;--i){
t.remove(ao[i]);
}
if(t.isEmpty()){
var _24=true,ls=o._listeners;
if(ls.length){
for(i in ls){
if(!(i in ap)){
_24=false;
break;
}
}
}
if(_24){
obj[_23]=o.target;
}else{
var x=obj[_23]=d._listener.getDispatcher();
x.target=o.target;
x._listeners=ls;
}
}
}
};
_4.getContext=function(){
return _6;
};
_4.getContextStack=function(){
return _5;
};
_4.proceed=function(){
var _25=_6.joinPoint,_26=_25.advices;
for(var c=_6.around;c!=_26;c=_6.around){
_6.around=c.prev_around;
if(c.dynamic){
var a=_6.dynAdvices[_6.dynIndex++],t=a.around;
if(t){
return t.apply(a,arguments);
}
}else{
return c.advice.around.apply(c.advice,arguments);
}
}
return _25.target.apply(_6.instance,arguments);
};
})();
});
