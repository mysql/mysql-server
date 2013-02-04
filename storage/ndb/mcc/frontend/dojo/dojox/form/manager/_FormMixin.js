//>>built
define("dojox/form/manager/_FormMixin",["dojo/_base/lang","dojo/_base/kernel","dojo/_base/event","dojo/window","./_Mixin","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6){
var fm=_1.getObject("dojox.form.manager",true),aa=fm.actionAdapter;
return _6("dojox.form.manager._FormMixin",null,{name:"",action:"",method:"",encType:"","accept-charset":"",accept:"",target:"",startup:function(){
this.isForm=this.domNode.tagName.toLowerCase()=="form";
if(this.isForm){
this.connect(this.domNode,"onreset","_onReset");
this.connect(this.domNode,"onsubmit","_onSubmit");
}
this.inherited(arguments);
},_onReset:function(_7){
var _8={returnValue:true,preventDefault:function(){
this.returnValue=false;
},stopPropagation:function(){
},currentTarget:_7.currentTarget,target:_7.target};
if(!(this.onReset(_8)===false)&&_8.returnValue){
this.reset();
}
_3.stop(_7);
return false;
},onReset:function(){
return true;
},reset:function(){
this.inspectFormWidgets(aa(function(_9,_a){
if(_a.reset){
_a.reset();
}
}));
if(this.isForm){
this.domNode.reset();
}
return this;
},_onSubmit:function(_b){
if(this.onSubmit(_b)===false){
_3.stop(_b);
}
},onSubmit:function(){
return this.isValid();
},submit:function(){
if(this.isForm){
if(!(this.onSubmit()===false)){
this.domNode.submit();
}
}
},isValid:function(){
for(var _c in this.formWidgets){
var _d=false;
aa(function(_e,_f){
if(!_f.get("disabled")&&_f.isValid&&!_f.isValid()){
_d=true;
}
}).call(this,null,this.formWidgets[_c].widget);
if(_d){
return false;
}
}
return true;
},validate:function(){
var _10=true,_11=this.formWidgets,_12=false,_13;
for(_13 in _11){
aa(function(_14,_15){
_15._hasBeenBlurred=true;
var _16=_15.disabled||!_15.validate||_15.validate();
if(!_16&&!_12){
_4.scrollIntoView(_15.containerNode||_15.domNode);
_15.focus();
_12=true;
}
_10=_10&&_16;
}).call(this,null,_11[_13].widget);
}
return _10;
}});
});
