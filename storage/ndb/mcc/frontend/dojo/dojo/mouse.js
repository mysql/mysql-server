/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/mouse",["./_base/kernel","./on","./has","./dom","./_base/window"],function(_1,on,_2,_3,_4){
_2.add("dom-quirks",_4.doc&&_4.doc.compatMode=="BackCompat");
_2.add("events-mouseenter",_4.doc&&"onmouseenter" in _4.doc.createElement("div"));
var _5;
if(_2("dom-quirks")||!_2("dom-addeventlistener")){
_5={LEFT:1,MIDDLE:4,RIGHT:2,isButton:function(e,_6){
return e.button&_6;
},isLeft:function(e){
return e.button&1;
},isMiddle:function(e){
return e.button&4;
},isRight:function(e){
return e.button&2;
}};
}else{
_5={LEFT:0,MIDDLE:1,RIGHT:2,isButton:function(e,_7){
return e.button==_7;
},isLeft:function(e){
return e.button==0;
},isMiddle:function(e){
return e.button==1;
},isRight:function(e){
return e.button==2;
}};
}
_1.mouseButtons=_5;
function _8(_9,_a){
var _b=function(_c,_d){
return on(_c,_9,function(_e){
if(!_3.isDescendant(_e.relatedTarget,_a?_e.target:_c)){
return _d.call(this,_e);
}
});
};
if(!_a){
_b.bubble=_8(_9,true);
}
return _b;
};
return {enter:_8("mouseover"),leave:_8("mouseout"),isLeft:_5.isLeft,isMiddle:_5.isMiddle,isRight:_5.isRight};
});
