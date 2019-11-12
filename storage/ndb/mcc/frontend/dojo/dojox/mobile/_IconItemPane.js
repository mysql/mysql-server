//>>built
define("dojox/mobile/_IconItemPane",["dojo/_base/declare","dojo/dom-construct","./Pane","./iconUtils"],function(_1,_2,_3,_4){
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
},_setLabelAttr:function(_5){
this._set("label",_5);
this.labelNode.innerHTML=this._cv?this._cv(_5):_5;
},_setCloseIconAttr:function(_6){
this._set("closeIcon",_6);
this.closeIconNode=_4.setIcon(_6,this.iconPos,this.closeIconNode,null,this.closeHeaderNode);
}});
});
