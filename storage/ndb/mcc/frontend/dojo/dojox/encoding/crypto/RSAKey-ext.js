//>>built
define("dojox/encoding/crypto/RSAKey-ext",["dojo/_base/kernel","dojo/_base/lang","./RSAKey","../../math/BigInteger-ext"],function(_1,_2,_3,_4){
_1.experimental("dojox.encoding.crypto.RSAKey-ext");
function _5(d,n){
var b=d.toByteArray();
for(var i=0,_6=b.length;i<_6&&!b[i];++i){
}
if(b.length-i!==n-1||b[i]!==2){
return null;
}
for(++i;b[i];){
if(++i>=_6){
return null;
}
}
var _7="";
while(++i<_6){
_7+=String.fromCharCode(b[i]);
}
return _7;
};
_2.extend(_3,{setPrivate:function(N,E,D){
if(N&&E&&N.length&&E.length){
this.n=new _4(N,16);
this.e=parseInt(E,16);
this.d=new _4(D,16);
}else{
throw new Error("Invalid RSA private key");
}
},setPrivateEx:function(N,E,D,P,Q,DP,DQ,C){
if(N&&E&&N.length&&E.length){
this.n=new _4(N,16);
this.e=parseInt(E,16);
this.d=new _4(D,16);
this.p=new _4(P,16);
this.q=new _4(Q,16);
this.dmp1=new _4(DP,16);
this.dmq1=new _4(DQ,16);
this.coeff=new _4(C,16);
}else{
throw new Error("Invalid RSA private key");
}
},generate:function(B,E){
var _8=this.rngf(),qs=B>>1;
this.e=parseInt(E,16);
var ee=new _4(E,16);
for(;;){
for(;;){
this.p=new _4(B-qs,1,_8);
if(!this.p.subtract(_4.ONE).gcd(ee).compareTo(_4.ONE)&&this.p.isProbablePrime(10)){
break;
}
}
for(;;){
this.q=new _4(qs,1,_8);
if(!this.q.subtract(_4.ONE).gcd(ee).compareTo(_4.ONE)&&this.q.isProbablePrime(10)){
break;
}
}
if(this.p.compareTo(this.q)<=0){
var t=this.p;
this.p=this.q;
this.q=t;
}
var p1=this.p.subtract(_4.ONE);
var q1=this.q.subtract(_4.ONE);
var _9=p1.multiply(q1);
if(!_9.gcd(ee).compareTo(_4.ONE)){
this.n=this.p.multiply(this.q);
this.d=ee.modInverse(_9);
this.dmp1=this.d.mod(p1);
this.dmq1=this.d.mod(q1);
this.coeff=this.q.modInverse(this.p);
break;
}
}
_8.destroy();
},decrypt:function(_a){
var c=new _4(_a,16),m;
if(!this.p||!this.q){
m=c.modPow(this.d,this.n);
}else{
var cp=c.mod(this.p).modPow(this.dmp1,this.p),cq=c.mod(this.q).modPow(this.dmq1,this.q);
while(cp.compareTo(cq)<0){
cp=cp.add(this.p);
}
m=cp.subtract(cq).multiply(this.coeff).mod(this.p).multiply(this.q).add(cq);
}
if(!m){
return null;
}
return _5(m,(this.n.bitLength()+7)>>3);
}});
return _3;
});
