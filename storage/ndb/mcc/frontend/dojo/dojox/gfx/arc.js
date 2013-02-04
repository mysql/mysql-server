//>>built
define("dojox/gfx/arc",["./_base","dojo/_base/lang","./matrix"],function(g,_1,m){
var _2=2*Math.PI,_3=Math.PI/4,_4=Math.PI/8,_5=_3+_4,_6=_7(_4);
function _7(_8){
var _9=Math.cos(_8),_a=Math.sin(_8),p2={x:_9+(4/3)*(1-_9),y:_a-(4/3)*_9*(1-_9)/_a};
return {s:{x:_9,y:-_a},c1:{x:p2.x,y:-p2.y},c2:p2,e:{x:_9,y:_a}};
};
var _b=g.arc={unitArcAsBezier:_7,curvePI4:_6,arcAsBezier:function(_c,rx,ry,_d,_e,_f,x,y){
_e=Boolean(_e);
_f=Boolean(_f);
var _10=m._degToRad(_d),rx2=rx*rx,ry2=ry*ry,pa=m.multiplyPoint(m.rotate(-_10),{x:(_c.x-x)/2,y:(_c.y-y)/2}),_11=pa.x*pa.x,_12=pa.y*pa.y,c1=Math.sqrt((rx2*ry2-rx2*_12-ry2*_11)/(rx2*_12+ry2*_11));
if(isNaN(c1)){
c1=0;
}
var ca={x:c1*rx*pa.y/ry,y:-c1*ry*pa.x/rx};
if(_e==_f){
ca={x:-ca.x,y:-ca.y};
}
var c=m.multiplyPoint([m.translate((_c.x+x)/2,(_c.y+y)/2),m.rotate(_10)],ca);
var _13=m.normalize([m.translate(c.x,c.y),m.rotate(_10),m.scale(rx,ry)]);
var _14=m.invert(_13),sp=m.multiplyPoint(_14,_c),ep=m.multiplyPoint(_14,x,y),_15=Math.atan2(sp.y,sp.x),_16=Math.atan2(ep.y,ep.x),_17=_15-_16;
if(_f){
_17=-_17;
}
if(_17<0){
_17+=_2;
}else{
if(_17>_2){
_17-=_2;
}
}
var _18=_4,_19=_6,_1a=_f?_18:-_18,_1b=[];
for(var _1c=_17;_1c>0;_1c-=_3){
if(_1c<_5){
_18=_1c/2;
_19=_7(_18);
_1a=_f?_18:-_18;
_1c=0;
}
var c2,e,M=m.normalize([_13,m.rotate(_15+_1a)]);
if(_f){
c1=m.multiplyPoint(M,_19.c1);
c2=m.multiplyPoint(M,_19.c2);
e=m.multiplyPoint(M,_19.e);
}else{
c1=m.multiplyPoint(M,_19.c2);
c2=m.multiplyPoint(M,_19.c1);
e=m.multiplyPoint(M,_19.s);
}
_1b.push([c1.x,c1.y,c2.x,c2.y,e.x,e.y]);
_15+=2*_1a;
}
return _1b;
}};
return _b;
});
