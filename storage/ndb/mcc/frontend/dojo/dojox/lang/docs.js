//>>built
define("dojox/lang/docs",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.docs");
(function(){
function _4(_5){
};
var _6={};
var _7=[];
var _8=_3.lang.docs._loadedDocs={};
var _9=function(_a,_b){
_6[_b]=_a;
};
var _c=function(_d){
var _e=_d.type||"";
var _f,_10=false,_11=false,_12;
_e=_e.replace(/\?/,function(){
_10=true;
return "";
});
_e=_e.replace(/\[\]/,function(){
_11=true;
return "";
});
if(_e.match(/HTML/)){
_e="string";
}else{
if(_e=="String"||_e=="Number"||_e=="Boolean"||_e=="Object"||_e=="Array"||_e=="Integer"||_e=="Function"){
_e=_e.toLowerCase();
}else{
if(_e=="bool"){
_e="boolean";
}else{
if(_e){
_f=_1.getObject(_e)||{};
_12=true;
}else{
_f={};
}
}
}
}
_f=_f||{type:_e};
if(_11){
_f={items:_f,type:"array"};
_12=false;
}
if(!_12){
if(_10){
_f.optional=true;
}
if(/const/.test(_d.tags)){
_f.readonly=true;
}
}
return _f;
};
var _13=function(_14,_15){
var _16=_8[_15];
if(_16){
_14.description=_16.description;
_14.properties={};
_14.methods={};
if(_16.properties){
var _17=_16.properties;
for(var i=0,l=_17.length;i<l;i++){
if(_17[i].scope=="prototype"){
var _18=_14.properties[_17[i].name]=_c(_17[i]);
_18.description=_17[i].summary;
}
}
}
if(_16.methods){
var _19=_16.methods;
for(i=0,l=_19.length;i<l;i++){
_15=_19[i].name;
if(_15&&_19[i].scope=="prototype"){
var _1a=_14.methods[_15]={};
_1a.description=_19[i].summary;
var _1b=_19[i].parameters;
if(_1b){
_1a.parameters=[];
for(var j=0,k=_1b.length;j<k;j++){
var _1c=_1b[j];
var _1d=_1a.parameters[j]=_c(_1c);
_1d.name=_1c.name;
_1d.optional="optional"==_1c.usage;
}
}
var ret=_19[i]["return-types"];
if(ret&&ret[0]){
var _1e=_c(ret[0]);
if(_1e.type){
_1a.returns=_1e;
}
}
}
}
}
var _1f=_16.superclass;
if(_1f){
_14["extends"]=_1.getObject(_1f);
}
}
};
var _20=function(_21){
_7.push(_21);
};
var _22=_1.declare;
_1.declare=function(_23){
var _24=_22.apply(this,arguments);
_9(_24,_23);
return _24;
};
_1.mixin(_1.declare,_22);
var _25;
var _26=_1.require;
_1.require=function(_27){
_20(_27);
var _28=_26.apply(this,arguments);
return _28;
};
_3.lang.docs.init=function(_29){
function _2a(){
_1.require=_26;
_7=null;
try{
_1.xhrGet({sync:!_29,url:_1.baseUrl+"../util/docscripts/api.json",handleAs:"text"}).addCallbacks(function(obj){
_8=(new Function("return "+obj))();
obj=null;
_9=_13;
for(var i in _6){
_9(_6[i],i);
}
_6=null;
},_4);
}
catch(e){
_4(e);
}
};
if(_25){
return null;
}
_25=true;
var _2b=function(_2c,_2d){
return _1.xhrGet({sync:_2d||!_29,url:_1.baseUrl+"../util/docscripts/api/"+_2c+".json",handleAs:"text"}).addCallback(function(obj){
obj=(new Function("return "+obj))();
for(var _2e in obj){
if(!_8[_2e]){
_8[_2e]=obj[_2e];
}
}
});
};
try{
var _2f=_7.shift();
_2b(_2f,true).addCallbacks(function(){
_20=function(_30){
if(!_8[_30]){
try{
_2b(_30);
}
catch(e){
_8[_30]={};
}
}
};
_1.forEach(_7,function(mod){
_20(mod);
});
_7=null;
_9=_13;
for(i in _6){
_9(_6[i],i);
}
_6=null;
},_2a);
}
catch(e){
_2a();
}
return null;
};
})();
});
