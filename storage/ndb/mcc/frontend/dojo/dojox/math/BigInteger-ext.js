//>>built
define("dojox/math/BigInteger-ext",["dojo","dojox","dojox/math/BigInteger"],function(_1,_2){
_1.experimental("dojox.math.BigInteger-ext");
var _3=_2.math.BigInteger,_4=_3._nbi,_5=_3._nbv,_6=_3._nbits,_7=_3._Montgomery;
function _8(){
var r=_4();
this._copyTo(r);
return r;
};
function _9(){
if(this.s<0){
if(this.t==1){
return this[0]-this._DV;
}else{
if(this.t==0){
return -1;
}
}
}else{
if(this.t==1){
return this[0];
}else{
if(this.t==0){
return 0;
}
}
}
return ((this[1]&((1<<(32-this._DB))-1))<<this._DB)|this[0];
};
function _a(){
return (this.t==0)?this.s:(this[0]<<24)>>24;
};
function _b(){
return (this.t==0)?this.s:(this[0]<<16)>>16;
};
function _c(r){
return Math.floor(Math.LN2*this._DB/Math.log(r));
};
function _d(){
if(this.s<0){
return -1;
}else{
if(this.t<=0||(this.t==1&&this[0]<=0)){
return 0;
}else{
return 1;
}
}
};
function _e(b){
if(b==null){
b=10;
}
if(this.signum()==0||b<2||b>36){
return "0";
}
var cs=this._chunkSize(b);
var a=Math.pow(b,cs);
var d=_5(a),y=_4(),z=_4(),r="";
this._divRemTo(d,y,z);
while(y.signum()>0){
r=(a+z.intValue()).toString(b).substr(1)+r;
y._divRemTo(d,y,z);
}
return z.intValue().toString(b)+r;
};
function _f(s,b){
this._fromInt(0);
if(b==null){
b=10;
}
var cs=this._chunkSize(b);
var d=Math.pow(b,cs),mi=false,j=0,w=0;
for(var i=0;i<s.length;++i){
var x=intAt(s,i);
if(x<0){
if(s.charAt(i)=="-"&&this.signum()==0){
mi=true;
}
continue;
}
w=b*w+x;
if(++j>=cs){
this._dMultiply(d);
this._dAddOffset(w,0);
j=0;
w=0;
}
}
if(j>0){
this._dMultiply(Math.pow(b,j));
this._dAddOffset(w,0);
}
if(mi){
_3.ZERO._subTo(this,this);
}
};
function _10(a,b,c){
if("number"==typeof b){
if(a<2){
this._fromInt(1);
}else{
this._fromNumber(a,c);
if(!this.testBit(a-1)){
this._bitwiseTo(_3.ONE.shiftLeft(a-1),_11,this);
}
if(this._isEven()){
this._dAddOffset(1,0);
}
while(!this.isProbablePrime(b)){
this._dAddOffset(2,0);
if(this.bitLength()>a){
this._subTo(_3.ONE.shiftLeft(a-1),this);
}
}
}
}else{
var x=[],t=a&7;
x.length=(a>>3)+1;
b.nextBytes(x);
if(t>0){
x[0]&=((1<<t)-1);
}else{
x[0]=0;
}
this._fromString(x,256);
}
};
function _12(){
var i=this.t,r=[];
r[0]=this.s;
var p=this._DB-(i*this._DB)%8,d,k=0;
if(i-->0){
if(p<this._DB&&(d=this[i]>>p)!=(this.s&this._DM)>>p){
r[k++]=d|(this.s<<(this._DB-p));
}
while(i>=0){
if(p<8){
d=(this[i]&((1<<p)-1))<<(8-p);
d|=this[--i]>>(p+=this._DB-8);
}else{
d=(this[i]>>(p-=8))&255;
if(p<=0){
p+=this._DB;
--i;
}
}
if((d&128)!=0){
d|=-256;
}
if(k==0&&(this.s&128)!=(d&128)){
++k;
}
if(k>0||d!=this.s){
r[k++]=d;
}
}
}
return r;
};
function _13(a){
return (this.compareTo(a)==0);
};
function _14(a){
return (this.compareTo(a)<0)?this:a;
};
function _15(a){
return (this.compareTo(a)>0)?this:a;
};
function _16(a,op,r){
var i,f,m=Math.min(a.t,this.t);
for(i=0;i<m;++i){
r[i]=op(this[i],a[i]);
}
if(a.t<this.t){
f=a.s&this._DM;
for(i=m;i<this.t;++i){
r[i]=op(this[i],f);
}
r.t=this.t;
}else{
f=this.s&this._DM;
for(i=m;i<a.t;++i){
r[i]=op(f,a[i]);
}
r.t=a.t;
}
r.s=op(this.s,a.s);
r._clamp();
};
function _17(x,y){
return x&y;
};
function _18(a){
var r=_4();
this._bitwiseTo(a,_17,r);
return r;
};
function _11(x,y){
return x|y;
};
function _19(a){
var r=_4();
this._bitwiseTo(a,_11,r);
return r;
};
function _1a(x,y){
return x^y;
};
function _1b(a){
var r=_4();
this._bitwiseTo(a,_1a,r);
return r;
};
function _1c(x,y){
return x&~y;
};
function _1d(a){
var r=_4();
this._bitwiseTo(a,_1c,r);
return r;
};
function _1e(){
var r=_4();
for(var i=0;i<this.t;++i){
r[i]=this._DM&~this[i];
}
r.t=this.t;
r.s=~this.s;
return r;
};
function _1f(n){
var r=_4();
if(n<0){
this._rShiftTo(-n,r);
}else{
this._lShiftTo(n,r);
}
return r;
};
function _20(n){
var r=_4();
if(n<0){
this._lShiftTo(-n,r);
}else{
this._rShiftTo(n,r);
}
return r;
};
function _21(x){
if(x==0){
return -1;
}
var r=0;
if((x&65535)==0){
x>>=16;
r+=16;
}
if((x&255)==0){
x>>=8;
r+=8;
}
if((x&15)==0){
x>>=4;
r+=4;
}
if((x&3)==0){
x>>=2;
r+=2;
}
if((x&1)==0){
++r;
}
return r;
};
function _22(){
for(var i=0;i<this.t;++i){
if(this[i]!=0){
return i*this._DB+_21(this[i]);
}
}
if(this.s<0){
return this.t*this._DB;
}
return -1;
};
function _23(x){
var r=0;
while(x!=0){
x&=x-1;
++r;
}
return r;
};
function _24(){
var r=0,x=this.s&this._DM;
for(var i=0;i<this.t;++i){
r+=_23(this[i]^x);
}
return r;
};
function _25(n){
var j=Math.floor(n/this._DB);
if(j>=this.t){
return (this.s!=0);
}
return ((this[j]&(1<<(n%this._DB)))!=0);
};
function _26(n,op){
var r=_3.ONE.shiftLeft(n);
this._bitwiseTo(r,op,r);
return r;
};
function _27(n){
return this._changeBit(n,_11);
};
function _28(n){
return this._changeBit(n,_1c);
};
function _29(n){
return this._changeBit(n,_1a);
};
function _2a(a,r){
var i=0,c=0,m=Math.min(a.t,this.t);
while(i<m){
c+=this[i]+a[i];
r[i++]=c&this._DM;
c>>=this._DB;
}
if(a.t<this.t){
c+=a.s;
while(i<this.t){
c+=this[i];
r[i++]=c&this._DM;
c>>=this._DB;
}
c+=this.s;
}else{
c+=this.s;
while(i<a.t){
c+=a[i];
r[i++]=c&this._DM;
c>>=this._DB;
}
c+=a.s;
}
r.s=(c<0)?-1:0;
if(c>0){
r[i++]=c;
}else{
if(c<-1){
r[i++]=this._DV+c;
}
}
r.t=i;
r._clamp();
};
function _2b(a){
var r=_4();
this._addTo(a,r);
return r;
};
function _2c(a){
var r=_4();
this._subTo(a,r);
return r;
};
function _2d(a){
var r=_4();
this._multiplyTo(a,r);
return r;
};
function _2e(a){
var r=_4();
this._divRemTo(a,r,null);
return r;
};
function _2f(a){
var r=_4();
this._divRemTo(a,null,r);
return r;
};
function _30(a){
var q=_4(),r=_4();
this._divRemTo(a,q,r);
return [q,r];
};
function _31(n){
this[this.t]=this.am(0,n-1,this,0,0,this.t);
++this.t;
this._clamp();
};
function _32(n,w){
while(this.t<=w){
this[this.t++]=0;
}
this[w]+=n;
while(this[w]>=this._DV){
this[w]-=this._DV;
if(++w>=this.t){
this[this.t++]=0;
}
++this[w];
}
};
function _33(){
};
function _34(x){
return x;
};
function _35(x,y,r){
x._multiplyTo(y,r);
};
function _36(x,r){
x._squareTo(r);
};
_33.prototype.convert=_34;
_33.prototype.revert=_34;
_33.prototype.mulTo=_35;
_33.prototype.sqrTo=_36;
function _37(e){
return this._exp(e,new _33());
};
function _38(a,n,r){
var i=Math.min(this.t+a.t,n);
r.s=0;
r.t=i;
while(i>0){
r[--i]=0;
}
var j;
for(j=r.t-this.t;i<j;++i){
r[i+this.t]=this.am(0,a[i],r,i,0,this.t);
}
for(j=Math.min(a.t,n);i<j;++i){
this.am(0,a[i],r,i,0,n-i);
}
r._clamp();
};
function _39(a,n,r){
--n;
var i=r.t=this.t+a.t-n;
r.s=0;
while(--i>=0){
r[i]=0;
}
for(i=Math.max(n-this.t,0);i<a.t;++i){
r[this.t+i-n]=this.am(n-i,a[i],r,0,0,this.t+i-n);
}
r._clamp();
r._drShiftTo(1,r);
};
function _3a(m){
this.r2=_4();
this.q3=_4();
_3.ONE._dlShiftTo(2*m.t,this.r2);
this.mu=this.r2.divide(m);
this.m=m;
};
function _3b(x){
if(x.s<0||x.t>2*this.m.t){
return x.mod(this.m);
}else{
if(x.compareTo(this.m)<0){
return x;
}else{
var r=_4();
x._copyTo(r);
this.reduce(r);
return r;
}
}
};
function _3c(x){
return x;
};
function _3d(x){
x._drShiftTo(this.m.t-1,this.r2);
if(x.t>this.m.t+1){
x.t=this.m.t+1;
x._clamp();
}
this.mu._multiplyUpperTo(this.r2,this.m.t+1,this.q3);
this.m._multiplyLowerTo(this.q3,this.m.t+1,this.r2);
while(x.compareTo(this.r2)<0){
x._dAddOffset(1,this.m.t+1);
}
x._subTo(this.r2,x);
while(x.compareTo(this.m)>=0){
x._subTo(this.m,x);
}
};
function _3e(x,r){
x._squareTo(r);
this.reduce(r);
};
function _3f(x,y,r){
x._multiplyTo(y,r);
this.reduce(r);
};
_3a.prototype.convert=_3b;
_3a.prototype.revert=_3c;
_3a.prototype.reduce=_3d;
_3a.prototype.mulTo=_3f;
_3a.prototype.sqrTo=_3e;
function _40(e,m){
var i=e.bitLength(),k,r=_5(1),z;
if(i<=0){
return r;
}else{
if(i<18){
k=1;
}else{
if(i<48){
k=3;
}else{
if(i<144){
k=4;
}else{
if(i<768){
k=5;
}else{
k=6;
}
}
}
}
}
if(i<8){
z=new Classic(m);
}else{
if(m._isEven()){
z=new _3a(m);
}else{
z=new _7(m);
}
}
var g=[],n=3,k1=k-1,km=(1<<k)-1;
g[1]=z.convert(this);
if(k>1){
var g2=_4();
z.sqrTo(g[1],g2);
while(n<=km){
g[n]=_4();
z.mulTo(g2,g[n-2],g[n]);
n+=2;
}
}
var j=e.t-1,w,is1=true,r2=_4(),t;
i=_6(e[j])-1;
while(j>=0){
if(i>=k1){
w=(e[j]>>(i-k1))&km;
}else{
w=(e[j]&((1<<(i+1))-1))<<(k1-i);
if(j>0){
w|=e[j-1]>>(this._DB+i-k1);
}
}
n=k;
while((w&1)==0){
w>>=1;
--n;
}
if((i-=n)<0){
i+=this._DB;
--j;
}
if(is1){
g[w]._copyTo(r);
is1=false;
}else{
while(n>1){
z.sqrTo(r,r2);
z.sqrTo(r2,r);
n-=2;
}
if(n>0){
z.sqrTo(r,r2);
}else{
t=r;
r=r2;
r2=t;
}
z.mulTo(r2,g[w],r);
}
while(j>=0&&(e[j]&(1<<i))==0){
z.sqrTo(r,r2);
t=r;
r=r2;
r2=t;
if(--i<0){
i=this._DB-1;
--j;
}
}
}
return z.revert(r);
};
function _41(a){
var x=(this.s<0)?this.negate():this.clone();
var y=(a.s<0)?a.negate():a.clone();
if(x.compareTo(y)<0){
var t=x;
x=y;
y=t;
}
var i=x.getLowestSetBit(),g=y.getLowestSetBit();
if(g<0){
return x;
}
if(i<g){
g=i;
}
if(g>0){
x._rShiftTo(g,x);
y._rShiftTo(g,y);
}
while(x.signum()>0){
if((i=x.getLowestSetBit())>0){
x._rShiftTo(i,x);
}
if((i=y.getLowestSetBit())>0){
y._rShiftTo(i,y);
}
if(x.compareTo(y)>=0){
x._subTo(y,x);
x._rShiftTo(1,x);
}else{
y._subTo(x,y);
y._rShiftTo(1,y);
}
}
if(g>0){
y._lShiftTo(g,y);
}
return y;
};
function _42(n){
if(n<=0){
return 0;
}
var d=this._DV%n,r=(this.s<0)?n-1:0;
if(this.t>0){
if(d==0){
r=this[0]%n;
}else{
for(var i=this.t-1;i>=0;--i){
r=(d*r+this[i])%n;
}
}
}
return r;
};
function _43(m){
var ac=m._isEven();
if((this._isEven()&&ac)||m.signum()==0){
return _3.ZERO;
}
var u=m.clone(),v=this.clone();
var a=_5(1),b=_5(0),c=_5(0),d=_5(1);
while(u.signum()!=0){
while(u._isEven()){
u._rShiftTo(1,u);
if(ac){
if(!a._isEven()||!b._isEven()){
a._addTo(this,a);
b._subTo(m,b);
}
a._rShiftTo(1,a);
}else{
if(!b._isEven()){
b._subTo(m,b);
}
}
b._rShiftTo(1,b);
}
while(v._isEven()){
v._rShiftTo(1,v);
if(ac){
if(!c._isEven()||!d._isEven()){
c._addTo(this,c);
d._subTo(m,d);
}
c._rShiftTo(1,c);
}else{
if(!d._isEven()){
d._subTo(m,d);
}
}
d._rShiftTo(1,d);
}
if(u.compareTo(v)>=0){
u._subTo(v,u);
if(ac){
a._subTo(c,a);
}
b._subTo(d,b);
}else{
v._subTo(u,v);
if(ac){
c._subTo(a,c);
}
d._subTo(b,d);
}
}
if(v.compareTo(_3.ONE)!=0){
return _3.ZERO;
}
if(d.compareTo(m)>=0){
return d.subtract(m);
}
if(d.signum()<0){
d._addTo(m,d);
}else{
return d;
}
if(d.signum()<0){
return d.add(m);
}else{
return d;
}
};
var _44=[2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317,331,337,347,349,353,359,367,373,379,383,389,397,401,409,419,421,431,433,439,443,449,457,461,463,467,479,487,491,499,503,509];
var _45=(1<<26)/_44[_44.length-1];
function _46(t){
var i,x=this.abs();
if(x.t==1&&x[0]<=_44[_44.length-1]){
for(i=0;i<_44.length;++i){
if(x[0]==_44[i]){
return true;
}
}
return false;
}
if(x._isEven()){
return false;
}
i=1;
while(i<_44.length){
var m=_44[i],j=i+1;
while(j<_44.length&&m<_45){
m*=_44[j++];
}
m=x._modInt(m);
while(i<j){
if(m%_44[i++]==0){
return false;
}
}
}
return x._millerRabin(t);
};
function _47(t){
var n1=this.subtract(_3.ONE);
var k=n1.getLowestSetBit();
if(k<=0){
return false;
}
var r=n1.shiftRight(k);
t=(t+1)>>1;
if(t>_44.length){
t=_44.length;
}
var a=_4();
for(var i=0;i<t;++i){
a._fromInt(_44[i]);
var y=a.modPow(r,this);
if(y.compareTo(_3.ONE)!=0&&y.compareTo(n1)!=0){
var j=1;
while(j++<k&&y.compareTo(n1)!=0){
y=y.modPowInt(2,this);
if(y.compareTo(_3.ONE)==0){
return false;
}
}
if(y.compareTo(n1)!=0){
return false;
}
}
}
return true;
};
_1.extend(_3,{_chunkSize:_c,_toRadix:_e,_fromRadix:_f,_fromNumber:_10,_bitwiseTo:_16,_changeBit:_26,_addTo:_2a,_dMultiply:_31,_dAddOffset:_32,_multiplyLowerTo:_38,_multiplyUpperTo:_39,_modInt:_42,_millerRabin:_47,clone:_8,intValue:_9,byteValue:_a,shortValue:_b,signum:_d,toByteArray:_12,equals:_13,min:_14,max:_15,and:_18,or:_19,xor:_1b,andNot:_1d,not:_1e,shiftLeft:_1f,shiftRight:_20,getLowestSetBit:_22,bitCount:_24,testBit:_25,setBit:_27,clearBit:_28,flipBit:_29,add:_2b,subtract:_2c,multiply:_2d,divide:_2e,remainder:_2f,divideAndRemainder:_30,modPow:_40,modInverse:_43,pow:_37,gcd:_41,isProbablePrime:_46});
return _2.math.BigInteger;
});
