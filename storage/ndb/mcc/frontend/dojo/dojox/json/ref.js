//>>built
define("dojox/json/ref",["dojo/_base/array","dojo/_base/json","dojo/_base/kernel","dojo/_base/lang","dojo/date/stamp","dojox"],function(_1,_2,_3,_4,_5,_6){
_4.getObject("json",true,_6);
return _6.json.ref={resolveJson:function(_7,_8){
_8=_8||{};
var _9=_8.idAttribute||"id";
var _a=this.refAttribute;
var _b=_8.idAsRef;
var _c=_8.idPrefix||"";
var _d=_8.assignAbsoluteIds;
var _e=_8.index||{};
var _f=_8.timeStamps;
var ref,_10=[];
var _11=/^(.*\/)?(\w+:\/\/)|[^\/\.]+\/\.\.\/|^.*\/(\/)/;
var _12=this._addProp;
var F=function(){
};
function _13(it,_14,_15,_16,_17,_18,_19){
var i,_1a,val,id=_9 in it?it[_9]:_15;
if(_9 in it||((id!==undefined)&&_16)){
id=(_c+id).replace(_11,"$2$3");
}
var _1b=_18||it;
if(id!==undefined){
if(_d){
it.__id=id;
}
if(_8.schemas&&(!(it instanceof Array))&&(val=id.match(/^(.+\/)[^\.\[]*$/))){
_17=_8.schemas[val[1]];
}
if(_e[id]&&((it instanceof Array)==(_e[id] instanceof Array))){
_1b=_e[id];
delete _1b.$ref;
delete _1b._loadObject;
_1a=true;
}else{
var _1c=_17&&_17.prototype;
if(_1c){
F.prototype=_1c;
_1b=new F();
}
}
_e[id]=_1b;
if(_f){
_f[id]=_8.time;
}
}
while(_17){
var _1d=_17.properties;
if(_1d){
for(i in it){
var _1e=_1d[i];
if(_1e&&_1e.format=="date-time"&&typeof it[i]=="string"){
it[i]=_5.fromISOString(it[i]);
}
}
}
_17=_17["extends"];
}
var _1f=it.length;
for(i in it){
if(i==_1f){
break;
}
if(it.hasOwnProperty(i)){
val=it[i];
if((typeof val=="object")&&val&&!(val instanceof Date)&&i!="__parent"){
ref=val[_a]||(_b&&val[_9]);
if(it!=_10&&(!ref||!val.__parent)){
val.__parent=_19?_19:_1b;
}
if(ref){
delete it[i];
var _20=ref.toString().replace(/(#)([^\.\[])/,"$1.$2").match(/(^([^\[]*\/)?[^#\.\[]*)#?([\.\[].*)?/);
if(_e[(_c+ref).replace(_11,"$2$3")]){
ref=_e[(_c+ref).replace(_11,"$2$3")];
}else{
if((ref=(_20[1]=="$"||_20[1]=="this"||_20[1]==="")?_7:_e[(_c+_20[1]).replace(_11,"$2$3")])){
if(_20[3]){
_20[3].replace(/(\[([^\]]+)\])|(\.?([^\.\[]+))/g,function(t,a,b,c,d){
ref=ref&&ref[b?b.replace(/[\"\'\\]/,""):d];
});
}
}
}
if(ref){
val=ref;
}else{
if(!_14){
var _21;
if(!_21){
_10.push(_1b);
}
_21=true;
val=_13(val,false,val[_a],true,_1e);
val._loadObject=_8.loader;
}
}
}else{
if(!_14){
val=_13(val,_10==it,id===undefined?undefined:_12(id,i),false,_1e,_1b!=it&&typeof _1b[i]=="object"&&_1b[i],it);
}
}
}
it[i]=val;
if(_1b!=it&&!_1b.__isDirty){
var old=_1b[i];
_1b[i]=val;
if(_1a&&val!==old&&!_1b._loadObject&&!(i.charAt(0)=="_"&&i.charAt(1)=="_")&&i!="$ref"&&!(val instanceof Date&&old instanceof Date&&val.getTime()==old.getTime())&&!(typeof val=="function"&&typeof old=="function"&&val.toString()==old.toString())&&_e.onUpdate){
_e.onUpdate(_1b,i,old,val);
}
}
}
}
if(_1a&&(_9 in it||_1b instanceof Array)){
for(i in _1b){
if(!_1b.__isDirty&&_1b.hasOwnProperty(i)&&!it.hasOwnProperty(i)&&!(i.charAt(0)=="_"&&i.charAt(1)=="_")&&!(_1b instanceof Array&&isNaN(i))){
if(_e.onUpdate&&i!="_loadObject"&&i!="_idAttr"){
_e.onUpdate(_1b,i,_1b[i],undefined);
}
delete _1b[i];
while(_1b instanceof Array&&_1b.length&&_1b[_1b.length-1]===undefined){
_1b.length--;
}
}
}
}else{
if(_e.onLoad){
_e.onLoad(_1b);
}
}
return _1b;
};
if(_7&&typeof _7=="object"){
_7=_13(_7,false,_8.defaultId,true);
_13(_10,false);
}
return _7;
},fromJson:function(str,_22){
function ref(_23){
var _24={};
_24[this.refAttribute]=_23;
return _24;
};
var _25;
try{
_25=eval("("+str+")");
}
catch(e){
throw new SyntaxError("Invalid JSON string: "+e.message+" parsing: "+str);
}
if(_25){
return this.resolveJson(_25,_22);
}
return _25;
},toJson:function(it,_26,_27,_28,_29){
var _2a=this._useRefs;
var _2b=this._addProp;
var _2c=this.refAttribute;
_27=_27||"";
var _2d={};
var _2e={};
var _2f=_29?true:false;
var _30=1;
function _31(){
var str=(_30++).toString();
if(_2e.hasOwnProperty(str)){
return _31();
}
return str;
};
function _32(it,_33,_34){
if(typeof it=="object"&&it){
var _35;
if(it instanceof Date){
return "\""+_5.toISOString(it,{zulu:true})+"\"";
}
var id=it.__id;
if(id){
if(_33!="#"&&((_2a&&!id.match(/#/))||_2d[id])){
var ref=id;
if(id.charAt(0)!="#"){
if(it.__clientId==id){
ref="cid:"+id;
}else{
if(id.substring(0,_27.length)==_27){
ref=id.substring(_27.length);
}else{
ref=id;
}
}
}
var _36={};
_36[_2c]=ref;
return _2.toJson(_36,_26);
}
_33=id;
}else{
if(_2f){
if(!(it instanceof Array)){
_33=_31();
it.__id=_33;
_2e[_33]=it;
}
}else{
it.__id=_33;
_2e[_33]=it;
}
}
_2d[_33]=it;
_34=_34||"";
var _37=_26?_34+_2.toJsonIndentStr:"";
var _38=_26?"\n":"";
var sep=_26?" ":"";
if(it instanceof Array){
var res=_1.map(it,function(obj,i){
var val=_32(obj,_2f?undefined:_2b(_33,i),_37);
if(typeof val!="string"){
val="undefined";
}
return _38+_37+val;
});
return "["+res.join(","+sep)+_38+_34+"]";
}
var _39=[];
if(_2f&&(typeof it[_29]==="undefined")){
_39.push(_38+_37+_2._escapeString(_29)+":"+sep+_2.toJson(it.__id));
}
for(var i in it){
if(it.hasOwnProperty(i)){
var _3a;
if(typeof i=="number"){
_3a="\""+i+"\"";
}else{
if(typeof i=="string"&&(i.charAt(0)!="_"||i.charAt(1)!="_")){
_3a=_2._escapeString(i);
}else{
continue;
}
}
var val=_32(it[i],_2f?undefined:_2b(_33,i),_37);
if(typeof val!="string"){
continue;
}
_39.push(_38+_37+_3a+":"+sep+val);
}
}
return "{"+_39.join(","+sep)+_38+_34+"}";
}else{
if(typeof it=="function"&&_6.json.ref.serializeFunctions){
return it.toString();
}
}
return _2.toJson(it);
};
var _3b=_32(it,_2f?undefined:"#","");
if(!_28){
for(var i in _2e){
delete _2e[i].__id;
}
}
return _3b;
},_addProp:function(id,_3c){
return id+(id.match(/#/)?id.length==1?"":".":"#")+_3c;
},refAttribute:"$ref",_useRefs:false,serializeFunctions:false};
});
