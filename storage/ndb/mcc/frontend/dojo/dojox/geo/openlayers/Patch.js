//>>built
define("dojox/geo/openlayers/Patch",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/sniff","dojox/gfx","dojox/gfx/shape"],function(_1,_2,_3,_4,_5){
var _6=_2.getObject("geo.openlayers",true,dojox);
_6.Patch={patchMethod:function(_7,_8,_9,_a){
var _b=_7.prototype[_8];
_7.prototype[_8]=function(){
var _c=_8;
if(_9){
_9.call(this,_c,arguments);
}
var _d=_b.apply(this,arguments);
if(_a){
_d=_a.call(this,_c,_d,arguments)||_d;
}
return _d;
};
},patchGFX:function(){
var _e=function(){
if(!this.rawNode.path){
this.rawNode.path={};
}
};
var _f=function(){
if(this.rawNode.fill&&!this.rawNode.fill.colors){
this.rawNode.fill.colors={};
}
};
if(_3.isIE<=8){
dojox.geo.openlayers.Patch.patchMethod(_4.Line,"setShape",_e,null);
dojox.geo.openlayers.Patch.patchMethod(_4.Polyline,"setShape",_e,null);
dojox.geo.openlayers.Patch.patchMethod(_4.Path,"setShape",_e,null);
dojox.geo.openlayers.Patch.patchMethod(_5.Shape,"setFill",_f,null);
}
}};
return _6.Patch;
});
