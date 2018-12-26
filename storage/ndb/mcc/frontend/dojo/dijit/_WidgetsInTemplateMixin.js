//>>built
define("dijit/_WidgetsInTemplateMixin",["dojo/_base/array","dojo/_base/declare","dojo/parser","dijit/registry"],function(_1,_2,_3,_4){
return _2("dijit._WidgetsInTemplateMixin",null,{_earlyTemplatedStartup:false,widgetsInTemplate:true,_beforeFillContent:function(){
if(this.widgetsInTemplate){
var _5=this.domNode;
var cw=(this._startupWidgets=_3.parse(_5,{noStart:!this._earlyTemplatedStartup,template:true,inherited:{dir:this.dir,lang:this.lang,textDir:this.textDir},propsThis:this,scope:"dojo"}));
this._supportingWidgets=_4.findWidgets(_5);
this._attachTemplateNodes(cw,function(n,p){
return n[p];
});
}
},startup:function(){
_1.forEach(this._startupWidgets,function(w){
if(w&&!w._started&&w.startup){
w.startup();
}
});
this.inherited(arguments);
}});
});
