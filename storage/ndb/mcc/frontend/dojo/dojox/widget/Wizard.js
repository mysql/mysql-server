//>>built
require({cache:{"url:dojox/widget/Wizard/Wizard.html":"<div class=\"dojoxWizard\" dojoAttachPoint=\"wizardNode\">\n    <div class=\"dojoxWizardContainer\" dojoAttachPoint=\"containerNode\"></div>\n    <div class=\"dojoxWizardButtons\" dojoAttachPoint=\"wizardNav\">\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"previousButton\">${previousButtonLabel}</button>\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"nextButton\">${nextButtonLabel}</button>\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"doneButton\" style=\"display:none\">${doneButtonLabel}</button>\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"cancelButton\">${cancelButtonLabel}</button>\n    </div>\n</div>\n"}});
define("dojox/widget/Wizard",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dijit/layout/StackContainer","dijit/layout/ContentPane","dijit/form/Button","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dojo/i18n","dojo/text!./Wizard/Wizard.html","dojo/i18n!dijit/nls/common","dojo/i18n!./nls/Wizard"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var _b=_2("dojox.widget.Wizard",[_4,_7,_8],{templateString:_a,nextButtonLabel:"",previousButtonLabel:"",cancelButtonLabel:"",doneButtonLabel:"",cancelFunction:null,hideDisabled:false,postMixInProperties:function(){
this.inherited(arguments);
var _c=_1.mixin({cancel:_9.getLocalization("dijit","common",this.lang).buttonCancel},_9.getLocalization("dojox.widget","Wizard",this.lang));
var _d;
for(_d in _c){
if(!this[_d+"ButtonLabel"]){
this[_d+"ButtonLabel"]=_c[_d];
}
}
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
this.connect(this.nextButton,"onClick","_forward");
this.connect(this.previousButton,"onClick","back");
if(this.cancelFunction){
if(_1.isString(this.cancelFunction)){
this.cancelFunction=_1.getObject(this.cancelFunction);
}
this.connect(this.cancelButton,"onClick",this.cancelFunction);
}else{
this.cancelButton.domNode.style.display="none";
}
this.connect(this.doneButton,"onClick","done");
this._subscription=_3.subscribe(this.id+"-selectChild",_1.hitch(this,"_checkButtons"));
this._started=true;
},resize:function(){
this.inherited(arguments);
this._checkButtons();
},_checkButtons:function(){
var sw=this.selectedChildWidget;
var _e=sw.isLastChild;
this.nextButton.set("disabled",_e);
this._setButtonClass(this.nextButton);
if(sw.doneFunction){
this.doneButton.domNode.style.display="";
if(_e){
this.nextButton.domNode.style.display="none";
}
}else{
this.doneButton.domNode.style.display="none";
}
this.previousButton.set("disabled",!this.selectedChildWidget.canGoBack);
this._setButtonClass(this.previousButton);
},_setButtonClass:function(_f){
_f.domNode.style.display=(this.hideDisabled&&_f.disabled)?"none":"";
},_forward:function(){
if(this.selectedChildWidget._checkPass()){
this.forward();
}
},done:function(){
this.selectedChildWidget.done();
},destroy:function(){
_3.unsubscribe(this._subscription);
this.inherited(arguments);
}});
_2("dojox.widget.WizardPane",_5,{canGoBack:true,passFunction:null,doneFunction:null,startup:function(){
this.inherited(arguments);
if(this.isFirstChild){
this.canGoBack=false;
}
if(_1.isString(this.passFunction)){
this.passFunction=_1.getObject(this.passFunction);
}
if(_1.isString(this.doneFunction)&&this.doneFunction){
this.doneFunction=_1.getObject(this.doneFunction);
}
},_onShow:function(){
if(this.isFirstChild){
this.canGoBack=false;
}
this.inherited(arguments);
},_checkPass:function(){
var r=true;
if(this.passFunction&&_1.isFunction(this.passFunction)){
var _10=this.passFunction();
switch(typeof _10){
case "boolean":
r=_10;
break;
case "string":
alert(_10);
r=false;
break;
}
}
return r;
},done:function(){
if(this.doneFunction&&_1.isFunction(this.doneFunction)){
this.doneFunction();
}
}});
return _b;
});
