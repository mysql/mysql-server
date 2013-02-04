//>>built
define("dojox/app/animation",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/Deferred","dojo/DeferredList","dojo/on","dojo/_base/sniff"],function(_1,_2,_3,_4,_5,_6,on,_7){
var _8="transitionend";
var _9="t";
var _a="translate3d(";
var _b=",0,0)";
if(_7("webkit")){
_9="WebkitT";
_8="webkitTransitionEnd";
}else{
if(_7("mozilla")){
_9="MozT";
_a="translateX(";
_b=")";
}
}
_3("dojox.app.animation",null,{constructor:function(_c){
var _d={startState:{},endState:{},node:null,duration:250,"in":true,direction:1,autoClear:true};
_2.mixin(this,_d);
_2.mixin(this,_c);
if(!this.deferred){
this.deferred=new _5();
}
},play:function(){
dojox.app.animation.groupedPlay([this]);
},_applyState:function(_e){
var _f=this.node.style;
for(var _10 in _e){
if(_e.hasOwnProperty(_10)){
_f[_10]=_e[_10];
}
}
},initState:function(){
this.node.style[_9+"ransitionProperty"]="none";
this.node.style[_9+"ransitionDuration"]="0ms";
this._applyState(this.startState);
},_beforeStart:function(){
if(this.node.style.display==="none"){
this.node.style.display="";
}
this.beforeStart();
},_beforeClear:function(){
this.node.style[_9+"ransitionProperty"]=null;
this.node.style[_9+"ransitionDuration"]=null;
if(this["in"]!==true){
this.node.style.display="none";
}
this.beforeClear();
},_onAfterEnd:function(){
this.deferred.resolve(this.node);
if(this.node.id&&dojox.app.animation.playing[this.node.id]===this.deferred){
delete dojox.app.animation.playing[this.node.id];
}
this.onAfterEnd();
},beforeStart:function(){
},beforeClear:function(){
},onAfterEnd:function(){
},start:function(){
this._beforeStart();
var _11=this;
_11.node.style[_9+"ransitionProperty"]="all";
_11.node.style[_9+"ransitionDuration"]=_11.duration+"ms";
on.once(_11.node,_8,function(){
_11.clear();
});
this._applyState(this.endState);
},clear:function(){
this._beforeClear();
this._removeState(this.endState);
this._onAfterEnd();
},_removeState:function(_12){
var _13=this.node.style;
for(var _14 in _12){
if(_12.hasOwnProperty(_14)){
_13[_14]=null;
}
}
}});
dojox.app.animation.slide=function(_15,_16){
var ret=new dojox.app.animation(_16);
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
ret.startState[_9+"ransform"]=_a+_17+_b;
ret.endState[_9+"ransform"]=_a+_18+_b;
return ret;
};
dojox.app.animation.fade=function(_19,_1a){
var ret=new dojox.app.animation(_1a);
ret.node=_19;
var _1b="0";
var _1c="0";
if(ret["in"]){
_1c="1";
}else{
_1b="1";
}
_2.mixin(ret,{startState:{"opacity":_1b},endState:{"opacity":_1c}});
return ret;
};
dojox.app.animation.flip=function(_1d,_1e){
var ret=new dojox.app.animation(_1e);
ret.node=_1d;
if(ret["in"]){
_2.mixin(ret,{startState:{"opacity":"0"},endState:{"opacity":"1"}});
ret.startState[_9+"ransform"]="scale(0,0.8) skew(0,-30deg)";
ret.endState[_9+"ransform"]="scale(1,1) skew(0,0)";
}else{
_2.mixin(ret,{startState:{"opacity":"1"},endState:{"opacity":"0"}});
ret.startState[_9+"ransform"]="scale(1,1) skew(0,0)";
ret.endState[_9+"ransform"]="scale(0,0.8) skew(0,30deg)";
}
return ret;
};
var _1f=function(_20){
var _21=[];
_4.forEach(_20,function(_22){
if(_22.id&&dojox.app.animation.playing[_22.id]){
_21.push(dojox.app.animation.playing[_22.id]);
}
});
return new _6(_21);
};
dojox.app.animation.getWaitingList=_1f;
dojox.app.animation.groupedPlay=function(_23){
var _24=_4.filter(_23,function(_25){
return _25.node;
});
var _26=_1f(_24);
_4.forEach(_23,function(_27){
if(_27.node.id){
dojox.app.animation.playing[_27.node.id]=_27.deferred;
}
});
_1.when(_26,function(){
_4.forEach(_23,function(_28){
_28.initState();
});
setTimeout(function(){
_4.forEach(_23,function(_29){
_29.start();
});
},33);
});
};
dojox.app.animation.chainedPlay=function(_2a){
var _2b=_4.filter(_2a,function(_2c){
return _2c.node;
});
var _2d=_1f(_2b);
_4.forEach(_2a,function(_2e){
if(_2e.node.id){
dojox.app.animation.playing[_2e.node.id]=_2e.deferred;
}
});
_1.when(_2d,function(){
_4.forEach(_2a,function(_2f){
_2f.initState();
});
for(var i=1,len=_2a.length;i<len;i++){
_2a[i-1].deferred.then(_2.hitch(_2a[i],function(){
this.start();
}));
}
setTimeout(function(){
_2a[0].start();
},33);
});
};
dojox.app.animation.playing={};
return dojox.app.animation;
});
