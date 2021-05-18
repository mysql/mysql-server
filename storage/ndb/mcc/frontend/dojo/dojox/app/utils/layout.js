//>>built
define("dojox/app/utils/layout",["dojo/_base/array","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/lang"],function(_1,_2,_3,_4,_5){
var _6={};
_6.marginBox2contentBox=function(_7,mb){
var cs=_4.getComputedStyle(_7);
var me=_3.getMarginExtents(_7,cs);
var pb=_3.getPadBorderExtents(_7,cs);
return {l:_4.toPixelValue(_7,cs.paddingLeft),t:_4.toPixelValue(_7,cs.paddingTop),w:mb.w-(me.w+pb.w),h:mb.h-(me.h+pb.h)};
};
function _8(_9){
return _9.substring(0,1).toUpperCase()+_9.substring(1);
};
function _a(_b,_c){
var _d=_b.resize?_b.resize(_c):_3.setMarginBox(_b.domNode,_c);
if(_d){
_5.mixin(_b,_d);
}else{
_5.mixin(_b,_3.getMarginBox(_b.domNode));
_5.mixin(_b,_c);
}
};
_6.layoutChildren=function(_e,_f,_10,_11,_12){
_f=_5.mixin({},_f);
_2.add(_e,"dijitLayoutContainer");
_10=_1.filter(_10,function(_13){
return _13._constraint!="center"&&_13.layoutAlign!="client";
}).concat(_1.filter(_10,function(_14){
return _14._constraint=="center"||_14.layoutAlign=="client";
}));
_1.forEach(_10,function(_15){
var elm=_15.domNode,pos=(_15._constraint||_15.layoutAlign);
if(!pos){
throw new Error("No constraint setting for "+_15.id);
}
var _16=elm.style;
_16.left=_f.l+"px";
_16.top=_f.t+"px";
_16.position="absolute";
_2.add(elm,"dijitAlign"+_8(pos));
var _17={};
if(_11&&_11==_15.id){
_17[_15._constraint=="top"||_15._constraint=="bottom"?"h":"w"]=_12;
}
if(pos=="top"||pos=="bottom"){
_17.w=_f.w;
_a(_15,_17);
_f.h-=_15.h;
if(pos=="top"){
_f.t+=_15.h;
}else{
_16.top=_f.t+_f.h+"px";
}
}else{
if(pos=="left"||pos=="right"){
_17.h=_f.h;
_a(_15,_17);
_f.w-=_15.w;
if(pos=="left"){
_f.l+=_15.w;
}else{
_16.left=_f.l+_f.w+"px";
}
}else{
if(pos=="client"||pos=="center"){
_a(_15,_f);
}
}
}
});
};
return {marginBox2contentBox:_6.marginBox2contentBox,layoutChildren:_6.layoutChildren};
});
