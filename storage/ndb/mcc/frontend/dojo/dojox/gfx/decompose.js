//>>built
define("dojox/gfx/decompose",["./_base","dojo/_base/lang","./matrix"],function(g,_1,m){
function eq(a,b){
return Math.abs(a-b)<=0.000001*(Math.abs(a)+Math.abs(b));
};
function _2(r1,m1,r2,m2){
if(!isFinite(r1)){
return r2;
}else{
if(!isFinite(r2)){
return r1;
}
}
m1=Math.abs(m1);
m2=Math.abs(m2);
return (m1*r1+m2*r2)/(m1+m2);
};
function _3(_4){
var M=new m.Matrix2D(_4);
return _1.mixin(M,{dx:0,dy:0,xy:M.yx,yx:M.xy});
};
function _5(_6){
return (_6.xx*_6.yy<0||_6.xy*_6.yx>0)?-1:1;
};
function _7(_8){
var M=m.normalize(_8),b=-M.xx-M.yy,c=M.xx*M.yy-M.xy*M.yx,d=Math.sqrt(b*b-4*c),l1=-(b+(b<0?-d:d))/2,l2=c/l1,_9=M.xy/(l1-M.xx),_a=1,_b=M.xy/(l2-M.xx),_c=1;
if(eq(l1,l2)){
_9=1,_a=0,_b=0,_c=1;
}
if(!isFinite(_9)){
_9=1,_a=(l1-M.xx)/M.xy;
if(!isFinite(_a)){
_9=(l1-M.yy)/M.yx,_a=1;
if(!isFinite(_9)){
_9=1,_a=M.yx/(l1-M.yy);
}
}
}
if(!isFinite(_b)){
_b=1,_c=(l2-M.xx)/M.xy;
if(!isFinite(_c)){
_b=(l2-M.yy)/M.yx,_c=1;
if(!isFinite(_b)){
_b=1,_c=M.yx/(l2-M.yy);
}
}
}
var d1=Math.sqrt(_9*_9+_a*_a),d2=Math.sqrt(_b*_b+_c*_c);
if(!isFinite(_9/=d1)){
_9=0;
}
if(!isFinite(_a/=d1)){
_a=0;
}
if(!isFinite(_b/=d2)){
_b=0;
}
if(!isFinite(_c/=d2)){
_c=0;
}
return {value1:l1,value2:l2,vector1:{x:_9,y:_a},vector2:{x:_b,y:_c}};
};
function _d(M,_e){
var _f=_5(M),a=_e.angle1=(Math.atan2(M.yx,M.yy)+Math.atan2(-_f*M.xy,_f*M.xx))/2,cos=Math.cos(a),sin=Math.sin(a);
_e.sx=_2(M.xx/cos,cos,-M.xy/sin,sin);
_e.sy=_2(M.yy/cos,cos,M.yx/sin,sin);
return _e;
};
function _10(M,_11){
var _12=_5(M),a=_11.angle2=(Math.atan2(_12*M.yx,_12*M.xx)+Math.atan2(-M.xy,M.yy))/2,cos=Math.cos(a),sin=Math.sin(a);
_11.sx=_2(M.xx/cos,cos,M.yx/sin,sin);
_11.sy=_2(M.yy/cos,cos,-M.xy/sin,sin);
return _11;
};
return g.decompose=function(_13){
var M=m.normalize(_13),_14={dx:M.dx,dy:M.dy,sx:1,sy:1,angle1:0,angle2:0};
if(eq(M.xy,0)&&eq(M.yx,0)){
return _1.mixin(_14,{sx:M.xx,sy:M.yy});
}
if(eq(M.xx*M.yx,-M.xy*M.yy)){
return _d(M,_14);
}
if(eq(M.xx*M.xy,-M.yx*M.yy)){
return _10(M,_14);
}
var MT=_3(M),u=_7([M,MT]),v=_7([MT,M]),U=new m.Matrix2D({xx:u.vector1.x,xy:u.vector2.x,yx:u.vector1.y,yy:u.vector2.y}),VT=new m.Matrix2D({xx:v.vector1.x,xy:v.vector1.y,yx:v.vector2.x,yy:v.vector2.y}),S=new m.Matrix2D([m.invert(U),M,m.invert(VT)]);
_d(VT,_14);
S.xx*=_14.sx;
S.yy*=_14.sy;
_10(U,_14);
S.xx*=_14.sx;
S.yy*=_14.sy;
return _1.mixin(_14,{sx:S.xx,sy:S.yy});
};
});
