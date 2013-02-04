//>>built
require({cache:{"url:dojox/grid/resources/View.html":"<div class=\"dojoxGridView\" role=\"presentation\">\n\t<div class=\"dojoxGridHeader\" dojoAttachPoint=\"headerNode\" role=\"presentation\">\n\t\t<div dojoAttachPoint=\"headerNodeContainer\" style=\"width:9000em\" role=\"presentation\">\n\t\t\t<div dojoAttachPoint=\"headerContentNode\" role=\"row\"></div>\n\t\t</div>\n\t</div>\n\t<input type=\"checkbox\" class=\"dojoxGridHiddenFocus\" dojoAttachPoint=\"hiddenFocusNode\" role=\"presentation\" />\n\t<input type=\"checkbox\" class=\"dojoxGridHiddenFocus\" role=\"presentation\" />\n\t<div class=\"dojoxGridScrollbox\" dojoAttachPoint=\"scrollboxNode\" role=\"presentation\">\n\t\t<div class=\"dojoxGridContent\" dojoAttachPoint=\"contentNode\" hidefocus=\"hidefocus\" role=\"presentation\"></div>\n\t</div>\n</div>\n"}});
define("dojox/grid/_View",["dojo","dijit/registry","../main","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/connect","dojo/_base/sniff","dojo/query","dojo/_base/window","dojo/text!./resources/View.html","dojo/dnd/Source","dijit/_Widget","dijit/_TemplatedMixin","dojox/html/metrics","./util","dojo/_base/html","./_Builder","dojo/dnd/Avatar","dojo/dnd/Manager"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13){
var _14=function(_15,_16){
return _15.style.cssText==undefined?_15.getAttribute("style"):_15.style.cssText;
};
var _17=_4("dojox.grid._View",[_d,_e],{defaultWidth:"18em",viewWidth:"",templateString:_b,themeable:false,classTag:"dojoxGrid",marginBottom:0,rowPad:2,_togglingColumn:-1,_headerBuilderClass:_12._HeaderBuilder,_contentBuilderClass:_12._ContentBuilder,postMixInProperties:function(){
this.rowNodes={};
},postCreate:function(){
this.connect(this.scrollboxNode,"onscroll","doscroll");
_10.funnelEvents(this.contentNode,this,"doContentEvent",["mouseover","mouseout","click","dblclick","contextmenu","mousedown"]);
_10.funnelEvents(this.headerNode,this,"doHeaderEvent",["dblclick","mouseover","mouseout","mousemove","mousedown","click","contextmenu"]);
this.content=new this._contentBuilderClass(this);
this.header=new this._headerBuilderClass(this);
if(!this.grid.isLeftToRight()){
this.headerNodeContainer.style.width="";
}
},destroy:function(){
_11.destroy(this.headerNode);
delete this.headerNode;
for(var i in this.rowNodes){
this._cleanupRowWidgets(this.rowNodes[i]);
_11.destroy(this.rowNodes[i]);
}
this.rowNodes={};
if(this.source){
this.source.destroy();
}
this.inherited(arguments);
},focus:function(){
if(_8("ie")||_8("webkit")||_8("opera")){
this.hiddenFocusNode.focus();
}else{
this.scrollboxNode.focus();
}
},setStructure:function(_18){
var vs=(this.structure=_18);
if(vs.width&&!isNaN(vs.width)){
this.viewWidth=vs.width+"em";
}else{
this.viewWidth=vs.width||(vs.noscroll?"auto":this.viewWidth);
}
this._onBeforeRow=vs.onBeforeRow||function(){
};
this._onAfterRow=vs.onAfterRow||function(){
};
this.noscroll=vs.noscroll;
if(this.noscroll){
this.scrollboxNode.style.overflow="hidden";
}
this.simpleStructure=Boolean(vs.cells.length==1);
this.testFlexCells();
this.updateStructure();
},_cleanupRowWidgets:function(_19){
if(_19){
_5.forEach(_9("[widgetId]",_19).map(_2.byNode),function(w){
if(w._destroyOnRemove){
w.destroy();
delete w;
}else{
if(w.domNode&&w.domNode.parentNode){
w.domNode.parentNode.removeChild(w.domNode);
}
}
});
}
},onBeforeRow:function(_1a,_1b){
this._onBeforeRow(_1a,_1b);
if(_1a>=0){
this._cleanupRowWidgets(this.getRowNode(_1a));
}
},onAfterRow:function(_1c,_1d,_1e){
this._onAfterRow(_1c,_1d,_1e);
var g=this.grid;
_5.forEach(_9(".dojoxGridStubNode",_1e),function(n){
if(n&&n.parentNode){
var lw=n.getAttribute("linkWidget");
var _1f=window.parseInt(_11.attr(n,"cellIdx"),10);
var _20=g.getCell(_1f);
var w=_2.byId(lw);
if(w){
n.parentNode.replaceChild(w.domNode,n);
if(!w._started){
w.startup();
}
_1.destroy(n);
}else{
n.innerHTML="";
}
}
},this);
},testFlexCells:function(){
this.flexCells=false;
for(var j=0,row;(row=this.structure.cells[j]);j++){
for(var i=0,_21;(_21=row[i]);i++){
_21.view=this;
this.flexCells=this.flexCells||_21.isFlex();
}
}
return this.flexCells;
},updateStructure:function(){
this.header.update();
this.content.update();
},getScrollbarWidth:function(){
var _22=this.hasVScrollbar();
var _23=_11.style(this.scrollboxNode,"overflow");
if(this.noscroll||!_23||_23=="hidden"){
_22=false;
}else{
if(_23=="scroll"){
_22=true;
}
}
return (_22?_f.getScrollbar().w:0);
},getColumnsWidth:function(){
var h=this.headerContentNode;
return h&&h.firstChild?h.firstChild.offsetWidth:0;
},setColumnsWidth:function(_24){
this.headerContentNode.firstChild.style.width=_24+"px";
if(this.viewWidth){
this.viewWidth=_24+"px";
}
},getWidth:function(){
return this.viewWidth||(this.getColumnsWidth()+this.getScrollbarWidth())+"px";
},getContentWidth:function(){
return Math.max(0,_11._getContentBox(this.domNode).w-this.getScrollbarWidth())+"px";
},render:function(){
this.scrollboxNode.style.height="";
this.renderHeader();
if(this._togglingColumn>=0){
this.setColumnsWidth(this.getColumnsWidth()-this._togglingColumn);
this._togglingColumn=-1;
}
var _25=this.grid.layout.cells;
var _26=_6.hitch(this,function(_27,_28){
!this.grid.isLeftToRight()&&(_28=!_28);
var inc=_28?-1:1;
var idx=this.header.getCellNodeIndex(_27)+inc;
var _29=_25[idx];
while(_29&&_29.getHeaderNode()&&_29.getHeaderNode().style.display=="none"){
idx+=inc;
_29=_25[idx];
}
if(_29){
return _29.getHeaderNode();
}
return null;
});
if(this.grid.columnReordering&&this.simpleStructure){
if(this.source){
this.source.destroy();
}
var _2a="dojoxGrid_bottomMarker";
var _2b="dojoxGrid_topMarker";
if(this.bottomMarker){
_11.destroy(this.bottomMarker);
}
this.bottomMarker=_11.byId(_2a);
if(this.topMarker){
_11.destroy(this.topMarker);
}
this.topMarker=_11.byId(_2b);
if(!this.bottomMarker){
this.bottomMarker=_11.create("div",{"id":_2a,"class":"dojoxGridColPlaceBottom"},_a.body());
this._hide(this.bottomMarker);
this.topMarker=_11.create("div",{"id":_2b,"class":"dojoxGridColPlaceTop"},_a.body());
this._hide(this.topMarker);
}
this.arrowDim=_11.contentBox(this.bottomMarker);
var _2c=_11.contentBox(this.headerContentNode.firstChild.rows[0]).h;
this.source=new _c(this.headerContentNode.firstChild.rows[0],{horizontal:true,accept:["gridColumn_"+this.grid.id],viewIndex:this.index,generateText:false,onMouseDown:_6.hitch(this,function(e){
this.header.decorateEvent(e);
if((this.header.overRightResizeArea(e)||this.header.overLeftResizeArea(e))&&this.header.canResize(e)&&!this.header.moveable){
this.header.beginColumnResize(e);
}else{
if(this.grid.headerMenu){
this.grid.headerMenu.onCancel(true);
}
if(e.button===(_8("ie")?1:0)){
_c.prototype.onMouseDown.call(this.source,e);
}
}
}),onMouseOver:_6.hitch(this,function(e){
var src=this.source;
if(src._getChildByEvent(e)){
_c.prototype.onMouseOver.apply(src,arguments);
}
}),_markTargetAnchor:_6.hitch(this,function(_2d){
var src=this.source;
if(src.current==src.targetAnchor&&src.before==_2d){
return;
}
if(src.targetAnchor&&_26(src.targetAnchor,src.before)){
src._removeItemClass(_26(src.targetAnchor,src.before),src.before?"After":"Before");
}
_c.prototype._markTargetAnchor.call(src,_2d);
var _2e=_2d?src.targetAnchor:_26(src.targetAnchor,src.before);
var _2f=0;
if(!_2e){
_2e=src.targetAnchor;
_2f=_11.contentBox(_2e).w+this.arrowDim.w/2+2;
}
var pos=_11.position(_2e,true);
var _30=Math.floor(pos.x-this.arrowDim.w/2+_2f);
_11.style(this.bottomMarker,"visibility","visible");
_11.style(this.topMarker,"visibility","visible");
_11.style(this.bottomMarker,{"left":_30+"px","top":(_2c+pos.y)+"px"});
_11.style(this.topMarker,{"left":_30+"px","top":(pos.y-this.arrowDim.h)+"px"});
if(src.targetAnchor&&_26(src.targetAnchor,src.before)){
src._addItemClass(_26(src.targetAnchor,src.before),src.before?"After":"Before");
}
}),_unmarkTargetAnchor:_6.hitch(this,function(){
var src=this.source;
if(!src.targetAnchor){
return;
}
if(src.targetAnchor&&_26(src.targetAnchor,src.before)){
src._removeItemClass(_26(src.targetAnchor,src.before),src.before?"After":"Before");
}
this._hide(this.bottomMarker);
this._hide(this.topMarker);
_c.prototype._unmarkTargetAnchor.call(src);
}),destroy:_6.hitch(this,function(){
_7.disconnect(this._source_conn);
_7.unsubscribe(this._source_sub);
_c.prototype.destroy.call(this.source);
if(this.bottomMarker){
_11.destroy(this.bottomMarker);
delete this.bottomMarker;
}
if(this.topMarker){
_11.destroy(this.topMarker);
delete this.topMarker;
}
}),onDndCancel:_6.hitch(this,function(){
_c.prototype.onDndCancel.call(this.source);
this._hide(this.bottomMarker);
this._hide(this.topMarker);
})});
this._source_conn=_7.connect(this.source,"onDndDrop",this,"_onDndDrop");
this._source_sub=_7.subscribe("/dnd/drop/before",this,"_onDndDropBefore");
this.source.startup();
}
},_hide:function(_31){
_11.style(_31,{top:"-10000px","visibility":"hidden"});
},_onDndDropBefore:function(_32,_33,_34){
if(_1.dnd.manager().target!==this.source){
return;
}
this.source._targetNode=this.source.targetAnchor;
this.source._beforeTarget=this.source.before;
var _35=this.grid.views.views;
var _36=_35[_32.viewIndex];
var _37=_35[this.index];
if(_37!=_36){
_36.convertColPctToFixed();
_37.convertColPctToFixed();
}
},_onDndDrop:function(_38,_39,_3a){
if(_1.dnd.manager().target!==this.source){
if(_1.dnd.manager().source===this.source){
this._removingColumn=true;
}
return;
}
this._hide(this.bottomMarker);
this._hide(this.topMarker);
var _3b=function(n){
return n?_11.attr(n,"idx"):null;
};
var w=_11.marginBox(_39[0]).w;
if(_38.viewIndex!==this.index){
var _3c=this.grid.views.views;
var _3d=_3c[_38.viewIndex];
var _3e=_3c[this.index];
if(_3d.viewWidth&&_3d.viewWidth!="auto"){
_3d.setColumnsWidth(_3d.getColumnsWidth()-w);
}
if(_3e.viewWidth&&_3e.viewWidth!="auto"){
_3e.setColumnsWidth(_3e.getColumnsWidth());
}
}
var stn=this.source._targetNode;
var stb=this.source._beforeTarget;
!this.grid.isLeftToRight()&&(stb=!stb);
var _3f=this.grid.layout;
var idx=this.index;
delete this.source._targetNode;
delete this.source._beforeTarget;
_3f.moveColumn(_38.viewIndex,idx,_3b(_39[0]),_3b(stn),stb);
},renderHeader:function(){
this.headerContentNode.innerHTML=this.header.generateHtml(this._getHeaderContent);
if(this.flexCells){
this.contentWidth=this.getContentWidth();
this.headerContentNode.firstChild.style.width=this.contentWidth;
}
_10.fire(this,"onAfterRow",[-1,this.structure.cells,this.headerContentNode]);
},_getHeaderContent:function(_40){
var n=_40.name||_40.grid.getCellName(_40);
if(/^\s+$/.test(n)){
n="&nbsp;";
}
var ret=["<div class=\"dojoxGridSortNode"];
if(_40.index!=_40.grid.getSortIndex()){
ret.push("\">");
}else{
ret=ret.concat([" ",_40.grid.sortInfo>0?"dojoxGridSortUp":"dojoxGridSortDown","\"><div class=\"dojoxGridArrowButtonChar\">",_40.grid.sortInfo>0?"&#9650;":"&#9660;","</div><div class=\"dojoxGridArrowButtonNode\" role=\"presentation\"></div>","<div class=\"dojoxGridColCaption\">"]);
}
ret=ret.concat([n,"</div></div>"]);
return ret.join("");
},resize:function(){
this.adaptHeight();
this.adaptWidth();
},hasHScrollbar:function(_41){
var _42=this._hasHScroll||false;
if(this._hasHScroll==undefined||_41){
if(this.noscroll){
this._hasHScroll=false;
}else{
var _43=_11.style(this.scrollboxNode,"overflow");
if(_43=="hidden"){
this._hasHScroll=false;
}else{
if(_43=="scroll"){
this._hasHScroll=true;
}else{
this._hasHScroll=(this.scrollboxNode.offsetWidth-this.getScrollbarWidth()<this.contentNode.offsetWidth);
}
}
}
}
if(_42!==this._hasHScroll){
this.grid.update();
}
return this._hasHScroll;
},hasVScrollbar:function(_44){
var _45=this._hasVScroll||false;
if(this._hasVScroll==undefined||_44){
if(this.noscroll){
this._hasVScroll=false;
}else{
var _46=_11.style(this.scrollboxNode,"overflow");
if(_46=="hidden"){
this._hasVScroll=false;
}else{
if(_46=="scroll"){
this._hasVScroll=true;
}else{
this._hasVScroll=(this.scrollboxNode.scrollHeight>this.scrollboxNode.clientHeight);
}
}
}
}
if(_45!==this._hasVScroll){
this.grid.update();
}
return this._hasVScroll;
},convertColPctToFixed:function(){
var _47=false;
this.grid.initialWidth="";
var _48=_9("th",this.headerContentNode);
var _49=_5.map(_48,function(c,_4a){
var w=c.style.width;
_11.attr(c,"vIdx",_4a);
if(w&&w.slice(-1)=="%"){
_47=true;
}else{
if(w&&w.slice(-2)=="px"){
return window.parseInt(w,10);
}
}
return _11.contentBox(c).w;
});
if(_47){
_5.forEach(this.grid.layout.cells,function(_4b,idx){
if(_4b.view==this){
var _4c=_4b.view.getHeaderCellNode(_4b.index);
if(_4c&&_11.hasAttr(_4c,"vIdx")){
var _4d=window.parseInt(_11.attr(_4c,"vIdx"));
this.setColWidth(idx,_49[_4d]);
_11.removeAttr(_4c,"vIdx");
}
}
},this);
return true;
}
return false;
},adaptHeight:function(_4e){
if(!this.grid._autoHeight){
var h=(this.domNode.style.height&&parseInt(this.domNode.style.height.replace(/px/,""),10))||this.domNode.clientHeight;
var _4f=this;
var _50=function(){
var v;
for(var i in _4f.grid.views.views){
v=_4f.grid.views.views[i];
if(v!==_4f&&v.hasHScrollbar()){
return true;
}
}
return false;
};
if(_4e||(this.noscroll&&_50())){
h-=_f.getScrollbar().h;
}
_10.setStyleHeightPx(this.scrollboxNode,h);
}
this.hasVScrollbar(true);
},adaptWidth:function(){
if(this.flexCells){
this.contentWidth=this.getContentWidth();
this.headerContentNode.firstChild.style.width=this.contentWidth;
}
var w=this.scrollboxNode.offsetWidth-this.getScrollbarWidth();
if(!this._removingColumn){
w=Math.max(w,this.getColumnsWidth())+"px";
}else{
w=Math.min(w,this.getColumnsWidth())+"px";
this._removingColumn=false;
}
var cn=this.contentNode;
cn.style.width=w;
this.hasHScrollbar(true);
},setSize:function(w,h){
var ds=this.domNode.style;
var hs=this.headerNode.style;
if(w){
ds.width=w;
hs.width=w;
}
ds.height=(h>=0?h+"px":"");
},renderRow:function(_51){
var _52=this.createRowNode(_51);
this.buildRow(_51,_52);
return _52;
},createRowNode:function(_53){
var _54=document.createElement("div");
_54.className=this.classTag+"Row";
if(this instanceof _3.grid._RowSelector){
_11.attr(_54,"role","presentation");
}else{
_11.attr(_54,"role","row");
if(this.grid.selectionMode!="none"){
_54.setAttribute("aria-selected","false");
}
}
_54[_10.gridViewTag]=this.id;
_54[_10.rowIndexTag]=_53;
this.rowNodes[_53]=_54;
return _54;
},buildRow:function(_55,_56){
this.buildRowContent(_55,_56);
this.styleRow(_55,_56);
},buildRowContent:function(_57,_58){
_58.innerHTML=this.content.generateHtml(_57,_57);
if(this.flexCells&&this.contentWidth){
_58.firstChild.style.width=this.contentWidth;
}
_10.fire(this,"onAfterRow",[_57,this.structure.cells,_58]);
},rowRemoved:function(_59){
if(_59>=0){
this._cleanupRowWidgets(this.getRowNode(_59));
}
this.grid.edit.save(this,_59);
delete this.rowNodes[_59];
},getRowNode:function(_5a){
return this.rowNodes[_5a];
},getCellNode:function(_5b,_5c){
var row=this.getRowNode(_5b);
if(row){
return this.content.getCellNode(row,_5c);
}
},getHeaderCellNode:function(_5d){
if(this.headerContentNode){
return this.header.getCellNode(this.headerContentNode,_5d);
}
},styleRow:function(_5e,_5f){
_5f._style=_14(_5f);
this.styleRowNode(_5e,_5f);
},styleRowNode:function(_60,_61){
if(_61){
this.doStyleRowNode(_60,_61);
}
},doStyleRowNode:function(_62,_63){
this.grid.styleRowNode(_62,_63);
},updateRow:function(_64){
var _65=this.getRowNode(_64);
if(_65){
_65.style.height="";
this.buildRow(_64,_65);
}
return _65;
},updateRowStyles:function(_66){
this.styleRowNode(_66,this.getRowNode(_66));
},lastTop:0,firstScroll:0,doscroll:function(_67){
var _68=this.grid.isLeftToRight();
if(this.firstScroll<2){
if((!_68&&this.firstScroll==1)||(_68&&this.firstScroll===0)){
var s=_11.marginBox(this.headerNodeContainer);
if(_8("ie")){
this.headerNodeContainer.style.width=s.w+this.getScrollbarWidth()+"px";
}else{
if(_8("mozilla")){
this.headerNodeContainer.style.width=s.w-this.getScrollbarWidth()+"px";
this.scrollboxNode.scrollLeft=_68?this.scrollboxNode.clientWidth-this.scrollboxNode.scrollWidth:this.scrollboxNode.scrollWidth-this.scrollboxNode.clientWidth;
}
}
}
this.firstScroll++;
}
this.headerNode.scrollLeft=this.scrollboxNode.scrollLeft;
var top=this.scrollboxNode.scrollTop;
if(top!==this.lastTop){
this.grid.scrollTo(top);
}
},setScrollTop:function(_69){
this.lastTop=_69;
this.scrollboxNode.scrollTop=_69;
return this.scrollboxNode.scrollTop;
},doContentEvent:function(e){
if(this.content.decorateEvent(e)){
this.grid.onContentEvent(e);
}
},doHeaderEvent:function(e){
if(this.header.decorateEvent(e)){
this.grid.onHeaderEvent(e);
}
},dispatchContentEvent:function(e){
return this.content.dispatchEvent(e);
},dispatchHeaderEvent:function(e){
return this.header.dispatchEvent(e);
},setColWidth:function(_6a,_6b){
this.grid.setCellWidth(_6a,_6b+"px");
},update:function(){
if(!this.domNode){
return;
}
this.content.update();
this.grid.update();
var _6c=this.scrollboxNode.scrollLeft;
this.scrollboxNode.scrollLeft=_6c;
this.headerNode.scrollLeft=_6c;
}});
var _6d=_4("dojox.grid._GridAvatar",_13,{construct:function(){
var dd=_a.doc;
var a=dd.createElement("table");
a.cellPadding=a.cellSpacing="0";
a.className="dojoxGridDndAvatar";
a.style.position="absolute";
a.style.zIndex=1999;
a.style.margin="0px";
var b=dd.createElement("tbody");
var tr=dd.createElement("tr");
var td=dd.createElement("td");
var img=dd.createElement("td");
tr.className="dojoxGridDndAvatarItem";
img.className="dojoxGridDndAvatarItemImage";
img.style.width="16px";
var _6e=this.manager.source,_6f;
if(_6e.creator){
_6f=_6e._normalizedCreator(_6e.getItem(this.manager.nodes[0].id).data,"avatar").node;
}else{
_6f=this.manager.nodes[0].cloneNode(true);
var _70,_71;
if(_6f.tagName.toLowerCase()=="tr"){
_70=dd.createElement("table");
_71=dd.createElement("tbody");
_71.appendChild(_6f);
_70.appendChild(_71);
_6f=_70;
}else{
if(_6f.tagName.toLowerCase()=="th"){
_70=dd.createElement("table");
_71=dd.createElement("tbody");
var r=dd.createElement("tr");
_70.cellPadding=_70.cellSpacing="0";
r.appendChild(_6f);
_71.appendChild(r);
_70.appendChild(_71);
_6f=_70;
}
}
}
_6f.id="";
td.appendChild(_6f);
tr.appendChild(img);
tr.appendChild(td);
_11.style(tr,"opacity",0.9);
b.appendChild(tr);
a.appendChild(b);
this.node=a;
var m=_1.dnd.manager();
this.oldOffsetY=m.OFFSET_Y;
m.OFFSET_Y=1;
},destroy:function(){
_1.dnd.manager().OFFSET_Y=this.oldOffsetY;
this.inherited(arguments);
}});
var _72=_1.dnd.manager().makeAvatar;
_1.dnd.manager().makeAvatar=function(){
var src=this.source;
if(src.viewIndex!==undefined&&!_11.hasClass(_a.body(),"dijit_a11y")){
return new _6d(this);
}
return _72.call(_1.dnd.manager());
};
return _17;
});
