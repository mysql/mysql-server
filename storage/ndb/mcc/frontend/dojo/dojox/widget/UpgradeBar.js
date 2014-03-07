//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/window,dojo/fx,dojo/cookie,dijit/_Widget,dijit/_Templated"],function(_1,_2,_3){
_2.provide("dojox.widget.UpgradeBar");
_2.require("dojo.window");
_2.require("dojo.fx");
_2.require("dojo.cookie");
_2.require("dijit._Widget");
_2.require("dijit._Templated");
_2.experimental("dojox.widget.UpgradeBar");
_2.declare("dojox.widget.UpgradeBar",[_1._Widget,_1._Templated],{notifications:[],buttonCancel:"Close for now",noRemindButton:"Don't Remind Me Again",templateString:_2.cache("dojox.widget","UpgradeBar/UpgradeBar.html","<div class=\"dojoxUpgradeBar\">\n\t<div class=\"dojoxUpgradeBarMessage\" dojoAttachPoint=\"messageNode\">message</div>\n\t<div class=\"dojoxUpgradeBarReminderButton\" dojoAttachPoint=\"dontRemindButtonNode\" dojoAttachEvent=\"onclick:_onDontRemindClick\">${noRemindButton}</div>\n\t<span dojoAttachPoint=\"closeButtonNode\" class=\"dojoxUpgradeBarCloseIcon\" dojoAttachEvent=\"onclick: hide, onmouseenter: _onCloseEnter, onmouseleave: _onCloseLeave\" title=\"${buttonCancel}\"></span>\n</div>"),constructor:function(_4,_5){
if(!_4.notifications&&_5){
_2.forEach(_5.childNodes,function(n){
if(n.nodeType==1){
var _6=_2.attr(n,"validate");
this.notifications.push({message:n.innerHTML,validate:function(){
var _7=true;
try{
_7=_2.eval(_6);
}
catch(e){
}
return _7;
}});
}
},this);
}
},checkNotifications:function(){
if(!this.notifications.length){
return;
}
for(var i=0;i<this.notifications.length;i++){
var _8=this.notifications[i].validate();
if(_8){
this.notify(this.notifications[i].message);
break;
}
}
},postCreate:function(){
this.inherited(arguments);
if(this.domNode.parentNode){
_2.style(this.domNode,"display","none");
}
_2.mixin(this.attributeMap,{message:{node:"messageNode",type:"innerHTML"}});
if(!this.noRemindButton){
_2.destroy(this.dontRemindButtonNode);
}
if(_2.isIE==6){
var _9=this;
var _a=function(){
var v=_2.window.getBox();
_2.style(_9.domNode,"width",v.w+"px");
};
this.connect(window,"resize",function(){
_a();
});
_a();
}
_2.addOnLoad(this,"checkNotifications");
},notify:function(_b){
if(_2.cookie("disableUpgradeReminders")){
return;
}
if(!this.domNode.parentNode||!this.domNode.parentNode.innerHTML){
document.body.appendChild(this.domNode);
}
_2.style(this.domNode,"display","");
if(_b){
this.set("message",_b);
}
},show:function(){
this._bodyMarginTop=_2.style(_2.body(),"marginTop");
this._size=_2.contentBox(this.domNode).h;
_2.style(this.domNode,{display:"block",height:0,opacity:0});
if(!this._showAnim){
this._showAnim=_2.fx.combine([_2.animateProperty({node:_2.body(),duration:500,properties:{marginTop:this._bodyMarginTop+this._size}}),_2.animateProperty({node:this.domNode,duration:500,properties:{height:this._size,opacity:1}})]);
}
this._showAnim.play();
},hide:function(){
if(!this._hideAnim){
this._hideAnim=_2.fx.combine([_2.animateProperty({node:_2.body(),duration:500,properties:{marginTop:this._bodyMarginTop}}),_2.animateProperty({node:this.domNode,duration:500,properties:{height:0,opacity:0}})]);
_2.connect(this._hideAnim,"onEnd",this,function(){
_2.style(this.domNode,"display","none");
});
}
this._hideAnim.play();
},_onDontRemindClick:function(){
_2.cookie("disableUpgradeReminders",true,{expires:3650});
this.hide();
},_onCloseEnter:function(){
_2.addClass(this.closeButtonNode,"dojoxUpgradeBarCloseIcon-hover");
},_onCloseLeave:function(){
_2.removeClass(this.closeButtonNode,"dojoxUpgradeBarCloseIcon-hover");
}});
});
