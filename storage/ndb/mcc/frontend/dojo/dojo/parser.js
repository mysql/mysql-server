/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/parser",["require","./_base/kernel","./_base/lang","./_base/array","./_base/config","./_base/html","./_base/window","./_base/url","./_base/json","./aspect","./date/stamp","./Deferred","./has","./query","./on","./ready"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
new Date("X");
var _11=0;
_a.after(_3,"extend",function(){
_11++;
},true);
function _12(_13){
var map=_13._nameCaseMap,_14=_13.prototype;
if(!map||map._extendCnt<_11){
map=_13._nameCaseMap={};
for(var _15 in _14){
if(_15.charAt(0)==="_"){
continue;
}
map[_15.toLowerCase()]=_15;
}
map._extendCnt=_11;
}
return map;
};
var _16={};
function _17(_18){
var ts=_18.join();
if(!_16[ts]){
var _19=[];
for(var i=0,l=_18.length;i<l;i++){
var t=_18[i];
_19[_19.length]=(_16[t]=_16[t]||(_3.getObject(t)||(~t.indexOf("/")&&_1(t))));
}
var _1a=_19.shift();
_16[ts]=_19.length?(_1a.createSubclass?_1a.createSubclass(_19):_1a.extend.apply(_1a,_19)):_1a;
}
return _16[ts];
};
var _1b={_clearCache:function(){
_11++;
_16={};
},_functionFromScript:function(_1c,_1d){
var _1e="",_1f="",_20=(_1c.getAttribute(_1d+"args")||_1c.getAttribute("args")),_21=_1c.getAttribute("with");
var _22=(_20||"").split(/\s*,\s*/);
if(_21&&_21.length){
_4.forEach(_21.split(/\s*,\s*/),function(_23){
_1e+="with("+_23+"){";
_1f+="}";
});
}
return new Function(_22,_1e+_1c.innerHTML+_1f);
},instantiate:function(_24,_25,_26){
_25=_25||{};
_26=_26||{};
var _27=(_26.scope||_2._scopeName)+"Type",_28="data-"+(_26.scope||_2._scopeName)+"-",_29=_28+"type",_2a=_28+"mixins";
var _2b=[];
_4.forEach(_24,function(_2c){
var _2d=_27 in _25?_25[_27]:_2c.getAttribute(_29)||_2c.getAttribute(_27);
if(_2d){
var _2e=_2c.getAttribute(_2a),_2f=_2e?[_2d].concat(_2e.split(/\s*,\s*/)):[_2d];
_2b.push({node:_2c,types:_2f});
}
});
return this._instantiate(_2b,_25,_26);
},_instantiate:function(_30,_31,_32){
var _33=_4.map(_30,function(obj){
var _34=obj.ctor||_17(obj.types);
if(!_34){
throw new Error("Unable to resolve constructor for: '"+obj.types.join()+"'");
}
return this.construct(_34,obj.node,_31,_32,obj.scripts,obj.inherited);
},this);
if(!_31._started&&!_32.noStart){
_4.forEach(_33,function(_35){
if(typeof _35.startup==="function"&&!_35._started){
_35.startup();
}
});
}
return _33;
},construct:function(_36,_37,_38,_39,_3a,_3b){
var _3c=_36&&_36.prototype;
_39=_39||{};
var _3d={};
if(_39.defaults){
_3.mixin(_3d,_39.defaults);
}
if(_3b){
_3.mixin(_3d,_3b);
}
var _3e;
if(_d("dom-attributes-explicit")){
_3e=_37.attributes;
}else{
if(_d("dom-attributes-specified-flag")){
_3e=_4.filter(_37.attributes,function(a){
return a.specified;
});
}else{
var _3f=/^input$|^img$/i.test(_37.nodeName)?_37:_37.cloneNode(false),_40=_3f.outerHTML.replace(/=[^\s"']+|="[^"]*"|='[^']*'/g,"").replace(/^\s*<[a-zA-Z0-9]*\s*/,"").replace(/\s*>.*$/,"");
_3e=_4.map(_40.split(/\s+/),function(_41){
var _42=_41.toLowerCase();
return {name:_41,value:(_37.nodeName=="LI"&&_41=="value")||_42=="enctype"?_37.getAttribute(_42):_37.getAttributeNode(_42).value};
});
}
}
var _43=_39.scope||_2._scopeName,_44="data-"+_43+"-",_45={};
if(_43!=="dojo"){
_45[_44+"props"]="data-dojo-props";
_45[_44+"type"]="data-dojo-type";
_45[_44+"mixins"]="data-dojo-mixins";
_45[_43+"type"]="dojoType";
_45[_44+"id"]="data-dojo-id";
}
var i=0,_46,_47=[],_48,_49;
while(_46=_3e[i++]){
var _4a=_46.name,_4b=_4a.toLowerCase(),_4c=_46.value;
switch(_45[_4b]||_4b){
case "data-dojo-type":
case "dojotype":
case "data-dojo-mixins":
break;
case "data-dojo-props":
_49=_4c;
break;
case "data-dojo-id":
case "jsid":
_48=_4c;
break;
case "data-dojo-attach-point":
case "dojoattachpoint":
_3d.dojoAttachPoint=_4c;
break;
case "data-dojo-attach-event":
case "dojoattachevent":
_3d.dojoAttachEvent=_4c;
break;
case "class":
_3d["class"]=_37.className;
break;
case "style":
_3d["style"]=_37.style&&_37.style.cssText;
break;
default:
if(!(_4a in _3c)){
var map=_12(_36);
_4a=map[_4b]||_4a;
}
if(_4a in _3c){
switch(typeof _3c[_4a]){
case "string":
_3d[_4a]=_4c;
break;
case "number":
_3d[_4a]=_4c.length?Number(_4c):NaN;
break;
case "boolean":
_3d[_4a]=_4c.toLowerCase()!="false";
break;
case "function":
if(_4c===""||_4c.search(/[^\w\.]+/i)!=-1){
_3d[_4a]=new Function(_4c);
}else{
_3d[_4a]=_3.getObject(_4c,false)||new Function(_4c);
}
_47.push(_4a);
break;
default:
var _4d=_3c[_4a];
_3d[_4a]=(_4d&&"length" in _4d)?(_4c?_4c.split(/\s*,\s*/):[]):(_4d instanceof Date)?(_4c==""?new Date(""):_4c=="now"?new Date():_b.fromISOString(_4c)):(_4d instanceof _8)?(_2.baseUrl+_4c):_9.fromJson(_4c);
}
}else{
_3d[_4a]=_4c;
}
}
}
for(var j=0;j<_47.length;j++){
var _4e=_47[j].toLowerCase();
_37.removeAttribute(_4e);
_37[_4e]=null;
}
if(_49){
try{
_49=_9.fromJson.call(_39.propsThis,"{"+_49+"}");
_3.mixin(_3d,_49);
}
catch(e){
throw new Error(e.toString()+" in data-dojo-props='"+_49+"'");
}
}
_3.mixin(_3d,_38);
if(!_3a){
_3a=(_36&&(_36._noScript||_3c._noScript)?[]:_e("> script[type^='dojo/']",_37));
}
var _4f=[],_50=[],_51=[],ons=[];
if(_3a){
for(i=0;i<_3a.length;i++){
var _52=_3a[i];
_37.removeChild(_52);
var _53=(_52.getAttribute(_44+"event")||_52.getAttribute("event")),_54=_52.getAttribute(_44+"prop"),_55=_52.getAttribute(_44+"method"),_56=_52.getAttribute(_44+"advice"),_57=_52.getAttribute("type"),nf=this._functionFromScript(_52,_44);
if(_53){
if(_57=="dojo/connect"){
_4f.push({method:_53,func:nf});
}else{
if(_57=="dojo/on"){
ons.push({event:_53,func:nf});
}else{
_3d[_53]=nf;
}
}
}else{
if(_57=="dojo/aspect"){
_4f.push({method:_55,advice:_56,func:nf});
}else{
if(_57=="dojo/watch"){
_51.push({prop:_54,func:nf});
}else{
_50.push(nf);
}
}
}
}
}
var _58=_36.markupFactory||_3c.markupFactory;
var _59=_58?_58(_3d,_37,_36):new _36(_3d,_37);
if(_48){
_3.setObject(_48,_59);
}
for(i=0;i<_4f.length;i++){
_a[_4f[i].advice||"after"](_59,_4f[i].method,_3.hitch(_59,_4f[i].func),true);
}
for(i=0;i<_50.length;i++){
_50[i].call(_59);
}
for(i=0;i<_51.length;i++){
_59.watch(_51[i].prop,_51[i].func);
}
for(i=0;i<ons.length;i++){
_f(_59,ons[i].event,ons[i].func);
}
return _59;
},scan:function(_5a,_5b){
var _5c=[],_5d=[],_5e={};
var _5f=(_5b.scope||_2._scopeName)+"Type",_60="data-"+(_5b.scope||_2._scopeName)+"-",_61=_60+"type",_62=_60+"textdir",_63=_60+"mixins";
var _64=_5a.firstChild;
var _65=_5b.inherited;
if(!_65){
function _66(_67,_68){
return (_67.getAttribute&&_67.getAttribute(_68))||(_67.parentNode&&_66(_67.parentNode,_68));
};
_65={dir:_66(_5a,"dir"),lang:_66(_5a,"lang"),textDir:_66(_5a,_62)};
for(var key in _65){
if(!_65[key]){
delete _65[key];
}
}
}
var _69={inherited:_65};
var _6a;
var _6b;
function _6c(_6d){
if(!_6d.inherited){
_6d.inherited={};
var _6e=_6d.node,_6f=_6c(_6d.parent);
var _70={dir:_6e.getAttribute("dir")||_6f.dir,lang:_6e.getAttribute("lang")||_6f.lang,textDir:_6e.getAttribute(_62)||_6f.textDir};
for(var key in _70){
if(_70[key]){
_6d.inherited[key]=_70[key];
}
}
}
return _6d.inherited;
};
while(true){
if(!_64){
if(!_69||!_69.node){
break;
}
_64=_69.node.nextSibling;
_6b=false;
_69=_69.parent;
_6a=_69.scripts;
continue;
}
if(_64.nodeType!=1){
_64=_64.nextSibling;
continue;
}
if(_6a&&_64.nodeName.toLowerCase()=="script"){
_71=_64.getAttribute("type");
if(_71&&/^dojo\/\w/i.test(_71)){
_6a.push(_64);
}
_64=_64.nextSibling;
continue;
}
if(_6b){
_64=_64.nextSibling;
continue;
}
var _71=_64.getAttribute(_61)||_64.getAttribute(_5f);
var _72=_64.firstChild;
if(!_71&&(!_72||(_72.nodeType==3&&!_72.nextSibling))){
_64=_64.nextSibling;
continue;
}
var _73;
var _74=null;
if(_71){
var _75=_64.getAttribute(_63),_76=_75?[_71].concat(_75.split(/\s*,\s*/)):[_71];
try{
_74=_17(_76);
}
catch(e){
}
if(!_74){
_4.forEach(_76,function(t){
if(~t.indexOf("/")&&!_5e[t]){
_5e[t]=true;
_5d[_5d.length]=t;
}
});
}
var _77=_74&&!_74.prototype._noScript?[]:null;
_73={types:_76,ctor:_74,parent:_69,node:_64,scripts:_77};
_73.inherited=_6c(_73);
_5c.push(_73);
}else{
_73={node:_64,scripts:_6a,parent:_69};
}
_64=_72;
_6a=_77;
_6b=_74&&_74.prototype.stopParser&&!(_5b.template);
_69=_73;
}
var d=new _c();
if(_5d.length){
if(_d("dojo-debug-messages")){
console.warn("WARNING: Modules being Auto-Required: "+_5d.join(", "));
}
_1(_5d,function(){
d.resolve(_4.filter(_5c,function(_78){
if(!_78.ctor){
try{
_78.ctor=_17(_78.types);
}
catch(e){
}
}
var _79=_78.parent;
while(_79&&!_79.types){
_79=_79.parent;
}
var _7a=_78.ctor&&_78.ctor.prototype;
_78.instantiateChildren=!(_7a&&_7a.stopParser&&!(_5b.template));
_78.instantiate=!_79||(_79.instantiate&&_79.instantiateChildren);
return _78.instantiate;
}));
});
}else{
d.resolve(_5c);
}
return d.promise;
},_require:function(_7b){
var _7c=_9.fromJson("{"+_7b.innerHTML+"}"),_7d=[],_7e=[],d=new _c();
for(var _7f in _7c){
_7d.push(_7f);
_7e.push(_7c[_7f]);
}
_1(_7e,function(){
for(var i=0;i<_7d.length;i++){
_3.setObject(_7d[i],arguments[i]);
}
d.resolve(arguments);
});
return d.promise;
},_scanAmd:function(_80){
var _81=new _c(),_82=_81.promise;
_81.resolve(true);
var _83=this;
_e("script[type='dojo/require']",_80).forEach(function(_84){
_82=_82.then(function(){
return _83._require(_84);
});
_84.parentNode.removeChild(_84);
});
return _82;
},parse:function(_85,_86){
if(_85&&typeof _85!="string"&&!("nodeType" in _85)){
_86=_85;
_85=_86.rootNode;
}
var _87=_85?_6.byId(_85):_7.body();
_86=_86||{};
var _88=_86.template?{template:true}:{},_89=[],_8a=this;
var p=this._scanAmd(_87,_86).then(function(){
return _8a.scan(_87,_86);
}).then(function(_8b){
return _89=_89.concat(_8a._instantiate(_8b,_88,_86));
}).otherwise(function(e){
console.error("dojo/parser::parse() error",e);
throw e;
});
_3.mixin(_89,p);
return _89;
}};
if(1){
_2.parser=_1b;
}
if(_5.parseOnLoad){
_10(100,_1b,"parse");
}
return _1b;
});
