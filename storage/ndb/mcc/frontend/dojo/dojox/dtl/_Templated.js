//>>built
define("dojox/dtl/_Templated",["dojo/_base/declare","./_base","dijit/_TemplatedMixin","dojo/dom-construct","dojo/cache","dojo/_base/array","dojo/string","dojo/parser","dijit/_base/manager"],function(_1,dd,_2,_3,_4,_5,_6,_7,_8){
return _1("dojox.dtl._Templated",_2,{_dijitTemplateCompat:false,buildRendering:function(){
var _9;
if(this.domNode&&!this._template){
return;
}
if(!this._template){
var t=this.getCachedTemplate(this.templatePath,this.templateString,this._skipNodeCache);
if(t instanceof dd.Template){
this._template=t;
}else{
_9=t;
}
}
if(!_9){
var _a=new dd._Context(this);
if(!this._created){
delete _a._getter;
}
var _b=_3.toDom(this._template.render(_a));
if(_b.nodeType!==1&&_b.nodeType!==3){
for(var i=0,l=_b.childNodes.length;i<l;++i){
_9=_b.childNodes[i];
if(_9.nodeType==1){
break;
}
}
}else{
_9=_b;
}
}
this._attachTemplateNodes(_9,function(n,p){
return n.getAttribute(p);
});
if(this.widgetsInTemplate){
var _c=_7,_d,_e;
if(_c._query!="[dojoType]"){
_d=_c._query;
_e=_c._attrName;
_c._query="[dojoType]";
_c._attrName="dojoType";
}
var cw=(this._startupWidgets=_7.parse(_9,{noStart:!this._earlyTemplatedStartup,inherited:{dir:this.dir,lang:this.lang}}));
if(_d){
_c._query=_d;
_c._attrName=_e;
}
this._supportingWidgets=_8.findWidgets(_9);
this._attachTemplateNodes(cw,function(n,p){
return n[p];
});
}
if(this.domNode){
_3.place(_9,this.domNode,"before");
this.destroyDescendants();
_3.destroy(this.domNode);
}
this.domNode=_9;
this._fillContent(this.srcNodeRef);
},_templateCache:{},getCachedTemplate:function(_f,_10,_11){
var _12=this._templateCache;
var key=_10||_f;
if(_12[key]){
return _12[key];
}
_10=_6.trim(_10||_4(_f,{sanitize:true}));
if(this._dijitTemplateCompat&&(_11||_10.match(/\$\{([^\}]+)\}/g))){
_10=this._stringRepl(_10);
}
if(_11||!_10.match(/\{[{%]([^\}]+)[%}]\}/g)){
return _12[key]=_3.toDom(_10);
}else{
return _12[key]=new dd.Template(_10);
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
