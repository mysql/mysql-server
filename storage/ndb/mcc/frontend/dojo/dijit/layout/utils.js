//>>built
define("dijit/layout/utils",["dojo/_base/array","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/lang","../main"],function(_1,_2,_3,_4,_5,_6){
var _7=_5.getObject("layout",true,_6);
_7.marginBox2contentBox=function(_8,mb){
var cs=_4.getComputedStyle(_8);
var me=_3.getMarginExtents(_8,cs);
var pb=_3.getPadBorderExtents(_8,cs);
return {l:_4.toPixelValue(_8,cs.paddingLeft),t:_4.toPixelValue(_8,cs.paddingTop),w:mb.w-(me.w+pb.w),h:mb.h-(me.h+pb.h)};
};
function _9(_a){
return _a.substring(0,1).toUpperCase()+_a.substring(1);
};
function _b(_c,_d){
var _e=_c.resize?_c.resize(_d):_3.setMarginBox(_c.domNode,_d);
if(_e){
_5.mixin(_c,_e);
}else{
_5.mixin(_c,_3.getMarginBox(_c.domNode));
_5.mixin(_c,_d);
}
};
_7.layoutChildren=function(_f,dim,_10,_11,_12){
dim=_5.mixin({},dim);
_2.add(_f,"dijitLayoutContainer");
_10=_1.filter(_10,function(_13){
return _13.region!="center"&&_13.layoutAlign!="client";
}).concat(_1.filter(_10,function(_14){
return _14.region=="center"||_14.layoutAlign=="client";
}));
_1.forEach(_10,function(_15){
var elm=_15.domNode,pos=(_15.region||_15.layoutAlign);
if(!pos){
throw new Error("No region setting for "+_15.id);
}
var _16=elm.style;
_16.left=dim.l+"px";
_16.top=dim.t+"px";
_16.position="absolute";
_2.add(elm,"dijitAlign"+_9(pos));
var _17={};
if(_11&&_11==_15.id){
_17[_15.region=="top"||_15.region=="bottom"?"h":"w"]=_12;
}
if(pos=="top"||pos=="bottom"){
_17.w=dim.w;
_b(_15,_17);
dim.h-=_15.h;
if(pos=="top"){
dim.t+=_15.h;
}else{
_16.top=dim.t+dim.h+"px";
}
}else{
if(pos=="left"||pos=="right"){
_17.h=dim.h;
_b(_15,_17);
dim.w-=_15.w;
if(pos=="left"){
dim.l+=_15.w;
}else{
_16.left=dim.l+dim.w+"px";
}
}else{
if(pos=="client"||pos=="center"){
_b(_15,dim);
}
}
}
});
};
return {marginBox2contentBox:_7.marginBox2contentBox,layoutChildren:_7.layoutChildren};
});
