//>>built
define("dojox/math/curves",["dojo","dojox"],function(_1,_2){
_1.getObject("math.curves",true,_2);
_1.mixin(_2.math.curves,{Line:function(_3,_4){
this.start=_3;
this.end=_4;
this.dimensions=_3.length;
for(var i=0;i<_3.length;i++){
_3[i]=Number(_3[i]);
}
for(var i=0;i<_4.length;i++){
_4[i]=Number(_4[i]);
}
this.getValue=function(n){
var _5=new Array(this.dimensions);
for(var i=0;i<this.dimensions;i++){
_5[i]=((this.end[i]-this.start[i])*n)+this.start[i];
}
return _5;
};
return this;
},Bezier:function(_6){
this.getValue=function(_7){
if(_7>=1){
return this.p[this.p.length-1];
}
if(_7<=0){
return this.p[0];
}
var _8=new Array(this.p[0].length);
for(var k=0;j<this.p[0].length;k++){
_8[k]=0;
}
for(var j=0;j<this.p[0].length;j++){
var C=0;
var D=0;
for(var i=0;i<this.p.length;i++){
C+=this.p[i][j]*this.p[this.p.length-1][0]*_2.math.bernstein(_7,this.p.length,i);
}
for(var l=0;l<this.p.length;l++){
D+=this.p[this.p.length-1][0]*_2.math.bernstein(_7,this.p.length,l);
}
_8[j]=C/D;
}
return _8;
};
this.p=_6;
return this;
},CatmullRom:function(_9,c){
this.getValue=function(_a){
var _b=_a*(this.p.length-1);
var _c=Math.floor(_b);
var _d=_b-_c;
var i0=_c-1;
if(i0<0){
i0=0;
}
var i=_c;
var i1=_c+1;
if(i1>=this.p.length){
i1=this.p.length-1;
}
var i2=_c+2;
if(i2>=this.p.length){
i2=this.p.length-1;
}
var u=_d;
var u2=_d*_d;
var u3=_d*_d*_d;
var _e=new Array(this.p[0].length);
for(var k=0;k<this.p[0].length;k++){
var x1=(-this.c*this.p[i0][k])+((2-this.c)*this.p[i][k])+((this.c-2)*this.p[i1][k])+(this.c*this.p[i2][k]);
var x2=(2*this.c*this.p[i0][k])+((this.c-3)*this.p[i][k])+((3-2*this.c)*this.p[i1][k])+(-this.c*this.p[i2][k]);
var x3=(-this.c*this.p[i0][k])+(this.c*this.p[i1][k]);
var x4=this.p[i][k];
_e[k]=x1*u3+x2*u2+x3*u+x4;
}
return _e;
};
if(!c){
this.c=0.7;
}else{
this.c=c;
}
this.p=_9;
return this;
},Arc:function(_f,end,ccw){
function _10(a,b){
var c=new Array(a.length);
for(var i=0;i<a.length;i++){
c[i]=a[i]+b[i];
}
return c;
};
function _11(a){
var b=new Array(a.length);
for(var i=0;i<a.length;i++){
b[i]=-a[i];
}
return b;
};
var _12=_2.math.midpoint(_f,end);
var _13=_10(_11(_12),_f);
var rad=Math.sqrt(Math.pow(_13[0],2)+Math.pow(_13[1],2));
var _14=_2.math.radiansToDegrees(Math.atan(_13[1]/_13[0]));
if(_13[0]<0){
_14-=90;
}else{
_14+=90;
}
_2.math.curves.CenteredArc.call(this,_12,rad,_14,_14+(ccw?-180:180));
},CenteredArc:function(_15,_16,_17,end){
this.center=_15;
this.radius=_16;
this.start=_17||0;
this.end=end;
this.getValue=function(n){
var _18=new Array(2);
var _19=_2.math.degreesToRadians(this.start+((this.end-this.start)*n));
_18[0]=this.center[0]+this.radius*Math.sin(_19);
_18[1]=this.center[1]-this.radius*Math.cos(_19);
return _18;
};
return this;
},Circle:function(_1a,_1b){
_2.math.curves.CenteredArc.call(this,_1a,_1b,0,360);
return this;
},Path:function(){
var _1c=[];
var _1d=[];
var _1e=[];
var _1f=0;
this.add=function(_20,_21){
if(_21<0){
console.error("dojox.math.curves.Path.add: weight cannot be less than 0");
}
_1c.push(_20);
_1d.push(_21);
_1f+=_21;
_22();
};
this.remove=function(_23){
for(var i=0;i<_1c.length;i++){
if(_1c[i]==_23){
_1c.splice(i,1);
_1f-=_1d.splice(i,1)[0];
break;
}
}
_22();
};
this.removeAll=function(){
_1c=[];
_1d=[];
_1f=0;
};
this.getValue=function(n){
var _24=false,_25=0;
for(var i=0;i<_1e.length;i++){
var r=_1e[i];
if(n>=r[0]&&n<r[1]){
var _26=(n-r[0])/r[2];
_25=_1c[i].getValue(_26);
_24=true;
break;
}
}
if(!_24){
_25=_1c[_1c.length-1].getValue(1);
}
for(var j=0;j<i;j++){
_25=_2.math.points.translate(_25,_1c[j].getValue(1));
}
return _25;
};
function _22(){
var _27=0;
for(var i=0;i<_1d.length;i++){
var end=_27+_1d[i]/_1f;
var len=end-_27;
_1e[i]=[_27,end,len];
_27=end;
}
};
return this;
}});
return _2.math.curves;
});
