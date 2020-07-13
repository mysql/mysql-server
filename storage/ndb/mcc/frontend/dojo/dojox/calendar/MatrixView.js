//>>built
require({cache:{"url:dojox/calendar/templates/MatrixView.html":"<div data-dojo-attach-events=\"keydown:_onKeyDown\">\n\t<div  class=\"dojoxCalendarYearColumnHeader\" data-dojo-attach-point=\"yearColumnHeader\">\n\t\t<table><tr><td><span data-dojo-attach-point=\"yearColumnHeaderContent\"></span></td></tr></table>\t\t\n\t</div>\t\n\t<div data-dojo-attach-point=\"columnHeader\" class=\"dojoxCalendarColumnHeader\">\n\t\t<table data-dojo-attach-point=\"columnHeaderTable\" class=\"dojoxCalendarColumnHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t</div>\t\t\n\t<div dojoAttachPoint=\"rowHeader\" class=\"dojoxCalendarRowHeader\">\n\t\t<table data-dojo-attach-point=\"rowHeaderTable\" class=\"dojoxCalendarRowHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t</div>\t\n\t<div dojoAttachPoint=\"grid\" class=\"dojoxCalendarGrid\">\n\t\t<table data-dojo-attach-point=\"gridTable\" class=\"dojoxCalendarGridTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t</div>\t\n\t<div data-dojo-attach-point=\"itemContainer\" class=\"dojoxCalendarContainer\" data-dojo-attach-event=\"mousedown:_onGridMouseDown,mouseup:_onGridMouseUp,ondblclick:_onGridDoubleClick,touchstart:_onGridTouchStart,touchmove:_onGridTouchMove,touchend:_onGridTouchEnd\">\n\t\t<table data-dojo-attach-point=\"itemContainerTable\" class=\"dojoxCalendarContainerTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t</div>\t\n</div>\n"}});
define("dojox/calendar/MatrixView",["dojo/_base/declare","dojo/_base/array","dojo/_base/event","dojo/_base/lang","dojo/_base/sniff","dojo/_base/fx","dojo/_base/html","dojo/on","dojo/dom","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/dom-construct","dojo/query","dojo/i18n","./ViewBase","dojo/text!./templates/MatrixView.html","dijit/_TemplatedMixin"],function(_1,_2,_3,_4,_5,fx,_6,on,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
return _1("dojox.calendar.MatrixView",[_e,_10],{templateString:_f,baseClass:"dojoxCalendarMatrixView",_setTabIndexAttr:"domNode",viewKind:"matrix",renderData:null,startDate:null,refStartTime:null,refEndTime:null,columnCount:7,rowCount:5,horizontalRenderer:null,labelRenderer:null,expandRenderer:null,horizontalDecorationRenderer:null,percentOverlap:0,verticalGap:2,horizontalRendererHeight:17,labelRendererHeight:14,expandRendererHeight:15,cellPaddingTop:16,expandDuration:300,expandEasing:null,layoutDuringResize:false,roundToDay:true,showCellLabel:true,scrollable:false,resizeCursor:"e-resize",constructor:function(){
this.invalidatingProperties=["columnCount","rowCount","startDate","horizontalRenderer","horizontalDecaorationRenderer","labelRenderer","expandRenderer","rowHeaderDatePattern","columnHeaderLabelLength","cellHeaderShortPattern","cellHeaderLongPattern","percentOverlap","verticalGap","horizontalRendererHeight","labelRendererHeight","expandRendererHeight","cellPaddingTop","roundToDay","itemToRendererKindFunc","layoutPriorityFunction","formatItemTimeFunc","textDir","items"];
this._ddRendererList=[];
this._ddRendererPool=[];
this._rowHeaderHandles=[];
},destroy:function(_11){
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
d=this.addAndFloor(d,"day",1);
}
}
rd.startTime=this.newDate(rd.dates[0][0],rd);
rd.endTime=this.newDate(rd.dates[rd.rowCount-1][rd.columnCount-1],rd);
rd.endTime=rd.dateModule.add(rd.endTime,"day",1);
rd.endTime=this.floorToDay(rd.endTime,true);
if(this.displayedItemsInvalidated&&!this._isEditing){
this.displayedItemsInvalidated=false;
this._computeVisibleItems(rd);
}else{
if(this.renderData){
rd.items=this.renderData.items;
}
}
if(this.displayedDecorationItemsInvalidated){
rd.decorationItems=this.decorationStoreManager._computeVisibleItems(rd);
}else{
if(this.renderData){
rd.decorationItems=this.renderData.decorationItems;
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
},_setStartDateAttr:function(_12){
this.displayedItemsInvalidated=true;
this._set("startDate",_12);
},_setColumnCountAttr:function(_13){
this.displayedItemsInvalidated=true;
this._set("columnCount",_13);
},_setRowCountAttr:function(_14){
this.displayedItemsInvalidated=true;
this._set("rowCount",_14);
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
},cellHeaderShortPattern:null,cellHeaderLongPattern:null,_formatGridCellLabel:function(d,row,col){
var _15=row==0&&col==0||d.getDate()==1;
var _16,rb;
if(_15){
if(this.cellHeaderLongPattern){
_16=this.cellHeaderLongPattern;
}else{
rb=_d.getLocalization("dojo.cldr",this._calendar);
_16=rb["dateFormatItem-MMMd"];
}
}else{
if(this.cellHeaderShortPattern){
_16=this.cellHeaderShortPattern;
}else{
rb=_d.getLocalization("dojo.cldr",this._calendar);
_16=rb["dateFormatItem-d"];
}
}
return this.renderData.dateLocaleModule.format(d,{selector:"date",datePattern:_16});
},refreshRendering:function(){
this.inherited(arguments);
if(!this.domNode){
return;
}
this._validateProperties();
var _17=this.renderData;
var rd=this.renderData=this._createRenderData();
this._createRendering(rd,_17);
this._layoutDecorationRenderers(rd);
this._layoutRenderers(rd);
},_createRendering:function(_18,_19){
if(_18.rowHeight<=0){
_18.columnCount=1;
_18.rowCount=1;
_18.invalidRowHeight=true;
return;
}
if(_19){
if(this.itemContainerTable){
var _1a=_c(".dojoxCalendarItemContainerRow",this.itemContainerTable);
_19.rowCount=_1a.length;
}
}
this._buildColumnHeader(_18,_19);
this._buildRowHeader(_18,_19);
this._buildGrid(_18,_19);
this._buildItemContainer(_18,_19);
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
_8.add(_21,this._cssDays[_22.getDay()]);
if(this.isWeekEnd(_22)){
_8.add(_21,"dojoxCalendarWeekend");
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
var _33=_c("tr",_32);
var _34=_30.rowCount-_33.length;
var _35=_34>0;
var _36=_30.columnCount-(_33?_c("td",_33[0]).length:0);
if(_5("ie")==8){
if(this._gridTableSave==null){
this._gridTableSave=_4.clone(_32);
}else{
if(_36<0){
this.grid.removeChild(_32);
_b.destroy(_32);
_32=_4.clone(this._gridTableSave);
this.gridTable=_32;
this.grid.appendChild(_32);
_36=_30.columnCount;
_34=_30.rowCount;
_35=true;
}
}
}
var _37=_c("tbody",_32);
var _38;
if(_37.length==1){
_38=_37[0];
}else{
_38=_b.create("tbody",null,_32);
}
if(_35){
for(var i=0;i<_34;i++){
_b.create("tr",null,_38);
}
}else{
_34=-_34;
for(var i=0;i<_34;i++){
_38.removeChild(_38.lastChild);
}
}
var _39=_30.rowCount-_34;
var _3a=_35||_36>0;
_36=_3a?_36:-_36;
_c("tr",_32).forEach(function(tr,i){
if(_3a){
var len=i>=_39?_30.columnCount:_36;
for(var i=0;i<len;i++){
var td=_b.create("td",null,tr);
_b.create("span",null,td);
}
}else{
for(var i=0;i<_36;i++){
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
var _3b=_c("span",td)[0];
this._setText(_3b,this.showCellLabel?this._formatGridCellLabel(d,row,col):null);
this.styleGridCell(td,d,_30);
},this);
},this);
},styleGridCellFunc:null,defaultStyleGridCell:function(_3c,_3d,_3e){
_8.add(_3c,this._cssDays[_3d.getDay()]);
var cal=this.dateModule;
if(this.isToday(_3d)){
_8.add(_3c,"dojoxCalendarToday");
}else{
if(this.refStartTime!=null&&this.refEndTime!=null&&(cal.compare(_3d,this.refEndTime)>=0||cal.compare(cal.add(_3d,"day",1),this.refStartTime)<=0)){
_8.add(_3c,"dojoxCalendarDayDisabled");
}else{
if(this.isWeekEnd(_3d)){
_8.add(_3c,"dojoxCalendarWeekend");
}
}
}
},styleGridCell:function(_3f,_40,_41){
if(this.styleGridCellFunc){
this.styleGridCellFunc(_3f,_40,_41);
}else{
this.defaultStyleGridCell(_3f,_40,_41);
}
},_buildItemContainer:function(_42,_43){
var _44=this.itemContainerTable;
if(!_44){
return;
}
var _45=[];
var _46=_42.rowCount-(_43?_43.rowCount:0);
if(_5("ie")==8){
if(this._itemTableSave==null){
this._itemTableSave=_4.clone(_44);
}else{
if(_46<0){
this.itemContainer.removeChild(_44);
this._recycleItemRenderers(true);
this._recycleExpandRenderers(true);
_b.destroy(_44);
_44=_4.clone(this._itemTableSave);
this.itemContainerTable=_44;
this.itemContainer.appendChild(_44);
_46=_42.columnCount;
}
}
}
var _47=_c("tbody",_44);
var _48,tr,td,div;
if(_47.length==1){
_48=_47[0];
}else{
_48=_b.create("tbody",null,_44);
}
if(_46>0){
for(var i=0;i<_46;i++){
tr=_b.create("tr",null,_48);
_8.add(tr,"dojoxCalendarItemContainerRow");
td=_b.create("td",null,tr);
div=_b.create("div",null,td);
_8.add(div,"dojoxCalendarContainerRow");
}
}else{
_46=-_46;
for(var i=0;i<_46;i++){
_48.removeChild(_48.lastChild);
}
}
_c(".dojoxCalendarItemContainerRow",_44).forEach(function(tr,i){
_9.set(tr,"height",this._getRowHeight(i)+"px");
_45.push(tr.childNodes[0].childNodes[0]);
},this);
_42.cells=_45;
},resize:function(_49){
this.inherited(arguments);
this._resizeHandler(null,false);
},_resizeHandler:function(e,_4a){
var rd=this.renderData;
if(rd==null){
this.refreshRendering();
return;
}
if(rd.sheetHeight!=this.itemContainer.offsetHeight){
rd.sheetHeight=this.itemContainer.offsetHeight;
var _4b=this.getExpandedRowIndex();
if(_4b==-1){
this._computeRowsHeight();
this._resizeRows();
}else{
this.expandRow(rd.expandedRow,rd.expandedRowCol,0,null,true);
}
if(rd.invalidRowHeight){
delete rd.invalidRowHeight;
this.renderData=null;
this.displayedItemsInvalidated=true;
this.refreshRendering();
return;
}
}
if(this.layoutDuringResize||_4a){
setTimeout(_4.hitch(this,function(){
this._layoutRenderers(this.renderData);
this._layoutDecorationRenderers(this.renderData);
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
this._layoutDecorationRenderers(this.renderData);
if(this.resizeAnimationDuration==0){
_9.set(this.itemContainer,"opacity",1);
}else{
fx.fadeIn({node:this.itemContainer,curve:[0,1]}).play(this.resizeAnimationDuration);
}
}),200);
}
},resizeAnimationDuration:0,getExpandedRowIndex:function(){
return this.renderData.expandedRow==null?-1:this.renderData.expandedRow;
},collapseRow:function(_4c,_4d,_4e){
var rd=this.renderData;
if(_4e==undefined){
_4e=true;
}
if(_4c==undefined){
_4c=this.expandDuration;
}
if(rd&&rd.expandedRow!=null&&rd.expandedRow!=-1){
if(_4e&&_4c){
var _4f=rd.expandedRow;
var _50=rd.expandedRowHeight;
delete rd.expandedRow;
this._computeRowsHeight(rd);
var _51=this._getRowHeight(_4f);
rd.expandedRow=_4f;
this._recycleExpandRenderers();
this._recycleItemRenderers();
_9.set(this.itemContainer,"display","none");
this._expandAnimation=new fx.Animation({curve:[_50,_51],duration:_4c,easing:_4d,onAnimate:_4.hitch(this,function(_52){
this._expandRowImpl(Math.floor(_52));
}),onEnd:_4.hitch(this,function(_53){
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
this._collapseRowImpl(_4e);
}
}
},_collapseRowImpl:function(_54){
var rd=this.renderData;
delete rd.expandedRow;
delete rd.expandedRowHeight;
this._computeRowsHeight(rd);
if(_54==undefined||_54){
this._resizeRows();
this._layoutRenderers(rd);
}
},expandRow:function(_55,_56,_57,_58,_59){
var rd=this.renderData;
if(!rd||_55<0||_55>=rd.rowCount){
return -1;
}
if(_56==undefined||_56<0||_56>=rd.columnCount){
_56=-1;
}
if(_59==undefined){
_59=true;
}
if(_57==undefined){
_57=this.expandDuration;
}
if(_58==undefined){
_58=this.expandEasing;
}
var _5a=this._getRowHeight(_55);
var _5b=rd.sheetHeight-Math.ceil(this.cellPaddingTop*(rd.rowCount-1));
rd.expandedRow=_55;
rd.expandedRowCol=_56;
rd.expandedRowHeight=_5b;
if(_59){
if(_57){
this._recycleExpandRenderers();
this._recycleItemRenderers();
_9.set(this.itemContainer,"display","none");
this._expandAnimation=new fx.Animation({curve:[_5a,_5b],duration:_57,delay:50,easing:_58,onAnimate:_4.hitch(this,function(_5c){
this._expandRowImpl(Math.floor(_5c));
}),onEnd:_4.hitch(this,function(){
this._expandAnimation=null;
_9.set(this.itemContainer,"display","block");
setTimeout(_4.hitch(this,function(){
this._expandRowImpl(_5b,true);
}),100);
this.onExpandAnimationEnd(true);
})});
this._expandAnimation.play();
}else{
this._expandRowImpl(_5b,true);
}
}
},_expandRowImpl:function(_5d,_5e){
var rd=this.renderData;
rd.expandedRowHeight=_5d;
this._computeRowsHeight(rd,rd.sheetHeight-_5d);
this._resizeRows();
if(_5e){
this._layoutRenderers(rd);
}
},onExpandAnimationEnd:function(_5f){
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
},_computeRowsHeight:function(_60,max){
var rd=_60==null?this.renderData:_60;
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
var _61=rd.expandedRow==null?rd.rowCount:rd.rowCount-1;
var rhx=max/_61;
var rhf,rhl,rh;
var _62=max-(Math.floor(rhx)*_61);
var _63=Math.abs(max-(Math.ceil(rhx)*_61));
var _64;
var _65=1;
if(_62<_63){
rh=Math.floor(rhx);
_64=_62;
}else{
_65=-1;
rh=Math.ceil(rhx);
_64=_63;
}
rhf=rh+_65*Math.floor(_64/2);
rhl=rhf+_65*(_64%2);
rd.rowHeight=rh;
rd.rowHeightFirst=rhf;
rd.rowHeightLast=rhl;
},_getRowHeight:function(_66){
var rd=this.renderData;
if(_66==rd.expandedRow){
return rd.expandedRowHeight;
}else{
if(rd.expandedRow==0&&_66==1||_66==0){
return rd.rowHeightFirst;
}else{
if(rd.expandedRow==this.renderData.rowCount-1&&_66==this.renderData.rowCount-2||_66==this.renderData.rowCount-1){
return rd.rowHeightLast;
}else{
return rd.rowHeight;
}
}
}
},_resizeRowsImpl:function(_67,_68){
dojo.query(_68,_67).forEach(function(tr,i){
_9.set(tr,"height",this._getRowHeight(i)+"px");
},this);
},_setHorizontalRendererAttr:function(_69){
this._destroyRenderersByKind("horizontal");
this._set("horizontalRenderer",_69);
},_setLabelRendererAttr:function(_6a){
this._destroyRenderersByKind("label");
this._set("labelRenderer",_6a);
},_destroyExpandRenderer:function(_6b){
if(_6b["destroyRecursive"]){
_6b.destroyRecursive();
}
_6.destroy(_6b.domNode);
},_setExpandRendererAttr:function(_6c){
while(this._ddRendererList.length>0){
this._destroyExpandRenderer(this._ddRendererList.pop());
}
var _6d=this._ddRendererPool;
if(_6d){
while(_6d.length>0){
this._destroyExpandRenderer(_6d.pop());
}
}
this._set("expandRenderer",_6c);
},_ddRendererList:null,_ddRendererPool:null,_getExpandRenderer:function(_6e,_6f,_70,_71,_72){
if(this.expandRenderer==null){
return null;
}
var ir=this._ddRendererPool.pop();
if(ir==null){
ir=new this.expandRenderer();
}
this._ddRendererList.push(ir);
ir.set("owner",this);
ir.set("date",_6e);
ir.set("items",_6f);
ir.set("rowIndex",_70);
ir.set("columnIndex",_71);
ir.set("expanded",_72);
return ir;
},_recycleExpandRenderers:function(_73){
for(var i=0;i<this._ddRendererList.length;i++){
var ir=this._ddRendererList[i];
ir.set("Up",false);
ir.set("Down",false);
if(_73){
ir.domNode.parentNode.removeChild(ir.domNode);
}
_9.set(ir.domNode,"display","none");
}
this._ddRendererPool=this._ddRendererPool.concat(this._ddRendererList);
this._ddRendererList=[];
},_defaultItemToRendererKindFunc:function(_74){
var dur=Math.abs(this.renderData.dateModule.difference(_74.startTime,_74.endTime,"minute"));
return dur>=1440?"horizontal":"label";
},naturalRowsHeight:null,_roundItemToDay:function(_75){
var s=_75.startTime,e=_75.endTime;
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
},_overlapLayoutPass3:function(_76){
var pos=0,_77=0;
var res=[];
var _78=_a.position(this.gridTable).x;
for(var col=0;col<this.renderData.columnCount;col++){
var _79=false;
var _7a=_a.position(this._getCellAt(0,col));
pos=_7a.x-_78;
_77=pos+_7a.w;
for(var _7b=_76.length-1;_7b>=0&&!_79;_7b--){
for(var i=0;i<_76[_7b].length;i++){
var _7c=_76[_7b][i];
_79=_7c.start<_77&&pos<_7c.end;
if(_79){
res[col]=_7b+1;
break;
}
}
}
if(!_79){
res[col]=0;
}
}
return res;
},applyRendererZIndex:function(_7d,_7e,_7f,_80,_81,_82){
_9.set(_7e.container,{"zIndex":_81||_80?_7e.renderer.mobile?100:0:_7d.lane==undefined?1:_7d.lane+1});
},_layoutDecorationRenderers:function(_83){
if(_83==null||_83.decorationItems==null||_83.rowHeight<=0){
return;
}
if(!this.gridTable||this._expandAnimation!=null||this.horizontalDecorationRenderer==null){
this.decorationRendererManager.recycleItemRenderers();
return;
}
this._layoutStep=_83.columnCount;
this.renderData.gridTablePosX=_a.position(this.gridTable).x;
this.inherited(arguments);
},_layoutRenderers:function(_84){
if(_84==null||_84.items==null||_84.rowHeight<=0){
return;
}
if(!this.gridTable||this._expandAnimation!=null||(this.horizontalRenderer==null&&this.labelRenderer==null)){
this._recycleItemRenderers();
return;
}
this.renderData.gridTablePosX=_a.position(this.gridTable).x;
this._layoutStep=_84.columnCount;
this._recycleExpandRenderers();
this._hiddenItems=[];
this._offsets=[];
this.naturalRowsHeight=[];
this.inherited(arguments);
},_offsets:null,_layoutInterval:function(_85,_86,_87,end,_88,_89){
if(this.renderData.cells==null){
return;
}
if(_89==="dataItems"){
var _8a=[];
var _8b=[];
for(var i=0;i<_88.length;i++){
var _8c=_88[i];
var _8d=this._itemToRendererKind(_8c);
if(_8d=="horizontal"){
_8a.push(_8c);
}else{
if(_8d=="label"){
_8b.push(_8c);
}
}
}
var _8e=this.getExpandedRowIndex();
if(_8e!=-1&&_8e!=_86){
return;
}
var _8f;
var _90=[];
var _91=null;
var _92=[];
if(_8a.length>0&&this.horizontalRenderer){
var _91=this._createHorizontalLayoutItems(_86,_87,end,_8a,_89);
var _93=this._computeHorizontalOverlapLayout(_91,_92);
}
var _94;
var _95=[];
if(_8b.length>0&&this.labelRenderer){
_94=this._createLabelLayoutItems(_86,_87,end,_8b);
this._computeLabelOffsets(_94,_95);
}
var _96=this._computeColHasHiddenItems(_86,_92,_95);
if(_91!=null){
this._layoutHorizontalItemsImpl(_86,_91,_93,_96,_90,_89);
}
if(_94!=null){
this._layoutLabelItemsImpl(_86,_94,_96,_90,_92,_89);
}
this._layoutExpandRenderers(_86,_96,_90);
this._hiddenItems[_86]=_90;
}else{
if(this.horizontalDecorationRenderer){
var _91=this._createHorizontalLayoutItems(_86,_87,end,_88,_89);
if(_91!=null){
this._layoutHorizontalItemsImpl(_86,_91,null,false,null,_89);
}
}
}
},_createHorizontalLayoutItems:function(_97,_98,_99,_9a,_9b){
var rd=this.renderData;
var cal=rd.dateModule;
var _9c=rd.rtl?-1:1;
var _9d=[];
var _9e=_9b==="decorationItems";
for(var i=0;i<_9a.length;i++){
var _9f=_9a[i];
var _a0=this.computeRangeOverlap(rd,_9f.startTime,_9f.endTime,_98,_99);
var _a1=cal.difference(_98,this.floorToDay(_a0[0],false,rd),"day");
var _a2=rd.dates[_97][_a1];
var _a3=_a.position(this._getCellAt(_97,_a1,false));
var _a4=_a3.x-rd.gridTablePosX;
if(rd.rtl){
_a4+=_a3.w;
}
if(_9e&&!_9f.isAllDay||!_9e&&!this.roundToDay&&!_9f.allDay){
_a4+=_9c*this.computeProjectionOnDate(rd,_a2,_a0[0],_a3.w);
}
_a4=Math.ceil(_a4);
var _a5=cal.difference(_98,this.floorToDay(_a0[1],false,rd),"day");
var end;
if(_a5>rd.columnCount-1){
_a3=_a.position(this._getCellAt(_97,rd.columnCount-1,false));
if(rd.rtl){
end=_a3.x-rd.gridTablePosX;
}else{
end=_a3.x-rd.gridTablePosX+_a3.w;
}
}else{
_a2=rd.dates[_97][_a5];
_a3=_a.position(this._getCellAt(_97,_a5,false));
end=_a3.x-rd.gridTablePosX;
if(rd.rtl){
end+=_a3.w;
}
if(!_9e&&this.roundToDay){
if(!this.isStartOfDay(_a0[1])){
end+=_9c*_a3.w;
}
}else{
end+=_9c*this.computeProjectionOnDate(rd,_a2,_a0[1],_a3.w);
}
}
end=Math.floor(end);
if(rd.rtl){
var t=end;
end=_a4;
_a4=t;
}
if(end>_a4){
var _a6=_4.mixin({start:_a4,end:end,range:_a0,item:_9f,startOffset:_a1,endOffset:_a5},_9f);
_9d.push(_a6);
}
}
return _9d;
},_computeHorizontalOverlapLayout:function(_a7,_a8){
var rd=this.renderData;
var _a9=this.horizontalRendererHeight;
var _aa=this.computeOverlapping(_a7,this._overlapLayoutPass3);
var _ab=this.percentOverlap/100;
for(var i=0;i<rd.columnCount;i++){
var _ac=_aa.addedPassRes[i];
var _ad=rd.rtl?rd.columnCount-i-1:i;
if(_ab==0){
_a8[_ad]=_ac==0?0:_ac==1?_a9:_a9+(_ac-1)*(_a9+this.verticalGap);
}else{
_a8[_ad]=_ac==0?0:_ac*_a9-(_ac-1)*(_ab*_a9)+this.verticalGap;
}
_a8[_ad]+=this.cellPaddingTop;
}
return _aa;
},_createLabelLayoutItems:function(_ae,_af,_b0,_b1){
if(this.labelRenderer==null){
return;
}
var d;
var rd=this.renderData;
var cal=rd.dateModule;
var _b2=[];
for(var i=0;i<_b1.length;i++){
var _b3=_b1[i];
d=this.floorToDay(_b3.startTime,false,rd);
var _b4=this.dateModule.compare;
while(_b4(d,_b3.endTime)==-1&&_b4(d,_b0)==-1){
var _b5=cal.add(d,"day",1);
_b5=this.floorToDay(_b5,true);
var _b6=this.computeRangeOverlap(rd,_b3.startTime,_b3.endTime,d,_b5);
var _b7=cal.difference(_af,this.floorToDay(_b6[0],false,rd),"day");
if(_b7>=this.columnCount){
break;
}
if(_b7>=0){
var _b8=_b2[_b7];
if(_b8==null){
_b8=[];
_b2[_b7]=_b8;
}
_b8.push(_4.mixin({startOffset:_b7,range:_b6,item:_b3},_b3));
}
d=cal.add(d,"day",1);
this.floorToDay(d,true);
}
}
return _b2;
},_computeLabelOffsets:function(_b9,_ba){
for(var i=0;i<this.renderData.columnCount;i++){
_ba[i]=_b9[i]==null?0:_b9[i].length*(this.labelRendererHeight+this.verticalGap);
}
},_computeColHasHiddenItems:function(_bb,_bc,_bd){
var res=[];
var _be=this._getRowHeight(_bb);
var h;
var _bf=0;
for(var i=0;i<this.renderData.columnCount;i++){
h=_bc==null||_bc[i]==null?this.cellPaddingTop:_bc[i];
h+=_bd==null||_bd[i]==null?0:_bd[i];
if(h>_bf){
_bf=h;
}
res[i]=h>_be;
}
this.naturalRowsHeight[_bb]=_bf;
return res;
},_layoutHorizontalItemsImpl:function(_c0,_c1,_c2,_c3,_c4,_c5){
var rd=this.renderData;
var _c6=rd.cells[_c0];
var _c7=this._getRowHeight(_c0);
var _c8=this.horizontalRendererHeight;
var _c9=this.percentOverlap/100;
for(var i=0;i<_c1.length;i++){
var _ca=_c1[i];
var _cb=_ca.lane;
if(_c5==="dataItems"){
var _cc=this.cellPaddingTop;
if(_c9==0){
_cc+=_cb*(_c8+this.verticalGap);
}else{
_cc+=_cb*(_c8-_c9*_c8);
}
var exp=false;
var _cd=_c7;
if(this.expandRenderer){
for(var off=_ca.startOffset;off<=_ca.endOffset;off++){
if(_c3[off]){
exp=true;
break;
}
}
_cd=exp?_c7-this.expandRendererHeight:_c7;
}
if(_cc+_c8<=_cd){
var ir=this._createRenderer(_ca,"horizontal",this.horizontalRenderer,"dojoxCalendarHorizontal");
var _ce=this.isItemBeingEdited(_ca)&&!this.liveLayout&&this._isEditing;
var h=_ce?_c7-this.cellPaddingTop:_c8;
var w=_ca.end-_ca.start;
if(_5("ie")>=9&&_ca.start+w<this.itemContainer.offsetWidth){
w++;
}
_9.set(ir.container,{"top":(_ce?this.cellPaddingTop:_cc)+"px","left":_ca.start+"px","width":w+"px","height":h+"px"});
this._applyRendererLayout(_ca,ir,_c6,w,h,"horizontal");
}else{
for(var d=_ca.startOffset;d<_ca.endOffset;d++){
if(_c4[d]==null){
_c4[d]=[_ca.item];
}else{
_c4[d].push(_ca.item);
}
}
}
}else{
var ir=this.decorationRendererManager.createRenderer(_ca,"horizontal",this.horizontalDecorationRenderer,"dojoxCalendarDecoration");
var h=_c7;
var w=_ca.end-_ca.start;
if(_5("ie")>=9&&_ca.start+w<this.itemContainer.offsetWidth){
w++;
}
_9.set(ir.container,{"top":"0","left":_ca.start+"px","width":w+"px","height":h+"px"});
_b.place(ir.container,_c6);
_9.set(ir.container,"display","block");
}
}
},_layoutLabelItemsImpl:function(_cf,_d0,_d1,_d2,_d3){
var _d4,_d5;
var rd=this.renderData;
var _d6=rd.cells[_cf];
var _d7=this._getRowHeight(_cf);
var _d8=this.labelRendererHeight;
var _d9=_a.getMarginBox(this.itemContainer).w;
for(var i=0;i<_d0.length;i++){
_d4=_d0[i];
if(_d4!=null){
_d4.sort(_4.hitch(this,function(a,b){
return this.dateModule.compare(a.range[0],b.range[0]);
}));
var _da=this.expandRenderer?(_d1[i]?_d7-this.expandRendererHeight:_d7):_d7;
_d5=_d3==null||_d3[i]==null?this.cellPaddingTop:_d3[i]+this.verticalGap;
var _db=_a.position(this._getCellAt(_cf,i));
var _dc=_db.x-rd.gridTablePosX;
for(var j=0;j<_d4.length;j++){
if(_d5+_d8+this.verticalGap<=_da){
var _dd=_d4[j];
_4.mixin(_dd,{start:_dc,end:_dc+_db.w});
var ir=this._createRenderer(_dd,"label",this.labelRenderer,"dojoxCalendarLabel");
var _de=this.isItemBeingEdited(_dd)&&!this.liveLayout&&this._isEditing;
var h=_de?this._getRowHeight(_cf)-this.cellPaddingTop:_d8;
if(rd.rtl){
_dd.start=_d9-_dd.end;
_dd.end=_dd.start+_db.w;
}
_9.set(ir.container,{"top":(_de?this.cellPaddingTop:_d5)+"px","left":_dd.start+"px","width":_db.w+"px","height":h+"px"});
this._applyRendererLayout(_dd,ir,_d6,_db.w,h,"label");
}else{
break;
}
_d5+=_d8+this.verticalGap;
}
for(var j;j<_d4.length;j++){
if(_d2[i]==null){
_d2[i]=[_d4[j]];
}else{
_d2[i].push(_d4[j]);
}
}
}
}
},_applyRendererLayout:function(_df,ir,_e0,w,h,_e1){
var _e2=this.isItemBeingEdited(_df);
var _e3=this.isItemSelected(_df);
var _e4=this.isItemHovered(_df);
var _e5=this.isItemFocused(_df);
var _e6=ir.renderer;
_e6.set("hovered",_e4);
_e6.set("selected",_e3);
_e6.set("edited",_e2);
_e6.set("focused",this.showFocus?_e5:false);
_e6.set("moveEnabled",this.isItemMoveEnabled(_df._item,_e1));
_e6.set("storeState",this.getItemStoreState(_df));
if(_e1!="label"){
_e6.set("resizeEnabled",this.isItemResizeEnabled(_df,_e1));
}
this.applyRendererZIndex(_df,ir,_e4,_e3,_e2,_e5);
if(_e6.updateRendering){
_e6.updateRendering(w,h);
}
_b.place(ir.container,_e0);
_9.set(ir.container,"display","block");
},_getCellAt:function(_e7,_e8,rtl){
if((rtl==undefined||rtl==true)&&!this.isLeftToRight()){
_e8=this.renderData.columnCount-1-_e8;
}
return this.gridTable.childNodes[0].childNodes[_e7].childNodes[_e8];
},_layoutExpandRenderers:function(_e9,_ea,_eb){
if(!this.expandRenderer){
return;
}
var rd=this.renderData;
if(rd.expandedRow==_e9){
if(rd.expandedRowCol!=null&&rd.expandedRowCol!=-1){
this._layoutExpandRendererImpl(rd.expandedRow,rd.expandedRowCol,null,true);
}
}else{
if(rd.expandedRow==null){
for(var i=0;i<rd.columnCount;i++){
if(_ea[i]){
this._layoutExpandRendererImpl(_e9,rd.rtl?rd.columnCount-1-i:i,_eb[i],false);
}
}
}
}
},_layoutExpandRendererImpl:function(_ec,_ed,_ee,_ef){
var rd=this.renderData;
var d=_4.clone(rd.dates[_ec][_ed]);
var ir=null;
var _f0=rd.cells[_ec];
ir=this._getExpandRenderer(d,_ee,_ec,_ed,_ef);
var dim=_a.position(this._getCellAt(_ec,_ed));
dim.x-=rd.gridTablePosX;
this.layoutExpandRenderer(ir,d,_ee,dim,this.expandRendererHeight);
_b.place(ir.domNode,_f0);
_9.set(ir.domNode,"display","block");
},layoutExpandRenderer:function(_f1,_f2,_f3,_f4,_f5){
_9.set(_f1.domNode,{"left":_f4.x+"px","width":_f4.w+"px","height":_f5+"px","top":(_f4.h-_f5-1)+"px"});
},_onItemEditBeginGesture:function(e){
var p=this._edProps;
var _f6=p.editedItem;
var _f7=e.dates;
var _f8=this.newDate(p.editKind=="resizeEnd"?_f6.endTime:_f6.startTime);
if(p.rendererKind=="label"){
}else{
if(e.editKind=="move"&&(_f6.allDay||this.roundToDay)){
var cal=this.renderData.dateModule;
p.dayOffset=cal.difference(this.floorToDay(_f7[0],false,this.renderData),_f8,"day");
}
}
this.inherited(arguments);
},_computeItemEditingTimes:function(_f9,_fa,_fb,_fc,_fd){
var cal=this.renderData.dateModule;
var p=this._edProps;
if(_fb=="label"){
}else{
if(_f9.allDay||this.roundToDay){
var _fe=this.isStartOfDay(_fc[0]);
switch(_fa){
case "resizeEnd":
if(!_fe&&_f9.allDay){
_fc[0]=cal.add(_fc[0],"day",1);
}
case "resizeStart":
if(!_fe){
_fc[0]=this.floorToDay(_fc[0],true);
}
break;
case "move":
_fc[0]=cal.add(_fc[0],"day",p.dayOffset);
break;
case "resizeBoth":
if(!_fe){
_fc[0]=this.floorToDay(_fc[0],true);
}
if(!this.isStartOfDay(_fc[1])){
_fc[1]=this.floorToDay(cal.add(_fc[1],"day",1),true);
}
break;
}
}else{
_fc=this.inherited(arguments);
}
}
return _fc;
},getTime:function(e,x,y,_ff){
var rd=this.renderData;
if(e!=null){
var _100=_a.position(this.itemContainer,true);
if(e.touches){
_ff=_ff==undefined?0:_ff;
x=e.touches[_ff].pageX-_100.x;
y=e.touches[_ff].pageY-_100.y;
}else{
x=e.pageX-_100.x;
y=e.pageY-_100.y;
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
var colW=w/rd.columnCount;
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
var col=Math.floor(x/colW);
var tm=Math.floor((x-(col*colW))*1440/colW);
var date=null;
if(row<rd.dates.length&&col<this.renderData.dates[row].length){
date=this.newDate(this.renderData.dates[row][col]);
date=this.renderData.dateModule.add(date,"minute",tm);
}
return date;
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
},expandRendererClickHandler:function(e,_101){
_3.stop(e);
var ri=_101.get("rowIndex");
var ci=_101.get("columnIndex");
this._onExpandRendererClick(_4.mixin(this._createItemEditEvent(),{rowIndex:ri,columnIndex:ci,renderer:_101,triggerEvent:e,date:this.renderData.dates[ri][ci]}));
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
