//>>built
define("dojox/gfx3d/matrix",["dojo/_base/lang","./_base"],function(_1,_2){
_2.matrix={_degToRad:function(_3){
return Math.PI*_3/180;
},_radToDeg:function(_4){
return _4/Math.PI*180;
}};
_2.matrix.Matrix3D=function(_5){
if(_5){
if(typeof _5=="number"){
this.xx=this.yy=this.zz=_5;
}else{
if(_5 instanceof Array){
if(_5.length>0){
var m=_2.matrix.normalize(_5[0]);
for(var i=1;i<_5.length;++i){
var l=m;
var r=_2.matrix.normalize(_5[i]);
m=new _2.matrix.Matrix3D();
m.xx=l.xx*r.xx+l.xy*r.yx+l.xz*r.zx;
m.xy=l.xx*r.xy+l.xy*r.yy+l.xz*r.zy;
m.xz=l.xx*r.xz+l.xy*r.yz+l.xz*r.zz;
m.yx=l.yx*r.xx+l.yy*r.yx+l.yz*r.zx;
m.yy=l.yx*r.xy+l.yy*r.yy+l.yz*r.zy;
m.yz=l.yx*r.xz+l.yy*r.yz+l.yz*r.zz;
m.zx=l.zx*r.xx+l.zy*r.yx+l.zz*r.zx;
m.zy=l.zx*r.xy+l.zy*r.yy+l.zz*r.zy;
m.zz=l.zx*r.xz+l.zy*r.yz+l.zz*r.zz;
m.dx=l.xx*r.dx+l.xy*r.dy+l.xz*r.dz+l.dx;
m.dy=l.yx*r.dx+l.yy*r.dy+l.yz*r.dz+l.dy;
m.dz=l.zx*r.dx+l.zy*r.dy+l.zz*r.dz+l.dz;
}
_1.mixin(this,m);
}
}else{
_1.mixin(this,_5);
}
}
}
};
_1.extend(_2.matrix.Matrix3D,{xx:1,xy:0,xz:0,yx:0,yy:1,yz:0,zx:0,zy:0,zz:1,dx:0,dy:0,dz:0});
_1.mixin(_2.matrix,{identity:new _2.matrix.Matrix3D(),translate:function(a,b,c){
if(arguments.length>1){
return new _2.matrix.Matrix3D({dx:a,dy:b,dz:c});
}
return new _2.matrix.Matrix3D({dx:a.x,dy:a.y,dz:a.z});
},scale:function(a,b,c){
if(arguments.length>1){
return new _2.matrix.Matrix3D({xx:a,yy:b,zz:c});
}
if(typeof a=="number"){
return new _2.matrix.Matrix3D({xx:a,yy:a,zz:a});
}
return new _2.matrix.Matrix3D({xx:a.x,yy:a.y,zz:a.z});
},rotateX:function(_6){
var c=Math.cos(_6);
var s=Math.sin(_6);
return new _2.matrix.Matrix3D({yy:c,yz:-s,zy:s,zz:c});
},rotateXg:function(_7){
return _2.matrix.rotateX(_2.matrix._degToRad(_7));
},rotateY:function(_8){
var c=Math.cos(_8);
var s=Math.sin(_8);
return new _2.matrix.Matrix3D({xx:c,xz:s,zx:-s,zz:c});
},rotateYg:function(_9){
return _2.matrix.rotateY(_2.matrix._degToRad(_9));
},rotateZ:function(_a){
var c=Math.cos(_a);
var s=Math.sin(_a);
return new _2.matrix.Matrix3D({xx:c,xy:-s,yx:s,yy:c});
},rotateZg:function(_b){
return _2.matrix.rotateZ(_2.matrix._degToRad(_b));
},cameraTranslate:function(a,b,c){
if(arguments.length>1){
return new _2.matrix.Matrix3D({dx:-a,dy:-b,dz:-c});
}
return new _2.matrix.Matrix3D({dx:-a.x,dy:-a.y,dz:-a.z});
},cameraRotateX:function(_c){
var c=Math.cos(-_c);
var s=Math.sin(-_c);
return new _2.matrix.Matrix3D({yy:c,yz:-s,zy:s,zz:c});
},cameraRotateXg:function(_d){
return _2.matrix.rotateX(_2.matrix._degToRad(_d));
},cameraRotateY:function(_e){
var c=Math.cos(-_e);
var s=Math.sin(-_e);
return new _2.matrix.Matrix3D({xx:c,xz:s,zx:-s,zz:c});
},cameraRotateYg:function(_f){
return _2.matrix.rotateY(dojox.gfx3d.matrix._degToRad(_f));
},cameraRotateZ:function(_10){
var c=Math.cos(-_10);
var s=Math.sin(-_10);
return new _2.matrix.Matrix3D({xx:c,xy:-s,yx:s,yy:c});
},cameraRotateZg:function(_11){
return _2.matrix.rotateZ(_2.matrix._degToRad(_11));
},normalize:function(_12){
return (_12 instanceof _2.matrix.Matrix3D)?_12:new _2.matrix.Matrix3D(_12);
},clone:function(_13){
var obj=new _2.matrix.Matrix3D();
for(var i in _13){
if(typeof (_13[i])=="number"&&typeof (obj[i])=="number"&&obj[i]!=_13[i]){
obj[i]=_13[i];
}
}
return obj;
},invert:function(_14){
var m=_2.matrix.normalize(_14);
var D=m.xx*m.yy*m.zz+m.xy*m.yz*m.zx+m.xz*m.yx*m.zy-m.xx*m.yz*m.zy-m.xy*m.yx*m.zz-m.xz*m.yy*m.zx;
var M=new _2.matrix.Matrix3D({xx:(m.yy*m.zz-m.yz*m.zy)/D,xy:(m.xz*m.zy-m.xy*m.zz)/D,xz:(m.xy*m.yz-m.xz*m.yy)/D,yx:(m.yz*m.zx-m.yx*m.zz)/D,yy:(m.xx*m.zz-m.xz*m.zx)/D,yz:(m.xz*m.yx-m.xx*m.yz)/D,zx:(m.yx*m.zy-m.yy*m.zx)/D,zy:(m.xy*m.zx-m.xx*m.zy)/D,zz:(m.xx*m.yy-m.xy*m.yx)/D,dx:-1*(m.xy*m.yz*m.dz+m.xz*m.dy*m.zy+m.dx*m.yy*m.zz-m.xy*m.dy*m.zz-m.xz*m.yy*m.dz-m.dx*m.yz*m.zy)/D,dy:(m.xx*m.yz*m.dz+m.xz*m.dy*m.zx+m.dx*m.yx*m.zz-m.xx*m.dy*m.zz-m.xz*m.yx*m.dz-m.dx*m.yz*m.zx)/D,dz:-1*(m.xx*m.yy*m.dz+m.xy*m.dy*m.zx+m.dx*m.yx*m.zy-m.xx*m.dy*m.zy-m.xy*m.yx*m.dz-m.dx*m.yy*m.zx)/D});
return M;
},_multiplyPoint:function(m,x,y,z){
return {x:m.xx*x+m.xy*y+m.xz*z+m.dx,y:m.yx*x+m.yy*y+m.yz*z+m.dy,z:m.zx*x+m.zy*y+m.zz*z+m.dz};
},multiplyPoint:function(_15,a,b,c){
var m=_2.matrix.normalize(_15);
if(typeof a=="number"&&typeof b=="number"&&typeof c=="number"){
return _2.matrix._multiplyPoint(m,a,b,c);
}
return _2.matrix._multiplyPoint(m,a.x,a.y,a.z);
},multiply:function(_16){
var m=_2.matrix.normalize(_16);
for(var i=1;i<arguments.length;++i){
var l=m;
var r=_2.matrix.normalize(arguments[i]);
m=new _2.matrix.Matrix3D();
m.xx=l.xx*r.xx+l.xy*r.yx+l.xz*r.zx;
m.xy=l.xx*r.xy+l.xy*r.yy+l.xz*r.zy;
m.xz=l.xx*r.xz+l.xy*r.yz+l.xz*r.zz;
m.yx=l.yx*r.xx+l.yy*r.yx+l.yz*r.zx;
m.yy=l.yx*r.xy+l.yy*r.yy+l.yz*r.zy;
m.yz=l.yx*r.xz+l.yy*r.yz+l.yz*r.zz;
m.zx=l.zx*r.xx+l.zy*r.yx+l.zz*r.zx;
m.zy=l.zx*r.xy+l.zy*r.yy+l.zz*r.zy;
m.zz=l.zx*r.xz+l.zy*r.yz+l.zz*r.zz;
m.dx=l.xx*r.dx+l.xy*r.dy+l.xz*r.dz+l.dx;
m.dy=l.yx*r.dx+l.yy*r.dy+l.yz*r.dz+l.dy;
m.dz=l.zx*r.dx+l.zy*r.dy+l.zz*r.dz+l.dz;
}
return m;
},_project:function(m,x,y,z){
return {x:m.xx*x+m.xy*y+m.xz*z+m.dx,y:m.yx*x+m.yy*y+m.yz*z+m.dy,z:m.zx*x+m.zy*y+m.zz*z+m.dz};
},project:function(_17,a,b,c){
var m=_2.matrix.normalize(_17);
if(typeof a=="number"&&typeof b=="number"&&typeof c=="number"){
return _2.matrix._project(m,a,b,c);
}
return _2.matrix._project(m,a.x,a.y,a.z);
}});
_2.Matrix3D=_2.matrix.Matrix3D;
return _2.matrix;
});
