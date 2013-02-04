//>>built
require({cache:{"url:dojox/form/resources/PasswordValidator.html":"<div dojoAttachPoint=\"containerNode\">\n\t<input type=\"hidden\" name=\"${name}\" value=\"\" dojoAttachPoint=\"focusNode\" />\n</div>"}});
define("dojox/form/PasswordValidator",["dojo/_base/array","dojo/_base/lang","dojo/dom-attr","dojo/i18n","dojo/query","dojo/keys","dijit/form/_FormValueWidget","dijit/form/ValidationTextBox","dojo/text!./resources/PasswordValidator.html","dojo/i18n!./nls/PasswordValidator","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_b("dojox.form._ChildTextBox",_8,{containerWidget:null,type:"password",reset:function(){
_8.prototype._setValueAttr.call(this,"",true);
this._hasBeenBlurred=false;
},postCreate:function(){
this.inherited(arguments);
if(!this.name){
_3.remove(this.focusNode,"name");
}
this.connect(this.focusNode,"onkeypress","_onChildKeyPress");
},_onChildKeyPress:function(e){
if(e&&e.keyCode==_6.ENTER){
this._setBlurValue();
}
}});
var _d=_b("dojox.form._OldPWBox",_c,{_isPWValid:false,_setValueAttr:function(_e,_f){
if(_e===""){
_e=_d.superclass.attr.call(this,"value");
}
if(_f!==null){
this._isPWValid=this.containerWidget.pwCheck(_e);
}
this.inherited(arguments);
this.containerWidget._childValueAttr(this.containerWidget._inputWidgets[1].get("value"));
},isValid:function(_10){
return this.inherited("isValid",arguments)&&this._isPWValid;
},_update:function(e){
if(this._hasBeenBlurred){
this.validate(true);
}
this._onMouse(e);
},_getValueAttr:function(){
if(this.containerWidget._started&&this.containerWidget.isValid()){
return this.inherited(arguments);
}
return "";
},_setBlurValue:function(){
var _11=_8.prototype._getValueAttr.call(this);
this._setValueAttr(_11,(this.isValid?this.isValid():true));
}});
var _12=_b("dojox.form._NewPWBox",_c,{required:true,onChange:function(){
this.containerWidget._inputWidgets[2].validate(false);
this.inherited(arguments);
}});
var _13=_b("dojox.form._VerifyPWBox",_c,{isValid:function(_14){
return this.inherited("isValid",arguments)&&(this.get("value")==this.containerWidget._inputWidgets[1].get("value"));
}});
return _b("dojox.form.PasswordValidator",_7,{required:true,_inputWidgets:null,oldName:"",templateString:_9,_hasBeenBlurred:false,isValid:function(_15){
return _1.every(this._inputWidgets,function(i){
if(i&&i._setStateClass){
i._setStateClass();
}
return (!i||i.isValid());
});
},validate:function(_16){
return _1.every(_1.map(this._inputWidgets,function(i){
if(i&&i.validate){
i._hasBeenBlurred=(i._hasBeenBlurred||this._hasBeenBlurred);
return i.validate();
}
return true;
},this),function(_17){
return _17;
});
},reset:function(){
this._hasBeenBlurred=false;
_1.forEach(this._inputWidgets,function(i){
if(i&&i.reset){
i.reset();
}
},this);
},_createSubWidgets:function(){
var _18=this._inputWidgets,msg=_4.getLocalization("dojox.form","PasswordValidator",this.lang);
_1.forEach(_18,function(i,idx){
if(i){
var p={containerWidget:this},c;
if(idx===0){
p.name=this.oldName;
p.invalidMessage=msg.badPasswordMessage;
c=_d;
}else{
if(idx===1){
p.required=this.required;
c=_12;
}else{
if(idx===2){
p.invalidMessage=msg.nomatchMessage;
c=_13;
}
}
}
_18[idx]=new c(p,i);
}
},this);
},pwCheck:function(_19){
return false;
},postCreate:function(){
this.inherited(arguments);
var _1a=this._inputWidgets=[];
_1.forEach(["old","new","verify"],function(i){
_1a.push(_5("input[pwType="+i+"]",this.containerNode)[0]);
},this);
if(!_1a[1]||!_1a[2]){
throw new Error("Need at least pwType=\"new\" and pwType=\"verify\"");
}
if(this.oldName&&!_1a[0]){
throw new Error("Need to specify pwType=\"old\" if using oldName");
}
this.containerNode=this.domNode;
this._createSubWidgets();
this.connect(this._inputWidgets[1],"_setValueAttr","_childValueAttr");
this.connect(this._inputWidgets[2],"_setValueAttr","_childValueAttr");
},_childValueAttr:function(v){
this.set("value",this.isValid()?v:"");
},_setDisabledAttr:function(_1b){
this.inherited(arguments);
_1.forEach(this._inputWidgets,function(i){
if(i&&i.set){
i.set("disabled",_1b);
}
});
},_setRequiredAttribute:function(_1c){
this.required=_1c;
_3.set(this.focusNode,"required",_1c);
this.focusNode.setAttribute("aria-required",_1c);
this._refreshState();
_1.forEach(this._inputWidgets,function(i){
if(i&&i.set){
i.set("required",_1c);
}
});
},_setValueAttr:function(v){
this.inherited(arguments);
_3.set(this.focusNode,"value",v);
},_getValueAttr:function(){
return this.value||"";
},focus:function(){
var f=false;
_1.forEach(this._inputWidgets,function(i){
if(i&&!i.isValid()&&!f){
i.focus();
f=true;
}
});
if(!f){
this._inputWidgets[1].focus();
}
}});
});
