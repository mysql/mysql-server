/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/parser",["./_base/kernel","./_base/lang","./_base/array","./_base/html","./_base/window","./_base/url","./_base/json","./aspect","./date/stamp","./query","./on","./ready"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
new Date("X");
var _c={"dom-attributes-explicit":document.createElement("div").attributes.length<40};
function _d(_e){
return _c[_e];
};
_1.parser=new function(){
var _f={};
function _10(_11){
var map={};
for(var _12 in _11){
if(_12.charAt(0)=="_"){
continue;
}
map[_12.toLowerCase()]=_12;
}
return map;
};
_8.after(_2,"extend",function(){
_f={};
},true);
var _13={};
this._functionFromScript=function(_14,_15){
var _16="";
var _17="";
var _18=(_14.getAttribute(_15+"args")||_14.getAttribute("args"));
if(_18){
_3.forEach(_18.split(/\s*,\s*/),function(_19,idx){
_16+="var "+_19+" = arguments["+idx+"]; ";
});
}
var _1a=_14.getAttribute("with");
if(_1a&&_1a.length){
_3.forEach(_1a.split(/\s*,\s*/),function(_1b){
_16+="with("+_1b+"){";
_17+="}";
});
}
return new Function(_16+_14.innerHTML+_17);
};
this.instantiate=function(_1c,_1d,_1e){
var _1f=[],_1d=_1d||{};
_1e=_1e||{};
var _20=(_1e.scope||_1._scopeName)+"Type",_21="data-"+(_1e.scope||_1._scopeName)+"-",_22=_21+"type",_23=_21+"props",_24=_21+"attach-point",_25=_21+"attach-event",_26=_21+"id";
var _27={};
_3.forEach([_23,_22,_20,_26,"jsId",_24,_25,"dojoAttachPoint","dojoAttachEvent","class","style"],function(_28){
_27[_28.toLowerCase()]=_28.replace(_1e.scope,"dojo");
});
_3.forEach(_1c,function(obj){
if(!obj){
return;
}
var _29=obj.node||obj,_2a=_20 in _1d?_1d[_20]:obj.node?obj.type:(_29.getAttribute(_22)||_29.getAttribute(_20)),_2b=_13[_2a]||(_13[_2a]=_2.getObject(_2a)),_2c=_2b&&_2b.prototype;
if(!_2b){
throw new Error("Could not load class '"+_2a);
}
var _2d={};
if(_1e.defaults){
_2.mixin(_2d,_1e.defaults);
}
if(obj.inherited){
_2.mixin(_2d,obj.inherited);
}
var _2e;
if(_d("dom-attributes-explicit")){
_2e=_29.attributes;
}else{
var _2f=/^input$|^img$/i.test(_29.nodeName)?_29:_29.cloneNode(false),_30=_2f.outerHTML.replace(/=[^\s"']+|="[^"]*"|='[^']*'/g,"").replace(/^\s*<[a-zA-Z0-9]*/,"").replace(/>.*$/,"");
_2e=_3.map(_30.split(/\s+/),function(_31){
var _32=_31.toLowerCase();
return {name:_31,value:(_29.nodeName=="LI"&&_31=="value")||_32=="enctype"?_29.getAttribute(_32):_29.getAttributeNode(_32).value,specified:true};
});
}
var i=0,_33;
while(_33=_2e[i++]){
if(!_33||!_33.specified){
continue;
}
var _34=_33.name,_35=_34.toLowerCase(),_36=_33.value;
if(_35 in _27){
switch(_27[_35]){
case "data-dojo-props":
var _37=_36;
break;
case "data-dojo-id":
case "jsId":
var _38=_36;
break;
case "data-dojo-attach-point":
case "dojoAttachPoint":
_2d.dojoAttachPoint=_36;
break;
case "data-dojo-attach-event":
case "dojoAttachEvent":
_2d.dojoAttachEvent=_36;
break;
case "class":
_2d["class"]=_29.className;
break;
case "style":
_2d["style"]=_29.style&&_29.style.cssText;
break;
}
}else{
if(!(_34 in _2c)){
var map=(_f[_2a]||(_f[_2a]=_10(_2c)));
_34=map[_35]||_34;
}
if(_34 in _2c){
switch(typeof _2c[_34]){
case "string":
_2d[_34]=_36;
break;
case "number":
_2d[_34]=_36.length?Number(_36):NaN;
break;
case "boolean":
_2d[_34]=_36.toLowerCase()!="false";
break;
case "function":
if(_36===""||_36.search(/[^\w\.]+/i)!=-1){
_2d[_34]=new Function(_36);
}else{
_2d[_34]=_2.getObject(_36,false)||new Function(_36);
}
break;
default:
var _39=_2c[_34];
_2d[_34]=(_39&&"length" in _39)?(_36?_36.split(/\s*,\s*/):[]):(_39 instanceof Date)?(_36==""?new Date(""):_36=="now"?new Date():_9.fromISOString(_36)):(_39 instanceof _1._Url)?(_1.baseUrl+_36):_7.fromJson(_36);
}
}else{
_2d[_34]=_36;
}
}
}
if(_37){
try{
_37=_7.fromJson.call(_1e.propsThis,"{"+_37+"}");
_2.mixin(_2d,_37);
}
catch(e){
throw new Error(e.toString()+" in data-dojo-props='"+_37+"'");
}
}
_2.mixin(_2d,_1d);
var _3a=obj.node?obj.scripts:(_2b&&(_2b._noScript||_2c._noScript)?[]:_a("> script[type^='dojo/']",_29));
var _3b=[],_3c=[],_3d=[],on=[];
if(_3a){
for(i=0;i<_3a.length;i++){
var _3e=_3a[i];
_29.removeChild(_3e);
var _3f=(_3e.getAttribute(_21+"event")||_3e.getAttribute("event")),_40=_3e.getAttribute(_21+"prop"),_2a=_3e.getAttribute("type"),nf=this._functionFromScript(_3e,_21);
if(_3f){
if(_2a=="dojo/connect"){
_3b.push({event:_3f,func:nf});
}else{
if(_2a=="dojo/on"){
on.push({event:_3f,func:nf});
}else{
_2d[_3f]=nf;
}
}
}else{
if(_2a=="dojo/watch"){
_3d.push({prop:_40,func:nf});
}else{
_3c.push(nf);
}
}
}
}
var _41=_2b.markupFactory||_2c.markupFactory;
var _42=_41?_41(_2d,_29,_2b):new _2b(_2d,_29);
_1f.push(_42);
if(_38){
_2.setObject(_38,_42);
}
for(i=0;i<_3b.length;i++){
_8.after(_42,_3b[i].event,_1.hitch(_42,_3b[i].func),true);
}
for(i=0;i<_3c.length;i++){
_3c[i].call(_42);
}
for(i=0;i<_3d.length;i++){
_42.watch(_3d[i].prop,_3d[i].func);
}
for(i=0;i<on.length;i++){
_b(_42,on[i].event,on[i].func);
}
},this);
if(!_1d._started){
_3.forEach(_1f,function(_43){
if(!_1e.noStart&&_43&&_2.isFunction(_43.startup)&&!_43._started){
_43.startup();
}
});
}
return _1f;
};
this.parse=function(_44,_45){
var _46;
if(!_45&&_44&&_44.rootNode){
_45=_44;
_46=_45.rootNode;
}else{
_46=_44;
}
_46=_46?_4.byId(_46):_5.body();
_45=_45||{};
var _47=(_45.scope||_1._scopeName)+"Type",_48="data-"+(_45.scope||_1._scopeName)+"-",_49=_48+"type",_4a=_48+"textdir";
var _4b=[];
var _4c=_46.firstChild;
var _4d=_45&&_45.inherited;
if(!_4d){
function _4e(_4f,_50){
return (_4f.getAttribute&&_4f.getAttribute(_50))||(_4f!==_5.doc&&_4f!==_5.doc.documentElement&&_4f.parentNode?_4e(_4f.parentNode,_50):null);
};
_4d={dir:_4e(_46,"dir"),lang:_4e(_46,"lang"),textDir:_4e(_46,_4a)};
for(var key in _4d){
if(!_4d[key]){
delete _4d[key];
}
}
}
var _51={inherited:_4d};
var _52;
var _53;
function _54(_55){
if(!_55.inherited){
_55.inherited={};
var _56=_55.node,_57=_54(_55.parent);
var _58={dir:_56.getAttribute("dir")||_57.dir,lang:_56.getAttribute("lang")||_57.lang,textDir:_56.getAttribute(_4a)||_57.textDir};
for(var key in _58){
if(_58[key]){
_55.inherited[key]=_58[key];
}
}
}
return _55.inherited;
};
while(true){
if(!_4c){
if(!_51||!_51.node){
break;
}
_4c=_51.node.nextSibling;
_52=_51.scripts;
_53=false;
_51=_51.parent;
continue;
}
if(_4c.nodeType!=1){
_4c=_4c.nextSibling;
continue;
}
if(_52&&_4c.nodeName.toLowerCase()=="script"){
_59=_4c.getAttribute("type");
if(_59&&/^dojo\/\w/i.test(_59)){
_52.push(_4c);
}
_4c=_4c.nextSibling;
continue;
}
if(_53){
_4c=_4c.nextSibling;
continue;
}
var _59=_4c.getAttribute(_49)||_4c.getAttribute(_47);
var _5a=_4c.firstChild;
if(!_59&&(!_5a||(_5a.nodeType==3&&!_5a.nextSibling))){
_4c=_4c.nextSibling;
continue;
}
var _5b={node:_4c,scripts:_52,parent:_51};
var _5c=_59&&(_13[_59]||(_13[_59]=_2.getObject(_59))),_5d=_5c&&!_5c.prototype._noScript?[]:null;
if(_59){
_4b.push({"type":_59,node:_4c,scripts:_5d,inherited:_54(_5b)});
}
_4c=_5a;
_52=_5d;
_53=_5c&&_5c.prototype.stopParser&&!(_45&&_45.template);
_51=_5b;
}
var _5e=_45&&_45.template?{template:true}:null;
return this.instantiate(_4b,_5e,_45);
};
}();
if(_1.config.parseOnLoad){
_1.ready(100,_1.parser,"parse");
}
return _1.parser;
});
