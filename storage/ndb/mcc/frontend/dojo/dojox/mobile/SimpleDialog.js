//>>built
define("dojox/mobile/SimpleDialog",["dojo/_base/declare","dojo/_base/window","dojo/dom-class","dojo/dom-attr","dojo/dom-construct","dojo/on","dojo/touch","dijit/registry","./Pane","./iconUtils","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/SimpleDialog"],function(_1,_2,_3,_4,_5,on,_6,_7,_8,_9,_a,_b){
var _c=_1(_a("dojo-bidi")?"dojox.mobile.NonBidiSimpleDialog":"dojox.mobile.SimpleDialog",_8,{top:"auto",left:"auto",modal:true,closeButton:false,closeButtonClass:"mblDomButtonSilverCircleRedCross",tabIndex:"0",_setTabIndexAttr:"",baseClass:"mblSimpleDialog",_cover:[],buildRendering:function(){
this.containerNode=_5.create("div",{className:"mblSimpleDialogContainer"});
if(this.srcNodeRef){
for(var i=0,_d=this.srcNodeRef.childNodes.length;i<_d;i++){
this.containerNode.appendChild(this.srcNodeRef.removeChild(this.srcNodeRef.firstChild));
}
}
this.inherited(arguments);
_4.set(this.domNode,"role","dialog");
if(this.containerNode.getElementsByClassName){
var _e=this.containerNode.getElementsByClassName("mblSimpleDialogTitle")[0];
if(_e){
_e.id=_e.id||_7.getUniqueId("dojo_mobile_mblSimpleDialogTitle");
_4.set(this.domNode,"aria-labelledby",_e.id);
}
var _f=this.containerNode.getElementsByClassName("mblSimpleDialogText")[0];
if(_f){
_f.id=_f.id||_7.getUniqueId("dojo_mobile_mblSimpleDialogText");
_4.set(this.domNode,"aria-describedby",_f.id);
}
}
_3.add(this.domNode,"mblSimpleDialogDecoration");
this.domNode.style.display="none";
this.domNode.appendChild(this.containerNode);
if(this.closeButton){
this.closeButtonNode=_5.create("div",{className:"mblSimpleDialogCloseBtn "+this.closeButtonClass},this.domNode);
_9.createDomButton(this.closeButtonNode);
this.connect(this.closeButtonNode,"onclick","_onCloseButtonClick");
}
this.connect(this.domNode,"onkeydown","_onKeyDown");
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
_2.body().appendChild(this.domNode);
},addCover:function(){
if(!this._cover[0]){
this._cover[0]=_5.create("div",{className:"mblSimpleDialogCover"},_2.body());
}else{
this._cover[0].style.display="";
}
if(_a("windows-theme")){
this.own(on(this._cover[0],_6.press,function(){
}));
}
},removeCover:function(){
this._cover[0].style.display="none";
},_onCloseButtonClick:function(e){
if(this.onCloseButtonClick(e)===false){
return;
}
this.hide();
},onCloseButtonClick:function(){
},_onKeyDown:function(e){
if(e.keyCode==27){
this.hide();
}
},refresh:function(){
var n=this.domNode;
var h;
if(this.closeButton){
var b=this.closeButtonNode;
var s=Math.round(b.offsetHeight/2);
b.style.top=-s+"px";
b.style.left=n.offsetWidth-s+"px";
}
if(this.top==="auto"){
h=_2.global.innerHeight||_2.doc.documentElement.clientHeight;
n.style.top=Math.round((h-n.offsetHeight)/2)+"px";
}else{
n.style.top=this.top;
}
if(this.left==="auto"){
h=_2.global.innerWidth||_2.doc.documentElement.clientWidth;
n.style.left=Math.round((h-n.offsetWidth)/2)+"px";
}else{
n.style.left=this.left;
}
},show:function(){
if(this.domNode.style.display===""){
return;
}
if(this.modal){
this.addCover();
}
this.domNode.style.display="";
this.resize();
this.refresh();
var _10;
if(this.domNode.getElementsByClassName){
_10=this.domNode.getElementsByClassName("mblSimpleDialogButton")[0];
}
var _11=_10||this.closeButtonNode||this.domNode;
this.defer(function(){
_11.focus();
},1000);
},hide:function(){
if(this.domNode.style.display==="none"){
return;
}
this.domNode.style.display="none";
if(this.modal){
this.removeCover();
}
}});
return _a("dojo-bidi")?_1("dojox.mobile.SimpleDialog",[_c,_b]):_c;
});
