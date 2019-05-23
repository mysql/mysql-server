//>>built
define("dojox/mvc/StatefulArray",["dojo/_base/lang","dojo/Stateful"],function(_1,_2){
function _3(a){
if(a._watchElementCallbacks){
a._watchElementCallbacks();
}
return a;
};
var _4=function(a){
var _5=_1._toArray(a||[]);
var _6=_4;
_6._meta={bases:[_2]};
_5.constructor=_6;
return _1.mixin(_5,{pop:function(){
return this.splice(this.get("length")-1,1)[0];
},push:function(){
this.splice.apply(this,[this.get("length"),0].concat(_1._toArray(arguments)));
return this.get("length");
},reverse:function(){
return _3([].reverse.apply(this,_1._toArray(arguments)));
},shift:function(){
return this.splice(0,1)[0];
},sort:function(){
return _3([].sort.apply(this,_1._toArray(arguments)));
},splice:function(_7,n){
var l=this.get("length");
_7+=_7<0?l:0;
var p=Math.min(_7,l),_8=this.slice(_7,_7+n),_9=_1._toArray(arguments).slice(2);
[].splice.apply(this,[_7,n].concat(new Array(_9.length)));
for(var i=0;i<_9.length;i++){
this.set(p+i,_9[i]);
}
if(this._watchElementCallbacks){
this._watchElementCallbacks(_7,_8,_9);
}
if(this._watchCallbacks){
this._watchCallbacks("length",l,l-_8.length+_9.length);
}
return _8;
},unshift:function(){
this.splice.apply(this,[0,0].concat(_1._toArray(arguments)));
return this.get("length");
},concat:function(a){
return new _4([].concat.apply(this,arguments));
},join:function(_a){
var _b=[];
for(var l=this.get("length"),i=0;i<l;i++){
_b.push(this.get(i));
}
return _b.join(_a);
},slice:function(_c,_d){
var l=this.get("length");
_c+=_c<0?l:0;
_d=(_d===void 0?l:_d)+(_d<0?l:0);
var _e=[];
for(var i=_c||0;i<Math.min(_d,this.get("length"));i++){
_e.push(this.get(i));
}
return new _4(_e);
},watchElements:function(_f){
var _10=this._watchElementCallbacks,_11=this;
if(!_10){
_10=this._watchElementCallbacks=function(idx,_12,_13){
for(var _14=[].concat(_10.list),i=0;i<_14.length;i++){
_14[i].call(_11,idx,_12,_13);
}
};
_10.list=[];
}
_10.list.push(_f);
var h={};
h.unwatch=h.remove=function(){
for(var _15=_10.list,i=0;i<_15.length;i++){
if(_15[i]==_f){
_15.splice(i,1);
break;
}
}
};
return h;
}},_2.prototype,{set:function(_16,_17){
if(_16=="length"){
var old=this.get("length");
if(old<_17){
this.splice.apply(this,[old,0].concat(new Array(_17-old)));
}else{
if(_17>old){
this.splice.apply(this,[_17,old-_17]);
}
}
return this;
}else{
var _18=this.length;
_2.prototype.set.call(this,_16,_17);
if(_18!=this.length){
_2.prototype.set.call(this,"length",this.length);
}
return this;
}
}});
};
return _1.setObject("dojox.mvc.StatefulArray",_4);
});
