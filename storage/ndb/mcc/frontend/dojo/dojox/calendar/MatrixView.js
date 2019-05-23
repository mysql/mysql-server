//>>built
require({cache:{"url:dojox/calendar/templates/MatrixView.html":"<div data-dojo-attach-events=\"keydown:_onKeyDown\">\n\t<div  class=\"dojoxCalendarYearColumnHeader\" data-dojo-attach-point=\"yearColumnHeader\">\n\t\t<table><tr><td><span data-dojo-attach-point=\"yearColumnHeaderContent\"></span></td></tr></table>\t\t\n\t</div>\t\n\t<div data-dojo-attach-point=\"columnHeader\" class=\"dojoxCalendarColumnHeader\">\n\t\t<table data-dojo-attach-point=\"columnHeaderTable\" class=\"dojoxCalendarColumnHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t</div>\t\t\n\t<div dojoAttachPoint=\"rowHeader\" class=\"dojoxCalendarRowHeader\">\n\t\t<table data-dojo-attach-point=\"rowHeaderTable\" class=\"dojoxCalendarRowHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t</div>\t\n\t<div dojoAttachPoint=\"grid\" class=\"dojoxCalendarGrid\">\n\t\t<table data-dojo-attach-point=\"gridTable\" class=\"dojoxCalendarGridTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t</div>\t\n\t<div data-dojo-attach-point=\"itemContainer\" class=\"dojoxCalendarContainer\" data-dojo-attach-event=\"mousedown:_onGridMouseDown,mouseup:_onGridMouseUp,ondblclick:_onGridDoubleClick,touchstart:_onGridTouchStart,touchmove:_onGridTouchMove,touchend:_onGridTouchEnd\">\n\t\t<table data-dojo-attach-point=\"itemContainerTable\" class=\"dojoxCalendarContainerTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t</div>\t\n</div>\n"}});
define("dojox/calendar/MatrixView",["dojo/_base/declare","dojo/_base/array","dojo/_base/event","dojo/_base/lang","dojo/_base/sniff","dojo/_base/fx","dojo/_base/html","dojo/on","dojo/dom","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/dom-construct","dojo/query","dojox/html/metrics","dojo/i18n","./ViewBase","dojo/text!./templates/MatrixView.html","dijit/_TemplatedMixin"],function(_1,_2,_3,_4,_5,fx,_6,on,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11){
return _1("dojox.calendar.MatrixView",[_f,_11],{templateString:_10,baseClass:"dojoxCalendarMatrixView",_setTabIndexAttr:"domNode",viewKind:"matrix",renderData:null,startDate:null,refStartTime:null,refEndTime:null,columnCount:7,rowCount:5,horizontalRenderer:null,labelRenderer:null,expandRenderer:null,percentOverlap:0,verticalGap:2,horizontalRendererHeight:17,labelRendererHeight:14,expandRendererHeight:15,cellPaddingTop:16,expandDuration:300,expandEasing:null,layoutDuringResize:false,roundToDay:true,showCellLabel:true,scrollable:false,resizeCursor:"e-resize",constructor:function(){
this.invalidatingProperties=["columnCount","rowCount","startDate","horizontalRenderer","labelRenderer","expandRenderer","rowHeaderDatePattern","columnHeaderLabelLength","cellHeaderShortPattern","cellHeaderLongPattern","percentOverlap","verticalGap","horizontalRendererHeight","labelRendererHeight","expandRendererHeight","cellPaddingTop","roundToDay","itemToRendererKindFunc","layoutPriorityFunction","formatItemTimeFunc","textDir","items"];
this._ddRendererList=[];
this._ddRendererPool=[];
this._rowHeaderHandles=[];
this._viewHandles.push(on(window,"resize",_4.hitch(this,this._resizeHandler)));
},destroy:function(_12){
this._cleanupRowHeader();
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
this._initialized=true;
if(!this.invalidRendering){
this.refreshRendering();
}
},_createRenderData:function(){
var rd={};
rd.dateLocaleModule=this.dateLocaleModule;
rd.dateClassObj=this.dateClassObj;
rd.dateModule=this.dateModule;
rd.dates=[];
rd.columnCount=this.get("columnCount");
rd.rowCount=this.get("rowCount");
rd.sheetHeight=this.itemContainer.offsetHeight;
this._computeRowsHeight(rd);
var d=this.get("startDate");
if(d==null){
d=new rd.dateClassObj();
}
d=this.floorToDay(d,false,rd);
this.startDate=d;
for(var row=0;row<rd.rowCount;row++){
rd.dates.push([]);
for(var col=0;col<rd.columnCount;col++){
rd.dates[row].push(d);
d=rd.dateModule.add(d,"day",1);
d=this.floorToDay(d,false,rd);
}
}
rd.startTime=this.newDate(rd.dates[0][0],rd);
rd.endTime=this.newDate(rd.dates[rd.rowCount-1][rd.columnCount-1],rd);
rd.endTime=rd.dateModule.add(rd.endTime,"day",1);
rd.endTime=this.floorToDay(rd.endTime,true);
if(this.displayedItemsInvalidated){
this.displayedItemsInvalidated=false;
this._computeVisibleItems(rd);
if(this._isEditing){
this._endItemEditing(null,false);
}
}else{
if(this.renderData){
rd.items=this.renderData.items;
}
}
rd.rtl=!this.isLeftToRight();
return rd;
},_validateProperties:function(){
this.inherited(arguments);
if(this.columnCount<1||isNaN(this.columnCount)){
this.columnCount=1;
}
if(this.rowCount<1||isNaN(this.rowCount)){
this.rowCount=1;
}
if(isNaN(this.percentOverlap)||this.percentOverlap<0||this.percentOverlap>100){
this.percentOverlap=0;
}
if(isNaN(this.verticalGap)||this.verticalGap<0){
this.verticalGap=2;
}
if(isNaN(this.horizontalRendererHeight)||this.horizontalRendererHeight<1){
this.horizontalRendererHeight=17;
}
if(isNaN(this.labelRendererHeight)||this.labelRendererHeight<1){
this.labelRendererHeight=14;
}
if(isNaN(this.expandRendererHeight)||this.expandRendererHeight<1){
this.expandRendererHeight=15;
}
},_setStartDateAttr:function(_13){
this.displayedItemsInvalidated=true;
this._set("startDate",_13);
},_setColumnCountAttr:function(_14){
this.displayedItemsInvalidated=true;
this._set("columnCount",_14);
},_setRowCountAttr:function(_15){
this.displayedItemsInvalidated=true;
this._set("rowCount",_15);
},__fixEvt:function(e){
e.sheet="primary";
e.source=this;
return e;
},_formatRowHeaderLabel:function(d){
if(this.rowHeaderDatePattern){
return this.renderData.dateLocaleModule.format(d,{selector:"date",datePattern:this.rowHeaderDatePattern});
}else{
return this.getWeekNumberLabel(d);
}
},_formatColumnHeaderLabel:function(d){
return this.renderData.dateLocaleModule.getNames("days",this.columnHeaderLabelLength?this.columnHeaderLabelLength:"wide","standAlone")[d.getDay()];
},_formatGridCellLabel:function(d,row,col){
var _16=row==0&&col==0||d.getDate()==1;
var _17,rb;
if(_16){
if(this.cellHeaderLongPattern){
_17=this.cellHeaderLongPattern;
}else{
rb=_e.getLocalization("dojo.cldr",this._calendar);
_17=rb["dateFormatItem-MMMd"];
}
}else{
if(this.cellHeaderShortPattern){
_17=this.cellHeaderShortPattern;
}else{
rb=_e.getLocalization("dojo.cldr",this._calendar);
_17=rb["dateFormatItem-d"];
}
}
return this.renderData.dateLocaleModule.format(d,{selector:"date",datePattern:_17});
},refreshRendering:function(){
this.inherited(arguments);
if(!this.domNode){
return;
}
this._validateProperties();
var _18=this.renderData;
this.renderData=this._createRenderData();
this._createRendering(this.renderData,_18);
this._layoutRenderers(this.renderData);
},_createRendering:function(_19,_1a){
if(_19.rowHeight<=0){
_19.columnCount=0;
_19.rowCount=0;
return;
}
this._buildColumnHeader(_19,_1a);
this._buildRowHeader(_19,_1a);
this._buildGrid(_19,_1a);
this._buildItemContainer(_19,_1a);
if(this.buttonContainer&&this.owner!=null&&this.owner.currentView==this){
_9.set(this.buttonContainer,{"right":0,"left":0});
}
},_buildColumnHeader:function(_1b,_1c){
var _1d=this.columnHeaderTable;
if(!_1d){
return;
}
var _1e=_1b.columnCount-(_1c?_1c.columnCount:0);
if(_5("ie")==8){
if(this._colTableSave==null){
this._colTableSave=_4.clone(_1d);
}else{
if(_1e<0){
this.columnHeader.removeChild(_1d);
_b.destroy(_1d);
_1d=_4.clone(this._colTableSave);
this.columnHeaderTable=_1d;
this.columnHeader.appendChild(_1d);
_1e=_1b.columnCount;
}
}
}
var _1f=_c("tbody",_1d);
var trs=_c("tr",_1d);
var _20,tr,td;
if(_1f.length==1){
_20=_1f[0];
}else{
_20=_6.create("tbody",null,_1d);
}
if(trs.length==1){
tr=trs[0];
}else{
tr=_b.create("tr",null,_20);
}
if(_1e>0){
for(var i=0;i<_1e;i++){
td=_b.create("td",null,tr);
}
}else{
_1e=-_1e;
for(var i=0;i<_1e;i++){
tr.removeChild(tr.lastChild);
}
}
_c("td",_1d).forEach(function(td,i){
td.className="";
var d=_1b.dates[0][i];
this._setText(td,this._formatColumnHeaderLabel(d));
if(i==0){
_8.add(td,"first-child");
}else{
if(i==this.renderData.columnCount-1){
_8.add(td,"last-child");
}
}
this.styleColumnHeaderCell(td,d,_1b);
},this);
if(this.yearColumnHeaderContent){
var d=_1b.dates[0][0];
this._setText(this.yearColumnHeaderContent,_1b.dateLocaleModule.format(d,{selector:"date",datePattern:"yyyy"}));
}
},styleColumnHeaderCell:function(_21,_22,_23){
if(this.isWeekEnd(_22)){
return _8.add(_21,"dojoxCalendarWeekend");
}
},_rowHeaderHandles:null,_cleanupRowHeader:function(){
while(this._rowHeaderHandles.length>0){
var _24=this._rowHeaderHandles.pop();
while(_24.length>0){
_24.pop().remove();
}
}
},_rowHeaderClick:function(e){
var _25=_c("td",this.rowHeaderTable).indexOf(e.currentTarget);
this._onRowHeaderClick({index:_25,date:this.renderData.dates[_25][0],triggerEvent:e});
},_buildRowHeader:function(_26,_27){
var _28=this.rowHeaderTable;
if(!_28){
return;
}
var _29=_c("tbody",_28);
var _2a,tr,td;
if(_29.length==1){
_2a=_29[0];
}else{
_2a=_b.create("tbody",null,_28);
}
var _2b=_26.rowCount-(_27?_27.rowCount:0);
if(_2b>0){
for(var i=0;i<_2b;i++){
tr=_b.create("tr",null,_2a);
td=_b.create("td",null,tr);
var h=[];
h.push(on(td,"click",_4.hitch(this,this._rowHeaderClick)));
if(!_5("touch")){
h.push(on(td,"mousedown",function(e){
_8.add(e.currentTarget,"Active");
}));
h.push(on(td,"mouseup",function(e){
_8.remove(e.currentTarget,"Active");
}));
h.push(on(td,"mouseover",function(e){
_8.add(e.currentTarget,"Hover");
}));
h.push(on(td,"mouseout",function(e){
_8.remove(e.currentTarget,"Hover");
}));
}
this._rowHeaderHandles.push(h);
}
}else{
_2b=-_2b;
for(var i=0;i<_2b;i++){
_2a.removeChild(_2a.lastChild);
var _2c=this._rowHeaderHandles.pop();
while(_2c.length>0){
_2c.pop().remove();
}
}
}
_c("tr",_28).forEach(function(tr,i){
_9.set(tr,"height",this._getRowHeight(i)+"px");
var d=_26.dates[i][0];
var td=_c("td",tr)[0];
td.className="";
if(i==0){
_8.add(td,"first-child");
}
if(i==this.renderData.rowCount-1){
_8.add(td,"last-child");
}
this.styleRowHeaderCell(td,d,_26);
this._setText(td,this._formatRowHeaderLabel(d));
},this);
},styleRowHeaderCell:function(_2d,_2e,_2f){
},_buildGrid:function(_30,_31){
var _32=this.gridTable;
if(!_32){
return;
}
var _33=_30.rowCount-(_31?_31.rowCount:0);
var _34=_33>0;
var _35=_30.columnCount-(_31?_31.columnCount:0);
if(_5("ie")==8){
if(this._gridTableSave==null){
this._gridTableSave=_4.clone(_32);
}else{
if(_35<0){
this.grid.removeChild(_32);
_b.destroy(_32);
_32=_4.clone(this._gridTableSave);
this.gridTable=_32;
this.grid.appendChild(_32);
_35=_30.columnCount;
_33=_30.rowCount;
_34=true;
}
}
}
var _36=_c("tbody",_32);
var _37;
if(_36.length==1){
_37=_36[0];
}else{
_37=_b.create("tbody",null,_32);
}
if(_34){
for(var i=0;i<_33;i++){
_b.create("tr",null,_37);
}
}else{
_33=-_33;
for(var i=0;i<_33;i++){
_37.removeChild(_37.lastChild);
}
}
var _38=_30.rowCount-_33;
var _39=_34||_35>0;
_35=_39?_35:-_35;
_c("tr",_32).forEach(function(tr,i){
if(_39){
var len=i>=_38?_30.columnCount:_35;
for(var i=0;i<len;i++){
var td=_b.create("td",null,tr);
_b.create("span",null,td);
}
}else{
for(var i=0;i<_35;i++){
tr.removeChild(tr.lastChild);
}
}
});
_c("tr",_32).forEach(function(tr,row){
_9.set(tr,"height",this._getRowHeight(row)+"px");
tr.className="";
if(row==0){
_8.add(tr,"first-child");
}
if(row==_30.rowCount-1){
_8.add(tr,"last-child");
}
_c("td",tr).forEach(function(td,col){
td.className="";
if(col==0){
_8.add(td,"first-child");
}
if(col==_30.columnCount-1){
_8.add(td,"last-child");
}
var d=_30.dates[row][col];
var _3a=_c("span",td)[0];
this._setText(_3a,this.showCellLabel?this._formatGridCellLabel(d,row,col):null);
this.styleGridCell(td,d,_30);
},this);
},this);
},styleGridCell:function(_3b,_3c,_3d){
var cal=_3d.dateModule;
if(this.isToday(_3c)){
_8.add(_3b,"dojoxCalendarToday");
}else{
if(this.refStartTime!=null&&this.refEndTime!=null&&(cal.compare(_3c,this.refEndTime)>=0||cal.compare(cal.add(_3c,"day",1),this.refStartTime)<=0)){
_8.add(_3b,"dojoxCalendarDayDisabled");
}else{
if(this.isWeekEnd(_3c)){
_8.add(_3b,"dojoxCalendarWeekend");
}
}
}
},_buildItemContainer:function(_3e,_3f){
var _40=this.itemContainerTable;
if(!_40){
return;
}
var _41=[];
var _42=_3e.rowCount-(_3f?_3f.rowCount:0);
if(_5("ie")==8){
if(this._itemTableSave==null){
this._itemTableSave=_4.clone(_40);
}else{
if(_42<0){
this.itemContainer.removeChild(_40);
this._recycleItemRenderers(true);
this._recycleExpandRenderers(true);
_b.destroy(_40);
_40=_4.clone(this._itemTableSave);
this.itemContainerTable=_40;
this.itemContainer.appendChild(_40);
_42=_3e.columnCount;
}
}
}
var _43=_c("tbody",_40);
var _44,tr,td,div;
if(_43.length==1){
_44=_43[0];
}else{
_44=_b.create("tbody",null,_40);
}
if(_42>0){
for(var i=0;i<_42;i++){
tr=_b.create("tr",null,_44);
_8.add(tr,"dojoxCalendarItemContainerRow");
td=_b.create("td",null,tr);
div=_b.create("div",null,td);
_8.add(div,"dojoxCalendarContainerRow");
}
}else{
_42=-_42;
for(var i=0;i<_42;i++){
_44.removeChild(_44.lastChild);
}
}
_c(".dojoxCalendarItemContainerRow",_40).forEach(function(tr,i){
_9.set(tr,"height",this._getRowHeight(i)+"px");
_41.push(tr.childNodes[0].childNodes[0]);
},this);
_3e.cells=_41;
},_resizeHandler:function(e,_45){
var rd=this.renderData;
if(rd==null){
this.refreshRendering();
return;
}
if(rd.sheetHeight!=this.itemContainer.offsetHeight){
rd.sheetHeight=this.itemContainer.offsetHeight;
var _46=this.getExpandedRowIndex();
if(_46==-1){
this._computeRowsHeight();
this._resizeRows();
}else{
this.expandRow(rd.expandedRow,rd.expandedRowCol,0,null,true);
}
}
if(this.layoutDuringResize||_45){
setTimeout(_4.hitch(this,function(){
this._layoutRenderers(this.renderData);
}),20);
}else{
_9.set(this.itemContainer,"opacity",0);
this._recycleItemRenderers();
this._recycleExpandRenderers();
if(this._resizeTimer!=undefined){
clearTimeout(this._resizeTimer);
}
this._resizeTimer=setTimeout(_4.hitch(this,function(){
delete this._resizeTimer;
this._resizeRowsImpl(this.itemContainer,"tr");
this._layoutRenderers(this.renderData);
if(this.resizeAnimationDuration==0){
_9.set(this.itemContainer,"opacity",1);
}else{
fx.fadeIn({node:this.itemContainer,curve:[0,1]}).play(this.resizeAnimationDuration);
}
}),200);
}
},resizeAnimationDuration:0,getExpandedRowIndex:function(){
return this.renderData.expandedRow==null?-1:this.renderData.expandedRow;
},collapseRow:function(_47,_48,_49){
var rd=this.renderData;
if(_49==undefined){
_49=true;
}
if(_47==undefined){
_47=this.expandDuration;
}
if(rd&&rd.expandedRow!=null&&rd.expandedRow!=-1){
if(_49&&_47){
var _4a=rd.expandedRow;
var _4b=rd.expandedRowHeight;
delete rd.expandedRow;
this._computeRowsHeight(rd);
var _4c=this._getRowHeight(_4a);
rd.expandedRow=_4a;
this._recycleExpandRenderers();
this._recycleItemRenderers();
_9.set(this.itemContainer,"display","none");
this._expandAnimation=new fx.Animation({curve:[_4b,_4c],duration:_47,easing:_48,onAnimate:_4.hitch(this,function(_4d){
this._expandRowImpl(Math.floor(_4d));
}),onEnd:_4.hitch(this,function(_4e){
this._expandAnimation=null;
this._collapseRowImpl(false);
this._resizeRows();
_9.set(this.itemContainer,"display","block");
setTimeout(_4.hitch(this,function(){
this._layoutRenderers(rd);
}),100);
this.onExpandAnimationEnd(false);
})});
this._expandAnimation.play();
}else{
this._collapseRowImpl(_49);
}
}
},_collapseRowImpl:function(_4f){
var rd=this.renderData;
delete rd.expandedRow;
delete rd.expandedRowHeight;
this._computeRowsHeight(rd);
if(_4f==undefined||_4f){
this._resizeRows();
this._layoutRenderers(rd);
}
},expandRow:function(_50,_51,_52,_53,_54){
var rd=this.renderData;
if(!rd||_50<0||_50>=rd.rowCount){
return -1;
}
if(_51==undefined||_51<0||_51>=rd.columnCount){
_51=-1;
}
if(_54==undefined){
_54=true;
}
if(_52==undefined){
_52=this.expandDuration;
}
if(_53==undefined){
_53=this.expandEasing;
}
var _55=this._getRowHeight(_50);
var _56=rd.sheetHeight-Math.ceil(this.cellPaddingTop*(rd.rowCount-1));
rd.expandedRow=_50;
rd.expandedRowCol=_51;
rd.expandedRowHeight=_56;
if(_54){
if(_52){
this._recycleExpandRenderers();
this._recycleItemRenderers();
_9.set(this.itemContainer,"display","none");
this._expandAnimation=new fx.Animation({curve:[_55,_56],duration:_52,delay:50,easing:_53,onAnimate:_4.hitch(this,function(_57){
this._expandRowImpl(Math.floor(_57));
}),onEnd:_4.hitch(this,function(){
this._expandAnimation=null;
_9.set(this.itemContainer,"display","block");
setTimeout(_4.hitch(this,function(){
this._expandRowImpl(_56,true);
}),100);
this.onExpandAnimationEnd(true);
})});
this._expandAnimation.play();
}else{
this._expandRowImpl(_56);
}
}
},_expandRowImpl:function(_58,_59){
var rd=this.renderData;
rd.expandedRowHeight=_58;
this._computeRowsHeight(rd,rd.sheetHeight-_58);
this._resizeRows();
if(_59){
this._layoutRenderers(rd);
}
},onExpandAnimationEnd:function(_5a){
},_resizeRows:function(){
if(this._getRowHeight(0)<=0){
return;
}
if(this.rowHeaderTable){
this._resizeRowsImpl(this.rowHeaderTable,"tr");
}
if(this.gridTable){
this._resizeRowsImpl(this.gridTable,"tr");
}
if(this.itemContainerTable){
this._resizeRowsImpl(this.itemContainerTable,"tr");
}
},_computeRowsHeight:function(_5b,max){
var rd=_5b==null?this.renderData:_5b;
max=max||rd.sheetHeight;
max--;
if(_5("ie")==7){
max-=rd.rowCount;
}
if(rd.rowCount==1){
rd.rowHeight=max;
rd.rowHeightFirst=max;
rd.rowHeightLast=max;
return;
}
var _5c=rd.expandedRow==null?rd.rowCount:rd.rowCount-1;
var rhx=max/_5c;
var rhf,rhl,rh;
var _5d=max-(Math.floor(rhx)*_5c);
var _5e=Math.abs(max-(Math.ceil(rhx)*_5c));
var _5f;
var _60=1;
if(_5d<_5e){
rh=Math.floor(rhx);
_5f=_5d;
}else{
_60=-1;
rh=Math.ceil(rhx);
_5f=_5e;
}
rhf=rh+_60*Math.floor(_5f/2);
rhl=rhf+_60*(_5f%2);
rd.rowHeight=rh;
rd.rowHeightFirst=rhf;
rd.rowHeightLast=rhl;
},_getRowHeight:function(_61){
var rd=this.renderData;
if(_61==rd.expandedRow){
return rd.expandedRowHeight;
}else{
if(rd.expandedRow==0&&_61==1||_61==0){
return rd.rowHeightFirst;
}else{
if(rd.expandedRow==this.renderData.rowCount-1&&_61==this.renderData.rowCount-2||_61==this.renderData.rowCount-1){
return rd.rowHeightLast;
}else{
return rd.rowHeight;
}
}
}
},_resizeRowsImpl:function(_62,_63){
var rd=this.renderData;
dojo.query(_63,_62).forEach(function(tr,i){
_9.set(tr,"height",this._getRowHeight(i)+"px");
},this);
},_setHorizontalRendererAttr:function(_64){
this._destroyRenderersByKind("horizontal");
this._set("horizontalRenderer",_64);
},_setLabelRendererAttr:function(_65){
this._destroyRenderersByKind("label");
this._set("labelRenderer",_65);
},_destroyExpandRenderer:function(_66){
_2.forEach(_66.__handles,function(_67){
_67.remove();
});
if(_66["destroy"]){
_66.destroy();
}
_6.destroy(_66.domNode);
},_setExpandRendererAttr:function(_68){
while(this._ddRendererList.length>0){
this._destroyExpandRenderer(this._ddRendererList.pop());
}
var _69=this._ddRendererPool;
if(_69){
while(_69.length>0){
this._destroyExpandRenderer(_69.pop());
}
}
this._set("expandRenderer",_68);
},_ddRendererList:null,_ddRendererPool:null,_getExpandRenderer:function(_6a,_6b,_6c,_6d,_6e){
if(this.expandRenderer==null){
return null;
}
var ir=this._ddRendererPool.pop();
if(ir==null){
ir=new this.expandRenderer();
}
this._ddRendererList.push(ir);
ir.set("owner",this);
ir.set("date",_6a);
ir.set("items",_6b);
ir.set("rowIndex",_6c);
ir.set("columnIndex",_6d);
ir.set("expanded",_6e);
return ir;
},_recycleExpandRenderers:function(_6f){
for(var i=0;i<this._ddRendererList.length;i++){
var ir=this._ddRendererList[i];
ir.set("Up",false);
ir.set("Down",false);
if(_6f){
ir.domNode.parentNode.removeChild(ir.domNode);
}
_9.set(ir.domNode,"display","none");
}
this._ddRendererPool=this._ddRendererPool.concat(this._ddRendererList);
this._ddRendererList=[];
},_defaultItemToRendererKindFunc:function(_70){
var dur=Math.abs(this.renderData.dateModule.difference(_70.startTime,_70.endTime,"minute"));
return dur>=1440?"horizontal":"label";
},naturalRowsHeight:null,_roundItemToDay:function(_71){
var s=_71.startTime,e=_71.endTime;
if(!this.isStartOfDay(s)){
s=this.floorToDay(s,false,this.renderData);
}
if(!this.isStartOfDay(e)){
e=this.renderData.dateModule.add(e,"day",1);
e=this.floorToDay(e,true);
}
return {startTime:s,endTime:e};
},_sortItemsFunction:function(a,b){
if(this.roundToDay){
a=this._roundItemToDay(a);
b=this._roundItemToDay(b);
}
var res=this.dateModule.compare(a.startTime,b.startTime);
if(res==0){
res=-1*this.dateModule.compare(a.endTime,b.endTime);
}
return res;
},_overlapLayoutPass3:function(_72){
var pos=0,_73=0;
var res=[];
var _74=_a.position(this.gridTable).x;
for(var col=0;col<this.renderData.columnCount;col++){
var _75=false;
var _76=_a.position(this._getCellAt(0,col));
pos=_76.x-_74;
_73=pos+_76.w;
for(var _77=_72.length-1;_77>=0&&!_75;_77--){
for(var i=0;i<_72[_77].length;i++){
var _78=_72[_77][i];
_75=_78.start<_73&&pos<_78.end;
if(_75){
res[col]=_77+1;
break;
}
}
}
if(!_75){
res[col]=0;
}
}
return res;
},applyRendererZIndex:function(_79,_7a,_7b,_7c,_7d,_7e){
_9.set(_7a.container,{"zIndex":_7d||_7c?_7a.renderer.mobile?100:0:_79.lane==undefined?1:_79.lane+1});
},_layoutRenderers:function(_7f){
if(_7f==null||_7f.items==null||_7f.rowHeight<=0){
return;
}
if(!this.gridTable||this._expandAnimation!=null||(this.horizontalRenderer==null&&this.labelRenderer==null)){
this._recycleItemRenderers();
return;
}
this.renderData.gridTablePosX=_a.position(this.gridTable).x;
this._layoutStep=_7f.columnCount;
this._recycleExpandRenderers();
this._hiddenItems=[];
this._offsets=[];
this.naturalRowsHeight=[];
this.inherited(arguments);
},_offsets:null,_layoutInterval:function(_80,_81,_82,end,_83){
if(this.renderData.cells==null){
return;
}
var _84=[];
var _85=[];
for(var i=0;i<_83.length;i++){
var _86=_83[i];
var _87=this._itemToRendererKind(_86);
if(_87=="horizontal"){
_84.push(_86);
}else{
if(_87=="label"){
_85.push(_86);
}
}
}
var _88=this.getExpandedRowIndex();
if(_88!=-1&&_88!=_81){
return;
}
var _89;
var _8a=[];
var _8b;
var _8c=[];
if(_84.length>0&&this.horizontalRenderer){
var _8b=this._createHorizontalLayoutItems(_81,_82,end,_84);
var _8d=this._computeHorizontalOverlapLayout(_8b,_8c);
}
var _8e;
var _8f=[];
if(_85.length>0&&this.labelRenderer){
_8e=this._createLabelLayoutItems(_81,_82,end,_85);
this._computeLabelOffsets(_8e,_8f);
}
var _90=this._computeColHasHiddenItems(_81,_8c,_8f);
if(_8b!=null){
this._layoutHorizontalItemsImpl(_81,_8b,_8d,_90,_8a);
}
if(_8e!=null){
this._layoutLabelItemsImpl(_81,_8e,_90,_8a,_8c);
}
this._layoutExpandRenderers(_81,_90,_8a);
this._hiddenItems[_81]=_8a;
},_createHorizontalLayoutItems:function(_91,_92,_93,_94){
if(this.horizontalRenderer==null){
return;
}
var rd=this.renderData;
var cal=rd.dateModule;
var _95=rd.cells[_91];
var _96=this.horizontalRendererHeight;
var _97=this.percentOverlap/100;
var _98=_a.getMarginBox(this.itemContainer).w;
var _99=rd.rtl?-1:1;
var _9a=[];
for(var i=0;i<_94.length;i++){
var _9b=_94[i];
var _9c=this.computeRangeOverlap(rd,_9b.startTime,_9b.endTime,_92,_93);
var _9d=cal.difference(_92,this.floorToDay(_9c[0],false,rd),"day");
var _9e=rd.dates[_91][_9d];
var _9f=_a.position(this._getCellAt(_91,_9d,false));
var _a0=_9f.x-rd.gridTablePosX;
if(rd.rtl){
_a0+=_9f.w;
}
if(!this.roundToDay&&!_9b.allDay){
_a0+=_99*this.computeProjectionOnDate(rd,_9e,_9c[0],_9f.w);
}
_a0=Math.ceil(_a0);
var _a1=cal.difference(_92,this.floorToDay(_9c[1],false,rd),"day");
var end;
if(_a1>rd.columnCount-1){
_9f=_a.position(this._getCellAt(_91,rd.columnCount-1,false));
if(rd.rtl){
end=_9f.x-rd.gridTablePosX;
}else{
end=_9f.x-rd.gridTablePosX+_9f.w;
}
}else{
_9e=rd.dates[_91][_a1];
_9f=_a.position(this._getCellAt(_91,_a1,false));
end=_9f.x-rd.gridTablePosX;
if(rd.rtl){
end+=_9f.w;
}
if(this.roundToDay){
if(!this.isStartOfDay(_9c[1])){
end+=_99*_9f.w;
}
}else{
end+=_99*this.computeProjectionOnDate(rd,_9e,_9c[1],_9f.w);
}
}
end=Math.floor(end);
if(rd.rtl){
var t=end;
end=_a0;
_a0=t;
}
if(end>_a0){
var _a2=_4.mixin({start:_a0,end:end,range:_9c,item:_9b,startOffset:_9d,endOffset:_a1},_9b);
_9a.push(_a2);
}
}
return _9a;
},_computeHorizontalOverlapLayout:function(_a3,_a4){
var rd=this.renderData;
var _a5=this.horizontalRendererHeight;
var _a6=this.computeOverlapping(_a3,this._overlapLayoutPass3);
var _a7=this.percentOverlap/100;
for(i=0;i<rd.columnCount;i++){
var _a8=_a6.addedPassRes[i];
var _a9=rd.rtl?rd.columnCount-i-1:i;
if(_a7==0){
_a4[_a9]=_a8==0?0:_a8==1?_a5:_a5+(_a8-1)*(_a5+this.verticalGap);
}else{
_a4[_a9]=_a8==0?0:_a8*_a5-(_a8-1)*(_a7*_a5)+this.verticalGap;
}
_a4[_a9]+=this.cellPaddingTop;
}
return _a6;
},_createLabelLayoutItems:function(_aa,_ab,_ac,_ad){
if(this.labelRenderer==null){
return;
}
var d;
var rd=this.renderData;
var cal=rd.dateModule;
var _ae=[];
for(var i=0;i<_ad.length;i++){
var _af=_ad[i];
d=this.floorToDay(_af.startTime,false,rd);
var _b0=this.dateModule.compare;
while(_b0(d,_af.endTime)==-1&&_b0(d,_ac)==-1){
var _b1=cal.add(d,"day",1);
_b1=this.floorToDay(_b1,true);
var _b2=this.computeRangeOverlap(rd,_af.startTime,_af.endTime,d,_b1);
var _b3=cal.difference(_ab,this.floorToDay(_b2[0],false,rd),"day");
if(_b3>=this.columnCount){
break;
}
if(_b3>=0){
var _b4=_ae[_b3];
if(_b4==null){
_b4=[];
_ae[_b3]=_b4;
}
_b4.push(_4.mixin({startOffset:_b3,range:_b2,item:_af},_af));
}
d=cal.add(d,"day",1);
this.floorToDay(d,true);
}
}
return _ae;
},_computeLabelOffsets:function(_b5,_b6){
for(var i=0;i<this.renderData.columnCount;i++){
_b6[i]=_b5[i]==null?0:_b5[i].length*(this.labelRendererHeight+this.verticalGap);
}
},_computeColHasHiddenItems:function(_b7,_b8,_b9){
var res=[];
var _ba=this._getRowHeight(_b7);
var h;
var _bb=0;
for(var i=0;i<this.renderData.columnCount;i++){
h=_b8==null||_b8[i]==null?this.cellPaddingTop:_b8[i];
h+=_b9==null||_b9[i]==null?0:_b9[i];
if(h>_bb){
_bb=h;
}
res[i]=h>_ba;
}
this.naturalRowsHeight[_b7]=_bb;
return res;
},_layoutHorizontalItemsImpl:function(_bc,_bd,_be,_bf,_c0){
var rd=this.renderData;
var cal=rd.dateModule;
var _c1=rd.cells[_bc];
var _c2=this._getRowHeight(_bc);
var _c3=this.horizontalRendererHeight;
var _c4=this.percentOverlap/100;
for(var i=0;i<_bd.length;i++){
var _c5=_bd[i];
var _c6=_c5.lane;
var _c7=this.cellPaddingTop;
if(_c4==0){
_c7+=_c6*(_c3+this.verticalGap);
}else{
_c7+=_c6*(_c3-_c4*_c3);
}
var exp=false;
var _c8=_c2;
if(this.expandRenderer){
for(var off=_c5.startOffset;off<=_c5.endOffset;off++){
if(_bf[off]){
exp=true;
break;
}
}
_c8=exp?_c2-this.expandRendererHeight:_c2;
}
if(_c7+_c3<=_c8){
var ir=this._createRenderer(_c5,"horizontal",this.horizontalRenderer,"dojoxCalendarHorizontal");
var _c9=this.isItemBeingEdited(_c5)&&!this.liveLayout&&this._isEditing;
var h=_c9?_c2-this.cellPaddingTop:_c3;
var w=_c5.end-_c5.start;
if(_5("ie")>=9&&_c5.start+w<this.itemContainer.offsetWidth){
w++;
}
_9.set(ir.container,{"top":(_c9?this.cellPaddingTop:_c7)+"px","left":_c5.start+"px","width":w+"px","height":h+"px"});
this._applyRendererLayout(_c5,ir,_c1,w,h,"horizontal");
}else{
for(var d=_c5.startOffset;d<_c5.endOffset;d++){
if(_c0[d]==null){
_c0[d]=[_c5.item];
}else{
_c0[d].push(_c5.item);
}
}
}
}
},_layoutLabelItemsImpl:function(_ca,_cb,_cc,_cd,_ce){
var d,_cf,_d0;
var rd=this.renderData;
var cal=rd.dateModule;
var _d1=rd.cells[_ca];
var _d2=this._getRowHeight(_ca);
var _d3=this.labelRendererHeight;
var _d4=_a.getMarginBox(this.itemContainer).w;
for(var i=0;i<_cb.length;i++){
_cf=_cb[i];
if(_cf!=null){
var _d5=this.expandRenderer?(_cc[i]?_d2-this.expandRendererHeight:_d2):_d2;
_d0=_ce==null||_ce[i]==null?this.cellPaddingTop:_ce[i]+this.verticalGap;
var _d6=_a.position(this._getCellAt(_ca,i));
var _d7=rd.dates[_ca][i];
var _d8=_d6.x-rd.gridTablePosX;
for(var j=0;j<_cf.length;j++){
if(_d0+_d3+this.verticalGap<=_d5){
var _d9=_cf[j];
_4.mixin(_d9,{start:_d8,end:_d8+_d6.w});
var ir=this._createRenderer(_d9,"label",this.labelRenderer,"dojoxCalendarLabel");
var _da=this.isItemBeingEdited(_d9)&&!this.liveLayout&&this._isEditing;
var h=_da?this._getRowHeight(_ca)-this.cellPaddingTop:_d3;
if(rd.rtl){
_d9.start=_d4-_d9.end;
_d9.end=_d9.start+_d6.w;
}
_9.set(ir.container,{"top":(_da?this.cellPaddingTop:_d0)+"px","left":_d9.start+"px","width":_d6.w+"px","height":h+"px"});
this._applyRendererLayout(_d9,ir,_d1,_d6.w,h,"label");
}else{
break;
}
_d0+=_d3+this.verticalGap;
}
for(var j;j<_cf.length;j++){
if(_cd[i]==null){
_cd[i]=[_cf[j]];
}else{
_cd[i].push(_cf[j]);
}
}
}
}
},_applyRendererLayout:function(_db,ir,_dc,w,h,_dd){
var _de=this.isItemBeingEdited(_db);
var _df=this.isItemSelected(_db);
var _e0=this.isItemHovered(_db);
var _e1=this.isItemFocused(_db);
var _e2=ir.renderer;
_e2.set("hovered",_e0);
_e2.set("selected",_df);
_e2.set("edited",_de);
_e2.set("focused",this.showFocus?_e1:false);
_e2.set("moveEnabled",this.isItemMoveEnabled(_db,_dd));
if(_dd!="label"){
_e2.set("resizeEnabled",this.isItemResizeEnabled(_db,_dd));
}
this.applyRendererZIndex(_db,ir,_e0,_df,_de,_e1);
if(_e2.updateRendering){
_e2.updateRendering(w,h);
}
_b.place(ir.container,_dc);
_9.set(ir.container,"display","block");
},_getCellAt:function(_e3,_e4,rtl){
if((rtl==undefined||rtl==true)&&!this.isLeftToRight()){
_e4=this.renderData.columnCount-1-_e4;
}
return this.gridTable.childNodes[0].childNodes[_e3].childNodes[_e4];
},_layoutExpandRenderers:function(_e5,_e6,_e7){
if(!this.expandRenderer){
return;
}
var rd=this.renderData;
if(rd.expandedRow==_e5){
if(rd.expandedRowCol!=null&&rd.expandedRowCol!=-1){
this._layoutExpandRendererImpl(rd.expandedRow,rd.expandedRowCol,null,true);
}
}else{
if(rd.expandedRow==null){
for(var i=0;i<rd.columnCount;i++){
if(_e6[i]){
this._layoutExpandRendererImpl(_e5,rd.rtl?rd.columnCount-1-i:i,_e7[i],false);
}
}
}
}
},_layoutExpandRendererImpl:function(_e8,_e9,_ea,_eb){
var d,ir;
var rd=this.renderData;
var cal=rd.dateModule;
var _ec=rd.cells[_e8];
ir=this._getExpandRenderer(_4.clone(rd.dates[_e8][_e9]),_ea,_e8,_e9,_eb);
var dim=_a.position(this._getCellAt(_e8,_e9));
dim.x-=rd.gridTablePosX;
this.layoutExpandRenderer(ir,d,_ea,dim,this.expandRendererHeight);
_b.place(ir.domNode,_ec);
_9.set(ir.domNode,"display","block");
},layoutExpandRenderer:function(_ed,_ee,_ef,_f0,_f1){
_9.set(_ed.domNode,{"left":_f0.x+"px","width":_f0.w+"px","height":_f1+"px","top":(_f0.h-_f1-1)+"px"});
},_onItemEditBeginGesture:function(e){
var p=this._edProps;
var _f2=p.editedItem;
var _f3=e.dates;
var _f4=this.newDate(p.editKind=="resizeEnd"?_f2.endTime:_f2.startTime);
if(p.rendererKind=="label"){
}else{
if(e.editKind=="move"&&(_f2.allDay||this.roundToDay)){
var cal=this.renderData.dateModule;
p.dayOffset=cal.difference(this.floorToDay(_f3[0],false,this.renderData),_f4,"day");
}
}
this.inherited(arguments);
},_computeItemEditingTimes:function(_f5,_f6,_f7,_f8,_f9){
var cal=this.renderData.dateModule;
var p=this._edProps;
if(_f7=="label"){
}else{
if(_f5.allDay||this.roundToDay){
var _fa=this.isStartOfDay(_f8[0]);
switch(_f6){
case "resizeEnd":
if(!_fa&&_f5.allDay){
_f8[0]=cal.add(_f8[0],"day",1);
}
case "resizeStart":
if(!_fa){
_f8[0]=this.floorToDay(_f8[0],true);
}
break;
case "move":
_f8[0]=cal.add(_f8[0],"day",p.dayOffset);
break;
case "resizeBoth":
if(!_fa){
_f8[0]=this.floorToDay(_f8[0],true);
}
if(!this.isStartOfDay(_f8[1])){
_f8[1]=this.floorToDay(cal.add(_f8[1],"day",1),true);
}
break;
}
}else{
_f8=this.inherited(arguments);
}
}
return _f8;
},getTime:function(e,x,y,_fb){
var rd=this.renderData;
if(e!=null){
var _fc=_a.position(this.itemContainer,true);
if(e.touches){
_fb=_fb==undefined?0:_fb;
x=e.touches[_fb].pageX-_fc.x;
y=e.touches[_fb].pageY-_fc.y;
}else{
x=e.pageX-_fc.x;
y=e.pageY-_fc.y;
}
}
var r=_a.getContentBox(this.itemContainer);
if(x<0){
x=0;
}else{
if(x>r.w){
x=r.w-1;
}
}
if(y<0){
y=0;
}else{
if(y>r.h){
y=r.h-1;
}
}
var w=_a.getMarginBox(this.itemContainer).w;
var _fd=w/rd.columnCount;
var row;
if(rd.expandedRow==null){
row=Math.floor(y/(_a.getMarginBox(this.itemContainer).h/rd.rowCount));
}else{
row=rd.expandedRow;
}
var r=_a.getContentBox(this.itemContainer);
if(rd.rtl){
x=r.w-x;
}
var col=Math.floor(x/_fd);
var tm=Math.floor((x-(col*_fd))*1440/_fd);
var _fe=null;
if(row<rd.dates.length&&col<this.renderData.dates[row].length){
_fe=this.newDate(this.renderData.dates[row][col]);
_fe=this.renderData.dateModule.add(_fe,"minute",tm);
}
return _fe;
},_onGridMouseUp:function(e){
this.inherited(arguments);
if(this._gridMouseDown){
this._gridMouseDown=false;
this._onGridClick({date:this.getTime(e),triggerEvent:e});
}
},_onGridTouchEnd:function(e){
this.inherited(arguments);
var g=this._gridProps;
if(g){
if(!this._isEditing){
if(!g.fromItem&&!g.editingOnStart){
this.selectFromEvent(e,null,null,true);
}
if(!g.fromItem){
if(this._pendingDoubleTap&&this._pendingDoubleTap.grid){
this._onGridDoubleClick({date:this.getTime(this._gridProps.event),triggerEvent:this._gridProps.event});
clearTimeout(this._pendingDoubleTap.timer);
delete this._pendingDoubleTap;
}else{
this._onGridClick({date:this.getTime(this._gridProps.event),triggerEvent:this._gridProps.event});
this._pendingDoubleTap={grid:true,timer:setTimeout(_4.hitch(this,function(){
delete this._pendingDoubleTap;
}),this.doubleTapDelay)};
}
}
}
this._gridProps=null;
}
},_onRowHeaderClick:function(e){
this._dispatchCalendarEvt(e,"onRowHeaderClick");
},onRowHeaderClick:function(e){
},expandRendererClickHandler:function(e,_ff){
_3.stop(e);
var ri=_ff.get("rowIndex");
var ci=_ff.get("columnIndex");
this._onExpandRendererClick(_4.mixin(this._createItemEditEvent(),{rowIndex:ri,columnIndex:ci,renderer:_ff,triggerEvent:e,date:this.renderData.dates[ri][ci]}));
},onExpandRendererClick:function(e){
},_onExpandRendererClick:function(e){
this._dispatchCalendarEvt(e,"onExpandRendererClick");
if(!e.isDefaultPrevented()){
if(this.getExpandedRowIndex()!=-1){
this.collapseRow();
}else{
this.expandRow(e.rowIndex,e.columnIndex);
}
}
},snapUnit:"minute",snapSteps:15,minDurationUnit:"minute",minDurationSteps:15,triggerExtent:3,liveLayout:false,stayInView:true,allowStartEndSwap:true,allowResizeLessThan24H:false});
});
