//>>built
define("dojox/calendar/CalendarBase",["dojo/_base/declare","dojo/_base/sniff","dojo/_base/event","dojo/_base/lang","dojo/_base/array","dojo/cldr/supplemental","dojo/dom","dojo/dom-class","dojo/dom-style","dojo/dom-construct","dojo/dom-geometry","dojo/date","dojo/date/locale","dojo/_base/fx","dojo/fx","dojo/on","dijit/_WidgetBase","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","./StoreMixin","./StoreManager","dojox/widget/_Invalidating","dojox/widget/Selection","./time","dojo/i18n!./nls/buttons"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,fx,on,_f,_10,_11,_12,_13,_14,_15,_16,_17){
return _1("dojox.calendar.CalendarBase",[_f,_10,_11,_12,_14,_15],{baseClass:"dojoxCalendar",datePackage:_c,startDate:null,endDate:null,date:null,minDate:null,maxDate:null,dateInterval:"week",dateIntervalSteps:1,viewContainer:null,firstDayOfWeek:-1,formatItemTimeFunc:null,editable:true,moveEnabled:true,resizeEnabled:true,columnView:null,matrixView:null,columnViewProps:null,matrixViewProps:null,createOnGridClick:false,createItemFunc:null,currentView:null,_currentViewIndex:-1,views:null,_calendar:"gregorian",constructor:function(_18){
this.views=[];
this.invalidatingProperties=["store","items","startDate","endDate","views","date","minDate","maxDate","dateInterval","dateIntervalSteps","firstDayOfWeek"];
_18=_18||{};
this._calendar=_18.datePackage?_18.datePackage.substr(_18.datePackage.lastIndexOf(".")+1):this._calendar;
this.dateModule=_18.datePackage?_4.getObject(_18.datePackage,false):_c;
this.dateClassObj=this.dateModule.Date||Date;
this.dateLocaleModule=_18.datePackage?_4.getObject(_18.datePackage+".locale",false):_d;
this.invalidateRendering();
this.storeManager=new _13({owner:this,_ownerItemsProperty:"items"});
this.storeManager.on("layoutInvalidated",_4.hitch(this,this._refreshItemsRendering));
this.storeManager.on("renderersInvalidated",_4.hitch(this,this._updateRenderers));
this.storeManager.on("dataLoaded",_4.hitch(this,function(_19){
this.set("items",_19);
}));
this.decorationStoreManager=new _13({owner:this,_ownerItemsProperty:"decorationItems"});
this.decorationStoreManager.on("layoutInvalidated",_4.hitch(this,this._refreshDecorationItemsRendering));
this.decorationStoreManager.on("dataLoaded",_4.hitch(this,function(_1a){
this.set("decorationItems",_1a);
}));
},buildRendering:function(){
this.inherited(arguments);
if(this.views==null||this.views.length==0){
this.set("views",this._createDefaultViews());
}
},_applyAttributes:function(){
this._applyAttr=true;
this.inherited(arguments);
delete this._applyAttr;
},_setStartDateAttr:function(_1b){
this._set("startDate",_1b);
this._timeRangeInvalidated=true;
this._startDateChanged=true;
},_setEndDateAttr:function(_1c){
this._set("endDate",_1c);
this._timeRangeInvalidated=true;
this._endDateChanged=true;
},_setDateAttr:function(_1d){
this._set("date",_1d);
this._timeRangeInvalidated=true;
this._dateChanged=true;
},_setDateIntervalAttr:function(_1e){
this._set("dateInterval",_1e);
this._timeRangeInvalidated=true;
},_setDateIntervalStepsAttr:function(_1f){
this._set("dateIntervalSteps",_1f);
this._timeRangeInvalidated=true;
},_setFirstDayOfWeekAttr:function(_20){
this._set("firstDayOfWeek",_20);
if(this.get("date")!=null&&this.get("dateInterval")=="week"){
this._timeRangeInvalidated=true;
}
},_setTextDirAttr:function(_21){
_5.forEach(this.views,function(_22){
_22.set("textDir",_21);
});
},refreshRendering:function(){
this.inherited(arguments);
this._validateProperties();
},_refreshItemsRendering:function(){
if(this.currentView){
this.currentView._refreshItemsRendering();
}
},_updateRenderers:function(_23){
if(this.currentView){
this.currentView.updateRenderers(_23);
}
},_refreshDecorationItemsRendering:function(){
if(this.currentView){
this.currentView._refreshDecorationItemsRendering();
}
},resize:function(_24){
if(_24){
_b.setMarginBox(this.domNode,_24);
}
if(this.currentView){
this.currentView.resize();
}
},_validateProperties:function(){
var cal=this.dateModule;
var _25=this.get("startDate");
var _26=this.get("endDate");
var _27=this.get("date");
if(this.firstDayOfWeek<-1||this.firstDayOfWeek>6){
this._set("firstDayOfWeek",0);
}
var _28=this.get("minDate");
var _29=this.get("maxDate");
if(_28&&_29){
if(cal.compare(_28,_29)>0){
var t=_28;
this._set("minDate",_29);
this._set("maxDate",t);
}
}
if(_27==null&&(_25!=null||_26!=null)){
if(_25==null){
_25=new this.dateClassObj();
this._set("startDate",_25);
this._timeRangeInvalidated=true;
}
if(_26==null){
_26=new this.dateClassObj();
this._set("endDate",_26);
this._timeRangeInvalidated=true;
}
if(cal.compare(_25,_26)>0){
_26=cal.add(_25,"day",1);
this._set("endDate",_26);
this._timeRangeInvalidated=true;
}
}else{
if(this.date==null){
this._set("date",new this.dateClassObj());
this._timeRangeInvalidated=true;
}
var _2a=this.get("dateInterval");
if(_2a!="day"&&_2a!="week"&&_2a!="month"){
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
var _2b=this.computeTimeInterval();
if(this._timeInterval==null||cal.compare(this._timeInterval[0],_2b[0])!=0||cal.compare(this._timeInterval[1],_2b[1])!=0){
if(this._dateChanged){
this._lastValidDate=this.get("date");
this._dateChanged=false;
}else{
if(this._startDateChanged||this._endDateChanged){
this._lastValidStartDate=this.get("startDate");
this._lastValidEndDate=this.get("endDate");
this._startDateChanged=false;
this._endDateChanged=false;
}
}
this.onTimeIntervalChange({oldStartTime:this._timeInterval==null?null:this._timeInterval[0],oldEndTime:this._timeInterval==null?null:this._timeInterval[1],startTime:_2b[0],endTime:_2b[1]});
}else{
if(this._dateChanged){
this._dateChanged=false;
if(this.lastValidDate!=null){
this._set("date",this.lastValidDate);
}
}else{
if(this._startDateChanged||this._endDateChanged){
this._startDateChanged=false;
this._endDateChanged=false;
this._set("startDate",this._lastValidStartDate);
this._set("endDate",this._lastValidEndDate);
}
}
return;
}
this._timeInterval=_2b;
var _2c=this.dateModule.difference(this._timeInterval[0],this._timeInterval[1],"day");
var _2d=this._computeCurrentView(_2b[0],_2b[1],_2c);
var _2e=_5.indexOf(this.views,_2d);
if(_2d==null||_2e==-1){
return;
}
this._performViewTransition(_2d,_2e,_2b,_2c);
}
},_performViewTransition:function(_2f,_30,_31,_32){
var _33=this.currentView;
if(this.animateRange&&(!_2("ie")||_2("ie")>8)){
if(_33){
_33.beforeDeactivate();
var ltr=this.isLeftToRight();
var _34=this._animRangeInDir=="left"||this._animRangeInDir==null;
var _35=this._animRangeOutDir=="left"||this._animRangeOutDir==null;
this._animateRange(this.currentView.domNode,_35&&ltr,false,0,_35?-100:100,_4.hitch(this,function(){
_33.afterDeactivate();
_2f.beforeActivate();
this.animateRangeTimer=setTimeout(_4.hitch(this,function(){
this._applyViewChange(_2f,_30,_31,_32);
this._animateRange(this.currentView.domNode,_34&&ltr,true,_34?-100:100,0,function(){
_2f.afterActivate();
});
this._animRangeInDir=null;
this._animRangeOutDir=null;
}),100);
}));
}else{
_2f.beforeActivate();
this._applyViewChange(_2f,_30,_31,_32);
_2f.afterActivate();
}
}else{
if(_33){
_33.beforeDeactivate();
}
_2f.beforeActivate();
this._applyViewChange(_2f,_30,_31,_32);
if(_33){
_33.afterDeactivate();
}
_2f.afterActivate();
}
},onViewConfigurationChange:function(_36){
},_applyViewChange:function(_37,_38,_39,_3a){
this._configureView(_37,_38,_39,_3a);
this.onViewConfigurationChange(_37);
if(_38!=this._currentViewIndex){
if(this.currentView==null){
_37.set("items",this.items);
_37.set("decorationItems",this.decorationItems);
this.set("currentView",_37);
}else{
if(this.items==null||this.items.length==0){
this.set("currentView",_37);
if(this.animateRange&&(!_2("ie")||_2("ie")>8)){
_9.set(this.currentView.domNode,"opacity",0);
}
_37.set("items",this.items);
_37.set("decorationItems",this.decorationItems);
}else{
this.currentView=_37;
_37.set("items",this.items);
_37.set("decorationItems",this.decorationItems);
this.set("currentView",_37);
if(this.animateRange&&(!_2("ie")||_2("ie")>8)){
_9.set(this.currentView.domNode,"opacity",0);
}
}
}
}
},_timeInterval:null,computeTimeInterval:function(){
var d=this.get("date");
var _3b=this.get("minDate");
var _3c=this.get("maxDate");
var cal=this.dateModule;
if(d==null){
var _3d=this.get("startDate");
var _3e=cal.add(this.get("endDate"),"day",1);
if(_3b!=null||_3c!=null){
var dur=this.dateModule.difference(_3d,_3e,"day");
if(cal.compare(_3b,_3d)>0){
_3d=_3b;
_3e=cal.add(_3d,"day",dur);
}
if(cal.compare(_3c,_3e)<0){
_3e=_3c;
_3d=cal.add(_3e,"day",-dur);
}
if(cal.compare(_3b,_3d)>0){
_3d=_3b;
_3e=_3c;
}
}
return [this.floorToDay(_3d),this.floorToDay(_3e)];
}else{
var _3f=this._computeTimeIntervalImpl(d);
if(_3b!=null){
var _40=this._computeTimeIntervalImpl(_3b);
if(cal.compare(_40[0],_3f[0])>0){
_3f=_40;
}
}
if(_3c!=null){
var _41=this._computeTimeIntervalImpl(_3c);
if(cal.compare(_41[1],_3f[1])<0){
_3f=_41;
}
}
return _3f;
}
},_computeTimeIntervalImpl:function(d){
var cal=this.dateModule;
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
default:
e=cal.add(s,"day",1);
}
return [s,e];
},onTimeIntervalChange:function(e){
},views:null,_setViewsAttr:function(_42){
if(!this._applyAttr){
for(var i=0;i<this.views.length;i++){
this._onViewRemoved(this.views[i]);
}
}
if(_42!=null){
for(var i=0;i<_42.length;i++){
this._onViewAdded(_42[i]);
}
}
this._set("views",_42==null?[]:_42.concat());
},_getViewsAttr:function(){
return this.views.concat();
},_createDefaultViews:function(){
},addView:function(_43,_44){
if(_44<=0||_44>this.views.length){
_44=this.views.length;
}
this.views.splice(_44,_43);
this._onViewAdded(_43);
},removeView:function(_45){
if(index<0||index>=this.views.length){
return;
}
this._onViewRemoved(this.views[index]);
this.views.splice(index,1);
},_onViewAdded:function(_46){
_46.owner=this;
_46.buttonContainer=this.buttonContainer;
_46._calendar=this._calendar;
_46.datePackage=this.datePackage;
_46.dateModule=this.dateModule;
_46.dateClassObj=this.dateClassObj;
_46.dateLocaleModule=this.dateLocaleModule;
_9.set(_46.domNode,"display","none");
_8.add(_46.domNode,"view");
_a.place(_46.domNode,this.viewContainer);
this.onViewAdded(_46);
},onViewAdded:function(_47){
},_onViewRemoved:function(_48){
_48.owner=null;
_48.buttonContainer=null;
_8.remove(_48.domNode,"view");
this.viewContainer.removeChild(_48.domNode);
this.onViewRemoved(_48);
},onViewRemoved:function(_49){
},_setCurrentViewAttr:function(_4a){
var _4b=_5.indexOf(this.views,_4a);
if(_4b!=-1){
var _4c=this.get("currentView");
this._currentViewIndex=_4b;
this._set("currentView",_4a);
this._showView(_4c,_4a);
this.onCurrentViewChange({oldView:_4c,newView:_4a});
}
},_getCurrentViewAttr:function(){
return this.views[this._currentViewIndex];
},onCurrentViewChange:function(e){
},_configureView:function(_4d,_4e,_4f,_50){
var cal=this.dateModule;
if(_4d.viewKind=="columns"){
_4d.set("startDate",_4f[0]);
_4d.set("columnCount",_50);
}else{
if(_4d.viewKind=="matrix"){
if(_50>7){
var s=this.floorToWeek(_4f[0]);
var e=this.floorToWeek(_4f[1]);
if(cal.compare(e,_4f[1])!=0){
e=this.dateModule.add(e,"week",1);
}
_50=this.dateModule.difference(s,e,"day");
_4d.set("startDate",s);
_4d.set("columnCount",7);
_4d.set("rowCount",Math.ceil(_50/7));
_4d.set("refStartTime",_4f[0]);
_4d.set("refEndTime",_4f[1]);
}else{
_4d.set("startDate",_4f[0]);
_4d.set("columnCount",_50);
_4d.set("rowCount",1);
_4d.set("refStartTime",null);
_4d.set("refEndTime",null);
}
}
}
},_computeCurrentView:function(_51,_52,_53){
return _53<=7?this.columnView:this.matrixView;
},matrixViewRowHeaderClick:function(e){
var _54=this.matrixView.getExpandedRowIndex();
if(_54==e.index){
this.matrixView.collapseRow();
}else{
if(_54==-1){
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
},viewChangeDuration:0,_showView:function(_55,_56){
if(_55!=null){
_9.set(_55.domNode,"display","none");
}
if(_56!=null){
_9.set(_56.domNode,"display","block");
_56.resize();
if(!_2("ie")||_2("ie")>7){
_9.set(_56.domNode,"opacity","1");
}
}
},_setItemsAttr:function(_57){
this._set("items",_57);
if(this.currentView){
this.currentView.set("items",_57);
if(!this._isEditing){
this.currentView.invalidateRendering();
}
}
},_setDecorationItemsAttr:function(_58){
this._set("decorationItems",_58);
if(this.currentView){
this.currentView.set("decorationItems",_58);
this.currentView.invalidateRendering();
}
},_setDecorationStoreAttr:function(_59){
this._set("decorationStore",_59);
this.decorationStore=_59;
this.decorationStoreManager.set("store",_59);
},floorToDay:function(_5a,_5b){
return _16.floorToDay(_5a,_5b,this.dateClassObj);
},floorToWeek:function(d){
return _16.floorToWeek(d,this.dateClassObj,this.dateModule,this.firstDayOfWeek,this.locale);
},newDate:function(obj){
return _16.newDate(obj,this.dateClassObj);
},isToday:function(_5c){
return _16.isToday(_5c,this.dateClassObj);
},isStartOfDay:function(d){
return _16.isStartOfDay(d,this.dateClassObj,this.dateModule);
},floorDate:function(_5d,_5e,_5f,_60){
return _16.floor(_5d,_5e,_5f,_60,this.classFuncObj);
},isOverlapping:function(_61,_62,_63,_64,_65,_66){
return _16.isOverlapping(_61,_62,_63,_64,_65,_66);
},animateRange:true,animationRangeDuration:400,_animateRange:function(_67,_68,_69,_6a,xTo,_6b){
if(this.animateRangeTimer){
clearTimeout(this.animateRangeTimer);
delete this.animateRangeTimer;
}
var _6c=_69?_e.fadeIn:_e.fadeOut;
_9.set(_67,{left:_6a+"px",right:(-_6a)+"px"});
fx.combine([_e.animateProperty({node:_67,properties:{left:xTo,right:-xTo},duration:this.animationRangeDuration/2,onEnd:_6b}),_6c({node:_67,duration:this.animationRangeDuration/2})]).play();
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
var rtl=!this.isLeftToRight();
if(this.previousButton){
this.previousButton.set("label",_17[rtl?"nextButton":"previousButton"]);
this.own(on(this.previousButton,"click",_4.hitch(this,this.previousRange)));
}
if(this.nextButton){
this.nextButton.set("label",_17[rtl?"previousButton":"nextButton"]);
this.own(on(this.nextButton,"click",_4.hitch(this,this.nextRange)));
}
if(rtl&&this.previousButton&&this.nextButton){
var t=this.previousButton;
this.previousButton=this.nextButton;
this.nextButton=t;
}
if(this.todayButton){
this.todayButton.set("label",_17.todayButton);
this.own(on(this.todayButton,"click",_4.hitch(this,this.todayButtonClick)));
}
if(this.dayButton){
this.dayButton.set("label",_17.dayButton);
this.own(on(this.dayButton,"click",_4.hitch(this,this.dayButtonClick)));
}
if(this.weekButton){
this.weekButton.set("label",_17.weekButton);
this.own(on(this.weekButton,"click",_4.hitch(this,this.weekButtonClick)));
}
if(this.fourDaysButton){
this.fourDaysButton.set("label",_17.fourDaysButton);
this.own(on(this.fourDaysButton,"click",_4.hitch(this,this.fourDaysButtonClick)));
}
if(this.monthButton){
this.monthButton.set("label",_17.monthButton);
this.own(on(this.monthButton,"click",_4.hitch(this,this.monthButtonClick)));
}
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
},updateRenderers:function(obj,_6d){
if(this.currentView){
this.currentView.updateRenderers(obj,_6d);
}
},getIdentity:function(_6e){
return _6e?_6e.id:null;
},_setHoveredItem:function(_6f,_70){
if(this.hoveredItem&&_6f&&this.hoveredItem.id!=_6f.id||_6f==null||this.hoveredItem==null){
var old=this.hoveredItem;
this.hoveredItem=_6f;
this.updateRenderers([old,this.hoveredItem],true);
if(_6f&&_70){
this.currentView._updateEditingCapabilities(_6f._item?_6f._item:_6f,_70);
}
}
},hoveredItem:null,isItemHovered:function(_71){
return this.hoveredItem!=null&&this.hoveredItem.id==_71.id;
},isItemEditable:function(_72,_73){
return this.editable;
},isItemMoveEnabled:function(_74,_75){
return this.isItemEditable(_74,_75)&&this.moveEnabled;
},isItemResizeEnabled:function(_76,_77){
return this.isItemEditable(_76,_77)&&this.resizeEnabled;
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
},_onRendererCreated:function(e){
this.onRendererCreated(e);
},onRendererCreated:function(e){
},_onRendererRecycled:function(e){
this.onRendererRecycled(e);
},onRendererRecycled:function(e){
},_onRendererReused:function(e){
this.onRendererReused(e);
},onRendererReused:function(e){
},_onRendererDestroyed:function(e){
this.onRendererDestroyed(e);
},onRendererDestroyed:function(e){
},_onRenderersLayoutDone:function(_78){
this.onRenderersLayoutDone(_78);
},onRenderersLayoutDone:function(_79){
}});
});
