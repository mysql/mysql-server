//>>built
require({cache:{"url:dojox/widget/Wizard/Wizard.html":"<div class=\"dojoxWizard\" dojoAttachPoint=\"wizardNode\">\n    <div class=\"dojoxWizardContainer\" dojoAttachPoint=\"containerNode\"></div>\n    <div class=\"dojoxWizardButtons\" dojoAttachPoint=\"wizardNav\">\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"previousButton\">${previousButtonLabel}</button>\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"nextButton\">${nextButtonLabel}</button>\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"doneButton\" style=\"display:none\">${doneButtonLabel}</button>\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"cancelButton\">${cancelButtonLabel}</button>\n    </div>\n</div>\n"}});
define("dojox/widget/WizardPane",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dijit/layout/StackContainer","dijit/layout/ContentPane","dijit/form/Button","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dojo/i18n","dojo/text!./Wizard/Wizard.html","dojo/i18n!dijit/nls/common","dojo/i18n!./nls/Wizard","dojox/widget/WizardPane"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_2("dojox.widget.WizardPane",_5,{canGoBack:true,passFunction:null,doneFunction:null,startup:function(){
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
var _d=this.passFunction();
switch(typeof _d){
case "boolean":
r=_d;
break;
case "string":
alert(_d);
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
return _c;
});
