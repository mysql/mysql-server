//>>built
define("dojox/widget/rotator/Wipe",["dojo/_base/lang","dojo/_base/fx","dojo/dom-style"],function(_1,fx,_2){
var _3=2,_4=3,UP=0,_5=1;
function _6(_7,w,h,x){
var a=[0,w,0,0];
if(_7==_4){
a=[0,w,h,w];
}else{
if(_7==UP){
a=[h,w,h,0];
}else{
if(_7==_5){
a=[0,0,h,0];
}
}
}
if(x!=null){
a[_7]=_7==_3||_7==_5?x:(_7%2?w:h)-x;
}
return a;
};
function _8(n,_9,w,h,x){
_2.set(n,"clip",_9==null?"auto":"rect("+_6(_9,w,h,x).join("px,")+"px)");
};
function _a(_b,_c){
var _d=_c.next.node,w=_c.rotatorBox.w,h=_c.rotatorBox.h;
_2.set(_d,{display:"",zIndex:(_2.get(_c.current.node,"zIndex")||1)+1});
_8(_d,_b,w,h);
return new fx.Animation(_1.mixin({node:_d,curve:[0,_b%2?w:h],onAnimate:function(x){
_8(_d,_b,w,h,parseInt(x));
}},_c));
};
var _e={wipeDown:function(_f){
return _a(_3,_f);
},wipeRight:function(_10){
return _a(_4,_10);
},wipeUp:function(_11){
return _a(UP,_11);
},wipeLeft:function(_12){
return _a(_5,_12);
}};
_1.mixin(_1.getObject("dojox.widget.rotator"),_e);
return _e;
});
