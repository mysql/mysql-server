//>>built
define("dojox/css3/transition",["dojo/_base/lang","dojo/_base/array","dojo/Deferred","dojo/when","dojo/promise/all","dojo/on","dojo/sniff"],function(_1,_2,_3,_4,_5,on,_6){
var _7="transitionend";
var _8="t";
var _9="translate3d(";
var _a=",0,0)";
if(_6("webkit")){
_8="WebkitT";
_7="webkitTransitionEnd";
}else{
if(_6("mozilla")){
_8="MozT";
_9="translateX(";
_a=")";
}
}
var _b=function(_c){
var _d={startState:{},endState:{},node:null,duration:250,"in":true,direction:1,autoClear:true};
_1.mixin(this,_d);
_1.mixin(this,_c);
if(!this.deferred){
this.deferred=new _3();
}
};
_1.extend(_b,{play:function(){
_b.groupedPlay([this]);
},_applyState:function(_e){
var _f=this.node.style;
for(var _10 in _e){
if(_e.hasOwnProperty(_10)){
_f[_10]=_e[_10];
}
}
},initState:function(){
this.node.style[_8+"ransitionProperty"]="none";
this.node.style[_8+"ransitionDuration"]="0ms";
this._applyState(this.startState);
},_beforeStart:function(){
if(this.node.style.display==="none"){
this.node.style.display="";
}
this.beforeStart();
},_beforeClear:function(){
this.node.style[_8+"ransitionProperty"]="";
this.node.style[_8+"ransitionDuration"]="";
if(this["in"]!==true){
this.node.style.display="none";
}
this.beforeClear();
},_onAfterEnd:function(){
this.deferred.resolve(this.node);
if(this.node.id&&_b.playing[this.node.id]===this.deferred){
delete _b.playing[this.node.id];
}
this.onAfterEnd();
},beforeStart:function(){
},beforeClear:function(){
},onAfterEnd:function(){
},start:function(){
this._beforeStart();
this._startTime=new Date().getTime();
this._cleared=false;
var _11=this;
_11.node.style[_8+"ransitionProperty"]="all";
_11.node.style[_8+"ransitionDuration"]=_11.duration+"ms";
on.once(_11.node,_7,function(){
_11.clear();
});
this._applyState(this.endState);
},clear:function(){
if(this._cleared){
return;
}
this._cleared=true;
this._beforeClear();
this._removeState(this.endState);
this._onAfterEnd();
},_removeState:function(_12){
var _13=this.node.style;
for(var _14 in _12){
if(_12.hasOwnProperty(_14)){
_13[_14]="";
}
}
}});
_b.slide=function(_15,_16){
var ret=new _b(_16);
ret.node=_15;
var _17="0";
var _18="0";
if(ret["in"]){
if(ret.direction===1){
_17="100%";
}else{
_17="-100%";
}
}else{
if(ret.direction===1){
_18="-100%";
}else{
_18="100%";
}
}
ret.startState[_8+"ransform"]=_9+_17+_a;
ret.endState[_8+"ransform"]=_9+_18+_a;
return ret;
};
_b.fade=function(_19,_1a){
var ret=new _b(_1a);
ret.node=_19;
var _1b="0";
var _1c="0";
if(ret["in"]){
_1c="1";
}else{
_1b="1";
}
_1.mixin(ret,{startState:{"opacity":_1b},endState:{"opacity":_1c}});
return ret;
};
_b.flip=function(_1d,_1e){
var ret=new _b(_1e);
ret.node=_1d;
if(ret["in"]){
_1.mixin(ret,{startState:{"opacity":"0"},endState:{"opacity":"1"}});
ret.startState[_8+"ransform"]="scale(0,0.8) skew(0,-30deg)";
ret.endState[_8+"ransform"]="scale(1,1) skew(0,0)";
}else{
_1.mixin(ret,{startState:{"opacity":"1"},endState:{"opacity":"0"}});
ret.startState[_8+"ransform"]="scale(1,1) skew(0,0)";
ret.endState[_8+"ransform"]="scale(0,0.8) skew(0,30deg)";
}
return ret;
};
var _1f=function(_20){
var _21=[];
_2.forEach(_20,function(_22){
if(_22.id&&_b.playing[_22.id]){
_21.push(_b.playing[_22.id]);
}
});
return _5(_21);
};
_b.getWaitingList=_1f;
_b.groupedPlay=function(_23){
var _24=_2.filter(_23,function(_25){
return _25.node;
});
var _26=_1f(_24);
_2.forEach(_23,function(_27){
if(_27.node.id){
_b.playing[_27.node.id]=_27.deferred;
}
});
_4(_26,function(){
_2.forEach(_23,function(_28){
_28.initState();
});
setTimeout(function(){
_2.forEach(_23,function(_29){
_29.start();
});
on.once(_23[_23.length-1].node,_7,function(){
var _2a;
for(var i=0;i<_23.length-1;i++){
if(_23[i].deferred.fired!==0&&!_23[i]._cleared){
_2a=new Date().getTime()-_23[i]._startTime;
if(_2a>=_23[i].duration){
_23[i].clear();
}
}
}
});
setTimeout(function(){
var _2b;
for(var i=0;i<_23.length;i++){
if(_23[i].deferred.fired!==0&&!_23[i]._cleared){
_2b=new Date().getTime()-_23[i]._startTime;
if(_2b>=_23[i].duration){
_23[i].clear();
}
}
}
},_23[0].duration+50);
},33);
});
};
_b.chainedPlay=function(_2c){
var _2d=_2.filter(_2c,function(_2e){
return _2e.node;
});
var _2f=_1f(_2d);
_2.forEach(_2c,function(_30){
if(_30.node.id){
_b.playing[_30.node.id]=_30.deferred;
}
});
_4(_2f,function(){
_2.forEach(_2c,function(_31){
_31.initState();
});
for(var i=1,len=_2c.length;i<len;i++){
_2c[i-1].deferred.then(_1.hitch(_2c[i],function(){
this.start();
}));
}
setTimeout(function(){
_2c[0].start();
},33);
});
};
_b.playing={};
return _b;
});
