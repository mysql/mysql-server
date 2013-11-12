/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/fx",["./_base/lang","./Evented","./_base/kernel","./_base/array","./_base/connect","./_base/fx","./dom","./dom-style","./dom-geometry","./ready","require"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
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
this.duration+=a.duration;
if(a.delay){
this.duration+=a.delay;
}
},this);
};
_11.prototype=new _2();
_1.extend(_11,{_onAnimate:function(){
this._fire("onAnimate",arguments);
},_onEnd:function(){
_5.disconnect(this._onAnimateCtx);
_5.disconnect(this._onEndCtx);
this._onAnimateCtx=this._onEndCtx=null;
if(this._index+1==this._animations.length){
this._fire("onEnd");
}else{
this._current=this._animations[++this._index];
this._onAnimateCtx=_5.connect(this._current,"onAnimate",this,"_onAnimate");
this._onEndCtx=_5.connect(this._current,"onEnd",this,"_onEnd");
this._current.play(0,true);
}
},play:function(_13,_14){
if(!this._current){
this._current=this._animations[this._index=0];
}
if(!_14&&this._current.status()=="playing"){
return this;
}
var _15=_5.connect(this._current,"beforeBegin",this,function(){
this._fire("beforeBegin");
}),_16=_5.connect(this._current,"onBegin",this,function(arg){
this._fire("onBegin",arguments);
}),_17=_5.connect(this._current,"onPlay",this,function(arg){
this._fire("onPlay",arguments);
_5.disconnect(_15);
_5.disconnect(_16);
_5.disconnect(_17);
});
if(this._onAnimateCtx){
_5.disconnect(this._onAnimateCtx);
}
this._onAnimateCtx=_5.connect(this._current,"onAnimate",this,"_onAnimate");
if(this._onEndCtx){
_5.disconnect(this._onEndCtx);
}
this._onEndCtx=_5.connect(this._current,"onEnd",this,"_onEnd");
this._current.play.apply(this._current,arguments);
return this;
},pause:function(){
if(this._current){
var e=_5.connect(this._current,"onPause",this,function(arg){
this._fire("onPause",arguments);
_5.disconnect(e);
});
this._current.pause();
}
return this;
},gotoPercent:function(_18,_19){
this.pause();
var _1a=this.duration*_18;
this._current=null;
_4.some(this._animations,function(a){
if(a.duration<=_1a){
this._current=a;
return true;
}
_1a-=a.duration;
return false;
});
if(this._current){
this._current.gotoPercent(_1a/this._current.duration,_19);
}
return this;
},stop:function(_1b){
if(this._current){
if(_1b){
for(;this._index+1<this._animations.length;++this._index){
this._animations[this._index].stop(true);
}
this._current=this._animations[this._index];
}
var e=_5.connect(this._current,"onStop",this,function(arg){
this._fire("onStop",arguments);
_5.disconnect(e);
});
this._current.stop();
}
return this;
},status:function(){
return this._current?this._current.status():"stopped";
},destroy:function(){
if(this._onAnimateCtx){
_5.disconnect(this._onAnimateCtx);
}
if(this._onEndCtx){
_5.disconnect(this._onEndCtx);
}
}});
_1.extend(_11,_e);
_d.chain=function(_1c){
return new _11(_1c);
};
var _1d=function(_1e){
this._animations=_1e||[];
this._connects=[];
this._finished=0;
this.duration=0;
_4.forEach(_1e,function(a){
var _1f=a.duration;
if(a.delay){
_1f+=a.delay;
}
if(this.duration<_1f){
this.duration=_1f;
}
this._connects.push(_5.connect(a,"onEnd",this,"_onEnd"));
},this);
this._pseudoAnimation=new _6.Animation({curve:[0,1],duration:this.duration});
var _20=this;
_4.forEach(["beforeBegin","onBegin","onPlay","onAnimate","onPause","onStop","onEnd"],function(evt){
_20._connects.push(_5.connect(_20._pseudoAnimation,evt,function(){
_20._fire(evt,arguments);
}));
});
};
_1.extend(_1d,{_doAction:function(_21,_22){
_4.forEach(this._animations,function(a){
a[_21].apply(a,_22);
});
return this;
},_onEnd:function(){
if(++this._finished>this._animations.length){
this._fire("onEnd");
}
},_call:function(_23,_24){
var t=this._pseudoAnimation;
t[_23].apply(t,_24);
},play:function(_25,_26){
this._finished=0;
this._doAction("play",arguments);
this._call("play",arguments);
return this;
},pause:function(){
this._doAction("pause",arguments);
this._call("pause",arguments);
return this;
},gotoPercent:function(_27,_28){
var ms=this.duration*_27;
_4.forEach(this._animations,function(a){
a.gotoPercent(a.duration<ms?1:(ms/a.duration),_28);
});
this._call("gotoPercent",arguments);
return this;
},stop:function(_29){
this._doAction("stop",arguments);
this._call("stop",arguments);
return this;
},status:function(){
return this._pseudoAnimation.status();
},destroy:function(){
_4.forEach(this._connects,_5.disconnect);
}});
_1.extend(_1d,_e);
_d.combine=function(_2a){
return new _1d(_2a);
};
_d.wipeIn=function(_2b){
var _2c=_2b.node=_7.byId(_2b.node),s=_2c.style,o;
var _2d=_6.animateProperty(_1.mixin({properties:{height:{start:function(){
o=s.overflow;
s.overflow="hidden";
if(s.visibility=="hidden"||s.display=="none"){
s.height="1px";
s.display="";
s.visibility="";
return 1;
}else{
var _2e=_8.get(_2c,"height");
return Math.max(_2e,1);
}
},end:function(){
return _2c.scrollHeight;
}}}},_2b));
var _2f=function(){
s.height="auto";
s.overflow=o;
};
_5.connect(_2d,"onStop",_2f);
_5.connect(_2d,"onEnd",_2f);
return _2d;
};
_d.wipeOut=function(_30){
var _31=_30.node=_7.byId(_30.node),s=_31.style,o;
var _32=_6.animateProperty(_1.mixin({properties:{height:{end:1}}},_30));
_5.connect(_32,"beforeBegin",function(){
o=s.overflow;
s.overflow="hidden";
s.display="";
});
var _33=function(){
s.overflow=o;
s.height="auto";
s.display="none";
};
_5.connect(_32,"onStop",_33);
_5.connect(_32,"onEnd",_33);
return _32;
};
_d.slideTo=function(_34){
var _35=_34.node=_7.byId(_34.node),top=null,_36=null;
var _37=(function(n){
return function(){
var cs=_8.getComputedStyle(n);
var pos=cs.position;
top=(pos=="absolute"?n.offsetTop:parseInt(cs.top)||0);
_36=(pos=="absolute"?n.offsetLeft:parseInt(cs.left)||0);
if(pos!="absolute"&&pos!="relative"){
var ret=_9.position(n,true);
top=ret.y;
_36=ret.x;
n.style.position="absolute";
n.style.top=top+"px";
n.style.left=_36+"px";
}
};
})(_35);
_37();
var _38=_6.animateProperty(_1.mixin({properties:{top:_34.top||0,left:_34.left||0}},_34));
_5.connect(_38,"beforeBegin",_38,_37);
return _38;
};
return _d;
});
