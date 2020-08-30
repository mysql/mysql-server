//>>built
define("dojox/app/utils/constraints",["dojo/_base/array"],function(_1){
var _2=[];
return {getSelectedChild:function(_3,_4){
var _5=typeof (_4);
var _6=(_5=="string"||_5=="number")?_4:_4.__hash;
return (_3&&_3.selectedChildren&&_3.selectedChildren[_6])?_3.selectedChildren[_6]:null;
},setSelectedChild:function(_7,_8,_9){
var _a=typeof (_8);
var _b=(_a=="string"||_a=="number")?_8:_8.__hash;
_7.selectedChildren[_b]=_9;
},getAllSelectedChildren:function(_c,_d){
_d=_d||[];
if(_c&&_c.selectedChildren){
for(var _e in _c.selectedChildren){
if(_c.selectedChildren[_e]){
var _f=_c.selectedChildren[_e];
_d.push(_f);
this.getAllSelectedChildren(_f,_d);
}
}
}
return _d;
},register:function(_10){
var _11=typeof (_10);
if(!_10.__hash&&_11!="string"&&_11!="number"){
var _12=null;
_1.some(_2,function(_13){
var ok=true;
for(var _14 in _13){
if(_14.charAt(0)!=="_"){
if(_13[_14]!=_10[_14]){
ok=false;
break;
}
}
}
if(ok==true){
_12=_13;
}
return ok;
});
if(_12){
_10.__hash=_12.__hash;
}else{
var _15="";
for(var _16 in _10){
if(_16.charAt(0)!=="_"){
_15+=_10[_16];
}
}
_10.__hash=_15;
_2.push(_10);
}
}
}};
});
