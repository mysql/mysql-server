//>>built
define("dijit/typematic",["dojo/_base/array","dojo/_base/connect","dojo/_base/event","dojo/_base/kernel","dojo/_base/lang","dojo/on","dojo/_base/sniff","."],function(_1,_2,_3,_4,_5,on,_6,_7){
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
this._evt=_9;
this._node=_b;
this._currentTimeout=-1;
this._count=-1;
this._callback=_5.hitch(_a,_c);
this._fireEventAndReload();
this._evt=_5.mixin({faux:true},_9);
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
},addKeyListener:function(_11,_12,_13,_14,_15,_16,_17){
if(_12.keyCode){
_12.charOrCode=_12.keyCode;
_4.deprecated("keyCode attribute parameter for dijit.typematic.addKeyListener is deprecated. Use charOrCode instead.","","2.0");
}else{
if(_12.charCode){
_12.charOrCode=String.fromCharCode(_12.charCode);
_4.deprecated("charCode attribute parameter for dijit.typematic.addKeyListener is deprecated. Use charOrCode instead.","","2.0");
}
}
var _18=[on(_11,_2._keypress,_5.hitch(this,function(evt){
if(evt.charOrCode==_12.charOrCode&&(_12.ctrlKey===undefined||_12.ctrlKey==evt.ctrlKey)&&(_12.altKey===undefined||_12.altKey==evt.altKey)&&(_12.metaKey===undefined||_12.metaKey==(evt.metaKey||false))&&(_12.shiftKey===undefined||_12.shiftKey==evt.shiftKey)){
_3.stop(evt);
_8.trigger(evt,_13,_11,_14,_12,_15,_16,_17);
}else{
if(_8._obj==_12){
_8.stop();
}
}
})),on(_11,"keyup",_5.hitch(this,function(){
if(_8._obj==_12){
_8.stop();
}
}))];
return {remove:function(){
_1.forEach(_18,function(h){
h.remove();
});
}};
},addMouseListener:function(_19,_1a,_1b,_1c,_1d,_1e){
var _1f=[on(_19,"mousedown",_5.hitch(this,function(evt){
_3.stop(evt);
_8.trigger(evt,_1a,_19,_1b,_19,_1c,_1d,_1e);
})),on(_19,"mouseup",_5.hitch(this,function(evt){
if(this._obj){
_3.stop(evt);
}
_8.stop();
})),on(_19,"mouseout",_5.hitch(this,function(evt){
_3.stop(evt);
_8.stop();
})),on(_19,"mousemove",_5.hitch(this,function(evt){
evt.preventDefault();
})),on(_19,"dblclick",_5.hitch(this,function(evt){
_3.stop(evt);
if(_6("ie")){
_8.trigger(evt,_1a,_19,_1b,_19,_1c,_1d,_1e);
setTimeout(_5.hitch(this,_8.stop),50);
}
}))];
return {remove:function(){
_1.forEach(_1f,function(h){
h.remove();
});
}};
},addListener:function(_20,_21,_22,_23,_24,_25,_26,_27){
var _28=[this.addKeyListener(_21,_22,_23,_24,_25,_26,_27),this.addMouseListener(_20,_23,_24,_25,_26,_27)];
return {remove:function(){
_1.forEach(_28,function(h){
h.remove();
});
}};
}});
return _8;
});
