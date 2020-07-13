//>>built
define("dojox/widget/WizardPane",["dojo/_base/lang","dojo/_base/declare","dijit/layout/ContentPane"],function(_1,_2,_3){
return _2("dojox.widget.WizardPane",_3,{canGoBack:true,passFunction:null,doneFunction:null,startup:function(){
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
var _4=this.passFunction();
switch(typeof _4){
case "boolean":
r=_4;
break;
case "string":
alert(_4);
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
});
