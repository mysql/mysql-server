//>>built
define("dojox/uuid/generateRandomUuid",["./_base"],function(){
dojox.uuid.generateRandomUuid=function(){
var _1=16;
function _2(){
var _3=Math.floor((Math.random()%1)*Math.pow(2,32));
var _4=_3.toString(_1);
while(_4.length<8){
_4="0"+_4;
}
return _4;
};
var _5="-";
var _6="4";
var _7="8";
var a=_2();
var b=_2();
b=b.substring(0,4)+_5+_6+b.substring(5,8);
var c=_2();
c=_7+c.substring(1,4)+_5+c.substring(4,8);
var d=_2();
var _8=a+_5+b+_5+c+d;
_8=_8.toLowerCase();
return _8;
};
return dojox.uuid.generateRandomUuid;
});
