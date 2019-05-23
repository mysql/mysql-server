//>>built
require({cache:{"url:dijit/layout/templates/_TabButton.html":"<div role=\"presentation\" data-dojo-attach-point=\"titleNode,innerDiv,tabContent\" class=\"dijitTabInner dijitTabContent\">\n\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitIcon dijitTabButtonIcon\" data-dojo-attach-point='iconNode'/>\n\t<span data-dojo-attach-point='containerNode,focusNode' class='tabLabel'></span>\n\t<span class=\"dijitInline dijitTabCloseButton dijitTabCloseIcon\" data-dojo-attach-point='closeNode'\n\t\t  role=\"presentation\">\n\t\t<span data-dojo-attach-point='closeText' class='dijitTabCloseText'>[x]</span\n\t\t\t\t></span>\n</div>\n"}});
define("dijit/layout/TabController",["dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/i18n","dojo/_base/lang","./StackController","../registry","../Menu","../MenuItem","dojo/text!./templates/_TabButton.html","dojo/i18n!../nls/common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_1("dijit.layout._TabButton",_7.StackButton,{baseClass:"dijitTab",cssStateNodes:{closeNode:"dijitTabCloseButton"},templateString:_b,scrollOnFocus:false,buildRendering:function(){
this.inherited(arguments);
_2.setSelectable(this.containerNode,false);
},startup:function(){
this.inherited(arguments);
var n=this.domNode;
this.defer(function(){
n.className=n.className;
},1);
},_setCloseButtonAttr:function(_d){
this._set("closeButton",_d);
_4.toggle(this.domNode,"dijitClosable",_d);
this.closeNode.style.display=_d?"":"none";
if(_d){
var _e=_5.getLocalization("dijit","common");
if(this.closeNode){
_3.set(this.closeNode,"title",_e.itemClose);
}
}
},_setDisabledAttr:function(_f){
this.inherited(arguments);
if(this.closeNode){
if(_f){
_3.remove(this.closeNode,"title");
}else{
var _10=_5.getLocalization("dijit","common");
_3.set(this.closeNode,"title",_10.itemClose);
}
}
},_setLabelAttr:function(_11){
this.inherited(arguments);
if(!this.showLabel&&!this.params.title){
this.iconNode.alt=_6.trim(this.containerNode.innerText||this.containerNode.textContent||"");
}
}});
var _12=_1("dijit.layout.TabController",_7,{baseClass:"dijitTabController",templateString:"<div role='tablist' data-dojo-attach-event='onkeypress:onkeypress'></div>",tabPosition:"top",buttonWidget:_c,buttonWidgetCloseClass:"dijitTabCloseButton",postCreate:function(){
this.inherited(arguments);
var _13=new _9({id:this.id+"_Menu",ownerDocument:this.ownerDocument,dir:this.dir,lang:this.lang,textDir:this.textDir,targetNodeIds:[this.domNode],selector:function(_14){
return _4.contains(_14,"dijitClosable")&&!_4.contains(_14,"dijitTabDisabled");
}});
this.own(_13);
var _15=_5.getLocalization("dijit","common"),_16=this;
_13.addChild(new _a({label:_15.itemClose,ownerDocument:this.ownerDocument,dir:this.dir,lang:this.lang,textDir:this.textDir,onClick:function(evt){
var _17=_8.byNode(this.getParent().currentTarget);
_16.onCloseButtonClick(_17.page);
}}));
}});
_12.TabButton=_c;
return _12;
});
