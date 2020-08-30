//>>built
define("dojox/encoding/crypto/RSAKey",["dojo/_base/kernel","dojo/_base/declare","../../math/BigInteger","../../math/random/Simple"],function(_1,_2,_3,_4){
_1.experimental("dojox.encoding.crypto.RSAKey");
var _5=function(){
return new _4();
};
function _6(s,n,_7){
if(n<s.length+11){
throw new Error("Message too long for RSA");
}
var ba=new Array(n);
var i=s.length;
while(i&&n){
ba[--n]=s.charCodeAt(--i);
}
ba[--n]=0;
var _8=_7();
var x=[0];
while(n>2){
x[0]=0;
while(x[0]==0){
_8.nextBytes(x);
}
ba[--n]=x[0];
}
ba[--n]=2;
ba[--n]=0;
_8.destroy();
return new _3(ba);
};
return _2("dojox.encoding.crypto.RSAKey",null,{constructor:function(_9){
this.rngf=_9||_5;
this.e=0;
this.n=this.d=this.p=this.q=this.dmp1=this.dmq1=this.coeff=null;
},setPublic:function(N,E){
if(N&&E&&N.length&&E.length){
this.n=new _3(N,16);
this.e=parseInt(E,16);
}else{
throw new Error("Invalid RSA public key");
}
},encrypt:function(_a){
var m=_6(_a,(this.n.bitLength()+7)>>3,this.rngf);
if(!m){
return null;
}
var c=m.modPowInt(this.e,this.n);
if(!c){
return null;
}
var h=c.toString(16);
return h.length%2?"0"+h:h;
}});
});
