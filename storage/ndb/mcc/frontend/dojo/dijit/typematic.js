//>>built
define("dijit/typematic",["dojo/_base/array","dojo/_base/connect","dojo/_base/event","dojo/_base/kernel","dojo/_base/lang","dojo/on","dojo/sniff","./main"],function(_1,_2,_3,_4,_5,on,_6,_7){
var _8=(_7.typematic={_fireEventAndReload:function(){
this._timer=null;
this._callback(++this._count,this._node,this._evt);
this._currentTimeout=Math.max(this._currentTimeout<0?this._initialDelay:(this._subsequentDelay>1?this._subsequentDelay:Math.round(this._currentTimeout*this._subsequentDelay)),this._minDelay);
this._timer=setTimeout(_5.hitch(this,"_fireEventAndReload"),this._currentTimeout);
},trigger:function(_9,_a,_b,_c,_d,_e,_f,_10){
if(_d!=this._obj){
this.stop();
this._initialDelay=_f||500;
this._subsequentDelay=_e||0.9;
this._minDelay=_10||10;
this._obj=_d;
this._node=_b;
this._currentTimeout=-1;
this._count=-1;
this._callback=_5.hitch(_a,_c);
this._evt={faux:true};
for(var _11 in _9){
if(_11!="layerX"&&_11!="layerY"){
var v=_9[_11];
if(typeof v!="function"&&typeof v!="undefined"){
this._evt[_11]=v;
}
}
}
this._fireEventAndReload();
}
},stop:function(){
if(this._timer){
clearTimeout(this._timer);
this._timer=null;
}
if(this._obj){
this._callback(-1,this._node,this._evt);
this._obj=null;
}
},addKeyListener:function(_12,_13,_14,_15,_16,_17,_18){
if(_13.keyCode){
_13.charOrCode=_13.keyCode;
_4.deprecated("keyCode attribute parameter for dijit.typematic.addKeyListener is deprecated. Use charOrCode instead.","","2.0");
}else{
if(_13.charCode){
_13.charOrCode=String.fromCharCode(_13.charCode);
_4.deprecated("charCode attribute parameter for dijit.typematic.addKeyListener is deprecated. Use charOrCode instead.","","2.0");
}
}
var _19=[on(_12,_2._keypress,_5.hitch(this,function(evt){
if(evt.charOrCode==_13.charOrCode&&(_13.ctrlKey===undefined||_13.ctrlKey==evt.ctrlKey)&&(_13.altKey===undefined||_13.altKey==evt.altKey)&&(_13.metaKey===undefined||_13.metaKey==(evt.metaKey||false))&&(_13.shiftKey===undefined||_13.shiftKey==evt.shiftKey)){
_3.stop(evt);
_8.trigger(evt,_14,_12,_15,_13,_16,_17,_18);
}else{
if(_8._obj==_13){
_8.stop();
}
}
})),on(_12,"keyup",_5.hitch(this,function(){
if(_8._obj==_13){
_8.stop();
}
}))];
return {remove:function(){
_1.forEach(_19,function(h){
h.remove();
});
}};
},addMouseListener:function(_1a,_1b,_1c,_1d,_1e,_1f){
var _20=[on(_1a,"mousedown",_5.hitch(this,function(evt){
evt.preventDefault();
_8.trigger(evt,_1b,_1a,_1c,_1a,_1d,_1e,_1f);
})),on(_1a,"mouseup",_5.hitch(this,function(evt){
if(this._obj){
evt.preventDefault();
}
_8.stop();
})),on(_1a,"mouseout",_5.hitch(this,function(evt){
if(this._obj){
evt.preventDefault();
}
_8.stop();
})),on(_1a,"dblclick",_5.hitch(this,function(evt){
evt.preventDefault();
if(_6("ie")<9){
_8.trigger(evt,_1b,_1a,_1c,_1a,_1d,_1e,_1f);
setTimeout(_5.hitch(this,_8.stop),50);
}
}))];
return {remove:function(){
_1.forEach(_20,function(h){
h.remove();
});
}};
},addListener:function(_21,_22,_23,_24,_25,_26,_27,_28){
var _29=[this.addKeyListener(_22,_23,_24,_25,_26,_27,_28),this.addMouseListener(_21,_24,_25,_26,_27,_28)];
return {remove:function(){
_1.forEach(_29,function(h){
h.remove();
});
}};
}});
return _8;
});
