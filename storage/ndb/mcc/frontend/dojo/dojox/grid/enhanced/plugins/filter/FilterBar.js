//>>built
require({cache:{"url:dojox/grid/enhanced/templates/FilterBar.html":"<table class=\"dojoxGridFBar\" border=\"0\" cellspacing=\"0\" role=\"presentation\" dojoAttachEvent=\"onclick:_onClickFilterBar, onmouseenter:_onMouseEnter, onmouseleave:_onMouseLeave, onmousemove:_onMouseMove\"\n\t><tr><td class=\"dojoxGridFBarBtnTD\"\n\t\t><span dojoType=\"dijit.form.Button\" class=\"dojoxGridFBarBtn\" dojoAttachPoint=\"defineFilterButton\" label=\"...\" iconClass=\"dojoxGridFBarDefFilterBtnIcon\" showLabel=\"true\" dojoAttachEvent=\"onClick:_showFilterDefDialog, onMouseEnter:_onEnterButton, onMouseLeave:_onLeaveButton, onMouseMove:_onMoveButton\"></span\n\t></td><td class=\"dojoxGridFBarInfoTD\"\n\t\t><span class=\"dojoxGridFBarInner\"\n\t\t\t><span class=\"dojoxGridFBarStatus\" dojoAttachPoint=\"statusBarNode\">${_noFilterMsg}</span\n\t\t\t><span dojoType=\"dijit.form.Button\" class=\"dojoxGridFBarClearFilterBtn\" dojoAttachPoint=\"clearFilterButton\" \n\t\t\t\tlabel=\"${_filterBarClearBtnLabel}\" iconClass=\"dojoxGridFBarClearFilterBtnIcon\" showLabel=\"true\" \n\t\t\t\tdojoAttachEvent=\"onClick:_clearFilterDefDialog, onMouseEnter:_onEnterButton, onMouseLeave:_onLeaveButton, onMouseMove:_onMoveButton\"></span\n\t\t\t><span dojotype=\"dijit.form.Button\" class=\"dojoxGridFBarCloseBtn\" dojoAttachPoint=\"closeFilterBarButton\" \n\t\t\t\tlabel=\"${_closeFilterBarBtnLabel}\" iconClass=\"dojoxGridFBarCloseBtnIcon\" showLabel=\"false\" \n\t\t\t\tdojoAttachEvent=\"onClick:_closeFilterBar, onMouseEnter:_onEnterButton, onMouseLeave:_onLeaveButton, onMouseMove:_onMoveButton\"></span\n\t\t></span\n\t></td></tr\n></table>\n"}});
define("dojox/grid/enhanced/plugins/filter/FilterBar",["dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojo/_base/lang","dojo/_base/sniff","dojo/_base/event","dojo/_base/html","dojo/_base/window","dojo/query","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dojo/fx","dojo/_base/fx","dojo/string","dijit/focus","dojo/text!../../templates/FilterBar.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,fx,_d,_e,_f,_10){
var _11="dojoxGridFBarHover",_12="dojoxGridFBarFiltered",_13=function(evt){
try{
if(evt&&evt.preventDefault){
_6.stop(evt);
}
}
catch(e){
}
};
return _1("dojox.grid.enhanced.plugins.filter.FilterBar",[_a,_b,_c],{templateString:_10,widgetsInTemplate:true,_timeout_statusTooltip:300,_handle_statusTooltip:null,_curColIdx:-1,plugin:null,postMixInProperties:function(){
var _14=this.plugin;
var nls=_14.nls;
this._filterBarDefBtnLabel=nls["filterBarDefButton"];
this._filterBarClearBtnLabel=nls["filterBarClearButton"];
this._closeFilterBarBtnLabel=nls["closeFilterBarBtn"];
var _15=_14.args.itemsName||nls["defaultItemsName"];
this._noFilterMsg=_e.substitute(nls["filterBarMsgNoFilterTemplate"],["",_15]);
var t=this.plugin.args.statusTipTimeout;
if(typeof t=="number"){
this._timeout_statusTooltip=t;
}
var g=_14.grid;
g.showFilterBar=_4.hitch(this,"showFilterBar");
g.toggleFilterBar=_4.hitch(this,"toggleFilterBar");
g.isFilterBarShown=_4.hitch(this,"isFilterBarShown");
},postCreate:function(){
this.inherited(arguments);
if(!this.plugin.args.closeFilterbarButton){
_7.style(this.closeFilterBarButton.domNode,"display","none");
}
var _16=this,g=this.plugin.grid,_17=this.oldGetHeaderHeight=_4.hitch(g,g._getHeaderHeight);
this.placeAt(g.viewsHeaderNode,"after");
this.connect(this.plugin.filterDefDialog,"showDialog","_onShowFilterDefDialog");
this.connect(this.plugin.filterDefDialog,"closeDialog","_onCloseFilterDefDialog");
this.connect(g.layer("filter"),"onFiltered",this._onFiltered);
this.defineFilterButton.domNode.title=this.plugin.nls["filterBarDefButton"];
if(_7.hasClass(_8.body(),"dijit_a11y")){
this.defineFilterButton.set("label",this.plugin.nls["a11yFilterBarDefButton"]);
}
this.connect(this.defineFilterButton.domNode,"click",_13);
this.connect(this.clearFilterButton.domNode,"click",_13);
this.connect(this.closeFilterBarButton.domNode,"click",_13);
this.toggleClearFilterBtn(true);
this._initAriaInfo();
g._getHeaderHeight=function(){
return _17()+_7.marginBox(_16.domNode).h;
};
g.focus.addArea({name:"filterbar",onFocus:_4.hitch(this,this._onFocusFilterBar,false),onBlur:_4.hitch(this,this._onBlurFilterBar)});
g.focus.placeArea("filterbar","after","header");
},uninitialize:function(){
var g=this.plugin.grid;
g._getHeaderHeight=this.oldGetHeaderHeight;
g.focus.removeArea("filterbar");
this.plugin=null;
},isFilterBarShown:function(){
return _7.style(this.domNode,"display")!="none";
},showFilterBar:function(_18,_19,_1a){
var g=this.plugin.grid;
if(_19){
if(Boolean(_18)==this.isFilterBarShown()){
return;
}
_1a=_1a||{};
var _1b=[],_1c=500;
_1b.push(fx[_18?"wipeIn":"wipeOut"](_4.mixin({"node":this.domNode,"duration":_1c},_1a)));
var _1d=g.views.views[0].domNode.offsetHeight;
var _1e={"duration":_1c,"properties":{"height":{"end":_4.hitch(this,function(){
var _1f=this.domNode.scrollHeight;
if(_5("ff")){
_1f-=2;
}
return _18?(_1d-_1f):(_1d+_1f);
})}}};
_2.forEach(g.views.views,function(_20){
_1b.push(_d.animateProperty(_4.mixin({"node":_20.domNode},_1e,_1a)),_d.animateProperty(_4.mixin({"node":_20.scrollboxNode},_1e,_1a)));
});
_1b.push(_d.animateProperty(_4.mixin({"node":g.viewsNode},_1e,_1a)));
fx.combine(_1b).play();
}else{
_7.style(this.domNode,"display",_18?"":"none");
g.update();
}
},toggleFilterBar:function(_21,_22){
this.showFilterBar(!this.isFilterBarShown(),_21,_22);
},getColumnIdx:function(_23){
var _24=_9("[role='columnheader']",this.plugin.grid.viewsHeaderNode);
var idx=-1;
for(var i=_24.length-1;i>=0;--i){
var _25=_7.position(_24[i]);
if(_23>=_25.x&&_23<_25.x+_25.w){
idx=i;
break;
}
}
if(idx>=0&&this.plugin.grid.layout.cells[idx].filterable!==false){
return idx;
}else{
return -1;
}
},toggleClearFilterBtn:function(_26){
_7.style(this.clearFilterButton.domNode,"display",_26?"none":"");
},_closeFilterBar:function(e){
_13(e);
var _27=this.plugin.filterDefDialog.getCriteria();
if(_27){
var _28=_3.connect(this.plugin.filterDefDialog,"clearFilter",this,function(){
this.showFilterBar(false,true);
_3.disconnect(_28);
});
this._clearFilterDefDialog(e);
}else{
this.showFilterBar(false,true);
}
},_showFilterDefDialog:function(e){
_13(e);
this.plugin.filterDefDialog.showDialog(this._curColIdx);
this.plugin.grid.focus.focusArea("filterbar");
},_clearFilterDefDialog:function(e){
_13(e);
this.plugin.filterDefDialog.onClearFilter();
this.plugin.grid.focus.focusArea("filterbar");
},_onEnterButton:function(e){
this._onBlurFilterBar();
_13(e);
},_onMoveButton:function(e){
this._onBlurFilterBar();
},_onLeaveButton:function(e){
this._leavingBtn=true;
},_onShowFilterDefDialog:function(_29){
if(typeof _29=="number"){
this._curColIdx=_29;
}
this._defPaneIsShown=true;
},_onCloseFilterDefDialog:function(){
this._defPaneIsShown=false;
this._curColIdx=-1;
_f.focus(this.defineFilterButton.domNode);
},_onClickFilterBar:function(e){
_13(e);
this._clearStatusTipTimeout();
this.plugin.grid.focus.focusArea("filterbar");
this.plugin.filterDefDialog.showDialog(this.getColumnIdx(e.clientX));
},_onMouseEnter:function(e){
this._onFocusFilterBar(true,null);
this._updateTipPosition(e);
this._setStatusTipTimeout();
},_onMouseMove:function(e){
if(this._leavingBtn){
this._onFocusFilterBar(true,null);
this._leavingBtn=false;
}
if(this._isFocused){
this._setStatusTipTimeout();
this._highlightHeader(this.getColumnIdx(e.clientX));
if(this._handle_statusTooltip){
this._updateTipPosition(e);
}
}
},_onMouseLeave:function(e){
this._onBlurFilterBar();
},_updateTipPosition:function(evt){
this._tippos={x:evt.pageX,y:evt.pageY};
},_onFocusFilterBar:function(_2a,evt,_2b){
if(!this.isFilterBarShown()){
return false;
}
this._isFocused=true;
_7.addClass(this.domNode,_11);
if(!_2a){
var _2c=_7.style(this.clearFilterButton.domNode,"display")!=="none";
var _2d=_7.style(this.closeFilterBarButton.domNode,"display")!=="none";
if(typeof this._focusPos=="undefined"){
if(_2b>0){
this._focusPos=0;
}else{
if(_2d){
this._focusPos=1;
}else{
this._focusPos=0;
}
if(_2c){
++this._focusPos;
}
}
}
if(this._focusPos===0){
_f.focus(this.defineFilterButton.focusNode);
}else{
if(this._focusPos===1&&_2c){
_f.focus(this.clearFilterButton.focusNode);
}else{
_f.focus(this.closeFilterBarButton.focusNode);
}
}
}
_13(evt);
return true;
},_onBlurFilterBar:function(evt,_2e){
if(this._isFocused){
this._isFocused=false;
_7.removeClass(this.domNode,_11);
this._clearStatusTipTimeout();
this._clearHeaderHighlight();
}
var _2f=true;
if(_2e){
var _30=3;
if(_7.style(this.closeFilterBarButton.domNode,"display")==="none"){
--_30;
}
if(_7.style(this.clearFilterButton.domNode,"display")==="none"){
--_30;
}
if(_30==1){
delete this._focusPos;
}else{
var _31=this._focusPos;
for(var _32=_31+_2e;_32<0;_32+=_30){
}
_32%=_30;
if((_2e>0&&_32<_31)||(_2e<0&&_32>_31)){
delete this._focusPos;
}else{
this._focusPos=_32;
_2f=false;
}
}
}
return _2f;
},_onFiltered:function(_33,_34){
var p=this.plugin,_35=p.args.itemsName||p.nls["defaultItemsName"],msg="",g=p.grid,_36=g.layer("filter");
if(_36.filterDef()){
msg=_e.substitute(p.nls["filterBarMsgHasFilterTemplate"],[_33,_34,_35]);
_7.addClass(this.domNode,_12);
}else{
msg=_e.substitute(p.nls["filterBarMsgNoFilterTemplate"],[_34,_35]);
_7.removeClass(this.domNode,_12);
}
this.statusBarNode.innerHTML=msg;
this._focusPos=0;
},_initAriaInfo:function(){
this.defineFilterButton.domNode.setAttribute("aria-label",this.plugin.nls["waiFilterBarDefButton"]);
this.clearFilterButton.domNode.setAttribute("aria-label",this.plugin.nls["waiFilterBarClearButton"]);
},_isInColumn:function(_37,_38,_39){
var _3a=_7.position(_38);
return _37>=_3a.x&&_37<_3a.x+_3a.w;
},_setStatusTipTimeout:function(){
this._clearStatusTipTimeout();
if(!this._defPaneIsShown){
this._handle_statusTooltip=setTimeout(_4.hitch(this,this._showStatusTooltip),this._timeout_statusTooltip);
}
},_clearStatusTipTimeout:function(){
clearTimeout(this._handle_statusTooltip);
this._handle_statusTooltip=null;
},_showStatusTooltip:function(){
this._handle_statusTooltip=null;
if(this.plugin){
this.plugin.filterStatusTip.showDialog(this._tippos.x,this._tippos.y,this.getColumnIdx(this._tippos.x));
}
},_highlightHeader:function(_3b){
if(_3b!=this._previousHeaderIdx){
var g=this.plugin.grid,_3c=g.getCell(this._previousHeaderIdx);
if(_3c){
_7.removeClass(_3c.getHeaderNode(),"dojoxGridCellOver");
}
_3c=g.getCell(_3b);
if(_3c){
_7.addClass(_3c.getHeaderNode(),"dojoxGridCellOver");
}
this._previousHeaderIdx=_3b;
}
},_clearHeaderHighlight:function(){
if(typeof this._previousHeaderIdx!="undefined"){
var g=this.plugin.grid,_3d=g.getCell(this._previousHeaderIdx);
if(_3d){
g.onHeaderCellMouseOut({cellNode:_3d.getHeaderNode()});
}
delete this._previousHeaderIdx;
}
}});
});
