//>>built
define("dojox/encoding/ascii85",["dojo/_base/lang"],function(_1){
var _2=_1.getObject("dojox.encoding.ascii85",true);
var c=function(_3,_4,_5){
var i,j,n,b=[0,0,0,0,0];
for(i=0;i<_4;i+=4){
n=((_3[i]*256+_3[i+1])*256+_3[i+2])*256+_3[i+3];
if(!n){
_5.push("z");
}else{
for(j=0;j<5;b[j++]=n%85+33,n=Math.floor(n/85)){
}
}
_5.push(String.fromCharCode(b[4],b[3],b[2],b[1],b[0]));
}
};
_2.encode=function(_6){
var _7=[],_8=_6.length%4,_9=_6.length-_8;
c(_6,_9,_7);
if(_8){
var t=_6.slice(_9);
while(t.length<4){
t.push(0);
}
c(t,4,_7);
var x=_7.pop();
if(x=="z"){
x="!!!!!";
}
_7.push(x.substr(0,_8+1));
}
return _7.join("");
};
_2.decode=function(_a){
var n=_a.length,r=[],b=[0,0,0,0,0],i,j,t,x,y,d;
for(i=0;i<n;++i){
if(_a.charAt(i)=="z"){
r.push(0,0,0,0);
continue;
}
for(j=0;j<5;++j){
b[j]=_a.charCodeAt(i+j)-33;
}
d=n-i;
if(d<5){
for(j=d;j<4;b[++j]=0){
}
b[d]=85;
}
t=(((b[0]*85+b[1])*85+b[2])*85+b[3])*85+b[4];
x=t&255;
t>>>=8;
y=t&255;
t>>>=8;
r.push(t>>>8,t&255,y,x);
for(j=d;j<5;++j,r.pop()){
}
i+=4;
}
return r;
};
return _2;
});
