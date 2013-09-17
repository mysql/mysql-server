//>>built
require({cache:{"url:dojox/grid/resources/_Grid.html":"<div hidefocus=\"hidefocus\" role=\"grid\" dojoAttachEvent=\"onmouseout:_mouseOut\">\n\t<div class=\"dojoxGridMasterHeader\" dojoAttachPoint=\"viewsHeaderNode\" role=\"presentation\"></div>\n\t<div class=\"dojoxGridMasterView\" dojoAttachPoint=\"viewsNode\" role=\"presentation\"></div>\n\t<div class=\"dojoxGridMasterMessages\" style=\"display: none;\" dojoAttachPoint=\"messagesNode\"></div>\n\t<span dojoAttachPoint=\"lastFocusNode\" tabindex=\"0\"></span>\n</div>\n"}});
define("dojox/grid/_Grid",["dojo/_base/kernel","../main","dojo/_base/declare","./_Events","./_Scroller","./_Layout","./_View","./_ViewManager","./_RowManager","./_FocusManager","./_EditManager","./Selection","./_RowSelector","./util","dijit/_Widget","dijit/_TemplatedMixin","dijit/CheckedMenuItem","dojo/text!./resources/_Grid.html","dojo/string","dojo/_base/array","dojo/_base/lang","dojo/_base/sniff","dojox/html/metrics","dojo/_base/html","dojo/query","dojo/dnd/common","dojo/i18n!dijit/nls/loading"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,has,_16,_17,_18){
if(!_1.isCopyKey){
_1.isCopyKey=_1.dnd.getCopyKeyState;
}
var _19=_3("dojox.grid._Grid",[_f,_10,_4],{templateString:_12,classTag:"dojoxGrid",rowCount:5,keepRows:75,rowsPerPage:25,autoWidth:false,initialWidth:"",autoHeight:"",rowHeight:0,autoRender:true,defaultHeight:"15em",height:"",structure:null,elasticView:-1,singleClickEdit:false,selectionMode:"extended",rowSelector:"",columnReordering:false,headerMenu:null,placeholderLabel:"GridColumns",selectable:false,_click:null,loadingMessage:"<span class='dojoxGridLoading'>${loadingState}</span>",errorMessage:"<span class='dojoxGridError'>${errorState}</span>",noDataMessage:"",escapeHTMLInData:true,formatterScope:null,editable:false,sortInfo:0,themeable:true,_placeholders:null,_layoutClass:_6,buildRendering:function(){
this.inherited(arguments);
if(!this.domNode.getAttribute("tabIndex")){
this.domNode.tabIndex="0";
}
this.createScroller();
this.createLayout();
this.createViews();
this.createManagers();
this.createSelection();
this.connect(this.selection,"onSelected","onSelected");
this.connect(this.selection,"onDeselected","onDeselected");
this.connect(this.selection,"onChanged","onSelectionChanged");
_16.initOnFontResize();
this.connect(_16,"onFontResize","textSizeChanged");
_e.funnelEvents(this.domNode,this,"doKeyEvent",_e.keyEvents);
if(this.selectionMode!="none"){
this.domNode.setAttribute("aria-multiselectable",this.selectionMode=="single"?"false":"true");
}
_17.addClass(this.domNode,this.classTag);
if(!this.isLeftToRight()){
_17.addClass(this.domNode,this.classTag+"Rtl");
}
},postMixInProperties:function(){
this.inherited(arguments);
var _1a=_1.i18n.getLocalization("dijit","loading",this.lang);
this.loadingMessage=_13.substitute(this.loadingMessage,_1a);
this.errorMessage=_13.substitute(this.errorMessage,_1a);
if(this.srcNodeRef&&this.srcNodeRef.style.height){
this.height=this.srcNodeRef.style.height;
}
this._setAutoHeightAttr(this.autoHeight,true);
this.lastScrollTop=this.scrollTop=0;
},postCreate:function(){
this._placeholders=[];
this._setHeaderMenuAttr(this.headerMenu);
this._setStructureAttr(this.structure);
this._click=[];
this.inherited(arguments);
if(this.domNode&&this.autoWidth&&this.initialWidth){
this.domNode.style.width=this.initialWidth;
}
if(this.domNode&&!this.editable){
_17.attr(this.domNode,"aria-readonly","true");
}
},destroy:function(){
this.domNode.onReveal=null;
this.domNode.onSizeChange=null;
delete this._click;
if(this.scroller){
this.scroller.destroy();
delete this.scroller;
}
this.edit.destroy();
delete this.edit;
this.views.destroyViews();
if(this.focus){
this.focus.destroy();
delete this.focus;
}
if(this.headerMenu&&this._placeholders.length){
_14.forEach(this._placeholders,function(p){
p.unReplace(true);
});
this.headerMenu.unBindDomNode(this.viewsHeaderNode);
}
this.inherited(arguments);
},_setAutoHeightAttr:function(ah,_1b){
if(typeof ah=="string"){
if(!ah||ah=="false"){
ah=false;
}else{
if(ah=="true"){
ah=true;
}else{
ah=window.parseInt(ah,10);
}
}
}
if(typeof ah=="number"){
if(isNaN(ah)){
ah=false;
}
if(ah<0){
ah=true;
}else{
if(ah===0){
ah=false;
}
}
}
this.autoHeight=ah;
if(typeof ah=="boolean"){
this._autoHeight=ah;
}else{
if(typeof ah=="number"){
this._autoHeight=(ah>=this.get("rowCount"));
}else{
this._autoHeight=false;
}
}
if(this._started&&!_1b){
this.render();
}
},_getRowCountAttr:function(){
return this.updating&&this.invalidated&&this.invalidated.rowCount!=undefined?this.invalidated.rowCount:this.rowCount;
},textSizeChanged:function(){
this.render();
},sizeChange:function(){
this.update();
},createManagers:function(){
this.rows=new _9(this);
this.focus=new _a(this);
this.edit=new _b(this);
},createSelection:function(){
this.selection=new _c(this);
},createScroller:function(){
this.scroller=new _5();
this.scroller.grid=this;
this.scroller.renderRow=_15.hitch(this,"renderRow");
this.scroller.removeRow=_15.hitch(this,"rowRemoved");
},createLayout:function(){
this.layout=new this._layoutClass(this);
this.connect(this.layout,"moveColumn","onMoveColumn");
},onMoveColumn:function(){
this.render();
},onResizeColumn:function(_1c){
},createViews:function(){
this.views=new _8(this);
this.views.createView=_15.hitch(this,"createView");
},createView:function(_1d,idx){
var c=_15.getObject(_1d);
var _1e=new c({grid:this,index:idx});
this.viewsNode.appendChild(_1e.domNode);
this.viewsHeaderNode.appendChild(_1e.headerNode);
this.views.addView(_1e);
_17.attr(this.domNode,"align",this.isLeftToRight()?"left":"right");
return _1e;
},buildViews:function(){
for(var i=0,vs;(vs=this.layout.structure[i]);i++){
this.createView(vs.type||_2._scopeName+".grid._View",i).setStructure(vs);
}
this.scroller.setContentNodes(this.views.getContentNodes());
},_setStructureAttr:function(_1f){
var s=_1f;
if(s&&_15.isString(s)){
_1.deprecated("dojox.grid._Grid.set('structure', 'objVar')","use dojox.grid._Grid.set('structure', objVar) instead","2.0");
s=_15.getObject(s);
}
this.structure=s;
if(!s){
if(this.layout.structure){
s=this.layout.structure;
}else{
return;
}
}
this.views.destroyViews();
this.focus.focusView=null;
if(s!==this.layout.structure){
this.layout.setStructure(s);
}
this._structureChanged();
},setStructure:function(_20){
_1.deprecated("dojox.grid._Grid.setStructure(obj)","use dojox.grid._Grid.set('structure', obj) instead.","2.0");
this._setStructureAttr(_20);
},getColumnTogglingItems:function(){
var _21,_22=[];
_21=_14.map(this.layout.cells,function(_23){
if(!_23.menuItems){
_23.menuItems=[];
}
var _24=this;
var _25=new _11({label:_23.name,checked:!_23.hidden,_gridCell:_23,onChange:function(_26){
if(_24.layout.setColumnVisibility(this._gridCell.index,_26)){
var _27=this._gridCell.menuItems;
if(_27.length>1){
_14.forEach(_27,function(_28){
if(_28!==this){
_28.setAttribute("checked",_26);
}
},this);
}
_26=_14.filter(_24.layout.cells,function(c){
if(c.menuItems.length>1){
_14.forEach(c.menuItems,"item.set('disabled', false);");
}else{
c.menuItems[0].set("disabled",false);
}
return !c.hidden;
});
if(_26.length==1){
_14.forEach(_26[0].menuItems,"item.set('disabled', true);");
}
}
},destroy:function(){
var _29=_14.indexOf(this._gridCell.menuItems,this);
this._gridCell.menuItems.splice(_29,1);
delete this._gridCell;
_11.prototype.destroy.apply(this,arguments);
}});
_23.menuItems.push(_25);
if(!_23.hidden){
_22.push(_25);
}
return _25;
},this);
if(_22.length==1){
_22[0].set("disabled",true);
}
return _21;
},_setHeaderMenuAttr:function(_2a){
if(this._placeholders&&this._placeholders.length){
_14.forEach(this._placeholders,function(p){
p.unReplace(true);
});
this._placeholders=[];
}
if(this.headerMenu){
this.headerMenu.unBindDomNode(this.viewsHeaderNode);
}
this.headerMenu=_2a;
if(!_2a){
return;
}
this.headerMenu.bindDomNode(this.viewsHeaderNode);
if(this.headerMenu.getPlaceholders){
this._placeholders=this.headerMenu.getPlaceholders(this.placeholderLabel);
}
},setHeaderMenu:function(_2b){
_1.deprecated("dojox.grid._Grid.setHeaderMenu(obj)","use dojox.grid._Grid.set('headerMenu', obj) instead.","2.0");
this._setHeaderMenuAttr(_2b);
},setupHeaderMenu:function(){
if(this._placeholders&&this._placeholders.length){
_14.forEach(this._placeholders,function(p){
if(p._replaced){
p.unReplace(true);
}
p.replace(this.getColumnTogglingItems());
},this);
}
},_fetch:function(_2c){
this.setScrollTop(0);
},getItem:function(_2d){
return null;
},showMessage:function(_2e){
if(_2e){
this.messagesNode.innerHTML=_2e;
this.messagesNode.style.display="";
}else{
this.messagesNode.innerHTML="";
this.messagesNode.style.display="none";
}
},_structureChanged:function(){
this.buildViews();
if(this.autoRender&&this._started){
this.render();
}
},hasLayout:function(){
return this.layout.cells.length;
},resize:function(_2f,_30){
if(_1.isIE&&!_2f&&!_30&&this._autoHeight){
return;
}
this._pendingChangeSize=_2f;
this._pendingResultSize=_30;
this.sizeChange();
},_getPadBorder:function(){
this._padBorder=this._padBorder||_17._getPadBorderExtents(this.domNode);
return this._padBorder;
},_getHeaderHeight:function(){
var vns=this.viewsHeaderNode.style,t=vns.display=="none"?0:this.views.measureHeader();
vns.height=t+"px";
this.views.normalizeHeaderNodeHeight();
return t;
},_resize:function(_31,_32){
_31=_31||this._pendingChangeSize;
_32=_32||this._pendingResultSize;
delete this._pendingChangeSize;
delete this._pendingResultSize;
if(!this.domNode){
return;
}
var pn=this.domNode.parentNode;
if(!pn||pn.nodeType!=1||!this.hasLayout()||pn.style.visibility=="hidden"||pn.style.display=="none"){
return;
}
var _33=this._getPadBorder();
var hh=undefined;
var h;
if(this._autoHeight){
this.domNode.style.height="auto";
}else{
if(typeof this.autoHeight=="number"){
h=hh=this._getHeaderHeight();
h+=(this.scroller.averageRowHeight*this.autoHeight);
this.domNode.style.height=h+"px";
}else{
if(this.domNode.clientHeight<=_33.h){
if(pn==document.body){
this.domNode.style.height=this.defaultHeight;
}else{
if(this.height){
this.domNode.style.height=this.height;
}else{
this.fitTo="parent";
}
}
}
}
}
if(_32){
_31=_32;
}
if(!this._autoHeight&&_31){
_17.marginBox(this.domNode,_31);
this.height=this.domNode.style.height;
delete this.fitTo;
}else{
if(this.fitTo=="parent"){
h=this._parentContentBoxHeight=this._parentContentBoxHeight||_17._getContentBox(pn).h;
this.domNode.style.height=Math.max(0,h)+"px";
}
}
var _34=_14.some(this.views.views,function(v){
return v.flexCells;
});
if(!this._autoHeight&&(h||_17._getContentBox(this.domNode).h)===0){
this.viewsHeaderNode.style.display="none";
}else{
this.viewsHeaderNode.style.display="block";
if(!_34&&hh===undefined){
hh=this._getHeaderHeight();
}
}
if(_34){
hh=undefined;
}
this.adaptWidth();
this.adaptHeight(hh);
this.postresize();
},adaptWidth:function(){
var _35=(!this.initialWidth&&this.autoWidth);
var w=_35?0:this.domNode.clientWidth||(this.domNode.offsetWidth-this._getPadBorder().w),vw=this.views.arrange(1,w);
this.views.onEach("adaptWidth");
if(_35){
this.domNode.style.width=vw+"px";
}
},adaptHeight:function(_36){
var t=_36===undefined?this._getHeaderHeight():_36;
var h=(this._autoHeight?-1:Math.max(this.domNode.clientHeight-t,0)||0);
this.views.onEach("setSize",[0,h]);
this.views.onEach("adaptHeight");
if(!this._autoHeight){
var _37=0,_38=0;
var _39=_14.filter(this.views.views,function(v){
var has=v.hasHScrollbar();
if(has){
_37++;
}else{
_38++;
}
return (!has);
});
if(_37>0&&_38>0){
_14.forEach(_39,function(v){
v.adaptHeight(true);
});
}
}
if(this.autoHeight===true||h!=-1||(typeof this.autoHeight=="number"&&this.autoHeight>=this.get("rowCount"))){
this.scroller.windowHeight=h;
}else{
this.scroller.windowHeight=Math.max(this.domNode.clientHeight-t,0);
}
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(this.autoRender){
this.render();
}
},render:function(){
if(!this.domNode){
return;
}
if(!this._started){
return;
}
if(!this.hasLayout()){
this.scroller.init(0,this.keepRows,this.rowsPerPage);
return;
}
this.update=this.defaultUpdate;
this._render();
},_render:function(){
this.scroller.init(this.get("rowCount"),this.keepRows,this.rowsPerPage);
this.prerender();
this.setScrollTop(0);
this.postrender();
},prerender:function(){
this.keepRows=this._autoHeight?0:this.keepRows;
this.scroller.setKeepInfo(this.keepRows);
this.views.render();
this._resize();
},postrender:function(){
this.postresize();
this.focus.initFocusView();
_17.setSelectable(this.domNode,this.selectable);
},postresize:function(){
if(this._autoHeight){
var _3a=Math.max(this.views.measureContent())+"px";
this.viewsNode.style.height=_3a;
}
},renderRow:function(_3b,_3c){
this.views.renderRow(_3b,_3c,this._skipRowRenormalize);
},rowRemoved:function(_3d){
this.views.rowRemoved(_3d);
},invalidated:null,updating:false,beginUpdate:function(){
this.invalidated=[];
this.updating=true;
},endUpdate:function(){
this.updating=false;
var i=this.invalidated,r;
if(i.all){
this.update();
}else{
if(i.rowCount!=undefined){
this.updateRowCount(i.rowCount);
}else{
for(r in i){
this.updateRow(Number(r));
}
}
}
this.invalidated=[];
},defaultUpdate:function(){
if(!this.domNode){
return;
}
if(this.updating){
this.invalidated.all=true;
return;
}
this.lastScrollTop=this.scrollTop;
this.prerender();
this.scroller.invalidateNodes();
this.setScrollTop(this.lastScrollTop);
this.postrender();
},update:function(){
this.render();
},updateRow:function(_3e){
_3e=Number(_3e);
if(this.updating){
this.invalidated[_3e]=true;
}else{
this.views.updateRow(_3e);
this.scroller.rowHeightChanged(_3e);
}
},updateRows:function(_3f,_40){
_3f=Number(_3f);
_40=Number(_40);
var i;
if(this.updating){
for(i=0;i<_40;i++){
this.invalidated[i+_3f]=true;
}
}else{
for(i=0;i<_40;i++){
this.views.updateRow(i+_3f,this._skipRowRenormalize);
}
this.scroller.rowHeightChanged(_3f);
}
},updateRowCount:function(_41){
if(this.updating){
this.invalidated.rowCount=_41;
}else{
this.rowCount=_41;
this._setAutoHeightAttr(this.autoHeight,true);
if(this.layout.cells.length){
this.scroller.updateRowCount(_41);
}
this._resize();
if(this.layout.cells.length){
this.setScrollTop(this.scrollTop);
}
}
},updateRowStyles:function(_42){
this.views.updateRowStyles(_42);
},getRowNode:function(_43){
if(this.focus.focusView&&!(this.focus.focusView instanceof _d)){
return this.focus.focusView.rowNodes[_43];
}else{
for(var i=0,_44;(_44=this.views.views[i]);i++){
if(!(_44 instanceof _d)){
return _44.rowNodes[_43];
}
}
}
return null;
},rowHeightChanged:function(_45){
this.views.renormalizeRow(_45);
this.scroller.rowHeightChanged(_45);
},fastScroll:true,delayScroll:false,scrollRedrawThreshold:(has("ie")?100:50),scrollTo:function(_46){
if(!this.fastScroll){
this.setScrollTop(_46);
return;
}
var _47=Math.abs(this.lastScrollTop-_46);
this.lastScrollTop=_46;
if(_47>this.scrollRedrawThreshold||this.delayScroll){
this.delayScroll=true;
this.scrollTop=_46;
this.views.setScrollTop(_46);
if(this._pendingScroll){
window.clearTimeout(this._pendingScroll);
}
var _48=this;
this._pendingScroll=window.setTimeout(function(){
delete _48._pendingScroll;
_48.finishScrollJob();
},200);
}else{
this.setScrollTop(_46);
}
},finishScrollJob:function(){
this.delayScroll=false;
this.setScrollTop(this.scrollTop);
},setScrollTop:function(_49){
this.scroller.scroll(this.views.setScrollTop(_49));
},scrollToRow:function(_4a){
this.setScrollTop(this.scroller.findScrollTop(_4a)+1);
},styleRowNode:function(_4b,_4c){
if(_4c){
this.rows.styleRowNode(_4b,_4c);
}
},_mouseOut:function(e){
this.rows.setOverRow(-2);
},getCell:function(_4d){
return this.layout.cells[_4d];
},setCellWidth:function(_4e,_4f){
this.getCell(_4e).unitWidth=_4f;
},getCellName:function(_50){
return "Cell "+_50.index;
},canSort:function(_51){
},sort:function(){
},getSortAsc:function(_52){
_52=_52==undefined?this.sortInfo:_52;
return Boolean(_52>0);
},getSortIndex:function(_53){
_53=_53==undefined?this.sortInfo:_53;
return Math.abs(_53)-1;
},setSortIndex:function(_54,_55){
var si=_54+1;
if(_55!=undefined){
si*=(_55?1:-1);
}else{
if(this.getSortIndex()==_54){
si=-this.sortInfo;
}
}
this.setSortInfo(si);
},setSortInfo:function(_56){
if(this.canSort(_56)){
this.sortInfo=_56;
this.sort();
this.update();
}
},doKeyEvent:function(e){
e.dispatch="do"+e.type;
this.onKeyEvent(e);
},_dispatch:function(m,e){
if(m in this){
return this[m](e);
}
return false;
},dispatchKeyEvent:function(e){
this._dispatch(e.dispatch,e);
},dispatchContentEvent:function(e){
this.edit.dispatchEvent(e)||e.sourceView.dispatchContentEvent(e)||this._dispatch(e.dispatch,e);
},dispatchHeaderEvent:function(e){
e.sourceView.dispatchHeaderEvent(e)||this._dispatch("doheader"+e.type,e);
},dokeydown:function(e){
this.onKeyDown(e);
},doclick:function(e){
if(e.cellNode){
this.onCellClick(e);
}else{
this.onRowClick(e);
}
},dodblclick:function(e){
if(e.cellNode){
this.onCellDblClick(e);
}else{
this.onRowDblClick(e);
}
},docontextmenu:function(e){
if(e.cellNode){
this.onCellContextMenu(e);
}else{
this.onRowContextMenu(e);
}
},doheaderclick:function(e){
if(e.cellNode){
this.onHeaderCellClick(e);
}else{
this.onHeaderClick(e);
}
},doheaderdblclick:function(e){
if(e.cellNode){
this.onHeaderCellDblClick(e);
}else{
this.onHeaderDblClick(e);
}
},doheadercontextmenu:function(e){
if(e.cellNode){
this.onHeaderCellContextMenu(e);
}else{
this.onHeaderContextMenu(e);
}
},doStartEdit:function(_57,_58){
this.onStartEdit(_57,_58);
},doApplyCellEdit:function(_59,_5a,_5b){
this.onApplyCellEdit(_59,_5a,_5b);
},doCancelEdit:function(_5c){
this.onCancelEdit(_5c);
},doApplyEdit:function(_5d){
this.onApplyEdit(_5d);
},addRow:function(){
this.updateRowCount(this.get("rowCount")+1);
},removeSelectedRows:function(){
if(this.allItemsSelected){
this.updateRowCount(0);
}else{
this.updateRowCount(Math.max(0,this.get("rowCount")-this.selection.getSelected().length));
}
this.selection.clear();
}});
_19.markupFactory=function(_5e,_5f,_60,_61){
var _62=function(n){
var w=_17.attr(n,"width")||"auto";
if((w!="auto")&&(w.slice(-2)!="em")&&(w.slice(-1)!="%")){
w=parseInt(w,10)+"px";
}
return w;
};
if(!_5e.structure&&_5f.nodeName.toLowerCase()=="table"){
_5e.structure=_18("> colgroup",_5f).map(function(cg){
var sv=_17.attr(cg,"span");
var v={noscroll:(_17.attr(cg,"noscroll")=="true")?true:false,__span:(!!sv?parseInt(sv,10):1),cells:[]};
if(_17.hasAttr(cg,"width")){
v.width=_62(cg);
}
return v;
});
if(!_5e.structure.length){
_5e.structure.push({__span:Infinity,cells:[]});
}
_18("thead > tr",_5f).forEach(function(tr,_63){
var _64=0;
var _65=0;
var _66;
var _67=null;
_18("> th",tr).map(function(th){
if(!_67){
_66=0;
_67=_5e.structure[0];
}else{
if(_64>=(_66+_67.__span)){
_65++;
_66+=_67.__span;
var _68=_67;
_67=_5e.structure[_65];
}
}
var _69={name:_15.trim(_17.attr(th,"name")||th.innerHTML),colSpan:parseInt(_17.attr(th,"colspan")||1,10),type:_15.trim(_17.attr(th,"cellType")||""),id:_15.trim(_17.attr(th,"id")||"")};
_64+=_69.colSpan;
var _6a=_17.attr(th,"rowspan");
if(_6a){
_69.rowSpan=_6a;
}
if(_17.hasAttr(th,"width")){
_69.width=_62(th);
}
if(_17.hasAttr(th,"relWidth")){
_69.relWidth=window.parseInt(_17.attr(th,"relWidth"),10);
}
if(_17.hasAttr(th,"hidden")){
_69.hidden=(_17.attr(th,"hidden")=="true"||_17.attr(th,"hidden")===true);
}
if(_61){
_61(th,_69);
}
_69.type=_69.type?_15.getObject(_69.type):_2.grid.cells.Cell;
if(_69.type&&_69.type.markupFactory){
_69.type.markupFactory(th,_69);
}
if(!_67.cells[_63]){
_67.cells[_63]=[];
}
_67.cells[_63].push(_69);
});
});
}
return new _60(_5e,_5f);
};
return _19;
});
