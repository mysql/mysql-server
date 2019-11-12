//>>built
define("dojox/css3/transition",["dojo/_base/lang","dojo/_base/array","dojo/_base/Deferred","dojo/DeferredList","dojo/on","dojo/_base/sniff"],function(_1,_2,_3,_4,on,_5){
var _6="transitionend";
var _7="t";
var _8="translate3d(";
var _9=",0,0)";
if(_5("webkit")){
_7="WebkitT";
_6="webkitTransitionEnd";
}else{
if(_5("mozilla")){
_7="MozT";
_8="translateX(";
_9=")";
}
}
var _a=function(_b){
var _c={startState:{},endState:{},node:null,duration:250,"in":true,direction:1,autoClear:true};
_1.mixin(this,_c);
_1.mixin(this,_b);
if(!this.deferred){
this.deferred=new _3();
}
};
_1.extend(_a,{play:function(){
_a.groupedPlay([this]);
},_applyState:function(_d){
var _e=this.node.style;
for(var _f in _d){
if(_d.hasOwnProperty(_f)){
_e[_f]=_d[_f];
}
}
},initState:function(){
this.node.style[_7+"ransitionProperty"]="none";
this.node.style[_7+"ransitionDuration"]="0ms";
this._applyState(this.startState);
},_beforeStart:function(){
if(this.node.style.display==="none"){
this.node.style.display="";
}
this.beforeStart();
},_beforeClear:function(){
this.node.style[_7+"ransitionProperty"]=null;
this.node.style[_7+"ransitionDuration"]=null;
if(this["in"]!==true){
this.node.style.display="none";
}
this.beforeClear();
},_onAfterEnd:function(){
this.deferred.resolve(this.node);
if(this.node.id&&_a.playing[this.node.id]===this.deferred){
delete _a.playing[this.node.id];
}
this.onAfterEnd();
},beforeStart:function(){
},beforeClear:function(){
},onAfterEnd:function(){
},start:function(){
this._beforeStart();
this._startTime=new Date().getTime();
this._cleared=false;
var _10=this;
_10.node.style[_7+"ransitionProperty"]="all";
_10.node.style[_7+"ransitionDuration"]=_10.duration+"ms";
on.once(_10.node,_6,function(){
_10.clear();
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
},_removeState:function(_11){
var _12=this.node.style;
for(var _13 in _11){
if(_11.hasOwnProperty(_13)){
_12[_13]=null;
}
}
}});
_a.slide=function(_14,_15){
var ret=new _a(_15);
ret.node=_14;
var _16="0";
var _17="0";
if(ret["in"]){
if(ret.direction===1){
_16="100%";
}else{
_16="-100%";
}
}else{
if(ret.direction===1){
_17="-100%";
}else{
_17="100%";
}
}
ret.startState[_7+"ransform"]=_8+_16+_9;
ret.endState[_7+"ransform"]=_8+_17+_9;
return ret;
};
_a.fade=function(_18,_19){
var ret=new _a(_19);
ret.node=_18;
var _1a="0";
var _1b="0";
if(ret["in"]){
_1b="1";
}else{
_1a="1";
}
_1.mixin(ret,{startState:{"opacity":_1a},endState:{"opacity":_1b}});
return ret;
};
_a.flip=function(_1c,_1d){
var ret=new _a(_1d);
ret.node=_1c;
if(ret["in"]){
_1.mixin(ret,{startState:{"opacity":"0"},endState:{"opacity":"1"}});
ret.startState[_7+"ransform"]="scale(0,0.8) skew(0,-30deg)";
ret.endState[_7+"ransform"]="scale(1,1) skew(0,0)";
}else{
_1.mixin(ret,{startState:{"opacity":"1"},endState:{"opacity":"0"}});
ret.startState[_7+"ransform"]="scale(1,1) skew(0,0)";
ret.endState[_7+"ransform"]="scale(0,0.8) skew(0,30deg)";
}
return ret;
};
var _1e=function(_1f){
var _20=[];
_2.forEach(_1f,function(_21){
if(_21.id&&_a.playing[_21.id]){
_20.push(_a.playing[_21.id]);
}
});
return new _4(_20);
};
_a.getWaitingList=_1e;
_a.groupedPlay=function(_22){
var _23=_2.filter(_22,function(_24){
return _24.node;
});
var _25=_1e(_23);
_2.forEach(_22,function(_26){
if(_26.node.id){
_a.playing[_26.node.id]=_26.deferred;
}
});
_3.when(_25,function(){
_2.forEach(_22,function(_27){
_27.initState();
});
setTimeout(function(){
_2.forEach(_22,function(_28){
_28.start();
});
on.once(_22[_22.length-1].node,_6,function(){
var _29;
for(var i=0;i<_22.length-1;i++){
if(_22[i].deferred.fired!==0){
_29=new Date().getTime()-_22[i]._startTime;
if(_29>=_22[i].duration){
_22[i].clear();
}
}
}
});
},33);
});
};
_a.chainedPlay=function(_2a){
var _2b=_2.filter(_2a,function(_2c){
return _2c.node;
});
var _2d=_1e(_2b);
_2.forEach(_2a,function(_2e){
if(_2e.node.id){
_a.playing[_2e.node.id]=_2e.deferred;
}
});
_3.when(_2d,function(){
_2.forEach(_2a,function(_2f){
_2f.initState();
});
for(var i=1,len=_2a.length;i<len;i++){
_2a[i-1].deferred.then(_1.hitch(_2a[i],function(){
this.start();
}));
}
setTimeout(function(){
_2a[0].start();
},33);
});
};
_a.playing={};
return _a;
});
