//>>built
define("dojox/encoding/easy64",["dojo/_base/lang"],function(_1){
var _2=_1.getObject("dojox.encoding.easy64",true);
var c=function(_3,_4,_5){
for(var i=0;i<_4;i+=3){
_5.push(String.fromCharCode((_3[i]>>>2)+33),String.fromCharCode(((_3[i]&3)<<4)+(_3[i+1]>>>4)+33),String.fromCharCode(((_3[i+1]&15)<<2)+(_3[i+2]>>>6)+33),String.fromCharCode((_3[i+2]&63)+33));
}
};
_2.encode=function(_6){
var _7=[],_8=_6.length%3,_9=_6.length-_8;
c(_6,_9,_7);
if(_8){
var t=_6.slice(_9);
while(t.length<3){
t.push(0);
}
c(t,3,_7);
for(var i=3;i>_8;_7.pop(),--i){
}
}
return _7.join("");
};
_2.decode=function(_a){
var n=_a.length,r=[],b=[0,0,0,0],i,j,d;
for(i=0;i<n;i+=4){
for(j=0;j<4;++j){
b[j]=_a.charCodeAt(i+j)-33;
}
d=n-i;
for(j=d;j<4;b[++j]=0){
}
r.push((b[0]<<2)+(b[1]>>>4),((b[1]&15)<<4)+(b[2]>>>2),((b[2]&3)<<6)+b[3]);
for(j=d;j<4;++j,r.pop()){
}
}
return r;
};
return _2;
});
