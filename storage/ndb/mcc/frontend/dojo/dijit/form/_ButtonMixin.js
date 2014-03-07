//>>built
define("dijit/form/_ButtonMixin",["dojo/_base/declare","dojo/dom","dojo/_base/event","../registry"],function(_1,_2,_3,_4){
return _1("dijit.form._ButtonMixin",null,{label:"",type:"button",_onClick:function(e){
if(this.disabled){
_3.stop(e);
return false;
}
var _5=this.onClick(e)===false;
if(!_5&&this.type=="submit"&&!(this.valueNode||this.focusNode).form){
for(var _6=this.domNode;_6.parentNode;_6=_6.parentNode){
var _7=_4.byNode(_6);
if(_7&&typeof _7._onSubmit=="function"){
_7._onSubmit(e);
_5=true;
break;
}
}
}
if(_5){
e.preventDefault();
}
return !_5;
},postCreate:function(){
this.inherited(arguments);
_2.setSelectable(this.focusNode,false);
},onClick:function(){
return true;
},_setLabelAttr:function(_8){
this._set("label",_8);
(this.containerNode||this.focusNode).innerHTML=_8;
}});
});
