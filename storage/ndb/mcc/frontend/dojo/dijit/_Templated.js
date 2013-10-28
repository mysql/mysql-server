//>>built
define("dijit/_Templated",["./_WidgetBase","./_TemplatedMixin","./_WidgetsInTemplateMixin","dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/kernel"],function(_1,_2,_3,_4,_5,_6,_7){
_6.extend(_1,{waiRole:"",waiState:""});
return _5("dijit._Templated",[_2,_3],{widgetsInTemplate:false,constructor:function(){
_7.deprecated(this.declaredClass+": dijit._Templated deprecated, use dijit._TemplatedMixin and if necessary dijit._WidgetsInTemplateMixin","","2.0");
},_attachTemplateNodes:function(_8,_9){
this.inherited(arguments);
var _a=_6.isArray(_8)?_8:(_8.all||_8.getElementsByTagName("*"));
var x=_6.isArray(_8)?0:-1;
for(;x<_a.length;x++){
var _b=(x==-1)?_8:_a[x];
var _c=_9(_b,"waiRole");
if(_c){
_b.setAttribute("role",_c);
}
var _d=_9(_b,"waiState");
if(_d){
_4.forEach(_d.split(/\s*,\s*/),function(_e){
if(_e.indexOf("-")!=-1){
var _f=_e.split("-");
_b.setAttribute("aria-"+_f[0],_f[1]);
}
});
}
}
}});
});
