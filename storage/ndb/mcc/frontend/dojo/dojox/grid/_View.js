//>>built
require({cache:{"url:dojox/grid/resources/View.html":"<div class=\"dojoxGridView\" role=\"presentation\">\n\t<div class=\"dojoxGridHeader\" dojoAttachPoint=\"headerNode\" role=\"presentation\">\n\t\t<div dojoAttachPoint=\"headerNodeContainer\" style=\"width:9000em\" role=\"presentation\">\n\t\t\t<div dojoAttachPoint=\"headerContentNode\" role=\"row\"></div>\n\t\t</div>\n\t</div>\n\t<input type=\"checkbox\" class=\"dojoxGridHiddenFocus\" dojoAttachPoint=\"hiddenFocusNode\" role=\"presentation\" />\n\t<input type=\"checkbox\" class=\"dojoxGridHiddenFocus\" role=\"presentation\" />\n\t<div class=\"dojoxGridScrollbox\" dojoAttachPoint=\"scrollboxNode\" role=\"presentation\">\n\t\t<div class=\"dojoxGridContent\" dojoAttachPoint=\"contentNode\" hidefocus=\"hidefocus\" role=\"presentation\"></div>\n\t</div>\n</div>\n"}});
define("dojox/grid/_View",["dojo","dijit/registry","../main","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/connect","dojo/_base/sniff","dojo/query","dojo/_base/window","dojo/text!./resources/View.html","dojo/dnd/Source","dijit/_Widget","dijit/_TemplatedMixin","dojox/html/metrics","./util","dojo/_base/html","./_Builder","dojo/dnd/Avatar","dojo/dnd/Manager"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14){
var _15=function(_16,_17){
return _16.style.cssText==undefined?_16.getAttribute("style"):_16.style.cssText;
};
var _18=_4("dojox.grid._View",[_d,_e],{defaultWidth:"18em",viewWidth:"",templateString:_b,classTag:"dojoxGrid",marginBottom:0,rowPad:2,_togglingColumn:-1,_headerBuilderClass:_12._HeaderBuilder,_contentBuilderClass:_12._ContentBuilder,postMixInProperties:function(){
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
},setStructure:function(_19){
var vs=(this.structure=_19);
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
},_cleanupRowWidgets:function(_1a){
if(_1a){
_5.forEach(_9("[widgetId]",_1a).map(_2.byNode),function(w){
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
},onBeforeRow:function(_1b,_1c){
this._onBeforeRow(_1b,_1c);
if(_1b>=0){
this._cleanupRowWidgets(this.getRowNode(_1b));
}
},onAfterRow:function(_1d,_1e,_1f){
this._onAfterRow(_1d,_1e,_1f);
var g=this.grid;
_5.forEach(_9(".dojoxGridStubNode",_1f),function(n){
if(n&&n.parentNode){
var lw=n.getAttribute("linkWidget");
var _20=window.parseInt(_11.attr(n,"cellIdx"),10);
var _21=g.getCell(_20);
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
for(var i=0,_22;(_22=row[i]);i++){
_22.view=this;
this.flexCells=this.flexCells||_22.isFlex();
}
}
return this.flexCells;
},updateStructure:function(){
this.header.update();
this.content.update();
},getScrollbarWidth:function(){
var _23=this.hasVScrollbar();
var _24=_11.style(this.scrollboxNode,"overflow");
if(this.noscroll||!_24||_24=="hidden"){
_23=false;
}else{
if(_24=="scroll"){
_23=true;
}
}
return (_23?_f.getScrollbar().w:0);
},getColumnsWidth:function(){
var h=this.headerContentNode;
return h&&h.firstChild?(h.firstChild.offsetWidth||_11.style(h.firstChild,"width")):0;
},setColumnsWidth:function(_25){
this.headerContentNode.firstChild.style.width=_25+"px";
if(this.viewWidth){
this.viewWidth=_25+"px";
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
var _26=this.grid.layout.cells;
var _27=_6.hitch(this,function(_28,_29){
!this.grid.isLeftToRight()&&(_29=!_29);
var inc=_29?-1:1;
var idx=this.header.getCellNodeIndex(_28)+inc;
var _2a=_26[idx];
while(_2a&&_2a.getHeaderNode()&&_2a.getHeaderNode().style.display=="none"){
idx+=inc;
_2a=_26[idx];
}
if(_2a){
return _2a.getHeaderNode();
}
return null;
});
if(this.grid.columnReordering&&this.simpleStructure){
if(this.source){
this.source.destroy();
}
var _2b="dojoxGrid_bottomMarker";
var _2c="dojoxGrid_topMarker";
if(this.bottomMarker){
_11.destroy(this.bottomMarker);
}
this.bottomMarker=_11.byId(_2b);
if(this.topMarker){
_11.destroy(this.topMarker);
}
this.topMarker=_11.byId(_2c);
if(!this.bottomMarker){
this.bottomMarker=_11.create("div",{"id":_2b,"class":"dojoxGridColPlaceBottom"},_a.body());
this._hide(this.bottomMarker);
this.topMarker=_11.create("div",{"id":_2c,"class":"dojoxGridColPlaceTop"},_a.body());
this._hide(this.topMarker);
}
this.arrowDim=_11.contentBox(this.bottomMarker);
var _2d=_11.contentBox(this.headerContentNode.firstChild.rows[0]).h;
this.source=new _c(this.headerContentNode.firstChild.rows[0],{horizontal:true,accept:["gridColumn_"+this.grid.id],viewIndex:this.index,generateText:false,onMouseDown:_6.hitch(this,function(e){
this.header.decorateEvent(e);
if((this.header.overRightResizeArea(e)||this.header.overLeftResizeArea(e))&&this.header.canResize(e)&&!this.header.moveable){
this.header.beginColumnResize(e);
}else{
if(this.grid.headerMenu){
this.grid.headerMenu.onCancel(true);
}
if(e.button===(_8("ie")<9?1:0)){
_c.prototype.onMouseDown.call(this.source,e);
}
}
}),onMouseOver:_6.hitch(this,function(e){
var src=this.source;
if(src._getChildByEvent(e)){
_c.prototype.onMouseOver.apply(src,arguments);
}
}),_markTargetAnchor:_6.hitch(this,function(_2e){
var src=this.source;
if(src.current==src.targetAnchor&&src.before==_2e){
return;
}
if(src.targetAnchor&&_27(src.targetAnchor,src.before)){
src._removeItemClass(_27(src.targetAnchor,src.before),src.before?"After":"Before");
}
_c.prototype._markTargetAnchor.call(src,_2e);
var _2f=_2e?src.targetAnchor:_27(src.targetAnchor,src.before);
var _30=0;
if(!_2f){
_2f=src.targetAnchor;
_30=_11.contentBox(_2f).w+this.arrowDim.w/2+2;
}
var pos=_11.position(_2f,true);
var _31=Math.floor(pos.x-this.arrowDim.w/2+_30);
_11.style(this.bottomMarker,"visibility","visible");
_11.style(this.topMarker,"visibility","visible");
_11.style(this.bottomMarker,{"left":_31+"px","top":(_2d+pos.y)+"px"});
_11.style(this.topMarker,{"left":_31+"px","top":(pos.y-this.arrowDim.h)+"px"});
if(src.targetAnchor&&_27(src.targetAnchor,src.before)){
src._addItemClass(_27(src.targetAnchor,src.before),src.before?"After":"Before");
}
}),_unmarkTargetAnchor:_6.hitch(this,function(){
var src=this.source;
if(!src.targetAnchor){
return;
}
if(src.targetAnchor&&_27(src.targetAnchor,src.before)){
src._removeItemClass(_27(src.targetAnchor,src.before),src.before?"After":"Before");
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
},_hide:function(_32){
_11.style(_32,{top:"-10000px","visibility":"hidden"});
},_onDndDropBefore:function(_33,_34,_35){
if(_14.manager().target!==this.source){
return;
}
this.source._targetNode=this.source.targetAnchor;
this.source._beforeTarget=this.source.before;
var _36=this.grid.views.views;
var _37=_36[_33.viewIndex];
var _38=_36[this.index];
if(_38!=_37){
_37.convertColPctToFixed();
_38.convertColPctToFixed();
}
},_onDndDrop:function(_39,_3a,_3b){
if(_14.manager().target!==this.source){
if(_14.manager().source===this.source){
this._removingColumn=true;
}
return;
}
this._hide(this.bottomMarker);
this._hide(this.topMarker);
var _3c=function(n){
return n?_11.attr(n,"idx"):null;
};
var w=_11.marginBox(_3a[0]).w;
if(_39.viewIndex!==this.index){
var _3d=this.grid.views.views;
var _3e=_3d[_39.viewIndex];
var _3f=_3d[this.index];
if(_3e.viewWidth&&_3e.viewWidth!="auto"){
_3e.setColumnsWidth(_3e.getColumnsWidth()-w);
}
if(_3f.viewWidth&&_3f.viewWidth!="auto"){
_3f.setColumnsWidth(_3f.getColumnsWidth());
}
}
var stn=this.source._targetNode;
var stb=this.source._beforeTarget;
!this.grid.isLeftToRight()&&(stb=!stb);
var _40=this.grid.layout;
var idx=this.index;
delete this.source._targetNode;
delete this.source._beforeTarget;
_40.moveColumn(_39.viewIndex,idx,_3c(_3a[0]),_3c(stn),stb);
},renderHeader:function(){
this.headerContentNode.innerHTML=this.header.generateHtml(this._getHeaderContent);
if(this.flexCells){
this.contentWidth=this.getContentWidth();
this.headerContentNode.firstChild.style.width=this.contentWidth;
}
_10.fire(this,"onAfterRow",[-1,this.structure.cells,this.headerContentNode]);
},_getHeaderContent:function(_41){
var n=_41.name||_41.grid.getCellName(_41);
if(/^\s+$/.test(n)){
n="&nbsp;";
}
var ret=["<div class=\"dojoxGridSortNode"];
if(_41.index!=_41.grid.getSortIndex()){
ret.push("\">");
}else{
ret=ret.concat([" ",_41.grid.sortInfo>0?"dojoxGridSortUp":"dojoxGridSortDown","\"><div class=\"dojoxGridArrowButtonChar\">",_41.grid.sortInfo>0?"&#9650;":"&#9660;","</div><div class=\"dojoxGridArrowButtonNode\" role=\"presentation\"></div>","<div class=\"dojoxGridColCaption\">"]);
}
ret=ret.concat([n,"</div></div>"]);
return ret.join("");
},resize:function(){
this.adaptHeight();
this.adaptWidth();
},hasHScrollbar:function(_42){
var _43=this._hasHScroll||false;
if(this._hasHScroll==undefined||_42){
if(this.noscroll){
this._hasHScroll=false;
}else{
var _44=_11.style(this.scrollboxNode,"overflow");
if(_44=="hidden"){
this._hasHScroll=false;
}else{
if(_44=="scroll"){
this._hasHScroll=true;
}else{
this._hasHScroll=(this.scrollboxNode.offsetWidth-this.getScrollbarWidth()<this.contentNode.offsetWidth);
}
}
}
}
if(_43!==this._hasHScroll){
this.grid.update();
}
return this._hasHScroll;
},hasVScrollbar:function(_45){
var _46=this._hasVScroll||false;
if(this._hasVScroll==undefined||_45){
if(this.noscroll){
this._hasVScroll=false;
}else{
var _47=_11.style(this.scrollboxNode,"overflow");
if(_47=="hidden"){
this._hasVScroll=false;
}else{
if(_47=="scroll"){
this._hasVScroll=true;
}else{
this._hasVScroll=(this.scrollboxNode.scrollHeight>this.scrollboxNode.clientHeight);
}
}
}
}
if(_46!==this._hasVScroll){
this.grid.update();
}
return this._hasVScroll;
},convertColPctToFixed:function(){
var _48=false;
this.grid.initialWidth="";
var _49=_9("th",this.headerContentNode);
var _4a=_5.map(_49,function(c,_4b){
var w=c.style.width;
_11.attr(c,"vIdx",_4b);
if(w&&w.slice(-1)=="%"){
_48=true;
}else{
if(w&&w.slice(-2)=="px"){
return window.parseInt(w,10);
}
}
return _11.contentBox(c).w;
});
if(_48){
_5.forEach(this.grid.layout.cells,function(_4c,idx){
if(_4c.view==this){
var _4d=_4c.view.getHeaderCellNode(_4c.index);
if(_4d&&_11.hasAttr(_4d,"vIdx")){
var _4e=window.parseInt(_11.attr(_4d,"vIdx"));
this.setColWidth(idx,_4a[_4e]);
_11.removeAttr(_4d,"vIdx");
}
}
},this);
return true;
}
return false;
},adaptHeight:function(_4f){
if(!this.grid._autoHeight){
var h=(this.domNode.style.height&&parseInt(this.domNode.style.height.replace(/px/,""),10))||this.domNode.clientHeight;
var _50=this;
var _51=function(){
var v;
for(var i in _50.grid.views.views){
v=_50.grid.views.views[i];
if(v!==_50&&v.hasHScrollbar()){
return true;
}
}
return false;
};
if(_4f||(this.noscroll&&_51())){
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
},renderRow:function(_52){
var _53=this.createRowNode(_52);
this.buildRow(_52,_53);
return _53;
},createRowNode:function(_54){
var _55=document.createElement("div");
_55.className=this.classTag+"Row";
if(this instanceof _3.grid._RowSelector){
_11.attr(_55,"role","presentation");
}else{
_11.attr(_55,"role","row");
if(this.grid.selectionMode!="none"){
_55.setAttribute("aria-selected","false");
}
}
_55[_10.gridViewTag]=this.id;
_55[_10.rowIndexTag]=_54;
this.rowNodes[_54]=_55;
return _55;
},buildRow:function(_56,_57){
this.buildRowContent(_56,_57);
this.styleRow(_56,_57);
},buildRowContent:function(_58,_59){
_59.innerHTML=this.content.generateHtml(_58,_58);
if(this.flexCells&&this.contentWidth){
_59.firstChild.style.width=this.contentWidth;
}
_10.fire(this,"onAfterRow",[_58,this.structure.cells,_59]);
},rowRemoved:function(_5a){
if(_5a>=0){
this._cleanupRowWidgets(this.getRowNode(_5a));
}
this.grid.edit.save(this,_5a);
delete this.rowNodes[_5a];
},getRowNode:function(_5b){
return this.rowNodes[_5b];
},getCellNode:function(_5c,_5d){
var row=this.getRowNode(_5c);
if(row){
return this.content.getCellNode(row,_5d);
}
},getHeaderCellNode:function(_5e){
if(this.headerContentNode){
return this.header.getCellNode(this.headerContentNode,_5e);
}
},styleRow:function(_5f,_60){
_60._style=_15(_60);
this.styleRowNode(_5f,_60);
},styleRowNode:function(_61,_62){
if(_62){
this.doStyleRowNode(_61,_62);
}
},doStyleRowNode:function(_63,_64){
this.grid.styleRowNode(_63,_64);
},updateRow:function(_65){
var _66=this.getRowNode(_65);
if(_66){
_66.style.height="";
this.buildRow(_65,_66);
}
return _66;
},updateRowStyles:function(_67){
this.styleRowNode(_67,this.getRowNode(_67));
},lastTop:0,firstScroll:0,_nativeScroll:false,doscroll:function(_68){
if(_8("ff")>=13||_8("chrome")){
this._nativeScroll=true;
}
var _69=this.grid.isLeftToRight();
if(this.firstScroll<2){
if((!_69&&this.firstScroll==1)||(_69&&this.firstScroll===0)){
var s=_11.marginBox(this.headerNodeContainer);
if(_8("ie")){
this.headerNodeContainer.style.width=s.w+this.getScrollbarWidth()+"px";
}else{
if(_8("mozilla")){
this.headerNodeContainer.style.width=s.w-this.getScrollbarWidth()+"px";
this.scrollboxNode.scrollLeft=_69?this.scrollboxNode.clientWidth-this.scrollboxNode.scrollWidth:this.scrollboxNode.scrollWidth-this.scrollboxNode.clientWidth;
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
this._nativeScroll=false;
},setScrollTop:function(_6a){
this.lastTop=_6a;
if(!this._nativeScroll){
this.scrollboxNode.scrollTop=_6a;
}
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
},setColWidth:function(_6b,_6c){
this.grid.setCellWidth(_6b,_6c+"px");
},update:function(){
if(!this.domNode){
return;
}
this.content.update();
this.grid.update();
var _6d=this.scrollboxNode.scrollLeft;
this.scrollboxNode.scrollLeft=_6d;
this.headerNode.scrollLeft=_6d;
}});
var _6e=_4("dojox.grid._GridAvatar",_13,{construct:function(){
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
var _6f=this.manager.source,_70;
if(_6f.creator){
_70=_6f._normalizedCreator(_6f.getItem(this.manager.nodes[0].id).data,"avatar").node;
}else{
_70=this.manager.nodes[0].cloneNode(true);
var _71,_72;
if(_70.tagName.toLowerCase()=="tr"){
_71=dd.createElement("table");
_72=dd.createElement("tbody");
_72.appendChild(_70);
_71.appendChild(_72);
_70=_71;
}else{
if(_70.tagName.toLowerCase()=="th"){
_71=dd.createElement("table");
_72=dd.createElement("tbody");
var r=dd.createElement("tr");
_71.cellPadding=_71.cellSpacing="0";
r.appendChild(_70);
_72.appendChild(r);
_71.appendChild(_72);
_70=_71;
}
}
}
_70.id="";
td.appendChild(_70);
tr.appendChild(img);
tr.appendChild(td);
_11.style(tr,"opacity",0.9);
b.appendChild(tr);
a.appendChild(b);
this.node=a;
var m=_14.manager();
this.oldOffsetY=m.OFFSET_Y;
m.OFFSET_Y=1;
},destroy:function(){
_14.manager().OFFSET_Y=this.oldOffsetY;
this.inherited(arguments);
}});
var _73=_14.manager().makeAvatar;
_14.manager().makeAvatar=function(){
var src=this.source;
if(src.viewIndex!==undefined&&!_11.hasClass(_a.body(),"dijit_a11y")){
return new _6e(this);
}
return _73.call(_14.manager());
};
return _18;
});
