/*
This file was modified by Oracle on 2019-05-23.
We first make button state "BUSY" and only then call onClick. This prevents
button from receiving multiple clicks while already triggered.
Modifications copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
*/
//>>built
define("dojox/form/BusyButton",["dojo/_base/lang","dojo/dom-attr","dojo/dom-class","dijit/form/Button","dijit/form/DropDownButton","dijit/form/ComboButton","dojo/i18n","dojo/i18n!dijit/nls/loading","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_9("dojox.form._BusyButtonMixin",null,{isBusy:false,busyLabel:"",timeout:null,useIcon:true,postMixInProperties:function(){
this.inherited(arguments);
if(!this.busyLabel){
this.busyLabel=_7.getLocalization("dijit","loading",this.lang).loadingState;
}
},postCreate:function(){
this.inherited(arguments);
this._label=this.containerNode.innerHTML;
this._initTimeout=this.timeout;
if(this.isBusy){
this.makeBusy();
}
},makeBusy:function(){
this.isBusy=true;
if(this._disableHandle){
this._disableHandle.remove();
}
this._disableHandle=this.defer(function(){
this.set("disabled",true);
});
this.setLabel(this.busyLabel,this.timeout);
},cancel:function(){
if(this._disableHandle){
this._disableHandle.remove();
}
this.set("disabled",false);
this.isBusy=false;
this.setLabel(this._label);
if(this._timeout){
clearTimeout(this._timeout);
}
this.timeout=this._initTimeout;
},resetTimeout:function(_b){
if(this._timeout){
clearTimeout(this._timeout);
}
if(_b){
this._timeout=setTimeout(_1.hitch(this,function(){
this.cancel();
}),_b);
}else{
if(_b==undefined||_b===0){
this.cancel();
}
}
},setLabel:function(_c,_d){
this.label=_c;
while(this.containerNode.firstChild){
this.containerNode.removeChild(this.containerNode.firstChild);
}
this.containerNode.appendChild(document.createTextNode(this.label));
if(this.showLabel==false&&!_2.get(this.domNode,"title")){
this.titleNode.title=_1.trim(this.containerNode.innerText||this.containerNode.textContent||"");
}
if(_d){
this.resetTimeout(_d);
}else{
this.timeout=null;
}
if(this.useIcon&&this.isBusy){
var _e=new Image();
_e.src=this._blankGif;
_2.set(_e,"id",this.id+"_icon");
_3.add(_e,"dojoxBusyButtonIcon");
this.containerNode.appendChild(_e);
}
},_onClick:function(e){
if(!this.isBusy){
this.makeBusy();
this.inherited(arguments);
}
}});
var _f=_9("dojox.form.BusyButton",[_4,_a],{});
_9("dojox.form.BusyComboButton",[_6,_a],{});
_9("dojox.form.BusyDropDownButton",[_5,_a],{});
return _f;
});
