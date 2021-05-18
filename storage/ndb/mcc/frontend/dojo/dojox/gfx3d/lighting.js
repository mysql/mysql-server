//>>built
define("dojox/gfx3d/lighting",["dojo/_base/lang","dojo/_base/Color","dojo/_base/declare","dojox/gfx/_base","./_base"],function(_1,_2,_3,_4,_5){
var _6=_5.lighting={black:function(){
return {r:0,g:0,b:0,a:1};
},white:function(){
return {r:1,g:1,b:1,a:1};
},toStdColor:function(c){
c=_4.normalizeColor(c);
return {r:c.r/255,g:c.g/255,b:c.b/255,a:c.a};
},fromStdColor:function(c){
return new _2([Math.round(255*c.r),Math.round(255*c.g),Math.round(255*c.b),c.a]);
},scaleColor:function(s,c){
return {r:s*c.r,g:s*c.g,b:s*c.b,a:s*c.a};
},addColor:function(a,b){
return {r:a.r+b.r,g:a.g+b.g,b:a.b+b.b,a:a.a+b.a};
},multiplyColor:function(a,b){
return {r:a.r*b.r,g:a.g*b.g,b:a.b*b.b,a:a.a*b.a};
},saturateColor:function(c){
return {r:c.r<0?0:c.r>1?1:c.r,g:c.g<0?0:c.g>1?1:c.g,b:c.b<0?0:c.b>1?1:c.b,a:c.a<0?0:c.a>1?1:c.a};
},mixColor:function(c1,c2,s){
return _6.addColor(_6.scaleColor(s,c1),_6.scaleColor(1-s,c2));
},diff2Color:function(c1,c2){
var r=c1.r-c2.r;
var g=c1.g-c2.g;
var b=c1.b-c2.b;
var a=c1.a-c2.a;
return r*r+g*g+b*b+a*a;
},length2Color:function(c){
return c.r*c.r+c.g*c.g+c.b*c.b+c.a*c.a;
},dot:function(a,b){
return a.x*b.x+a.y*b.y+a.z*b.z;
},scale:function(s,v){
return {x:s*v.x,y:s*v.y,z:s*v.z};
},add:function(a,b){
return {x:a.x+b.x,y:a.y+b.y,z:a.z+b.z};
},saturate:function(v){
return Math.min(Math.max(v,0),1);
},length:function(v){
return Math.sqrt(_5.lighting.dot(v,v));
},normalize:function(v){
return _6.scale(1/_6.length(v),v);
},faceforward:function(n,i){
var p=_5.lighting;
var s=p.dot(i,n)<0?1:-1;
return p.scale(s,n);
},reflect:function(i,n){
var p=_5.lighting;
return p.add(i,p.scale(-2*p.dot(i,n),n));
},diffuse:function(_7,_8){
var c=_6.black();
for(var i=0;i<_8.length;++i){
var l=_8[i],d=_6.dot(_6.normalize(l.direction),_7);
c=_6.addColor(c,_6.scaleColor(d,l.color));
}
return _6.saturateColor(c);
},specular:function(_9,v,_a,_b){
var c=_6.black();
for(var i=0;i<_b.length;++i){
var l=_b[i],h=_6.normalize(_6.add(_6.normalize(l.direction),v)),s=Math.pow(Math.max(0,_6.dot(_9,h)),1/_a);
c=_6.addColor(c,_6.scaleColor(s,l.color));
}
return _6.saturateColor(c);
},phong:function(_c,v,_d,_e){
_c=_6.normalize(_c);
var c=_6.black();
for(var i=0;i<_e.length;++i){
var l=_e[i],r=_6.reflect(_6.scale(-1,_6.normalize(v)),_c),s=Math.pow(Math.max(0,_6.dot(r,_6.normalize(l.direction))),_d);
c=_6.addColor(c,_6.scaleColor(s,l.color));
}
return _6.saturateColor(c);
}};
_3("dojox.gfx3d.lighting.Model",null,{constructor:function(_f,_10,_11,_12){
this.incident=_6.normalize(_f);
this.lights=[];
for(var i=0;i<_10.length;++i){
var l=_10[i];
this.lights.push({direction:_6.normalize(l.direction),color:_6.toStdColor(l.color)});
}
this.ambient=_6.toStdColor(_11.color?_11.color:"white");
this.ambient=_6.scaleColor(_11.intensity,this.ambient);
this.ambient=_6.scaleColor(this.ambient.a,this.ambient);
this.ambient.a=1;
this.specular=_6.toStdColor(_12?_12:"white");
this.specular=_6.scaleColor(this.specular.a,this.specular);
this.specular.a=1;
this.npr_cool={r:0,g:0,b:0.4,a:1};
this.npr_warm={r:0.4,g:0.4,b:0.2,a:1};
this.npr_alpha=0.2;
this.npr_beta=0.6;
this.npr_scale=0.6;
},constant:function(_13,_14,_15){
_15=_6.toStdColor(_15);
var _16=_15.a,_17=_6.scaleColor(_16,_15);
_17.a=_16;
return _6.fromStdColor(_6.saturateColor(_17));
},matte:function(_18,_19,_1a){
if(typeof _19=="string"){
_19=_6.finish[_19];
}
_1a=_6.toStdColor(_1a);
_18=_6.faceforward(_6.normalize(_18),this.incident);
var _1b=_6.scaleColor(_19.Ka,this.ambient),_1c=_6.saturate(-4*_6.dot(_18,this.incident)),_1d=_6.scaleColor(_1c*_19.Kd,_6.diffuse(_18,this.lights)),_1e=_6.scaleColor(_1a.a,_6.multiplyColor(_1a,_6.addColor(_1b,_1d)));
_1e.a=_1a.a;
return _6.fromStdColor(_6.saturateColor(_1e));
},metal:function(_1f,_20,_21){
if(typeof _20=="string"){
_20=_6.finish[_20];
}
_21=_6.toStdColor(_21);
_1f=_6.faceforward(_6.normalize(_1f),this.incident);
var v=_6.scale(-1,this.incident),_22,_23,_24=_6.scaleColor(_20.Ka,this.ambient),_25=_6.saturate(-4*_6.dot(_1f,this.incident));
if("phong" in _20){
_22=_6.scaleColor(_25*_20.Ks*_20.phong,_6.phong(_1f,v,_20.phong_size,this.lights));
}else{
_22=_6.scaleColor(_25*_20.Ks,_6.specular(_1f,v,_20.roughness,this.lights));
}
_23=_6.scaleColor(_21.a,_6.addColor(_6.multiplyColor(_21,_24),_6.multiplyColor(this.specular,_22)));
_23.a=_21.a;
return _6.fromStdColor(_6.saturateColor(_23));
},plastic:function(_26,_27,_28){
if(typeof _27=="string"){
_27=_6.finish[_27];
}
_28=_6.toStdColor(_28);
_26=_6.faceforward(_6.normalize(_26),this.incident);
var v=_6.scale(-1,this.incident),_29,_2a,_2b=_6.scaleColor(_27.Ka,this.ambient),_2c=_6.saturate(-4*_6.dot(_26,this.incident)),_2d=_6.scaleColor(_2c*_27.Kd,_6.diffuse(_26,this.lights));
if("phong" in _27){
_29=_6.scaleColor(_2c*_27.Ks*_27.phong,_6.phong(_26,v,_27.phong_size,this.lights));
}else{
_29=_6.scaleColor(_2c*_27.Ks,_6.specular(_26,v,_27.roughness,this.lights));
}
_2a=_6.scaleColor(_28.a,_6.addColor(_6.multiplyColor(_28,_6.addColor(_2b,_2d)),_6.multiplyColor(this.specular,_29)));
_2a.a=_28.a;
return _6.fromStdColor(_6.saturateColor(_2a));
},npr:function(_2e,_2f,_30){
if(typeof _2f=="string"){
_2f=_6.finish[_2f];
}
_30=_6.toStdColor(_30);
_2e=_6.faceforward(_6.normalize(_2e),this.incident);
var _31=_6.scaleColor(_2f.Ka,this.ambient),_32=_6.saturate(-4*_6.dot(_2e,this.incident)),_33=_6.scaleColor(_32*_2f.Kd,_6.diffuse(_2e,this.lights)),_34=_6.scaleColor(_30.a,_6.multiplyColor(_30,_6.addColor(_31,_33))),_35=_6.addColor(this.npr_cool,_6.scaleColor(this.npr_alpha,_34)),_36=_6.addColor(this.npr_warm,_6.scaleColor(this.npr_beta,_34)),d=(1+_6.dot(this.incident,_2e))/2,_34=_6.scaleColor(this.npr_scale,_6.addColor(_34,_6.mixColor(_35,_36,d)));
_34.a=_30.a;
return _6.fromStdColor(_6.saturateColor(_34));
}});
_5.lighting.finish={defaults:{Ka:0.1,Kd:0.6,Ks:0,roughness:0.05},dull:{Ka:0.1,Kd:0.6,Ks:0.5,roughness:0.15},shiny:{Ka:0.1,Kd:0.6,Ks:1,roughness:0.001},glossy:{Ka:0.1,Kd:0.6,Ks:1,roughness:0.0001},phong_dull:{Ka:0.1,Kd:0.6,Ks:0.5,phong:0.5,phong_size:1},phong_shiny:{Ka:0.1,Kd:0.6,Ks:1,phong:1,phong_size:200},phong_glossy:{Ka:0.1,Kd:0.6,Ks:1,phong:1,phong_size:300},luminous:{Ka:1,Kd:0,Ks:0,roughness:0.05},metalA:{Ka:0.35,Kd:0.3,Ks:0.8,roughness:1/20},metalB:{Ka:0.3,Kd:0.4,Ks:0.7,roughness:1/60},metalC:{Ka:0.25,Kd:0.5,Ks:0.8,roughness:1/80},metalD:{Ka:0.15,Kd:0.6,Ks:0.8,roughness:1/100},metalE:{Ka:0.1,Kd:0.7,Ks:0.8,roughness:1/120}};
return _6;
});
