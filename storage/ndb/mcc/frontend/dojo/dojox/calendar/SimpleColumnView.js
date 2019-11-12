//>>built
require({cache:{"url:dojox/calendar/templates/SimpleColumnView.html":"<div data-dojo-attach-events=\"keydown:_onKeyDown\">\t\n\t<div data-dojo-attach-point=\"header\" class=\"dojoxCalendarHeader\">\n\t\t<div class=\"dojoxCalendarYearColumnHeader\" data-dojo-attach-point=\"yearColumnHeader\">\n\t\t\t<table><tr><td><span data-dojo-attach-point=\"yearColumnHeaderContent\"></span></td></tr></table>\t\t\n\t\t</div>\n\t\t<div data-dojo-attach-point=\"columnHeader\" class=\"dojoxCalendarColumnHeader\">\n\t\t\t<table data-dojo-attach-point=\"columnHeaderTable\" class=\"dojoxCalendarColumnHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t\t</div>\n\t</div>\t\n\t<div data-dojo-attach-point=\"vScrollBar\" class=\"dojoxCalendarVScrollBar\">\n\t\t<div data-dojo-attach-point=\"vScrollBarContent\" style=\"visibility:hidden;position:relative; width:1px; height:1px;\" ></div>\n\t</div>\t\n\t<div data-dojo-attach-point=\"scrollContainer\" class=\"dojoxCalendarScrollContainer\">\n\t\t<div data-dojo-attach-point=\"sheetContainer\" style=\"position:relative;left:0;right:0;margin:0;padding:0\">\n\t\t\t<div data-dojo-attach-point=\"rowHeader\" class=\"dojoxCalendarRowHeader\">\n\t\t\t\t<table data-dojo-attach-point=\"rowHeaderTable\" class=\"dojoxCalendarRowHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t\t\t</div>\n\t\t\t<div data-dojo-attach-point=\"grid\" class=\"dojoxCalendarGrid\">\n\t\t\t\t<table data-dojo-attach-point=\"gridTable\" class=\"dojoxCalendarGridTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t\t\t</div>\n\t\t\t<div data-dojo-attach-point=\"itemContainer\" class=\"dojoxCalendarContainer\" data-dojo-attach-event=\"mousedown:_onGridMouseDown,mouseup:_onGridMouseUp,ondblclick:_onGridDoubleClick,touchstart:_onGridTouchStart,touchmove:_onGridTouchMove,touchend:_onGridTouchEnd\">\n\t\t\t\t<table data-dojo-attach-point=\"itemContainerTable\" class=\"dojoxCalendarContainerTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t\t\t</div>\n\t\t</div> \n\t</div>\n</div>\n\n"}});
define("dojox/calendar/SimpleColumnView",["./ViewBase","dijit/_TemplatedMixin","./_VerticalScrollBarBase","dojo/text!./templates/SimpleColumnView.html","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/array","dojo/_base/sniff","dojo/_base/fx","dojo/_base/html","dojo/on","dojo/dom","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/dom-construct","dojo/mouse","dojo/query","dojox/html/metrics"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,fx,_a,on,_b,_c,_d,_e,_f,_10,_11,_12){
return _5("dojox.calendar.SimpleColumnView",[_1,_2],{baseClass:"dojoxCalendarSimpleColumnView",templateString:_4,viewKind:"columns",_setTabIndexAttr:"domNode",renderData:null,startDate:null,columnCount:7,minHours:8,maxHours:18,hourSize:100,timeSlotDuration:15,verticalRenderer:null,percentOverlap:70,horizontalGap:4,_columnHeaderHandlers:null,constructor:function(){
this.invalidatingProperties=["columnCount","startDate","minHours","maxHours","hourSize","verticalRenderer","rowHeaderTimePattern","columnHeaderDatePattern","timeSlotDuration","percentOverlap","horizontalGap","scrollBarRTLPosition","itemToRendererKindFunc","layoutPriorityFunction","formatItemTimeFunc","textDir","items"];
this._columnHeaderHandlers=[];
},destroy:function(_13){
this._cleanupColumnHeader();
if(this.scrollBar){
this.scrollBar.destroy(_13);
}
this.inherited(arguments);
},_scrollBar_onScroll:function(_14){
this._setScrollPosition(_14);
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
},_setVerticalRendererAttr:function(_15){
this._destroyRenderersByKind("vertical");
this._set("verticalRenderer",_15);
},_createRenderData:function(){
var _16={};
_16.minHours=this.get("minHours");
_16.maxHours=this.get("maxHours");
_16.hourSize=this.get("hourSize");
_16.hourCount=_16.maxHours-_16.minHours;
_16.slotDuration=this.get("timeSlotDuration");
_16.slotSize=Math.ceil(_16.hourSize/(60/_16.slotDuration));
_16.hourSize=_16.slotSize*(60/_16.slotDuration);
_16.sheetHeight=_16.hourSize*_16.hourCount;
_16.scrollbarWidth=_12.getScrollbar().w+1;
_16.dateLocaleModule=this.dateLocaleModule;
_16.dateClassObj=this.dateClassObj;
_16.dateModule=this.dateModule;
_16.dates=[];
_16.columnCount=this.get("columnCount");
var d=this.get("startDate");
if(d==null){
d=new _16.dateClassObj();
}
d=this.floorToDay(d,false,_16);
this.startDate=d;
for(var col=0;col<_16.columnCount;col++){
_16.dates.push(d);
d=_16.dateModule.add(d,"day",1);
d=this.floorToDay(d,false,_16);
}
_16.startTime=new _16.dateClassObj(_16.dates[0]);
_16.startTime.setHours(_16.minHours);
_16.endTime=new _16.dateClassObj(_16.dates[_16.columnCount-1]);
_16.endTime.setHours(_16.maxHours);
if(this.displayedItemsInvalidated){
this.displayedItemsInvalidated=false;
this._computeVisibleItems(_16);
if(this._isEditing){
this._endItemEditing(null,false);
}
}else{
if(this.renderData){
_16.items=this.renderData.items;
}
}
return _16;
},_validateProperties:function(){
this.inherited(arguments);
var v=this.minHours;
if(v<0||v>24||isNaN(v)){
this.minHours=0;
}
v=this.maxHours;
if(v<0||v>24||isNaN(v)){
this.minHours=24;
}
if(this.minHours>this.maxHours){
var t=this.maxHours;
this.maxHours=this.minHours;
this.maxHours=t;
}
if(v-this.minHours<1){
this.minHours=0;
this.maxHours=24;
}
if(this.columnCount<1||isNaN(this.columnCount)){
this.columnCount=1;
}
v=this.percentOverlap;
if(this.percentOverlap<0||this.percentOverlap>100||isNaN(this.percentOverlap)){
this.percentOverlap=70;
}
if(this.hourSize<5||isNaN(this.hourSize)){
this.hourSize=10;
}
v=this.timeSlotDuration;
if(v<1||v>60||isNaN(v)){
v=15;
}
},_setStartDateAttr:function(_17){
this.displayedItemsInvalidated=true;
this._set("startDate",_17);
},_setColumnCountAttr:function(_18){
this.displayedItemsInvalidated=true;
this._set("columnCount",_18);
},__fixEvt:function(e){
e.sheet="primary";
e.source=this;
return e;
},_formatRowHeaderLabel:function(d){
return this.renderData.dateLocaleModule.format(d,{selector:"time",timePattern:this.rowHeaderTimePattern});
},_formatColumnHeaderLabel:function(d){
return this.renderData.dateLocaleModule.format(d,{selector:"date",datePattern:this.columnHeaderDatePattern,formatLength:"medium"});
},startTimeOfDay:null,scrollBarRTLPosition:"left",_getStartTimeOfDay:function(){
var v=(this.get("maxHours")-this.get("minHours"))*this._getScrollPosition()/this.renderData.sheetHeight;
return {hours:this.renderData.minHours+Math.floor(v),minutes:(v-Math.floor(v))*60};
},_getEndTimeOfDay:function(){
var v=(this.get("maxHours")-this.get("minHours"))*(this._getScrollPosition()+this.scrollContainer.offsetHeight)/this.renderData.sheetHeight;
return {hours:this.renderData.minHours+Math.floor(v),minutes:(v-Math.floor(v))*60};
},_setStartTimeOfDayAttr:function(_19){
this._setStartTimeOfDay(_19.hours,_19.minutes,_19.duration,_19.easing);
},_getStartTimeOfDayAttr:function(){
return this._getStartTimeOfDay();
},_setStartTimeOfDay:function(_1a,_1b,_1c,_1d){
var rd=this.renderData;
_1a=_1a||rd.minHours;
_1b=_1b||0;
_1c=_1c||0;
if(_1b<0){
_1b=0;
}else{
if(_1b>59){
_1b=59;
}
}
if(_1a<0){
_1a=0;
}else{
if(_1a>24){
_1a=24;
}
}
var _1e=_1a*60+_1b;
var _1f=rd.minHours*60;
var _20=rd.maxHours*60;
if(_1e<_1f){
_1e=_1f;
}else{
if(_1e>_20){
_1e=_20;
}
}
var pos=(_1e-_1f)*rd.sheetHeight/(_20-_1f);
pos=Math.min(rd.sheetHeight-this.scrollContainer.offsetHeight,pos);
this._scrollToPosition(pos,_1c,_1d);
},_scrollToPosition:function(_21,_22,_23){
if(_22){
if(this._scrollAnimation){
this._scrollAnimation.stop();
}
var _24=this._getScrollPosition();
var _25=Math.abs(((_21-_24)*_22)/this.renderData.sheetHeight);
this._scrollAnimation=new fx.Animation({curve:[_24,_21],duration:_25,easing:_23,onAnimate:_7.hitch(this,function(_26){
this._setScrollImpl(_26);
})});
this._scrollAnimation.play();
}else{
this._setScrollImpl(_21);
}
},_setScrollImpl:function(v){
this._setScrollPosition(v);
if(this.scrollBar){
this.scrollBar.set("value",v);
}
},ensureVisibility:function(_27,end,_28,_29,_2a){
_29=_29==undefined?this.renderData.slotDuration:_29;
if(this.scrollable&&this.autoScroll){
var s=_27.getHours()*60+_27.getMinutes()-_29;
var e=end.getHours()*60+end.getMinutes()+_29;
var vs=this._getStartTimeOfDay();
var ve=this._getEndTimeOfDay();
var _2b=vs.hours*60+vs.minutes;
var _2c=ve.hours*60+ve.minutes;
var _2d=false;
var _2e=null;
switch(_28){
case "start":
_2d=s>=_2b&&s<=_2c;
_2e=s;
break;
case "end":
_2d=e>=_2b&&e<=_2c;
_2e=e-(_2c-_2b);
break;
case "both":
_2d=s>=_2b&&e<=_2c;
_2e=s;
break;
}
if(!_2d){
this._setStartTimeOfDay(Math.floor(_2e/60),_2e%60,_2a);
}
}
},scrollView:function(dir){
var t=this._getStartTimeOfDay();
t=t.hours*60+t.minutes+(dir*this.timeSlotDuration);
this._setStartTimeOfDay(Math.floor(t/60),t%60);
},_mouseWheelScrollHander:function(e){
this.scrollView(e.wheelDelta>0?-1:1);
},refreshRendering:function(){
if(!this._initialized){
return;
}
this._validateProperties();
var _2f=this.renderData;
var rd=this._createRenderData();
this.renderData=rd;
this._createRendering(rd,_2f);
this._layoutRenderers(rd);
},_createRendering:function(_30,_31){
_d.set(this.sheetContainer,"height",_30.sheetHeight+"px");
this._configureScrollBar(_30);
this._buildColumnHeader(_30,_31);
this._buildRowHeader(_30,_31);
this._buildGrid(_30,_31);
this._buildItemContainer(_30,_31);
},_configureScrollBar:function(_32){
if(_9("ie")&&this.scrollBar){
_d.set(this.scrollBar.domNode,"width",(_32.scrollbarWidth+1)+"px");
}
var _33=this.isLeftToRight()?true:this.scrollBarRTLPosition=="right";
var _34=_33?"right":"left";
var _35=_33?"left":"right";
if(this.scrollBar){
this.scrollBar.set("maximum",_32.sheetHeight);
_d.set(this.scrollBar.domNode,_34,0);
_d.set(this.scrollBar.domNode,_33?"left":"right","auto");
}
_d.set(this.scrollContainer,_34,_32.scrollbarWidth+"px");
_d.set(this.scrollContainer,_35,"0");
_d.set(this.header,_34,_32.scrollbarWidth+"px");
_d.set(this.header,_35,"0");
if(this.buttonContainer&&this.owner!=null&&this.owner.currentView==this){
_d.set(this.buttonContainer,_34,_32.scrollbarWidth+"px");
_d.set(this.buttonContainer,_35,"0");
}
},_columnHeaderClick:function(e){
_6.stop(e);
var _36=_11("td",this.columnHeaderTable).indexOf(e.currentTarget);
this._onColumnHeaderClick({index:_36,date:this.renderData.dates[_36],triggerEvent:e});
},_buildColumnHeader:function(_37,_38){
var _39=this.columnHeaderTable;
if(!_39){
return;
}
var _3a=_37.columnCount-(_38?_38.columnCount:0);
if(_9("ie")==8){
if(this._colTableSave==null){
this._colTableSave=_7.clone(_39);
}else{
if(_3a<0){
this._cleanupColumnHeader();
this.columnHeader.removeChild(_39);
_f.destroy(_39);
_39=_7.clone(this._colTableSave);
this.columnHeaderTable=_39;
this.columnHeader.appendChild(_39);
_3a=_37.columnCount;
}
}
}
var _3b=_11("tbody",_39);
var trs=_11("tr",_39);
var _3c,tr,td;
if(_3b.length==1){
_3c=_3b[0];
}else{
_3c=_a.create("tbody",null,_39);
}
if(trs.length==1){
tr=trs[0];
}else{
tr=_f.create("tr",null,_3c);
}
if(_3a>0){
for(var i=0;i<_3a;i++){
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
_3a=-_3a;
for(var i=0;i<_3a;i++){
td=tr.lastChild;
tr.removeChild(td);
_f.destroy(td);
var _3d=this._columnHeaderHandlers.pop();
while(_3d.length>0){
_3d.pop().remove();
}
}
}
_11("td",_39).forEach(function(td,i){
td.className="";
if(i==0){
_c.add(td,"first-child");
}else{
if(i==this.renderData.columnCount-1){
_c.add(td,"last-child");
}
}
var d=_37.dates[i];
this._setText(td,this._formatColumnHeaderLabel(d));
this.styleColumnHeaderCell(td,d,_37);
},this);
if(this.yearColumnHeaderContent){
var d=_37.dates[0];
this._setText(this.yearColumnHeaderContent,_37.dateLocaleModule.format(d,{selector:"date",datePattern:"yyyy"}));
}
},_cleanupColumnHeader:function(){
while(this._columnHeaderHandlers.length>0){
var _3e=this._columnHeaderHandlers.pop();
while(_3e.length>0){
_3e.pop().remove();
}
}
},styleColumnHeaderCell:function(_3f,_40,_41){
if(this.isToday(_40)){
return _c.add(_3f,"dojoxCalendarToday");
}else{
if(this.isWeekEnd(_40)){
return _c.add(_3f,"dojoxCalendarWeekend");
}
}
},_buildRowHeader:function(_42,_43){
var _44=this.rowHeaderTable;
if(!_44){
return;
}
_d.set(_44,"height",_42.sheetHeight+"px");
var _45=_11("tbody",_44);
var _46,tr,td;
if(_45.length==1){
_46=_45[0];
}else{
_46=_f.create("tbody",null,_44);
}
var _47=_42.hourCount-(_43?_43.hourCount:0);
if(_47>0){
for(var i=0;i<_47;i++){
tr=_f.create("tr",null,_46);
td=_f.create("td",null,tr);
}
}else{
_47=-_47;
for(var i=0;i<_47;i++){
_46.removeChild(_46.lastChild);
}
}
var d=new Date(2000,0,1,0,0,0);
_11("tr",_44).forEach(function(tr,i){
var td=_11("td",tr)[0];
td.className="";
var _48=_42.hourSize;
if(_9("ie")==7){
_48-=2;
}
_d.set(tr,"height",_48+"px");
d.setHours(this.renderData.minHours+(i));
this.styleRowHeaderCell(td,d.getHours(),_42);
this._setText(td,this._formatRowHeaderLabel(d));
},this);
},styleRowHeaderCell:function(_49,h,_4a){
},_buildGrid:function(_4b,_4c){
var _4d=this.gridTable;
if(!_4d){
return;
}
_d.set(_4d,"height",_4b.sheetHeight+"px");
var _4e=Math.floor(60/_4b.slotDuration)*_4b.hourCount;
var _4f=_4e-(_4c?Math.floor(60/_4c.slotDuration)*_4c.hourCount:0);
var _50=_4f>0;
var _51=_4b.columnCount-(_4c?_4c.columnCount:0);
if(_9("ie")==8){
if(this._gridTableSave==null){
this._gridTableSave=_7.clone(_4d);
}else{
if(_51<0){
this.grid.removeChild(_4d);
_f.destroy(_4d);
_4d=_7.clone(this._gridTableSave);
this.gridTable=_4d;
this.grid.appendChild(_4d);
_51=_4b.columnCount;
_4f=_4e;
_50=true;
}
}
}
var _52=_11("tbody",_4d);
var _53;
if(_52.length==1){
_53=_52[0];
}else{
_53=_f.create("tbody",null,_4d);
}
if(_50){
for(var i=0;i<_4f;i++){
_f.create("tr",null,_53);
}
}else{
_4f=-_4f;
for(var i=0;i<_4f;i++){
_53.removeChild(_53.lastChild);
}
}
var _54=Math.floor(60/_4b.slotDuration)*_4b.hourCount-_4f;
var _55=_50||_51>0;
_51=_55?_51:-_51;
_11("tr",_4d).forEach(function(tr,i){
if(_55){
var len=i>=_54?_4b.columnCount:_51;
for(var i=0;i<len;i++){
_f.create("td",null,tr);
}
}else{
for(var i=0;i<_51;i++){
tr.removeChild(tr.lastChild);
}
}
});
_11("tr",_4d).forEach(function(tr,i){
_d.set(tr,"height",_4b.slotSize+"px");
if(i==0){
_c.add(tr,"first-child");
}else{
if(i==_4e-1){
_c.add(tr,"last-child");
}
}
var m=(i*this.renderData.slotDuration)%60;
_11("td",tr).forEach(function(td,col){
td.className="";
if(col==0){
_c.add(td,"first-child");
}else{
if(col==this.renderData.columnCount-1){
_c.add(td,"last-child");
}
}
var d=_4b.dates[col];
this.styleGridColumn(td,d,_4b);
switch(m){
case 0:
_c.add(td,"hour");
break;
case 30:
_c.add(td,"halfhour");
break;
case 15:
case 45:
_c.add(td,"quarterhour");
break;
}
},this);
},this);
},styleGridColumn:function(_56,_57,_58){
if(this.isToday(_57)){
return _c.add(_56,"dojoxCalendarToday");
}else{
if(this.isWeekEnd(_57)){
return _c.add(_56,"dojoxCalendarWeekend");
}
}
},_buildItemContainer:function(_59,_5a){
var _5b=this.itemContainerTable;
if(!_5b){
return;
}
var _5c=[];
_d.set(_5b,"height",_59.sheetHeight+"px");
var _5d=_59.columnCount-(_5a?_5a.columnCount:0);
if(_9("ie")==8){
if(this._itemTableSave==null){
this._itemTableSave=_7.clone(_5b);
}else{
if(_5d<0){
this.itemContainer.removeChild(_5b);
this._recycleItemRenderers(true);
_f.destroy(_5b);
_5b=_7.clone(this._itemTableSave);
this.itemContainerTable=_5b;
this.itemContainer.appendChild(_5b);
_5d=_59.columnCount;
}
}
}
var _5e=_11("tbody",_5b);
var trs=_11("tr",_5b);
var _5f,tr,td;
if(_5e.length==1){
_5f=_5e[0];
}else{
_5f=_f.create("tbody",null,_5b);
}
if(trs.length==1){
tr=trs[0];
}else{
tr=_f.create("tr",null,_5f);
}
if(_5d>0){
for(var i=0;i<_5d;i++){
td=_f.create("td",null,tr);
_f.create("div",{"className":"dojoxCalendarContainerColumn"},td);
}
}else{
_5d=-_5d;
for(var i=0;i<_5d;i++){
tr.removeChild(tr.lastChild);
}
}
_11("td>div",_5b).forEach(function(div,i){
_d.set(div,{"height":_59.sheetHeight+"px"});
_5c.push(div);
},this);
_59.cells=_5c;
},_overlapLayoutPass2:function(_60){
var i,j,_61,_62;
_61=_60[_60.length-1];
for(j=0;j<_61.length;j++){
_61[j].extent=1;
}
for(i=0;i<_60.length-1;i++){
_61=_60[i];
for(var j=0;j<_61.length;j++){
_62=_61[j];
if(_62.extent==-1){
_62.extent=1;
var _63=0;
var _64=false;
for(var k=i+1;k<_60.length&&!_64;k++){
var _65=_60[k];
for(var l=0;l<_65.length&&!_64;l++){
var _66=_65[l];
if(_62.start<_66.end&&_66.start<_62.end){
_64=true;
}
}
if(!_64){
_63++;
}
}
_62.extent+=_63;
}
}
}
},_defaultItemToRendererKindFunc:function(_67){
return "vertical";
},_layoutInterval:function(_68,_69,_6a,end,_6b){
var _6c=[];
_68.colW=this.itemContainer.offsetWidth/_68.columnCount;
for(var i=0;i<_6b.length;i++){
var _6d=_6b[i];
if(this._itemToRendererKind(_6d)=="vertical"){
_6c.push(_6d);
}
}
if(_6c.length>0){
this._layoutVerticalItems(_68,_69,_6a,end,_6c);
}
},_layoutVerticalItems:function(_6e,_6f,_70,_71,_72){
if(this.verticalRenderer==null){
return;
}
var _73=_6e.cells[_6f];
var _74=[];
for(var i=0;i<_72.length;i++){
var _75=_72[i];
var _76=this.computeRangeOverlap(_6e,_75.startTime,_75.endTime,_70,_71);
var top=this.computeProjectionOnDate(_6e,_70,_76[0],_6e.sheetHeight);
var _77=this.computeProjectionOnDate(_6e,_70,_76[1],_6e.sheetHeight);
if(_77>top){
var _78=_7.mixin({start:top,end:_77,range:_76,item:_75},_75);
_74.push(_78);
}
}
var _79=this.computeOverlapping(_74,this._overlapLayoutPass2).numLanes;
var _7a=this.percentOverlap/100;
for(i=0;i<_74.length;i++){
_75=_74[i];
var _7b=_75.lane;
var _7c=_75.extent;
var w;
var _7d;
if(_7a==0){
w=_79==1?_6e.colW:((_6e.colW-(_79-1)*this.horizontalGap)/_79);
_7d=_7b*(w+this.horizontalGap);
w=_7c==1?w:w*_7c+(_7c-1)*this.horizontalGap;
w=100*w/_6e.colW;
_7d=100*_7d/_6e.colW;
}else{
w=_79==1?100:(100/(_79-(_79-1)*_7a));
_7d=_7b*(w-_7a*w);
w=_7c==1?w:w*(_7c-(_7c-1)*_7a);
}
var ir=this._createRenderer(_75,"vertical",this.verticalRenderer,"dojoxCalendarVertical");
_d.set(ir.container,{"top":_75.start+"px","left":_7d+"%","width":w+"%","height":(_75.end-_75.start+1)+"px"});
var _7e=this.isItemBeingEdited(_75);
var _7f=this.isItemSelected(_75);
var _80=this.isItemHovered(_75);
var _81=this.isItemFocused(_75);
var _82=ir.renderer;
_82.set("hovered",_80);
_82.set("selected",_7f);
_82.set("edited",_7e);
_82.set("focused",this.showFocus?_81:false);
_82.set("moveEnabled",this.isItemMoveEnabled(_75,"vertical"));
_82.set("resizeEnabled",this.isItemResizeEnabled(_75,"vertical"));
this.applyRendererZIndex(_75,ir,_80,_7f,_7e,_81);
if(_82.updateRendering){
_82.updateRendering(w,_75.end-_75.start+1);
}
_f.place(ir.container,_73);
_d.set(ir.container,"display","block");
}
},_sortItemsFunction:function(a,b){
var res=this.dateModule.compare(a.startTime,b.startTime);
if(res==0){
res=-1*this.dateModule.compare(a.endTime,b.endTime);
}
return this.isLeftToRight()?res:-res;
},getTime:function(e,x,y,_83){
if(e!=null){
var _84=_e.position(this.itemContainer,true);
if(e.touches){
_83=_83==undefined?0:_83;
x=e.touches[_83].pageX-_84.x;
y=e.touches[_83].pageY-_84.y;
}else{
x=e.pageX-_84.x;
y=e.pageY-_84.y;
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
var col=Math.floor(x/(_e.getMarginBox(this.itemContainer).w/this.renderData.columnCount));
var t=this.getTimeOfDay(y,this.renderData);
var _85=null;
if(col<this.renderData.dates.length){
_85=this.newDate(this.renderData.dates[col]);
_85=this.floorToDay(_85,true);
_85.setHours(t.hours);
_85.setMinutes(t.minutes);
}
return _85;
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
var _86={x:e.touches[0].screenX,y:e.touches[0].screenY};
var p=this._edProps;
if(!p||p&&(Math.abs(_86.x-p.start.x)>25||Math.abs(_86.y-p.start.y)>25)){
this._gridProps.moved=true;
var d=e.touches[0].screenY-this._gridProps.start;
var _87=this._gridProps.scrollTop-d;
var max=this.itemContainer.offsetHeight-this.scrollContainer.offsetHeight;
if(_87<0){
this._gridProps.start=e.touches[0].screenY;
this._setScrollImpl(0);
this._gridProps.scrollTop=0;
}else{
if(_87>max){
this._gridProps.start=e.touches[0].screenY;
this._setScrollImpl(max);
this._gridProps.scrollTop=max;
}else{
this._setScrollImpl(_87);
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
var _88=rd.minHours*60;
var _89=rd.maxHours*60;
var _8a=_88+(pos*(_89-_88)/rd.sheetHeight);
var d={hours:Math.floor(_8a/60),minutes:Math.floor(_8a%60)};
return d;
},_isItemInView:function(_8b){
var res=this.inherited(arguments);
if(res){
var rd=this.renderData;
var len=rd.dateModule.difference(_8b.startTime,_8b.endTime,"millisecond");
var _8c=(24-rd.maxHours+rd.minHours)*3600000;
if(len>_8c){
return true;
}
var _8d=_8b.startTime.getHours()*60+_8b.startTime.getMinutes();
var _8e=_8b.endTime.getHours()*60+_8b.endTime.getMinutes();
var sV=rd.minHours*60;
var eV=rd.maxHours*60;
if(_8d>0&&_8d<sV||_8d>eV&&_8d<=1440){
return false;
}
if(_8e>0&&_8e<sV||_8e>eV&&_8e<=1440){
return false;
}
}
return res;
},_ensureItemInView:function(_8f){
var _90;
var _91=_8f.startTime;
var _92=_8f.endTime;
var rd=this.renderData;
var cal=rd.dateModule;
var len=Math.abs(cal.difference(_8f.startTime,_8f.endTime,"millisecond"));
var _93=(24-rd.maxHours+rd.minHours)*3600000;
if(len>_93){
return false;
}
var _94=_91.getHours()*60+_91.getMinutes();
var _95=_92.getHours()*60+_92.getMinutes();
var sV=rd.minHours*60;
var eV=rd.maxHours*60;
if(_94>0&&_94<sV){
this.floorToDay(_8f.startTime,true,rd);
_8f.startTime.setHours(rd.minHours);
_8f.endTime=cal.add(_8f.startTime,"millisecond",len);
_90=true;
}else{
if(_94>eV&&_94<=1440){
this.floorToDay(_8f.startTime,true,rd);
_8f.startTime=cal.add(_8f.startTime,"day",1);
_8f.startTime.setHours(rd.minHours);
_8f.endTime=cal.add(_8f.startTime,"millisecond",len);
_90=true;
}
}
if(_95>0&&_95<sV){
this.floorToDay(_8f.endTime,true,rd);
_8f.endTime=cal.add(_8f.endTime,"day",-1);
_8f.endTime.setHours(rd.maxHours);
_8f.startTime=cal.add(_8f.endTime,"millisecond",-len);
_90=true;
}else{
if(_95>eV&&_95<=1440){
this.floorToDay(_8f.endTime,true,rd);
_8f.endTime.setHours(rd.maxHours);
_8f.startTime=cal.add(_8f.endTime,"millisecond",-len);
_90=true;
}
}
_90=_90||this.inherited(arguments);
return _90;
},_onScrollTimer_tick:function(){
this._scrollToPosition(this._getScrollPosition()+this._scrollProps.scrollStep);
},snapUnit:"minute",snapSteps:15,minDurationUnit:"minute",minDurationSteps:15,liveLayout:false,stayInView:true,allowStartEndSwap:true,allowResizeLessThan24H:true});
});
