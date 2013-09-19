//>>built
define("dojox/gfx3d/gradient",["dojo/_base/lang","./matrix","./vector"],function(_1,m,v){
var _2=_1.getObject("dojox.gfx3d",true);
var _3=function(a,b){
return Math.sqrt(Math.pow(b.x-a.x,2)+Math.pow(b.y-a.y,2));
};
var N=32;
_2.gradient=function(_4,_5,_6,_7,_8,to,_9){
var mx=m.normalize(_9),f=m.multiplyPoint(mx,_7*Math.cos(_8)+_6.x,_7*Math.sin(_8)+_6.y,_6.z),t=m.multiplyPoint(mx,_7*Math.cos(to)+_6.x,_7*Math.sin(to)+_6.y,_6.z),c=m.multiplyPoint(mx,_6.x,_6.y,_6.z),_a=(to-_8)/N,r=_3(f,t)/2,_b=_4[_5.type],_c=_5.finish,_d=_5.color,_e=[{offset:0,color:_b.call(_4,v.substract(f,c),_c,_d)}];
for(var a=_8+_a;a<to;a+=_a){
var p=m.multiplyPoint(mx,_7*Math.cos(a)+_6.x,_7*Math.sin(a)+_6.y,_6.z),df=_3(f,p),dt=_3(t,p);
_e.push({offset:df/(df+dt),color:_b.call(_4,v.substract(p,c),_c,_d)});
}
_e.push({offset:1,color:_b.call(_4,v.substract(t,c),_c,_d)});
return {type:"linear",x1:0,y1:-r,x2:0,y2:r,colors:_e};
};
return _2.gradient;
});
