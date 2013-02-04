//>>built
define("dojox/json/schema",["dojo/_base/kernel","dojox","dojo/_base/array"],function(_1,_2){
_1.getObject("json.schema",true,_2);
_2.json.schema.validate=function(_3,_4){
return this._validate(_3,_4,false);
};
_2.json.schema.checkPropertyChange=function(_5,_6,_7){
return this._validate(_5,_6,_7||"property");
};
_2.json.schema.mustBeValid=function(_8){
if(!_8.valid){
throw new TypeError(_1.map(_8.errors,function(_9){
return "for property "+_9.property+": "+_9.message;
}).join(", "));
}
};
_2.json.schema._validate=function(_a,_b,_c){
var _d=[];
function _e(_f,_10,_11,i){
var l;
_11+=_11?typeof i=="number"?"["+i+"]":typeof i=="undefined"?"":"."+i:i;
function _12(_13){
_d.push({property:_11,message:_13});
};
if((typeof _10!="object"||_10 instanceof Array)&&(_11||typeof _10!="function")){
if(typeof _10=="function"){
if(!(Object(_f) instanceof _10)){
_12("is not an instance of the class/constructor "+_10.name);
}
}else{
if(_10){
_12("Invalid schema/property definition "+_10);
}
}
return null;
}
if(_c&&_10.readonly){
_12("is a readonly field, it can not be changed");
}
if(_10["extends"]){
_e(_f,_10["extends"],_11,i);
}
function _14(_15,_16){
if(_15){
if(typeof _15=="string"&&_15!="any"&&(_15=="null"?_16!==null:typeof _16!=_15)&&!(_16 instanceof Array&&_15=="array")&&!(_15=="integer"&&_16%1===0)){
return [{property:_11,message:(typeof _16)+" value found, but a "+_15+" is required"}];
}
if(_15 instanceof Array){
var _17=[];
for(var j=0;j<_15.length;j++){
if(!(_17=_14(_15[j],_16)).length){
break;
}
}
if(_17.length){
return _17;
}
}else{
if(typeof _15=="object"){
var _18=_d;
_d=[];
_e(_16,_15,_11);
var _19=_d;
_d=_18;
return _19;
}
}
}
return [];
};
if(_f===undefined){
if(!_10.optional){
_12("is missing and it is not optional");
}
}else{
_d=_d.concat(_14(_10.type,_f));
if(_10.disallow&&!_14(_10.disallow,_f).length){
_12(" disallowed value was matched");
}
if(_f!==null){
if(_f instanceof Array){
if(_10.items){
if(_10.items instanceof Array){
for(i=0,l=_f.length;i<l;i++){
_d.concat(_e(_f[i],_10.items[i],_11,i));
}
}else{
for(i=0,l=_f.length;i<l;i++){
_d.concat(_e(_f[i],_10.items,_11,i));
}
}
}
if(_10.minItems&&_f.length<_10.minItems){
_12("There must be a minimum of "+_10.minItems+" in the array");
}
if(_10.maxItems&&_f.length>_10.maxItems){
_12("There must be a maximum of "+_10.maxItems+" in the array");
}
}else{
if(_10.properties){
_d.concat(_1a(_f,_10.properties,_11,_10.additionalProperties));
}
}
if(_10.pattern&&typeof _f=="string"&&!_f.match(_10.pattern)){
_12("does not match the regex pattern "+_10.pattern);
}
if(_10.maxLength&&typeof _f=="string"&&_f.length>_10.maxLength){
_12("may only be "+_10.maxLength+" characters long");
}
if(_10.minLength&&typeof _f=="string"&&_f.length<_10.minLength){
_12("must be at least "+_10.minLength+" characters long");
}
if(typeof _10.minimum!==undefined&&typeof _f==typeof _10.minimum&&_10.minimum>_f){
_12("must have a minimum value of "+_10.minimum);
}
if(typeof _10.maximum!==undefined&&typeof _f==typeof _10.maximum&&_10.maximum<_f){
_12("must have a maximum value of "+_10.maximum);
}
if(_10["enum"]){
var _1b=_10["enum"];
l=_1b.length;
var _1c;
for(var j=0;j<l;j++){
if(_1b[j]===_f){
_1c=1;
break;
}
}
if(!_1c){
_12("does not have a value in the enumeration "+_1b.join(", "));
}
}
if(typeof _10.maxDecimal=="number"&&(_f.toString().match(new RegExp("\\.[0-9]{"+(_10.maxDecimal+1)+",}")))){
_12("may only have "+_10.maxDecimal+" digits of decimal places");
}
}
}
return null;
};
function _1a(_1d,_1e,_1f,_20){
if(typeof _1e=="object"){
if(typeof _1d!="object"||_1d instanceof Array){
_d.push({property:_1f,message:"an object is required"});
}
for(var i in _1e){
if(_1e.hasOwnProperty(i)&&!(i.charAt(0)=="_"&&i.charAt(1)=="_")){
var _21=_1d[i];
var _22=_1e[i];
_e(_21,_22,_1f,i);
}
}
}
for(i in _1d){
if(_1d.hasOwnProperty(i)&&!(i.charAt(0)=="_"&&i.charAt(1)=="_")&&_1e&&!_1e[i]&&_20===false){
_d.push({property:_1f,message:(typeof _21)+"The property "+i+" is not defined in the schema and the schema does not allow additional properties"});
}
var _23=_1e&&_1e[i]&&_1e[i].requires;
if(_23&&!(_23 in _1d)){
_d.push({property:_1f,message:"the presence of the property "+i+" requires that "+_23+" also be present"});
}
_21=_1d[i];
if(_1e&&typeof _1e=="object"&&!(i in _1e)){
_e(_21,_20,_1f,i);
}
if(!_c&&_21&&_21.$schema){
_d=_d.concat(_e(_21,_21.$schema,_1f,i));
}
}
return _d;
};
if(_b){
_e(_a,_b,"",_c||"");
}
if(!_c&&_a&&_a.$schema){
_e(_a,_a.$schema,"","");
}
return {valid:!_d.length,errors:_d};
};
return _2.json.schema;
});
