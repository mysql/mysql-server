//>>built
define("dojox/charting/plot3d/Bars",["dojox/gfx3d","dojo/_base/kernel","dojo/_base/declare","dojo/_base/Color","./Base"],function(_1,_2,_3,_4,_5){
var _6=function(a,f,o){
a=typeof a=="string"?a.split(""):a;
o=o||_2.global;
var z=a[0];
for(var i=1;i<a.length;z=f.call(o,z,a[i++])){
}
return z;
};
return _3("dojox.charting.plot3d.Bars",_5,{constructor:function(_7,_8,_9){
this.depth="auto";
this.gap=0;
this.data=[];
this.material={type:"plastic",finish:"dull",color:"lime"};
if(_9){
if("depth" in _9){
this.depth=_9.depth;
}
if("gap" in _9){
this.gap=_9.gap;
}
if("material" in _9){
var m=_9.material;
if(typeof m=="string"||m instanceof _4){
this.material.color=m;
}else{
this.material=m;
}
}
}
},getDepth:function(){
if(this.depth=="auto"){
var w=this.width;
if(this.data&&this.data.length){
w=w/this.data.length;
}
return w-2*this.gap;
}
return this.depth;
},generate:function(_a,_b){
if(!this.data){
return this;
}
var _c=this.width/this.data.length,_d=0,_e=this.depth=="auto"?_c-2*this.gap:this.depth,_f=this.height/_6(this.data,Math.max);
if(!_b){
_b=_a.view;
}
for(var i=0;i<this.data.length;++i,_d+=_c){
_b.createCube({bottom:{x:_d+this.gap,y:0,z:0},top:{x:_d+_c-this.gap,y:this.data[i]*_f,z:_e}}).setFill(this.material);
}
}});
});
