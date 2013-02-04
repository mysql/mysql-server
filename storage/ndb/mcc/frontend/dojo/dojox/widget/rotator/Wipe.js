//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.widget.rotator.Wipe");
(function(d){
var _4=2,_5=3,UP=0,_6=1;
function _7(_8,w,h,x){
var a=[0,w,0,0];
if(_8==_5){
a=[0,w,h,w];
}else{
if(_8==UP){
a=[h,w,h,0];
}else{
if(_8==_6){
a=[0,0,h,0];
}
}
}
if(x!=null){
a[_8]=_8==_4||_8==_6?x:(_8%2?w:h)-x;
}
return a;
};
function _9(n,_a,w,h,x){
d.style(n,"clip",_a==null?"auto":"rect("+_7(_a,w,h,x).join("px,")+"px)");
};
function _b(_c,_d){
var _e=_d.next.node,w=_d.rotatorBox.w,h=_d.rotatorBox.h;
d.style(_e,{display:"",zIndex:(d.style(_d.current.node,"zIndex")||1)+1});
_9(_e,_c,w,h);
return new d.Animation(d.mixin({node:_e,curve:[0,_c%2?w:h],onAnimate:function(x){
_9(_e,_c,w,h,parseInt(x));
}},_d));
};
d.mixin(_3.widget.rotator,{wipeDown:function(_f){
return _b(_4,_f);
},wipeRight:function(_10){
return _b(_5,_10);
},wipeUp:function(_11){
return _b(UP,_11);
},wipeLeft:function(_12){
return _b(_6,_12);
}});
})(_2);
});
