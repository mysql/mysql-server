//>>built
define("dojox/mobile/_IconItemPane",["dojo/_base/declare","dojo/dom-construct","./Pane","./iconUtils","./sniff"],function(_1,_2,_3,_4,_5){
return _1("dojox.mobile._IconItemPane",_3,{iconPos:"",closeIconRole:"",closeIconTitle:"",label:"",closeIcon:"mblDomButtonBlueMinus",baseClass:"mblIconItemPane",tabIndex:"0",_setTabIndexAttr:"closeIconNode",buildRendering:function(){
this.inherited(arguments);
this.hide();
this.closeHeaderNode=_2.create("h2",{className:"mblIconItemPaneHeading"},this.domNode);
this.closeIconNode=_2.create("div",{className:"mblIconItemPaneIcon",role:this.closeIconRole,title:this.closeIconTitle},this.closeHeaderNode);
this.labelNode=_2.create("span",{className:"mblIconItemPaneTitle"},this.closeHeaderNode);
this.containerNode=_2.create("div",{className:"mblContent"},this.domNode);
},show:function(){
this.domNode.style.display="";
},hide:function(){
this.domNode.style.display="none";
},isOpen:function(e){
return this.domNode.style.display!=="none";
},_setLabelAttr:function(_6){
this._set("label",_6);
this.labelNode.innerHTML=this._cv?this._cv(_6):_6;
},_setCloseIconAttr:function(_7){
this._set("closeIcon",_7);
this.closeIconNode=_4.setIcon(_7,this.iconPos,this.closeIconNode,null,this.closeHeaderNode);
if(_5("windows-theme")&&this.closeIconTitle!==""){
this.closeButtonNode=_2.create("span",{className:"mblButton mblCloseButton",innerHTML:this.closeIconTitle,style:{display:"none"}},this.closeIconNode);
}
}});
});
