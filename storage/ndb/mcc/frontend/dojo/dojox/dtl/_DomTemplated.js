//>>built
define("dojox/dtl/_DomTemplated",["dojo/dom-construct",".","./contrib/dijit","./render/dom","dojo/cache","dijit/_TemplatedMixin"],function(_1,_2,_3,_4,_5,_6){
_2._DomTemplated=function(){
};
_2._DomTemplated.prototype={_dijitTemplateCompat:false,buildRendering:function(){
this.domNode=this.srcNodeRef||dojo.create("div");
if(!this._render){
var _7=_3.widgetsInTemplate;
_3.widgetsInTemplate=this.widgetsInTemplate;
this.template=this.template&&this.template!==true?this.template:this._getCachedTemplate(this.templatePath,this.templateString);
this._render=new _4.Render(this.domNode,this.template);
_3.widgetsInTemplate=_7;
}
var _8=this._getContext();
if(!this._created){
delete _8._getter;
}
this.render(_8);
this.domNode=this.template.getRootNode();
if(this.srcNodeRef&&this.srcNodeRef.parentNode){
_1.destroy(this.srcNodeRef);
delete this.srcNodeRef;
}
},setTemplate:function(_9,_a){
if(dojox.dtl.text._isTemplate(_9)){
this.template=this._getCachedTemplate(null,_9);
}else{
this.template=this._getCachedTemplate(_9);
}
this.render(_a);
},render:function(_b,_c){
if(_c){
this.template=_c;
}
this._render.render(this._getContext(_b),this.template);
},_getContext:function(_d){
if(!(_d instanceof dojox.dtl.Context)){
_d=false;
}
_d=_d||new dojox.dtl.Context(this);
_d.setThis(this);
return _d;
},_getCachedTemplate:function(_e,_f){
if(!this._templates){
this._templates={};
}
if(!_f){
_f=_5(_e,{sanitize:true});
}
var key=_f;
var _10=this._templates;
if(_10[key]){
return _10[key];
}
return (_10[key]=new dojox.dtl.DomTemplate(_6.getCachedTemplate(_f,true)));
}};
return _2._DomTemplated;
});
