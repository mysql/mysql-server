//>>built
define("dijit/_editor/plugins/ToggleDir",["dojo/_base/declare","dojo/dom-style","dojo/_base/kernel","dojo/_base/lang","dojo/on","../_Plugin","../../form/ToggleButton"],function(_1,_2,_3,_4,on,_5,_6){
var _7=_1("dijit._editor.plugins.ToggleDir",_5,{useDefaultCommand:false,command:"toggleDir",buttonClass:_6,_initButton:function(){
this.inherited(arguments);
var _8=this.button,_9=this.editor.isLeftToRight();
this.own(this.button.on("change",_4.hitch(this,function(_a){
this.editor.set("textDir",_9^_a?"ltr":"rtl");
})));
var _b=_9?"ltr":"rtl";
function _c(_d){
_8.set("checked",_d&&_d!==_b,false);
};
_c(this.editor.get("textDir"));
this.editor.watch("textDir",function(_e,_f,_10){
_c(_10);
});
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
}});
_5.registry["toggleDir"]=function(){
return new _7({command:"toggleDir"});
};
return _7;
});
