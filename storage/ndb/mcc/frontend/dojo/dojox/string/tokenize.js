//>>built
define("dojox/string/tokenize",["dojo/_base/lang","dojo/_base/sniff"],function(_1,_2){
var _3=_1.getObject("dojox.string",true).tokenize;
_3=function(_4,re,_5,_6){
var _7=[];
var _8,_9,_a=0;
while(_8=re.exec(_4)){
_9=_4.slice(_a,re.lastIndex-_8[0].length);
if(_9.length){
_7.push(_9);
}
if(_5){
if(_2("opera")){
var _b=_8.slice(0);
while(_b.length<_8.length){
_b.push(null);
}
_8=_b;
}
var _c=_5.apply(_6,_8.slice(1).concat(_7.length));
if(typeof _c!="undefined"){
_7.push(_c);
}
}
_a=re.lastIndex;
}
_9=_4.slice(_a);
if(_9.length){
_7.push(_9);
}
return _7;
};
return _3;
});
