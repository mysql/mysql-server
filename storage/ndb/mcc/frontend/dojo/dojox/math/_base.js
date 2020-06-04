//>>built
define("dojox/math/_base",["dojo","dojox"],function(_1,_2){
_1.getObject("math",true,_2);
var m=_2.math;
_1.mixin(_2.math,{toRadians:function(n){
return (n*Math.PI)/180;
},toDegrees:function(n){
return (n*180)/Math.PI;
},degreesToRadians:function(n){
return m.toRadians(n);
},radiansToDegrees:function(n){
return m.toDegrees(n);
},_gamma:function(z){
var _3=1;
while(--z>=1){
_3*=z;
}
if(z==0){
return _3;
}
if(Math.floor(z)==z){
return NaN;
}
if(z==-0.5){
return Math.sqrt(Math.PI);
}
if(z<-0.5){
return Math.PI/(Math.sin(Math.PI*(z+1))*this._gamma(-z));
}
var a=13;
var c=[0.000005665805601518633,1.274371766337968,-4.937419909315511,7.872026703248596,-6.676050374943609,3.252529844448517,-0.9185252144102627,0.14474022977730785,-0.011627561382389852,0.0004011798075706662,-0.0000042652458386405745,6.665191329033609e-9,-1.5392547381874824e-13];
var _4=c[0];
for(var k=1;k<a;k++){
_4+=c[k]/(z+k);
}
return _3*Math.pow(z+a,z+0.5)/Math.exp(z)*_4;
},factorial:function(n){
return this._gamma(n+1);
},permutations:function(n,k){
if(n==0||k==0){
return 1;
}
return this.factorial(n)/this.factorial(n-k);
},combinations:function(n,r){
if(n==0||r==0){
return 1;
}
return this.factorial(n)/(this.factorial(n-r)*this.factorial(r));
},bernstein:function(t,n,i){
return this.combinations(n,i)*Math.pow(t,i)*Math.pow(1-t,n-i);
},gaussian:function(){
var k=2;
do{
var i=2*Math.random()-1;
var j=2*Math.random()-1;
k=i*i+j*j;
}while(k>=1);
return i*Math.sqrt((-2*Math.log(k))/k);
},range:function(a,b,_5){
if(arguments.length<2){
b=a,a=0;
}
var _6=[],s=_5||1,i;
if(s>0){
for(i=a;i<b;i+=s){
_6.push(i);
}
}else{
if(s<0){
for(i=a;i>b;i+=s){
_6.push(i);
}
}else{
throw new Error("dojox.math.range: step must not be zero.");
}
}
return _6;
},distance:function(a,b){
return Math.sqrt(Math.pow(b[0]-a[0],2)+Math.pow(b[1]-a[1],2));
},midpoint:function(a,b){
if(a.length!=b.length){
console.error("dojox.math.midpoint: Points A and B are not the same dimensionally.",a,b);
}
var m=[];
for(var i=0;i<a.length;i++){
m[i]=(a[i]+b[i])/2;
}
return m;
}});
return _2.math;
});
