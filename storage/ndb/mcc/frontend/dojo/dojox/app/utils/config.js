//>>built
define("dojox/app/utils/config",["dojo/sniff"],function(_1){
return {configProcessHas:function(_2){
for(var _3 in _2){
var _4=_2[_3];
if(_3=="has"){
for(var _5 in _4){
if(!(_5.charAt(0)=="_"&&_5.charAt(1)=="_")&&_4&&typeof _4==="object"){
var _6=_5.split(",");
if(_6.length>0){
while(_6.length>0){
var _7=_6.shift();
if((_1(_7))||(_7.charAt(0)=="!"&&!(_1(_7.substring(1))))){
var _8=_4[_5];
this.configMerge(_2,_8);
break;
}
}
}
}
}
delete _2["has"];
}else{
if(!(_3.charAt(0)=="_"&&_3.charAt(1)=="_")&&_4&&typeof _4==="object"){
this.configProcessHas(_4);
}
}
}
return _2;
},configMerge:function(_9,_a){
for(var _b in _a){
var _c=_9[_b];
var _d=_a[_b];
if(_c!==_d&&!(_b.charAt(0)=="_"&&_b.charAt(1)=="_")){
if(_c&&typeof _c==="object"&&_d&&typeof _d==="object"){
this.configMerge(_c,_d);
}else{
if(_9 instanceof Array){
_9.push(_d);
}else{
_9[_b]=_d;
}
}
}
}
return _9;
}};
});
