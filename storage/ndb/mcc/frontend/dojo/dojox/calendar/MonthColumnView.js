//>>built
require({cache:{"url:dojox/calendar/templates/MonthColumnView.html":"<div data-dojo-attach-events=\"keydown:_onKeyDown\">\t\t\n\t<div data-dojo-attach-point=\"columnHeader\" class=\"dojoxCalendarColumnHeader\">\n\t\t<table data-dojo-attach-point=\"columnHeaderTable\" class=\"dojoxCalendarColumnHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t</div>\t\n\t<div data-dojo-attach-point=\"vScrollBar\" class=\"dojoxCalendarVScrollBar\">\n\t\t<div data-dojo-attach-point=\"vScrollBarContent\" style=\"visibility:hidden;position:relative; width:1px; height:1px;\" ></div>\n\t</div>\t\n\t<div data-dojo-attach-point=\"scrollContainer\" class=\"dojoxCalendarScrollContainer\">\n\t\t<div data-dojo-attach-point=\"sheetContainer\" style=\"position:relative;left:0;right:0;margin:0;padding:0\">\t\t\t\n\t\t\t<div data-dojo-attach-point=\"grid\" class=\"dojoxCalendarGrid\">\n\t\t\t\t<table data-dojo-attach-point=\"gridTable\" class=\"dojoxCalendarGridTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t\t\t</div>\n\t\t\t<div data-dojo-attach-point=\"itemContainer\" class=\"dojoxCalendarContainer\" data-dojo-attach-event=\"mousedown:_onGridMouseDown,mouseup:_onGridMouseUp,ondblclick:_onGridDoubleClick,touchstart:_onGridTouchStart,touchmove:_onGridTouchMove,touchend:_onGridTouchEnd\">\n\t\t\t\t<table data-dojo-attach-point=\"itemContainerTable\" class=\"dojoxCalendarContainerTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t\t\t</div>\n\t\t</div> \n\t</div>\t\n</div>\n"}});
define("dojox/calendar/MonthColumnView",["./ViewBase","dijit/_TemplatedMixin","./_VerticalScrollBarBase","dojo/text!./templates/MonthColumnView.html","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/array","dojo/_base/sniff","dojo/_base/fx","dojo/_base/html","dojo/on","dojo/dom","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/dom-construct","dojo/mouse","dojo/query","dojo/i18n","dojox/html/metrics"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,fx,_a,on,_b,_c,_d,_e,_f,_10,_11,_12,_13){
return _5("dojox.calendar.MonthColumnView",[_1,_2],{baseClass:"dojoxCalendarMonthColumnView",templateString:_4,viewKind:"monthColumns",_setTabIndexAttr:"domNode",renderData:null,startDate:null,columnCount:6,daySize:30,showCellLabel:true,showHiddenItems:true,verticalRenderer:null,percentOverlap:0,horizontalGap:4,columnHeaderFormatLength:null,gridCellDatePattern:null,roundToDay:true,_layoutUnit:"month",_columnHeaderHandlers:null,constructor:function(){
this.invalidatingProperties=["columnCount","startDate","daySize","percentOverlap","verticalRenderer","columnHeaderDatePattern","horizontalGap","scrollBarRTLPosition","itemToRendererKindFunc","layoutPriorityFunction","textDir","items","showCellLabel","showHiddenItems"];
this._columnHeaderHandlers=[];
},postCreate:function(){
this.inherited(arguments);
this.keyboardUpDownUnit="day";
this.keyboardUpDownSteps=1;
this.keyboardLeftRightUnit="month";
this.keyboardLeftRightSteps=1;
this.allDayKeyboardUpDownUnit="day";
this.allDayKeyboardUpDownSteps=1;
this.allDayKeyboardLeftRightUnit="month";
this.allDayKeyboardLeftRightSteps=1;
},destroy:function(_14){
this._cleanupColumnHeader();
if(this.scrollBar){
this.scrollBar.destroy(_14);
}
this.inherited(arguments);
},_scrollBar_onScroll:function(_15){
this.scrollContainer.scrollTop=_15;
},buildRendering:function(){
this.inherited(arguments);
if(this.vScrollBar){
this.scrollBar=new _3({content:this.vScrollBarContent},this.vScrollBar);
this.scrollBar.on("scroll",_7.hitch(this,this._scrollBar_onScroll));
this._viewHandles.push(on(this.scrollContainer,_10.wheel,dojo.hitch(this,this._mouseWheelScrollHander)));
}
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
rd.daySize=this.get("daySize");
rd.scrollbarWidth=_13.getScrollbar().w+1;
rd.dateLocaleModule=this.dateLocaleModule;
rd.dateClassObj=this.dateClassObj;
rd.dateModule=this.dateModule;
rd.dates=[];
rd.columnCount=this.get("columnCount");
var d=this.get("startDate");
if(d==null){
d=new rd.dateClassObj();
}
d=this.floorToMonth(d,false,rd);
this.startDate=d;
var _17=d.getMonth();
var _18=0;
for(var col=0;col<rd.columnCount;col++){
var _19=[];
rd.dates.push(_19);
while(d.getMonth()==_17){
_19.push(d);
d=rd.dateModule.add(d,"day",1);
d=this.floorToDay(d,false,rd);
}
_17=d.getMonth();
if(_18<_19.length){
_18=_19.length;
}
}
rd.startTime=new rd.dateClassObj(rd.dates[0][0]);
rd.endTime=new rd.dateClassObj(_19[_19.length-1]);
rd.endTime=rd.dateModule.add(rd.endTime,"day",1);
rd.maxDayCount=_18;
rd.sheetHeight=rd.daySize*_18;
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
return rd;
},_validateProperties:function(){
this.inherited(arguments);
if(this.columnCount<1||isNaN(this.columnCount)){
this.columnCount=1;
}
if(this.daySize<5||isNaN(this.daySize)){
this.daySize=5;
}
},_setStartDateAttr:function(_1a){
this.displayedItemsInvalidated=true;
this._set("startDate",_1a);
},_setColumnCountAttr:function(_1b){
this.displayedItemsInvalidated=true;
this._set("columnCount",_1b);
},__fixEvt:function(e){
e.sheet="primary";
e.source=this;
return e;
},_formatColumnHeaderLabel:function(d){
var len="wide";
if(this.columnHeaderFormatLength){
len=this.columnHeaderFormatLength;
}
var _1c=this.renderData.dateLocaleModule.getNames("months",len,"standAlone");
return _1c[d.getMonth()];
},_formatGridCellLabel:function(d,row,col){
var _1d,rb;
if(d==null){
return "";
}
if(this.gridCellPattern){
return this.renderData.dateLocaleModule.format(d,{selector:"date",datePattern:this.gridCellDatePattern});
}else{
rb=_12.getLocalization("dojo.cldr",this._calendar);
_1d=rb["dateFormatItem-d"];
var _1e=this.renderData.dateLocaleModule.getNames("days","abbr","standAlone");
return _1e[d.getDay()].substring(0,1)+" "+this.renderData.dateLocaleModule.format(d,{selector:"date",datePattern:_1d});
}
},scrollPosition:null,scrollBarRTLPosition:"left",_setScrollPositionAttr:function(_1f){
this._setScrollPosition(_1f.date,_1f.duration,_1f.easing);
},_getScrollPositionAttr:function(){
return {date:(this.scrollContainer.scrollTop/this.daySize)+1};
},_setScrollPosition:function(_20,_21,_22){
if(_20<1){
_20=1;
}else{
if(_20>31){
_20=31;
}
}
var _23=(_20-1)*this.daySize;
if(_21){
if(this._scrollAnimation){
this._scrollAnimation.stop();
}
var _24=Math.abs(((_23-this.scrollContainer.scrollTop)*_21)/this.renderData.sheetHeight);
this._scrollAnimation=new fx.Animation({curve:[this.scrollContainer.scrollTop,_23],duration:_24,easing:_22,onAnimate:_7.hitch(this,function(_25){
this._setScrollImpl(_25);
})});
this._scrollAnimation.play();
}else{
this._setScrollImpl(_23);
}
},_setScrollImpl:function(v){
this.scrollContainer.scrollTop=v;
if(this.scrollBar){
this.scrollBar.set("value",v);
}
},ensureVisibility:function(_26,end,_27,_28,_29){
_28=_28==undefined?1:_28;
if(this.scrollable&&this.autoScroll){
var s=_26.getDate()-_28;
if(this.isStartOfDay(end)){
end=this._waDojoxAddIssue(end,"day",-1);
}
var e=end.getDate()+_28;
var _2a=this.get("scrollPosition").date;
var r=_e.getContentBox(this.scrollContainer);
var _2b=(this.get("scrollPosition").date+(r.h/this.daySize));
var _2c=false;
var _2d=null;
switch(_27){
case "start":
_2c=s>=_2a&&s<=_2b;
_2d=s;
break;
case "end":
_2c=e>=_2a&&e<=_2b;
_2d=e-(_2b-_2a);
break;
case "both":
_2c=s>=_2a&&e<=_2b;
_2d=s;
break;
}
if(!_2c){
this._setScrollPosition(_2d,_29);
}
}
},scrollView:function(dir){
var pos=this.get("scrollPosition").date+dir;
this._setScrollPosition(pos);
},_mouseWheelScrollHander:function(e){
this.scrollView(e.wheelDelta>0?-1:1);
},refreshRendering:function(){
if(!this._initialized){
return;
}
this._validateProperties();
var _2e=this.renderData;
var rd=this._createRenderData();
this.renderData=rd;
this._createRendering(rd,_2e);
this._layoutRenderers(rd);
},_createRendering:function(_2f,_30){
_d.set(this.sheetContainer,"height",_2f.sheetHeight+"px");
this._configureScrollBar(_2f);
this._buildColumnHeader(_2f,_30);
this._buildGrid(_2f,_30);
this._buildItemContainer(_2f,_30);
},_configureScrollBar:function(_31){
if(_9("ie")&&this.scrollBar){
_d.set(this.scrollBar.domNode,"width",(_31.scrollbarWidth+1)+"px");
}
var _32=this.isLeftToRight()?true:this.scrollBarRTLPosition=="right";
var _33=_32?"right":"left";
var _34=_32?"left":"right";
if(this.scrollBar){
this.scrollBar.set("maximum",_31.sheetHeight);
_d.set(this.scrollBar.domNode,_33,0);
_d.set(this.scrollBar.domNode,_34,"auto");
}
_d.set(this.scrollContainer,_33,_31.scrollbarWidth+"px");
_d.set(this.scrollContainer,_34,"0");
_d.set(this.columnHeader,_33,_31.scrollbarWidth+"px");
_d.set(this.columnHeader,_34,"0");
if(this.buttonContainer&&this.owner!=null&&this.owner.currentView==this){
_d.set(this.buttonContainer,_33,_31.scrollbarWidth+"px");
_d.set(this.buttonContainer,_34,"0");
}
},_columnHeaderClick:function(e){
_6.stop(e);
var _35=_11("td",this.columnHeaderTable).indexOf(e.currentTarget);
this._onColumnHeaderClick({index:_35,date:this.renderData.dates[_35][0],triggerEvent:e});
},_buildColumnHeader:function(_36,_37){
var _38=this.columnHeaderTable;
if(!_38){
return;
}
var _39=_36.columnCount-(_37?_37.columnCount:0);
if(_9("ie")==8){
if(this._colTableSave==null){
this._colTableSave=_7.clone(_38);
}else{
if(_39<0){
this._cleanupColumnHeader();
this.columnHeader.removeChild(_38);
_f.destroy(_38);
_38=_7.clone(this._colTableSave);
this.columnHeaderTable=_38;
this.columnHeader.appendChild(_38);
_39=_36.columnCount;
}
}
}
var _3a=_11("tbody",_38);
var trs=_11("tr",_38);
var _3b,tr,td;
if(_3a.length==1){
_3b=_3a[0];
}else{
_3b=_a.create("tbody",null,_38);
}
if(trs.length==1){
tr=trs[0];
}else{
tr=_f.create("tr",null,_3b);
}
if(_39>0){
for(var i=0;i<_39;i++){
td=_f.create("td",null,tr);
var h=[];
h.push(on(td,"click",_7.hitch(this,this._columnHeaderClick)));
if(_9("touch")){
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
_39=-_39;
for(var i=0;i<_39;i++){
td=tr.lastChild;
tr.removeChild(td);
_f.destroy(td);
var _3c=this._columnHeaderHandlers.pop();
while(_3c.length>0){
_3c.pop().remove();
}
}
}
_11("td",_38).forEach(function(td,i){
td.className="";
if(i==0){
_c.add(td,"first-child");
}else{
if(i==this.renderData.columnCount-1){
_c.add(td,"last-child");
}
}
var d=_36.dates[i][0];
this._setText(td,this._formatColumnHeaderLabel(d));
this.styleColumnHeaderCell(td,d,_36);
},this);
},_cleanupColumnHeader:function(){
while(this._columnHeaderHandlers.length>0){
var _3d=this._columnHeaderHandlers.pop();
while(_3d.length>0){
_3d.pop().remove();
}
}
},styleColumnHeaderCell:function(_3e,_3f,_40){
},_buildGrid:function(_41,_42){
var _43=this.gridTable;
if(!_43){
return;
}
_d.set(_43,"height",_41.sheetHeight+"px");
var _44=_41.maxDayCount-(_42?_42.maxDayCount:0);
var _45=_44>0;
var _46=_41.columnCount-(_42?_42.columnCount:0);
if(_9("ie")==8){
if(this._gridTableSave==null){
this._gridTableSave=_7.clone(_43);
}else{
if(_46<0){
this.grid.removeChild(_43);
_f.destroy(_43);
_43=_7.clone(this._gridTableSave);
this.gridTable=_43;
this.grid.appendChild(_43);
_46=_41.columnCount;
_44=_41.maxDayCount;
_45=true;
}
}
}
var _47=_11("tbody",_43);
var _48;
if(_47.length==1){
_48=_47[0];
}else{
_48=_f.create("tbody",null,_43);
}
if(_45){
for(var i=0;i<_44;i++){
_f.create("tr",null,_48);
}
}else{
_44=-_44;
for(var i=0;i<_44;i++){
_48.removeChild(_48.lastChild);
}
}
var _49=_41.maxDayCount-_44;
var _4a=_45||_46>0;
_46=_4a?_46:-_46;
_11("tr",_43).forEach(function(tr,i){
if(_4a){
var len=i>=_49?_41.columnCount:_46;
for(var i=0;i<len;i++){
var td=_f.create("td",null,tr);
_f.create("span",null,td);
}
}else{
for(var i=0;i<_46;i++){
tr.removeChild(tr.lastChild);
}
}
});
_11("tr",_43).forEach(function(tr,row){
tr.className="";
if(row==0){
_c.add(tr,"first-child");
}
if(row==_41.maxDayCount-1){
_c.add(tr,"last-child");
}
_11("td",tr).forEach(function(td,col){
td.className="";
if(col==0){
_c.add(td,"first-child");
}
if(col==_41.columnCount-1){
_c.add(td,"last-child");
}
var d=null;
if(row<_41.dates[col].length){
d=_41.dates[col][row];
}
var _4b=_11("span",td)[0];
this._setText(_4b,this.showCellLabel?this._formatGridCellLabel(d,row,col):null);
this.styleGridCell(td,d,col,row,_41);
},this);
},this);
},styleGridCell:function(_4c,_4d,col,row,_4e){
var cal=_4e.dateModule;
if(_4d==null){
return;
}
if(this.isToday(_4d)){
_c.add(_4c,"dojoxCalendarToday");
}else{
if(this.isWeekEnd(_4d)){
_c.add(_4c,"dojoxCalendarWeekend");
}
}
},_buildItemContainer:function(_4f,_50){
var _51=this.itemContainerTable;
if(!_51){
return;
}
var _52=[];
_d.set(_51,"height",_4f.sheetHeight+"px");
var _53=_4f.columnCount-(_50?_50.columnCount:0);
if(_9("ie")==8){
if(this._itemTableSave==null){
this._itemTableSave=_7.clone(_51);
}else{
if(_53<0){
this.itemContainer.removeChild(_51);
this._recycleItemRenderers(true);
_f.destroy(_51);
_51=_7.clone(this._itemTableSave);
this.itemContainerTable=_51;
this.itemContainer.appendChild(_51);
_53=_4f.columnCount;
}
}
}
var _54=_11("tbody",_51);
var trs=_11("tr",_51);
var _55,tr,td;
if(_54.length==1){
_55=_54[0];
}else{
_55=_f.create("tbody",null,_51);
}
if(trs.length==1){
tr=trs[0];
}else{
tr=_f.create("tr",null,_55);
}
if(_53>0){
for(var i=0;i<_53;i++){
td=_f.create("td",null,tr);
_f.create("div",{"className":"dojoxCalendarContainerColumn"},td);
}
}else{
_53=-_53;
for(var i=0;i<_53;i++){
tr.removeChild(tr.lastChild);
}
}
_11("td>div",_51).forEach(function(div,i){
_d.set(div,{"height":_4f.sheetHeight+"px"});
_52.push(div);
},this);
_4f.cells=_52;
},_overlapLayoutPass2:function(_56){
var i,j,_57,_58;
_57=_56[_56.length-1];
for(j=0;j<_57.length;j++){
_57[j].extent=1;
}
for(i=0;i<_56.length-1;i++){
_57=_56[i];
for(var j=0;j<_57.length;j++){
_58=_57[j];
if(_58.extent==-1){
_58.extent=1;
var _59=0;
var _5a=false;
for(var k=i+1;k<_56.length&&!_5a;k++){
var _5b=_56[k];
for(var l=0;l<_5b.length&&!_5a;l++){
var _5c=_5b[l];
if(_58.start<_5c.end&&_5c.start<_58.end){
_5a=true;
}
}
if(!_5a){
_59++;
}
}
_58.extent+=_59;
}
}
}
},_defaultItemToRendererKindFunc:function(_5d){
if(_5d.allDay){
return "vertical";
}
var dur=Math.abs(this.renderData.dateModule.difference(_5d.startTime,_5d.endTime,"minute"));
return dur>=1440?"vertical":null;
},_layoutRenderers:function(_5e){
this.hiddenEvents={};
this.inherited(arguments);
},_layoutInterval:function(_5f,_60,_61,end,_62){
var _63=[];
var _64=[];
_5f.colW=this.itemContainer.offsetWidth/_5f.columnCount;
for(var i=0;i<_62.length;i++){
var _65=_62[i];
if(this._itemToRendererKind(_65)=="vertical"){
_63.push(_65);
}else{
if(this.showHiddenItems){
_64.push(_65);
}
}
}
if(_63.length>0){
this._layoutVerticalItems(_5f,_60,_61,end,_63);
}
if(_64.length>0){
this._layoutBgItems(_5f,_60,_61,end,_64);
}
},_dateToYCoordinate:function(_66,d,_67){
var pos=0;
if(_67||d.getHours()!=0||d.getMinutes()!=0){
pos=(d.getDate()-1)*this.renderData.daySize;
}else{
var d2=this._waDojoxAddIssue(d,"day",-1);
pos=this.renderData.daySize+((d2.getDate()-1)*this.renderData.daySize);
}
pos+=(d.getHours()*60+d.getMinutes())*this.renderData.daySize/1440;
return pos;
},_layoutVerticalItems:function(_68,_69,_6a,_6b,_6c){
if(this.verticalRenderer==null){
return;
}
var _6d=_68.cells[_69];
var _6e=[];
for(var i=0;i<_6c.length;i++){
var _6f=_6c[i];
var _70=this.computeRangeOverlap(_68,_6f.startTime,_6f.endTime,_6a,_6b);
var top=this._dateToYCoordinate(_68,_70[0],true);
var _71=this._dateToYCoordinate(_68,_70[1],false);
if(_71>top){
var _72=_7.mixin({start:top,end:_71,range:_70,item:_6f},_6f);
_6e.push(_72);
}
}
var _73=this.computeOverlapping(_6e,this._overlapLayoutPass2).numLanes;
var _74=this.percentOverlap/100;
for(i=0;i<_6e.length;i++){
_6f=_6e[i];
var _75=_6f.lane;
var _76=_6f.extent;
var w;
var _77;
if(_74==0){
w=_73==1?_68.colW:((_68.colW-(_73-1)*this.horizontalGap)/_73);
_77=_75*(w+this.horizontalGap);
w=_76==1?w:w*_76+(_76-1)*this.horizontalGap;
w=100*w/_68.colW;
_77=100*_77/_68.colW;
}else{
w=_73==1?100:(100/(_73-(_73-1)*_74));
_77=_75*(w-_74*w);
w=_76==1?w:w*(_76-(_76-1)*_74);
}
var ir=this._createRenderer(_6f,"vertical",this.verticalRenderer,"dojoxCalendarVertical");
_d.set(ir.container,{"top":_6f.start+"px","left":_77+"%","width":w+"%","height":(_6f.end-_6f.start+1)+"px"});
var _78=this.isItemBeingEdited(_6f);
var _79=this.isItemSelected(_6f);
var _7a=this.isItemHovered(_6f);
var _7b=this.isItemFocused(_6f);
var _7c=ir.renderer;
_7c.set("hovered",_7a);
_7c.set("selected",_79);
_7c.set("edited",_78);
_7c.set("focused",this.showFocus?_7b:false);
_7c.set("moveEnabled",this.isItemMoveEnabled(_6f,"vertical"));
_7c.set("resizeEnabled",this.isItemResizeEnabled(_6f,"vertical"));
this.applyRendererZIndex(_6f,ir,_7a,_79,_78,_7b);
if(_7c.updateRendering){
_7c.updateRendering(w,_6f.end-_6f.start+1);
}
_f.place(ir.container,_6d);
_d.set(ir.container,"display","block");
}
},_getCellAt:function(_7d,_7e,rtl){
if((rtl==undefined||rtl==true)&&!this.isLeftToRight()){
_7e=this.renderData.columnCount-1-_7e;
}
return this.gridTable.childNodes[0].childNodes[_7d].childNodes[_7e];
},invalidateLayout:function(){
_11("td",this.gridTable).forEach(function(td){
_c.remove(td,"dojoxCalendarHiddenEvents");
});
this.inherited(arguments);
},_layoutBgItems:function(_7f,col,_80,_81,_82){
var _83={};
for(var i=0;i<_82.length;i++){
var _84=_82[i];
var _85=this.computeRangeOverlap(_7f,_84.startTime,_84.endTime,_80,_81);
var _86=_85[0].getDate()-1;
var end;
if(this.isStartOfDay(_85[1])){
end=this._waDojoxAddIssue(_85[1],"day",-1);
end=end.getDate()-1;
}else{
end=_85[1].getDate()-1;
}
for(var d=_86;d<=end;d++){
_83[d]=true;
}
}
for(var row in _83){
if(_83[row]){
var _87=this._getCellAt(row,col,false);
_c.add(_87,"dojoxCalendarHiddenEvents");
}
}
},_sortItemsFunction:function(a,b){
var res=this.dateModule.compare(a.startTime,b.startTime);
if(res==0){
res=-1*this.dateModule.compare(a.endTime,b.endTime);
}
return this.isLeftToRight()?res:-res;
},getTime:function(e,x,y,_88){
if(e!=null){
var _89=_e.position(this.itemContainer,true);
if(e.touches){
_88=_88==undefined?0:_88;
x=e.touches[_88].pageX-_89.x;
y=e.touches[_88].pageY-_89.y;
}else{
x=e.pageX-_89.x;
y=e.pageY-_89.y;
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
var col=Math.floor(x/(r.w/this.renderData.columnCount));
var row=Math.floor(y/(r.h/this.renderData.maxDayCount));
var _8a=null;
if(col<this.renderData.dates.length&&row<this.renderData.dates[col].length){
_8a=this.newDate(this.renderData.dates[col][row]);
}
return _8a;
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
g.scrollTop=this.scrollContainer.scrollTop;
},_onGridTouchMove:function(e){
this.inherited(arguments);
if(e.touches.length>1&&!this._isEditing){
_6.stop(e);
return;
}
if(this._gridProps&&!this._isEditing){
var _8b={x:e.touches[0].screenX,y:e.touches[0].screenY};
var p=this._edProps;
if(!p||p&&(Math.abs(_8b.x-p.start.x)>25||Math.abs(_8b.y-p.start.y)>25)){
this._gridProps.moved=true;
var d=e.touches[0].screenY-this._gridProps.start;
var _8c=this._gridProps.scrollTop-d;
var max=this.itemContainer.offsetHeight-this.scrollContainer.offsetHeight;
if(_8c<0){
this._gridProps.start=e.touches[0].screenY;
this._setScrollImpl(0);
this._gridProps.scrollTop=0;
}else{
if(_8c>max){
this._gridProps.start=e.touches[0].screenY;
this._setScrollImpl(max);
this._gridProps.scrollTop=max;
}else{
this._setScrollImpl(_8c);
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
},_onScrollTimer_tick:function(){
this._setScrollImpl(this.scrollContainer.scrollTop+this._scrollProps.scrollStep);
},snapUnit:"day",snapSteps:1,minDurationUnit:"day",minDurationSteps:1,liveLayout:false,stayInView:true,allowStartEndSwap:true,allowResizeLessThan24H:false});
});
