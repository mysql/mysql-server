//>>built
require({cache:{"url:dojox/grid/enhanced/templates/Pagination.html":"<div dojoAttachPoint=\"paginatorBar\"\n\t><table cellpadding=\"0\" cellspacing=\"0\"  class=\"dojoxGridPaginator\"\n\t\t><tr\n\t\t\t><td dojoAttachPoint=\"descriptionTd\" class=\"dojoxGridDescriptionTd\"\n\t\t\t\t><div dojoAttachPoint=\"descriptionDiv\" class=\"dojoxGridDescription\"></div\n\t\t\t></div></td\n\t\t\t><td dojoAttachPoint=\"sizeSwitchTd\"></td\n\t\t\t><td dojoAttachPoint=\"pageStepperTd\" class=\"dojoxGridPaginatorFastStep\"\n\t\t\t\t><div dojoAttachPoint=\"pageStepperDiv\" class=\"dojoxGridPaginatorStep\"></div\n\t\t\t></td\n\t\t\t><td dojoAttachPoint=\"gotoPageTd\" class=\"dojoxGridPaginatorGotoTd\"\n\t\t\t\t><div dojoAttachPoint=\"gotoPageDiv\" class=\"dojoxGridPaginatorGotoDiv\" dojoAttachEvent=\"onclick:_openGotopageDialog, onkeydown:_openGotopageDialog\"\n\t\t\t\t\t><span class=\"dojoxGridWardButtonInner\">&#8869;</span\n\t\t\t\t></div\n\t\t\t></td\n\t\t></tr\n\t></table\n></div>\n"}});
define("dojox/grid/enhanced/plugins/Pagination",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojo/_base/lang","dojo/_base/html","dojo/_base/event","dojo/query","dojo/string","dojo/keys","dojo/text!../templates/Pagination.html","./Dialog","./_StoreLayer","../_Plugin","../../EnhancedGrid","dijit/form/Button","dijit/form/NumberTextBox","dijit/focus","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dojox/html/metrics","dojo/i18n!../nls/Pagination"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,nls){
var _17=_2("dojox.grid.enhanced.plugins.pagination._GotoPagePane",[_13,_14,_15],{templateString:"<div>"+"<div class='dojoxGridDialogMargin' dojoAttachPoint='_mainMsgNode'></div>"+"<div class='dojoxGridDialogMargin'>"+"<input dojoType='dijit.form.NumberTextBox' style='width: 50px;' dojoAttachPoint='_pageInputBox' dojoAttachEvent='onKeyUp: _onKey'></input>"+"<label dojoAttachPoint='_pageLabelNode'></label>"+"</div>"+"<div class='dojoxGridDialogButton'>"+"<button dojoType='dijit.form.Button' dojoAttachPoint='_confirmBtn' dojoAttachEvent='onClick: _onConfirm'></button>"+"<button dojoType='dijit.form.Button' dojoAttachPoint='_cancelBtn' dojoAttachEvent='onClick: _onCancel'></button>"+"</div>"+"</div>",widgetsInTemplate:true,dlg:null,postMixInProperties:function(){
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
_18(evt);
},_onCancel:function(evt){
this._pageInputBox.reset();
this.dlg._gotoPageDialog.hide();
_18(evt);
},_onKey:function(evt){
this._confirmBtn.set("disabled",!this._pageInputBox.isValid()||this._pageInputBox.getDisplayedValue()=="");
if(!evt.altKey&&!evt.metaKey&&evt.keyCode===_a.ENTER){
this._onConfirm(evt);
}
}});
var _19=_2("dojox.grid.enhanced.plugins.pagination._GotoPageDialog",null,{pageCount:0,dlgPane:null,constructor:function(_1a){
this.plugin=_1a;
this.dlgPane=new _17({"dlg":this});
this.dlgPane.startup();
this._gotoPageDialog=new _c({"refNode":_1a.grid.domNode,"title":this.plugin._nls[11],"content":this.dlgPane});
this._gotoPageDialog.startup();
},_updatePageCount:function(){
this.pageCount=this.plugin.getTotalPageNum();
this.dlgPane._pageInputBox.constraints={fractional:false,min:1,max:this.pageCount};
this.dlgPane._pageLabelNode.innerHTML=_9.substitute(this.plugin._nls[13],[this.pageCount]);
},showDialog:function(){
this._updatePageCount();
this._gotoPageDialog.show();
},destroy:function(){
this._gotoPageDialog.destroy();
}});
var _1b=_2("dojox.grid.enhanced.plugins._ForcedPageStoreLayer",_d._StoreLayer,{tags:["presentation"],constructor:function(_1c){
this._plugin=_1c;
},_fetch:function(_1d){
var _1e=this,_1f=_1e._plugin,_20=_1f.grid,_21=_1d.scope||_1.global,_22=_1d.onBegin;
_1d.start=(_1f._currentPage-1)*_1f._currentPageSize+_1d.start;
_1e.startIdx=_1d.start;
_1e.endIdx=_1d.start+_1f._currentPageSize-1;
var p=_1f._paginator;
if(!_1f._showAll){
_1f._showAll=!p.sizeSwitch&&!p.pageStepper&&!p.gotoButton;
}
if(_22&&_1f._showAll){
_1d.onBegin=function(_23,req){
_1f._maxSize=_1f._currentPageSize=_23;
_1e.startIdx=0;
_1e.endIdx=_23-1;
_1f._paginator._update();
req.onBegin=_22;
req.onBegin.call(_21,_23,req);
};
}else{
if(_22){
_1d.onBegin=function(_24,req){
req.start=0;
req.count=_1f._currentPageSize;
_1f._maxSize=_24;
_1e.endIdx=_1e.endIdx>=_24?(_24-1):_1e.endIdx;
if(_1e.startIdx>_24&&_24!==0){
_20._pending_requests[req.start]=false;
_1f.firstPage();
}
_1f._paginator._update();
req.onBegin=_22;
req.onBegin.call(_21,Math.min(_1f._currentPageSize,(_24-_1e.startIdx)),req);
};
}
}
return _5.hitch(this._store,this._originFetch)(_1d);
}});
var _18=function(evt){
try{
if(evt){
_7.stop(evt);
}
}
catch(e){
}
};
var _25=_2("dojox.grid.enhanced.plugins.pagination._Focus",null,{_focusedNode:null,_isFocused:false,constructor:function(_26){
this._pager=_26;
var _27=_26.plugin.grid.focus;
_26.plugin.connect(_26,"onSwitchPageSize",_5.hitch(this,"_onActive"));
_26.plugin.connect(_26,"onPageStep",_5.hitch(this,"_onActive"));
_26.plugin.connect(_26,"onShowGotoPageDialog",_5.hitch(this,"_onActive"));
_26.plugin.connect(_26,"_update",_5.hitch(this,"_moveFocus"));
},_onFocus:function(evt,_28){
var _29,_2a;
if(!this._isFocused){
_29=this._focusedNode||_8("[tabindex]",this._pager.domNode)[0];
}else{
if(_28&&this._focusedNode){
var dir=_28>0?-1:1,_2b=parseInt(this._focusedNode.getAttribute("tabindex"),10)+dir;
while(_2b>=-3&&_2b<0){
_29=_8("[tabindex="+_2b+"]",this._pager.domNode)[0];
if(_29){
break;
}else{
_2b+=dir;
}
}
}
}
return this._focus(_29,evt);
},_onBlur:function(evt,_2c){
if(!_2c||!this._focusedNode){
this._isFocused=false;
if(this._focusedNode&&_6.hasClass(this._focusedNode,"dojoxGridButtonFocus")){
_6.removeClass(this._focusedNode,"dojoxGridButtonFocus");
}
return true;
}
var _2d,dir=_2c>0?-1:1,_2e=parseInt(this._focusedNode.getAttribute("tabindex"),10)+dir;
while(_2e>=-3&&_2e<0){
_2d=_8("[tabindex="+_2e+"]",this._pager.domNode)[0];
if(_2d){
break;
}else{
_2e+=dir;
}
}
if(!_2d){
this._isFocused=false;
if(_6.hasClass(this._focusedNode,"dojoxGridButtonFocus")){
_6.removeClass(this._focusedNode,"dojoxGridButtonFocus");
}
}
return _2d?false:true;
},_onMove:function(_2f,_30,evt){
if(this._focusedNode){
var _31=this._focusedNode.getAttribute("tabindex"),_32=_30==1?"nextSibling":"previousSibling",_33=this._focusedNode[_32];
while(_33){
if(_33.getAttribute("tabindex")==_31){
this._focus(_33);
break;
}
_33=_33[_32];
}
}
},_focus:function(_34,evt){
if(_34){
this._isFocused=true;
if(_1.isIE&&this._focusedNode){
_6.removeClass(this._focusedNode,"dojoxGridButtonFocus");
}
this._focusedNode=_34;
_34.focus();
if(_1.isIE){
_6.addClass(_34,"dojoxGridButtonFocus");
}
_18(evt);
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
var _35=this._focusedNode.nextSibling;
while(_35){
if(_35.getAttribute("tabindex")){
this._focus(_35);
return;
}
_35=_35.nextSibling;
}
var _36=this._focusedNode.previousSibling;
while(_36){
if(_36.getAttribute("tabindex")){
this._focus(_36);
return;
}
_36=_36.previousSibling;
}
this._focusedNode=null;
this._onBlur();
}else{
if(_1.isIE&&this._focusedNode){
_6.addClass(this._focusedNode,"dojoxGridButtonFocus");
}
}
}});
var _37=_2("dojox.grid.enhanced.plugins._Paginator",[_13,_14],{templateString:_b,constructor:function(_38){
_5.mixin(this,_38);
this.grid=this.plugin.grid;
},postCreate:function(){
this.inherited(arguments);
var _39=this,g=this.grid;
this.plugin.connect(g,"_resize",_5.hitch(this,"_resetGridHeight"));
this._originalResize=g.resize;
g.resize=function(_3a,_3b){
_39._changeSize=_3a;
_39._resultSize=_3b;
_39._originalResize.apply(g,arguments);
};
this.focus=_25(this);
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
},_registerFocus:function(_3c){
var _3d=this.grid.focus,_3e="pagination"+this.position,f=this.focus;
_3d.addArea({name:_3e,onFocus:_5.hitch(this.focus,"_onFocus"),onBlur:_5.hitch(this.focus,"_onBlur"),onMove:_5.hitch(this.focus,"_onMove")});
_3d.placeArea(_3e,_3c?"before":"after",_3c?"header":"content");
},_placeSelf:function(){
var g=this.grid,_3f=this.position=="top";
this.placeAt(_3f?g.viewsHeaderNode:g.viewsNode,_3f?"before":"after");
this._registerFocus(_3f);
},_resetGridHeight:function(_40,_41){
var g=this.grid;
_40=_40||this._changeSize;
_41=_41||this._resultSize;
delete this._changeSize;
delete this._resultSize;
if(g._autoHeight){
return;
}
var _42=g._getPadBorder().h;
if(!this.plugin.gh){
this.plugin.gh=(g.domNode.clientHeight||_6.style(g.domNode,"height"))+2*_42;
}
if(_41){
_40=_41;
}
if(_40){
this.plugin.gh=_6.contentBox(g.domNode).h+2*_42;
}
var gh=this.plugin.gh,hh=g._getHeaderHeight(),ph=_6.marginBox(this.domNode).h;
if(typeof g.autoHeight==="number"){
var cgh=gh+ph-_42;
_6.style(g.domNode,"height",cgh+"px");
_6.style(g.viewsNode,"height",(cgh-ph-hh)+"px");
this._styleMsgNode(hh,_6.marginBox(g.viewsNode).w,cgh-ph-hh);
}else{
var h=gh-ph-hh-_42;
_6.style(g.viewsNode,"height",h+"px");
var _43=_3.some(g.views.views,function(v){
return v.hasHScrollbar();
});
_3.forEach(g.viewsNode.childNodes,function(c){
_6.style(c,"height",h+"px");
});
_3.forEach(g.views.views,function(v){
if(v.scrollboxNode){
if(!v.hasHScrollbar()&&_43){
_6.style(v.scrollboxNode,"height",(h-_16.getScrollbar().h)+"px");
}else{
_6.style(v.scrollboxNode,"height",h+"px");
}
}
});
this._styleMsgNode(hh,_6.marginBox(g.viewsNode).w,h);
}
},_styleMsgNode:function(top,_44,_45){
var _46=this.grid.messagesNode;
_6.style(_46,{"position":"absolute","top":top+"px","width":_44+"px","height":_45+"px","z-Index":"100"});
},_updateDescription:function(){
var s=this.plugin.forcePageStoreLayer,_47=this.plugin._maxSize,nls=this.plugin._nls,_48=function(){
return _47<=0||_47==1?nls[5]:nls[4];
};
if(this.description&&this.descriptionDiv){
this.descriptionDiv.innerHTML=_47>0?_9.substitute(nls[0],[_48(),_47,s.startIdx+1,s.endIdx+1]):"0 "+_48();
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
var _49=null,nls=this.plugin._nls,_4=_5.hitch(this.plugin,"connect");
_3.forEach(this.pageSizes,function(_4a){
var _4b=isFinite(_4a)?_9.substitute(nls[2],[_4a]):nls[1],_4c=isFinite(_4a)?_4a:nls[16];
_49=_6.create("span",{innerHTML:_4c,title:_4b,value:_4a,tabindex:"-1"},this.sizeSwitchTd,"last");
_49.setAttribute("aria-label",_4b);
_4(_49,"onclick",_5.hitch(this,"_onSwitchPageSize"));
_4(_49,"onkeydown",_5.hitch(this,"_onSwitchPageSize"));
_4(_49,"onmouseover",function(e){
_6.addClass(e.target,"dojoxGridPageTextHover");
});
_4(_49,"onmouseout",function(e){
_6.removeClass(e.target,"dojoxGridPageTextHover");
});
_49=_6.create("span",{innerHTML:"|"},this.sizeSwitchTd,"last");
_6.addClass(_49,"dojoxGridSeparator");
},this);
_6.destroy(_49);
},_updateSwitchNodesStyle:function(){
var _4d=null;
var _4e=function(_4f,_50){
if(_50){
_6.addClass(_4f,"dojoxGridActivedSwitch");
_6.removeAttr(_4f,"tabindex");
}else{
_6.addClass(_4f,"dojoxGridInactiveSwitch");
_4f.setAttribute("tabindex","-1");
}
};
_3.forEach(this.sizeSwitchTd.childNodes,function(_51){
if(_51.value){
_6.removeClass(_51);
_4d=_51.value;
if(this.plugin._showAll){
_4e(_51,isNaN(parseInt(_4d,10)));
}else{
_4e(_51,this.plugin._currentPageSize==_4d);
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
var _52=this._getStartPage(),_53=this._getStepPageSize(),_54="",_55=null,i=_52,_4=_5.hitch(this.plugin,"connect");
for(;i<_52+this.maxPageStep+1;i++){
_54=_9.substitute(this.plugin._nls[3],[i]);
_55=_6.create("div",{innerHTML:i,value:i,title:_54},this.pageStepperDiv,"last");
_55.setAttribute("aria-label",_54);
_4(_55,"onclick",_5.hitch(this,"_onPageStep"));
_4(_55,"onkeydown",_5.hitch(this,"_onPageStep"));
_4(_55,"onmouseover",function(e){
_6.addClass(e.target,"dojoxGridPageTextHover");
});
_4(_55,"onmouseout",function(e){
_6.removeClass(e.target,"dojoxGridPageTextHover");
});
_6.style(_55,"display",i<_52+_53?"":"none");
}
},_createWardBtns:function(){
var _56=this,nls=this.plugin._nls;
var _57={prevPage:"&#60;",firstPage:"&#171;",nextPage:"&#62;",lastPage:"&#187;"};
var _58=function(_59,_5a,_5b){
var _5c=_6.create("div",{value:_59,title:_5a,tabindex:"-2"},_56.pageStepperDiv,_5b);
_56.plugin.connect(_5c,"onclick",_5.hitch(_56,"_onPageStep"));
_56.plugin.connect(_5c,"onkeydown",_5.hitch(_56,"_onPageStep"));
_5c.setAttribute("aria-label",_5a);
var _5d=_6.create("span",{value:_59,title:_5a,innerHTML:_57[_59]},_5c,_5b);
_6.addClass(_5d,"dojoxGridWardButtonInner");
};
_58("prevPage",nls[6],"first");
_58("firstPage",nls[7],"first");
_58("nextPage",nls[8],"last");
_58("lastPage",nls[9],"last");
},_resetPageStepNodes:function(){
var _5e=this._getStartPage(),_5f=this._getStepPageSize(),_60=this.pageStepperDiv.childNodes,_61=null,i=_5e,j=2,tip;
for(;j<_60.length-2;j++,i++){
_61=_60[j];
if(i<_5e+_5f){
tip=_9.substitute(this.plugin._nls[3],[i]);
_6.attr(_61,{"innerHTML":i,"title":tip,"value":i});
_6.style(_61,"display","");
_61.setAttribute("aria-label",tip);
}else{
_6.style(_61,"display","none");
}
}
},_updatePageStepNodesStyle:function(){
var _62=null,_63=this.plugin.currentPage(),_64=this.plugin.getTotalPageNum();
var _65=function(_66,_67,_68){
var _69=_66.value,_6a=_67?"dojoxGrid"+_69+"Btn":"dojoxGridInactived",_6b=_67?"dojoxGrid"+_69+"BtnDisable":"dojoxGridActived";
if(_68){
_6.addClass(_66,_6b);
_6.removeAttr(_66,"tabindex");
}else{
_6.addClass(_66,_6a);
_66.setAttribute("tabindex","-2");
}
};
_3.forEach(this.pageStepperDiv.childNodes,function(_6c){
_6.removeClass(_6c);
if(isNaN(parseInt(_6c.value,10))){
_6.addClass(_6c,"dojoxGridWardButton");
var _6d=_6c.value=="prevPage"||_6c.value=="firstPage"?1:_64;
_65(_6c,true,(_63===_6d));
}else{
_62=parseInt(_6c.value,10);
_65(_6c,false,(_62===_63||_6.style(_6c,"display")==="none"));
}
},this);
},_showGotoButton:function(_6e){
this.gotoButton=_6e;
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
if(e.type==="keydown"&&e.keyCode!==_a.ENTER&&e.keyCode!==_a.SPACE){
return;
}
if(!this._gotoPageDialog){
this._gotoPageDialog=new _19(this.plugin);
}
this._gotoPageDialog.showDialog();
this.onShowGotoPageDialog(e);
},_onSwitchPageSize:function(e){
if(e.type==="keydown"&&e.keyCode!==_a.ENTER&&e.keyCode!==_a.SPACE){
return;
}
this.onSwitchPageSize(e);
this.plugin.currentPageSize(e.target.value);
},_onPageStep:function(e){
if(e.type==="keydown"&&e.keyCode!==_a.ENTER&&e.keyCode!==_a.SPACE){
return;
}
var p=this.plugin,_6f=e.target.value;
this.onPageStep(e);
if(!isNaN(parseInt(_6f,10))){
p.currentPage(parseInt(_6f,10));
}else{
p[_6f]();
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
var _70=_2("dojox.grid.enhanced.plugins.Pagination",_e,{name:"pagination",defaultPageSize:25,defaultPage:1,description:true,sizeSwitch:true,pageStepper:true,gotoButton:false,pageSizes:[10,25,50,100,Infinity],maxPageStep:7,position:"bottom",init:function(){
var g=this.grid;
g.usingPagination=true;
this._initOptions();
this._currentPage=this.defaultPage;
this._currentPageSize=this.grid.rowsPerPage=this.defaultPageSize;
this._store=g.store;
this.forcePageStoreLayer=new _1b(this);
_d.wrap(g,"_storeLayerFetch",this.forcePageStoreLayer);
this._paginator=this.option.position!="top"?new _37(_5.mixin(this.option,{position:"bottom",plugin:this})):new _37(_5.mixin(this.option,{position:"top",plugin:this}));
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
},currentPage:function(_71){
if(_71<=this.getTotalPageNum()&&_71>0&&this._currentPage!==_71){
this._currentPage=_71;
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
},currentPageSize:function(_72){
if(!isNaN(_72)){
var g=this.grid,_73=this._currentPageSize*(this._currentPage-1),_74;
this._showAll=!isFinite(_72);
this.grid.usingPagination=!this._showAll;
this._currentPageSize=this._showAll?this._maxSize:_72;
g.rowsPerPage=this._showAll?this._defaultRowsPerPage:_72;
_74=_73+Math.min(this._currentPageSize,this._maxSize);
if(_74>this._maxSize){
this.lastPage();
}else{
var cp=Math.ceil(_73/this._currentPageSize)+1;
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
},scrollToRow:function(_75){
var _76=parseInt(_75/this._currentPageSize,10)+1;
if(_76>this.getTotalPageNum()){
return;
}
this.currentPage(_76);
var _77=_75%this._currentPageSize;
return this._gridOriginalfuncs[0](_77);
},removeSelectedRows:function(){
this._multiRemoving=true;
this._gridOriginalfuncs[2].apply();
this._multiRemoving=false;
if(this.grid.store.save){
this.grid.store.save();
}
this.grid.resize();
this.grid._refresh();
},showGotoPageButton:function(_78){
this._paginator.gotoButton=_78;
this._paginator._updateGotoButton();
},gotoPage:function(_79){
_1.deprecated("dojox.grid.enhanced.EnhancedGrid.gotoPage(page)","use dojox.grid.enhanced.EnhancedGrid.currentPage(page) instead","1.8");
this.currentPage(_79);
},gotoFirstPage:function(){
_1.deprecated("dojox.grid.enhanced.EnhancedGrid.gotoFirstPage()","use dojox.grid.enhanced.EnhancedGrid.firstPage() instead","1.8");
this.firstPage();
},gotoLastPage:function(){
_1.deprecated("dojox.grid.enhanced.EnhancedGrid.gotoLastPage()","use dojox.grid.enhanced.EnhancedGrid.lastPage() instead","1.8");
this.lastPage();
},changePageSize:function(_7a){
_1.deprecated("dojox.grid.enhanced.EnhancedGrid.changePageSize(size)","use dojox.grid.enhanced.EnhancedGrid.currentPageSize(size) instead","1.8");
this.currentPageSize(_7a);
},_nls:null,_showAll:false,_maxSize:0,_defaultRowsPerPage:25,_currentPage:1,_currentPageSize:25,_initOptions:function(){
this._defaultRowsPerPage=this.grid.rowsPerPage||25;
this.defaultPage=this.option.defaultPage>=1?parseInt(this.option.defaultPage,10):1;
this.option.description=this.option.description!==undefined?!!this.option.description:this.description;
this.option.sizeSwitch=this.option.sizeSwitch!==undefined?!!this.option.sizeSwitch:this.sizeSwitch;
this.option.pageStepper=this.option.pageStepper!==undefined?!!this.option.pageStepper:this.pageStepper;
this.option.gotoButton=this.option.gotoButton!==undefined?!!this.option.gotoButton:this.gotoButton;
if(_5.isArray(this.option.pageSizes)){
var _7b=[];
_3.forEach(this.option.pageSizes,function(_7c){
_7c=typeof _7c=="number"?_7c:parseInt(_7c,10);
if(!isNaN(_7c)&&_7c>0){
_7b.push(_7c);
}else{
if(_3.indexOf(_7b,Infinity)<0){
_7b.push(Infinity);
}
}
},this);
this.option.pageSizes=_7b.sort(function(a,b){
return a-b;
});
}else{
this.option.pageSizes=this.pageSizes;
}
this.defaultPageSize=this.option.defaultPageSize>=1?parseInt(this.option.defaultPageSize,10):this.option.pageSizes[0];
this.option.maxPageStep=this.option.maxPageStep>0?this.option.maxPageStep:this.maxPageStep;
this.option.position=_5.isString(this.option.position)?this.option.position.toLowerCase():this.position;
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
},_onNew:function(_7d,_7e){
var _7f=this.getTotalPageNum();
if(((this._currentPage===_7f||_7f===0)&&this.grid.get("rowCount")<this._currentPageSize)||this._showAll){
_5.hitch(this.grid,this._gridOriginalfuncs[1])(_7d,_7e);
this.forcePageStoreLayer.endIdx++;
}
this._maxSize++;
if(this._showAll){
this._currentPageSize++;
}
if(this._showAll&&this.grid.autoHeight){
this.grid._refresh();
}else{
this._paginator._update();
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
_f.registerPlugin(_70);
return _70;
});
