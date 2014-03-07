//>>built
define("dijit/form/Form",["dojo/_base/declare","dojo/dom-attr","dojo/_base/event","dojo/_base/kernel","dojo/_base/sniff","../_Widget","../_TemplatedMixin","./_FormMixin","../layout/_ContentPaneResizeMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _1("dijit.form.Form",[_6,_7,_8,_9],{name:"",action:"",method:"",encType:"","accept-charset":"",accept:"",target:"",templateString:"<form data-dojo-attach-point='containerNode' data-dojo-attach-event='onreset:_onReset,onsubmit:_onSubmit' ${!nameAttrSetting}></form>",postMixInProperties:function(){
this.nameAttrSetting=this.name?("name='"+this.name+"'"):"";
this.inherited(arguments);
},execute:function(){
},onExecute:function(){
},_setEncTypeAttr:function(_a){
this.encType=_a;
_2.set(this.domNode,"encType",_a);
if(_5("ie")){
this.domNode.encoding=_a;
}
},reset:function(e){
var _b={returnValue:true,preventDefault:function(){
this.returnValue=false;
},stopPropagation:function(){
},currentTarget:e?e.target:this.domNode,target:e?e.target:this.domNode};
if(!(this.onReset(_b)===false)&&_b.returnValue){
this.inherited(arguments,[]);
}
},onReset:function(){
return true;
},_onReset:function(e){
this.reset(e);
_3.stop(e);
return false;
},_onSubmit:function(e){
var fp=this.constructor.prototype;
if(this.execute!=fp.execute||this.onExecute!=fp.onExecute){
_4.deprecated("dijit.form.Form:execute()/onExecute() are deprecated. Use onSubmit() instead.","","2.0");
this.onExecute();
this.execute(this.getValues());
}
if(this.onSubmit(e)===false){
_3.stop(e);
}
},onSubmit:function(){
return this.isValid();
},submit:function(){
if(!(this.onSubmit()===false)){
this.containerNode.submit();
}
}});
});
