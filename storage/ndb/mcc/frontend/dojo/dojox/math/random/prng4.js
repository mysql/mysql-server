//>>built
define("dojox/math/random/prng4",["dojo","dojox"],function(_1,_2){
_1.getObject("math.random.prng4",true,_2);
function _3(){
this.i=0;
this.j=0;
this.S=new Array(256);
};
_1.extend(_3,{init:function(_4){
var i,j,t,S=this.S,_5=_4.length;
for(i=0;i<256;++i){
S[i]=i;
}
j=0;
for(i=0;i<256;++i){
j=(j+S[i]+_4[i%_5])&255;
t=S[i];
S[i]=S[j];
S[j]=t;
}
this.i=0;
this.j=0;
},next:function(){
var t,i,j,S=this.S;
this.i=i=(this.i+1)&255;
this.j=j=(this.j+S[i])&255;
t=S[i];
S[i]=S[j];
S[j]=t;
return S[(t+S[i])&255];
}});
_2.math.random.prng4=function(){
return new _3();
};
_2.math.random.prng4.size=256;
return _2.math.random.prng4;
});
