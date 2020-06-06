/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/fx",["./_base/lang","./Evented","./_base/kernel","./_base/array","./aspect","./_base/fx","./dom","./dom-style","./dom-geometry","./ready","require"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
if(!_3.isAsync){
_a(0,function(){
var _c=["./fx/Toggler"];
_b(_c);
});
}
var _d=_3.fx={};
var _e={_fire:function(_f,_10){
if(this[_f]){
this[_f].apply(this,_10||[]);
}
return this;
}};
var _11=function(_12){
this._index=-1;
this._animations=_12||[];
this._current=this._onAnimateCtx=this._onEndCtx=null;
this.duration=0;
_4.forEach(this._animations,function(a){
if(a){
if(typeof a.duration!="undefined"){
this.duration+=a.duration;
}
if(a.delay){
this.duration+=a.delay;
}
}
},this);
};
_11.prototype=new _2();
_1.extend(_11,{_onAnimate:function(){
this._fire("onAnimate",arguments);
},_onEnd:function(){
this._onAnimateCtx.remove();
this._onEndCtx.remove();
this._onAnimateCtx=this._onEndCtx=null;
if(this._index+1==this._animations.length){
this._fire("onEnd");
}else{
this._current=this._animations[++this._index];
this._onAnimateCtx=_5.after(this._current,"onAnimate",_1.hitch(this,"_onAnimate"),true);
this._onEndCtx=_5.after(this._current,"onEnd",_1.hitch(this,"_onEnd"),true);
this._current.play(0,true);
}
},play:function(_13,_14){
if(!this._current){
this._current=this._animations[this._index=0];
}
if(!_14&&this._current.status()=="playing"){
return this;
}
var _15=_5.after(this._current,"beforeBegin",_1.hitch(this,function(){
this._fire("beforeBegin");
}),true),_16=_5.after(this._current,"onBegin",_1.hitch(this,function(arg){
this._fire("onBegin",arguments);
}),true),_17=_5.after(this._current,"onPlay",_1.hitch(this,function(arg){
this._fire("onPlay",arguments);
_15.remove();
_16.remove();
_17.remove();
}));
if(this._onAnimateCtx){
this._onAnimateCtx.remove();
}
this._onAnimateCtx=_5.after(this._current,"onAnimate",_1.hitch(this,"_onAnimate"),true);
if(this._onEndCtx){
this._onEndCtx.remove();
}
this._onEndCtx=_5.after(this._current,"onEnd",_1.hitch(this,"_onEnd"),true);
this._current.play.apply(this._current,arguments);
return this;
},pause:function(){
if(this._current){
var e=_5.after(this._current,"onPause",_1.hitch(this,function(arg){
this._fire("onPause",arguments);
e.remove();
}),true);
this._current.pause();
}
return this;
},gotoPercent:function(_18,_19){
this.pause();
var _1a=this.duration*_18;
this._current=null;
_4.some(this._animations,function(a,_1b){
if(_1a<=a.duration){
this._current=a;
this._index=_1b;
return true;
}
_1a-=a.duration;
return false;
},this);
if(this._current){
this._current.gotoPercent(_1a/this._current.duration);
}
if(_19){
this.play();
}
return this;
},stop:function(_1c){
if(this._current){
if(_1c){
for(;this._index+1<this._animations.length;++this._index){
this._animations[this._index].stop(true);
}
this._current=this._animations[this._index];
}
var e=_5.after(this._current,"onStop",_1.hitch(this,function(arg){
this._fire("onStop",arguments);
e.remove();
}),true);
this._current.stop();
}
return this;
},status:function(){
return this._current?this._current.status():"stopped";
},destroy:function(){
this.stop();
if(this._onAnimateCtx){
this._onAnimateCtx.remove();
}
if(this._onEndCtx){
this._onEndCtx.remove();
}
}});
_1.extend(_11,_e);
_d.chain=function(_1d){
return new _11(_1.isArray(_1d)?_1d:Array.prototype.slice.call(_1d,0));
};
var _1e=function(_1f){
this._animations=_1f||[];
this._connects=[];
this._finished=0;
this.duration=0;
_4.forEach(_1f,function(a){
var _20=a.duration;
if(a.delay){
_20+=a.delay;
}
if(this.duration<_20){
this.duration=_20;
}
this._connects.push(_5.after(a,"onEnd",_1.hitch(this,"_onEnd"),true));
},this);
this._pseudoAnimation=new _6.Animation({curve:[0,1],duration:this.duration});
var _21=this;
_4.forEach(["beforeBegin","onBegin","onPlay","onAnimate","onPause","onStop","onEnd"],function(evt){
_21._connects.push(_5.after(_21._pseudoAnimation,evt,function(){
_21._fire(evt,arguments);
},true));
});
};
_1.extend(_1e,{_doAction:function(_22,_23){
_4.forEach(this._animations,function(a){
a[_22].apply(a,_23);
});
return this;
},_onEnd:function(){
if(++this._finished>this._animations.length){
this._fire("onEnd");
}
},_call:function(_24,_25){
var t=this._pseudoAnimation;
t[_24].apply(t,_25);
},play:function(_26,_27){
this._finished=0;
this._doAction("play",arguments);
this._call("play",arguments);
return this;
},pause:function(){
this._doAction("pause",arguments);
this._call("pause",arguments);
return this;
},gotoPercent:function(_28,_29){
var ms=this.duration*_28;
_4.forEach(this._animations,function(a){
a.gotoPercent(a.duration<ms?1:(ms/a.duration),_29);
});
this._call("gotoPercent",arguments);
return this;
},stop:function(_2a){
this._doAction("stop",arguments);
this._call("stop",arguments);
return this;
},status:function(){
return this._pseudoAnimation.status();
},destroy:function(){
this.stop();
_4.forEach(this._connects,function(_2b){
_2b.remove();
});
}});
_1.extend(_1e,_e);
_d.combine=function(_2c){
return new _1e(_1.isArray(_2c)?_2c:Array.prototype.slice.call(_2c,0));
};
_d.wipeIn=function(_2d){
var _2e=_2d.node=_7.byId(_2d.node),s=_2e.style,o;
var _2f=_6.animateProperty(_1.mixin({properties:{height:{start:function(){
o=s.overflow;
s.overflow="hidden";
if(s.visibility=="hidden"||s.display=="none"){
s.height="1px";
s.display="";
s.visibility="";
return 1;
}else{
var _30=_8.get(_2e,"height");
return Math.max(_30,1);
}
},end:function(){
return _2e.scrollHeight;
}}}},_2d));
var _31=function(){
s.height="auto";
s.overflow=o;
};
_5.after(_2f,"onStop",_31,true);
_5.after(_2f,"onEnd",_31,true);
return _2f;
};
_d.wipeOut=function(_32){
var _33=_32.node=_7.byId(_32.node),s=_33.style,o;
var _34=_6.animateProperty(_1.mixin({properties:{height:{end:1}}},_32));
_5.after(_34,"beforeBegin",function(){
o=s.overflow;
s.overflow="hidden";
s.display="";
},true);
var _35=function(){
s.overflow=o;
s.height="auto";
s.display="none";
};
_5.after(_34,"onStop",_35,true);
_5.after(_34,"onEnd",_35,true);
return _34;
};
_d.slideTo=function(_36){
var _37=_36.node=_7.byId(_36.node),top=null,_38=null;
var _39=(function(n){
return function(){
var cs=_8.getComputedStyle(n);
var pos=cs.position;
top=(pos=="absolute"?n.offsetTop:parseInt(cs.top)||0);
_38=(pos=="absolute"?n.offsetLeft:parseInt(cs.left)||0);
if(pos!="absolute"&&pos!="relative"){
var ret=_9.position(n,true);
top=ret.y;
_38=ret.x;
n.style.position="absolute";
n.style.top=top+"px";
n.style.left=_38+"px";
}
};
})(_37);
_39();
var _3a=_6.animateProperty(_1.mixin({properties:{top:_36.top||0,left:_36.left||0}},_36));
_5.after(_3a,"beforeBegin",_39,true);
return _3a;
};
return _d;
});
