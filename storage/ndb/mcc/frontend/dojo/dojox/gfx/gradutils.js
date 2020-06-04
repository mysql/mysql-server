//>>built
define("dojox/gfx/gradutils",["./_base","dojo/_base/lang","./matrix","dojo/_base/Color"],function(g,_1,m,_2){
var _3=g.gradutils={};
function _4(o,c){
if(o<=0){
return c[0].color;
}
var _5=c.length;
if(o>=1){
return c[_5-1].color;
}
for(var i=0;i<_5;++i){
var _6=c[i];
if(_6.offset>=o){
if(i){
var _7=c[i-1];
return _2.blendColors(new _2(_7.color),new _2(_6.color),(o-_7.offset)/(_6.offset-_7.offset));
}
return _6.color;
}
}
return c[_5-1].color;
};
_3.getColor=function(_8,pt){
var o;
if(_8){
switch(_8.type){
case "linear":
var _9=Math.atan2(_8.y2-_8.y1,_8.x2-_8.x1),_a=m.rotate(-_9),_b=m.project(_8.x2-_8.x1,_8.y2-_8.y1),p=m.multiplyPoint(_b,pt),_c=m.multiplyPoint(_b,_8.x1,_8.y1),_d=m.multiplyPoint(_b,_8.x2,_8.y2),_e=m.multiplyPoint(_a,_d.x-_c.x,_d.y-_c.y).x;
o=m.multiplyPoint(_a,p.x-_c.x,p.y-_c.y).x/_e;
break;
case "radial":
var dx=pt.x-_8.cx,dy=pt.y-_8.cy;
o=Math.sqrt(dx*dx+dy*dy)/_8.r;
break;
}
return _4(o,_8.colors);
}
return new _2(_8||[0,0,0,0]);
};
_3.reverse=function(_f){
if(_f){
switch(_f.type){
case "linear":
case "radial":
_f=_1.delegate(_f);
if(_f.colors){
var c=_f.colors,l=c.length,i=0,_10,n=_f.colors=new Array(c.length);
for(;i<l;++i){
_10=c[i];
n[i]={offset:1-_10.offset,color:_10.color};
}
n.sort(function(a,b){
return a.offset-b.offset;
});
}
break;
}
}
return _f;
};
return _3;
});
