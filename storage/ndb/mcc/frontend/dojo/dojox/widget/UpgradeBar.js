//>>built
define("dojox/widget/UpgradeBar",["dojo/_base/kernel","dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/fx","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/cache","dojo/cookie","dojo/domReady","dojo/fx","dojo/window","dijit/_WidgetBase","dijit/_TemplatedMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,fx,win,_11,_12){
_1.experimental("dojox.widget.UpgradeBar");
var _13=_4("dojox.widget.UpgradeBar",[_11,_12],{notifications:[],buttonCancel:"Close for now",noRemindButton:"Don't Remind Me Again",templateString:_e("dojox.widget","UpgradeBar/UpgradeBar.html"),constructor:function(_14,_15){
if(!_14.notifications&&_15){
_2.forEach(_15.childNodes,function(n){
if(n.nodeType==1){
var val=_9.get(n,"validate");
this.notifications.push({message:n.innerHTML,validate:function(){
var _16=true;
try{
_16=_1.eval(val);
}
catch(e){
}
return _16;
}});
}
},this);
}
},checkNotifications:function(){
if(!this.notifications.length){
return;
}
for(var i=0;i<this.notifications.length;i++){
var _17=this.notifications[i].validate();
if(_17){
this.notify(this.notifications[i].message);
break;
}
}
},postCreate:function(){
this.inherited(arguments);
if(this.domNode.parentNode){
_d.set(this.domNode,"display","none");
}
_6.mixin(this.attributeMap,{message:{node:"messageNode",type:"innerHTML"}});
if(!this.noRemindButton){
_b.destroy(this.dontRemindButtonNode);
}
if(_7("ie")==6){
var _18=this;
var _19=function(){
var v=win.getBox();
_d.set(_18.domNode,"width",v.w+"px");
};
this.connect(window,"resize",function(){
_19();
});
_19();
}
_10(_6.hitch(this,"checkNotifications"));
},notify:function(msg){
if(_f("disableUpgradeReminders")){
return;
}
if(!this.domNode.parentNode||!this.domNode.parentNode.innerHTML){
document.body.appendChild(this.domNode);
}
_d.set(this.domNode,"display","");
if(msg){
this.set("message",msg);
}
},show:function(){
this._bodyMarginTop=_d.get(_8.body(),"marginTop");
this._size=_c.getContentBox(this.domNode).h;
_d.set(this.domNode,{display:"block",height:0,opacity:0});
if(!this._showAnim){
this._showAnim=fx.combine([_5.animateProperty({node:_8.body(),duration:500,properties:{marginTop:this._bodyMarginTop+this._size}}),_5.animateProperty({node:this.domNode,duration:500,properties:{height:this._size,opacity:1}})]);
}
this._showAnim.play();
},hide:function(){
if(!this._hideAnim){
this._hideAnim=fx.combine([_5.animateProperty({node:_8.body(),duration:500,properties:{marginTop:this._bodyMarginTop}}),_5.animateProperty({node:this.domNode,duration:500,properties:{height:0,opacity:0}})]);
_3.connect(this._hideAnim,"onEnd",this,function(){
_d.set(this.domNode,{display:"none",opacity:1});
});
}
this._hideAnim.play();
},_onDontRemindClick:function(){
_f("disableUpgradeReminders",true,{expires:3650});
this.hide();
},_onCloseEnter:function(){
_a.add(this.closeButtonNode,"dojoxUpgradeBarCloseIcon-hover");
},_onCloseLeave:function(){
_a.remove(this.closeButtonNode,"dojoxUpgradeBarCloseIcon-hover");
}});
return _13;
});
