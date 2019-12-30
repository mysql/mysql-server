//>>built
define("dojox/calendar/CalendarBase",["dojo/_base/declare","dojo/_base/sniff","dojo/_base/event","dojo/_base/lang","dojo/_base/array","dojo/cldr/supplemental","dojo/dom","dojo/dom-class","dojo/dom-style","dojo/dom-construct","dojo/date","dojo/date/locale","dojo/_base/fx","dojo/fx","dojo/on","dijit/_WidgetBase","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","./StoreMixin","dojox/widget/_Invalidating","dojox/widget/Selection","dojox/calendar/time","dojo/i18n!./nls/buttons"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,fx,on,_e,_f,_10,_11,_12,_13,_14,_15){
return _1("dojox.calendar.CalendarBase",[_e,_f,_10,_11,_12,_13],{baseClass:"dojoxCalendar",datePackage:_b,startDate:null,endDate:null,date:null,dateInterval:"week",dateIntervalSteps:1,viewContainer:null,firstDayOfWeek:-1,formatItemTimeFunc:null,editable:true,moveEnabled:true,resizeEnabled:true,columnView:null,matrixView:null,columnViewProps:null,matrixViewProps:null,createOnGridClick:false,createItemFunc:null,_currentViewIndex:-1,views:null,_calendar:"gregorian",constructor:function(_16){
this.views=[];
this.invalidatingProperties=["store","items","startDate","endDate","views","date","dateInterval","dateIntervalSteps","firstDayOfWeek"];
_16=_16||{};
this._calendar=_16.datePackage?_16.datePackage.substr(_16.datePackage.lastIndexOf(".")+1):this._calendar;
this.dateModule=_16.datePackage?_4.getObject(_16.datePackage,false):_b;
this.dateClassObj=this.dateModule.Date||Date;
this.dateLocaleModule=_16.datePackage?_4.getObject(_16.datePackage+".locale",false):_c;
this.invalidateRendering();
},destroy:function(_17){
_5.forEach(this._buttonHandles,function(h){
h.remove();
});
this.inherited(arguments);
},buildRendering:function(){
this.inherited(arguments);
if(this.views==null||this.views.length==0){
this.set("views",this._createDefaultViews());
}
},_applyAttributes:function(){
this._applyAttr=true;
this.inherited(arguments);
delete this._applyAttr;
},_setStartDateAttr:function(_18){
this._set("startDate",_18);
this._timeRangeInvalidated=true;
},_setEndDateAttr:function(_19){
this._set("endDate",_19);
this._timeRangeInvalidated=true;
},_setDateAttr:function(_1a){
this._set("date",_1a);
this._timeRangeInvalidated=true;
},_setDateIntervalAttr:function(_1b){
this._set("dateInterval",_1b);
this._timeRangeInvalidated=true;
},_setDateIntervalStepsAttr:function(_1c){
this._set("dateIntervalSteps",_1c);
this._timeRangeInvalidated=true;
},_setFirstDayOfWeekAttr:function(_1d){
this._set("firstDayOfWeek",_1d);
if(this.get("date")!=null&&this.get("dateInterval")=="week"){
this._timeRangeInvalidated=true;
}
},_setTextDirAttr:function(_1e){
_5.forEach(this.views,function(_1f){
_1f.set("textDir",_1e);
});
},refreshRendering:function(){
this.inherited(arguments);
this._validateProperties();
},_refreshItemsRendering:function(){
if(this.currentView){
this.currentView._refreshItemsRendering();
}
},_validateProperties:function(){
var cal=this.dateModule;
var _20=this.get("startDate");
var _21=this.get("endDate");
var _22=this.get("date");
if(this.firstDayOfWeek<-1||this.firstDayOfWeek>6){
this._set("firstDayOfWeek",0);
}
if(_22==null&&(_20!=null||_21!=null)){
if(_20==null){
_20=new this.dateClassObj();
this._set("startDate",_20);
this._timeRangeInvalidated=true;
}
if(_21==null){
_21=new this.dateClassObj();
this._set("endDate",_21);
this._timeRangeInvalidated=true;
}
if(cal.compare(_20,_21)>=0){
_21=cal.add(_20,"day",1);
this._set("endDate",_21);
this._timeRangeInvalidated=true;
}
}else{
if(this.date==null){
this._set("date",new this.dateClassObj());
this._timeRangeInvalidated=true;
}
var _23=this.get("dateInterval");
if(_23!="day"&&_23!="week"&&_23!="month"){
this._set("dateInterval","day");
this._timeRangeInvalidated=true;
}
var dis=this.get("dateIntervalSteps");
if(_4.isString(dis)){
dis=parseInt(dis);
this._set("dateIntervalSteps",dis);
}
if(dis<=0){
this.set("dateIntervalSteps",1);
this._timeRangeInvalidated=true;
}
}
if(this._timeRangeInvalidated){
this._timeRangeInvalidated=false;
var _24=this.computeTimeInterval();
if(this._timeInterval==null||cal.compare(this._timeInterval[0],_24[0]!=0)||cal.compare(this._timeInterval[1],_24[1]!=0)){
this.onTimeIntervalChange({oldStartTime:this._timeInterval==null?null:this._timeInterval[0],oldEndTime:this._timeInterval==null?null:this._timeInterval[1],startTime:_24[0],endTime:_24[1]});
}
this._timeInterval=_24;
var _25=this.dateModule.difference(this._timeInterval[0],this._timeInterval[1],"day");
var _26=this._computeCurrentView(_24[0],_24[1],_25);
var _27=_5.indexOf(this.views,_26);
if(_26==null||_27==-1){
return;
}
if(this.animateRange&&(!_2("ie")||_2("ie")>8)){
if(this.currentView){
var ltr=this.isLeftToRight();
var _28=this._animRangeInDir=="left"||this._animRangeInDir==null;
var _29=this._animRangeOutDir=="left"||this._animRangeOutDir==null;
this._animateRange(this.currentView.domNode,_29&&ltr,false,0,_29?-100:100,_4.hitch(this,function(){
this.animateRangeTimer=setTimeout(_4.hitch(this,function(){
this._applyViewChange(_26,_27,_24,_25);
this._animateRange(this.currentView.domNode,_28&&ltr,true,_28?-100:100,0);
this._animRangeInDir=null;
this._animRangeOutDir=null;
}),100);
}));
}else{
this._applyViewChange(_26,_27,_24,_25);
}
}else{
this._applyViewChange(_26,_27,_24,_25);
}
}
},_applyViewChange:function(_2a,_2b,_2c,_2d){
this._configureView(_2a,_2b,_2c,_2d);
if(_2b!=this._currentViewIndex){
if(this.currentView==null){
_2a.set("items",this.items);
this.set("currentView",_2a);
}else{
if(this.items==null||this.items.length==0){
this.set("currentView",_2a);
if(this.animateRange&&(!_2("ie")||_2("ie")>8)){
_9.set(this.currentView.domNode,"opacity",0);
}
_2a.set("items",this.items);
}else{
this.currentView=_2a;
_2a.set("items",this.items);
this.set("currentView",_2a);
if(this.animateRange&&(!_2("ie")||_2("ie")>8)){
_9.set(this.currentView.domNode,"opacity",0);
}
}
}
}
},_timeInterval:null,computeTimeInterval:function(){
var cal=this.dateModule;
var d=this.get("date");
if(d==null){
return [this.floorToDay(this.get("startDate")),cal.add(this.get("endDate"),"day",1)];
}else{
var s=this.floorToDay(d);
var di=this.get("dateInterval");
var dis=this.get("dateIntervalSteps");
var e;
switch(di){
case "day":
e=cal.add(s,"day",dis);
break;
case "week":
s=this.floorToWeek(s);
e=cal.add(s,"week",dis);
break;
case "month":
s.setDate(1);
e=cal.add(s,"month",dis);
break;
}
return [s,e];
}
},onTimeIntervalChange:function(e){
},views:null,_setViewsAttr:function(_2e){
if(!this._applyAttr){
for(var i=0;i<this.views.length;i++){
this._onViewRemoved(this.views[i]);
}
}
if(_2e!=null){
for(var i=0;i<_2e.length;i++){
this._onViewAdded(_2e[i]);
}
}
this._set("views",_2e==null?[]:_2e.concat());
},_getViewsAttr:function(){
return this.views.concat();
},_createDefaultViews:function(){
},addView:function(_2f,_30){
if(_30<=0||_30>this.views.length){
_30=this.views.length;
}
this.views.splice(_30,_2f);
this._onViewAdded(_2f);
},removeView:function(_31){
if(index<0||index>=this.views.length){
return;
}
this._onViewRemoved(this.views[index]);
this.views.splice(index,1);
},_onViewAdded:function(_32){
_32.owner=this;
_32.buttonContainer=this.buttonContainer;
_32._calendar=this._calendar;
_32.datePackage=this.datePackage;
_32.dateModule=this.dateModule;
_32.dateClassObj=this.dateClassObj;
_32.dateLocaleModule=this.dateLocaleModule;
_9.set(_32.domNode,"display","none");
_8.add(_32.domNode,"view");
_a.place(_32.domNode,this.viewContainer);
this.onViewAdded(_32);
},onViewAdded:function(_33){
},_onViewRemoved:function(_34){
_34.owner=null;
_34.buttonContainer=null;
_8.remove(_34.domNode,"view");
this.viewContainer.removeChild(_34.domNode);
this.onViewRemoved(_34);
},onViewRemoved:function(_35){
},_setCurrentViewAttr:function(_36){
var _37=_5.indexOf(this.views,_36);
if(_37!=-1){
var _38=this.get("currentView");
this._currentViewIndex=_37;
this._set("currentView",_36);
this._showView(_38,_36);
this.onCurrentViewChange({oldView:_38,newView:_36});
}
},_getCurrentViewAttr:function(){
return this.views[this._currentViewIndex];
},onCurrentViewChange:function(e){
},_configureView:function(_39,_3a,_3b,_3c){
var cal=this.dateModule;
if(_39.viewKind=="columns"){
_39.set("startDate",_3b[0]);
_39.set("columnCount",_3c);
}else{
if(_39.viewKind=="matrix"){
if(_3c>7){
var s=this.floorToWeek(_3b[0]);
var e=this.floorToWeek(_3b[1]);
if(cal.compare(e,_3b[1])!=0){
e=this.dateModule.add(e,"week",1);
}
_3c=this.dateModule.difference(s,e,"day");
_39.set("startDate",s);
_39.set("columnCount",7);
_39.set("rowCount",Math.ceil(_3c/7));
_39.set("refStartTime",_3b[0]);
_39.set("refEndTime",_3b[1]);
}else{
_39.set("startDate",_3b[0]);
_39.set("columnCount",_3c);
_39.set("rowCount",1);
_39.set("refStartTime",null);
_39.set("refEndTime",null);
}
}
}
},_computeCurrentView:function(_3d,_3e,_3f){
return _3f<=7?this.columnView:this.matrixView;
},matrixViewRowHeaderClick:function(e){
var _40=this.matrixView.getExpandedRowIndex();
if(_40==e.index){
this.matrixView.collapseRow();
}else{
if(_40==-1){
this.matrixView.expandRow(e.index);
}else{
var h=this.matrixView.on("expandAnimationEnd",_4.hitch(this,function(){
h.remove();
this.matrixView.expandRow(e.index);
}));
this.matrixView.collapseRow();
}
}
},columnViewColumnHeaderClick:function(e){
var cal=this.dateModule;
if(cal.compare(e.date,this._timeInterval[0])==0&&this.dateInterval=="day"&&this.dateIntervalSteps==1){
this.set("dateInterval","week");
}else{
this.set("date",e.date);
this.set("dateInterval","day");
this.set("dateIntervalSteps",1);
}
},viewChangeDuration:0,_showView:function(_41,_42){
if(_41!=null){
_9.set(_41.domNode,"display","none");
}
if(_42!=null){
_9.set(_42.domNode,"display","block");
_42.resize();
if(!_2("ie")||_2("ie")>7){
_9.set(_42.domNode,"opacity","1");
}
}
},_setItemsAttr:function(_43){
this._set("items",_43);
if(this.currentView){
this.currentView.set("items",_43);
this.currentView.invalidateRendering();
}
},floorToDay:function(_44,_45){
return _14.floorToDay(_44,_45,this.dateClassObj);
},floorToWeek:function(d){
return _14.floorToWeek(d,this.dateClassObj,this.dateModule,this.firstDayOfWeek,this.locale);
},newDate:function(obj){
return _14.newDate(obj,this.dateClassObj);
},isToday:function(_46){
return _14.isToday(_46,this.dateClassObj);
},isStartOfDay:function(d){
return _14.isStartOfDay(d,this.dateClassObj,this.dateModule);
},floorDate:function(_47,_48,_49,_4a){
return _14.floor(_47,_48,_49,_4a,this.classFuncObj);
},animateRange:true,animationRangeDuration:400,_animateRange:function(_4b,_4c,_4d,_4e,xTo,_4f){
if(this.animateRangeTimer){
clearTimeout(this.animateRangeTimer);
delete this.animateRangeTimer;
}
var _50=_4d?_d.fadeIn:_d.fadeOut;
_9.set(_4b,{left:_4e+"px",right:(-_4e)+"px"});
fx.combine([_d.animateProperty({node:_4b,properties:{left:xTo,right:-xTo},duration:this.animationRangeDuration/2,onEnd:_4f}),_50({node:_4b,duration:this.animationRangeDuration/2})]).play();
},_animRangeOutDir:null,_animRangeOutDir:null,nextRange:function(){
this._animRangeOutDir="left";
this._animRangeInDir="right";
this._navigate(1);
},previousRange:function(){
this._animRangeOutDir="right";
this._animRangeInDir="left";
this._navigate(-1);
},_navigate:function(dir){
var d=this.get("date");
var cal=this.dateModule;
if(d==null){
var s=this.get("startDate");
var e=this.get("endDate");
var dur=cal.difference(s,e,"day");
if(dir==1){
e=cal.add(e,"day",1);
this.set("startDate",e);
this.set("endDate",cal.add(e,"day",dur));
}else{
s=cal.add(s,"day",-1);
this.set("startDate",cal.add(s,"day",-dur));
this.set("endDate",s);
}
}else{
var di=this.get("dateInterval");
var dis=this.get("dateIntervalSteps");
this.set("date",cal.add(d,di,dir*dis));
}
},goToday:function(){
this.set("date",this.floorToDay(new this.dateClassObj(),true));
this.set("dateInterval","day");
this.set("dateIntervalSteps",1);
},postCreate:function(){
this.inherited(arguments);
this.configureButtons();
},configureButtons:function(){
var h=[];
var rtl=!this.isLeftToRight();
if(this.previousButton){
this.previousButton.set("label",_15[rtl?"nextButton":"previousButton"]);
h.push(on(this.previousButton,"click",_4.hitch(this,rtl?this.nextRange:this.previousRange)));
}
if(this.nextButton){
this.nextButton.set("label",_15[rtl?"previousButton":"nextButton"]);
h.push(on(this.nextButton,"click",_4.hitch(this,rtl?this.previousRange:this.nextRange)));
}
if(rtl&&this.previousButton&&this.nextButton){
var t=this.previousButton;
this.previousButton=this.nextButton;
this.nextButton=t;
}
if(this.todayButton){
this.todayButton.set("label",_15.todayButton);
h.push(on(this.todayButton,"click",_4.hitch(this,this.todayButtonClick)));
}
if(this.dayButton){
this.dayButton.set("label",_15.dayButton);
h.push(on(this.dayButton,"click",_4.hitch(this,this.dayButtonClick)));
}
if(this.weekButton){
this.weekButton.set("label",_15.weekButton);
h.push(on(this.weekButton,"click",_4.hitch(this,this.weekButtonClick)));
}
if(this.fourDaysButton){
this.fourDaysButton.set("label",_15.fourDaysButton);
h.push(on(this.fourDaysButton,"click",_4.hitch(this,this.fourDaysButtonClick)));
}
if(this.monthButton){
this.monthButton.set("label",_15.monthButton);
h.push(on(this.monthButton,"click",_4.hitch(this,this.monthButtonClick)));
}
this._buttonHandles=h;
},todayButtonClick:function(e){
this.goToday();
},dayButtonClick:function(e){
if(this.get("date")==null){
this.set("date",this.floorToDay(new this.dateClassObj(),true));
}
this.set("dateInterval","day");
this.set("dateIntervalSteps",1);
},weekButtonClick:function(e){
this.set("dateInterval","week");
this.set("dateIntervalSteps",1);
},fourDaysButtonClick:function(e){
this.set("dateInterval","day");
this.set("dateIntervalSteps",4);
},monthButtonClick:function(e){
this.set("dateInterval","month");
this.set("dateIntervalSteps",1);
},updateRenderers:function(obj,_51){
if(this.currentView){
this.currentView.updateRenderers(obj,_51);
}
},getIdentity:function(_52){
return _52?_52.id:null;
},_setHoveredItem:function(_53,_54){
if(this.hoveredItem&&_53&&this.hoveredItem.id!=_53.id||_53==null||this.hoveredItem==null){
var old=this.hoveredItem;
this.hoveredItem=_53;
this.updateRenderers([old,this.hoveredItem],true);
if(_53&&_54){
this.currentView._updateEditingCapabilities(_53,_54);
}
}
},hoveredItem:null,isItemHovered:function(_55){
return this.hoveredItem!=null&&this.hoveredItem.id==_55.id;
},isItemEditable:function(_56,_57){
return this.editable;
},isItemMoveEnabled:function(_58,_59){
return this.isItemEditable()&&this.moveEnabled;
},isItemResizeEnabled:function(_5a,_5b){
return this.isItemEditable()&&this.resizeEnabled;
},onGridClick:function(e){
},onGridDoubleClick:function(e){
},onItemClick:function(e){
},onItemDoubleClick:function(e){
},onItemContextMenu:function(e){
},onItemEditBegin:function(e){
},onItemEditEnd:function(e){
},onItemEditBeginGesture:function(e){
},onItemEditMoveGesture:function(e){
},onItemEditResizeGesture:function(e){
},onItemEditEndGesture:function(e){
},onItemRollOver:function(e){
},onItemRollOut:function(e){
},onColumnHeaderClick:function(e){
},onRowHeaderClick:function(e){
},onExpandRendererClick:function(e){
},onRendererCreated:function(_5c){
},onRendererRecycled:function(_5d){
},onRendererReused:function(_5e){
},onRendererDestroyed:function(_5f){
},onRenderersLayoutDone:function(_60){
}});
});
