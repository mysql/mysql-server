/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/fx",["./kernel","./lang","../Evented","./Color","./connect","./sniff","../dom","../dom-style"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_2.mixin;
_1._Line=function(_a,_b){
this.start=_a;
this.end=_b;
};
_1._Line.prototype.getValue=function(n){
return ((this.end-this.start)*n)+this.start;
};
_1.Animation=function(_c){
_9(this,_c);
if(_2.isArray(this.curve)){
this.curve=new _1._Line(this.curve[0],this.curve[1]);
}
};
_1.Animation.prototype=new _3();
_1._Animation=_1.Animation;
_2.extend(_1.Animation,{duration:350,repeat:0,rate:20,_percent:0,_startRepeatCount:0,_getStep:function(){
var _d=this._percent,_e=this.easing;
return _e?_e(_d):_d;
},_fire:function(_f,_10){
var a=_10||[];
if(this[_f]){
if(_1.config.debugAtAllCosts){
this[_f].apply(this,a);
}else{
try{
this[_f].apply(this,a);
}
catch(e){
console.error("exception in animation handler for:",_f);
console.error(e);
}
}
}
return this;
},play:function(_11,_12){
var _13=this;
if(_13._delayTimer){
_13._clearTimer();
}
if(_12){
_13._stopTimer();
_13._active=_13._paused=false;
_13._percent=0;
}else{
if(_13._active&&!_13._paused){
return _13;
}
}
_13._fire("beforeBegin",[_13.node]);
var de=_11||_13.delay,_14=_2.hitch(_13,"_play",_12);
if(de>0){
_13._delayTimer=setTimeout(_14,de);
return _13;
}
_14();
return _13;
},_play:function(_15){
var _16=this;
if(_16._delayTimer){
_16._clearTimer();
}
_16._startTime=new Date().valueOf();
if(_16._paused){
_16._startTime-=_16.duration*_16._percent;
}
_16._active=true;
_16._paused=false;
var _17=_16.curve.getValue(_16._getStep());
if(!_16._percent){
if(!_16._startRepeatCount){
_16._startRepeatCount=_16.repeat;
}
_16._fire("onBegin",[_17]);
}
_16._fire("onPlay",[_17]);
_16._cycle();
return _16;
},pause:function(){
var _18=this;
if(_18._delayTimer){
_18._clearTimer();
}
_18._stopTimer();
if(!_18._active){
return _18;
}
_18._paused=true;
_18._fire("onPause",[_18.curve.getValue(_18._getStep())]);
return _18;
},gotoPercent:function(_19,_1a){
var _1b=this;
_1b._stopTimer();
_1b._active=_1b._paused=true;
_1b._percent=_19;
if(_1a){
_1b.play();
}
return _1b;
},stop:function(_1c){
var _1d=this;
if(_1d._delayTimer){
_1d._clearTimer();
}
if(!_1d._timer){
return _1d;
}
_1d._stopTimer();
if(_1c){
_1d._percent=1;
}
_1d._fire("onStop",[_1d.curve.getValue(_1d._getStep())]);
_1d._active=_1d._paused=false;
return _1d;
},status:function(){
if(this._active){
return this._paused?"paused":"playing";
}
return "stopped";
},_cycle:function(){
var _1e=this;
if(_1e._active){
var _1f=new Date().valueOf();
var _20=(_1f-_1e._startTime)/(_1e.duration);
if(_20>=1){
_20=1;
}
_1e._percent=_20;
if(_1e.easing){
_20=_1e.easing(_20);
}
_1e._fire("onAnimate",[_1e.curve.getValue(_20)]);
if(_1e._percent<1){
_1e._startTimer();
}else{
_1e._active=false;
if(_1e.repeat>0){
_1e.repeat--;
_1e.play(null,true);
}else{
if(_1e.repeat==-1){
_1e.play(null,true);
}else{
if(_1e._startRepeatCount){
_1e.repeat=_1e._startRepeatCount;
_1e._startRepeatCount=0;
}
}
}
_1e._percent=0;
_1e._fire("onEnd",[_1e.node]);
!_1e.repeat&&_1e._stopTimer();
}
}
return _1e;
},_clearTimer:function(){
clearTimeout(this._delayTimer);
delete this._delayTimer;
}});
var ctr=0,_21=null,_22={run:function(){
}};
_2.extend(_1.Animation,{_startTimer:function(){
if(!this._timer){
this._timer=_5.connect(_22,"run",this,"_cycle");
ctr++;
}
if(!_21){
_21=setInterval(_2.hitch(_22,"run"),this.rate);
}
},_stopTimer:function(){
if(this._timer){
_5.disconnect(this._timer);
this._timer=null;
ctr--;
}
if(ctr<=0){
clearInterval(_21);
_21=null;
ctr=0;
}
}});
var _23=_6("ie")?function(_24){
var ns=_24.style;
if(!ns.width.length&&_8.get(_24,"width")=="auto"){
ns.width="auto";
}
}:function(){
};
_1._fade=function(_25){
_25.node=_7.byId(_25.node);
var _26=_9({properties:{}},_25),_27=(_26.properties.opacity={});
_27.start=!("start" in _26)?function(){
return +_8.get(_26.node,"opacity")||0;
}:_26.start;
_27.end=_26.end;
var _28=_1.animateProperty(_26);
_5.connect(_28,"beforeBegin",_2.partial(_23,_26.node));
return _28;
};
_1.fadeIn=function(_29){
return _1._fade(_9({end:1},_29));
};
_1.fadeOut=function(_2a){
return _1._fade(_9({end:0},_2a));
};
_1._defaultEasing=function(n){
return 0.5+((Math.sin((n+1.5)*Math.PI))/2);
};
var _2b=function(_2c){
this._properties=_2c;
for(var p in _2c){
var _2d=_2c[p];
if(_2d.start instanceof _4){
_2d.tempColor=new _4();
}
}
};
_2b.prototype.getValue=function(r){
var ret={};
for(var p in this._properties){
var _2e=this._properties[p],_2f=_2e.start;
if(_2f instanceof _4){
ret[p]=_4.blendColors(_2f,_2e.end,r,_2e.tempColor).toCss();
}else{
if(!_2.isArray(_2f)){
ret[p]=((_2e.end-_2f)*r)+_2f+(p!="opacity"?_2e.units||"px":0);
}
}
}
return ret;
};
_1.animateProperty=function(_30){
var n=_30.node=_7.byId(_30.node);
if(!_30.easing){
_30.easing=_1._defaultEasing;
}
var _31=new _1.Animation(_30);
_5.connect(_31,"beforeBegin",_31,function(){
var pm={};
for(var p in this.properties){
if(p=="width"||p=="height"){
this.node.display="block";
}
var _32=this.properties[p];
if(_2.isFunction(_32)){
_32=_32(n);
}
_32=pm[p]=_9({},(_2.isObject(_32)?_32:{end:_32}));
if(_2.isFunction(_32.start)){
_32.start=_32.start(n);
}
if(_2.isFunction(_32.end)){
_32.end=_32.end(n);
}
var _33=(p.toLowerCase().indexOf("color")>=0);
function _34(_35,p){
var v={height:_35.offsetHeight,width:_35.offsetWidth}[p];
if(v!==undefined){
return v;
}
v=_8.get(_35,p);
return (p=="opacity")?+v:(_33?v:parseFloat(v));
};
if(!("end" in _32)){
_32.end=_34(n,p);
}else{
if(!("start" in _32)){
_32.start=_34(n,p);
}
}
if(_33){
_32.start=new _4(_32.start);
_32.end=new _4(_32.end);
}else{
_32.start=(p=="opacity")?+_32.start:parseFloat(_32.start);
}
}
this.curve=new _2b(pm);
});
_5.connect(_31,"onAnimate",_2.hitch(_8,"set",_31.node));
return _31;
};
_1.anim=function(_36,_37,_38,_39,_3a,_3b){
return _1.animateProperty({node:_36,duration:_38||_1.Animation.prototype.duration,properties:_37,easing:_39,onEnd:_3a}).play(_3b||0);
};
return {_Line:_1._Line,Animation:_1.Animation,_fade:_1._fade,fadeIn:_1.fadeIn,fadeOut:_1.fadeOut,_defaultEasing:_1._defaultEasing,animateProperty:_1.animateProperty,anim:_1.anim};
});
