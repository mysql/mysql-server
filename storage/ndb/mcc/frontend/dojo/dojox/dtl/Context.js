//>>built
define("dojox/dtl/Context",["dojo/_base/lang","./_base"],function(_1,dd){
dd.Context=_1.extend(function(_2){
this._this={};
dd._Context.call(this,_2);
},dd._Context.prototype,{getKeys:function(){
var _3=[];
for(var _4 in this){
if(this.hasOwnProperty(_4)&&_4!="_this"){
_3.push(_4);
}
}
return _3;
},extend:function(_5){
return _1.delegate(this,_5);
},filter:function(_6){
var _7=new dd.Context();
var _8=[];
var i,_9;
if(_6 instanceof dd.Context){
_8=_6.getKeys();
}else{
if(typeof _6=="object"){
for(var _a in _6){
_8.push(_a);
}
}else{
for(i=0;_9=arguments[i];i++){
if(typeof _9=="string"){
_8.push(_9);
}
}
}
}
for(i=0,_a;_a=_8[i];i++){
_7[_a]=this[_a];
}
return _7;
},setThis:function(_b){
this._this=_b;
},getThis:function(){
return this._this;
},hasKey:function(_c){
if(this._getter){
var _d=this._getter(_c);
if(typeof _d!="undefined"){
return true;
}
}
if(typeof this[_c]!="undefined"){
return true;
}
return false;
}});
return dd.Context;
});
