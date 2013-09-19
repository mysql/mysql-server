//>>built
define("dojox/grid/_FocusManager",["dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/_base/event","dojo/_base/sniff","dojo/query","./util","dojo/_base/html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _3("dojox.grid._FocusManager",null,{constructor:function(_a){
this.grid=_a;
this.cell=null;
this.rowIndex=-1;
this._connects=[];
this._headerConnects=[];
this.headerMenu=this.grid.headerMenu;
this._connects.push(_4.connect(this.grid.domNode,"onfocus",this,"doFocus"));
this._connects.push(_4.connect(this.grid.domNode,"onblur",this,"doBlur"));
this._connects.push(_4.connect(this.grid.domNode,"mousedown",this,"_mouseDown"));
this._connects.push(_4.connect(this.grid.domNode,"mouseup",this,"_mouseUp"));
this._connects.push(_4.connect(this.grid.domNode,"oncontextmenu",this,"doContextMenu"));
this._connects.push(_4.connect(this.grid.lastFocusNode,"onfocus",this,"doLastNodeFocus"));
this._connects.push(_4.connect(this.grid.lastFocusNode,"onblur",this,"doLastNodeBlur"));
this._connects.push(_4.connect(this.grid,"_onFetchComplete",this,"_delayedCellFocus"));
this._connects.push(_4.connect(this.grid,"postrender",this,"_delayedHeaderFocus"));
},destroy:function(){
_1.forEach(this._connects,_4.disconnect);
_1.forEach(this._headerConnects,_4.disconnect);
delete this.grid;
delete this.cell;
},_colHeadNode:null,_colHeadFocusIdx:null,_contextMenuBindNode:null,tabbingOut:false,focusClass:"dojoxGridCellFocus",focusView:null,initFocusView:function(){
this.focusView=this.grid.views.getFirstScrollingView()||this.focusView||this.grid.views.views[0];
this._initColumnHeaders();
},isFocusCell:function(_b,_c){
return (this.cell==_b)&&(this.rowIndex==_c);
},isLastFocusCell:function(){
if(this.cell){
return (this.rowIndex==this.grid.rowCount-1)&&(this.cell.index==this.grid.layout.cellCount-1);
}
return false;
},isFirstFocusCell:function(){
if(this.cell){
return (this.rowIndex===0)&&(this.cell.index===0);
}
return false;
},isNoFocusCell:function(){
return (this.rowIndex<0)||!this.cell;
},isNavHeader:function(){
return (!!this._colHeadNode);
},getHeaderIndex:function(){
if(this._colHeadNode){
return _1.indexOf(this._findHeaderCells(),this._colHeadNode);
}else{
return -1;
}
},_focusifyCellNode:function(_d){
var n=this.cell&&this.cell.getNode(this.rowIndex);
if(n){
_9.toggleClass(n,this.focusClass,_d);
if(_d){
var sl=this.scrollIntoView();
try{
if(!this.grid.edit.isEditing()){
_8.fire(n,"focus");
if(sl){
this.cell.view.scrollboxNode.scrollLeft=sl;
}
}
}
catch(e){
}
}
}
},_delayedCellFocus:function(){
if(this.isNavHeader()||!this.grid.focused){
return;
}
var n=this.cell&&this.cell.getNode(this.rowIndex);
if(n){
try{
if(!this.grid.edit.isEditing()){
_9.toggleClass(n,this.focusClass,true);
if(this._colHeadNode){
this.blurHeader();
}
_8.fire(n,"focus");
}
}
catch(e){
}
}
},_delayedHeaderFocus:function(){
if(this.isNavHeader()){
this.focusHeader();
this.grid.domNode.focus();
}
},_initColumnHeaders:function(){
_1.forEach(this._headerConnects,_4.disconnect);
this._headerConnects=[];
var _e=this._findHeaderCells();
for(var i=0;i<_e.length;i++){
this._headerConnects.push(_4.connect(_e[i],"onfocus",this,"doColHeaderFocus"));
this._headerConnects.push(_4.connect(_e[i],"onblur",this,"doColHeaderBlur"));
}
},_findHeaderCells:function(){
var _f=_7("th",this.grid.viewsHeaderNode);
var _10=[];
for(var i=0;i<_f.length;i++){
var _11=_f[i];
var _12=_9.hasAttr(_11,"tabIndex");
var _13=_9.attr(_11,"tabIndex");
if(_12&&_13<0){
_10.push(_11);
}
}
return _10;
},_setActiveColHeader:function(_14,_15,_16){
this.grid.domNode.setAttribute("aria-activedescendant",_14.id);
if(_16!=null&&_16>=0&&_16!=_15){
_9.toggleClass(this._findHeaderCells()[_16],this.focusClass,false);
}
_9.toggleClass(_14,this.focusClass,true);
this._colHeadNode=_14;
this._colHeadFocusIdx=_15;
this._scrollHeader(this._colHeadFocusIdx);
},scrollIntoView:function(){
var _17=(this.cell?this._scrollInfo(this.cell):null);
if(!_17||!_17.s){
return null;
}
var rt=this.grid.scroller.findScrollTop(this.rowIndex);
if(_17.n&&_17.sr){
if(_17.n.offsetLeft+_17.n.offsetWidth>_17.sr.l+_17.sr.w){
_17.s.scrollLeft=_17.n.offsetLeft+_17.n.offsetWidth-_17.sr.w;
}else{
if(_17.n.offsetLeft<_17.sr.l){
_17.s.scrollLeft=_17.n.offsetLeft;
}
}
}
if(_17.r&&_17.sr){
if(rt+_17.r.offsetHeight>_17.sr.t+_17.sr.h){
this.grid.setScrollTop(rt+_17.r.offsetHeight-_17.sr.h);
}else{
if(rt<_17.sr.t){
this.grid.setScrollTop(rt);
}
}
}
return _17.s.scrollLeft;
},_scrollInfo:function(_18,_19){
if(_18){
var cl=_18,sbn=cl.view.scrollboxNode,_1a={w:sbn.clientWidth,l:sbn.scrollLeft,t:sbn.scrollTop,h:sbn.clientHeight},rn=cl.view.getRowNode(this.rowIndex);
return {c:cl,s:sbn,sr:_1a,n:(_19?_19:_18.getNode(this.rowIndex)),r:rn};
}
return null;
},_scrollHeader:function(_1b){
var _1c=null;
if(this._colHeadNode){
var _1d=this.grid.getCell(_1b);
if(!_1d){
return;
}
_1c=this._scrollInfo(_1d,_1d.getNode(0));
}
if(_1c&&_1c.s&&_1c.sr&&_1c.n){
var _1e=_1c.sr.l+_1c.sr.w;
if(_1c.n.offsetLeft+_1c.n.offsetWidth>_1e){
_1c.s.scrollLeft=_1c.n.offsetLeft+_1c.n.offsetWidth-_1c.sr.w;
}else{
if(_1c.n.offsetLeft<_1c.sr.l){
_1c.s.scrollLeft=_1c.n.offsetLeft;
}else{
if(_6("ie")<=7&&_1d&&_1d.view.headerNode){
_1d.view.headerNode.scrollLeft=_1c.s.scrollLeft;
}
}
}
}
},_isHeaderHidden:function(){
var _1f=this.focusView;
if(!_1f){
for(var i=0,_20;(_20=this.grid.views.views[i]);i++){
if(_20.headerNode){
_1f=_20;
break;
}
}
}
return (_1f&&_9.getComputedStyle(_1f.headerNode).display=="none");
},colSizeAdjust:function(e,_21,_22){
var _23=this._findHeaderCells();
var _24=this.focusView;
if(!_24){
for(var i=0,_25;(_25=this.grid.views.views[i]);i++){
if(_25.header.tableMap.map){
_24=_25;
break;
}
}
}
var _26=_23[_21];
if(!_24||(_21==_23.length-1&&_21===0)){
return;
}
_24.content.baseDecorateEvent(e);
e.cellNode=_26;
e.cellIndex=_24.content.getCellNodeIndex(e.cellNode);
e.cell=(e.cellIndex>=0?this.grid.getCell(e.cellIndex):null);
if(_24.header.canResize(e)){
var _27={l:_22};
var _28=_24.header.colResizeSetup(e,false);
_24.header.doResizeColumn(_28,null,_27);
_24.update();
}
},styleRow:function(_29){
return;
},setFocusIndex:function(_2a,_2b){
this.setFocusCell(this.grid.getCell(_2b),_2a);
},setFocusCell:function(_2c,_2d){
if(_2c&&!this.isFocusCell(_2c,_2d)){
this.tabbingOut=false;
if(this._colHeadNode){
this.blurHeader();
}
this._colHeadNode=this._colHeadFocusIdx=null;
this.focusGridView();
this._focusifyCellNode(false);
this.cell=_2c;
this.rowIndex=_2d;
this._focusifyCellNode(true);
}
if(_6("opera")){
setTimeout(_2.hitch(this.grid,"onCellFocus",this.cell,this.rowIndex),1);
}else{
this.grid.onCellFocus(this.cell,this.rowIndex);
}
},next:function(){
if(this.cell){
var row=this.rowIndex,col=this.cell.index+1,cc=this.grid.layout.cellCount-1,rc=this.grid.rowCount-1;
if(col>cc){
col=0;
row++;
}
if(row>rc){
col=cc;
row=rc;
}
if(this.grid.edit.isEditing()){
var _2e=this.grid.getCell(col);
if(!this.isLastFocusCell()&&(!_2e.editable||this.grid.canEdit&&!this.grid.canEdit(_2e,row))){
this.cell=_2e;
this.rowIndex=row;
this.next();
return;
}
}
this.setFocusIndex(row,col);
}
},previous:function(){
if(this.cell){
var row=(this.rowIndex||0),col=(this.cell.index||0)-1;
if(col<0){
col=this.grid.layout.cellCount-1;
row--;
}
if(row<0){
row=0;
col=0;
}
if(this.grid.edit.isEditing()){
var _2f=this.grid.getCell(col);
if(!this.isFirstFocusCell()&&!_2f.editable){
this.cell=_2f;
this.rowIndex=row;
this.previous();
return;
}
}
this.setFocusIndex(row,col);
}
},move:function(_30,_31){
var _32=_31<0?-1:1;
if(this.isNavHeader()){
var _33=this._findHeaderCells();
var _34=currentIdx=_1.indexOf(_33,this._colHeadNode);
currentIdx+=_31;
while(currentIdx>=0&&currentIdx<_33.length&&_33[currentIdx].style.display=="none"){
currentIdx+=_32;
}
if((currentIdx>=0)&&(currentIdx<_33.length)){
this._setActiveColHeader(_33[currentIdx],currentIdx,_34);
}
}else{
if(this.cell){
var sc=this.grid.scroller,r=this.rowIndex,rc=this.grid.rowCount-1,row=Math.min(rc,Math.max(0,r+_30));
if(_30){
if(_30>0){
if(row>sc.getLastPageRow(sc.page)){
this.grid.setScrollTop(this.grid.scrollTop+sc.findScrollTop(row)-sc.findScrollTop(r));
}
}else{
if(_30<0){
if(row<=sc.getPageRow(sc.page)){
this.grid.setScrollTop(this.grid.scrollTop-sc.findScrollTop(r)-sc.findScrollTop(row));
}
}
}
}
var cc=this.grid.layout.cellCount-1,i=this.cell.index,col=Math.min(cc,Math.max(0,i+_31));
var _35=this.grid.getCell(col);
while(col>=0&&col<cc&&_35&&_35.hidden===true){
col+=_32;
_35=this.grid.getCell(col);
}
if(!_35||_35.hidden===true){
col=i;
}
var n=_35.getNode(row);
if(!n&&_30){
if((row+_30)>=0&&(row+_30)<=rc){
this.move(_30>0?++_30:--_30,_31);
}
return;
}else{
if((!n||_9.style(n,"display")==="none")&&_31){
if((col+_30)>=0&&(col+_30)<=cc){
this.move(_30,_31>0?++_31:--_31);
}
return;
}
}
this.setFocusIndex(row,col);
if(_30){
this.grid.updateRow(r);
}
}
}
},previousKey:function(e){
if(this.grid.edit.isEditing()){
_5.stop(e);
this.previous();
}else{
if(!this.isNavHeader()&&!this._isHeaderHidden()){
this.grid.domNode.focus();
_5.stop(e);
}else{
this.tabOut(this.grid.domNode);
if(this._colHeadFocusIdx!=null){
_9.toggleClass(this._findHeaderCells()[this._colHeadFocusIdx],this.focusClass,false);
this._colHeadFocusIdx=null;
}
this._focusifyCellNode(false);
}
}
},nextKey:function(e){
var _36=(this.grid.rowCount===0);
if(e.target===this.grid.domNode&&this._colHeadFocusIdx==null){
this.focusHeader();
_5.stop(e);
}else{
if(this.isNavHeader()){
this.blurHeader();
if(!this.findAndFocusGridCell()){
this.tabOut(this.grid.lastFocusNode);
}
this._colHeadNode=this._colHeadFocusIdx=null;
}else{
if(this.grid.edit.isEditing()){
_5.stop(e);
this.next();
}else{
this.tabOut(this.grid.lastFocusNode);
}
}
}
},tabOut:function(_37){
this.tabbingOut=true;
_37.focus();
},focusGridView:function(){
_8.fire(this.focusView,"focus");
},focusGrid:function(_38){
this.focusGridView();
this._focusifyCellNode(true);
},findAndFocusGridCell:function(){
var _39=true;
var _3a=(this.grid.rowCount===0);
if(this.isNoFocusCell()&&!_3a){
var _3b=0;
var _3c=this.grid.getCell(_3b);
if(_3c.hidden){
_3b=this.isNavHeader()?this._colHeadFocusIdx:0;
}
this.setFocusIndex(0,_3b);
}else{
if(this.cell&&!_3a){
if(this.focusView&&!this.focusView.rowNodes[this.rowIndex]){
this.grid.scrollToRow(this.rowIndex);
}
this.focusGrid();
}else{
_39=false;
}
}
this._colHeadNode=this._colHeadFocusIdx=null;
return _39;
},focusHeader:function(){
var _3d=this._findHeaderCells();
var _3e=this._colHeadFocusIdx;
if(this._isHeaderHidden()){
this.findAndFocusGridCell();
}else{
if(!this._colHeadFocusIdx){
if(this.isNoFocusCell()){
this._colHeadFocusIdx=0;
}else{
this._colHeadFocusIdx=this.cell.index;
}
}
}
this._colHeadNode=_3d[this._colHeadFocusIdx];
while(this._colHeadNode&&this._colHeadFocusIdx>=0&&this._colHeadFocusIdx<_3d.length&&this._colHeadNode.style.display=="none"){
this._colHeadFocusIdx++;
this._colHeadNode=_3d[this._colHeadFocusIdx];
}
if(this._colHeadNode&&this._colHeadNode.style.display!="none"){
if(this.headerMenu&&this._contextMenuBindNode!=this.grid.domNode){
this.headerMenu.unBindDomNode(this.grid.viewsHeaderNode);
this.headerMenu.bindDomNode(this.grid.domNode);
this._contextMenuBindNode=this.grid.domNode;
}
this._setActiveColHeader(this._colHeadNode,this._colHeadFocusIdx,_3e);
this._scrollHeader(this._colHeadFocusIdx);
this._focusifyCellNode(false);
}else{
this.findAndFocusGridCell();
}
},blurHeader:function(){
_9.removeClass(this._colHeadNode,this.focusClass);
_9.removeAttr(this.grid.domNode,"aria-activedescendant");
if(this.headerMenu&&this._contextMenuBindNode==this.grid.domNode){
var _3f=this.grid.viewsHeaderNode;
this.headerMenu.unBindDomNode(this.grid.domNode);
this.headerMenu.bindDomNode(_3f);
this._contextMenuBindNode=_3f;
}
},doFocus:function(e){
if(e&&e.target!=e.currentTarget){
_5.stop(e);
return;
}
if(this._clickFocus){
return;
}
if(!this.tabbingOut){
this.focusHeader();
}
this.tabbingOut=false;
_5.stop(e);
},doBlur:function(e){
_5.stop(e);
},doContextMenu:function(e){
if(!this.headerMenu){
_5.stop(e);
}
},doLastNodeFocus:function(e){
if(this.tabbingOut){
this._focusifyCellNode(false);
}else{
if(this.grid.rowCount>0){
if(this.isNoFocusCell()){
this.setFocusIndex(0,0);
}
this._focusifyCellNode(true);
}else{
this.focusHeader();
}
}
this.tabbingOut=false;
_5.stop(e);
},doLastNodeBlur:function(e){
_5.stop(e);
},doColHeaderFocus:function(e){
this._setActiveColHeader(e.target,_9.attr(e.target,"idx"),this._colHeadFocusIdx);
this._scrollHeader(this.getHeaderIndex());
_5.stop(e);
},doColHeaderBlur:function(e){
_9.toggleClass(e.target,this.focusClass,false);
},_mouseDown:function(e){
this._clickFocus=dojo.some(this.grid.views.views,function(v){
return v.scrollboxNode===e.target;
});
},_mouseUp:function(e){
this._clickFocus=false;
}});
});
