//>>built
define("dojox/mobile/SimpleDialog",["dojo/_base/declare","dojo/_base/window","dojo/dom-class","dojo/dom-construct","./Pane","./iconUtils"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.mobile.SimpleDialog",_5,{top:"auto",left:"auto",modal:true,closeButton:false,closeButtonClass:"mblDomButtonSilverCircleRedCross",tabIndex:"0",_setTabIndexAttr:"",baseClass:"mblSimpleDialog",_cover:[],buildRendering:function(){
this.containerNode=_4.create("div",{className:"mblSimpleDialogContainer"});
if(this.srcNodeRef){
for(var i=0,_7=this.srcNodeRef.childNodes.length;i<_7;i++){
this.containerNode.appendChild(this.srcNodeRef.removeChild(this.srcNodeRef.firstChild));
}
}
this.inherited(arguments);
_3.add(this.domNode,"mblSimpleDialogDecoration");
this.domNode.style.display="none";
this.domNode.appendChild(this.containerNode);
if(this.closeButton){
this.closeButtonNode=_4.create("div",{className:"mblSimpleDialogCloseBtn "+this.closeButtonClass},this.domNode);
_6.createDomButton(this.closeButtonNode);
this._clickHandle=this.connect(this.closeButtonNode,"onclick","_onCloseButtonClick");
}
this._keydownHandle=this.connect(this.domNode,"onkeydown","_onKeyDown");
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
_2.body().appendChild(this.domNode);
},addCover:function(){
if(!this._cover[0]){
this._cover[0]=_4.create("div",{className:"mblSimpleDialogCover"},_2.body());
}else{
this._cover[0].style.display="";
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
if(this.closeButton){
var b=this.closeButtonNode;
var s=Math.round(b.offsetHeight/2);
b.style.top=-s+"px";
b.style.left=n.offsetWidth-s+"px";
}
if(this.top==="auto"){
var h=_2.global.innerHeight||_2.doc.documentElement.clientHeight;
n.style.top=Math.round((h-n.offsetHeight)/2)+"px";
}else{
n.style.top=this.top;
}
if(this.left==="auto"){
var h=_2.global.innerWidth||_2.doc.documentElement.clientWidth;
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
this.refresh();
this.domNode.focus();
},hide:function(){
if(this.domNode.style.display==="none"){
return;
}
this.domNode.style.display="none";
if(this.modal){
this.removeCover();
}
}});
});
