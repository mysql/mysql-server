//>>built
define("dojox/gfx3d/vector",["dojo/_base/lang","dojo/_base/array","./_base"],function(_1,_2,_3){
_3.vector={sum:function(){
var v={x:0,y:0,z:0};
_2.forEach(arguments,function(_4){
v.x+=_4.x;
v.y+=_4.y;
v.z+=_4.z;
});
return v;
},center:function(){
var l=arguments.length;
if(l==0){
return {x:0,y:0,z:0};
}
var v=_3.vector.sum(arguments);
return {x:v.x/l,y:v.y/l,z:v.z/l};
},substract:function(a,b){
return {x:a.x-b.x,y:a.y-b.y,z:a.z-b.z};
},_crossProduct:function(x,y,z,u,v,w){
return {x:y*w-z*v,y:z*u-x*w,z:x*v-y*u};
},crossProduct:function(a,b,c,d,e,f){
if(arguments.length==6&&_2.every(arguments,function(_5){
return typeof _5=="number";
})){
return _3.vector._crossProduct(a,b,c,d,e,f);
}
return _3.vector._crossProduct(a.x,a.y,a.z,b.x,b.y,b.z);
},_dotProduct:function(x,y,z,u,v,w){
return x*u+y*v+z*w;
},dotProduct:function(a,b,c,d,e,f){
if(arguments.length==6&&_2.every(arguments,function(_6){
return typeof _6=="number";
})){
return _3.vector._dotProduct(a,b,c,d,e,f);
}
return _3.vector._dotProduct(a.x,a.y,a.z,b.x,b.y,b.z);
},normalize:function(a,b,c){
var l,m,n;
if(a instanceof Array){
l=a[0];
m=a[1];
n=a[2];
}else{
l=a;
m=b;
n=c;
}
var u=_3.vector.substract(m,l);
var v=_3.vector.substract(n,l);
return _3.vector.crossProduct(u,v);
}};
return _3.vector;
});
