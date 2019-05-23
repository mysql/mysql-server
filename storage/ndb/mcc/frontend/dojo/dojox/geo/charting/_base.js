//>>built
define("dojox/geo/charting/_base",["dojo/_base/lang","dojo/_base/array","../../main","dojo/_base/html","dojo/dom-geometry","dojox/gfx/matrix","dijit/Tooltip","dojo/_base/NodeList","dojo/NodeList-traverse"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_1.getObject("geo.charting",true,_3);
_a.showTooltip=function(_b,_c,_d){
var _e=_a._normalizeArround(_c);
return _7.show(_b,_e,_d);
};
_a.hideTooltip=function(_f){
return _7.hide(_f);
};
_a._normalizeArround=function(_10){
var _11=_a._getRealBBox(_10);
var _12=_10._getRealMatrix()||{xx:1,xy:0,yx:0,yy:1,dx:0,dy:0};
var _13=_6.multiplyPoint(_12,_11.x,_11.y);
var _14=_a._getGfxContainer(_10);
_10.x=_5.position(_14,true).x+_13.x,_10.y=_5.position(_14,true).y+_13.y,_10.w=_11.width*_12.xx,_10.h=_11.height*_12.yy;
return _10;
};
_a._getGfxContainer=function(_15){
if(_15.surface){
return (new _8(_15.surface.rawNode)).parents("div")[0];
}else{
return (new _8(_15.rawNode)).parents("div")[0];
}
};
_a._getRealBBox=function(_16){
var _17=_16.getBoundingBox();
if(!_17){
var _18=_16.children;
_17=_1.clone(_a._getRealBBox(_18[0]));
_2.forEach(_18,function(_19){
var _1a=_a._getRealBBox(_19);
_17.x=Math.min(_17.x,_1a.x);
_17.y=Math.min(_17.y,_1a.y);
_17.endX=Math.max(_17.x+_17.width,_1a.x+_1a.width);
_17.endY=Math.max(_17.y+_17.height,_1a.y+_1a.height);
});
_17.width=_17.endX-_17.x;
_17.height=_17.endY-_17.y;
}
return _17;
};
return _a;
});
