//>>built
define("dojox/help/_base",["dojo","dijit","dojox","dojo/require!dojox/rpc/Service,dojo/io/script"],function(_1,_2,_3){
_1.provide("dojox.help._base");
_1.require("dojox.rpc.Service");
_1.require("dojo.io.script");
_1.experimental("dojox.help");
console.warn("Script causes side effects (on numbers, strings, and booleans). Call dojox.help.noConflict() if you plan on executing code.");
_3.help={locate:function(_4,_5,_6){
_6=_6||20;
var _7=[];
var _8={};
var _9;
if(_5){
if(!_1.isArray(_5)){
_5=[_5];
}
for(var i=0,_a;_a=_5[i];i++){
_9=_a;
if(_1.isString(_a)){
_a=_1.getObject(_a);
if(!_a){
continue;
}
}else{
if(_1.isObject(_a)){
_9=_a.__name__;
}else{
continue;
}
}
_7.push(_a);
if(_9){
_9=_9.split(".")[0];
if(!_8[_9]&&_1.indexOf(_3.help._namespaces,_9)==-1){
_3.help.refresh(_9);
}
_8[_9]=true;
}
}
}
if(!_7.length){
_7.push({__name__:"window"});
_1.forEach(_3.help._namespaces,function(_b){
_8[_b]=true;
});
}
var _c=_4.toLowerCase();
var _d=[];
out:
for(var i=0,_a;_a=_7[i];i++){
var _e=_a.__name__||"";
var _f=_1.some(_7,function(_10){
_10=_10.__name__||"";
return (_e.indexOf(_10+".")==0);
});
if(_e&&!_f){
_9=_e.split(".")[0];
var _11=[];
if(_e=="window"){
for(_9 in _3.help._names){
if(_1.isArray(_3.help._names[_9])){
_11=_11.concat(_3.help._names[_9]);
}
}
}else{
_11=_3.help._names[_9];
}
for(var j=0,_12;_12=_11[j];j++){
if((_e=="window"||_12.indexOf(_e+".")==0)&&_12.toLowerCase().indexOf(_c)!=-1){
if(_12.slice(-10)==".prototype"){
continue;
}
var obj=_1.getObject(_12);
if(obj){
_d.push([_12,obj]);
if(_d.length==_6){
break out;
}
}
}
}
}
}
_3.help._displayLocated(_d);
if(!_1.isMoz){
return "";
}
},refresh:function(_13,_14){
if(arguments.length<2){
_14=true;
}
_3.help._recurse(_13,_14);
},noConflict:function(_15){
if(arguments.length){
return _3.help._noConflict(_15);
}else{
while(_3.help._overrides.length){
var _16=_3.help._overrides.pop();
var _17=_16[0];
var key=_16[1];
var _18=_17[key];
_17[key]=_3.help._noConflict(_18);
}
}
},init:function(_19,_1a){
if(_19){
_3.help._namespaces.concat(_19);
}
_1.addOnLoad(function(){
_1.require=(function(_1b){
return function(){
_3.help.noConflict();
_1b.apply(_1,arguments);
if(_3.help._timer){
clearTimeout(_3.help._timer);
}
_3.help._timer=setTimeout(function(){
_1.addOnLoad(function(){
_3.help.refresh();
_3.help._timer=false;
});
},500);
};
})(_1.require);
_3.help._recurse();
});
},_noConflict:function(_1c){
if(_1c instanceof String){
return _1c.toString();
}else{
if(_1c instanceof Number){
return +_1c;
}else{
if(_1c instanceof Boolean){
return (_1c==true);
}else{
if(_1.isObject(_1c)){
delete _1c.__name__;
delete _1c.help;
}
}
}
}
return _1c;
},_namespaces:["dojo","dojox","dijit","djConfig"],_rpc:new _3.rpc.Service(_1.moduleUrl("dojox.rpc.SMDLibrary","dojo-api.smd")),_attributes:["summary","type","returns","parameters"],_clean:function(_1d){
var obj={};
for(var i=0,_1e;_1e=_3.help._attributes[i];i++){
var _1f=_1d["__"+_1e+"__"];
if(_1f){
obj[_1e]=_1f;
}
}
return obj;
},_displayLocated:function(_20){
throw new Error("_displayLocated should be overridden in one of the dojox.help packages");
},_displayHelp:function(_21,obj){
throw new Error("_displayHelp should be overridden in one of the dojox.help packages");
},_addVersion:function(obj){
if(obj.name){
obj.version=[_1.version.major,_1.version.minor,_1.version.patch].join(".");
var _22=obj.name.split(".");
if(_22[0]=="dojo"||_22[0]=="dijit"||_22[0]=="dojox"){
obj.project=_22[0];
}
}
return obj;
},_stripPrototype:function(_23){
var _24=_23.replace(/\.prototype(\.|$)/g,".");
var _25=_24;
if(_24.slice(-1)=="."){
_25=_24=_24.slice(0,-1);
}else{
_24=_23;
}
return [_25,_24];
},_help:function(){
var _26=this.__name__;
var _27=_3.help._stripPrototype(_26)[0];
var _28=[];
for(var i=0,_29;_29=_3.help._attributes[i];i++){
if(!this["__"+_29+"__"]){
_28.push(_29);
}
}
_3.help._displayHelp(true,{name:this.__name__});
if(!_28.length||this.__searched__){
_3.help._displayHelp(false,_3.help._clean(this));
}else{
this.__searched__=true;
_3.help._rpc.get(_3.help._addVersion({name:_27,exact:true,attributes:_28})).addCallback(this,function(_2a){
if(this.toString===_3.help._toString){
this.toString(_2a);
}
if(_2a&&_2a.length){
_2a=_2a[0];
for(var i=0,_29;_29=_3.help._attributes[i];i++){
if(_2a[_29]){
this["__"+_29+"__"]=_2a[_29];
}
}
_3.help._displayHelp(false,_3.help._clean(this));
}else{
_3.help._displayHelp(false,false);
}
});
}
if(!_1.isMoz){
return "";
}
},_parse:function(_2b){
delete this.__searching__;
if(_2b&&_2b.length){
var _2c=_2b[0].parameters;
if(_2c){
var _2d=["function ",this.__name__,"("];
this.__parameters__=_2c;
for(var i=0,_2e;_2e=_2c[i];i++){
if(i){
_2d.push(", ");
}
_2d.push(_2e.name);
if(_2e.types){
var _2f=[];
for(var j=0,_30;_30=_2e.types[j];j++){
_2f.push(_30.title);
}
if(_2f.length){
_2d.push(": ");
_2d.push(_2f.join("|"));
}
}
if(_2e.repeating){
_2d.push("...");
}
if(_2e.optional){
_2d.push("?");
}
}
_2d.push(")");
this.__source__=this.__source__.replace(/function[^\(]*\([^\)]*\)/,_2d.join(""));
}
if(this.__output__){
delete this.__output__;
}
}else{
_3.help._displayHelp(false,false);
}
},_toStrings:{},_toString:function(_31){
if(!this.__source__){
return this.__name__;
}
var _32=(!this.__parameters__);
this.__parameters__=[];
if(_31){
_3.help._parse.call(this,_31);
}else{
if(_32){
this.__searching__=true;
_3.help._toStrings[_3.help._stripPrototype(this.__name__)[0]]=this;
if(_3.help._toStringTimer){
clearTimeout(_3.help._toStringTimer);
}
_3.help._toStringTimer=setTimeout(function(){
_3.help.__toString();
},50);
}
}
if(!_32||!this.__searching__){
return this.__source__;
}
var _33="function Loading info for "+this.__name__+"... (watch console for result) {}";
if(!_1.isMoz){
this.__output__=true;
return _33;
}
return {toString:_1.hitch(this,function(){
this.__output__=true;
return _33;
})};
},__toString:function(){
if(_3.help._toStringTimer){
clearTimeout(_3.help._toStringTimer);
}
var _34=[];
_3.help.noConflict(_3.help._toStrings);
for(var _35 in _3.help._toStrings){
_34.push(_35);
}
while(_34.length){
_3.help._rpc.batch(_3.help._addVersion({names:_34.splice(-50,50),exact:true,attributes:["parameters"]})).addCallback(this,function(_36){
for(var i=0,_37;_37=_36[i];i++){
var fn=_3.help._toStrings[_37.name];
if(fn){
_3.help._parse.call(fn,[_37]);
delete _3.help._toStrings[_37.name];
}
}
});
}
},_overrides:[],_recursions:[],_names:{},_recurse:function(_38,_39){
if(arguments.length<2){
_39=true;
}
var _3a=[];
if(_38&&_1.isString(_38)){
_3.help.__recurse(_1.getObject(_38),_38,_38,_3a,_39);
}else{
for(var i=0,ns;ns=_3.help._namespaces[i];i++){
if(window[ns]){
_3.help._recursions.push([window[ns],ns,ns]);
window[ns].__name__=ns;
if(!window[ns].help){
window[ns].help=_3.help._help;
}
}
}
}
while(_3.help._recursions.length){
var _3b=_3.help._recursions.shift();
_3.help.__recurse(_3b[0],_3b[1],_3b[2],_3a,_39);
}
for(var i=0,_3c;_3c=_3a[i];i++){
delete _3c.__seen__;
}
},__recurse:function(_3d,_3e,_3f,_40,_41){
for(var key in _3d){
if(key.match(/([^\w_.$]|__[\w_.$]+__)/)){
continue;
}
var _42=_3d[key];
if(typeof _42=="undefined"||_42===document||_42===window||_42===_3.help._toString||_42===_3.help._help||_42===null||(+_1.isIE&&_42.tagName)||_42.__seen__){
continue;
}
var _43=_1.isFunction(_42);
var _44=_1.isObject(_42)&&!_1.isArray(_42)&&!_42.nodeType;
var _45=(_3f)?(_3f+"."+key):key;
if(_45=="dojo._blockAsync"){
continue;
}
if(!_42.__name__){
var _46=null;
if(_1.isString(_42)){
_46=String;
}else{
if(typeof _42=="number"){
_46=Number;
}else{
if(typeof _42=="boolean"){
_46=Boolean;
}
}
}
if(_46){
_42=_3d[key]=new _46(_42);
}
}
_42.__seen__=true;
_42.__name__=_45;
(_3.help._names[_3e]=_3.help._names[_3e]||[]).push(_45);
_40.push(_42);
if(!_43){
_3.help._overrides.push([_3d,key]);
}
if((_43||_44)&&_41){
_3.help._recursions.push([_42,_3e,_45]);
}
if(_43){
if(!_42.__source__){
_42.__source__=_42.toString().replace(/^function\b ?/,"function "+_45);
}
if(_42.toString===Function.prototype.toString){
_42.toString=_3.help._toString;
}
}
if(!_42.help){
_42.help=_3.help._help;
}
}
}};
});
