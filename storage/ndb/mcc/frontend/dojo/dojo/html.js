/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/html",["./_base/kernel","./_base/lang","./_base/array","./_base/declare","./dom","./dom-construct","./parser"],function(_1,_2,_3,_4,_5,_6,_7){
_2.getObject("html",true,_1);
var _8=0;
_1.html._secureForInnerHtml=function(_9){
return _9.replace(/(?:\s*<!DOCTYPE\s[^>]+>|<title[^>]*>[\s\S]*?<\/title>)/ig,"");
};
_1.html._emptyNode=_6.empty;
_1.html._setNodeContent=function(_a,_b){
_6.empty(_a);
if(_b){
if(typeof _b=="string"){
_b=_6.toDom(_b,_a.ownerDocument);
}
if(!_b.nodeType&&_2.isArrayLike(_b)){
for(var _c=_b.length,i=0;i<_b.length;i=_c==_b.length?i+1:0){
_6.place(_b[i],_a,"last");
}
}else{
_6.place(_b,_a,"last");
}
}
return _a;
};
_4("dojo.html._ContentSetter",null,{node:"",content:"",id:"",cleanContent:false,extractContent:false,parseContent:false,parserScope:_1._scopeName,startup:true,constructor:function(_d,_e){
_2.mixin(this,_d||{});
_e=this.node=_5.byId(this.node||_e);
if(!this.id){
this.id=["Setter",(_e)?_e.id||_e.tagName:"",_8++].join("_");
}
},set:function(_f,_10){
if(undefined!==_f){
this.content=_f;
}
if(_10){
this._mixin(_10);
}
this.onBegin();
this.setContent();
this.onEnd();
return this.node;
},setContent:function(){
var _11=this.node;
if(!_11){
throw new Error(this.declaredClass+": setContent given no node");
}
try{
_11=_1.html._setNodeContent(_11,this.content);
}
catch(e){
var _12=this.onContentError(e);
try{
_11.innerHTML=_12;
}
catch(e){
console.error("Fatal "+this.declaredClass+".setContent could not change content due to "+e.message,e);
}
}
this.node=_11;
},empty:function(){
if(this.parseResults&&this.parseResults.length){
_3.forEach(this.parseResults,function(w){
if(w.destroy){
w.destroy();
}
});
delete this.parseResults;
}
_1.html._emptyNode(this.node);
},onBegin:function(){
var _13=this.content;
if(_2.isString(_13)){
if(this.cleanContent){
_13=_1.html._secureForInnerHtml(_13);
}
if(this.extractContent){
var _14=_13.match(/<body[^>]*>\s*([\s\S]+)\s*<\/body>/im);
if(_14){
_13=_14[1];
}
}
}
this.empty();
this.content=_13;
return this.node;
},onEnd:function(){
if(this.parseContent){
this._parse();
}
return this.node;
},tearDown:function(){
delete this.parseResults;
delete this.node;
delete this.content;
},onContentError:function(err){
return "Error occured setting content: "+err;
},_mixin:function(_15){
var _16={},key;
for(key in _15){
if(key in _16){
continue;
}
this[key]=_15[key];
}
},_parse:function(){
var _17=this.node;
try{
var _18={};
_3.forEach(["dir","lang","textDir"],function(_19){
if(this[_19]){
_18[_19]=this[_19];
}
},this);
this.parseResults=_7.parse({rootNode:_17,noStart:!this.startup,inherited:_18,scope:this.parserScope});
}
catch(e){
this._onError("Content",e,"Error parsing in _ContentSetter#"+this.id);
}
},_onError:function(_1a,err,_1b){
var _1c=this["on"+_1a+"Error"].call(this,err);
if(_1b){
console.error(_1b,err);
}else{
if(_1c){
_1.html._setNodeContent(this.node,_1c,true);
}
}
}});
_1.html.set=function(_1d,_1e,_1f){
if(undefined==_1e){
console.warn("dojo.html.set: no cont argument provided, using empty string");
_1e="";
}
if(!_1f){
return _1.html._setNodeContent(_1d,_1e,true);
}else{
var op=new _1.html._ContentSetter(_2.mixin(_1f,{content:_1e,node:_1d}));
return op.set();
}
};
return _1.html;
});
