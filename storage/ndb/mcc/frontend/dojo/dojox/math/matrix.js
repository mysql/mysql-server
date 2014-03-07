//>>built
define("dojox/math/matrix",["dojo","dojox"],function(_1,_2){
_1.getObject("math.matrix",true,_2);
_1.mixin(_2.math.matrix,{iDF:0,ALMOST_ZERO:1e-10,multiply:function(a,b){
var ay=a.length,ax=a[0].length,by=b.length,bx=b[0].length;
if(ax!=by){
console.warn("Can't multiply matricies of sizes "+ax+","+ay+" and "+bx+","+by);
return [[0]];
}
var c=[];
for(var k=0;k<ay;k++){
c[k]=[];
for(var i=0;i<bx;i++){
c[k][i]=0;
for(var m=0;m<ax;m++){
c[k][i]+=a[k][m]*b[m][i];
}
}
}
return c;
},product:function(){
if(arguments.length==0){
console.warn("can't multiply 0 matrices!");
return 1;
}
var m=arguments[0];
for(var i=1;i<arguments.length;i++){
m=this.multiply(m,arguments[i]);
}
return m;
},sum:function(){
if(arguments.length==0){
console.warn("can't sum 0 matrices!");
return 0;
}
var m=this.copy(arguments[0]);
var _3=m.length;
if(_3==0){
console.warn("can't deal with matrices of 0 rows!");
return 0;
}
var _4=m[0].length;
if(_4==0){
console.warn("can't deal with matrices of 0 cols!");
return 0;
}
for(var i=1;i<arguments.length;++i){
var _5=arguments[i];
if(_5.length!=_3||_5[0].length!=_4){
console.warn("can't add matrices of different dimensions: first dimensions were "+_3+"x"+_4+", current dimensions are "+_5.length+"x"+_5[0].length);
return 0;
}
for(var r=0;r<_3;r++){
for(var c=0;c<_4;c++){
m[r][c]+=_5[r][c];
}
}
}
return m;
},inverse:function(a){
if(a.length==1&&a[0].length==1){
return [[1/a[0][0]]];
}
var _6=a.length,m=this.create(_6,_6),mm=this.adjoint(a),_7=this.determinant(a),dd=0;
if(_7==0){
console.warn("Determinant Equals 0, Not Invertible.");
return [[0]];
}else{
dd=1/_7;
}
for(var i=0;i<_6;i++){
for(var j=0;j<_6;j++){
m[i][j]=dd*mm[i][j];
}
}
return m;
},determinant:function(a){
if(a.length!=a[0].length){
console.warn("Can't calculate the determinant of a non-squre matrix!");
return 0;
}
var _8=a.length,_9=1,b=this.upperTriangle(a);
for(var i=0;i<_8;i++){
var _a=b[i][i];
if(Math.abs(_a)<this.ALMOST_ZERO){
return 0;
}
_9*=_a;
}
_9*=this.iDF;
return _9;
},upperTriangle:function(m){
m=this.copy(m);
var f1=0,_b=0,_c=m.length,v=1;
this.iDF=1;
for(var _d=0;_d<_c-1;_d++){
if(typeof m[_d][_d]!="number"){
console.warn("non-numeric entry found in a numeric matrix: m["+_d+"]["+_d+"]="+m[_d][_d]);
}
v=1;
var _e=0;
while((m[_d][_d]==0)&&!_e){
if(_d+v>=_c){
this.iDF=0;
_e=1;
}else{
for(var r=0;r<_c;r++){
_b=m[_d][r];
m[_d][r]=m[_d+v][r];
m[_d+v][r]=_b;
}
v++;
this.iDF*=-1;
}
}
for(var _f=_d+1;_f<_c;_f++){
if(typeof m[_f][_d]!="number"){
console.warn("non-numeric entry found in a numeric matrix: m["+_f+"]["+_d+"]="+m[_f][_d]);
}
if(typeof m[_d][_f]!="number"){
console.warn("non-numeric entry found in a numeric matrix: m["+_d+"]["+_f+"]="+m[_d][_f]);
}
if(m[_d][_d]!=0){
var f1=(-1)*m[_f][_d]/m[_d][_d];
for(var i=_d;i<_c;i++){
m[_f][i]=f1*m[_d][i]+m[_f][i];
}
}
}
}
return m;
},create:function(a,b,_10){
_10=_10||0;
var m=[];
for(var i=0;i<b;i++){
m[i]=[];
for(var j=0;j<a;j++){
m[i][j]=_10;
}
}
return m;
},ones:function(a,b){
return this.create(a,b,1);
},zeros:function(a,b){
return this.create(a,b);
},identity:function(_11,_12){
_12=_12||1;
var m=[];
for(var i=0;i<_11;i++){
m[i]=[];
for(var j=0;j<_11;j++){
m[i][j]=(i==j?_12:0);
}
}
return m;
},adjoint:function(a){
var tms=a.length;
if(tms<=1){
console.warn("Can't find the adjoint of a matrix with a dimension less than 2");
return [[0]];
}
if(a.length!=a[0].length){
console.warn("Can't find the adjoint of a non-square matrix");
return [[0]];
}
var m=this.create(tms,tms),ap=this.create(tms-1,tms-1);
var ii=0,jj=0,ia=0,ja=0,det=0;
for(var i=0;i<tms;i++){
for(var j=0;j<tms;j++){
ia=0;
for(ii=0;ii<tms;ii++){
if(ii==i){
continue;
}
ja=0;
for(jj=0;jj<tms;jj++){
if(jj==j){
continue;
}
ap[ia][ja]=a[ii][jj];
ja++;
}
ia++;
}
det=this.determinant(ap);
m[i][j]=Math.pow(-1,(i+j))*det;
}
}
return this.transpose(m);
},transpose:function(a){
var m=this.create(a.length,a[0].length);
for(var i=0;i<a.length;i++){
for(var j=0;j<a[i].length;j++){
m[j][i]=a[i][j];
}
}
return m;
},format:function(a,_13){
_13=_13||5;
function _14(x,dp){
var fac=Math.pow(10,dp);
var a=Math.round(x*fac)/fac;
var b=a.toString();
if(b.charAt(0)!="-"){
b=" "+b;
}
if(b.indexOf(".")>-1){
b+=".";
}
while(b.length<dp+3){
b+="0";
}
return b;
};
var ya=a.length;
var xa=ya>0?a[0].length:0;
var _15="";
for(var y=0;y<ya;y++){
_15+="| ";
for(var x=0;x<xa;x++){
_15+=_14(a[y][x],_13)+" ";
}
_15+="|\n";
}
return _15;
},copy:function(a){
var ya=a.length,xa=a[0].length,m=this.create(xa,ya);
for(var y=0;y<ya;y++){
for(var x=0;x<xa;x++){
m[y][x]=a[y][x];
}
}
return m;
},scale:function(a,_16){
a=this.copy(a);
var ya=a.length,xa=a[0].length;
for(var y=0;y<ya;y++){
for(var x=0;x<xa;x++){
a[y][x]*=_16;
}
}
return a;
}});
return _2.math.matrix;
});
