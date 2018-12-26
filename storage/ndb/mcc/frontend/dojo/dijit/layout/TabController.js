//>>built
require({cache:{"url:dijit/layout/templates/_TabButton.html":"<div role=\"presentation\" data-dojo-attach-point=\"titleNode\" data-dojo-attach-event='onclick:onClick'>\n    <div role=\"presentation\" class='dijitTabInnerDiv' data-dojo-attach-point='innerDiv'>\n        <div role=\"presentation\" class='dijitTabContent' data-dojo-attach-point='tabContent'>\n        \t<div role=\"presentation\" data-dojo-attach-point='focusNode'>\n\t\t        <img src=\"${_blankGif}\" alt=\"\" class=\"dijitIcon dijitTabButtonIcon\" data-dojo-attach-point='iconNode' />\n\t\t        <span data-dojo-attach-point='containerNode' class='tabLabel'></span>\n\t\t        <span class=\"dijitInline dijitTabCloseButton dijitTabCloseIcon\" data-dojo-attach-point='closeNode'\n\t\t        \t\tdata-dojo-attach-event='onclick: onClickCloseButton' role=\"presentation\">\n\t\t            <span data-dojo-attach-point='closeText' class='dijitTabCloseText'>[x]</span\n\t\t        ></span>\n\t\t\t</div>\n        </div>\n    </div>\n</div>\n"}});
define("dijit/layout/TabController",["dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/i18n","dojo/_base/lang","./StackController","../Menu","../MenuItem","dojo/text!./templates/_TabButton.html","dojo/i18n!../nls/common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var _b=_1("dijit.layout._TabButton",_7.StackButton,{baseClass:"dijitTab",cssStateNodes:{closeNode:"dijitTabCloseButton"},templateString:_a,scrollOnFocus:false,buildRendering:function(){
this.inherited(arguments);
_2.setSelectable(this.containerNode,false);
},startup:function(){
this.inherited(arguments);
var n=this.domNode;
setTimeout(function(){
n.className=n.className;
},1);
},_setCloseButtonAttr:function(_c){
this._set("closeButton",_c);
_4.toggle(this.innerDiv,"dijitClosable",_c);
this.closeNode.style.display=_c?"":"none";
if(_c){
var _d=_5.getLocalization("dijit","common");
if(this.closeNode){
_3.set(this.closeNode,"title",_d.itemClose);
}
this._closeMenu=new _8({id:this.id+"_Menu",dir:this.dir,lang:this.lang,textDir:this.textDir,targetNodeIds:[this.domNode]});
this._closeMenu.addChild(new _9({label:_d.itemClose,dir:this.dir,lang:this.lang,textDir:this.textDir,onClick:_6.hitch(this,"onClickCloseButton")}));
}else{
if(this._closeMenu){
this._closeMenu.destroyRecursive();
delete this._closeMenu;
}
}
},_setLabelAttr:function(_e){
this.inherited(arguments);
if(!this.showLabel&&!this.params.title){
this.iconNode.alt=_6.trim(this.containerNode.innerText||this.containerNode.textContent||"");
}
},destroy:function(){
if(this._closeMenu){
this._closeMenu.destroyRecursive();
delete this._closeMenu;
}
this.inherited(arguments);
}});
var _f=_1("dijit.layout.TabController",_7,{baseClass:"dijitTabController",templateString:"<div role='tablist' data-dojo-attach-event='onkeypress:onkeypress'></div>",tabPosition:"top",buttonWidget:_b,_rectifyRtlTabList:function(){
if(0>=this.tabPosition.indexOf("-h")){
return;
}
if(!this.pane2button){
return;
}
var _10=0;
for(var _11 in this.pane2button){
var ow=this.pane2button[_11].innerDiv.scrollWidth;
_10=Math.max(_10,ow);
}
for(_11 in this.pane2button){
this.pane2button[_11].innerDiv.style.width=_10+"px";
}
}});
_f.TabButton=_b;
return _f;
});
