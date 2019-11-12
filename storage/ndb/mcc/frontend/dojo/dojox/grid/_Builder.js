//>>built
define("dojox/grid/_Builder",["../main","dojo/_base/array","dojo/_base/lang","dojo/_base/window","dojo/_base/event","dojo/_base/sniff","dojo/_base/connect","dojo/dnd/Moveable","dojox/html/metrics","./util","dojo/_base/html","dojo/dom-geometry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var dg=_1.grid;
var _d=function(td){
return td.cellIndex>=0?td.cellIndex:_2.indexOf(td.parentNode.cells,td);
};
var _e=function(tr){
return tr.rowIndex>=0?tr.rowIndex:_2.indexOf(tr.parentNode.childNodes,tr);
};
var _f=function(_10,_11){
return _10&&((_10.rows||0)[_11]||_10.childNodes[_11]);
};
var _12=function(_13){
for(var n=_13;n&&n.tagName!="TABLE";n=n.parentNode){
}
return n;
};
var _14=function(_15,_16){
for(var n=_15;n&&_16(n);n=n.parentNode){
}
return n;
};
var _17=function(_18){
var _19=_18.toUpperCase();
return function(_1a){
return _1a.tagName!=_19;
};
};
var _1b=_a.rowIndexTag;
var _1c=_a.gridViewTag;
var _1d=dg._Builder=_3.extend(function(_1e){
if(_1e){
this.view=_1e;
this.grid=_1e.grid;
}
},{view:null,_table:"<table class=\"dojoxGridRowTable\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\" role=\"presentation\"",getTableArray:function(){
var _1f=[this._table];
if(this.view.viewWidth){
_1f.push([" style=\"width:",this.view.viewWidth,";\""].join(""));
}
_1f.push(">");
return _1f;
},generateCellMarkup:function(_20,_21,_22,_23){
var _24=[],_b;
if(_23){
var _25=_20.index!=_20.grid.getSortIndex()?"":_20.grid.sortInfo>0?"aria-sort=\"ascending\"":"aria-sort=\"descending\"";
if(!_20.id){
_20.id=this.grid.id+"Hdr"+_20.index;
}
_b=["<th tabIndex=\"-1\" aria-readonly=\"true\" role=\"columnheader\"",_25,"id=\"",_20.id,"\""];
}else{
var _26=this.grid.editable&&!_20.editable?"aria-readonly=\"true\"":"";
_b=["<td tabIndex=\"-1\" role=\"gridcell\"",_26];
}
if(_20.colSpan){
_b.push(" colspan=\"",_20.colSpan,"\"");
}
if(_20.rowSpan){
_b.push(" rowspan=\"",_20.rowSpan,"\"");
}
_b.push(" class=\"dojoxGridCell ");
if(_20.classes){
_b.push(_20.classes," ");
}
if(_22){
_b.push(_22," ");
}
_24.push(_b.join(""));
_24.push("");
_b=["\" idx=\"",_20.index,"\" style=\""];
if(_21&&_21[_21.length-1]!=";"){
_21+=";";
}
_b.push(_20.styles,_21||"",_20.hidden?"display:none;":"");
if(_20.unitWidth){
_b.push("width:",_20.unitWidth,";");
}
_24.push(_b.join(""));
_24.push("");
_b=["\""];
if(_20.attrs){
_b.push(" ",_20.attrs);
}
_b.push(">");
_24.push(_b.join(""));
_24.push("");
_24.push(_23?"</th>":"</td>");
return _24;
},isCellNode:function(_27){
return Boolean(_27&&_27!=_4.doc&&_b.attr(_27,"idx"));
},getCellNodeIndex:function(_28){
return _28?Number(_b.attr(_28,"idx")):-1;
},getCellNode:function(_29,_2a){
for(var i=0,row;((row=_f(_29.firstChild,i))&&row.cells);i++){
for(var j=0,_2b;(_2b=row.cells[j]);j++){
if(this.getCellNodeIndex(_2b)==_2a){
return _2b;
}
}
}
return null;
},findCellTarget:function(_2c,_2d){
var n=_2c;
while(n&&(!this.isCellNode(n)||(n.offsetParent&&_1c in n.offsetParent.parentNode&&n.offsetParent.parentNode[_1c]!=this.view.id))&&(n!=_2d)){
n=n.parentNode;
}
return n!=_2d?n:null;
},baseDecorateEvent:function(e){
e.dispatch="do"+e.type;
e.grid=this.grid;
e.sourceView=this.view;
e.cellNode=this.findCellTarget(e.target,e.rowNode);
e.cellIndex=this.getCellNodeIndex(e.cellNode);
e.cell=(e.cellIndex>=0?this.grid.getCell(e.cellIndex):null);
},findTarget:function(_2e,_2f){
var n=_2e;
while(n&&(n!=this.domNode)&&(!(_2f in n)||(_1c in n&&n[_1c]!=this.view.id))){
n=n.parentNode;
}
return (n!=this.domNode)?n:null;
},findRowTarget:function(_30){
return this.findTarget(_30,_1b);
},isIntraNodeEvent:function(e){
try{
return (e.cellNode&&e.relatedTarget&&_b.isDescendant(e.relatedTarget,e.cellNode));
}
catch(x){
return false;
}
},isIntraRowEvent:function(e){
try{
var row=e.relatedTarget&&this.findRowTarget(e.relatedTarget);
return !row&&(e.rowIndex==-1)||row&&(e.rowIndex==row.gridRowIndex);
}
catch(x){
return false;
}
},dispatchEvent:function(e){
if(e.dispatch in this){
return this[e.dispatch](e);
}
return false;
},domouseover:function(e){
if(e.cellNode&&(e.cellNode!=this.lastOverCellNode)){
this.lastOverCellNode=e.cellNode;
this.grid.onMouseOver(e);
}
this.grid.onMouseOverRow(e);
},domouseout:function(e){
if(e.cellNode&&(e.cellNode==this.lastOverCellNode)&&!this.isIntraNodeEvent(e,this.lastOverCellNode)){
this.lastOverCellNode=null;
this.grid.onMouseOut(e);
if(!this.isIntraRowEvent(e)){
this.grid.onMouseOutRow(e);
}
}
},domousedown:function(e){
if(e.cellNode){
this.grid.onMouseDown(e);
}
this.grid.onMouseDownRow(e);
}});
var _31=dg._ContentBuilder=_3.extend(function(_32){
_1d.call(this,_32);
},_1d.prototype,{update:function(){
this.prepareHtml();
},prepareHtml:function(){
var _33=this.grid.get,_34=this.view.structure.cells;
for(var j=0,row;(row=_34[j]);j++){
for(var i=0,_35;(_35=row[i]);i++){
_35.get=_35.get||(_35.value==undefined)&&_33;
_35.markup=this.generateCellMarkup(_35,_35.cellStyles,_35.cellClasses,false);
if(!this.grid.editable&&_35.editable){
this.grid.editable=true;
}
}
}
},generateHtml:function(_36,_37){
var _38=this.getTableArray(),v=this.view,_39=v.structure.cells,_3a=this.grid.getItem(_37);
_a.fire(this.view,"onBeforeRow",[_37,_39]);
for(var j=0,row;(row=_39[j]);j++){
if(row.hidden||row.header){
continue;
}
_38.push(!row.invisible?"<tr>":"<tr class=\"dojoxGridInvisible\">");
for(var i=0,_3b,m,cc,cs;(_3b=row[i]);i++){
m=_3b.markup;
cc=_3b.customClasses=[];
cs=_3b.customStyles=[];
m[5]=_3b.format(_37,_3a);
m[1]=cc.join(" ");
m[3]=cs.join(";");
_38.push.apply(_38,m);
}
_38.push("</tr>");
}
_38.push("</table>");
return _38.join("");
},decorateEvent:function(e){
e.rowNode=this.findRowTarget(e.target);
if(!e.rowNode){
return false;
}
e.rowIndex=e.rowNode[_1b];
this.baseDecorateEvent(e);
e.cell=this.grid.getCell(e.cellIndex);
return true;
}});
var _3c=dg._HeaderBuilder=_3.extend(function(_3d){
this.moveable=null;
_1d.call(this,_3d);
},_1d.prototype,{_skipBogusClicks:false,overResizeWidth:4,minColWidth:1,update:function(){
if(this.tableMap){
this.tableMap.mapRows(this.view.structure.cells);
}else{
this.tableMap=new dg._TableMap(this.view.structure.cells);
}
},generateHtml:function(_3e,_3f){
var _40=this.getTableArray(),_41=this.view.structure.cells;
_a.fire(this.view,"onBeforeRow",[-1,_41]);
for(var j=0,row;(row=_41[j]);j++){
if(row.hidden){
continue;
}
_40.push(!row.invisible?"<tr>":"<tr class=\"dojoxGridInvisible\">");
for(var i=0,_42,_43;(_42=row[i]);i++){
_42.customClasses=[];
_42.customStyles=[];
if(this.view.simpleStructure){
if(_42.draggable){
if(_42.headerClasses){
if(_42.headerClasses.indexOf("dojoDndItem")==-1){
_42.headerClasses+=" dojoDndItem";
}
}else{
_42.headerClasses="dojoDndItem";
}
}
if(_42.attrs){
if(_42.attrs.indexOf("dndType='gridColumn_")==-1){
_42.attrs+=" dndType='gridColumn_"+this.grid.id+"'";
}
}else{
_42.attrs="dndType='gridColumn_"+this.grid.id+"'";
}
}
_43=this.generateCellMarkup(_42,_42.headerStyles,_42.headerClasses,true);
_43[5]=(_3f!=undefined?_3f:_3e(_42));
_43[3]=_42.customStyles.join(";");
_43[1]=_42.customClasses.join(" ");
_40.push(_43.join(""));
}
_40.push("</tr>");
}
_40.push("</table>");
return _40.join("");
},getCellX:function(e){
var n,x,pos;
n=_14(e.target,_17("th"));
if(n){
pos=_c.position(n);
x=e.clientX-pos.x;
}else{
x=e.layerX;
}
return x;
},decorateEvent:function(e){
this.baseDecorateEvent(e);
e.rowIndex=-1;
e.cellX=this.getCellX(e);
return true;
},prepareResize:function(e,mod){
do{
var i=e.cellIndex;
e.cellNode=(i?e.cellNode.parentNode.cells[i+mod]:null);
e.cellIndex=(e.cellNode?this.getCellNodeIndex(e.cellNode):-1);
}while(e.cellNode&&e.cellNode.style.display=="none");
return Boolean(e.cellNode);
},canResize:function(e){
if(!e.cellNode||e.cellNode.colSpan>1){
return false;
}
var _44=this.grid.getCell(e.cellIndex);
return !_44.noresize&&_44.canResize();
},overLeftResizeArea:function(e){
if(_b.hasClass(_4.body(),"dojoDndMove")){
return false;
}
if(_6("ie")){
var tN=e.target;
if(_b.hasClass(tN,"dojoxGridArrowButtonNode")||_b.hasClass(tN,"dojoxGridArrowButtonChar")||_b.hasClass(tN,"dojoxGridColCaption")){
return false;
}
}
if(this.grid.isLeftToRight()){
return (e.cellIndex>0)&&(e.cellX>0&&e.cellX<this.overResizeWidth)&&this.prepareResize(e,-1);
}
var t=e.cellNode&&(e.cellX>0&&e.cellX<this.overResizeWidth);
return t;
},overRightResizeArea:function(e){
if(_b.hasClass(_4.body(),"dojoDndMove")){
return false;
}
if(_6("ie")){
var tN=e.target;
if(_b.hasClass(tN,"dojoxGridArrowButtonNode")||_b.hasClass(tN,"dojoxGridArrowButtonChar")||_b.hasClass(tN,"dojoxGridColCaption")){
return false;
}
}
if(this.grid.isLeftToRight()){
return e.cellNode&&(e.cellX>=e.cellNode.offsetWidth-this.overResizeWidth);
}
return (e.cellIndex>0)&&(e.cellX>=e.cellNode.offsetWidth-this.overResizeWidth)&&this.prepareResize(e,-1);
},domousemove:function(e){
if(!this.moveable){
var c=(this.overRightResizeArea(e)?"dojoxGridColResize":(this.overLeftResizeArea(e)?"dojoxGridColResize":""));
if(c&&!this.canResize(e)){
c="dojoxGridColNoResize";
}
_b.toggleClass(e.sourceView.headerNode,"dojoxGridColNoResize",(c=="dojoxGridColNoResize"));
_b.toggleClass(e.sourceView.headerNode,"dojoxGridColResize",(c=="dojoxGridColResize"));
if(c){
_5.stop(e);
}
}
},domousedown:function(e){
if(!this.moveable){
if((this.overRightResizeArea(e)||this.overLeftResizeArea(e))&&this.canResize(e)){
this.beginColumnResize(e);
}else{
this.grid.onMouseDown(e);
this.grid.onMouseOverRow(e);
}
}
},doclick:function(e){
if(this._skipBogusClicks){
_5.stop(e);
return true;
}
return false;
},colResizeSetup:function(e,_45){
var _46=_b.contentBox(e.sourceView.headerNode);
if(_45){
this.lineDiv=document.createElement("div");
var vw=_b.position(e.sourceView.headerNode,true);
var _47=_b.contentBox(e.sourceView.domNode);
var l=e.pageX;
if(!this.grid.isLeftToRight()&&_6("ie")<8){
l-=_9.getScrollbar().w;
}
_b.style(this.lineDiv,{top:vw.y+"px",left:l+"px",height:(_47.h+_46.h)+"px"});
_b.addClass(this.lineDiv,"dojoxGridResizeColLine");
this.lineDiv._origLeft=l;
_4.body().appendChild(this.lineDiv);
}
var _48=[],_49=this.tableMap.findOverlappingNodes(e.cellNode);
for(var i=0,_4a;(_4a=_49[i]);i++){
_48.push({node:_4a,index:this.getCellNodeIndex(_4a),width:_4a.offsetWidth});
}
var _4b=e.sourceView;
var adj=this.grid.isLeftToRight()?1:-1;
var _4c=e.grid.views.views;
var _4d=[];
for(var j=_4b.idx+adj,_4e;(_4e=_4c[j]);j=j+adj){
_4d.push({node:_4e.headerNode,left:window.parseInt(_4e.headerNode.style.left)});
}
var _4f=_4b.headerContentNode.firstChild;
var _50={scrollLeft:e.sourceView.headerNode.scrollLeft,view:_4b,node:e.cellNode,index:e.cellIndex,w:_b.contentBox(e.cellNode).w,vw:_46.w,table:_4f,tw:_b.contentBox(_4f).w,spanners:_48,followers:_4d};
return _50;
},beginColumnResize:function(e){
this.moverDiv=document.createElement("div");
_b.style(this.moverDiv,{position:"absolute",left:0});
_4.body().appendChild(this.moverDiv);
_b.addClass(this.grid.domNode,"dojoxGridColumnResizing");
var m=(this.moveable=new _8(this.moverDiv));
var _51=this.colResizeSetup(e,true);
m.onMove=_3.hitch(this,"doResizeColumn",_51);
_7.connect(m,"onMoveStop",_3.hitch(this,function(){
this.endResizeColumn(_51);
if(_51.node.releaseCapture){
_51.node.releaseCapture();
}
this.moveable.destroy();
delete this.moveable;
this.moveable=null;
_b.removeClass(this.grid.domNode,"dojoxGridColumnResizing");
}));
if(e.cellNode.setCapture){
e.cellNode.setCapture();
}
m.onMouseDown(e);
},doResizeColumn:function(_52,_53,_54){
var _55=_54.l;
var _56={deltaX:_55,w:_52.w+(this.grid.isLeftToRight()?_55:-_55),vw:_52.vw+_55,tw:_52.tw+_55};
this.dragRecord={inDrag:_52,mover:_53,leftTop:_54};
if(_56.w>=this.minColWidth){
if(!_53){
this.doResizeNow(_52,_56);
}else{
_b.style(this.lineDiv,"left",(this.lineDiv._origLeft+_56.deltaX)+"px");
}
}
},endResizeColumn:function(_57){
if(this.dragRecord){
var _58=this.dragRecord.leftTop;
var _59=this.grid.isLeftToRight()?_58.l:-_58.l;
_59+=Math.max(_57.w+_59,this.minColWidth)-(_57.w+_59);
if(_6("webkit")&&_57.spanners.length){
_59+=_b._getPadBorderExtents(_57.spanners[0].node).w;
}
var _5a={deltaX:_59,w:_57.w+_59,vw:_57.vw+_59,tw:_57.tw+_59};
this.doResizeNow(_57,_5a);
delete this.dragRecord;
}
_b.destroy(this.lineDiv);
_b.destroy(this.moverDiv);
_b.destroy(this.moverDiv);
delete this.moverDiv;
this._skipBogusClicks=true;
_57.view.update();
this._skipBogusClicks=false;
this.grid.onResizeColumn(_57.index);
},doResizeNow:function(_5b,_5c){
_5b.view.convertColPctToFixed();
if(_5b.view.flexCells&&!_5b.view.testFlexCells()){
var t=_12(_5b.node);
if(t){
(t.style.width="");
}
}
var i,s,sw,f,fl;
for(i=0;(s=_5b.spanners[i]);i++){
sw=s.width+_5c.deltaX;
if(sw>0){
s.node.style.width=sw+"px";
_5b.view.setColWidth(s.index,sw);
}
}
if(this.grid.isLeftToRight()||!_6("ie")){
for(i=0;(f=_5b.followers[i]);i++){
fl=f.left+_5c.deltaX;
f.node.style.left=fl+"px";
}
}
_5b.node.style.width=_5c.w+"px";
_5b.view.setColWidth(_5b.index,_5c.w);
_5b.view.headerNode.style.width=_5c.vw+"px";
_5b.view.setColumnsWidth(_5c.tw);
if(!this.grid.isLeftToRight()){
_5b.view.headerNode.scrollLeft=_5b.scrollLeft+_5c.deltaX;
}
}});
dg._TableMap=_3.extend(function(_5d){
this.mapRows(_5d);
},{map:null,mapRows:function(_5e){
var _5f=_5e.length;
if(!_5f){
return;
}
this.map=[];
var row;
for(var k=0;(row=_5e[k]);k++){
this.map[k]=[];
}
for(var j=0;(row=_5e[j]);j++){
for(var i=0,x=0,_60,_61,_62;(_60=row[i]);i++){
while(this.map[j][x]){
x++;
}
this.map[j][x]={c:i,r:j};
_62=_60.rowSpan||1;
_61=_60.colSpan||1;
for(var y=0;y<_62;y++){
for(var s=0;s<_61;s++){
this.map[j+y][x+s]=this.map[j][x];
}
}
x+=_61;
}
}
},dumpMap:function(){
for(var j=0,row,h="";(row=this.map[j]);j++,h=""){
for(var i=0,_63;(_63=row[i]);i++){
h+=_63.r+","+_63.c+"   ";
}
}
},getMapCoords:function(_64,_65){
for(var j=0,row;(row=this.map[j]);j++){
for(var i=0,_66;(_66=row[i]);i++){
if(_66.c==_65&&_66.r==_64){
return {j:j,i:i};
}
}
}
return {j:-1,i:-1};
},getNode:function(_67,_68,_69){
var row=_67&&_67.rows[_68];
return row&&row.cells[_69];
},_findOverlappingNodes:function(_6a,_6b,_6c){
var _6d=[];
var m=this.getMapCoords(_6b,_6c);
for(var j=0,row;(row=this.map[j]);j++){
if(j==m.j){
continue;
}
var rw=row[m.i];
var n=(rw?this.getNode(_6a,rw.r,rw.c):null);
if(n){
_6d.push(n);
}
}
return _6d;
},findOverlappingNodes:function(_6e){
return this._findOverlappingNodes(_12(_6e),_e(_6e.parentNode),_d(_6e));
}});
return {_Builder:_1d,_HeaderBuilder:_3c,_ContentBuilder:_31};
});
