//>>built
define("dojox/json/ref",["dojo/_base/kernel","dojox","dojo/date/stamp","dojo/_base/array","dojo/_base/json"],function(_1,_2){
_1.getObject("json",true,_2);
return _2.json.ref={resolveJson:function(_3,_4){
_4=_4||{};
var _5=_4.idAttribute||"id";
var _6=this.refAttribute;
var _7=_4.idAsRef;
var _8=_4.idPrefix||"";
var _9=_4.assignAbsoluteIds;
var _a=_4.index||{};
var _b=_4.timeStamps;
var _c,_d=[];
var _e=/^(.*\/)?(\w+:\/\/)|[^\/\.]+\/\.\.\/|^.*\/(\/)/;
var _f=this._addProp;
var F=function(){
};
function _10(it,_11,_12,_13,_14,_15){
var i,_16,val,id=_5 in it?it[_5]:_12;
if(_5 in it||((id!==undefined)&&_13)){
id=(_8+id).replace(_e,"$2$3");
}
var _17=_15||it;
if(id!==undefined){
if(_9){
it.__id=id;
}
if(_4.schemas&&(!(it instanceof Array))&&(val=id.match(/^(.+\/)[^\.\[]*$/))){
_14=_4.schemas[val[1]];
}
if(_a[id]&&((it instanceof Array)==(_a[id] instanceof Array))){
_17=_a[id];
delete _17.$ref;
delete _17._loadObject;
_16=true;
}else{
var _18=_14&&_14.prototype;
if(_18){
F.prototype=_18;
_17=new F();
}
}
_a[id]=_17;
if(_b){
_b[id]=_4.time;
}
}
while(_14){
var _19=_14.properties;
if(_19){
for(i in it){
var _1a=_19[i];
if(_1a&&_1a.format=="date-time"&&typeof it[i]=="string"){
it[i]=_1.date.stamp.fromISOString(it[i]);
}
}
}
_14=_14["extends"];
}
var _1b=it.length;
for(i in it){
if(i==_1b){
break;
}
if(it.hasOwnProperty(i)){
val=it[i];
if((typeof val=="object")&&val&&!(val instanceof Date)&&i!="__parent"){
_c=val[_6]||(_7&&val[_5]);
if(!_c||!val.__parent){
if(it!=_d){
val.__parent=_17;
}
}
if(_c){
delete it[i];
var _1c=_c.toString().replace(/(#)([^\.\[])/,"$1.$2").match(/(^([^\[]*\/)?[^#\.\[]*)#?([\.\[].*)?/);
if(_a[(_8+_c).replace(_e,"$2$3")]){
_c=_a[(_8+_c).replace(_e,"$2$3")];
}else{
if((_c=(_1c[1]=="$"||_1c[1]=="this"||_1c[1]=="")?_3:_a[(_8+_1c[1]).replace(_e,"$2$3")])){
if(_1c[3]){
_1c[3].replace(/(\[([^\]]+)\])|(\.?([^\.\[]+))/g,function(t,a,b,c,d){
_c=_c&&_c[b?b.replace(/[\"\'\\]/,""):d];
});
}
}
}
if(_c){
val=_c;
}else{
if(!_11){
var _1d;
if(!_1d){
_d.push(_17);
}
_1d=true;
val=_10(val,false,val[_6],true,_1a);
val._loadObject=_4.loader;
}
}
}else{
if(!_11){
val=_10(val,_d==it,id===undefined?undefined:_f(id,i),false,_1a,_17!=it&&typeof _17[i]=="object"&&_17[i]);
}
}
}
it[i]=val;
if(_17!=it&&!_17.__isDirty){
var old=_17[i];
_17[i]=val;
if(_16&&val!==old&&!_17._loadObject&&!(i.charAt(0)=="_"&&i.charAt(1)=="_")&&i!="$ref"&&!(val instanceof Date&&old instanceof Date&&val.getTime()==old.getTime())&&!(typeof val=="function"&&typeof old=="function"&&val.toString()==old.toString())&&_a.onUpdate){
_a.onUpdate(_17,i,old,val);
}
}
}
}
if(_16&&(_5 in it||_17 instanceof Array)){
for(i in _17){
if(!_17.__isDirty&&_17.hasOwnProperty(i)&&!it.hasOwnProperty(i)&&!(i.charAt(0)=="_"&&i.charAt(1)=="_")&&!(_17 instanceof Array&&isNaN(i))){
if(_a.onUpdate&&i!="_loadObject"&&i!="_idAttr"){
_a.onUpdate(_17,i,_17[i],undefined);
}
delete _17[i];
while(_17 instanceof Array&&_17.length&&_17[_17.length-1]===undefined){
_17.length--;
}
}
}
}else{
if(_a.onLoad){
_a.onLoad(_17);
}
}
return _17;
};
if(_3&&typeof _3=="object"){
_3=_10(_3,false,_4.defaultId,true);
_10(_d,false);
}
return _3;
},fromJson:function(str,_1e){
function ref(_1f){
var _20={};
_20[this.refAttribute]=_1f;
return _20;
};
try{
var _21=eval("("+str+")");
}
catch(e){
throw new SyntaxError("Invalid JSON string: "+e.message+" parsing: "+str);
}
if(_21){
return this.resolveJson(_21,_1e);
}
return _21;
},toJson:function(it,_22,_23,_24){
var _25=this._useRefs;
var _26=this._addProp;
var _27=this.refAttribute;
_23=_23||"";
var _28={};
var _29={};
function _2a(it,_2b,_2c){
if(typeof it=="object"&&it){
var _2d;
if(it instanceof Date){
return "\""+_1.date.stamp.toISOString(it,{zulu:true})+"\"";
}
var id=it.__id;
if(id){
if(_2b!="#"&&((_25&&!id.match(/#/))||_28[id])){
var ref=id;
if(id.charAt(0)!="#"){
if(it.__clientId==id){
ref="cid:"+id;
}else{
if(id.substring(0,_23.length)==_23){
ref=id.substring(_23.length);
}else{
ref=id;
}
}
}
var _2e={};
_2e[_27]=ref;
return _2a(_2e,"#");
}
_2b=id;
}else{
it.__id=_2b;
_29[_2b]=it;
}
_28[_2b]=it;
_2c=_2c||"";
var _2f=_22?_2c+_1.toJsonIndentStr:"";
var _30=_22?"\n":"";
var sep=_22?" ":"";
if(it instanceof Array){
var res=_1.map(it,function(obj,i){
var val=_2a(obj,_26(_2b,i),_2f);
if(typeof val!="string"){
val="undefined";
}
return _30+_2f+val;
});
return "["+res.join(","+sep)+_30+_2c+"]";
}
var _31=[];
for(var i in it){
if(it.hasOwnProperty(i)){
var _32;
if(typeof i=="number"){
_32="\""+i+"\"";
}else{
if(typeof i=="string"&&(i.charAt(0)!="_"||i.charAt(1)!="_")){
_32=_1._escapeString(i);
}else{
continue;
}
}
var val=_2a(it[i],_26(_2b,i),_2f);
if(typeof val!="string"){
continue;
}
_31.push(_30+_2f+_32+":"+sep+val);
}
}
return "{"+_31.join(","+sep)+_30+_2c+"}";
}else{
if(typeof it=="function"&&_2.json.ref.serializeFunctions){
return it.toString();
}
}
return _1.toJson(it);
};
var _33=_2a(it,"#","");
if(!_24){
for(var i in _29){
delete _29[i].__id;
}
}
return _33;
},_addProp:function(id,_34){
return id+(id.match(/#/)?id.length==1?"":".":"#")+_34;
},refAttribute:"$ref",_useRefs:false,serializeFunctions:false};
});
