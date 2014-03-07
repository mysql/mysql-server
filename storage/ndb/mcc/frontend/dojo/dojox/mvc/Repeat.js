//>>built
define("dojox/mvc/Repeat",["dojo/_base/declare","dojo/dom","./_Container"],function(_1,_2,_3){
return _1("dojox.mvc.Repeat",[_3],{index:0,postscript:function(_4,_5){
this.srcNodeRef=_2.byId(_5);
if(this.srcNodeRef){
if(this.templateString==""){
this.templateString=this.srcNodeRef.innerHTML;
}
this.srcNodeRef.innerHTML="";
}
this.inherited(arguments);
},_updateBinding:function(_6,_7,_8){
this.inherited(arguments);
this._buildContained();
},_buildContained:function(){
this._destroyBody();
this._updateAddRemoveWatch();
var _9="";
for(this.index=0;this.get("binding").get(this.index);this.index++){
_9+=this._exprRepl(this.templateString);
}
var _a=this.srcNodeRef||this.domNode;
_a.innerHTML=_9;
this.srcNodeRef=_a;
this._createBody();
},_updateAddRemoveWatch:function(){
if(this._addRemoveWatch){
this._addRemoveWatch.unwatch();
}
var _b=this;
this._addRemoveWatch=this.get("binding").watch(function(_c,_d,_e){
if(/^[0-9]+$/.test(_c.toString())){
if(!_d||!_e){
_b._buildContained();
}
}
});
}});
});
