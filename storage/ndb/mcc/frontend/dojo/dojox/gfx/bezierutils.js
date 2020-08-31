//>>built
define("dojox/gfx/bezierutils",["./_base"],function(_1){
var bu=_1.bezierutils={},_2=0.1;
var _3=bu.tAtLength=function(_4,_5){
var t=0,_6=_4.length==6,_7=0,_8=0,_9=_6?_a:_b;
var _c=function(p,_d){
var _e=0;
for(var i=0;i<p.length-2;i+=2){
_e+=_f(p[i],p[i+1],p[i+2],p[i+3]);
}
var _10=_6?_f(_4[0],_4[1],_4[4],_4[5]):_f(_4[0],_4[1],_4[6],_4[7]);
if(_e-_10>_d||_7+_e>_5+_d){
++_8;
var _11=_9(p,0.5);
_c(_11[0],_d);
if(Math.abs(_7-_5)<=_d){
return;
}
_c(_11[1],_d);
return;
}
_7+=_e;
t+=1/(1<<_8);
};
if(_5){
_c(_4,0.5);
}
return t;
};
var _12=bu.computeLength=function(_13){
var _14=_13.length==6,_15=0;
for(var i=0;i<_13.length-2;i+=2){
_15+=_f(_13[i],_13[i+1],_13[i+2],_13[i+3]);
}
var _16=_14?_f(_13[0],_13[1],_13[4],_13[5]):_f(_13[0],_13[1],_13[6],_13[7]);
if(_15-_16>_2){
var _17=_14?_a(_13,0.5):_18(_13,0.5);
var _19=_12(_17[0],_14);
_19+=_12(_17[1],_14);
return _19;
}
return _15;
};
var _f=bu.distance=function(x1,y1,x2,y2){
return Math.sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
};
var _a=function(_1a,t){
var r=1-t,r2=r*r,t2=t*t,p1x=_1a[0],p1y=_1a[1],cx=_1a[2],cy=_1a[3],p2x=_1a[4],p2y=_1a[5],ax=r*p1x+t*cx,ay=r*p1y+t*cy,bx=r*cx+t*p2x,by=r*cy+t*p2y,px=r2*p1x+2*r*t*cx+t2*p2x,py=r2*p1y+2*r*t*cy+t2*p2y;
return [[p1x,p1y,ax,ay,px,py],[px,py,bx,by,p2x,p2y]];
};
var _18=function(_1b,t){
var r=1-t,r2=r*r,r3=r2*r,t2=t*t,t3=t2*t,p1x=_1b[0],p1y=_1b[1],c1x=_1b[2],c1y=_1b[3],c2x=_1b[4],c2y=_1b[5],p2x=_1b[6],p2y=_1b[7],ax=r*p1x+t*c1x,ay=r*p1y+t*c1y,cx=r*c2x+t*p2x,cy=r*c2y+t*p2y,mx=r2*p1x+2*r*t*c1x+t2*c2x,my=r2*p1y+2*r*t*c1y+t2*c2y,nx=r2*c1x+2*r*t*c2x+t2*p2x,ny=r2*c1y+2*r*t*c2y+t2*p2y,px=r3*p1x+3*r2*t*c1x+3*r*t2*c2x+t3*p2x,py=r3*p1y+3*r2*t*c1y+3*r*t2*c2y+t3*p2y;
return [[p1x,p1y,ax,ay,mx,my,px,py],[px,py,nx,ny,cx,cy,p2x,p2y]];
};
var _b=bu.splitBezierAtT=function(_1c,t){
return _1c.length==6?_a(_1c,t):_18(_1c,t);
};
return bu;
});
