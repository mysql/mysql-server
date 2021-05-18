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
_b=["<th tabIndex=\"-1\" aria-readonly=\"true\" role=\"columnheader\"",_25," id=\"",_20.id,"\""];
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
},_getTextDirStyle:function(_31,_32,_33){
return "";
}});
var _34=dg._ContentBuilder=_3.extend(function(_35){
_1d.call(this,_35);
},_1d.prototype,{update:function(){
this.prepareHtml();
},prepareHtml:function(){
var _36=this.grid.get,_37=this.view.structure.cells;
for(var j=0,row;(row=_37[j]);j++){
for(var i=0,_38;(_38=row[i]);i++){
_38.get=_38.get||(_38.value==undefined)&&_36;
_38.markup=this.generateCellMarkup(_38,_38.cellStyles,_38.cellClasses,false);
if(!this.grid.editable&&_38.editable){
this.grid.editable=true;
}
}
}
},generateHtml:function(_39,_3a){
var _3b=this.getTableArray();
var v=this.view;
var _3c=v.structure.cells;
var _3d=this.grid.getItem(_3a);
var dir;
_a.fire(this.view,"onBeforeRow",[_3a,_3c]);
for(var j=0,row;(row=_3c[j]);j++){
if(row.hidden||row.header){
continue;
}
_3b.push(!row.invisible?"<tr>":"<tr class=\"dojoxGridInvisible\">");
for(var i=0,_3e,m,cc,cs;(_3e=row[i]);i++){
m=_3e.markup;
cc=_3e.customClasses=[];
cs=_3e.customStyles=[];
m[5]=_3e.format(_3a,_3d);
m[1]=cc.join(" ");
m[3]=cs.join(";");
dir=_3e.textDir||this.grid.textDir;
if(dir){
m[3]+=this._getTextDirStyle(dir,_3e,_3a);
}
_3b.push.apply(_3b,m);
}
_3b.push("</tr>");
}
_3b.push("</table>");
return _3b.join("");
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
var _3f=dg._HeaderBuilder=_3.extend(function(_40){
this.moveable=null;
_1d.call(this,_40);
},_1d.prototype,{_skipBogusClicks:false,overResizeWidth:4,minColWidth:1,update:function(){
if(this.tableMap){
this.tableMap.mapRows(this.view.structure.cells);
}else{
this.tableMap=new dg._TableMap(this.view.structure.cells);
}
},generateHtml:function(_41,_42){
var dir,_b=this.getTableArray(),_43=this.view.structure.cells;
_a.fire(this.view,"onBeforeRow",[-1,_43]);
for(var j=0,row;(row=_43[j]);j++){
if(row.hidden){
continue;
}
_b.push(!row.invisible?"<tr>":"<tr class=\"dojoxGridInvisible\">");
for(var i=0,_44,_45;(_44=row[i]);i++){
_44.customClasses=[];
_44.customStyles=[];
if(this.view.simpleStructure){
if(_44.draggable){
if(_44.headerClasses){
if(_44.headerClasses.indexOf("dojoDndItem")==-1){
_44.headerClasses+=" dojoDndItem";
}
}else{
_44.headerClasses="dojoDndItem";
}
}
if(_44.attrs){
if(_44.attrs.indexOf("dndType='gridColumn_")==-1){
_44.attrs+=" dndType='gridColumn_"+this.grid.id+"'";
}
}else{
_44.attrs="dndType='gridColumn_"+this.grid.id+"'";
}
}
_45=this.generateCellMarkup(_44,_44.headerStyles,_44.headerClasses,true);
_45[5]=(_42!=undefined?_42:_41(_44));
_45[3]=_44.customStyles.join(";");
dir=_44.textDir||this.grid.textDir;
if(dir){
_45[3]+=this._getTextDirStyle(dir,_44,_42);
}
_45[1]=_44.customClasses.join(" ");
_b.push(_45.join(""));
}
_b.push("</tr>");
}
_b.push("</table>");
return _b.join("");
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
var _46=this.grid.getCell(e.cellIndex);
return !_46.noresize&&_46.canResize();
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
},colResizeSetup:function(e,_47){
var _48=_b.contentBox(e.sourceView.headerNode);
if(_47){
this.lineDiv=document.createElement("div");
var vw=_b.position(e.sourceView.headerNode,true);
var _49=_b.contentBox(e.sourceView.domNode);
var l=e.pageX;
if(!this.grid.isLeftToRight()&&_6("ie")<8){
l-=_9.getScrollbar().w;
}
_b.style(this.lineDiv,{top:vw.y+"px",left:l+"px",height:(_49.h+_48.h)+"px"});
_b.addClass(this.lineDiv,"dojoxGridResizeColLine");
this.lineDiv._origLeft=l;
_4.body().appendChild(this.lineDiv);
}
var _4a=[],_4b=this.tableMap.findOverlappingNodes(e.cellNode);
for(var i=0,_4c;(_4c=_4b[i]);i++){
_4a.push({node:_4c,index:this.getCellNodeIndex(_4c),width:_4c.offsetWidth});
}
var _4d=e.sourceView;
var adj=this.grid.isLeftToRight()?1:-1;
var _4e=e.grid.views.views;
var _4f=[];
for(var j=_4d.idx+adj,_50;(_50=_4e[j]);j=j+adj){
_4f.push({node:_50.headerNode,left:window.parseInt(_50.headerNode.style.left)});
}
var _51=_4d.headerContentNode.firstChild;
var _52={scrollLeft:e.sourceView.headerNode.scrollLeft,view:_4d,node:e.cellNode,index:e.cellIndex,w:_b.contentBox(e.cellNode).w,vw:_48.w,table:_51,tw:_b.contentBox(_51).w,spanners:_4a,followers:_4f};
return _52;
},beginColumnResize:function(e){
this.moverDiv=document.createElement("div");
_b.style(this.moverDiv,{position:"absolute",left:0});
_4.body().appendChild(this.moverDiv);
_b.addClass(this.grid.domNode,"dojoxGridColumnResizing");
var m=(this.moveable=new _8(this.moverDiv));
var _53=this.colResizeSetup(e,true);
m.onMove=_3.hitch(this,"doResizeColumn",_53);
_7.connect(m,"onMoveStop",_3.hitch(this,function(){
this.endResizeColumn(_53);
if(_53.node.releaseCapture){
_53.node.releaseCapture();
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
},doResizeColumn:function(_54,_55,_56){
var _57=_56.l;
var _58={deltaX:_57,w:_54.w+(this.grid.isLeftToRight()?_57:-_57),vw:_54.vw+_57,tw:_54.tw+_57};
this.dragRecord={inDrag:_54,mover:_55,leftTop:_56};
if(_58.w>=this.minColWidth){
if(!_55){
this.doResizeNow(_54,_58);
}else{
_b.style(this.lineDiv,"left",(this.lineDiv._origLeft+_58.deltaX)+"px");
}
}
},endResizeColumn:function(_59){
if(this.dragRecord){
var _5a=this.dragRecord.leftTop;
var _5b=this.grid.isLeftToRight()?_5a.l:-_5a.l;
_5b+=Math.max(_59.w+_5b,this.minColWidth)-(_59.w+_5b);
if(_6("webkit")&&_59.spanners.length){
_5b+=_b._getPadBorderExtents(_59.spanners[0].node).w;
}
var _5c={deltaX:_5b,w:_59.w+_5b,vw:_59.vw+_5b,tw:_59.tw+_5b};
this.doResizeNow(_59,_5c);
delete this.dragRecord;
}
_b.destroy(this.lineDiv);
_b.destroy(this.moverDiv);
_b.destroy(this.moverDiv);
delete this.moverDiv;
this._skipBogusClicks=true;
_59.view.update();
this._skipBogusClicks=false;
this.grid.onResizeColumn(_59.index);
},doResizeNow:function(_5d,_5e){
_5d.view.convertColPctToFixed();
if(_5d.view.flexCells&&!_5d.view.testFlexCells()){
var t=_12(_5d.node);
if(t){
(t.style.width="");
}
}
var i,s,sw,f,fl;
for(i=0;(s=_5d.spanners[i]);i++){
sw=s.width+_5e.deltaX;
if(sw>0){
s.node.style.width=sw+"px";
_5d.view.setColWidth(s.index,sw);
}
}
if(this.grid.isLeftToRight()||!_6("ie")){
for(i=0;(f=_5d.followers[i]);i++){
fl=f.left+_5e.deltaX;
f.node.style.left=fl+"px";
}
}
_5d.node.style.width=_5e.w+"px";
_5d.view.setColWidth(_5d.index,_5e.w);
_5d.view.headerNode.style.width=_5e.vw+"px";
_5d.view.setColumnsWidth(_5e.tw);
if(!this.grid.isLeftToRight()){
_5d.view.headerNode.scrollLeft=_5d.scrollLeft+_5e.deltaX;
}
}});
dg._TableMap=_3.extend(function(_5f){
this.mapRows(_5f);
},{map:null,mapRows:function(_60){
var _61=_60.length;
if(!_61){
return;
}
this.map=[];
var row;
for(var k=0;(row=_60[k]);k++){
this.map[k]=[];
}
for(var j=0;(row=_60[j]);j++){
for(var i=0,x=0,_62,_63,_64;(_62=row[i]);i++){
while(this.map[j][x]){
x++;
}
this.map[j][x]={c:i,r:j};
_64=_62.rowSpan||1;
_63=_62.colSpan||1;
for(var y=0;y<_64;y++){
for(var s=0;s<_63;s++){
this.map[j+y][x+s]=this.map[j][x];
}
}
x+=_63;
}
}
},dumpMap:function(){
for(var j=0,row,h="";(row=this.map[j]);j++,h=""){
for(var i=0,_65;(_65=row[i]);i++){
h+=_65.r+","+_65.c+"   ";
}
}
},getMapCoords:function(_66,_67){
for(var j=0,row;(row=this.map[j]);j++){
for(var i=0,_68;(_68=row[i]);i++){
if(_68.c==_67&&_68.r==_66){
return {j:j,i:i};
}
}
}
return {j:-1,i:-1};
},getNode:function(_69,_6a,_6b){
var row=_69&&_69.rows[_6a];
return row&&row.cells[_6b];
},_findOverlappingNodes:function(_6c,_6d,_6e){
var _6f=[];
var m=this.getMapCoords(_6d,_6e);
for(var j=0,row;(row=this.map[j]);j++){
if(j==m.j){
continue;
}
var rw=row[m.i];
var n=(rw?this.getNode(_6c,rw.r,rw.c):null);
if(n){
_6f.push(n);
}
}
return _6f;
},findOverlappingNodes:function(_70){
return this._findOverlappingNodes(_12(_70),_e(_70.parentNode),_d(_70));
}});
return {_Builder:_1d,_HeaderBuilder:_3f,_ContentBuilder:_34};
});
