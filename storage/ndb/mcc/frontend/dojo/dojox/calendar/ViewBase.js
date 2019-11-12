//>>built
define("dojox/calendar/ViewBase",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/window","dojo/_base/event","dojo/_base/html","dojo/_base/sniff","dojo/query","dojo/dom","dojo/dom-style","dojo/dom-construct","dojo/dom-geometry","dojo/on","dojo/date","dojo/date/locale","dijit/_WidgetBase","dojox/widget/_Invalidating","dojox/widget/Selection","dojox/calendar/time","./StoreMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,on,_d,_e,_f,_10,_11,_12,_13){
return _1("dojox.calendar.ViewBase",[_f,_13,_10,_11],{datePackage:_d,_calendar:"gregorian",viewKind:null,_layoutStep:1,_layoutUnit:"day",resizeCursor:"n-resize",formatItemTimeFunc:null,_getFormatItemTimeFuncAttr:function(){
if(this.owner!=null){
return this.owner.get("formatItemTimeFunc");
}else{
return this.formatItemTimeFunc;
}
},_viewHandles:null,doubleTapDelay:300,constructor:function(_14){
_14=_14||{};
this._calendar=_14.datePackage?_14.datePackage.substr(_14.datePackage.lastIndexOf(".")+1):this._calendar;
this.dateModule=_14.datePackage?_2.getObject(_14.datePackage,false):_d;
this.dateClassObj=this.dateModule.Date||Date;
this.dateLocaleModule=_14.datePackage?_2.getObject(_14.datePackage+".locale",false):_e;
this.rendererPool=[];
this.rendererList=[];
this.itemToRenderer={};
this._viewHandles=[];
},destroy:function(_15){
while(this.rendererList.length>0){
this._destroyRenderer(this.rendererList.pop());
}
for(kind in this._rendererPool){
var _16=this._rendererPool[kind];
if(_16){
while(_16.length>0){
this._destroyRenderer(_16.pop());
}
}
}
while(this._viewHandles.length>0){
this._viewHandles.pop().remove();
}
this.inherited(arguments);
},_createRenderData:function(){
},_validateProperties:function(){
},_setText:function(_17,_18,_19){
if(_18!=null){
if(!_19&&_17.hasChildNodes()){
_17.childNodes[0].childNodes[0].nodeValue=_18;
}else{
while(_17.hasChildNodes()){
_17.removeChild(_17.lastChild);
}
var _1a=_4.doc.createElement("span");
this.applyTextDir(_1a,_18);
if(_19){
_1a.innerHTML=_18;
}else{
_1a.appendChild(_4.doc.createTextNode(_18));
}
_17.appendChild(_1a);
}
}
},isAscendantHasClass:function(_1b,_1c,_1d){
while(_1b!=_1c&&_1b!=document){
if(dojo.hasClass(_1b,_1d)){
return true;
}
_1b=_1b.parentNode;
}
return false;
},isWeekEnd:function(_1e){
return _e.isWeekend(_1e);
},getWeekNumberLabel:function(_1f){
if(_1f.toGregorian){
_1f=_1f.toGregorian();
}
return _e.format(_1f,{selector:"date",datePattern:"w"});
},floorToDay:function(_20,_21){
return _12.floorToDay(_20,_21,this.dateClassObj);
},floorToMonth:function(_22,_23){
return _12.floorToMonth(_22,_23,this.dateClassObj);
},floorDate:function(_24,_25,_26,_27){
return _12.floor(_24,_25,_26,_27,this.dateClassObj);
},isToday:function(_28){
return _12.isToday(_28,this.dateClassObj);
},isStartOfDay:function(d){
return _12.isStartOfDay(d,this.dateClassObj,this.dateModule);
},isOverlapping:function(_29,_2a,_2b,_2c,_2d,_2e){
if(_2a==null||_2c==null||_2b==null||_2d==null){
return false;
}
var cal=_29.dateModule;
if(_2e){
if(cal.compare(_2a,_2d)==1||cal.compare(_2c,_2b)==1){
return false;
}
}else{
if(cal.compare(_2a,_2d)!=-1||cal.compare(_2c,_2b)!=-1){
return false;
}
}
return true;
},computeRangeOverlap:function(_2f,_30,_31,_32,_33,_34){
var cal=_2f.dateModule;
if(_30==null||_32==null||_31==null||_33==null){
return null;
}
var _35=cal.compare(_30,_33);
var _36=cal.compare(_32,_31);
if(_34){
if(_35==0||_35==1||_36==0||_36==1){
return null;
}
}else{
if(_35==1||_36==1){
return null;
}
}
return [this.newDate(cal.compare(_30,_32)>0?_30:_32,_2f),this.newDate(cal.compare(_31,_33)>0?_33:_31,_2f)];
},isSameDay:function(_37,_38){
if(_37==null||_38==null){
return false;
}
return _37.getFullYear()==_38.getFullYear()&&_37.getMonth()==_38.getMonth()&&_37.getDate()==_38.getDate();
},computeProjectionOnDate:function(_39,_3a,_3b,max){
var cal=_39.dateModule;
if(max<=0||cal.compare(_3b,_3a)==-1){
return 0;
}
var _3c=this.floorToDay(_3a,false,_39);
if(_3b.getDate()!=_3c.getDate()){
if(_3b.getMonth()==_3c.getMonth()){
if(_3b.getDate()<_3c.getDate()){
return 0;
}else{
if(_3b.getDate()>_3c.getDate()){
return max;
}
}
}else{
if(_3b.getFullYear()==_3c.getFullYear()){
if(_3b.getMonth()<_3c.getMonth()){
return 0;
}else{
if(_3b.getMonth()>_3c.getMonth()){
return max;
}
}
}else{
if(_3b.getFullYear()<_3c.getFullYear()){
return 0;
}else{
if(_3b.getFullYear()>_3c.getFullYear()){
return max;
}
}
}
}
}
var res;
if(this.isSameDay(_3a,_3b)){
var d=_2.clone(_3a);
var _3d=0;
if(_39.minHours!=null&&_39.minHours!=0){
d.setHours(_39.minHours);
_3d=d.getHours()*3600+d.getMinutes()*60+d.getSeconds();
}
d=_2.clone(_3a);
var _3e;
if(_39.maxHours==null||_39.maxHours==24){
_3e=86400;
}else{
d.setHours(_39.maxHours);
_3e=d.getHours()*3600+d.getMinutes()*60+d.getSeconds();
}
var _3f=_3b.getHours()*3600+_3b.getMinutes()*60+_3b.getSeconds()-_3d;
if(_3f<0){
return 0;
}
if(_3f>_3e){
return max;
}
res=(max*_3f)/(_3e-_3d);
}else{
if(_3b.getDate()<_3a.getDate()&&_3b.getMonth()==_3a.getMonth()){
return 0;
}
var d2=this.floorToDay(_3b);
var dp1=_39.dateModule.add(_3a,"day",1);
dp1=this.floorToDay(dp1,false,_39);
if(cal.compare(d2,_3a)==1&&cal.compare(d2,dp1)==0||cal.compare(d2,dp1)==1){
res=max;
}else{
res=0;
}
}
return res;
},getTime:function(e,x,y,_40){
return null;
},newDate:function(obj){
return _12.newDate(obj,this.dateClassObj);
},_isItemInView:function(_41){
var rd=this.renderData;
var cal=rd.dateModule;
if(cal.compare(_41.startTime,rd.startTime)==-1){
return false;
}
if(cal.compare(_41.endTime,rd.endTime)==1){
return false;
}
return true;
},_ensureItemInView:function(_42){
var rd=this.renderData;
var cal=rd.dateModule;
var _43=Math.abs(cal.difference(_42.startTime,_42.endTime,"millisecond"));
var _44=false;
if(cal.compare(_42.startTime,rd.startTime)==-1){
_42.startTime=rd.startTime;
_42.endTime=cal.add(_42.startTime,"millisecond",_43);
_44=true;
}else{
if(cal.compare(_42.endTime,rd.endTime)==1){
_42.endTime=rd.endTime;
_42.startTime=cal.add(_42.endTime,"millisecond",-_43);
_44=true;
}
}
return _44;
},scrollable:true,autoScroll:true,_autoScroll:function(gx,gy,_45){
return false;
},scrollMethod:"auto",_setScrollMethodAttr:function(_46){
if(this.scrollMethod!=_46){
this.scrollMethod=_46;
if(this._domScroll!==undefined){
if(this._domScroll){
_a.set(this.sheetContainer,this._cssPrefix+"transform","translateY(0px)");
}else{
this.scrollContainer.scrollTop=0;
}
}
delete this._domScroll;
var pos=this._getScrollPosition();
delete this._scrollPos;
this._setScrollPosition(pos);
}
},_startAutoScroll:function(_47){
var sp=this._scrollProps;
if(!sp){
sp=this._scrollProps={};
}
sp.scrollStep=_47;
if(!sp.isScrolling){
sp.isScrolling=true;
sp.scrollTimer=setInterval(_2.hitch(this,this._onScrollTimer_tick),10);
}
},_stopAutoScroll:function(){
var sp=this._scrollProps;
if(sp&&sp.isScrolling){
clearInterval(sp.scrollTimer);
sp.scrollTimer=null;
}
this._scrollProps=null;
},_onScrollTimer_tick:function(pos){
},_scrollPos:0,getCSSPrefix:function(){
if(_7("ie")){
return "-ms-";
}
if(_7("webkit")){
return "-webkit-";
}
if(_7("mozilla")){
return "-moz-";
}
if(_7("opera")){
return "-o-";
}
},_setScrollPosition:function(pos){
if(this._scrollPos==pos){
return;
}
if(this._domScroll===undefined){
var sm=this.get("scrollMethod");
if(sm==="auto"){
this._domScroll=!_7("ios")&&!_7("android")&&!_7("webkit");
}else{
this._domScroll=sm==="dom";
}
}
var _48=_c.getMarginBox(this.scrollContainer);
var _49=_c.getMarginBox(this.sheetContainer);
var max=_49.h-_48.h;
if(pos<0){
pos=0;
}else{
if(pos>max){
pos=max;
}
}
this._scrollPos=pos;
if(this._domScroll){
this.scrollContainer.scrollTop=pos;
}else{
if(!this._cssPrefix){
this._cssPrefix=this.getCSSPrefix();
}
_a.set(this.sheetContainer,this._cssPrefix+"transform","translateY(-"+pos+"px)");
}
},_getScrollPosition:function(){
return this._scrollPos;
},scrollView:function(dir){
},ensureVisibility:function(_4a,end,_4b,_4c,_4d){
},_getStoreAttr:function(){
if(this.owner){
return this.owner.get("store");
}
return this.store;
},_setItemsAttr:function(_4e){
this._set("items",_4e);
this.displayedItemsInvalidated=true;
},_refreshItemsRendering:function(){
var rd=this.renderData;
this._computeVisibleItems(rd);
this._layoutRenderers(rd);
},invalidateLayout:function(){
this._layoutRenderers(this.renderData);
},resize:function(){
},computeOverlapping:function(_4f,_50){
if(_4f.length==0){
return {numLanes:0,addedPassRes:[1]};
}
var _51=[];
for(var i=0;i<_4f.length;i++){
var _52=_4f[i];
this._layoutPass1(_52,_51);
}
var _53=null;
if(_50){
_53=_2.hitch(this,_50)(_51);
}
return {numLanes:_51.length,addedPassRes:_53};
},_layoutPass1:function(_54,_55){
var _56=true;
for(var i=0;i<_55.length;i++){
var _57=_55[i];
_56=false;
for(var j=0;j<_57.length&&!_56;j++){
if(_57[j].start<_54.end&&_54.start<_57[j].end){
_56=true;
_57[j].extent=1;
}
}
if(!_56){
_54.lane=i;
_54.extent=-1;
_57.push(_54);
return;
}
}
_55.push([_54]);
_54.lane=_55.length-1;
_54.extent=-1;
},_layoutInterval:function(_58,_59,_5a,end,_5b){
},layoutPriorityFunction:null,_sortItemsFunction:function(a,b){
var res=this.dateModule.compare(a.startTime,b.startTime);
if(res==0){
res=-1*this.dateModule.compare(a.endTime,b.endTime);
}
return res;
},_layoutRenderers:function(_5c){
if(!_5c.items){
return;
}
this._recycleItemRenderers();
var cal=_5c.dateModule;
var _5d=this.newDate(_5c.startTime);
var _5e=_2.clone(_5d);
var _5f;
var _60=_5c.items.concat();
var _61=[],_62;
var _63=0;
while(cal.compare(_5d,_5c.endTime)==-1&&_60.length>0){
_5f=cal.add(_5d,this._layoutUnit,this._layoutStep);
_5f=this.floorToDay(_5f,true,_5c);
var _64=_2.clone(_5f);
if(_5c.minHours){
_5e.setHours(_5c.minHours);
}
if(_5c.maxHours&&_5c.maxHours!=24){
_64=cal.add(_5f,"day",-1);
_64=this.floorToDay(_64,true,_5c);
_64.setHours(_5c.maxHours);
}
_62=_3.filter(_60,function(_65){
var r=this.isOverlapping(_5c,_65.startTime,_65.endTime,_5e,_64);
if(r){
if(cal.compare(_65.endTime,_64)==1){
_61.push(_65);
}
}else{
_61.push(_65);
}
return r;
},this);
_60=_61;
_61=[];
if(_62.length>0){
_62.sort(_2.hitch(this,this.layoutPriorityFunction?this.layoutPriorityFunction:this._sortItemsFunction));
this._layoutInterval(_5c,_63,_5e,_64,_62);
}
_5d=_5f;
_5e=_2.clone(_5d);
_63++;
}
this._onRenderersLayoutDone(this);
},_recycleItemRenderers:function(_66){
while(this.rendererList.length>0){
this._recycleRenderer(this.rendererList.pop(),_66);
}
this.itemToRenderer={};
},rendererPool:null,rendererList:null,itemToRenderer:null,getRenderers:function(_67){
if(_67==null||_67.id==null){
return null;
}
var _68=this.itemToRenderer[_67.id];
return _68==null?null:_68.concat();
},_rendererHandles:{},itemToRendererKindFunc:null,_itemToRendererKind:function(_69){
if(this.itemToRendererKindFunc){
return this.itemToRendererKindFunc(_69);
}
return this._defaultItemToRendererKindFunc(_69);
},_defaultItemToRendererKindFunc:function(_6a){
return null;
},_createRenderer:function(_6b,_6c,_6d,_6e){
if(_6b!=null&&_6c!=null&&_6d!=null){
var res,_6f;
var _70=this.rendererPool[_6c];
if(_70!=null){
res=_70.shift();
}
if(res==null){
_6f=new _6d;
var _71=_b.create("div");
_71.className="dojoxCalendarEventContainer "+_6e;
_71.appendChild(_6f.domNode);
res={renderer:_6f,container:_6f.domNode,kind:_6c};
this._onRendererCreated(res);
}else{
_6f=res.renderer;
this._onRendererReused(_6f);
}
_6f.owner=this;
_6f.set("rendererKind",_6c);
_6f.set("item",_6b);
var _72=this.itemToRenderer[_6b.id];
if(_72==null){
this.itemToRenderer[_6b.id]=_72=[];
}
_72.push(res);
this.rendererList.push(res);
return res;
}
return null;
},_onRendererCreated:function(_73){
this.onRendererCreated(_73);
var _74=this.owner&&this.owner.owner?this.owner.owner:this.owner;
if(_74){
_74.onRendererCreated(_73);
}
},onRendererCreated:function(_75){
},_onRendererRecycled:function(_76){
this.onRendererRecycled(_76);
var _77=this.owner&&this.owner.owner?this.owner.owner:this.owner;
if(_77){
_77.onRendererRecycled(_76);
}
},onRendererRecycled:function(_78){
},_onRendererReused:function(_79){
this.onRendererReused(_79);
var _7a=this.owner&&this.owner.owner?this.owner.owner:this.owner;
if(_7a){
_7a.onRendererReused(_79);
}
},onRendererReused:function(_7b){
},_onRendererDestroyed:function(_7c){
this.onRendererDestroyed(_7c);
var _7d=this.owner&&this.owner.owner?this.owner.owner:this.owner;
if(_7d){
_7d.onRendererDestroyed(_7c);
}
},onRendererDestroyed:function(_7e){
},_onRenderersLayoutDone:function(_7f){
this.onRenderersLayoutDone(_7f);
if(this.owner!=null){
this.owner.onRenderersLayoutDone(_7f);
}
},onRenderersLayoutDone:function(_80){
},_recycleRenderer:function(_81,_82){
this._onRendererRecycled(_81);
var _83=this.rendererPool[_81.kind];
if(_83==null){
this.rendererPool[_81.kind]=[_81];
}else{
_83.push(_81);
}
if(_82){
_81.container.parentNode.removeChild(_81.container);
}
_a.set(_81.container,"display","none");
_81.renderer.owner=null;
_81.renderer.set("item",null);
},_destroyRenderer:function(_84){
this._onRendererDestroyed(_84);
var ir=_84.renderer;
_3.forEach(ir.__handles,function(_85){
_85.remove();
});
if(ir["destroy"]){
ir.destroy();
}
_6.destroy(_84.container);
},_destroyRenderersByKind:function(_86){
var _87=[];
for(var i=0;i<this.rendererList.length;i++){
var ir=this.rendererList[i];
if(ir.kind==_86){
this._destroyRenderer(ir);
}else{
_87.push(ir);
}
}
this.rendererList=_87;
var _88=this.rendererPool[_86];
if(_88){
while(_88.length>0){
this._destroyRenderer(_88.pop());
}
}
},_updateEditingCapabilities:function(_89,_8a){
var _8b=this.isItemMoveEnabled(_89,_8a.rendererKind);
var _8c=this.isItemResizeEnabled(_89,_8a.rendererKind);
var _8d=false;
if(_8b!=_8a.get("moveEnabled")){
_8a.set("moveEnabled",_8b);
_8d=true;
}
if(_8c!=_8a.get("resizeEnabled")){
_8a.set("resizeEnabled",_8c);
_8d=true;
}
if(_8d){
_8a.updateRendering();
}
},updateRenderers:function(obj,_8e){
if(obj==null){
return;
}
var _8f=_2.isArray(obj)?obj:[obj];
for(var i=0;i<_8f.length;i++){
var _90=_8f[i];
if(_90==null||_90.id==null){
continue;
}
var _91=this.itemToRenderer[_90.id];
if(_91==null){
continue;
}
var _92=this.isItemSelected(_90);
var _93=this.isItemHovered(_90);
var _94=this.isItemBeingEdited(_90);
var _95=this.showFocus?this.isItemFocused(_90):false;
for(var j=0;j<_91.length;j++){
var _96=_91[j].renderer;
_96.set("hovered",_93);
_96.set("selected",_92);
_96.set("edited",_94);
_96.set("focused",_95);
this.applyRendererZIndex(_90,_91[j],_93,_92,_94,_95);
if(!_8e){
_96.set("item",_90);
if(_96.updateRendering){
_96.updateRendering();
}
}
}
}
},applyRendererZIndex:function(_97,_98,_99,_9a,_9b,_9c){
_a.set(_98.container,{"zIndex":_9b||_9a?20:_97.lane==undefined?0:_97.lane});
},getIdentity:function(_9d){
return this.owner?this.owner.getIdentity(_9d):_9d.id;
},_setHoveredItem:function(_9e,_9f){
if(this.owner){
this.owner._setHoveredItem(_9e,_9f);
return;
}
if(this.hoveredItem&&_9e&&this.hoveredItem.id!=_9e.id||_9e==null||this.hoveredItem==null){
var old=this.hoveredItem;
this.hoveredItem=_9e;
this.updateRenderers([old,this.hoveredItem],true);
if(_9e&&_9f){
this._updateEditingCapabilities(_9e,_9f);
}
}
},hoveredItem:null,isItemHovered:function(_a0){
if(this._isEditing&&this._edProps){
return _a0.id==this._edProps.editedItem.id;
}else{
return this.owner?this.owner.isItemHovered(_a0):this.hoveredItem!=null&&this.hoveredItem.id==_a0.id;
}
},isItemFocused:function(_a1){
return this._isItemFocused?this._isItemFocused(_a1):false;
},_setSelectionModeAttr:function(_a2){
if(this.owner){
this.owner.set("selectionMode",_a2);
}else{
this.inherited(arguments);
}
},_getSelectionModeAttr:function(_a3){
if(this.owner){
return this.owner.get("selectionMode");
}else{
return this.inherited(arguments);
}
},_setSelectedItemAttr:function(_a4){
if(this.owner){
this.owner.set("selectedItem",_a4);
}else{
this.inherited(arguments);
}
},_getSelectedItemAttr:function(_a5){
if(this.owner){
return this.owner.get("selectedItem");
}else{
return this.selectedItem;
}
},_setSelectedItemsAttr:function(_a6){
if(this.owner){
this.owner.set("selectedItems",_a6);
}else{
this.inherited(arguments);
}
},_getSelectedItemsAttr:function(){
if(this.owner){
return this.owner.get("selectedItems");
}else{
return this.inherited(arguments);
}
},isItemSelected:function(_a7){
if(this.owner){
return this.owner.isItemSelected(_a7);
}else{
return this.inherited(arguments);
}
},selectFromEvent:function(e,_a8,_a9,_aa){
if(this.owner){
this.owner.selectFromEvent(e,_a8,_a9,_aa);
}else{
this.inherited(arguments);
}
},setItemSelected:function(_ab,_ac){
if(this.owner){
this.owner.setItemSelected(_ab,_ac);
}else{
this.inherited(arguments);
}
},createItemFunc:null,_getCreateItemFuncAttr:function(){
if(this.owner){
return this.owner.get("createItemFunc");
}else{
return this.createItemFunc;
}
},createOnGridClick:false,_getCreateOnGridClickAttr:function(){
if(this.owner){
return this.owner.get("createOnGridClick");
}else{
return this.createOnGridClick;
}
},_gridMouseDown:false,_onGridMouseDown:function(e){
this._gridMouseDown=true;
this.showFocus=false;
if(this._isEditing){
this._endItemEditing("mouse",false);
}
this._doEndItemEditing(this.owner,"mouse");
this.set("focusedItem",null);
this.selectFromEvent(e,null,null,true);
if(this._setTabIndexAttr){
this[this._setTabIndexAttr].focus();
}
if(this._onRendererHandleMouseDown){
var f=this.get("createItemFunc");
if(!f){
return;
}
var _ad=f(this,this.getTime(e),e);
var _ae=this.get("store");
if(!_ad||_ae==null){
return;
}
_ae.put(_ad);
var _af=this.getRenderers(_ad);
if(_af&&_af.length>0){
var _b0=_af[0];
if(_b0){
this._onRendererHandleMouseDown(e,_b0.renderer,"resizeEnd");
}
}
}
},_onGridMouseMove:function(e){
},_onGridMouseUp:function(e){
},_onGridTouchStart:function(e){
var p=this._edProps;
this._gridProps={event:e,fromItem:this.isAscendantHasClass(e.target,this.eventContainer,"dojoxCalendarEventContainer")};
if(this._isEditing){
if(this._gridProps){
this._gridProps.editingOnStart=true;
}
_2.mixin(p,this._getTouchesOnRenderers(e,p.editedItem));
if(p.touchesLen==0){
if(p&&p.endEditingTimer){
clearTimeout(p.endEditingTimer);
p.endEditingTimer=null;
}
this._endItemEditing("touch",false);
}
}
this._doEndItemEditing(this.owner,"touch");
_5.stop(e);
},_doEndItemEditing:function(obj,_b1){
if(obj&&obj._isEditing){
p=obj._edProps;
if(p&&p.endEditingTimer){
clearTimeout(p.endEditingTimer);
p.endEditingTimer=null;
}
obj._endItemEditing(_b1,false);
}
},_onGridTouchEnd:function(e){
},_onGridTouchMove:function(e){
},__fixEvt:function(e){
return e;
},_dispatchCalendarEvt:function(e,_b2){
e=this.__fixEvt(e);
this[_b2](e);
if(this.owner){
this.owner[_b2](e);
}
return e;
},_onGridClick:function(e){
if(!e.triggerEvent){
e={date:this.getTime(e),triggerEvent:e};
}
this._dispatchCalendarEvt(e,"onGridClick");
},onGridClick:function(e){
},_onGridDoubleClick:function(e){
if(!e.triggerEvent){
e={date:this.getTime(e),triggerEvent:e};
}
this._dispatchCalendarEvt(e,"onGridDoubleClick");
},onGridDoubleClick:function(e){
},_onItemClick:function(e){
this._dispatchCalendarEvt(e,"onItemClick");
},onItemClick:function(e){
},_onItemDoubleClick:function(e){
this._dispatchCalendarEvt(e,"onItemDoubleClick");
},onItemDoubleClick:function(e){
},_onItemContextMenu:function(e){
this._dispatchCalendarEvt(e,"onItemContextMenu");
},onItemContextMenu:function(e){
},_getStartEndRenderers:function(_b3){
var _b4=this.itemToRenderer[_b3.id];
if(_b4==null){
return;
}
if(_b4.length==1){
var _b5=_b4[0].renderer;
return [_b5,_b5];
}
var rd=this.renderData;
var _b6=false;
var _b7=false;
var res=[];
for(var i=0;i<_b4.length;i++){
var ir=_b4[i].renderer;
if(!_b6){
_b6=rd.dateModule.compare(ir.item.range[0],ir.item.startTime)==0;
res[0]=ir;
}
if(!_b7){
_b7=rd.dateModule.compare(ir.item.range[1],ir.item.endTime)==0;
res[1]=ir;
}
if(_b6&&_b7){
break;
}
}
return res;
},editable:true,moveEnabled:true,resizeEnabled:true,isItemEditable:function(_b8,_b9){
return this.editable&&(this.owner?this.owner.isItemEditable():true);
},isItemMoveEnabled:function(_ba,_bb){
return this.isItemEditable(_ba,_bb)&&this.moveEnabled&&(this.owner?this.owner.isItemMoveEnabled(_ba,_bb):true);
},isItemResizeEnabled:function(_bc,_bd){
return this.isItemEditable(_bc,_bd)&&this.resizeEnabled&&(this.owner?this.owner.isItemResizeEnabled(_bc,_bd):true);
},_isEditing:false,isItemBeingEdited:function(_be){
return this._isEditing&&this._edProps&&this._edProps.editedItem&&this._edProps.editedItem.id==_be.id;
},_setEditingProperties:function(_bf){
this._edProps=_bf;
},_startItemEditing:function(_c0,_c1){
this._isEditing=true;
var p=this._edProps;
p.editedItem=_c0;
p.eventSource=_c1;
p.secItem=this._secondarySheet?this._findRenderItem(_c0.id,this._secondarySheet.renderData.items):null;
p.ownerItem=this.owner?this._findRenderItem(_c0.id,this.items):null;
if(!p.liveLayout){
p.editSaveStartTime=_c0.startTime;
p.editSaveEndTime=_c0.endTime;
p.editItemToRenderer=this.itemToRenderer;
p.editItems=this.renderData.items;
p.editRendererList=this.rendererList;
this.renderData.items=[p.editedItem];
var id=p.editedItem.id;
this.itemToRenderer={};
this.rendererList=[];
var _c2=p.editItemToRenderer[id];
p.editRendererIndices=[];
_3.forEach(_c2,_2.hitch(this,function(ir,i){
if(this.itemToRenderer[id]==null){
this.itemToRenderer[id]=[ir];
}else{
this.itemToRenderer[id].push(ir);
}
this.rendererList.push(ir);
}));
p.editRendererList=_3.filter(p.editRendererList,function(ir){
return ir!=null&&ir.renderer.item.id!=id;
});
delete p.editItemToRenderer[id];
}
this._layoutRenderers(this.renderData);
this._onItemEditBegin({item:_c0,eventSource:_c1});
},_onItemEditBegin:function(e){
this._editStartTimeSave=this.newDate(e.item.startTime);
this._editEndTimeSave=this.newDate(e.item.endTime);
this._dispatchCalendarEvt(e,"onItemEditBegin");
},onItemEditBegin:function(e){
},_endItemEditing:function(_c3,_c4){
this._isEditing=false;
var p=this._edProps;
_3.forEach(p.handles,function(_c5){
_c5.remove();
});
if(!p.liveLayout){
this.renderData.items=p.editItems;
this.rendererList=p.editRendererList.concat(this.rendererList);
_2.mixin(this.itemToRenderer,p.editItemToRenderer);
}
var _c6=this.get("store");
this._onItemEditEnd(_2.mixin(this._createItemEditEvent(),{item:this.renderItemToItem(p.editedItem,_c6),renderItem:p.editedItem,eventSource:_c3,completed:!_c4}));
this._layoutRenderers(this.renderData);
this._edProps=null;
},_onItemEditEnd:function(e){
this._dispatchCalendarEvt(e,"onItemEditEnd");
if(!e.isDefaultPrevented()){
if(e.completed){
var _c7=this.get("store");
_c7.put(e.item);
}else{
e.renderItem.startTime=this._editStartTimeSave;
e.renderItem.endTime=this._editEndTimeSave;
}
}
},onItemEditEnd:function(e){
},_createItemEditEvent:function(){
var e={cancelable:true,bubbles:false,__defaultPrevent:false};
e.preventDefault=function(){
this.__defaultPrevented=true;
};
e.isDefaultPrevented=function(){
return this.__defaultPrevented;
};
return e;
},_startItemEditingGesture:function(_c8,_c9,_ca,e){
var p=this._edProps;
if(!p||p.editedItem==null){
return;
}
this._editingGesture=true;
var _cb=p.editedItem;
p.editKind=_c9;
this._onItemEditBeginGesture(this.__fixEvt(_2.mixin(this._createItemEditEvent(),{item:_cb,startTime:_cb.startTime,endTime:_cb.endTime,editKind:_c9,rendererKind:p.rendererKind,triggerEvent:e,dates:_c8,eventSource:_ca})));
p.itemBeginDispatched=true;
},_onItemEditBeginGesture:function(e){
var p=this._edProps;
var _cc=p.editedItem;
var _cd=e.dates;
p.editingTimeFrom=[];
p.editingTimeFrom[0]=_cd[0];
p.editingItemRefTime=[];
p.editingItemRefTime[0]=this.newDate(p.editKind=="resizeEnd"?_cc.endTime:_cc.startTime);
if(p.editKind=="resizeBoth"){
p.editingTimeFrom[1]=_cd[1];
p.editingItemRefTime[1]=this.newDate(_cc.endTime);
}
var cal=this.renderData.dateModule;
p.inViewOnce=this._isItemInView(_cc);
if(p.rendererKind=="label"||this.roundToDay){
p._itemEditBeginSave=this.newDate(_cc.startTime);
p._itemEditEndSave=this.newDate(_cc.endTime);
}
p._initDuration=cal.difference(_cc.startTime,_cc.endTime,_cc.allDay?"day":"millisecond");
this._dispatchCalendarEvt(e,"onItemEditBeginGesture");
if(!e.isDefaultPrevented()){
if(e.eventSource=="mouse"){
var _ce=e.editKind=="move"?"move":this.resizeCursor;
p.editLayer=_b.create("div",{style:"position: absolute; left:0; right:0; bottom:0; top:0; z-index:30; tabIndex:-1; background-image:url('"+this._blankGif+"'); cursor: "+_ce,onresizestart:function(e){
return false;
},onselectstart:function(e){
return false;
}},this.domNode);
p.editLayer.focus();
}
}
},onItemEditBeginGesture:function(e){
},_waDojoxAddIssue:function(d,_cf,_d0){
var cal=this.renderData.dateModule;
if(this._calendar!="gregorian"&&_d0<0){
var gd=d.toGregorian();
gd=_d.add(gd,_cf,_d0);
return new this.renderData.dateClassObj(gd);
}else{
return cal.add(d,_cf,_d0);
}
},_computeItemEditingTimes:function(_d1,_d2,_d3,_d4,_d5){
var cal=this.renderData.dateModule;
var p=this._edProps;
var _d6=cal.difference(p.editingTimeFrom[0],_d4[0],"millisecond");
_d4[0]=this._waDojoxAddIssue(p.editingItemRefTime[0],"millisecond",_d6);
if(_d2=="resizeBoth"){
_d6=cal.difference(p.editingTimeFrom[1],_d4[1],"millisecond");
_d4[1]=this._waDojoxAddIssue(p.editingItemRefTime[1],"millisecond",_d6);
}
return _d4;
},_moveOrResizeItemGesture:function(_d7,_d8,e){
if(!this._isEditing||_d7[0]==null){
return;
}
var p=this._edProps;
var _d9=p.editedItem;
var rd=this.renderData;
var cal=rd.dateModule;
var _da=p.editKind;
var _db=[_d7[0]];
if(_da=="resizeBoth"){
_db[1]=_d7[1];
}
_db=this._computeItemEditingTimes(_d9,p.editKind,p.rendererKind,_db,_d8);
var _dc=_db[0];
var _dd=false;
var _de=_2.clone(_d9.startTime);
var _df=_2.clone(_d9.endTime);
var _e0=p.eventSource=="keyboard"?false:this.allowStartEndSwap;
if(_da=="move"){
if(cal.compare(_d9.startTime,_dc)!=0){
var _e1=cal.difference(_d9.startTime,_d9.endTime,"millisecond");
_d9.startTime=this.newDate(_dc);
_d9.endTime=cal.add(_d9.startTime,"millisecond",_e1);
_dd=true;
}
}else{
if(_da=="resizeStart"){
if(cal.compare(_d9.startTime,_dc)!=0){
if(cal.compare(_d9.endTime,_dc)!=-1){
_d9.startTime=this.newDate(_dc);
}else{
if(_e0){
_d9.startTime=this.newDate(_d9.endTime);
_d9.endTime=this.newDate(_dc);
p.editKind=_da="resizeEnd";
if(_d8=="touch"){
p.resizeEndTouchIndex=p.resizeStartTouchIndex;
p.resizeStartTouchIndex=-1;
}
}else{
_d9.startTime=this.newDate(_d9.endTime);
_d9.startTime.setHours(_dc.getHours());
_d9.startTime.setMinutes(_dc.getMinutes());
_d9.startTime.setSeconds(_dc.getSeconds());
}
}
_dd=true;
}
}else{
if(_da=="resizeEnd"){
if(cal.compare(_d9.endTime,_dc)!=0){
if(cal.compare(_d9.startTime,_dc)!=1){
_d9.endTime=this.newDate(_dc);
}else{
if(_e0){
_d9.endTime=this.newDate(_d9.startTime);
_d9.startTime=this.newDate(_dc);
p.editKind=_da="resizeStart";
if(_d8=="touch"){
p.resizeStartTouchIndex=p.resizeEndTouchIndex;
p.resizeEndTouchIndex=-1;
}
}else{
_d9.endTime=this.newDate(_d9.startTime);
_d9.endTime.setHours(_dc.getHours());
_d9.endTime.setMinutes(_dc.getMinutes());
_d9.endTime.setSeconds(_dc.getSeconds());
}
}
_dd=true;
}
}else{
if(_da=="resizeBoth"){
_dd=true;
var _e2=this.newDate(_dc);
var end=this.newDate(_db[1]);
if(cal.compare(_e2,end)!=-1){
if(_e0){
var t=_e2;
_e2=end;
end=t;
}else{
_dd=false;
}
}
if(_dd){
_d9.startTime=_e2;
_d9.endTime=end;
}
}else{
return false;
}
}
}
}
if(!_dd){
return false;
}
var evt=_2.mixin(this._createItemEditEvent(),{item:_d9,startTime:_d9.startTime,endTime:_d9.endTime,editKind:_da,rendererKind:p.rendererKind,triggerEvent:e,eventSource:_d8});
if(_da=="move"){
this._onItemEditMoveGesture(evt);
}else{
this._onItemEditResizeGesture(evt);
}
if(cal.compare(_d9.startTime,_d9.endTime)==1){
var tmp=_d9.startTime;
_d9.startTime=_d9.startTime;
_d9.endTime=tmp;
}
_dd=cal.compare(_de,_d9.startTime)!=0||cal.compare(_df,_d9.endTime)!=0;
if(!_dd){
return false;
}
this._layoutRenderers(this.renderData);
if(p.liveLayout&&p.secItem!=null){
p.secItem.startTime=_d9.startTime;
p.secItem.endTime=_d9.endTime;
this._secondarySheet._layoutRenderers(this._secondarySheet.renderData);
}else{
if(p.ownerItem!=null&&this.owner.liveLayout){
p.ownerItem.startTime=_d9.startTime;
p.ownerItem.endTime=_d9.endTime;
this.owner._layoutRenderers(this.owner.renderData);
}
}
return true;
},_findRenderItem:function(id,_e3){
_e3=_e3||this.renderData.items;
for(var i=0;i<_e3.length;i++){
if(_e3[i].id==id){
return _e3[i];
}
}
return null;
},_onItemEditMoveGesture:function(e){
this._dispatchCalendarEvt(e,"onItemEditMoveGesture");
if(!e.isDefaultPrevented()){
var p=e.source._edProps;
var rd=this.renderData;
var cal=rd.dateModule;
var _e4,_e5;
if(p.rendererKind=="label"||(this.roundToDay&&!e.item.allDay)){
_e4=this.floorToDay(e.item.startTime,false,rd);
_e4.setHours(p._itemEditBeginSave.getHours());
_e4.setMinutes(p._itemEditBeginSave.getMinutes());
_e5=cal.add(_e4,"millisecond",p._initDuration);
}else{
if(e.item.allDay){
_e4=this.floorToDay(e.item.startTime,true);
_e5=cal.add(_e4,"day",p._initDuration);
}else{
_e4=this.floorDate(e.item.startTime,this.snapUnit,this.snapSteps);
_e5=cal.add(_e4,"millisecond",p._initDuration);
}
}
e.item.startTime=_e4;
e.item.endTime=_e5;
if(!p.inViewOnce){
p.inViewOnce=this._isItemInView(e.item);
}
if(p.inViewOnce&&this.stayInView){
this._ensureItemInView(e.item);
}
}
},_DAY_IN_MILLISECONDS:24*60*60*1000,onItemEditMoveGesture:function(e){
},_onItemEditResizeGesture:function(e){
this._dispatchCalendarEvt(e,"onItemEditResizeGesture");
if(!e.isDefaultPrevented()){
var p=e.source._edProps;
var rd=this.renderData;
var cal=rd.dateModule;
var _e6=e.item.startTime;
var _e7=e.item.endTime;
if(e.editKind=="resizeStart"){
if(e.item.allDay){
_e6=this.floorToDay(e.item.startTime,false,this.renderData);
}else{
if(this.roundToDay){
_e6=this.floorToDay(e.item.startTime,false,rd);
_e6.setHours(p._itemEditBeginSave.getHours());
_e6.setMinutes(p._itemEditBeginSave.getMinutes());
}else{
_e6=this.floorDate(e.item.startTime,this.snapUnit,this.snapSteps);
}
}
}else{
if(e.editKind=="resizeEnd"){
if(e.item.allDay){
if(!this.isStartOfDay(e.item.endTime)){
_e7=this.floorToDay(e.item.endTime,false,this.renderData);
_e7=cal.add(_e7,"day",1);
}
}else{
if(this.roundToDay){
_e7=this.floorToDay(e.item.endTime,false,rd);
_e7.setHours(p._itemEditEndSave.getHours());
_e7.setMinutes(p._itemEditEndSave.getMinutes());
}else{
_e7=this.floorDate(e.item.endTime,this.snapUnit,this.snapSteps);
if(e.eventSource=="mouse"){
_e7=cal.add(_e7,this.snapUnit,this.snapSteps);
}
}
}
}else{
_e6=this.floorDate(e.item.startTime,this.snapUnit,this.snapSteps);
_e7=this.floorDate(e.item.endTime,this.snapUnit,this.snapSteps);
_e7=cal.add(_e7,this.snapUnit,this.snapSteps);
}
}
e.item.startTime=_e6;
e.item.endTime=_e7;
var _e8=e.item.allDay||p._initDuration>=this._DAY_IN_MILLISECONDS&&!this.allowResizeLessThan24H;
this.ensureMinimalDuration(this.renderData,e.item,_e8?"day":this.minDurationUnit,_e8?1:this.minDurationSteps,e.editKind);
if(!p.inViewOnce){
p.inViewOnce=this._isItemInView(e.item);
}
if(p.inViewOnce&&this.stayInView){
this._ensureItemInView(e.item);
}
}
},onItemEditResizeGesture:function(e){
},_endItemEditingGesture:function(_e9,e){
if(!this._isEditing){
return;
}
this._editingGesture=false;
var p=this._edProps;
var _ea=p.editedItem;
p.itemBeginDispatched=false;
this._onItemEditEndGesture(_2.mixin(this._createItemEditEvent(),{item:_ea,startTime:_ea.startTime,endTime:_ea.endTime,editKind:p.editKind,rendererKind:p.rendererKind,triggerEvent:e,eventSource:_e9}));
},_onItemEditEndGesture:function(e){
var p=this._edProps;
delete p._itemEditBeginSave;
delete p._itemEditEndSave;
this._dispatchCalendarEvt(e,"onItemEditEndGesture");
if(!e.isDefaultPrevented()){
if(p.editLayer){
if(_7("ie")){
p.editLayer.style.cursor="default";
}
setTimeout(_2.hitch(this,function(){
if(this.domNode){
this.domNode.focus();
p.editLayer.parentNode.removeChild(p.editLayer);
p.editLayer=null;
}
}),10);
}
}
},onItemEditEndGesture:function(e){
},ensureMinimalDuration:function(_eb,_ec,_ed,_ee,_ef){
var _f0;
var cal=_eb.dateModule;
if(_ef=="resizeStart"){
_f0=cal.add(_ec.endTime,_ed,-_ee);
if(cal.compare(_ec.startTime,_f0)==1){
_ec.startTime=_f0;
}
}else{
_f0=cal.add(_ec.startTime,_ed,_ee);
if(cal.compare(_ec.endTime,_f0)==-1){
_ec.endTime=_f0;
}
}
},doubleTapDelay:300,snapUnit:"minute",snapSteps:15,minDurationUnit:"hour",minDurationSteps:1,liveLayout:false,stayInView:true,allowStartEndSwap:true,allowResizeLessThan24H:false});
});
