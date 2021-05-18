//>>built
define("dojox/gesture/Base",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/dom","dojo/on","dojo/touch","dojo/has","../main"],function(_1,_2,_3,_4,_5,on,_6,_7,_8){
_1.experimental("dojox.gesture.Base");
_4.getObject("gesture",true,_8);
return _2(null,{defaultEvent:" ",subEvents:[],touchOnly:false,_elements:null,constructor:function(_9){
_4.mixin(this,_9);
this.init();
},init:function(){
this._elements=[];
if(!_7("touch")&&this.touchOnly){
console.warn("Gestures:[",this.defaultEvent,"] is only supported on touch devices!");
return;
}
var _a=this.defaultEvent;
this.call=this._handle(_a);
this._events=[_a];
_3.forEach(this.subEvents,function(_b){
this[_b]=this._handle(_a+"."+_b);
this._events.push(_a+"."+_b);
},this);
},_handle:function(_c){
var _d=this;
return function(_e,_f){
var a=arguments;
if(a.length>2){
_e=a[1];
_f=a[2];
}
var _10=_e&&(_e.nodeType||_e.attachEvent||_e.addEventListener);
if(!_10){
return on(_e,_c,_f);
}else{
var _11=_d._add(_e,_c,_f);
var _12={remove:function(){
_11.remove();
_d._remove(_e,_c);
}};
return _12;
}
};
},_add:function(_13,_14,_15){
var _16=this._getGestureElement(_13);
if(!_16){
_16={target:_13,data:{},handles:{}};
var _17=_4.hitch(this,"_process",_16,"press");
var _18=_4.hitch(this,"_process",_16,"move");
var _19=_4.hitch(this,"_process",_16,"release");
var _1a=_4.hitch(this,"_process",_16,"cancel");
var _1b=_16.handles;
if(this.touchOnly){
_1b.press=on(_13,"touchstart",_17);
_1b.move=on(_13,"touchmove",_18);
_1b.release=on(_13,"touchend",_19);
_1b.cancel=on(_13,"touchcancel",_1a);
}else{
_1b.press=_6.press(_13,_17);
_1b.move=_6.move(_13,_18);
_1b.release=_6.release(_13,_19);
_1b.cancel=_6.cancel(_13,_1a);
}
this._elements.push(_16);
}
_16.handles[_14]=!_16.handles[_14]?1:++_16.handles[_14];
return on(_13,_14,_15);
},_getGestureElement:function(_1c){
var i=0,_1d;
for(;i<this._elements.length;i++){
_1d=this._elements[i];
if(_1d.target===_1c){
return _1d;
}
}
},_process:function(_1e,_1f,e){
e._locking=e._locking||{};
if(e._locking[this.defaultEvent]||this.isLocked(e.currentTarget)){
return;
}
if((e.target.tagName!="INPUT"||e.target.type=="radio"||e.target.type=="checkbox")&&e.target.tagName!="TEXTAREA"){
e.preventDefault();
}
e._locking[this.defaultEvent]=true;
this[_1f](_1e.data,e);
},press:function(_20,e){
},move:function(_21,e){
},release:function(_22,e){
},cancel:function(_23,e){
},fire:function(_24,_25){
if(!_24||!_25){
return;
}
_25.bubbles=true;
_25.cancelable=true;
on.emit(_24,_25.type,_25);
},_remove:function(_26,_27){
var _28=this._getGestureElement(_26);
if(!_28||!_28.handles){
return;
}
_28.handles[_27]--;
var _29=_28.handles;
if(!_3.some(this._events,function(evt){
return _29[evt]>0;
})){
this._cleanHandles(_29);
var i=_3.indexOf(this._elements,_28);
if(i>=0){
this._elements.splice(i,1);
}
}
},_cleanHandles:function(_2a){
for(var x in _2a){
if(_2a[x].remove){
_2a[x].remove();
}
delete _2a[x];
}
},lock:function(_2b){
this._lock=_2b;
},unLock:function(){
this._lock=null;
},isLocked:function(_2c){
if(!this._lock||!_2c){
return false;
}
return this._lock!==_2c&&_5.isDescendant(_2c,this._lock);
},destroy:function(){
_3.forEach(this._elements,function(_2d){
this._cleanHandles(_2d.handles);
},this);
this._elements=null;
}});
});
