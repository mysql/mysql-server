//>>built
require({cache:{"url:dijit/layout/templates/ScrollingTabController.html":"<div class=\"dijitTabListContainer-${tabPosition}\" style=\"visibility:hidden\">\n\t<div data-dojo-type=\"dijit.layout._ScrollingTabControllerMenuButton\"\n\t\t\tclass=\"tabStripButton-${tabPosition}\"\n\t\t\tid=\"${id}_menuBtn\"\n\t\t\tdata-dojo-props=\"containerId: '${containerId}', iconClass: 'dijitTabStripMenuIcon',\n\t\t\t\t\tdropDownPosition: ['below-alt', 'above-alt']\"\n\t\t\tdata-dojo-attach-point=\"_menuBtn\" showLabel=\"false\" title=\"\">&#9660;</div>\n\t<div data-dojo-type=\"dijit.layout._ScrollingTabControllerButton\"\n\t\t\tclass=\"tabStripButton-${tabPosition}\"\n\t\t\tid=\"${id}_leftBtn\"\n\t\t\tdata-dojo-props=\"iconClass:'dijitTabStripSlideLeftIcon', showLabel:false, title:''\"\n\t\t\tdata-dojo-attach-point=\"_leftBtn\" data-dojo-attach-event=\"onClick: doSlideLeft\">&#9664;</div>\n\t<div data-dojo-type=\"dijit.layout._ScrollingTabControllerButton\"\n\t\t\tclass=\"tabStripButton-${tabPosition}\"\n\t\t\tid=\"${id}_rightBtn\"\n\t\t\tdata-dojo-props=\"iconClass:'dijitTabStripSlideRightIcon', showLabel:false, title:''\"\n\t\t\tdata-dojo-attach-point=\"_rightBtn\" data-dojo-attach-event=\"onClick: doSlideRight\">&#9654;</div>\n\t<div class='dijitTabListWrapper' data-dojo-attach-point='tablistWrapper'>\n\t\t<div role='tablist' data-dojo-attach-event='onkeypress:onkeypress'\n\t\t\t\tdata-dojo-attach-point='containerNode' class='nowrapTabStrip'></div>\n\t</div>\n</div>","url:dijit/layout/templates/_ScrollingTabControllerButton.html":"<div data-dojo-attach-event=\"onclick:_onClick\">\n\t<div role=\"presentation\" class=\"dijitTabInnerDiv\" data-dojo-attach-point=\"innerDiv,focusNode\">\n\t\t<div role=\"presentation\" class=\"dijitTabContent dijitButtonContents\" data-dojo-attach-point=\"tabContent\">\n\t\t\t<img role=\"presentation\" alt=\"\" src=\"${_blankGif}\" class=\"dijitTabStripIcon\" data-dojo-attach-point=\"iconNode\"/>\n\t\t\t<span data-dojo-attach-point=\"containerNode,titleNode\" class=\"dijitButtonText\"></span>\n\t\t</div>\n\t</div>\n</div>"}});
define("dijit/layout/ScrollingTabController",["dojo/_base/array","dojo/_base/declare","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/fx","dojo/_base/lang","dojo/query","dojo/_base/sniff","../registry","dojo/text!./templates/ScrollingTabController.html","dojo/text!./templates/_ScrollingTabControllerButton.html","./TabController","./utils","../_WidgetsInTemplateMixin","../Menu","../MenuItem","../form/Button","../_HasDropDown","dojo/NodeList-dom"],function(_1,_2,_3,_4,_5,fx,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12){
var _13=_2("dijit.layout.ScrollingTabController",[_c,_e],{baseClass:"dijitTabController dijitScrollingTabController",templateString:_a,useMenu:true,useSlider:true,tabStripClass:"",widgetsInTemplate:true,_minScroll:5,_setClassAttr:{node:"containerNode",type:"class"},buildRendering:function(){
this.inherited(arguments);
var n=this.domNode;
this.scrollNode=this.tablistWrapper;
this._initButtons();
if(!this.tabStripClass){
this.tabStripClass="dijitTabContainer"+this.tabPosition.charAt(0).toUpperCase()+this.tabPosition.substr(1).replace(/-.*/,"")+"None";
_3.add(n,"tabStrip-disabled");
}
_3.add(this.tablistWrapper,this.tabStripClass);
},onStartup:function(){
this.inherited(arguments);
_5.set(this.domNode,"visibility","");
this._postStartup=true;
},onAddChild:function(_14,_15){
this.inherited(arguments);
_1.forEach(["label","iconClass"],function(_16){
this.pane2watches[_14.id].push(this.pane2button[_14.id].watch(_16,_6.hitch(this,function(){
if(this._postStartup&&this._dim){
this.resize(this._dim);
}
})));
},this);
_5.set(this.containerNode,"width",(_5.get(this.containerNode,"width")+200)+"px");
},onRemoveChild:function(_17,_18){
var _19=this.pane2button[_17.id];
if(this._selectedTab===_19.domNode){
this._selectedTab=null;
}
this.inherited(arguments);
},_initButtons:function(){
this._btnWidth=0;
this._buttons=_7("> .tabStripButton",this.domNode).filter(function(btn){
if((this.useMenu&&btn==this._menuBtn.domNode)||(this.useSlider&&(btn==this._rightBtn.domNode||btn==this._leftBtn.domNode))){
this._btnWidth+=_4.getMarginSize(btn).w;
return true;
}else{
_5.set(btn,"display","none");
return false;
}
},this);
},_getTabsWidth:function(){
var _1a=this.getChildren();
if(_1a.length){
var _1b=_1a[this.isLeftToRight()?0:_1a.length-1].domNode,_1c=_1a[this.isLeftToRight()?_1a.length-1:0].domNode;
return _1c.offsetLeft+_5.get(_1c,"width")-_1b.offsetLeft;
}else{
return 0;
}
},_enableBtn:function(_1d){
var _1e=this._getTabsWidth();
_1d=_1d||_5.get(this.scrollNode,"width");
return _1e>0&&_1d<_1e;
},resize:function(dim){
this._dim=dim;
this.scrollNode.style.height="auto";
var cb=this._contentBox=_d.marginBox2contentBox(this.domNode,{h:0,w:dim.w});
cb.h=this.scrollNode.offsetHeight;
_4.setContentSize(this.domNode,cb);
var _1f=this._enableBtn(this._contentBox.w);
this._buttons.style("display",_1f?"":"none");
this._leftBtn.layoutAlign="left";
this._rightBtn.layoutAlign="right";
this._menuBtn.layoutAlign=this.isLeftToRight()?"right":"left";
_d.layoutChildren(this.domNode,this._contentBox,[this._menuBtn,this._leftBtn,this._rightBtn,{domNode:this.scrollNode,layoutAlign:"client"}]);
if(this._selectedTab){
if(this._anim&&this._anim.status()=="playing"){
this._anim.stop();
}
this.scrollNode.scrollLeft=this._convertToScrollLeft(this._getScrollForSelectedTab());
}
this._setButtonClass(this._getScroll());
this._postResize=true;
return {h:this._contentBox.h,w:dim.w};
},_getScroll:function(){
return (this.isLeftToRight()||_8("ie")<8||(_8("ie")&&_8("quirks"))||_8("webkit"))?this.scrollNode.scrollLeft:_5.get(this.containerNode,"width")-_5.get(this.scrollNode,"width")+(_8("ie")==8?-1:1)*this.scrollNode.scrollLeft;
},_convertToScrollLeft:function(val){
if(this.isLeftToRight()||_8("ie")<8||(_8("ie")&&_8("quirks"))||_8("webkit")){
return val;
}else{
var _20=_5.get(this.containerNode,"width")-_5.get(this.scrollNode,"width");
return (_8("ie")==8?-1:1)*(val-_20);
}
},onSelectChild:function(_21){
var tab=this.pane2button[_21.id];
if(!tab||!_21){
return;
}
var _22=tab.domNode;
if(_22!=this._selectedTab){
this._selectedTab=_22;
if(this._postResize){
var sl=this._getScroll();
if(sl>_22.offsetLeft||sl+_5.get(this.scrollNode,"width")<_22.offsetLeft+_5.get(_22,"width")){
this.createSmoothScroll().play();
}
}
}
this.inherited(arguments);
},_getScrollBounds:function(){
var _23=this.getChildren(),_24=_5.get(this.scrollNode,"width"),_25=_5.get(this.containerNode,"width"),_26=_25-_24,_27=this._getTabsWidth();
if(_23.length&&_27>_24){
return {min:this.isLeftToRight()?0:_23[_23.length-1].domNode.offsetLeft,max:this.isLeftToRight()?(_23[_23.length-1].domNode.offsetLeft+_5.get(_23[_23.length-1].domNode,"width"))-_24:_26};
}else{
var _28=this.isLeftToRight()?0:_26;
return {min:_28,max:_28};
}
},_getScrollForSelectedTab:function(){
var w=this.scrollNode,n=this._selectedTab,_29=_5.get(this.scrollNode,"width"),_2a=this._getScrollBounds();
var pos=(n.offsetLeft+_5.get(n,"width")/2)-_29/2;
pos=Math.min(Math.max(pos,_2a.min),_2a.max);
return pos;
},createSmoothScroll:function(x){
if(arguments.length>0){
var _2b=this._getScrollBounds();
x=Math.min(Math.max(x,_2b.min),_2b.max);
}else{
x=this._getScrollForSelectedTab();
}
if(this._anim&&this._anim.status()=="playing"){
this._anim.stop();
}
var _2c=this,w=this.scrollNode,_2d=new fx.Animation({beforeBegin:function(){
if(this.curve){
delete this.curve;
}
var _2e=w.scrollLeft,_2f=_2c._convertToScrollLeft(x);
_2d.curve=new fx._Line(_2e,_2f);
},onAnimate:function(val){
w.scrollLeft=val;
}});
this._anim=_2d;
this._setButtonClass(x);
return _2d;
},_getBtnNode:function(e){
var n=e.target;
while(n&&!_3.contains(n,"tabStripButton")){
n=n.parentNode;
}
return n;
},doSlideRight:function(e){
this.doSlide(1,this._getBtnNode(e));
},doSlideLeft:function(e){
this.doSlide(-1,this._getBtnNode(e));
},doSlide:function(_30,_31){
if(_31&&_3.contains(_31,"dijitTabDisabled")){
return;
}
var _32=_5.get(this.scrollNode,"width");
var d=(_32*0.75)*_30;
var to=this._getScroll()+d;
this._setButtonClass(to);
this.createSmoothScroll(to).play();
},_setButtonClass:function(_33){
var _34=this._getScrollBounds();
this._leftBtn.set("disabled",_33<=_34.min);
this._rightBtn.set("disabled",_33>=_34.max);
}});
var _35=_2("dijit.layout._ScrollingTabControllerButtonMixin",null,{baseClass:"dijitTab tabStripButton",templateString:_b,tabIndex:"",isFocusable:function(){
return false;
}});
_2("dijit.layout._ScrollingTabControllerButton",[_11,_35]);
_2("dijit.layout._ScrollingTabControllerMenuButton",[_11,_12,_35],{containerId:"",tabIndex:"-1",isLoaded:function(){
return false;
},loadDropDown:function(_36){
this.dropDown=new _f({id:this.containerId+"_menu",dir:this.dir,lang:this.lang,textDir:this.textDir});
var _37=_9.byId(this.containerId);
_1.forEach(_37.getChildren(),function(_38){
var _39=new _10({id:_38.id+"_stcMi",label:_38.title,iconClass:_38.iconClass,dir:_38.dir,lang:_38.lang,textDir:_38.textDir,onClick:function(){
_37.selectChild(_38);
}});
this.dropDown.addChild(_39);
},this);
_36();
},closeDropDown:function(_3a){
this.inherited(arguments);
if(this.dropDown){
this.dropDown.destroyRecursive();
delete this.dropDown;
}
}});
return _13;
});
