//>>built
define("dojox/dtl/_Templated",["dojo/_base/declare","./_base","dijit/_TemplatedMixin","dojo/dom-construct","dojo/cache","dojo/_base/array","dojo/string","dojo/parser"],function(_1,dd,_2,_3,_4,_5,_6,_7){
return _1("dojox.dtl._Templated",_2,{_dijitTemplateCompat:false,buildRendering:function(){
var _8;
if(this.domNode&&!this._template){
return;
}
if(!this._template){
var t=this.getCachedTemplate(this.templatePath,this.templateString,this._skipNodeCache);
if(t instanceof dd.Template){
this._template=t;
}else{
_8=t.cloneNode(true);
}
}
if(!_8){
var _9=new dd._Context(this);
if(!this._created){
delete _9._getter;
}
var _a=_3.toDom(this._template.render(_9));
if(_a.nodeType!==1&&_a.nodeType!==3){
for(var i=0,l=_a.childNodes.length;i<l;++i){
_8=_a.childNodes[i];
if(_8.nodeType==1){
break;
}
}
}else{
_8=_a;
}
}
this._attachTemplateNodes(_8,function(n,p){
return n.getAttribute(p);
});
if(this.widgetsInTemplate){
var _b=_7,_c,_d;
if(_b._query!="[dojoType]"){
_c=_b._query;
_d=_b._attrName;
_b._query="[dojoType]";
_b._attrName="dojoType";
}
var cw=(this._startupWidgets=_7.parse(_8,{noStart:!this._earlyTemplatedStartup,inherited:{dir:this.dir,lang:this.lang}}));
if(_c){
_b._query=_c;
_b._attrName=_d;
}
this._attachTemplateNodes(cw,function(n,p){
return n[p];
});
}
if(this.domNode){
_3.place(_8,this.domNode,"before");
this.destroyDescendants();
_3.destroy(this.domNode);
}
this.domNode=_8;
this._fillContent(this.srcNodeRef);
},_templateCache:{},getCachedTemplate:function(_e,_f,_10){
var _11=this._templateCache;
var key=_f||_e;
if(_11[key]){
return _11[key];
}
_f=_6.trim(_f||_4(_e,{sanitize:true}));
if(this._dijitTemplateCompat&&(_10||_f.match(/\$\{([^\}]+)\}/g))){
_f=this._stringRepl(_f);
}
if(_10||!_f.match(/\{[{%]([^\}]+)[%}]\}/g)){
return _11[key]=_3.toDom(_f);
}else{
return _11[key]=new dd.Template(_f);
}
},render:function(){
this.buildRendering();
},startup:function(){
_5.forEach(this._startupWidgets,function(w){
if(w&&!w._started&&w.startup){
w.startup();
}
});
this.inherited(arguments);
}});
});
