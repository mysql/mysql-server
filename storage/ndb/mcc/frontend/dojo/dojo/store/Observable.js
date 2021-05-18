/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/store/Observable",["../_base/kernel","../_base/lang","../when","../_base/array"],function(_1,_2,_3,_4){
var _5=function(_6){
var _7,_8=[],_9=0;
_6=_2.delegate(_6);
_6.notify=function(_a,_b){
_9++;
var _c=_8.slice();
for(var i=0,l=_c.length;i<l;i++){
_c[i](_a,_b);
}
};
var _d=_6.query;
_6.query=function(_e,_f){
_f=_f||{};
var _10=_d.apply(this,arguments);
if(_10&&_10.forEach){
var _11=_2.mixin({},_f);
delete _11.start;
delete _11.count;
var _12=_6.queryEngine&&_6.queryEngine(_e,_11);
var _13=_9;
var _14=[],_15;
_10.observe=function(_16,_17){
if(_14.push(_16)==1){
_8.push(_15=function(_18,_19){
_3(_10,function(_1a){
var _1b=_1a.length!=_f.count;
var i,l,_16;
if(++_13!=_9){
throw new Error("Query is out of date, you must observe() the query prior to any data modifications");
}
var _1c,_1d=-1,_1e=-1;
if(_19!==_7){
var _1f=[].concat(_1a);
if(_12&&!_18){
_1f=_12(_1a);
}
for(i=0,l=_1a.length;i<l;i++){
var _20=_1a[i];
if(_6.getIdentity(_20)==_19){
if(_1f.indexOf(_20)<0){
continue;
}
_1c=_20;
_1d=i;
if(_12||!_18){
_1a.splice(i,1);
}
break;
}
}
}
if(_12){
if(_18&&(_12.matches?_12.matches(_18):_12([_18]).length)){
var _21=_1d>-1?_1d:_1a.length;
_1a.splice(_21,0,_18);
_1e=_4.indexOf(_12(_1a),_18);
_1a.splice(_21,1);
if((_f.start&&_1e==0)||(!_1b&&_1e==_1a.length)){
_1e=-1;
}else{
_1a.splice(_1e,0,_18);
}
}
}else{
if(_18){
if(_19!==_7){
_1e=_1d;
}else{
if(!_f.start){
_1e=_6.defaultIndex||0;
_1a.splice(_1e,0,_18);
}
}
}
}
if((_1d>-1||_1e>-1)&&(_17||!_12||(_1d!=_1e))){
var _22=_14.slice();
for(i=0;_16=_22[i];i++){
_16(_18||_1c,_1d,_1e);
}
}
});
});
}
var _23={};
_23.remove=_23.cancel=function(){
var _24=_4.indexOf(_14,_16);
if(_24>-1){
_14.splice(_24,1);
if(!_14.length){
_8.splice(_4.indexOf(_8,_15),1);
}
}
};
return _23;
};
}
return _10;
};
var _25;
function _26(_27,_28){
var _29=_6[_27];
if(_29){
_6[_27]=function(_2a){
var _2b;
if(_27==="put"){
_2b=_6.getIdentity(_2a);
}
if(_25){
return _29.apply(this,arguments);
}
_25=true;
try{
var _2c=_29.apply(this,arguments);
_3(_2c,function(_2d){
_28((typeof _2d=="object"&&_2d)||_2a,_2b);
});
return _2c;
}
finally{
_25=false;
}
};
}
};
_26("put",function(_2e,_2f){
_6.notify(_2e,_2f);
});
_26("add",function(_30){
_6.notify(_30);
});
_26("remove",function(id){
_6.notify(undefined,id);
});
return _6;
};
_2.setObject("dojo.store.Observable",_5);
return _5;
});
