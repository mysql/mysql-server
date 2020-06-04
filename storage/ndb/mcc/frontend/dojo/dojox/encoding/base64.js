//>>built
define("dojox/encoding/base64",["dojo/_base/lang"],function(_1){
var _2=_1.getObject("dojox.encoding.base64",true);
var p="=";
var _3="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
_2.encode=function(ba){
var s=[],l=ba.length;
var rm=l%3;
var x=l-rm;
for(var i=0;i<x;){
var t=ba[i++]<<16|ba[i++]<<8|ba[i++];
s.push(_3.charAt((t>>>18)&63));
s.push(_3.charAt((t>>>12)&63));
s.push(_3.charAt((t>>>6)&63));
s.push(_3.charAt(t&63));
}
switch(rm){
case 2:
var t=ba[i++]<<16|ba[i++]<<8;
s.push(_3.charAt((t>>>18)&63));
s.push(_3.charAt((t>>>12)&63));
s.push(_3.charAt((t>>>6)&63));
s.push(p);
break;
case 1:
var t=ba[i++]<<16;
s.push(_3.charAt((t>>>18)&63));
s.push(_3.charAt((t>>>12)&63));
s.push(p);
s.push(p);
break;
}
return s.join("");
};
_2.decode=function(_4){
var s=_4.split(""),_5=[];
var l=s.length;
while(s[--l]==p){
}
for(var i=0;i<l;){
var t=_3.indexOf(s[i++])<<18;
if(i<=l){
t|=_3.indexOf(s[i++])<<12;
}
if(i<=l){
t|=_3.indexOf(s[i++])<<6;
}
if(i<=l){
t|=_3.indexOf(s[i++]);
}
_5.push((t>>>16)&255);
_5.push((t>>>8)&255);
_5.push(t&255);
}
while(_5[_5.length-1]==0){
_5.pop();
}
return _5;
};
return _2;
});
