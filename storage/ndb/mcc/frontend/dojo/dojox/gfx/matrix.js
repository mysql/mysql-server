//>>built
define("dojox/gfx/matrix",["./_base","dojo/_base/lang"],function(g,_1){
var m=g.matrix={};
var _2={};
m._degToRad=function(_3){
return _2[_3]||(_2[_3]=(Math.PI*_3/180));
};
m._radToDeg=function(_4){
return _4/Math.PI*180;
};
m.Matrix2D=function(_5){
if(_5){
if(typeof _5=="number"){
this.xx=this.yy=_5;
}else{
if(_5 instanceof Array){
if(_5.length>0){
var _6=m.normalize(_5[0]);
for(var i=1;i<_5.length;++i){
var l=_6,r=m.normalize(_5[i]);
_6=new m.Matrix2D();
_6.xx=l.xx*r.xx+l.xy*r.yx;
_6.xy=l.xx*r.xy+l.xy*r.yy;
_6.yx=l.yx*r.xx+l.yy*r.yx;
_6.yy=l.yx*r.xy+l.yy*r.yy;
_6.dx=l.xx*r.dx+l.xy*r.dy+l.dx;
_6.dy=l.yx*r.dx+l.yy*r.dy+l.dy;
}
_1.mixin(this,_6);
}
}else{
_1.mixin(this,_5);
}
}
}
};
_1.extend(m.Matrix2D,{xx:1,xy:0,yx:0,yy:1,dx:0,dy:0});
_1.mixin(m,{identity:new m.Matrix2D(),flipX:new m.Matrix2D({xx:-1}),flipY:new m.Matrix2D({yy:-1}),flipXY:new m.Matrix2D({xx:-1,yy:-1}),translate:function(a,b){
if(arguments.length>1){
return new m.Matrix2D({dx:a,dy:b});
}
return new m.Matrix2D({dx:a.x,dy:a.y});
},scale:function(a,b){
if(arguments.length>1){
return new m.Matrix2D({xx:a,yy:b});
}
if(typeof a=="number"){
return new m.Matrix2D({xx:a,yy:a});
}
return new m.Matrix2D({xx:a.x,yy:a.y});
},rotate:function(_7){
var c=Math.cos(_7);
var s=Math.sin(_7);
return new m.Matrix2D({xx:c,xy:-s,yx:s,yy:c});
},rotateg:function(_8){
return m.rotate(m._degToRad(_8));
},skewX:function(_9){
return new m.Matrix2D({xy:Math.tan(_9)});
},skewXg:function(_a){
return m.skewX(m._degToRad(_a));
},skewY:function(_b){
return new m.Matrix2D({yx:Math.tan(_b)});
},skewYg:function(_c){
return m.skewY(m._degToRad(_c));
},reflect:function(a,b){
if(arguments.length==1){
b=a.y;
a=a.x;
}
var a2=a*a,b2=b*b,n2=a2+b2,xy=2*a*b/n2;
return new m.Matrix2D({xx:2*a2/n2-1,xy:xy,yx:xy,yy:2*b2/n2-1});
},project:function(a,b){
if(arguments.length==1){
b=a.y;
a=a.x;
}
var a2=a*a,b2=b*b,n2=a2+b2,xy=a*b/n2;
return new m.Matrix2D({xx:a2/n2,xy:xy,yx:xy,yy:b2/n2});
},normalize:function(_d){
return (_d instanceof m.Matrix2D)?_d:new m.Matrix2D(_d);
},isIdentity:function(_e){
return _e.xx==1&&_e.xy==0&&_e.yx==0&&_e.yy==1&&_e.dx==0&&_e.dy==0;
},clone:function(_f){
var obj=new m.Matrix2D();
for(var i in _f){
if(typeof (_f[i])=="number"&&typeof (obj[i])=="number"&&obj[i]!=_f[i]){
obj[i]=_f[i];
}
}
return obj;
},invert:function(_10){
var M=m.normalize(_10),D=M.xx*M.yy-M.xy*M.yx;
M=new m.Matrix2D({xx:M.yy/D,xy:-M.xy/D,yx:-M.yx/D,yy:M.xx/D,dx:(M.xy*M.dy-M.yy*M.dx)/D,dy:(M.yx*M.dx-M.xx*M.dy)/D});
return M;
},_multiplyPoint:function(_11,x,y){
return {x:_11.xx*x+_11.xy*y+_11.dx,y:_11.yx*x+_11.yy*y+_11.dy};
},multiplyPoint:function(_12,a,b){
var M=m.normalize(_12);
if(typeof a=="number"&&typeof b=="number"){
return m._multiplyPoint(M,a,b);
}
return m._multiplyPoint(M,a.x,a.y);
},multiplyRectangle:function(_13,_14){
var M=m.normalize(_13);
_14=_14||{x:0,y:0,width:0,height:0};
if(m.isIdentity(M)){
return {x:_14.x,y:_14.y,width:_14.width,height:_14.height};
}
var p0=m.multiplyPoint(M,_14.x,_14.y),p1=m.multiplyPoint(M,_14.x,_14.y+_14.height),p2=m.multiplyPoint(M,_14.x+_14.width,_14.y),p3=m.multiplyPoint(M,_14.x+_14.width,_14.y+_14.height),_15=Math.min(p0.x,p1.x,p2.x,p3.x),_16=Math.min(p0.y,p1.y,p2.y,p3.y),_17=Math.max(p0.x,p1.x,p2.x,p3.x),_18=Math.max(p0.y,p1.y,p2.y,p3.y);
return {x:_15,y:_16,width:_17-_15,height:_18-_16};
},multiply:function(_19){
var M=m.normalize(_19);
for(var i=1;i<arguments.length;++i){
var l=M,r=m.normalize(arguments[i]);
M=new m.Matrix2D();
M.xx=l.xx*r.xx+l.xy*r.yx;
M.xy=l.xx*r.xy+l.xy*r.yy;
M.yx=l.yx*r.xx+l.yy*r.yx;
M.yy=l.yx*r.xy+l.yy*r.yy;
M.dx=l.xx*r.dx+l.xy*r.dy+l.dx;
M.dy=l.yx*r.dx+l.yy*r.dy+l.dy;
}
return M;
},_sandwich:function(_1a,x,y){
return m.multiply(m.translate(x,y),_1a,m.translate(-x,-y));
},scaleAt:function(a,b,c,d){
switch(arguments.length){
case 4:
return m._sandwich(m.scale(a,b),c,d);
case 3:
if(typeof c=="number"){
return m._sandwich(m.scale(a),b,c);
}
return m._sandwich(m.scale(a,b),c.x,c.y);
}
return m._sandwich(m.scale(a),b.x,b.y);
},rotateAt:function(_1b,a,b){
if(arguments.length>2){
return m._sandwich(m.rotate(_1b),a,b);
}
return m._sandwich(m.rotate(_1b),a.x,a.y);
},rotategAt:function(_1c,a,b){
if(arguments.length>2){
return m._sandwich(m.rotateg(_1c),a,b);
}
return m._sandwich(m.rotateg(_1c),a.x,a.y);
},skewXAt:function(_1d,a,b){
if(arguments.length>2){
return m._sandwich(m.skewX(_1d),a,b);
}
return m._sandwich(m.skewX(_1d),a.x,a.y);
},skewXgAt:function(_1e,a,b){
if(arguments.length>2){
return m._sandwich(m.skewXg(_1e),a,b);
}
return m._sandwich(m.skewXg(_1e),a.x,a.y);
},skewYAt:function(_1f,a,b){
if(arguments.length>2){
return m._sandwich(m.skewY(_1f),a,b);
}
return m._sandwich(m.skewY(_1f),a.x,a.y);
},skewYgAt:function(_20,a,b){
if(arguments.length>2){
return m._sandwich(m.skewYg(_20),a,b);
}
return m._sandwich(m.skewYg(_20),a.x,a.y);
}});
g.Matrix2D=m.Matrix2D;
return m;
});
