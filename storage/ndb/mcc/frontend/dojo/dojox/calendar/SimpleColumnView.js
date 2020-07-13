//>>built
require({cache:{"url:dojox/calendar/templates/ColumnView.html":"<div data-dojo-attach-events=\"keydown:_onKeyDown\">\n\t\n\t<div data-dojo-attach-point=\"header\" class=\"dojoxCalendarHeader\">\n\t\t<div class=\"dojoxCalendarYearColumnHeader\" data-dojo-attach-point=\"yearColumnHeader\">\n\t\t\t<table cellspacing=\"0\" cellpadding=\"0\"><tr><td><span data-dojo-attach-point=\"yearColumnHeaderContent\"></span></td></tr></table>\t\t\n\t\t</div>\n\t\t<div data-dojo-attach-point=\"columnHeader\" class=\"dojoxCalendarColumnHeader\">\n\t\t\t<table data-dojo-attach-point=\"columnHeaderTable\" class=\"dojoxCalendarColumnHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t\t</div>\n\t</div>\n\t\n\t<div data-dojo-attach-point=\"secondarySheetNode\"></div>\n\t\n\t<div data-dojo-attach-point=\"subHeader\" class=\"dojoxCalendarSubHeader\">\n\t\t<div class=\"dojoxCalendarSubRowHeader\">\n\t\t\t<table cellspacing=\"0\" cellpadding=\"0\"><tr><td></td></tr></table>\t\t\n\t\t</div>\n\t\t<div data-dojo-attach-point=\"subColumnHeader\" class=\"dojoxCalendarSubColumnHeader\">\n\t\t\t<table data-dojo-attach-point=\"subColumnHeaderTable\" class=\"dojoxCalendarSubColumnHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t\t</div>\n\t</div>\n\t\n\t<div data-dojo-attach-point=\"scrollContainer\" class=\"dojoxCalendarScrollContainer\">\n\t\t<div data-dojo-attach-point=\"sheetContainer\" style=\"position:relative;left:0;right:0;margin:0;padding:0\">\n\t\t\t<div data-dojo-attach-point=\"rowHeader\" class=\"dojoxCalendarRowHeader\">\n\t\t\t\t<table data-dojo-attach-point=\"rowHeaderTable\" class=\"dojoxCalendarRowHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t\t\t</div>\n\t\t\t<div data-dojo-attach-point=\"grid\" class=\"dojoxCalendarGrid\">\n\t\t\t\t<table data-dojo-attach-point=\"gridTable\" class=\"dojoxCalendarGridTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t\t\t</div>\n\t\t\t<div data-dojo-attach-point=\"itemContainer\" class=\"dojoxCalendarContainer\" data-dojo-attach-event=\"mousedown:_onGridMouseDown,mouseup:_onGridMouseUp,ondblclick:_onGridDoubleClick,touchstart:_onGridTouchStart,touchmove:_onGridTouchMove,touchend:_onGridTouchEnd\">\n\t\t\t\t<table data-dojo-attach-point=\"itemContainerTable\" class=\"dojoxCalendarContainerTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t\t\t</div>\n\t\t</div> \n\t</div>\n\t\n\t<div data-dojo-attach-point=\"vScrollBar\" class=\"dojoxCalendarVScrollBar\">\n\t\t<div data-dojo-attach-point=\"vScrollBarContent\" style=\"visibility:hidden;position:relative;width:1px;height:1px;\" ></div>\n\t</div>\n\t\n\t<div data-dojo-attach-point=\"hScrollBar\" class=\"dojoxCalendarHScrollBar\">\n\t\t<div data-dojo-attach-point=\"hScrollBarContent\" style=\"visibility:hidden;position:relative;width:1px;height:1px;\" ></div>\n\t</div>\n\t\n</div>\n"}});
define("dojox/calendar/SimpleColumnView",["./ViewBase","dijit/_TemplatedMixin","./_ScrollBarBase","dojo/text!./templates/ColumnView.html","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/array","dojo/_base/sniff","dojo/_base/fx","dojo/_base/html","dojo/on","dojo/dom","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/dom-construct","dojo/mouse","dojo/query","dojox/html/metrics"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,fx,_a,on,_b,_c,_d,_e,_f,_10,_11,_12){
return _5("dojox.calendar.SimpleColumnView",[_1,_2],{baseClass:"dojoxCalendarColumnView",templateString:_4,viewKind:"columns",_setTabIndexAttr:"domNode",renderData:null,startDate:null,columnCount:7,subColumns:null,minHours:8,maxHours:18,hourSize:100,timeSlotDuration:15,rowHeaderGridSlotDuration:60,rowHeaderLabelSlotDuration:60,rowHeaderLabelOffset:2,rowHeaderFirstLabelOffset:2,verticalRenderer:null,verticalDecorationRenderer:null,minColumnWidth:-1,percentOverlap:70,horizontalGap:4,_showSecondarySheet:false,_columnHeaderHandlers:null,constructor:function(){
this.invalidatingProperties=["columnCount","startDate","minHours","maxHours","hourSize","verticalRenderer","verticalDecorationRenderer","rowHeaderTimePattern","columnHeaderDatePattern","timeSlotDuration","rowHeaderGridSlotDuration","rowHeaderLabelSlotDuration","rowHeaderLabelOffset","rowHeaderFirstLabelOffset","percentOverlap","horizontalGap","scrollBarRTLPosition","itemToRendererKindFunc","layoutPriorityFunction","formatItemTimeFunc","textDir","items","subColumns","minColumnWidth"];
this._columnHeaderHandlers=[];
},destroy:function(_13){
this._cleanupColumnHeader();
if(this.scrollBar){
this.scrollBar.destroy(_13);
}
this.inherited(arguments);
},_scrollBar_onScroll:function(_14){
this._setScrollPosition(_14);
},_hscrollBar_onScroll:function(_15){
this._setHScrollPosition(_15);
},buildRendering:function(){
this.inherited(arguments);
if(this.vScrollBar){
this.scrollBar=new _3({content:this.vScrollBarContent},this.vScrollBar);
this.scrollBar.on("scroll",_7.hitch(this,this._scrollBar_onScroll));
}
if(this.hScrollBar){
this.hScrollBarW=new _3({content:this.hScrollBarContent,direction:"horizontal",value:0},this.hScrollBar);
this.hScrollBarW.on("scroll",_7.hitch(this,this._hscrollBar_onScroll));
this._hScrollNodes=[this.columnHeaderTable,this.subColumnHeaderTable,this.gridTable,this.itemContainerTable];
}
this._viewHandles.push(on(this.scrollContainer,_10.wheel,dojo.hitch(this,this._mouseWheelScrollHander)));
},postscript:function(){
this.inherited(arguments);
this._initialized=true;
if(!this.invalidRendering){
this.refreshRendering();
}
},_setVerticalRendererAttr:function(_16){
this._destroyRenderersByKind("vertical");
this._set("verticalRenderer",_16);
},_createRenderData:function(){
var rd={};
rd.minHours=this.get("minHours");
rd.maxHours=this.get("maxHours");
rd.hourSize=this.get("hourSize");
rd.hourCount=rd.maxHours-rd.minHours;
rd.slotDuration=this.get("timeSlotDuration");
rd.rowHeaderGridSlotDuration=this.get("rowHeaderGridSlotDuration");
rd.slotSize=Math.ceil(rd.hourSize/(60/rd.slotDuration));
rd.hourSize=rd.slotSize*(60/rd.slotDuration);
rd.sheetHeight=rd.hourSize*rd.hourCount;
if(!this._rowHeaderWidth){
this._rowHeaderWidth=_e.getMarginBox(this.rowHeader).w;
}
rd.rowHeaderWidth=this._rowHeaderWidth;
var _17=_12.getScrollbar();
rd.scrollbarWidth=_17.w+1;
rd.scrollbarHeight=_17.h+1;
rd.dateLocaleModule=this.dateLocaleModule;
rd.dateClassObj=this.dateClassObj;
rd.dateModule=this.dateModule;
rd.dates=[];
rd.columnCount=this.get("columnCount");
rd.subColumns=this.get("subColumns");
rd.subColumnCount=rd.subColumns?rd.subColumns.length:1;
rd.hScrollPaneWidth=_e.getMarginBox(this.grid).w;
rd.minSheetWidth=this.minColumnWidth<0?-1:this.minColumnWidth*rd.subColumnCount*rd.columnCount;
rd.hScrollBarEnabled=this.minColumnWidth>0&&rd.hScrollPaneWidth<rd.minSheetWidth;
var d=this.get("startDate");
if(d==null){
d=new rd.dateClassObj();
}
d=this.floorToDay(d,false,rd);
this.startDate=d;
for(var col=0;col<rd.columnCount;col++){
rd.dates.push(d);
d=this.addAndFloor(d,"day",1);
}
rd.startTime=new rd.dateClassObj(rd.dates[0]);
rd.startTime.setHours(rd.minHours);
rd.endTime=new rd.dateClassObj(rd.dates[rd.columnCount-1]);
rd.endTime.setHours(rd.maxHours);
if(this.displayedItemsInvalidated&&!this._isEditing){
rd.items=this.storeManager._computeVisibleItems(rd);
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
return rd;
},_validateProperties:function(){
this.inherited(arguments);
var v=this.minHours;
if(v<0||v>23||isNaN(v)){
this.minHours=0;
}
v=this.maxHours;
if(v<1||v>36||isNaN(v)){
this.minHours=36;
}
if(this.minHours>this.maxHours){
var t=this.maxHours;
this.maxHours=this.minHours;
this.minHours=t;
}
if(this.maxHours-this.minHours<1){
this.minHours=0;
this.maxHours=24;
}
if(this.columnCount<1||isNaN(this.columnCount)){
this.columnCount=1;
}
v=this.percentOverlap;
if(v<0||v>100||isNaN(v)){
this.percentOverlap=70;
}
if(this.hourSize<5||isNaN(this.hourSize)){
this.hourSize=10;
}
v=this.timeSlotDuration;
if(v<1||v>60||isNaN(v)){
this.timeSlotDuration=15;
}
},_setStartDateAttr:function(_18){
this.displayedItemsInvalidated=true;
this._set("startDate",_18);
},_setColumnCountAttr:function(_19){
this.displayedItemsInvalidated=true;
this._set("columnCount",_19);
},__fixEvt:function(e){
e.sheet="primary";
e.source=this;
return e;
},rowHeaderTimePattern:null,_formatRowHeaderLabel:function(d){
return this.renderData.dateLocaleModule.format(d,{selector:"time",timePattern:this.rowHeaderTimePattern});
},columnHeaderDatePattern:null,_formatColumnHeaderLabel:function(d){
return this.renderData.dateLocaleModule.format(d,{selector:"date",datePattern:this.columnHeaderDatePattern,formatLength:"medium"});
},scrollBarRTLPosition:"left",_getStartTimeOfDay:function(){
var v=(this.get("maxHours")-this.get("minHours"))*this._getScrollPosition()/this.renderData.sheetHeight;
return {hours:this.renderData.minHours+Math.floor(v),minutes:(v-Math.floor(v))*60};
},_getEndTimeOfDay:function(){
var v=(this.get("maxHours")-this.get("minHours"))*(this._getScrollPosition()+this.scrollContainer.offsetHeight)/this.renderData.sheetHeight;
return {hours:this.renderData.minHours+Math.floor(v),minutes:(v-Math.floor(v))*60};
},startTimeOfDay:0,_setStartTimeOfDayAttr:function(_1a){
if(this.renderData){
this._setStartTimeOfDay(_1a.hours,_1a.minutes,_1a.duration,_1a.easing);
}else{
this._startTimeOfDayInvalidated=true;
}
this._set("startTimeOfDay",_1a);
},_getStartTimeOfDayAttr:function(){
if(this.renderData){
return this._getStartTimeOfDay();
}else{
return this._get("startTimeOfDay");
}
},_setStartTimeOfDay:function(_1b,_1c,_1d,_1e){
var rd=this.renderData;
_1b=_1b||rd.minHours;
_1c=_1c||0;
_1d=_1d||0;
if(_1c<0){
_1c=0;
}else{
if(_1c>59){
_1c=59;
}
}
if(_1b<0){
_1b=0;
}else{
if(_1b>rd.maxHours){
_1b=rd.maxHours;
}
}
var _1f=_1b*60+_1c;
var _20=rd.minHours*60;
var _21=rd.maxHours*60;
if(_1f<_20){
_1f=_20;
}else{
if(_1f>_21){
_1f=_21;
}
}
var pos=(_1f-_20)*rd.sheetHeight/(_21-_20);
pos=Math.min(rd.sheetHeight-this.scrollContainer.offsetHeight,pos);
this._scrollToPosition(pos,_1d,_1e);
},_scrollToPosition:function(_22,_23,_24){
if(_23){
if(this._scrollAnimation){
this._scrollAnimation.stop();
}
var _25=this._getScrollPosition();
var _26=Math.abs(((_22-_25)*_23)/this.renderData.sheetHeight);
this._scrollAnimation=new fx.Animation({curve:[_25,_22],duration:_26,easing:_24,onAnimate:_7.hitch(this,function(_27){
this._setScrollImpl(_27);
})});
this._scrollAnimation.play();
}else{
this._setScrollImpl(_22);
}
},_setScrollImpl:function(v){
this._setScrollPosition(v);
if(this.scrollBar){
this.scrollBar.set("value",v);
}
},ensureVisibility:function(_28,end,_29,_2a,_2b){
_2a=_2a==undefined?this.renderData.slotDuration:_2a;
if(this.scrollable&&this.autoScroll){
var s=_28.getHours()*60+_28.getMinutes()-_2a;
var e=end.getHours()*60+end.getMinutes()+_2a;
var vs=this._getStartTimeOfDay();
var ve=this._getEndTimeOfDay();
var _2c=vs.hours*60+vs.minutes;
var _2d=ve.hours*60+ve.minutes;
var _2e=false;
var _2f=null;
switch(_29){
case "start":
_2e=s>=_2c&&s<=_2d;
_2f=s;
break;
case "end":
_2e=e>=_2c&&e<=_2d;
_2f=e-(_2d-_2c);
break;
case "both":
_2e=s>=_2c&&e<=_2d;
_2f=s;
break;
}
if(!_2e){
this._setStartTimeOfDay(Math.floor(_2f/60),_2f%60,_2b);
}
}
},scrollView:function(dir){
var t=this._getStartTimeOfDay();
t=t.hours*60+t.minutes+(dir*this.timeSlotDuration);
this._setStartTimeOfDay(Math.floor(t/60),t%60);
},scrollViewHorizontal:function(dir){
this._setHScrollPosition(this._getHScrollPosition()+(dir*this.minColumnWidth));
if(this.hScrollBarW){
this.hScrollBarW.set("value",this._getHScrollPosition());
}
},_hScrollNodes:null,_setHScrollPositionImpl:function(pos,_30,_31){
var _32=[this.columnHeaderTable,this.subColumnHeaderTable,this.gridTable,this.itemContainerTable];
var css=_30?null:"translateX(-"+pos+"px)";
_8.forEach(_32,function(elt){
if(_30){
elt.scrollLeft=pos;
_d.set(elt,"left",(-pos)+"px");
}else{
_d.set(elt,_31,css);
}
},this);
},_mouseWheelScrollHander:function(e){
if(this.renderData.hScrollBarEnabled&&e.altKey){
this.scrollViewHorizontal(e.wheelDelta>0?-1:1);
}else{
this.scrollView(e.wheelDelta>0?-1:1);
}
_6.stop(e);
},refreshRendering:function(){
if(!this._initialized){
return;
}
this._validateProperties();
var _33=this.renderData;
var rd=this._createRenderData();
this.renderData=rd;
this._createRendering(rd,_33);
this._layoutDecorationRenderers(rd);
this._layoutRenderers(rd);
},_createRendering:function(_34,_35){
_d.set(this.sheetContainer,"height",_34.sheetHeight+"px");
this._configureVisibleParts(_34);
this._configureScrollBar(_34);
this._buildColumnHeader(_34,_35);
this._buildSubColumnHeader(_34,_35);
this._buildRowHeader(_34,_35);
this._buildGrid(_34,_35);
this._buildItemContainer(_34,_35);
this._layoutTimeIndicator(_34);
this._commitProperties(_34);
},_configureVisibleParts:function(_36){
if(this.secondarySheetNode){
_d.set(this.secondarySheetNode,"display",this._showSecondarySheet?"block":"none");
}
_c[this.subColumns==null?"remove":"add"](this.domNode,"subColumns");
_c[this._showSecondarySheet?"add":"remove"](this.domNode,"secondarySheet");
},_commitProperties:function(_37){
if(this._startTimeOfDayInvalidated){
this._startTimeOfDayInvalidated=false;
var v=this.startTimeOfDay;
if(v!=null){
this._setStartTimeOfDay(v.hours,v.minutes==undefined?0:v.minutes);
}
}
},_configureScrollBar:function(_38){
if(_9("ie")&&this.scrollBar){
_d.set(this.vScrollBar,"width",(_38.scrollbarWidth+1)+"px");
}
var _39=this.isLeftToRight()?true:this.scrollBarRTLPosition=="right";
var _3a=_39?"right":"left";
var _3b=_39?"left":"right";
if(this.scrollBar){
this.scrollBar.set("maximum",_38.sheetHeight);
_d.set(this.vScrollBar,_3a,0);
_d.set(this.vScrollBar,_39?"left":"right","auto");
_d.set(this.vScrollBar,"bottom",_38.hScrollBarEnabled?_38.scrollbarHeight+"px":"0");
}
_d.set(this.scrollContainer,_3a,_38.scrollbarWidth+"px");
_d.set(this.scrollContainer,_3b,"0");
_d.set(this.header,_3a,_38.scrollbarWidth+"px");
_d.set(this.header,_3b,"0");
_d.set(this.subHeader,_3a,_38.scrollbarWidth+"px");
_d.set(this.subHeader,_3b,"0");
if(this.buttonContainer&&this.owner!=null&&this.owner.currentView==this){
_d.set(this.buttonContainer,_3a,_38.scrollbarWidth+"px");
_d.set(this.buttonContainer,_3b,"0");
}
if(this.hScrollBar){
_8.forEach(this._hScrollNodes,function(elt){
_c[_38.hScrollBarEnabled?"add":"remove"](elt.parentNode,"dojoxCalendarHorizontalScroll");
},this);
if(!_38.hScrollBarEnabled){
this._setHScrollPosition(0);
this.hScrollBarW.set("value",0);
}
_d.set(this.hScrollBar,{"display":_38.hScrollBarEnabled?"block":"none","height":_38.scrollbarHeight+"px","left":(_39?_38.rowHeaderWidth:_38.scrollbarWidth)+"px","right":(_39?_38.scrollbarWidth:_38.rowHeaderWidth)+"px"});
_d.set(this.scrollContainer,"bottom",_38.hScrollBarEnabled?(_38.scrollbarHeight+1)+"px":"0");
this._configureHScrollDomNodes(_38.hScrollBarEnabled?_38.minSheetWidth+"px":"100%");
this.hScrollBarW.set("maximum",_38.minSheetWidth);
this.hScrollBarW.set("containerSize",_38.hScrollPaneWidth);
}
},_configureHScrollDomNodes:function(_3c){
_8.forEach(this._hScrollNodes,function(elt){
_d.set(elt,"width",_3c);
},this);
},resize:function(e){
this._resizeHandler(e);
},_resizeHandler:function(e,_3d){
var rd=this.renderData;
if(rd==null){
return;
}
if(_3d){
var _3e=_e.getMarginBox(this.grid).w;
if(rd.hScrollPaneWidth!=_3e){
rd.hScrollPaneWidth=_3e;
rd.minSheetWidth=this.minColumnWidth<0?-1:this.minColumnWidth*rd.subColumnCount*rd.columnCount;
rd.hScrollBarEnabled=this.minColumnWidth>0&&_e.getMarginBox(this.grid).w<rd.minSheetWidth;
}
this._configureScrollBar(rd);
}else{
if(this._resizeTimer!=undefined){
clearTimeout(this._resizeTimer);
}
this._resizeTimer=setTimeout(_7.hitch(this,function(){
this._resizeHandler(e,true);
}),100);
}
},_columnHeaderClick:function(e){
_6.stop(e);
var _3f=_11("td",this.columnHeaderTable).indexOf(e.currentTarget);
this._onColumnHeaderClick({index:_3f,date:this.renderData.dates[_3f],triggerEvent:e});
},_buildColumnHeader:function(_40,_41){
var _42=this.columnHeaderTable;
if(!_42){
return;
}
var _43=_40.columnCount-(_41?_41.columnCount:0);
if(_9("ie")==8){
if(this._colTableSave==null){
this._colTableSave=_7.clone(_42);
}else{
if(_43<0){
this._cleanupColumnHeader();
this.columnHeader.removeChild(_42);
_f.destroy(_42);
_42=_7.clone(this._colTableSave);
this.columnHeaderTable=_42;
this.columnHeader.appendChild(_42);
_43=_40.columnCount;
}
}
}
var _44=_11("tbody",_42);
var trs=_11("tr",_42);
var _45,tr,td;
if(_44.length==1){
_45=_44[0];
}else{
_45=_a.create("tbody",null,_42);
}
if(trs.length==1){
tr=trs[0];
}else{
tr=_f.create("tr",null,_45);
}
if(_43>0){
for(var i=0;i<_43;i++){
td=_f.create("td",null,tr);
var h=[];
h.push(on(td,"click",_7.hitch(this,this._columnHeaderClick)));
if(_9("touch-events")){
h.push(on(td,"touchstart",function(e){
_6.stop(e);
_c.add(e.currentTarget,"Active");
}));
h.push(on(td,"touchend",function(e){
_6.stop(e);
_c.remove(e.currentTarget,"Active");
}));
}else{
h.push(on(td,"mousedown",function(e){
_6.stop(e);
_c.add(e.currentTarget,"Active");
}));
h.push(on(td,"mouseup",function(e){
_6.stop(e);
_c.remove(e.currentTarget,"Active");
}));
h.push(on(td,"mouseover",function(e){
_6.stop(e);
_c.add(e.currentTarget,"Hover");
}));
h.push(on(td,"mouseout",function(e){
_6.stop(e);
_c.remove(e.currentTarget,"Hover");
}));
}
this._columnHeaderHandlers.push(h);
}
}else{
_43=-_43;
for(var i=0;i<_43;i++){
td=tr.lastChild;
tr.removeChild(td);
_f.destroy(td);
var _46=this._columnHeaderHandlers.pop();
while(_46.length>0){
_46.pop().remove();
}
}
}
_11("td",_42).forEach(function(td,i){
td.className="";
if(i==0){
_c.add(td,"first-child");
}else{
if(i==this.renderData.columnCount-1){
_c.add(td,"last-child");
}
}
var d=_40.dates[i];
this._setText(td,this._formatColumnHeaderLabel(d));
this.styleColumnHeaderCell(td,d,_40);
},this);
if(this.yearColumnHeaderContent){
var d=_40.dates[0];
this._setText(this.yearColumnHeaderContent,_40.dateLocaleModule.format(d,{selector:"date",datePattern:"yyyy"}));
}
},_cleanupColumnHeader:function(){
while(this._columnHeaderHandlers.length>0){
var _47=this._columnHeaderHandlers.pop();
while(_47.length>0){
_47.pop().remove();
}
}
},styleColumnHeaderCell:function(_48,_49,_4a){
_c.add(_48,this._cssDays[_49.getDay()]);
if(this.isToday(_49)){
_c.add(_48,"dojoxCalendarToday");
}else{
if(this.isWeekEnd(_49)){
_c.add(_48,"dojoxCalendarWeekend");
}
}
},_buildSubColumnHeader:function(_4b,_4c){
var _4d=this.subColumnHeaderTable;
if(!_4d||this.subColumns==null){
return;
}
var _4e=_4b.columnCount-_11("td",_4d).length;
if(_9("ie")==8){
if(this._colSubTableSave==null){
this._colSubTableSave=_7.clone(_4d);
}else{
if(_4e<0){
this.subColumnHeader.removeChild(_4d);
_f.destroy(_4d);
_4d=_7.clone(this._colSubTableSave);
this.subColumnHeaderTable=_4d;
this.subColumnHeader.appendChild(_4d);
_4e=_4b.columnCount;
}
}
}
var _4f=_11(">tbody",_4d);
var _50,tr,td;
if(_4f.length==1){
_50=_4f[0];
}else{
_50=_a.create("tbody",null,_4d);
}
var trs=_11(">tr",_50);
if(trs.length==1){
tr=trs[0];
}else{
tr=_f.create("tr",null,_50);
}
var _51=_4b.subColumnCount;
if(_4e>0){
for(var i=0;i<_4e;i++){
td=_f.create("td",null,tr);
_f.create("div",{"className":"dojoxCalendarSubHeaderContainer"},td);
}
}else{
_4e=-_4e;
for(var i=0;i<_4e;i++){
td=tr.lastChild;
tr.removeChild(td);
_f.destroy(td);
}
}
_11("td",_4d).forEach(function(td,i){
td.className="";
if(i==0){
_c.add(td,"first-child");
}else{
if(i==this.renderData.columnCount-1){
_c.add(td,"last-child");
}
}
_11(".dojoxCalendarSubHeaderContainer",td).forEach(function(div,i){
var _52=_11(".dojoxCalendarSubHeaderContainer",div).length-_51;
if(_52!=0){
var len=div.childNodes.length;
for(var i=0;i<len;i++){
div.removeChild(div.lastChild);
}
for(var j=0;j<_51;j++){
_f.create("div",{"className":"dojoxCalendarSubHeaderCell dojoxCalendarSubHeaderLabel"},div);
}
}
var _53=(100/_51)+"%";
_11(".dojoxCalendarSubHeaderCell",div).forEach(function(div,i){
div.className="dojoxCalendarSubHeaderCell dojoxCalendarSubHeaderLabel";
var col=_51==1?i:Math.floor(i/_51);
subColIdx=_51==1?0:i-col*_51;
_d.set(div,{width:_53,left:((subColIdx*100)/_51)+"%"});
_c[subColIdx<_51-1&&_51!==1?"add":"remove"](div,"subColumn");
_c.add(div,this.subColumns[subColIdx]);
this._setText(div,this.subColumnLabelFunc(this.subColumns[subColIdx]));
},this);
},this);
var d=_4b.dates[i];
this.styleSubColumnHeaderCell(td,d,_4b);
},this);
},subColumnLabelFunc:function(_54){
return _54;
},styleSubColumnHeaderCell:function(_55,_56,_57){
_c.add(_55,this._cssDays[_56.getDay()]);
if(this.isToday(_56)){
_c.add(_55,"dojoxCalendarToday");
}else{
if(this.isWeekEnd(_56)){
_c.add(_55,"dojoxCalendarWeekend");
}
}
},_addMinutesClasses:function(_58,_59){
switch(_59){
case 0:
_c.add(_58,"hour");
break;
case 30:
_c.add(_58,"halfhour");
break;
case 15:
case 45:
_c.add(_58,"quarterhour");
break;
}
},_buildRowHeader:function(_5a,_5b){
var _5c=this.rowHeaderTable;
if(!_5c){
return;
}
if(this._rowHeaderLabelContainer==null){
this._rowHeaderLabelContainer=_f.create("div",{"class":"dojoxCalendarRowHeaderLabelContainer"},this.rowHeader);
}
_d.set(_5c,"height",_5a.sheetHeight+"px");
var _5d=_11("tbody",_5c);
var _5e,tr,td;
if(_5d.length==1){
_5e=_5d[0];
}else{
_5e=_f.create("tbody",null,_5c);
}
var _5f=Math.floor(60/_5a.rowHeaderGridSlotDuration)*_5a.hourCount;
var _60=_5f-(_5b?Math.floor(60/_5b.rowHeaderGridSlotDuration)*_5b.hourCount:0);
if(_60>0){
for(var i=0;i<_60;i++){
tr=_f.create("tr",null,_5e);
td=_f.create("td",null,tr);
}
}else{
_60=-_60;
for(var i=0;i<_60;i++){
_5e.removeChild(_5e.lastChild);
}
}
var rd=this.renderData;
var _61=Math.ceil(_5a.hourSize/(60/_5a.rowHeaderGridSlotDuration));
var d=new Date(2000,0,1,0,0,0);
_11("tr",_5c).forEach(function(tr,i){
var td=_11("td",tr)[0];
td.className="";
_d.set(tr,"height",(_9("ie")==7)?_61-2*(60/_5a.rowHeaderGridSlotDuration):_61+"px");
var h=_5a.minHours+(i*this.renderData.rowHeaderGridSlotDuration)/60;
var m=(i*this.renderData.rowHeaderGridSlotDuration)%60;
this.styleRowHeaderCell(td,h,m,rd);
this._addMinutesClasses(td,m);
},this);
var lc=this._rowHeaderLabelContainer;
_60=(Math.floor(60/this.rowHeaderLabelSlotDuration)*_5a.hourCount)-lc.childNodes.length;
var _62;
if(_60>0){
for(var i=0;i<_60;i++){
_62=_f.create("span",null,lc);
_c.add(_62,"dojoxCalendarRowHeaderLabel");
}
}else{
_60=-_60;
for(var i=0;i<_60;i++){
lc.removeChild(lc.lastChild);
}
}
_61=Math.ceil(_5a.hourSize/(60/this.rowHeaderLabelSlotDuration));
_11(">span",lc).forEach(function(_63,i){
d.setHours(0);
d.setMinutes(_5a.minHours*60+(i*this.rowHeaderLabelSlotDuration));
this._configureRowHeaderLabel(_63,d,i,_61*i,rd);
},this);
},_configureRowHeaderLabel:function(_64,d,_65,pos,_66){
this._setText(_64,this._formatRowHeaderLabel(d));
_d.set(_64,"top",(pos+(_65==0?this.rowHeaderFirstLabelOffset:this.rowHeaderLabelOffset))+"px");
var h=_66.minHours+(_65*this.rowHeaderLabelSlotDuration)/60;
var m=(_65*this.rowHeaderLabelSlotDuration)%60;
_c.remove(_64,["hour","halfhour","quarterhour"]);
this._addMinutesClasses(_64,m);
this.styleRowHeaderCell(_64,h,m,_66);
},styleRowHeaderCell:function(_67,h,m,_68){
},_buildGrid:function(_69,_6a){
var _6b=this.gridTable;
if(!_6b){
return;
}
_d.set(_6b,"height",_69.sheetHeight+"px");
var _6c=Math.floor(60/_69.slotDuration)*_69.hourCount;
var _6d=_6c-(_6a?Math.floor(60/_6a.slotDuration)*_6a.hourCount:0);
var _6e=_6d>0;
var _6f=(_69.columnCount-(_6a?_6a.columnCount:0));
if(_9("ie")==8){
if(this._gridTableSave==null){
this._gridTableSave=_7.clone(_6b);
}else{
if(_6f<0){
this.grid.removeChild(_6b);
_f.destroy(_6b);
_6b=_7.clone(this._gridTableSave);
this.gridTable=_6b;
this.grid.appendChild(_6b);
_6f=_69.columnCount;
_6d=_6c;
_6e=true;
}
}
}
var _70=_11("tbody",_6b);
var _71;
if(_70.length==1){
_71=_70[0];
}else{
_71=_f.create("tbody",null,_6b);
}
if(_6e){
for(var i=0;i<_6d;i++){
_f.create("tr",null,_71);
}
}else{
_6d=-_6d;
for(var i=0;i<_6d;i++){
_71.removeChild(_71.lastChild);
}
}
var _72=Math.floor(60/_69.slotDuration)*_69.hourCount-_6d;
var _73=_6e||_6f>0;
_6f=_73?_6f:-_6f;
_11("tr",_6b).forEach(function(tr,i){
if(_73){
var len=i>=_72?_69.columnCount:_6f;
for(var i=0;i<len;i++){
_f.create("td",null,tr);
}
}else{
for(var i=0;i<_6f;i++){
tr.removeChild(tr.lastChild);
}
}
});
_11("tr",_6b).forEach(function(tr,i){
_d.set(tr,"height",_69.slotSize+"px");
if(i==0){
_c.add(tr,"first-child");
}else{
if(i==_6c-1){
_c.add(tr,"last-child");
}
}
var m=(i*this.renderData.slotDuration)%60;
var h=this.minHours+Math.floor((i*this.renderData.slotDuration)/60);
_11("td",tr).forEach(function(td,col){
td.className="";
if(col==0){
_c.add(td,"first-child");
}else{
if(col==this.renderData.columnCount-1){
_c.add(td,"last-child");
}
}
var d=_69.dates[col];
this.styleGridCell(td,d,h,m,_69);
this._addMinutesClasses(td,m);
},this);
},this);
},styleGridCellFunc:null,defaultStyleGridCell:function(_74,_75,_76,_77,_78){
_c.add(_74,[this._cssDays[_75.getDay()],"H"+_76,"M"+_77]);
if(this.isToday(_75)){
return _c.add(_74,"dojoxCalendarToday");
}else{
if(this.isWeekEnd(_75)){
return _c.add(_74,"dojoxCalendarWeekend");
}
}
},styleGridCell:function(_79,_7a,_7b,_7c,_7d){
if(this.styleGridCellFunc){
this.styleGridCellFunc(_79,_7a,_7b,_7c,_7d);
}else{
this.defaultStyleGridCell(_79,_7a,_7b,_7c,_7d);
}
},_buildItemContainer:function(_7e,_7f){
var _80=this.itemContainerTable;
if(!_80){
return;
}
var _81=[],_82=[];
_d.set(_80,"height",_7e.sheetHeight+"px");
var _83=_7f?_7f.columnCount:0;
var _84=_7e.columnCount-_83;
if(_9("ie")==8){
if(this._itemTableSave==null){
this._itemTableSave=_7.clone(_80);
}else{
if(_84<0){
this.itemContainer.removeChild(_80);
this._recycleItemRenderers(true);
_f.destroy(_80);
_80=_7.clone(this._itemTableSave);
this.itemContainerTable=_80;
this.itemContainer.appendChild(_80);
_84=_7e.columnCount;
}
}
}
var _85=_11("tbody",_80);
var trs=_11("tr",_80);
var _86,tr,td;
if(_85.length==1){
_86=_85[0];
}else{
_86=_f.create("tbody",null,_80);
}
if(trs.length==1){
tr=trs[0];
}else{
tr=_f.create("tr",null,_86);
}
var _87=_7e.subColumnCount;
if(_84>0){
for(var i=0;i<_84;i++){
td=_f.create("td",null,tr);
_f.create("div",{"className":"dojoxCalendarContainerColumn"},td);
}
}else{
_84=-_84;
for(var i=0;i<_84;i++){
tr.removeChild(tr.lastChild);
}
}
_11("td",_80).forEach(function(td,i){
_11(".dojoxCalendarContainerColumn",td).forEach(function(div,i){
_d.set(div,"height",_7e.sheetHeight+"px");
var _88=_11(".dojoxCalendarSubContainerColumn",td).length-_87;
if(_88!=0){
var len=div.childNodes.length;
for(var i=0;i<len;i++){
div.removeChild(div.lastChild);
}
for(var j=0;j<_87;j++){
var _89=_f.create("div",{"className":"dojoxCalendarSubContainerColumn"},div);
_f.create("div",{"className":"dojoxCalendarDecorationContainerColumn"},_89);
_f.create("div",{"className":"dojoxCalendarEventContainerColumn"},_89);
}
}
},this);
var _8a=(100/_87)+"%";
_11(".dojoxCalendarSubContainerColumn",td).forEach(function(div,i){
var col=_87==1?i:Math.floor(i/_87);
subColIdx=_87==1?0:i-col*_87;
_d.set(div,{width:_8a,left:((subColIdx*100)/_87)+"%"});
_c[subColIdx<_87-1&&_87!==1?"add":"remove"](div,"subColumn");
_11(".dojoxCalendarEventContainerColumn",div).forEach(function(_8b,i){
_81.push(_8b);
},this);
_11(".dojoxCalendarDecorationContainerColumn",div).forEach(function(_8c,i){
_82.push(_8c);
},this);
},this);
},this);
_7e.cells=_81;
_7e.decorationCells=_82;
},showTimeIndicator:true,timeIndicatorRefreshInterval:60000,_setShowTimeIndicatorAttr:function(_8d){
this._set("showTimeIndicator",_8d);
this._layoutTimeIndicator(this.renderData);
},_layoutTimeIndicator:function(_8e){
if(!_8e){
return;
}
if(this.showTimeIndicator){
var now=new _8e.dateClassObj();
var _8f=this.isOverlapping(_8e,_8e.startTime,_8e.endTime,now,now)&&now.getHours()>=this.get("minHours")&&(now.getHours()*60+now.getMinutes()<this.get("maxHours")*60);
if(_8f){
if(!this._timeIndicator){
this._timeIndicator=_f.create("div",{"className":"dojoxCalendarTimeIndicator"});
}
var _90=this._timeIndicator;
for(var _91=0;_91<this.renderData.columnCount;_91++){
if(this.isSameDay(now,this.renderData.dates[_91])){
break;
}
}
var top=this.computeProjectionOnDate(_8e,this.floorToDay(now),now,_8e.sheetHeight);
if(top!=_8e.sheetHeight){
_d.set(_90,{top:top+"px",display:"block"});
var _92=_8e.cells[_91*_8e.subColumnCount].parentNode.parentNode;
if(_92!=_90.parentNode){
if(_90.parentNode!=null){
_90.parentNode.removeChild(_90);
}
_92.appendChild(_90);
}
if(this._timeIndicatorTimer==null){
this._timeIndicatorTimer=setInterval(_7.hitch(this,function(){
this._layoutTimeIndicator(this.renderData);
}),this.timeIndicatorRefreshInterval);
}
return;
}
}
}
if(this._timeIndicatorTimer){
clearInterval(this._timeIndicatorTimer);
this._timeIndicatorTimer=null;
}
if(this._timeIndicator){
_d.set(this._timeIndicator,"display","none");
}
},beforeDeactivate:function(){
if(this._timeIndicatorTimer){
clearInterval(this._timeIndicatorTimer);
this._timeIndicatorTimer=null;
}
},_overlapLayoutPass2:function(_93){
var i,j,_94,_95;
_94=_93[_93.length-1];
for(j=0;j<_94.length;j++){
_94[j].extent=1;
}
for(i=0;i<_93.length-1;i++){
_94=_93[i];
for(var j=0;j<_94.length;j++){
_95=_94[j];
if(_95.extent==-1){
_95.extent=1;
var _96=0;
var _97=false;
for(var k=i+1;k<_93.length&&!_97;k++){
var _98=_93[k];
for(var l=0;l<_98.length&&!_97;l++){
var _99=_98[l];
if(_95.start<_99.end&&_99.start<_95.end){
_97=true;
}
}
if(!_97){
_96++;
}
}
_95.extent+=_96;
}
}
}
},_defaultItemToRendererKindFunc:function(_9a){
return "vertical";
},_layoutInterval:function(_9b,_9c,_9d,end,_9e,_9f){
var _a0=[];
_9b.colW=this.itemContainer.offsetWidth/_9b.columnCount;
if(_9f==="dataItems"){
for(var i=0;i<_9e.length;i++){
var _a1=_9e[i];
var _a2=this._itemToRendererKind(_a1);
if(_a2==="vertical"){
_a0.push(_a1);
}
}
this._layoutRendererWithSubColumns(_9b,"vertical",true,_9c,_9d,end,_a0,_9f);
}else{
this._layoutRendererWithSubColumns(_9b,"decoration",false,_9c,_9d,end,_9e,_9f);
}
},_layoutRendererWithSubColumns:function(_a3,_a4,_a5,_a6,_a7,end,_a8,_a9){
if(_a8.length>0){
if(_a3.subColumnCount>1){
var _aa={};
var _ab=this.subColumns;
_8.forEach(_ab,function(_ac){
_aa[_ac]=[];
});
_8.forEach(_a8,function(_ad){
if(_a9==="decorationItems"){
if(_ad.subColumn){
if(_aa[_ad.subColumn]){
_aa[_ad.subColumn].push(_ad);
}
}else{
_8.forEach(_ab,function(_ae){
var _af=_7.mixin({},_ad);
_af.subColumn=_ae;
_aa[_ae].push(_af);
});
}
}else{
if(_ad.subColumn&&_aa[_ad.subColumn]){
_aa[_ad.subColumn].push(_ad);
}
}
});
var _b0=0;
_8.forEach(this.subColumns,function(_b1){
this._layoutVerticalItems(_a3,_a4,_a5,_a6,_b0++,_a7,end,_aa[_b1],_a9);
},this);
}else{
this._layoutVerticalItems(_a3,_a4,_a5,_a6,0,_a7,end,_a8,_a9);
}
}
},_getColumn:function(_b2,_b3,_b4,_b5){
var _b6=_b5==="dataItems"?_b2.cells:_b2.decorationCells;
return _b6[_b3*_b2.subColumnCount+_b4];
},_layoutVerticalItems:function(_b7,_b8,_b9,_ba,_bb,_bc,_bd,_be,_bf){
if(_bf==="dataItems"&&this.verticalRenderer==null||_bf==="decorationItems"&&this.verticalDecorationRenderer==null){
return;
}
var _c0=this._getColumn(_b7,_ba,_bb,_bf);
var _c1=[];
for(var i=0;i<_be.length;i++){
var _c2=_be[i];
var _c3=this.computeRangeOverlap(_b7,_c2.startTime,_c2.endTime,_bc,_bd);
var top=this.computeProjectionOnDate(_b7,_bc,_c3[0],_b7.sheetHeight);
var _c4=this.computeProjectionOnDate(_b7,_bc,_c3[1],_b7.sheetHeight);
if(_c4>top){
var _c5=_7.mixin({start:top,end:_c4,range:_c3,item:_c2},_c2);
_c1.push(_c5);
}
}
var _c6=_bf==="dataItems"?this.computeOverlapping(_c1,this._overlapLayoutPass2).numLanes:1;
var _c7=this.percentOverlap/100;
for(i=0;i<_c1.length;i++){
_c2=_c1[i];
var w,_c8,ir,_c9;
if(_bf==="dataItems"){
var _ca=_c2.lane;
var _cb=_c2.extent;
if(_c7==0){
w=_c6==1?_b7.colW:((_b7.colW-(_c6-1)*this.horizontalGap)/_c6);
_c8=_ca*(w+this.horizontalGap);
w=_cb==1?w:w*_cb+(_cb-1)*this.horizontalGap;
w=100*w/_b7.colW;
_c8=100*_c8/_b7.colW;
}else{
w=_c6==1?100:(100/(_c6-(_c6-1)*_c7));
_c8=_ca*(w-_c7*w);
w=_cb==1?w:w*(_cb-(_cb-1)*_c7);
}
ir=this._createRenderer(_c2,"vertical",this.verticalRenderer,"dojoxCalendarVertical");
var _cc=this.isItemBeingEdited(_c2);
var _cd=this.isItemSelected(_c2);
var _ce=this.isItemHovered(_c2);
var _cf=this.isItemFocused(_c2);
_c9=ir.renderer;
_c9.set("hovered",_ce);
_c9.set("selected",_cd);
_c9.set("edited",_cc);
_c9.set("focused",this.showFocus?_cf:false);
_c9.set("storeState",this.getItemStoreState(_c2));
_c9.set("moveEnabled",this.isItemMoveEnabled(_c2._item,"vertical"));
_c9.set("resizeEnabled",this.isItemResizeEnabled(_c2._item,"vertical"));
this.applyRendererZIndex(_c2,ir,_ce,_cd,_cc,_cf);
}else{
w=100;
_c8=0;
ir=this.decorationRendererManager.createRenderer(_c2,"vertical",this.verticalDecorationRenderer,"dojoxCalendarDecoration");
_c9=ir.renderer;
}
_d.set(ir.container,{"top":_c2.start+"px","left":_c8+"%","width":w+"%","height":(_c2.end-_c2.start+1)+"px"});
if(_c9.updateRendering){
_c9.updateRendering(w,_c2.end-_c2.start+1);
}
_f.place(ir.container,_c0);
_d.set(ir.container,"display","block");
}
},_sortItemsFunction:function(a,b){
var res=this.dateModule.compare(a.startTime,b.startTime);
if(res==0){
res=-1*this.dateModule.compare(a.endTime,b.endTime);
}
return this.isLeftToRight()?res:-res;
},_getNormalizedCoords:function(e,x,y,_d0){
if(e!=null){
var _d1=_e.position(this.itemContainer,true);
if(e.touches){
_d0=_d0==undefined?0:_d0;
x=e.touches[_d0].pageX-_d1.x;
y=e.touches[_d0].pageY-_d1.y;
}else{
x=e.pageX-_d1.x;
y=e.pageY-_d1.y;
}
}
var r=_e.getContentBox(this.itemContainer);
if(!this.isLeftToRight()){
x=r.w-x;
}
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
return {x:x,y:y};
},getTime:function(e,x,y,_d2){
var o=this._getNormalizedCoords(e,x,y,_d2);
var t=this.getTimeOfDay(o.y,this.renderData);
var _d3=_e.getMarginBox(this.itemContainer).w/this.renderData.columnCount;
var col=Math.floor(o.x/_d3);
var _d4=null;
if(col<this.renderData.dates.length){
_d4=this.newDate(this.renderData.dates[col]);
_d4=this.floorToDay(_d4,true);
_d4.setHours(t.hours);
_d4.setMinutes(t.minutes);
}
return _d4;
},getSubColumn:function(e,x,y,_d5){
if(this.subColumns==null||this.subColumns.length==1){
return null;
}
var o=this._getNormalizedCoords(e,x,y,_d5);
var rd=this.renderData;
var _d6=_e.getMarginBox(this.itemContainer).w/this.renderData.columnCount;
var col=Math.floor(o.x/_d6);
var idx=Math.floor((o.x-col*_d6)/(_d6/rd.subColumnCount));
return this.subColumns[idx];
},_onGridMouseUp:function(e){
this.inherited(arguments);
if(this._gridMouseDown){
this._gridMouseDown=false;
this._onGridClick({date:this.getTime(e),triggerEvent:e});
}
},_onGridTouchStart:function(e){
this.inherited(arguments);
var g=this._gridProps;
g.moved=false;
g.start=e.touches[0].screenY;
g.scrollTop=this._getScrollPosition();
},_onGridTouchMove:function(e){
this.inherited(arguments);
if(e.touches.length>1&&!this._isEditing){
_6.stop(e);
return;
}
if(this._gridProps&&!this._isEditing){
var _d7={x:e.touches[0].screenX,y:e.touches[0].screenY};
var p=this._edProps;
if(!p||p&&(Math.abs(_d7.x-p.start.x)>25||Math.abs(_d7.y-p.start.y)>25)){
this._gridProps.moved=true;
var d=e.touches[0].screenY-this._gridProps.start;
var _d8=this._gridProps.scrollTop-d;
var max=this.itemContainer.offsetHeight-this.scrollContainer.offsetHeight;
if(_d8<0){
this._gridProps.start=e.touches[0].screenY;
this._setScrollImpl(0);
this._gridProps.scrollTop=0;
}else{
if(_d8>max){
this._gridProps.start=e.touches[0].screenY;
this._setScrollImpl(max);
this._gridProps.scrollTop=max;
}else{
this._setScrollImpl(_d8);
}
}
}
}
},_onGridTouchEnd:function(e){
this.inherited(arguments);
var g=this._gridProps;
if(g){
if(!this._isEditing){
if(!g.moved){
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
this._pendingDoubleTap={grid:true,timer:setTimeout(_7.hitch(this,function(){
delete this._pendingDoubleTap;
}),this.doubleTapDelay)};
}
}
}
}
this._gridProps=null;
}
},_onColumnHeaderClick:function(e){
this._dispatchCalendarEvt(e,"onColumnHeaderClick");
},onColumnHeaderClick:function(e){
},getTimeOfDay:function(pos,rd){
var _d9=rd.minHours*60;
var _da=rd.maxHours*60;
var _db=_d9+(pos*(_da-_d9)/rd.sheetHeight);
return {hours:Math.floor(_db/60),minutes:Math.floor(_db%60)};
},_isItemInView:function(_dc){
var res=this.inherited(arguments);
if(res){
var rd=this.renderData;
var len=rd.dateModule.difference(_dc.startTime,_dc.endTime,"millisecond");
var _dd=(24-rd.maxHours+rd.minHours)*3600000;
if(len>_dd){
return true;
}
var _de=_dc.startTime.getHours()*60+_dc.startTime.getMinutes();
var _df=_dc.endTime.getHours()*60+_dc.endTime.getMinutes();
var sV=rd.minHours*60;
var eV=rd.maxHours*60;
if(_de>0&&_de<sV||_de>eV&&_de<=1440){
return false;
}
if(_df>0&&_df<sV||_df>eV&&_df<=1440){
return false;
}
}
return res;
},_ensureItemInView:function(_e0){
var _e1;
var _e2=_e0.startTime;
var _e3=_e0.endTime;
var rd=this.renderData;
var cal=rd.dateModule;
var len=Math.abs(cal.difference(_e0.startTime,_e0.endTime,"millisecond"));
var _e4=(24-rd.maxHours+rd.minHours)*3600000;
if(len>_e4){
return false;
}
var _e5=_e2.getHours()*60+_e2.getMinutes();
var _e6=_e3.getHours()*60+_e3.getMinutes();
var sV=rd.minHours*60;
var eV=rd.maxHours*60;
if(_e5>0&&_e5<sV){
this.floorToDay(_e0.startTime,true,rd);
_e0.startTime.setHours(rd.minHours);
_e0.endTime=cal.add(_e0.startTime,"millisecond",len);
_e1=true;
}else{
if(_e5>eV&&_e5<=1440){
this.floorToDay(_e0.startTime,true,rd);
_e0.startTime=cal.add(_e0.startTime,"day",1);
_e0.startTime.setHours(rd.minHours);
_e0.endTime=cal.add(_e0.startTime,"millisecond",len);
_e1=true;
}
}
if(_e6>0&&_e6<sV){
this.floorToDay(_e0.endTime,true,rd);
_e0.endTime=cal.add(_e0.endTime,"day",-1);
_e0.endTime.setHours(rd.maxHours);
_e0.startTime=cal.add(_e0.endTime,"millisecond",-len);
_e1=true;
}else{
if(_e6>eV&&_e6<=1440){
this.floorToDay(_e0.endTime,true,rd);
_e0.endTime.setHours(rd.maxHours);
_e0.startTime=cal.add(_e0.endTime,"millisecond",-len);
_e1=true;
}
}
_e1=_e1||this.inherited(arguments);
return _e1;
},_onScrollTimer_tick:function(){
this._scrollToPosition(this._getScrollPosition()+this._scrollProps.scrollStep);
},snapUnit:"minute",snapSteps:15,minDurationUnit:"minute",minDurationSteps:15,liveLayout:false,stayInView:true,allowStartEndSwap:true,allowResizeLessThan24H:true});
});
