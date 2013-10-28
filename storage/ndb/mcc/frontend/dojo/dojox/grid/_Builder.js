//>>built
define("dojox/grid/_Builder",["../main","dojo/_base/array","dojo/_base/lang","dojo/_base/window","dojo/_base/event","dojo/_base/sniff","dojo/_base/connect","dojo/dnd/Moveable","dojox/html/metrics","./util","dojo/_base/html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var dg=_1.grid;
var _c=function(td){
return td.cellIndex>=0?td.cellIndex:_2.indexOf(td.parentNode.cells,td);
};
var _d=function(tr){
return tr.rowIndex>=0?tr.rowIndex:_2.indexOf(tr.parentNode.childNodes,tr);
};
var _e=function(_f,_10){
return _f&&((_f.rows||0)[_10]||_f.childNodes[_10]);
};
var _11=function(_12){
for(var n=_12;n&&n.tagName!="TABLE";n=n.parentNode){
}
return n;
};
var _13=function(_14,_15){
for(var n=_14;n&&_15(n);n=n.parentNode){
}
return n;
};
var _16=function(_17){
var _18=_17.toUpperCase();
return function(_19){
return _19.tagName!=_18;
};
};
var _1a=_a.rowIndexTag;
var _1b=_a.gridViewTag;
var _1c=dg._Builder=_3.extend(function(_1d){
if(_1d){
this.view=_1d;
this.grid=_1d.grid;
}
},{view:null,_table:"<table class=\"dojoxGridRowTable\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\" role=\"presentation\"",getTableArray:function(){
var _1e=[this._table];
if(this.view.viewWidth){
_1e.push([" style=\"width:",this.view.viewWidth,";\""].join(""));
}
_1e.push(">");
return _1e;
},generateCellMarkup:function(_1f,_20,_21,_22){
var _23=[],_b;
if(_22){
var _24=_1f.index!=_1f.grid.getSortIndex()?"":_1f.grid.sortInfo>0?"aria-sort=\"ascending\"":"aria-sort=\"descending\"";
if(!_1f.id){
_1f.id=this.grid.id+"Hdr"+_1f.index;
}
_b=["<th tabIndex=\"-1\" aria-readonly=\"true\" role=\"columnheader\"",_24,"id=\"",_1f.id,"\""];
}else{
var _25=this.grid.editable&&!_1f.editable?"aria-readonly=\"true\"":"";
_b=["<td tabIndex=\"-1\" role=\"gridcell\"",_25];
}
if(_1f.colSpan){
_b.push(" colspan=\"",_1f.colSpan,"\"");
}
if(_1f.rowSpan){
_b.push(" rowspan=\"",_1f.rowSpan,"\"");
}
_b.push(" class=\"dojoxGridCell ");
if(_1f.classes){
_b.push(_1f.classes," ");
}
if(_21){
_b.push(_21," ");
}
_23.push(_b.join(""));
_23.push("");
_b=["\" idx=\"",_1f.index,"\" style=\""];
if(_20&&_20[_20.length-1]!=";"){
_20+=";";
}
_b.push(_1f.styles,_20||"",_1f.hidden?"display:none;":"");
if(_1f.unitWidth){
_b.push("width:",_1f.unitWidth,";");
}
_23.push(_b.join(""));
_23.push("");
_b=["\""];
if(_1f.attrs){
_b.push(" ",_1f.attrs);
}
_b.push(">");
_23.push(_b.join(""));
_23.push("");
_23.push(_22?"</th>":"</td>");
return _23;
},isCellNode:function(_26){
return Boolean(_26&&_26!=_4.doc&&_b.attr(_26,"idx"));
},getCellNodeIndex:function(_27){
return _27?Number(_b.attr(_27,"idx")):-1;
},getCellNode:function(_28,_29){
for(var i=0,row;((row=_e(_28.firstChild,i))&&row.cells);i++){
for(var j=0,_2a;(_2a=row.cells[j]);j++){
if(this.getCellNodeIndex(_2a)==_29){
return _2a;
}
}
}
return null;
},findCellTarget:function(_2b,_2c){
var n=_2b;
while(n&&(!this.isCellNode(n)||(n.offsetParent&&_1b in n.offsetParent.parentNode&&n.offsetParent.parentNode[_1b]!=this.view.id))&&(n!=_2c)){
n=n.parentNode;
}
return n!=_2c?n:null;
},baseDecorateEvent:function(e){
e.dispatch="do"+e.type;
e.grid=this.grid;
e.sourceView=this.view;
e.cellNode=this.findCellTarget(e.target,e.rowNode);
e.cellIndex=this.getCellNodeIndex(e.cellNode);
e.cell=(e.cellIndex>=0?this.grid.getCell(e.cellIndex):null);
},findTarget:function(_2d,_2e){
var n=_2d;
while(n&&(n!=this.domNode)&&(!(_2e in n)||(_1b in n&&n[_1b]!=this.view.id))){
n=n.parentNode;
}
return (n!=this.domNode)?n:null;
},findRowTarget:function(_2f){
return this.findTarget(_2f,_1a);
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
var _30=dg._ContentBuilder=_3.extend(function(_31){
_1c.call(this,_31);
},_1c.prototype,{update:function(){
this.prepareHtml();
},prepareHtml:function(){
var _32=this.grid.get,_33=this.view.structure.cells;
for(var j=0,row;(row=_33[j]);j++){
for(var i=0,_34;(_34=row[i]);i++){
_34.get=_34.get||(_34.value==undefined)&&_32;
_34.markup=this.generateCellMarkup(_34,_34.cellStyles,_34.cellClasses,false);
if(!this.grid.editable&&_34.editable){
this.grid.editable=true;
}
}
}
},generateHtml:function(_35,_36){
var _37=this.getTableArray(),v=this.view,_38=v.structure.cells,_39=this.grid.getItem(_36);
_a.fire(this.view,"onBeforeRow",[_36,_38]);
for(var j=0,row;(row=_38[j]);j++){
if(row.hidden||row.header){
continue;
}
_37.push(!row.invisible?"<tr>":"<tr class=\"dojoxGridInvisible\">");
for(var i=0,_3a,m,cc,cs;(_3a=row[i]);i++){
m=_3a.markup;
cc=_3a.customClasses=[];
cs=_3a.customStyles=[];
m[5]=_3a.format(_36,_39);
if(_6("ie")<8&&(m[5]===null||m[5]===""||/^\s+$/.test(m[5]))){
m[5]="&nbsp;";
}
m[1]=cc.join(" ");
m[3]=cs.join(";");
_37.push.apply(_37,m);
}
_37.push("</tr>");
}
_37.push("</table>");
return _37.join("");
},decorateEvent:function(e){
e.rowNode=this.findRowTarget(e.target);
if(!e.rowNode){
return false;
}
e.rowIndex=e.rowNode[_1a];
this.baseDecorateEvent(e);
e.cell=this.grid.getCell(e.cellIndex);
return true;
}});
var _3b=dg._HeaderBuilder=_3.extend(function(_3c){
this.moveable=null;
_1c.call(this,_3c);
},_1c.prototype,{_skipBogusClicks:false,overResizeWidth:4,minColWidth:1,update:function(){
if(this.tableMap){
this.tableMap.mapRows(this.view.structure.cells);
}else{
this.tableMap=new dg._TableMap(this.view.structure.cells);
}
},generateHtml:function(_3d,_3e){
var _3f=this.getTableArray(),_40=this.view.structure.cells;
_a.fire(this.view,"onBeforeRow",[-1,_40]);
for(var j=0,row;(row=_40[j]);j++){
if(row.hidden){
continue;
}
_3f.push(!row.invisible?"<tr>":"<tr class=\"dojoxGridInvisible\">");
for(var i=0,_41,_42;(_41=row[i]);i++){
_41.customClasses=[];
_41.customStyles=[];
if(this.view.simpleStructure){
if(_41.draggable){
if(_41.headerClasses){
if(_41.headerClasses.indexOf("dojoDndItem")==-1){
_41.headerClasses+=" dojoDndItem";
}
}else{
_41.headerClasses="dojoDndItem";
}
}
if(_41.attrs){
if(_41.attrs.indexOf("dndType='gridColumn_")==-1){
_41.attrs+=" dndType='gridColumn_"+this.grid.id+"'";
}
}else{
_41.attrs="dndType='gridColumn_"+this.grid.id+"'";
}
}
_42=this.generateCellMarkup(_41,_41.headerStyles,_41.headerClasses,true);
_42[5]=(_3e!=undefined?_3e:_3d(_41));
_42[3]=_41.customStyles.join(";");
_42[1]=_41.customClasses.join(" ");
_3f.push(_42.join(""));
}
_3f.push("</tr>");
}
_3f.push("</table>");
return _3f.join("");
},getCellX:function(e){
var n,x=e.layerX;
if(_6("mozilla")||_6("ie")>=9){
n=_13(e.target,_16("th"));
x-=(n&&n.offsetLeft)||0;
var t=e.sourceView.getScrollbarWidth();
if(!this.grid.isLeftToRight()){
table=_13(n,_16("table"));
x-=(table&&table.offsetLeft)||0;
}
}
n=_13(e.target,function(){
if(!n||n==e.cellNode){
return false;
}
x+=(n.offsetLeft<0?0:n.offsetLeft);
return true;
});
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
var _43=this.grid.getCell(e.cellIndex);
return !_43.noresize&&_43.canResize();
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
},colResizeSetup:function(e,_44){
var _45=_b.contentBox(e.sourceView.headerNode);
if(_44){
this.lineDiv=document.createElement("div");
var vw=_b.position(e.sourceView.headerNode,true);
var _46=_b.contentBox(e.sourceView.domNode);
var l=e.pageX;
if(!this.grid.isLeftToRight()&&_6("ie")<8){
l-=_9.getScrollbar().w;
}
_b.style(this.lineDiv,{top:vw.y+"px",left:l+"px",height:(_46.h+_45.h)+"px"});
_b.addClass(this.lineDiv,"dojoxGridResizeColLine");
this.lineDiv._origLeft=l;
_4.body().appendChild(this.lineDiv);
}
var _47=[],_48=this.tableMap.findOverlappingNodes(e.cellNode);
for(var i=0,_49;(_49=_48[i]);i++){
_47.push({node:_49,index:this.getCellNodeIndex(_49),width:_49.offsetWidth});
}
var _4a=e.sourceView;
var adj=this.grid.isLeftToRight()?1:-1;
var _4b=e.grid.views.views;
var _4c=[];
for(var j=_4a.idx+adj,_4d;(_4d=_4b[j]);j=j+adj){
_4c.push({node:_4d.headerNode,left:window.parseInt(_4d.headerNode.style.left)});
}
var _4e=_4a.headerContentNode.firstChild;
var _4f={scrollLeft:e.sourceView.headerNode.scrollLeft,view:_4a,node:e.cellNode,index:e.cellIndex,w:_b.contentBox(e.cellNode).w,vw:_45.w,table:_4e,tw:_b.contentBox(_4e).w,spanners:_47,followers:_4c};
return _4f;
},beginColumnResize:function(e){
this.moverDiv=document.createElement("div");
_b.style(this.moverDiv,{position:"absolute",left:0});
_4.body().appendChild(this.moverDiv);
_b.addClass(this.grid.domNode,"dojoxGridColumnResizing");
var m=(this.moveable=new _8(this.moverDiv));
var _50=this.colResizeSetup(e,true);
m.onMove=_3.hitch(this,"doResizeColumn",_50);
_7.connect(m,"onMoveStop",_3.hitch(this,function(){
this.endResizeColumn(_50);
if(_50.node.releaseCapture){
_50.node.releaseCapture();
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
},doResizeColumn:function(_51,_52,_53){
var _54=_53.l;
var _55={deltaX:_54,w:_51.w+(this.grid.isLeftToRight()?_54:-_54),vw:_51.vw+_54,tw:_51.tw+_54};
this.dragRecord={inDrag:_51,mover:_52,leftTop:_53};
if(_55.w>=this.minColWidth){
if(!_52){
this.doResizeNow(_51,_55);
}else{
_b.style(this.lineDiv,"left",(this.lineDiv._origLeft+_55.deltaX)+"px");
}
}
},endResizeColumn:function(_56){
if(this.dragRecord){
var _57=this.dragRecord.leftTop;
var _58=this.grid.isLeftToRight()?_57.l:-_57.l;
_58+=Math.max(_56.w+_58,this.minColWidth)-(_56.w+_58);
if(_6("webkit")&&_56.spanners.length){
_58+=_b._getPadBorderExtents(_56.spanners[0].node).w;
}
var _59={deltaX:_58,w:_56.w+_58,vw:_56.vw+_58,tw:_56.tw+_58};
this.doResizeNow(_56,_59);
delete this.dragRecord;
}
_b.destroy(this.lineDiv);
_b.destroy(this.moverDiv);
_b.destroy(this.moverDiv);
delete this.moverDiv;
this._skipBogusClicks=true;
_56.view.update();
this._skipBogusClicks=false;
this.grid.onResizeColumn(_56.index);
},doResizeNow:function(_5a,_5b){
_5a.view.convertColPctToFixed();
if(_5a.view.flexCells&&!_5a.view.testFlexCells()){
var t=_11(_5a.node);
if(t){
(t.style.width="");
}
}
var i,s,sw,f,fl;
for(i=0;(s=_5a.spanners[i]);i++){
sw=s.width+_5b.deltaX;
if(sw>0){
s.node.style.width=sw+"px";
_5a.view.setColWidth(s.index,sw);
}
}
if(this.grid.isLeftToRight()||!_6("ie")){
for(i=0;(f=_5a.followers[i]);i++){
fl=f.left+_5b.deltaX;
f.node.style.left=fl+"px";
}
}
_5a.node.style.width=_5b.w+"px";
_5a.view.setColWidth(_5a.index,_5b.w);
_5a.view.headerNode.style.width=_5b.vw+"px";
_5a.view.setColumnsWidth(_5b.tw);
if(!this.grid.isLeftToRight()){
_5a.view.headerNode.scrollLeft=_5a.scrollLeft+_5b.deltaX;
}
}});
dg._TableMap=_3.extend(function(_5c){
this.mapRows(_5c);
},{map:null,mapRows:function(_5d){
var _5e=_5d.length;
if(!_5e){
return;
}
this.map=[];
var row;
for(var k=0;(row=_5d[k]);k++){
this.map[k]=[];
}
for(var j=0;(row=_5d[j]);j++){
for(var i=0,x=0,_5f,_60,_61;(_5f=row[i]);i++){
while(this.map[j][x]){
x++;
}
this.map[j][x]={c:i,r:j};
_61=_5f.rowSpan||1;
_60=_5f.colSpan||1;
for(var y=0;y<_61;y++){
for(var s=0;s<_60;s++){
this.map[j+y][x+s]=this.map[j][x];
}
}
x+=_60;
}
}
},dumpMap:function(){
for(var j=0,row,h="";(row=this.map[j]);j++,h=""){
for(var i=0,_62;(_62=row[i]);i++){
h+=_62.r+","+_62.c+"   ";
}
}
},getMapCoords:function(_63,_64){
for(var j=0,row;(row=this.map[j]);j++){
for(var i=0,_65;(_65=row[i]);i++){
if(_65.c==_64&&_65.r==_63){
return {j:j,i:i};
}
}
}
return {j:-1,i:-1};
},getNode:function(_66,_67,_68){
var row=_66&&_66.rows[_67];
return row&&row.cells[_68];
},_findOverlappingNodes:function(_69,_6a,_6b){
var _6c=[];
var m=this.getMapCoords(_6a,_6b);
for(var j=0,row;(row=this.map[j]);j++){
if(j==m.j){
continue;
}
var rw=row[m.i];
var n=(rw?this.getNode(_69,rw.r,rw.c):null);
if(n){
_6c.push(n);
}
}
return _6c;
},findOverlappingNodes:function(_6d){
return this._findOverlappingNodes(_11(_6d),_d(_6d.parentNode),_c(_6d));
}});
return {_Builder:_1c,_HeaderBuilder:_3b,_ContentBuilder:_30};
});
