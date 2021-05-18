//>>built
require({cache:{"url:dojox/widget/Wizard/Wizard.html":"<div class=\"dojoxWizard\" dojoAttachPoint=\"wizardNode\">\n    <div class=\"dojoxWizardContainer\" dojoAttachPoint=\"containerNode\"></div>\n    <div class=\"dojoxWizardButtons\" dojoAttachPoint=\"wizardNav\">\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"previousButton\">${previousButtonLabel}</button>\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"nextButton\">${nextButtonLabel}</button>\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"doneButton\" style=\"display:none\">${doneButtonLabel}</button>\n        <button dojoType=\"dijit.form.Button\" type=\"button\" dojoAttachPoint=\"cancelButton\">${cancelButtonLabel}</button>\n    </div>\n</div>\n"}});
define("dojox/widget/Wizard",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dijit/layout/StackContainer","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dojo/i18n","dojo/text!./Wizard/Wizard.html","dojo/i18n!dijit/nls/common","dojo/i18n!./nls/Wizard","dijit/form/Button"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_2("dojox.widget.Wizard",[_4,_5,_6],{templateString:_8,nextButtonLabel:"",previousButtonLabel:"",cancelButtonLabel:"",doneButtonLabel:"",cancelFunction:null,hideDisabled:false,postMixInProperties:function(){
this.inherited(arguments);
var _a=_1.mixin({cancel:_7.getLocalization("dijit","common",this.lang).buttonCancel},_7.getLocalization("dojox.widget","Wizard",this.lang));
var _b;
for(_b in _a){
if(!this[_b+"ButtonLabel"]){
this[_b+"ButtonLabel"]=_a[_b];
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
var _c=sw.isLastChild||this.nextButton.get("disabled");
this.nextButton.set("disabled",_c);
this._setButtonClass(this.nextButton);
if(sw.doneFunction){
this.doneButton.domNode.style.display="";
if(_c){
this.nextButton.domNode.style.display="none";
}
}else{
this.doneButton.domNode.style.display="none";
}
this.previousButton.set("disabled",!this.selectedChildWidget.canGoBack);
this._setButtonClass(this.previousButton);
},_setButtonClass:function(_d){
_d.domNode.style.display=(this.hideDisabled&&_d.disabled)?"none":"";
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
return _9;
});
