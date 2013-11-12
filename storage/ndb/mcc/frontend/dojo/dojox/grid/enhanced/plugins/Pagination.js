//>>built
require({cache:{"url:dojox/grid/enhanced/templates/Pagination.html":"<div dojoAttachPoint=\"paginatorBar\"\n\t><table cellpadding=\"0\" cellspacing=\"0\"  class=\"dojoxGridPaginator\"\n\t\t><tr\n\t\t\t><td dojoAttachPoint=\"descriptionTd\" class=\"dojoxGridDescriptionTd\"\n\t\t\t\t><div dojoAttachPoint=\"descriptionDiv\" class=\"dojoxGridDescription\"></div\n\t\t\t></div></td\n\t\t\t><td dojoAttachPoint=\"sizeSwitchTd\"></td\n\t\t\t><td dojoAttachPoint=\"pageStepperTd\" class=\"dojoxGridPaginatorFastStep\"\n\t\t\t\t><div dojoAttachPoint=\"pageStepperDiv\" class=\"dojoxGridPaginatorStep\"></div\n\t\t\t></td\n\t\t\t><td dojoAttachPoint=\"gotoPageTd\" class=\"dojoxGridPaginatorGotoTd\"\n\t\t\t\t><div dojoAttachPoint=\"gotoPageDiv\" class=\"dojoxGridPaginatorGotoDiv\" dojoAttachEvent=\"onclick:_openGotopageDialog, onkeydown:_openGotopageDialog\"\n\t\t\t\t\t><span class=\"dojoxGridWardButtonInner\">&#8869;</span\n\t\t\t\t></div\n\t\t\t></td\n\t\t></tr\n\t></table\n></div>\n"}});
define("dojox/grid/enhanced/plugins/Pagination",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojo/_base/lang","dojo/_base/html","dojo/_base/event","dojo/_base/window","dojo/query","dojo/string","dojo/i18n","dojo/keys","dojo/text!../templates/Pagination.html","./Dialog","./_StoreLayer","../_Plugin","../../EnhancedGrid","dijit/form/Button","dijit/form/NumberTextBox","dijit/focus","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dojox/html/metrics","dojo/i18n!../nls/Pagination"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18){
var _19=_2("dojox.grid.enhanced.plugins.pagination._GotoPagePane",[_15,_16,_17],{templateString:"<div>"+"<div class='dojoxGridDialogMargin' dojoAttachPoint='_mainMsgNode'></div>"+"<div class='dojoxGridDialogMargin'>"+"<input dojoType='dijit.form.NumberTextBox' style='width: 50px;' dojoAttachPoint='_pageInputBox' dojoAttachEvent='onKeyUp: _onKey'></input>"+"<label dojoAttachPoint='_pageLabelNode'></label>"+"</div>"+"<div class='dojoxGridDialogButton'>"+"<button dojoType='dijit.form.Button' dojoAttachPoint='_confirmBtn' dojoAttachEvent='onClick: _onConfirm'></button>"+"<button dojoType='dijit.form.Button' dojoAttachPoint='_cancelBtn' dojoAttachEvent='onClick: _onCancel'></button>"+"</div>"+"</div>",widgetsInTemplate:true,dlg:null,postMixInProperties:function(){
this.plugin=this.dlg.plugin;
},postCreate:function(){
this.inherited(arguments);
this._mainMsgNode.innerHTML=this.plugin._nls[12];
this._confirmBtn.set("label",this.plugin._nls[14]);
this._confirmBtn.set("disabled",true);
this._cancelBtn.set("label",this.plugin._nls[15]);
},_onConfirm:function(evt){
if(this._pageInputBox.isValid()&&this._pageInputBox.getDisplayedValue()!==""){
this.plugin.currentPage(this._pageInputBox.parse(this._pageInputBox.getDisplayedValue()));
this.dlg._gotoPageDialog.hide();
this._pageInputBox.reset();
}
_1a(evt);
},_onCancel:function(evt){
this._pageInputBox.reset();
this.dlg._gotoPageDialog.hide();
_1a(evt);
},_onKey:function(evt){
this._confirmBtn.set("disabled",!this._pageInputBox.isValid()||this._pageInputBox.getDisplayedValue()=="");
if(!evt.altKey&&!evt.metaKey&&evt.keyCode===_c.ENTER){
this._onConfirm(evt);
}
}});
var _1b=_2("dojox.grid.enhanced.plugins.pagination._GotoPageDialog",null,{pageCount:0,dlgPane:null,constructor:function(_1c){
this.plugin=_1c;
this.dlgPane=new _19({"dlg":this});
this.dlgPane.startup();
this._gotoPageDialog=new _e({"refNode":_1c.grid.domNode,"title":this.plugin._nls[11],"content":this.dlgPane});
this._gotoPageDialog.startup();
},_updatePageCount:function(){
this.pageCount=this.plugin.getTotalPageNum();
this.dlgPane._pageInputBox.constraints={fractional:false,min:1,max:this.pageCount};
this.dlgPane._pageLabelNode.innerHTML=_a.substitute(this.plugin._nls[13],[this.pageCount]);
},showDialog:function(){
this._updatePageCount();
this._gotoPageDialog.show();
},destroy:function(){
this._gotoPageDialog.destroy();
}});
var _1d=_2("dojox.grid.enhanced.plugins._ForcedPageStoreLayer",_f._StoreLayer,{tags:["presentation"],constructor:function(_1e){
this._plugin=_1e;
},_fetch:function(_1f){
var _20=this,_21=_20._plugin,_22=_21.grid,_23=_1f.scope||_8.global,_24=_1f.onBegin;
_1f.start=(_21._currentPage-1)*_21._currentPageSize+_1f.start;
_20.startIdx=_1f.start;
_20.endIdx=_1f.start+_21._currentPageSize-1;
var p=_21._paginator;
if(!_21._showAll){
_21._showAll=!p.sizeSwitch&&!p.pageStepper&&!p.gotoButton;
}
if(_24&&_21._showAll){
_1f.onBegin=function(_25,req){
_21._maxSize=_21._currentPageSize=_25;
_20.startIdx=0;
_20.endIdx=_25-1;
_21._paginator._update();
req.onBegin=_24;
req.onBegin.call(_23,_25,req);
};
}else{
if(_24){
_1f.onBegin=function(_26,req){
req.start=0;
req.count=_21._currentPageSize;
_21._maxSize=_26;
_20.endIdx=_20.endIdx>=_26?(_26-1):_20.endIdx;
if(_20.startIdx>_26&&_26!==0){
_22._pending_requests[req.start]=false;
_21.firstPage();
}
_21._paginator._update();
req.onBegin=_24;
req.onBegin.call(_23,Math.min(_21._currentPageSize,(_26-_20.startIdx)),req);
};
}
}
return _5.hitch(this._store,this._originFetch)(_1f);
}});
var _1a=function(evt){
try{
_7.stop(evt);
}
catch(e){
}
};
var _27=_2("dojox.grid.enhanced.plugins.pagination._Focus",null,{_focusedNode:null,_isFocused:false,constructor:function(_28){
this._pager=_28;
var _29=_28.plugin.grid.focus;
_28.plugin.connect(_28,"onSwitchPageSize",_5.hitch(this,"_onActive"));
_28.plugin.connect(_28,"onPageStep",_5.hitch(this,"_onActive"));
_28.plugin.connect(_28,"onShowGotoPageDialog",_5.hitch(this,"_onActive"));
_28.plugin.connect(_28,"_update",_5.hitch(this,"_moveFocus"));
},_onFocus:function(evt,_2a){
var _2b,_2c;
if(!this._isFocused){
_2b=this._focusedNode||_9("[tabindex]",this._pager.domNode)[0];
}else{
if(_2a&&this._focusedNode){
var dir=_2a>0?-1:1,_2d=parseInt(this._focusedNode.getAttribute("tabindex"),10)+dir;
while(_2d>=-3&&_2d<0){
_2b=_9("[tabindex="+_2d+"]",this._pager.domNode)[0];
if(_2b){
break;
}else{
_2d+=dir;
}
}
}
}
return this._focus(_2b,evt);
},_onBlur:function(evt,_2e){
if(!_2e||!this._focusedNode){
this._isFocused=false;
if(this._focusedNode&&_6.hasClass(this._focusedNode,"dojoxGridButtonFocus")){
_6.removeClass(this._focusedNode,"dojoxGridButtonFocus");
}
return true;
}
var _2f,dir=_2e>0?-1:1,_30=parseInt(this._focusedNode.getAttribute("tabindex"),10)+dir;
while(_30>=-3&&_30<0){
_2f=_9("[tabindex="+_30+"]",this._pager.domNode)[0];
if(_2f){
break;
}else{
_30+=dir;
}
}
if(!_2f){
this._isFocused=false;
if(_6.hasClass(this._focusedNode,"dojoxGridButtonFocus")){
_6.removeClass(this._focusedNode,"dojoxGridButtonFocus");
}
}
return _2f?false:true;
},_onMove:function(_31,_32,evt){
if(this._focusedNode){
var _33=this._focusedNode.getAttribute("tabindex"),_34=_32==1?"nextSibling":"previousSibling",_35=this._focusedNode[_34];
while(_35){
if(_35.getAttribute("tabindex")==_33){
this._focus(_35);
break;
}
_35=_35[_34];
}
}
},_focus:function(_36,evt){
if(_36){
this._isFocused=true;
if(_1.isIE&&this._focusedNode){
_6.removeClass(this._focusedNode,"dojoxGridButtonFocus");
}
this._focusedNode=_36;
_36.focus();
if(_1.isIE){
_6.addClass(_36,"dojoxGridButtonFocus");
}
_1a(evt);
return true;
}
return false;
},_onActive:function(e){
this._focusedNode=e.target;
if(!this._isFocused){
this._pager.plugin.grid.focus.focusArea("pagination"+this._pager.position);
}
},_moveFocus:function(){
if(this._focusedNode&&!this._focusedNode.getAttribute("tabindex")){
var _37=this._focusedNode.nextSibling;
while(_37){
if(_37.getAttribute("tabindex")){
this._focus(_37);
return;
}
_37=_37.nextSibling;
}
var _38=this._focusedNode.previousSibling;
while(_38){
if(_38.getAttribute("tabindex")){
this._focus(_38);
return;
}
_38=_38.previousSibling;
}
this._focusedNode=null;
this._onBlur();
}else{
if(_1.isIE&&this._focusedNode){
_6.addClass(this._focusedNode,"dojoxGridButtonFocus");
}
}
}});
var _39=_2("dojox.grid.enhanced.plugins._Paginator",[_15,_16],{templateString:_d,constructor:function(_3a){
_5.mixin(this,_3a);
this.grid=this.plugin.grid;
},postCreate:function(){
this.inherited(arguments);
var _3b=this,g=this.grid;
this.plugin.connect(g,"_resize",_5.hitch(this,"_resetGridHeight"));
this._originalResize=_5.hitch(g,"resize");
g.resize=function(_3c,_3d){
_3b._changeSize=_3c;
_3b._resultSize=_3d;
_3b._originalResize();
};
this.focus=_27(this);
this._placeSelf();
},destroy:function(){
this.inherited(arguments);
this.grid.focus.removeArea("pagination"+this.position);
if(this._gotoPageDialog){
this._gotoPageDialog.destroy();
}
this.grid.resize=this._originalResize;
},onSwitchPageSize:function(evt){
},onPageStep:function(evt){
},onShowGotoPageDialog:function(evt){
},_update:function(){
this._updateDescription();
this._updatePageStepper();
this._updateSizeSwitch();
this._updateGotoButton();
},_registerFocus:function(_3e){
var _3f=this.grid.focus,_40="pagination"+this.position,f=this.focus;
_3f.addArea({name:_40,onFocus:_5.hitch(this.focus,"_onFocus"),onBlur:_5.hitch(this.focus,"_onBlur"),onMove:_5.hitch(this.focus,"_onMove")});
_3f.placeArea(_40,_3e?"before":"after",_3e?"header":"content");
},_placeSelf:function(){
var g=this.grid,_41=this.position=="top";
this.placeAt(_41?g.viewsHeaderNode:g.viewsNode,_41?"before":"after");
this._registerFocus(_41);
},_resetGridHeight:function(_42,_43){
var g=this.grid;
_42=_42||this._changeSize;
_43=_43||this._resultSize;
delete this._changeSize;
delete this._resultSize;
if(g._autoHeight){
return;
}
var _44=g._getPadBorder().h;
if(!this.plugin.gh){
this.plugin.gh=_6.contentBox(g.domNode).h+2*_44;
}
if(_43){
_42=_43;
}
if(_42){
this.plugin.gh=_6.contentBox(g.domNode).h+2*_44;
}
var gh=this.plugin.gh,hh=g._getHeaderHeight(),ph=_6.marginBox(this.domNode).h;
if(typeof g.autoHeight==="number"){
var cgh=gh+ph-_44;
_6.style(g.domNode,"height",cgh+"px");
_6.style(g.viewsNode,"height",(cgh-ph-hh)+"px");
this._styleMsgNode(hh,_6.marginBox(g.viewsNode).w,cgh-ph-hh);
}else{
var h=gh-ph-hh-_44;
_6.style(g.viewsNode,"height",h+"px");
var _45=_3.some(g.views.views,function(v){
return v.hasHScrollbar();
});
_3.forEach(g.viewsNode.childNodes,function(c){
_6.style(c,"height",h+"px");
});
_3.forEach(g.views.views,function(v){
if(v.scrollboxNode){
if(!v.hasHScrollbar()&&_45){
_6.style(v.scrollboxNode,"height",(h-_18.getScrollbar().h)+"px");
}else{
_6.style(v.scrollboxNode,"height",h+"px");
}
}
});
this._styleMsgNode(hh,_6.marginBox(g.viewsNode).w,h);
}
},_styleMsgNode:function(top,_46,_47){
var _48=this.grid.messagesNode;
_6.style(_48,{"position":"absolute","top":top+"px","width":_46+"px","height":_47+"px","z-Index":"100"});
},_updateDescription:function(){
var s=this.plugin.forcePageStoreLayer,_49=this.plugin._maxSize,nls=this.plugin._nls,_4a=function(){
return _49<=0||_49==1?nls[5]:nls[4];
};
if(this.description&&this.descriptionDiv){
this.descriptionDiv.innerHTML=_49>0?_a.substitute(nls[0],[_4a(),_49,s.startIdx+1,s.endIdx+1]):"0 "+_4a();
}
},_updateSizeSwitch:function(){
_6.style(this.sizeSwitchTd,"display",this.sizeSwitch?"":"none");
if(!this.sizeSwitch){
return;
}
if(this.sizeSwitchTd.childNodes.length<1){
this._createSizeSwitchNodes();
}
this._updateSwitchNodesStyle();
},_createSizeSwitchNodes:function(){
var _4b=null,nls=this.plugin._nls,_4=_5.hitch(this.plugin,"connect");
_3.forEach(this.pageSizes,function(_4c){
var _4d=isFinite(_4c)?_a.substitute(nls[2],[_4c]):nls[1],_4e=isFinite(_4c)?_4c:nls[16];
_4b=_6.create("span",{innerHTML:_4e,title:_4d,value:_4c,tabindex:"-1"},this.sizeSwitchTd,"last");
_4b.setAttribute("aria-label",_4d);
_4(_4b,"onclick",_5.hitch(this,"_onSwitchPageSize"));
_4(_4b,"onkeydown",_5.hitch(this,"_onSwitchPageSize"));
_4(_4b,"onmouseover",function(e){
_6.addClass(e.target,"dojoxGridPageTextHover");
});
_4(_4b,"onmouseout",function(e){
_6.removeClass(e.target,"dojoxGridPageTextHover");
});
_4b=_6.create("span",{innerHTML:"|"},this.sizeSwitchTd,"last");
_6.addClass(_4b,"dojoxGridSeparator");
},this);
_6.destroy(_4b);
},_updateSwitchNodesStyle:function(){
var _4f=null;
var _50=function(_51,_52){
if(_52){
_6.addClass(_51,"dojoxGridActivedSwitch");
_6.removeAttr(_51,"tabindex");
}else{
_6.addClass(_51,"dojoxGridInactiveSwitch");
_51.setAttribute("tabindex","-1");
}
};
_3.forEach(this.sizeSwitchTd.childNodes,function(_53){
if(_53.value){
_6.removeClass(_53);
_4f=_53.value;
if(this.plugin._showAll){
_50(_53,isNaN(parseInt(_4f,10)));
}else{
_50(_53,this.plugin._currentPageSize==_4f);
}
}
},this);
},_updatePageStepper:function(){
_6.style(this.pageStepperTd,"display",this.pageStepper?"":"none");
if(!this.pageStepper){
return;
}
if(this.pageStepperDiv.childNodes.length<1){
this._createPageStepNodes();
this._createWardBtns();
}else{
this._resetPageStepNodes();
}
this._updatePageStepNodesStyle();
},_createPageStepNodes:function(){
var _54=this._getStartPage(),_55=this._getStepPageSize(),_56="",_57=null,i=_54,_4=_5.hitch(this.plugin,"connect");
for(;i<_54+this.maxPageStep+1;i++){
_56=_a.substitute(this.plugin._nls[3],[i]);
_57=_6.create("div",{innerHTML:i,value:i,title:_56},this.pageStepperDiv,"last");
_57.setAttribute("aria-label",_56);
_4(_57,"onclick",_5.hitch(this,"_onPageStep"));
_4(_57,"onkeydown",_5.hitch(this,"_onPageStep"));
_4(_57,"onmouseover",function(e){
_6.addClass(e.target,"dojoxGridPageTextHover");
});
_4(_57,"onmouseout",function(e){
_6.removeClass(e.target,"dojoxGridPageTextHover");
});
_6.style(_57,"display",i<_54+_55?"":"none");
}
},_createWardBtns:function(){
var _58=this,nls=this.plugin._nls;
var _59={prevPage:"&#60;",firstPage:"&#171;",nextPage:"&#62;",lastPage:"&#187;"};
var _5a=function(_5b,_5c,_5d){
var _5e=_6.create("div",{value:_5b,title:_5c,tabindex:"-2"},_58.pageStepperDiv,_5d);
_58.plugin.connect(_5e,"onclick",_5.hitch(_58,"_onPageStep"));
_58.plugin.connect(_5e,"onkeydown",_5.hitch(_58,"_onPageStep"));
_5e.setAttribute("aria-label",_5c);
var _5f=_6.create("span",{value:_5b,title:_5c,innerHTML:_59[_5b]},_5e,_5d);
_6.addClass(_5f,"dojoxGridWardButtonInner");
};
_5a("prevPage",nls[6],"first");
_5a("firstPage",nls[7],"first");
_5a("nextPage",nls[8],"last");
_5a("lastPage",nls[9],"last");
},_resetPageStepNodes:function(){
var _60=this._getStartPage(),_61=this._getStepPageSize(),_62=this.pageStepperDiv.childNodes,_63=null,i=_60,j=2,tip;
for(;j<_62.length-2;j++,i++){
_63=_62[j];
if(i<_60+_61){
tip=_a.substitute(this.plugin._nls[3],[i]);
_6.attr(_63,{"innerHTML":i,"title":tip,"value":i});
_6.style(_63,"display","");
_63.setAttribute("aria-label",tip);
}else{
_6.style(_63,"display","none");
}
}
},_updatePageStepNodesStyle:function(){
var _64=null,_65=this.plugin.currentPage(),_66=this.plugin.getTotalPageNum();
var _67=function(_68,_69,_6a){
var _6b=_68.value,_6c=_69?"dojoxGrid"+_6b+"Btn":"dojoxGridInactived",_6d=_69?"dojoxGrid"+_6b+"BtnDisable":"dojoxGridActived";
if(_6a){
_6.addClass(_68,_6d);
_6.removeAttr(_68,"tabindex");
}else{
_6.addClass(_68,_6c);
_68.setAttribute("tabindex","-2");
}
};
_3.forEach(this.pageStepperDiv.childNodes,function(_6e){
_6.removeClass(_6e);
if(isNaN(parseInt(_6e.value,10))){
_6.addClass(_6e,"dojoxGridWardButton");
var _6f=_6e.value=="prevPage"||_6e.value=="firstPage"?1:_66;
_67(_6e,true,(_65===_6f));
}else{
_64=parseInt(_6e.value,10);
_67(_6e,false,(_64===_65||_6.style(_6e,"display")==="none"));
}
},this);
},_showGotoButton:function(_70){
this.gotoButton=_70;
this._updateGotoButton();
},_updateGotoButton:function(){
if(!this.gotoButton){
if(this._gotoPageDialog){
this._gotoPageDialog.destroy();
}
_6.removeAttr(this.gotoPageDiv,"tabindex");
_6.style(this.gotoPageTd,"display","none");
return;
}
if(_6.style(this.gotoPageTd,"display")=="none"){
_6.style(this.gotoPageTd,"display","");
}
this.gotoPageDiv.setAttribute("title",this.plugin._nls[10]);
_6.toggleClass(this.gotoPageDiv,"dojoxGridPaginatorGotoDivDisabled",this.plugin.getTotalPageNum()<=1);
if(this.plugin.getTotalPageNum()<=1){
_6.removeAttr(this.gotoPageDiv,"tabindex");
}else{
this.gotoPageDiv.setAttribute("tabindex","-3");
}
},_openGotopageDialog:function(e){
if(this.plugin.getTotalPageNum()<=1){
return;
}
if(e.type==="keydown"&&e.keyCode!==_c.ENTER&&e.keyCode!==_c.SPACE){
return;
}
if(!this._gotoPageDialog){
this._gotoPageDialog=new _1b(this.plugin);
}
this._gotoPageDialog.showDialog();
this.onShowGotoPageDialog(e);
},_onSwitchPageSize:function(e){
if(e.type==="keydown"&&e.keyCode!==_c.ENTER&&e.keyCode!==_c.SPACE){
return;
}
this.onSwitchPageSize(e);
this.plugin.currentPageSize(e.target.value);
},_onPageStep:function(e){
if(e.type==="keydown"&&e.keyCode!==_c.ENTER&&e.keyCode!==_c.SPACE){
return;
}
var p=this.plugin,_71=e.target.value;
this.onPageStep(e);
if(!isNaN(parseInt(_71,10))){
p.currentPage(parseInt(_71,10));
}else{
p[_71]();
}
},_getStartPage:function(){
var cp=this.plugin.currentPage(),ms=this.maxPageStep,hs=parseInt(ms/2,10),tp=this.plugin.getTotalPageNum();
if(cp<hs||(cp-hs)<1||tp<=ms){
return 1;
}else{
return tp-cp<hs&&cp-ms>=0?tp-ms+1:cp-hs;
}
},_getStepPageSize:function(){
var sp=this._getStartPage(),tp=this.plugin.getTotalPageNum(),ms=this.maxPageStep;
return sp+ms>tp?tp-sp+1:ms;
}});
var _72=_2("dojox.grid.enhanced.plugins.Pagination",_10,{name:"pagination",defaultPageSize:25,defaultPage:1,description:true,sizeSwitch:true,pageStepper:true,gotoButton:false,pageSizes:[10,25,50,100,Infinity],maxPageStep:7,position:"bottom",init:function(){
var g=this.grid;
g.usingPagination=true;
this._initOptions();
this._currentPage=this.defaultPage;
this._currentPageSize=this.grid.rowsPerPage=this.defaultPageSize;
this._store=g.store;
this.forcePageStoreLayer=new _1d(this);
_f.wrap(g,"_storeLayerFetch",this.forcePageStoreLayer);
this._paginator=this.option.position!="top"?new _39(_5.mixin(this.option,{position:"bottom",plugin:this})):new _39(_5.mixin(this.option,{position:"top",plugin:this}));
this._regApis();
},destroy:function(){
this.inherited(arguments);
this._paginator.destroy();
var g=this.grid;
g.unwrap(this.forcePageStoreLayer.name());
g.scrollToRow=this._gridOriginalfuncs[0];
g._onNew=this._gridOriginalfuncs[1];
g.removeSelectedRows=this._gridOriginalfuncs[2];
this._paginator=null;
this._nls=null;
},currentPage:function(_73){
if(_73<=this.getTotalPageNum()&&_73>0&&this._currentPage!==_73){
this._currentPage=_73;
this.grid._refresh(true);
this.grid.resize();
}
return this._currentPage;
},nextPage:function(){
this.currentPage(this._currentPage+1);
},prevPage:function(){
this.currentPage(this._currentPage-1);
},firstPage:function(){
this.currentPage(1);
},lastPage:function(){
this.currentPage(this.getTotalPageNum());
},currentPageSize:function(_74){
if(!isNaN(_74)){
var g=this.grid,_75=this._currentPageSize*(this._currentPage-1),_76;
this._showAll=!isFinite(_74);
this.grid.usingPagination=!this._showAll;
this._currentPageSize=this._showAll?this._maxSize:_74;
g.rowsPerPage=this._showAll?this._defaultRowsPerPage:_74;
_76=_75+Math.min(this._currentPageSize,this._maxSize);
if(_76>this._maxSize){
this.lastPage();
}else{
var cp=Math.ceil(_75/this._currentPageSize)+1;
if(cp!==this._currentPage){
this.currentPage(cp);
}else{
this.grid._refresh(true);
}
}
this.grid.resize();
}
return this._currentPageSize;
},getTotalPageNum:function(){
return Math.ceil(this._maxSize/this._currentPageSize);
},getTotalRowCount:function(){
return this._maxSize;
},scrollToRow:function(_77){
var _78=parseInt(_77/this._currentPageSize,10)+1;
if(_78>this.getTotalPageNum()){
return;
}
this.currentPage(_78);
var _79=_77%this._currentPageSize;
return this._gridOriginalfuncs[0](_79);
},removeSelectedRows:function(){
this._multiRemoving=true;
this._gridOriginalfuncs[2].apply();
this._multiRemoving=false;
this.grid.resize();
this.grid._refresh();
},showGotoPageButton:function(_7a){
this._paginator.gotoButton=_7a;
this._paginator._updateGotoButton();
},gotoPage:function(_7b){
_1.deprecated("dojox.grid.enhanced.EnhancedGrid.gotoPage(page)","use dojox.grid.enhanced.EnhancedGrid.currentPage(page) instead","1.8");
this.currentPage(_7b);
},gotoFirstPage:function(){
_1.deprecated("dojox.grid.enhanced.EnhancedGrid.gotoFirstPage()","use dojox.grid.enhanced.EnhancedGrid.firstPage() instead","1.8");
this.firstPage();
},gotoLastPage:function(){
_1.deprecated("dojox.grid.enhanced.EnhancedGrid.gotoLastPage()","use dojox.grid.enhanced.EnhancedGrid.lastPage() instead","1.8");
this.lastPage();
},changePageSize:function(_7c){
_1.deprecated("dojox.grid.enhanced.EnhancedGrid.changePageSize(size)","use dojox.grid.enhanced.EnhancedGrid.currentPageSize(size) instead","1.8");
this.currentPageSize(_7c);
},_nls:null,_showAll:false,_maxSize:0,_defaultRowsPerPage:25,_currentPage:1,_currentPageSize:25,_initOptions:function(){
this._defaultRowsPerPage=this.grid.rowsPerPage||25;
this.defaultPage=this.option.defaultPage>=1?parseInt(this.option.defaultPage,10):1;
this.option.description=this.option.description!==undefined?!!this.option.description:this.description;
this.option.sizeSwitch=this.option.sizeSwitch!==undefined?!!this.option.sizeSwitch:this.sizeSwitch;
this.option.pageStepper=this.option.pageStepper!==undefined?!!this.option.pageStepper:this.pageStepper;
this.option.gotoButton=this.option.gotoButton!==undefined?!!this.option.gotoButton:this.gotoButton;
if(_5.isArray(this.option.pageSizes)){
var _7d=[];
_3.forEach(this.option.pageSizes,function(_7e){
_7e=typeof _7e=="number"?_7e:parseInt(_7e,10);
if(!isNaN(_7e)&&_7e>0){
_7d.push(_7e);
}else{
if(_3.indexOf(_7d,Infinity)<0){
_7d.push(Infinity);
}
}
},this);
this.option.pageSizes=_7d.sort(function(a,b){
return a-b;
});
}else{
this.option.pageSizes=this.pageSizes;
}
this.defaultPageSize=this.option.defaultPageSize>=1?parseInt(this.option.defaultPageSize,10):this.pageSizes[0];
this.option.maxPageStep=this.option.maxPageStep>0?this.option.maxPageStep:this.maxPageStep;
this.option.position=_5.isString(this.option.position)?this.option.position.toLowerCase():this.position;
var nls=_b.getLocalization("dojox.grid.enhanced","Pagination");
this._nls=[nls.descTemplate,nls.allItemsLabelTemplate,nls.pageSizeLabelTemplate,nls.pageStepLabelTemplate,nls.itemTitle,nls.singularItemTitle,nls.prevTip,nls.firstTip,nls.nextTip,nls.lastTip,nls.gotoButtonTitle,nls.dialogTitle,nls.dialogIndication,nls.pageCountIndication,nls.dialogConfirm,nls.dialogCancel,nls.all];
},_regApis:function(){
var g=this.grid;
g.currentPage=_5.hitch(this,this.currentPage);
g.nextPage=_5.hitch(this,this.nextPage);
g.prevPage=_5.hitch(this,this.prevPage);
g.firstPage=_5.hitch(this,this.firstPage);
g.lastPage=_5.hitch(this,this.lastPage);
g.currentPageSize=_5.hitch(this,this.currentPageSize);
g.showGotoPageButton=_5.hitch(this,this.showGotoPageButton);
g.getTotalRowCount=_5.hitch(this,this.getTotalRowCount);
g.getTotalPageNum=_5.hitch(this,this.getTotalPageNum);
g.gotoPage=_5.hitch(this,this.gotoPage);
g.gotoFirstPage=_5.hitch(this,this.gotoFirstPage);
g.gotoLastPage=_5.hitch(this,this.gotoLastPage);
g.changePageSize=_5.hitch(this,this.changePageSize);
this._gridOriginalfuncs=[_5.hitch(g,g.scrollToRow),_5.hitch(g,g._onNew),_5.hitch(g,g.removeSelectedRows)];
g.scrollToRow=_5.hitch(this,this.scrollToRow);
g.removeSelectedRows=_5.hitch(this,this.removeSelectedRows);
g._onNew=_5.hitch(this,this._onNew);
this.connect(g,"_onDelete",_5.hitch(this,this._onDelete));
},_onNew:function(_7f,_80){
var _81=this.getTotalPageNum();
if(((this._currentPage===_81||_81===0)&&this.grid.rowCount<this._currentPageSize)||this._showAll){
_5.hitch(this.grid,this._gridOriginalfuncs[1])(_7f,_80);
this.forcePageStoreLayer.endIdx++;
}
this._maxSize++;
if(this._showAll){
this._currentPageSize++;
}
if(this._showAll&&this.grid.autoHeight){
this.grid._refresh();
}else{
this._paginator.update();
}
},_onDelete:function(){
if(!this._multiRemoving){
this.grid.resize();
if(this._showAll){
this.grid._refresh();
}
}
if(this.grid.get("rowCount")===0){
this.prevPage();
}
}});
_11.registerPlugin(_72);
return _72;
});
