/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/NodeList-fx",["dojo/_base/NodeList","./_base/lang","./_base/connect","./_base/fx","./fx"],function(_1,_2,_3,_4,_5){
_2.extend(_1,{_anim:function(_6,_7,_8){
_8=_8||{};
var a=_5.combine(this.map(function(_9){
var _a={node:_9};
_2.mixin(_a,_8);
return _6[_7](_a);
}));
return _8.auto?a.play()&&this:a;
},wipeIn:function(_b){
return this._anim(_5,"wipeIn",_b);
},wipeOut:function(_c){
return this._anim(_5,"wipeOut",_c);
},slideTo:function(_d){
return this._anim(_5,"slideTo",_d);
},fadeIn:function(_e){
return this._anim(_4,"fadeIn",_e);
},fadeOut:function(_f){
return this._anim(_4,"fadeOut",_f);
},animateProperty:function(_10){
return this._anim(_4,"animateProperty",_10);
},anim:function(_11,_12,_13,_14,_15){
var _16=_5.combine(this.map(function(_17){
return _4.animateProperty({node:_17,properties:_11,duration:_12||350,easing:_13});
}));
if(_14){
_3.connect(_16,"onEnd",_14);
}
return _16.play(_15||0);
}});
return _1;
});
