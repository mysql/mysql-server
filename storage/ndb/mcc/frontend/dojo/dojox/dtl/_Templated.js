//>>built
define("dojox/dtl/_Templated",["dojo/aspect","dojo/_base/declare","./_base","dijit/_TemplatedMixin","dojo/dom-construct","dojo/cache","dojo/_base/array","dojo/string","dojo/parser"],function(_1,_2,dd,_3,_4,_5,_6,_7,_8){
return _2("dojox.dtl._Templated",_3,{_dijitTemplateCompat:false,buildRendering:function(){
var _9;
if(this.domNode&&!this._template){
return;
}
if(!this._template){
var t=this.getCachedTemplate(this.templatePath,this.templateString,this._skipNodeCache);
if(t instanceof dd.Template){
this._template=t;
}else{
_9=t.cloneNode(true);
}
}
if(!_9){
var _a=new dd._Context(this);
if(!this._created){
delete _a._getter;
}
var _b=_4.toDom(this._template.render(_a));
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
this._attachTemplateNodes(_9);
if(this.widgetsInTemplate){
var _c=_8,_d,_e;
if(_c._query!="[dojoType]"){
_d=_c._query;
_e=_c._attrName;
_c._query="[dojoType]";
_c._attrName="dojoType";
}
var cw=(this._startupWidgets=_8.parse(_9,{noStart:!this._earlyTemplatedStartup,inherited:{dir:this.dir,lang:this.lang}}));
if(_d){
_c._query=_d;
_c._attrName=_e;
}
for(var i=0;i<cw.length;i++){
this._processTemplateNode(cw[i],function(n,p){
return n[p];
},function(_f,_10,_11){
if(_10 in _f){
return _1.after(_f,_10,_11,true);
}else{
return _f.on(_10,_11,true);
}
});
}
}
if(this.domNode){
_4.place(_9,this.domNode,"before");
this.destroyDescendants();
_4.destroy(this.domNode);
}
this.domNode=_9;
this._fillContent(this.srcNodeRef);
},_processTemplateNode:function(_12,_13,_14){
if(this.widgetsInTemplate&&(_13(_12,"dojoType")||_13(_12,"data-dojo-type"))){
return true;
}
this.inherited(arguments);
},_templateCache:{},getCachedTemplate:function(_15,_16,_17){
var _18=this._templateCache;
var key=_16||_15;
if(_18[key]){
return _18[key];
}
_16=_7.trim(_16||_5(_15,{sanitize:true}));
if(this._dijitTemplateCompat&&(_17||_16.match(/\$\{([^\}]+)\}/g))){
_16=this._stringRepl(_16);
}
if(_17||!_16.match(/\{[{%]([^\}]+)[%}]\}/g)){
return _18[key]=_4.toDom(_16);
}else{
return _18[key]=new dd.Template(_16);
}
},render:function(){
this.buildRendering();
},startup:function(){
_6.forEach(this._startupWidgets,function(w){
if(w&&!w._started&&w.startup){
w.startup();
}
});
this.inherited(arguments);
}});
});
