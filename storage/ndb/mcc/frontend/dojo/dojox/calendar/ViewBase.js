//>>built
define("dojox/calendar/ViewBase",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/window","dojo/_base/event","dojo/_base/html","dojo/sniff","dojo/query","dojo/dom","dojo/dom-style","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/on","dojo/date","dojo/date/locale","dojo/when","dijit/_WidgetBase","dojox/widget/_Invalidating","dojox/widget/Selection","./time","./StoreMixin","./StoreManager","./RendererManager"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,on,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17){
return _1("dojox.calendar.ViewBase",[_11,_15,_12,_13],{datePackage:_e,_calendar:"gregorian",viewKind:null,_layoutStep:1,_layoutUnit:"day",resizeCursor:"n-resize",formatItemTimeFunc:null,_cssDays:["Sun","Mon","Tue","Wed","Thu","Fri","Sat"],_getFormatItemTimeFuncAttr:function(){
if(this.formatItemTimeFunc){
return this.formatItemTimeFunc;
}
if(this.owner!=null){
return this.owner.get("formatItemTimeFunc");
}
},_viewHandles:null,doubleTapDelay:300,constructor:function(_18){
_18=_18||{};
this._calendar=_18.datePackage?_18.datePackage.substr(_18.datePackage.lastIndexOf(".")+1):this._calendar;
this.dateModule=_18.datePackage?_2.getObject(_18.datePackage,false):_e;
this.dateClassObj=this.dateModule.Date||Date;
this.dateLocaleModule=_18.datePackage?_2.getObject(_18.datePackage+".locale",false):_f;
this._viewHandles=[];
this.storeManager=new _16({owner:this,_ownerItemsProperty:"items"});
this.storeManager.on("layoutInvalidated",_2.hitch(this,this._refreshItemsRendering));
this.storeManager.on("dataLoaded",_2.hitch(this,function(_19){
this.set("items",_19);
}));
this.storeManager.on("renderersInvalidated",_2.hitch(this,function(_1a){
this.updateRenderers(_1a);
}));
this.rendererManager=new _17({owner:this});
this.rendererManager.on("rendererCreated",_2.hitch(this,this._onRendererCreated));
this.rendererManager.on("rendererReused",_2.hitch(this,this._onRendererReused));
this.rendererManager.on("rendererRecycled",_2.hitch(this,this._onRendererRecycled));
this.rendererManager.on("rendererDestroyed",_2.hitch(this,this._onRendererDestroyed));
this.decorationStoreManager=new _16({owner:this,_ownerItemsProperty:"decorationItems"});
this.decorationStoreManager.on("layoutInvalidated",_2.hitch(this,this._refreshDecorationItemsRendering));
this.decorationStoreManager.on("dataLoaded",_2.hitch(this,function(_1b){
this.set("decorationItems",_1b);
}));
this.decorationRendererManager=new _17({owner:this});
this._setupDayRefresh();
},destroy:function(_1c){
this.rendererManager.destroy();
this.decorationRendererManager.destroy();
while(this._viewHandles.length>0){
this._viewHandles.pop().remove();
}
this.inherited(arguments);
},_setupDayRefresh:function(){
var now=this.newDate(new Date());
var d=_14.floor(now,"day",1,this.dateClassObj);
var d=this.dateModule.add(d,"day",1);
if(d.getHours()==23){
d=this.dateModule.add(d,"hour",2);
}else{
d=_14.floorToDay(d,true,this.dateClassObj);
}
setTimeout(_2.hitch(this,function(){
if(!this._isEditing){
this.refreshRendering(true);
}
this._setupDayRefresh();
}),d.getTime()-now.getTime()+5000);
},resize:function(_1d){
if(_1d){
_d.setMarginBox(this.domNode,_1d);
}
},beforeActivate:function(){
},afterActivate:function(){
},beforeDeactivate:function(){
},afterDeactivate:function(){
},_getTopOwner:function(){
var p=this;
while(p.owner!=undefined){
p=p.owner;
}
return p;
},_createRenderData:function(){
},_validateProperties:function(){
},_setText:function(_1e,_1f,_20){
if(_1f!=null){
if(!_20&&_1e.hasChildNodes()){
_1e.childNodes[0].childNodes[0].nodeValue=_1f;
}else{
while(_1e.hasChildNodes()){
_1e.removeChild(_1e.lastChild);
}
var _21=_4.doc.createElement("span");
if(_7("dojo-bidi")){
this.applyTextDir(_21,_1f);
}
if(_20){
_21.innerHTML=_1f;
}else{
_21.appendChild(_4.doc.createTextNode(_1f));
}
_1e.appendChild(_21);
}
}
},isAscendantHasClass:function(_22,_23,_24){
while(_22!=_23&&_22!=document){
if(_b.contains(_22,_24)){
return true;
}
_22=_22.parentNode;
}
return false;
},isWeekEnd:function(_25){
return _f.isWeekend(_25);
},getWeekNumberLabel:function(_26){
if(_26.toGregorian){
_26=_26.toGregorian();
}
return _f.format(_26,{selector:"date",datePattern:"w"});
},addAndFloor:function(_27,_28,_29){
var d=this.dateModule.add(_27,_28,_29);
if(d.getHours()==23){
d=this.dateModule.add(d,"hour",2);
}else{
d=_14.floorToDay(d,true,this.dateClassObj);
}
return d;
},floorToDay:function(_2a,_2b){
return _14.floorToDay(_2a,_2b,this.dateClassObj);
},floorToMonth:function(_2c,_2d){
return _14.floorToMonth(_2c,_2d,this.dateClassObj);
},floorDate:function(_2e,_2f,_30,_31){
return _14.floor(_2e,_2f,_30,_31,this.dateClassObj);
},isToday:function(_32){
return _14.isToday(_32,this.dateClassObj);
},isStartOfDay:function(d){
return _14.isStartOfDay(d,this.dateClassObj,this.dateModule);
},isOverlapping:function(_33,_34,_35,_36,_37,_38){
return _14.isOverlapping(_33,_34,_35,_36,_37,_38);
},computeRangeOverlap:function(_39,_3a,_3b,_3c,_3d,_3e){
var cal=_39.dateModule;
if(_3a==null||_3c==null||_3b==null||_3d==null){
return null;
}
var _3f=cal.compare(_3a,_3d);
var _40=cal.compare(_3c,_3b);
if(_3e){
if(_3f==0||_3f==1||_40==0||_40==1){
return null;
}
}else{
if(_3f==1||_40==1){
return null;
}
}
return [this.newDate(cal.compare(_3a,_3c)>0?_3a:_3c,_39),this.newDate(cal.compare(_3b,_3d)>0?_3d:_3b,_39)];
},isSameDay:function(_41,_42){
if(_41==null||_42==null){
return false;
}
return _41.getFullYear()==_42.getFullYear()&&_41.getMonth()==_42.getMonth()&&_41.getDate()==_42.getDate();
},computeProjectionOnDate:function(_43,_44,_45,max){
var cal=_43.dateModule;
var _46=_43.minHours;
var _47=_43.maxHours;
if(max<=0||cal.compare(_45,_44)==-1){
return 0;
}
var gt=function(d){
return d.getHours()*3600+d.getMinutes()*60+d.getSeconds();
};
var _48=this.floorToDay(_44,false,_43);
if(_45.getDate()!=_48.getDate()){
if(_45.getMonth()==_48.getMonth()){
if(_45.getDate()<_48.getDate()){
return 0;
}else{
if(_45.getDate()>_48.getDate()&&_47<24){
return max;
}
}
}else{
if(_45.getFullYear()==_48.getFullYear()){
if(_45.getMonth()<_48.getMonth()){
return 0;
}else{
if(_45.getMonth()>_48.getMonth()){
return max;
}
}
}else{
if(_45.getFullYear()<_48.getFullYear()){
return 0;
}else{
if(_45.getFullYear()>_48.getFullYear()){
return max;
}
}
}
}
}
var res;
var _49=86400;
if(this.isSameDay(_44,_45)||_47>24){
var d=_2.clone(_44);
var _4a=0;
if(_46!=null&&_46!=0){
d.setHours(_46);
_4a=gt(d);
}
d=_2.clone(_44);
d.setHours(_47);
var _4b;
if(_47==null||_47==24){
_4b=_49;
}else{
if(_47>24){
_4b=_49+gt(d);
}else{
_4b=gt(d);
}
}
var _4c=0;
if(_47>24&&_44.getDate()!=_45.getDate()){
_4c=_49+gt(_45);
}else{
_4c=gt(_45);
}
if(_4c<_4a){
return 0;
}
if(_4c>_4b){
return max;
}
_4c-=_4a;
res=(max*_4c)/(_4b-_4a);
}else{
if(_45.getDate()<_44.getDate()&&_45.getMonth()==_44.getMonth()){
return 0;
}
var d2=this.floorToDay(_45);
var dp1=_43.dateModule.add(_44,"day",1);
dp1=this.floorToDay(dp1,false,_43);
if(cal.compare(d2,_44)==1&&cal.compare(d2,dp1)==0||cal.compare(d2,dp1)==1){
res=max;
}else{
res=0;
}
}
return res;
},getTime:function(e,x,y,_4d){
return null;
},getSubColumn:function(e,x,y,_4e){
return null;
},getSubColumnIndex:function(_4f){
if(this.subColumns){
for(var i=0;i<this.subColumns.length;i++){
if(this.subColumns[i]==_4f){
return i;
}
}
}
return -1;
},newDate:function(obj){
return _14.newDate(obj,this.dateClassObj);
},_isItemInView:function(_50){
var rd=this.renderData;
var cal=rd.dateModule;
if(cal.compare(_50.startTime,rd.startTime)==-1){
return false;
}
return cal.compare(_50.endTime,rd.endTime)!=1;
},_ensureItemInView:function(_51){
var rd=this.renderData;
var cal=rd.dateModule;
var _52=Math.abs(cal.difference(_51.startTime,_51.endTime,"millisecond"));
var _53=false;
if(cal.compare(_51.startTime,rd.startTime)==-1){
_51.startTime=rd.startTime;
_51.endTime=cal.add(_51.startTime,"millisecond",_52);
_53=true;
}else{
if(cal.compare(_51.endTime,rd.endTime)==1){
_51.endTime=rd.endTime;
_51.startTime=cal.add(_51.endTime,"millisecond",-_52);
_53=true;
}
}
return _53;
},scrollable:true,autoScroll:true,_autoScroll:function(gx,gy,_54){
return false;
},scrollMethod:"auto",_setScrollMethodAttr:function(_55){
if(this.scrollMethod!=_55){
this.scrollMethod=_55;
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
},_startAutoScroll:function(_56){
var sp=this._scrollProps;
if(!sp){
sp=this._scrollProps={};
}
sp.scrollStep=_56;
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
},_scrollPos:0,_hscrollPos:0,getCSSPrefix:function(){
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
return "";
},_hScrollNodes:null,_setScrollPositionBase:function(pos,_57){
if(_57&&this._scrollPos==pos||!_57&&this._hScrollPos==pos){
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
var max=0;
if(_57){
var _58=_d.getMarginBox(this.scrollContainer);
var _59=_d.getMarginBox(this.sheetContainer);
max=_59.h-_58.h;
}else{
var _5a=_d.getMarginBox(this.grid);
var _5b=_d.getMarginBox(this.gridTable);
max=_5b.w-_5a.w;
}
if(pos<0){
pos=0;
}else{
if(pos>max){
pos=max;
}
}
if(_57){
this._scrollPos=pos;
}else{
this._hScrollPos=pos;
}
var rtl=!this.isLeftToRight();
if(this._domScroll){
if(_57){
this.scrollContainer.scrollTop=pos;
}else{
_3.forEach(this._hScrollNodes,function(elt){
_a.set(elt,"left",((rtl?1:-1)*pos)+"px");
},this);
}
}else{
if(!this._cssPrefix){
this._cssPrefix=this.getCSSPrefix();
}
var _5c=this._cssPrefix+"transform";
if(_57){
_a.set(this.sheetContainer,_5c,"translateY(-"+pos+"px)");
}else{
var css="translateX("+(rtl?"":"-")+pos+"px)";
_3.forEach(this._hScrollNodes,function(elt){
_a.set(elt,_5c,css);
},this);
}
}
},_setScrollPosition:function(pos){
this._setScrollPositionBase(pos,true);
},_getScrollPosition:function(){
return this._scrollPos;
},_setHScrollPosition:function(pos){
this._setScrollPositionBase(pos,false);
},_setHScrollPositionImpl:function(pos,_5d,_5e){
var css=_5d?null:"translateX(-"+pos+"px)";
_3.forEach(this._hScrollNodes,function(elt){
if(_5d){
elt.scrollLeft=pos;
_a.set(elt,"left",(-pos)+"px");
}else{
_a.set(elt,cssProp,css);
}
},this);
},_hScrollPos:0,_getHScrollPosition:function(){
return this._hScrollPos;
},scrollView:function(dir){
},ensureVisibility:function(_5f,end,_60,_61,_62){
},_getStoreAttr:function(){
if(this.owner){
return this.owner.get("store");
}
return this.store;
},_setItemsAttr:function(_63){
this._set("items",_63);
this.displayedItemsInvalidated=true;
},_refreshItemsRendering:function(){
var rd=this.renderData;
this._computeVisibleItems(rd);
this._layoutRenderers(rd);
},_refreshDecorationItemsRendering:function(){
var rd=this.renderData;
this._computeVisibleItems(rd);
this._layoutDecorationRenderers(rd);
},invalidateLayout:function(){
this._layoutRenderers(this.renderData);
this._layoutDecorationRenderers(this.renderData);
},_setDecorationItemsAttr:function(_64){
this._set("decorationItems",_64);
this.displayedDecorationItemsInvalidated=true;
},_getDecorationStoreAttr:function(){
if(this.owner){
return this.owner.get("decorationStore");
}
return this.decorationStore;
},_setDecorationStoreAttr:function(_65){
this.decorationStore=_65;
this.decorationStoreManager.set("store",_65);
},computeOverlapping:function(_66,_67){
if(_66.length==0){
return {numLanes:0,addedPassRes:[1]};
}
var _68=[];
for(var i=0;i<_66.length;i++){
var _69=_66[i];
this._layoutPass1(_69,_68);
}
var _6a=null;
if(_67){
_6a=_2.hitch(this,_67)(_68);
}
return {numLanes:_68.length,addedPassRes:_6a};
},_layoutPass1:function(_6b,_6c){
var _6d=true;
for(var i=0;i<_6c.length;i++){
var _6e=_6c[i];
_6d=false;
for(var j=0;j<_6e.length&&!_6d;j++){
if(_6e[j].start<_6b.end&&_6b.start<_6e[j].end){
_6d=true;
_6e[j].extent=1;
}
}
if(!_6d){
_6b.lane=i;
_6b.extent=-1;
_6e.push(_6b);
return;
}
}
_6c.push([_6b]);
_6b.lane=_6c.length-1;
_6b.extent=-1;
},_layoutInterval:function(_6f,_70,_71,end,_72){
},layoutPriorityFunction:null,_sortItemsFunction:function(a,b){
var res=this.dateModule.compare(a.startTime,b.startTime);
if(res==0){
res=-1*this.dateModule.compare(a.endTime,b.endTime);
}
return res;
},_layoutRenderers:function(_73){
this._layoutRenderersImpl(_73,this.rendererManager,_73.items,"dataItems");
},_layoutDecorationRenderers:function(_74){
this._layoutRenderersImpl(_74,this.decorationRendererManager,_74.decorationItems,"decorationItems");
},_layoutRenderersImpl:function(_75,_76,_77,_78){
if(!_77){
return;
}
_76.recycleItemRenderers();
var cal=_75.dateModule;
var _79=this.newDate(_75.startTime);
var _7a=_2.clone(_79);
var _7b;
var _77=_77.concat();
var _7c=[],_7d;
var _7e={};
var _7f=0;
while(cal.compare(_79,_75.endTime)==-1&&_77.length>0){
_7b=this.addAndFloor(_79,this._layoutUnit,this._layoutStep);
var _80=_2.clone(_7b);
if(_75.minHours){
_7a.setHours(_75.minHours);
}
if(_75.maxHours!=undefined&&_75.maxHours!=24){
if(_75.maxHours<24){
_80=cal.add(_7b,"day",-1);
}
_80=this.floorToDay(_80,true,_75);
_80.setHours(_75.maxHours-(_75.maxHours<24?0:24));
}
_7d=_3.filter(_77,function(_81){
var r=this.isOverlapping(_75,_81.startTime,_81.endTime,_7a,_80);
if(r){
_7e[_81.id]=true;
_7c.push(_81);
}else{
if(_7e[_81.id]){
delete _7e[_81.id];
}else{
_7c.push(_81);
}
}
return r;
},this);
_77=_7c;
_7c=[];
if(_7d.length>0){
_7d.sort(_2.hitch(this,this.layoutPriorityFunction?this.layoutPriorityFunction:this._sortItemsFunction));
this._layoutInterval(_75,_7f,_7a,_80,_7d,_78);
}
_79=_7b;
_7a=_2.clone(_79);
_7f++;
}
this._onRenderersLayoutDone(this);
},_recycleItemRenderers:function(_82){
this.rendererManager.recycleItemRenderers(_82);
},getRenderers:function(_83){
return this.rendererManager.getRenderers(_83);
},itemToRendererKindFunc:null,_itemToRendererKind:function(_84){
if(this.itemToRendererKindFunc){
return this.itemToRendererKindFunc(_84);
}
return this._defaultItemToRendererKindFunc(_84);
},_defaultItemToRendererKindFunc:function(_85){
return null;
},_createRenderer:function(_86,_87,_88,_89){
return this.rendererManager.createRenderer(_86,_87,_88,_89);
},_onRendererCreated:function(e){
if(e.source==this){
this.onRendererCreated(e);
}
if(this.owner!=null){
this.owner._onRendererCreated(e);
}
},onRendererCreated:function(e){
},_onRendererRecycled:function(e){
if(e.source==this){
this.onRendererRecycled(e);
}
if(this.owner!=null){
this.owner._onRendererRecycled(e);
}
},onRendererRecycled:function(e){
},_onRendererReused:function(e){
if(e.source==this){
this.onRendererReused(e);
}
if(this.owner!=null){
this.owner._onRendererReused(e);
}
},onRendererReused:function(e){
},_onRendererDestroyed:function(e){
if(e.source==this){
this.onRendererDestroyed(e);
}
if(this.owner!=null){
this.owner._onRendererDestroyed(e);
}
},onRendererDestroyed:function(e){
},_onRenderersLayoutDone:function(_8a){
this.onRenderersLayoutDone(_8a);
if(this.owner!=null){
this.owner._onRenderersLayoutDone(_8a);
}
},onRenderersLayoutDone:function(_8b){
},_recycleRenderer:function(_8c,_8d){
this.rendererManager.recycleRenderer(_8c,_8d);
},_destroyRenderer:function(_8e){
this.rendererManager.destroyRenderer(_8e);
},_destroyRenderersByKind:function(_8f){
this.rendererManager.destroyRenderersByKind(_8f);
},_updateEditingCapabilities:function(_90,_91){
var _92=this.isItemMoveEnabled(_90,_91.rendererKind);
var _93=this.isItemResizeEnabled(_90,_91.rendererKind);
var _94=false;
if(_92!=_91.get("moveEnabled")){
_91.set("moveEnabled",_92);
_94=true;
}
if(_93!=_91.get("resizeEnabled")){
_91.set("resizeEnabled",_93);
_94=true;
}
if(_94){
_91.updateRendering();
}
},updateRenderers:function(obj,_95){
if(obj==null){
return;
}
var _96=_2.isArray(obj)?obj:[obj];
for(var i=0;i<_96.length;i++){
var _97=_96[i];
if(_97==null||_97.id==null){
continue;
}
var _98=this.rendererManager.itemToRenderer[_97.id];
if(_98==null){
continue;
}
var _99=this.isItemSelected(_97);
var _9a=this.isItemHovered(_97);
var _9b=this.isItemBeingEdited(_97);
var _9c=this.showFocus?this.isItemFocused(_97):false;
for(var j=0;j<_98.length;j++){
var _9d=_98[j].renderer;
_9d.set("hovered",_9a);
_9d.set("selected",_99);
_9d.set("edited",_9b);
_9d.set("focused",_9c);
_9d.set("storeState",this.getItemStoreState(_97));
this.applyRendererZIndex(_97,_98[j],_9a,_99,_9b,_9c);
if(!_95){
_9d.set("item",_97);
if(_9d.updateRendering){
_9d.updateRendering();
}
}
}
}
},applyRendererZIndex:function(_9e,_9f,_a0,_a1,_a2,_a3){
_a.set(_9f.container,{"zIndex":_a2||_a1?20:_9e.lane==undefined?0:_9e.lane});
},getIdentity:function(_a4){
return this.owner?this.owner.getIdentity(_a4):_a4.id;
},_setHoveredItem:function(_a5,_a6){
if(this.owner){
this.owner._setHoveredItem(_a5,_a6);
return;
}
if(this.hoveredItem&&_a5&&this.hoveredItem.id!=_a5.id||_a5==null||this.hoveredItem==null){
var old=this.hoveredItem;
this.hoveredItem=_a5;
this.updateRenderers([old,this.hoveredItem],true);
if(_a5&&_a6){
this._updateEditingCapabilities(_a5._item?_a5._item:_a5,_a6);
}
}
},hoveredItem:null,isItemHovered:function(_a7){
if(this._isEditing&&this._edProps){
return _a7.id==this._edProps.editedItem.id;
}
return this.owner?this.owner.isItemHovered(_a7):this.hoveredItem!=null&&this.hoveredItem.id==_a7.id;
},isItemFocused:function(_a8){
return this._isItemFocused?this._isItemFocused(_a8):false;
},_setSelectionModeAttr:function(_a9){
if(this.owner){
this.owner.set("selectionMode",_a9);
}else{
this.inherited(arguments);
}
},_getSelectionModeAttr:function(_aa){
if(this.owner){
return this.owner.get("selectionMode");
}
return this.inherited(arguments);
},_setSelectedItemAttr:function(_ab){
if(this.owner){
this.owner.set("selectedItem",_ab);
}else{
this.inherited(arguments);
}
},_getSelectedItemAttr:function(_ac){
if(this.owner){
return this.owner.get("selectedItem");
}
return this.selectedItem;
},_setSelectedItemsAttr:function(_ad){
if(this.owner){
this.owner.set("selectedItems",_ad);
}else{
this.inherited(arguments);
}
},_getSelectedItemsAttr:function(){
if(this.owner){
return this.owner.get("selectedItems");
}
return this.inherited(arguments);
},isItemSelected:function(_ae){
if(this.owner){
return this.owner.isItemSelected(_ae);
}
return this.inherited(arguments);
},selectFromEvent:function(e,_af,_b0,_b1){
if(this.owner){
this.owner.selectFromEvent(e,_af,_b0,_b1);
}else{
this.inherited(arguments);
}
},setItemSelected:function(_b2,_b3){
if(this.owner){
this.owner.setItemSelected(_b2,_b3);
}else{
this.inherited(arguments);
}
},createItemFunc:null,_getCreateItemFuncAttr:function(){
if(this.owner){
return this.owner.get("createItemFunc");
}
return this.createItemFunc;
},createOnGridClick:false,_getCreateOnGridClickAttr:function(){
if(this.owner){
return this.owner.get("createOnGridClick");
}
return this.createOnGridClick;
},_gridMouseDown:false,_tempIdCount:0,_tempItemsMap:null,_onGridMouseDown:function(e){
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
var _b4=this._createdEvent=f(this,this.getTime(e),e,this.getSubColumn(e));
var _b5=this.get("store");
if(!_b4||_b5==null){
return;
}
if(_b5.getIdentity(_b4)==undefined){
var id="_tempId_"+(this._tempIdCount++);
_b4[_b5.idProperty]=id;
if(this._tempItemsMap==null){
this._tempItemsMap={};
}
this._tempItemsMap[id]=true;
}
var _b6=this.itemToRenderItem(_b4,_b5);
_b6._item=_b4;
this._setItemStoreState(_b4,"unstored");
var _b7=this._getTopOwner();
var _b8=_b7.get("items");
_b7.set("items",_b8?_b8.concat([_b6]):[_b6]);
this._refreshItemsRendering();
var _b9=this.getRenderers(_b4);
if(_b9&&_b9.length>0){
var _ba=_b9[0];
if(_ba){
this._onRendererHandleMouseDown(e,_ba.renderer,"resizeEnd");
this._startItemEditing(_b6,"mouse");
}
}
}
},_onGridMouseMove:function(e){
},_onGridMouseUp:function(e){
},_onGridTouchStart:function(e){
var p=this._edProps;
this._gridProps={event:e,fromItem:this.isAscendantHasClass(e.target,this.eventContainer,"dojoxCalendarEvent")};
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
},_doEndItemEditing:function(obj,_bb){
if(obj&&obj._isEditing){
var p=obj._edProps;
if(p&&p.endEditingTimer){
clearTimeout(p.endEditingTimer);
p.endEditingTimer=null;
}
obj._endItemEditing(_bb,false);
}
},_onGridTouchEnd:function(e){
},_onGridTouchMove:function(e){
},__fixEvt:function(e){
return e;
},_dispatchCalendarEvt:function(e,_bc){
e=this.__fixEvt(e);
this[_bc](e);
if(this.owner){
this.owner[_bc](e);
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
},_getStartEndRenderers:function(_bd){
var _be=this.rendererManager.itemToRenderer[_bd.id];
if(_be==null){
return null;
}
if(_be.length==1){
var _bf=_be[0].renderer;
return [_bf,_bf];
}
var rd=this.renderData;
var _c0=false;
var _c1=false;
var res=[];
for(var i=0;i<_be.length;i++){
var ir=_be[i].renderer;
if(!_c0){
_c0=rd.dateModule.compare(ir.item.range[0],ir.item.startTime)==0;
res[0]=ir;
}
if(!_c1){
_c1=rd.dateModule.compare(ir.item.range[1],ir.item.endTime)==0;
res[1]=ir;
}
if(_c0&&_c1){
break;
}
}
return res;
},editable:true,moveEnabled:true,resizeEnabled:true,isItemEditable:function(_c2,_c3){
return this.getItemStoreState(_c2)!="storing"&&this.editable&&(this.owner?this.owner.isItemEditable(_c2,_c3):true);
},isItemMoveEnabled:function(_c4,_c5){
return this.isItemEditable(_c4,_c5)&&this.moveEnabled&&(this.owner?this.owner.isItemMoveEnabled(_c4,_c5):true);
},isItemResizeEnabled:function(_c6,_c7){
return this.isItemEditable(_c6,_c7)&&this.resizeEnabled&&(this.owner?this.owner.isItemResizeEnabled(_c6,_c7):true);
},_isEditing:false,isItemBeingEdited:function(_c8){
return this._isEditing&&this._edProps&&this._edProps.editedItem&&this._edProps.editedItem.id==_c8.id;
},_setEditingProperties:function(_c9){
this._edProps=_c9;
},_startItemEditing:function(_ca,_cb){
this._isEditing=true;
this._getTopOwner()._isEditing=true;
var p=this._edProps;
p.editedItem=_ca;
p.storeItem=_ca._item;
p.eventSource=_cb;
p.secItem=this._secondarySheet?this._findRenderItem(_ca.id,this._secondarySheet.renderData.items):null;
p.ownerItem=this.owner?this._findRenderItem(_ca.id,this.items):null;
if(!p.liveLayout){
p.editSaveStartTime=_ca.startTime;
p.editSaveEndTime=_ca.endTime;
p.editItemToRenderer=this.rendererManager.itemToRenderer;
p.editItems=this.renderData.items;
p.editRendererList=this.rendererManager.rendererList;
this.renderData.items=[p.editedItem];
var id=p.editedItem.id;
this.rendererManager.itemToRenderer={};
this.rendererManager.rendererList=[];
var _cc=p.editItemToRenderer[id];
p.editRendererIndices=[];
_3.forEach(_cc,_2.hitch(this,function(ir,i){
if(this.rendererManager.itemToRenderer[id]==null){
this.rendererManager.itemToRenderer[id]=[ir];
}else{
this.rendererManager.itemToRenderer[id].push(ir);
}
this.rendererManager.rendererList.push(ir);
}));
p.editRendererList=_3.filter(p.editRendererList,function(ir){
return ir!=null&&ir.renderer.item.id!=id;
});
delete p.editItemToRenderer[id];
}
this._layoutRenderers(this.renderData);
this._onItemEditBegin({item:_ca,storeItem:p.storeItem,eventSource:_cb});
},_onItemEditBegin:function(e){
this._editStartTimeSave=this.newDate(e.item.startTime);
this._editEndTimeSave=this.newDate(e.item.endTime);
this._dispatchCalendarEvt(e,"onItemEditBegin");
},onItemEditBegin:function(e){
},_endItemEditing:function(_cd,_ce){
if(this._editingGesture){
this._endItemEditingGesture(_cd);
}
this._isEditing=false;
this._getTopOwner()._isEditing=false;
var p=this._edProps;
_3.forEach(p.handles,function(_cf){
_cf.remove();
});
if(!p.liveLayout){
this.renderData.items=p.editItems;
this.rendererManager.rendererList=p.editRendererList.concat(this.rendererManager.rendererList);
_2.mixin(this.rendererManager.itemToRenderer,p.editItemToRenderer);
}
this._onItemEditEnd(_2.mixin(this._createItemEditEvent(),{item:p.editedItem,storeItem:p.storeItem,eventSource:_cd,completed:!_ce}));
this._layoutRenderers(this.renderData);
this._edProps=null;
},_onItemEditEnd:function(e){
this._dispatchCalendarEvt(e,"onItemEditEnd");
if(!e.isDefaultPrevented()){
var _d0=this.get("store");
var _d1=this.renderItemToItem(e.item,_d0);
var s=this._getItemStoreStateObj(e.item);
if(s!=null&&s.state=="unstored"){
if(e.completed){
_d1=_2.mixin(s.item,_d1);
this._setItemStoreState(_d1,"storing");
var _d2=_d0.getIdentity(_d1);
var _d3=null;
if(this._tempItemsMap&&this._tempItemsMap[_d2]){
_d3={temporaryId:_d2};
delete this._tempItemsMap[_d2];
delete _d1[_d0.idProperty];
}
_10(_d0.add(_d1,_d3),_2.hitch(this,function(res){
var id;
if(_2.isObject(res)){
id=_d0.getIdentity(res);
}else{
id=res;
}
if(id!=_d2){
this._removeRenderItem(_d2);
}
}));
}else{
this._removeRenderItem(s.id);
}
}else{
if(e.completed){
this._setItemStoreState(_d1,"storing");
_d0.put(_d1);
}else{
e.item.startTime=this._editStartTimeSave;
e.item.endTime=this._editEndTimeSave;
}
}
}
},_removeRenderItem:function(id){
var _d4=this._getTopOwner();
var _d5=_d4.get("items");
var l=_d5.length;
var _d6=false;
for(var i=l-1;i>=0;i--){
if(_d5[i].id==id){
_d5.splice(i,1);
_d6=true;
break;
}
}
this._cleanItemStoreState(id);
if(_d6){
_d4.set("items",_d5);
this.invalidateLayout();
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
},_startItemEditingGesture:function(_d7,_d8,_d9,e){
var p=this._edProps;
if(!p||p.editedItem==null){
return;
}
this._editingGesture=true;
var _da=p.editedItem;
p.editKind=_d8;
this._onItemEditBeginGesture(this.__fixEvt(_2.mixin(this._createItemEditEvent(),{item:_da,storeItem:p.storeItem,startTime:_da.startTime,endTime:_da.endTime,editKind:_d8,rendererKind:p.rendererKind,triggerEvent:e,dates:_d7,eventSource:_d9})));
p.itemBeginDispatched=true;
},_onItemEditBeginGesture:function(e){
var p=this._edProps;
var _db=p.editedItem;
var _dc=e.dates;
p.editingTimeFrom=[];
p.editingTimeFrom[0]=_dc[0];
p.editingItemRefTime=[];
p.editingItemRefTime[0]=this.newDate(p.editKind=="resizeEnd"?_db.endTime:_db.startTime);
if(p.editKind=="resizeBoth"){
p.editingTimeFrom[1]=_dc[1];
p.editingItemRefTime[1]=this.newDate(_db.endTime);
}
var cal=this.renderData.dateModule;
p.inViewOnce=this._isItemInView(_db);
if(p.rendererKind=="label"||this.roundToDay){
p._itemEditBeginSave=this.newDate(_db.startTime);
p._itemEditEndSave=this.newDate(_db.endTime);
}
p._initDuration=cal.difference(_db.startTime,_db.endTime,_db.allDay?"day":"millisecond");
this._dispatchCalendarEvt(e,"onItemEditBeginGesture");
if(!e.isDefaultPrevented()){
if(e.eventSource=="mouse"){
var _dd=e.editKind=="move"?"move":this.resizeCursor;
p.editLayer=_c.create("div",{style:"position: absolute; left:0; right:0; bottom:0; top:0; z-index:30; tabIndex:-1; background-image:url('"+this._blankGif+"'); cursor: "+_dd,onresizestart:function(e){
return false;
},onselectstart:function(e){
return false;
}},this.domNode);
p.editLayer.focus();
}
}
},onItemEditBeginGesture:function(e){
},_waDojoxAddIssue:function(d,_de,_df){
var cal=this.renderData.dateModule;
if(this._calendar!="gregorian"&&_df<0){
var gd=d.toGregorian();
gd=_e.add(gd,_de,_df);
return new this.renderData.dateClassObj(gd);
}else{
return cal.add(d,_de,_df);
}
},_computeItemEditingTimes:function(_e0,_e1,_e2,_e3,_e4){
var cal=this.renderData.dateModule;
var p=this._edProps;
if(_e1=="move"){
var _e5=cal.difference(p.editingTimeFrom[0],_e3[0],"millisecond");
_e3[0]=this._waDojoxAddIssue(p.editingItemRefTime[0],"millisecond",_e5);
}
return _e3;
},_moveOrResizeItemGesture:function(_e6,_e7,e,_e8){
if(!this._isEditing||_e6[0]==null){
return;
}
var p=this._edProps;
var _e9=p.editedItem;
var rd=this.renderData;
var cal=rd.dateModule;
var _ea=p.editKind;
var _eb=[_e6[0]];
if(_ea=="resizeBoth"){
_eb[1]=_e6[1];
}
_eb=this._computeItemEditingTimes(_e9,p.editKind,p.rendererKind,_eb,_e7);
var _ec=_eb[0];
var _ed=false;
var _ee=_2.clone(_e9.startTime);
var _ef=_2.clone(_e9.endTime);
var _f0=_e9.subColumn;
var _f1=p.eventSource=="keyboard"?false:this.allowStartEndSwap;
if(_ea=="move"){
if(_e8!=null&&_e9.subColumn!=_e8&&this.allowSubColumnMove){
_e9.subColumn=_e8;
var _f2=this.get("store");
var _f3=this.renderItemToItem(_e9,_f2);
_2.mixin(_e9,this.itemToRenderItem(_f3,_f2));
_ed=true;
}
if(cal.compare(_e9.startTime,_ec)!=0){
var _f4=cal.difference(_e9.startTime,_e9.endTime,"millisecond");
_e9.startTime=this.newDate(_ec);
_e9.endTime=cal.add(_e9.startTime,"millisecond",_f4);
_ed=true;
}
}else{
if(_ea=="resizeStart"){
if(cal.compare(_e9.startTime,_ec)!=0){
if(cal.compare(_e9.endTime,_ec)!=-1){
_e9.startTime=this.newDate(_ec);
}else{
if(_f1){
_e9.startTime=this.newDate(_e9.endTime);
_e9.endTime=this.newDate(_ec);
p.editKind=_ea="resizeEnd";
if(_e7=="touch"){
p.resizeEndTouchIndex=p.resizeStartTouchIndex;
p.resizeStartTouchIndex=-1;
}
}else{
_e9.startTime=this.newDate(_e9.endTime);
_e9.startTime.setHours(_ec.getHours());
_e9.startTime.setMinutes(_ec.getMinutes());
_e9.startTime.setSeconds(_ec.getSeconds());
}
}
_ed=true;
}
}else{
if(_ea=="resizeEnd"){
if(cal.compare(_e9.endTime,_ec)!=0){
if(cal.compare(_e9.startTime,_ec)!=1){
_e9.endTime=this.newDate(_ec);
}else{
if(_f1){
_e9.endTime=this.newDate(_e9.startTime);
_e9.startTime=this.newDate(_ec);
p.editKind=_ea="resizeStart";
if(_e7=="touch"){
p.resizeStartTouchIndex=p.resizeEndTouchIndex;
p.resizeEndTouchIndex=-1;
}
}else{
_e9.endTime=this.newDate(_e9.startTime);
_e9.endTime.setHours(_ec.getHours());
_e9.endTime.setMinutes(_ec.getMinutes());
_e9.endTime.setSeconds(_ec.getSeconds());
}
}
_ed=true;
}
}else{
if(_ea=="resizeBoth"){
_ed=true;
var _f5=this.newDate(_ec);
var end=this.newDate(_eb[1]);
if(cal.compare(_f5,end)!=-1){
if(_f1){
var t=_f5;
_f5=end;
end=t;
}else{
_ed=false;
}
}
if(_ed){
_e9.startTime=_f5;
_e9.endTime=end;
}
}else{
return false;
}
}
}
}
if(!_ed){
return false;
}
var evt=_2.mixin(this._createItemEditEvent(),{item:_e9,storeItem:p.storeItem,startTime:_e9.startTime,endTime:_e9.endTime,editKind:_ea,rendererKind:p.rendererKind,triggerEvent:e,eventSource:_e7});
if(_ea=="move"){
this._onItemEditMoveGesture(evt);
}else{
this._onItemEditResizeGesture(evt);
}
if(cal.compare(_e9.startTime,_e9.endTime)==1){
var tmp=_e9.startTime;
_e9.startTime=_e9.endTime;
_e9.endTime=tmp;
}
_ed=_f0!=_e9.subColumn||cal.compare(_ee,_e9.startTime)!=0||cal.compare(_ef,_e9.endTime)!=0;
if(!_ed){
return false;
}
this._layoutRenderers(this.renderData);
if(p.liveLayout&&p.secItem!=null){
p.secItem.startTime=_e9.startTime;
p.secItem.endTime=_e9.endTime;
this._secondarySheet._layoutRenderers(this._secondarySheet.renderData);
}else{
if(p.ownerItem!=null&&this.owner.liveLayout){
p.ownerItem.startTime=_e9.startTime;
p.ownerItem.endTime=_e9.endTime;
this.owner._layoutRenderers(this.owner.renderData);
}
}
return true;
},_findRenderItem:function(id,_f6){
_f6=_f6||this.renderData.items;
for(var i=0;i<_f6.length;i++){
if(_f6[i].id==id){
return _f6[i];
}
}
return null;
},_onItemEditMoveGesture:function(e){
this._dispatchCalendarEvt(e,"onItemEditMoveGesture");
if(!e.isDefaultPrevented()){
var p=e.source._edProps;
var rd=this.renderData;
var cal=rd.dateModule;
var _f7,_f8;
if(p.rendererKind=="label"||(this.roundToDay&&!e.item.allDay)){
_f7=this.floorToDay(e.item.startTime,false,rd);
_f7.setHours(p._itemEditBeginSave.getHours());
_f7.setMinutes(p._itemEditBeginSave.getMinutes());
_f8=cal.add(_f7,"millisecond",p._initDuration);
}else{
if(e.item.allDay){
_f7=this.floorToDay(e.item.startTime,true);
_f8=cal.add(_f7,"day",p._initDuration);
}else{
_f7=this.floorDate(e.item.startTime,this.snapUnit,this.snapSteps);
_f8=cal.add(_f7,"millisecond",p._initDuration);
}
}
e.item.startTime=_f7;
e.item.endTime=_f8;
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
var _f9=e.item.startTime;
var _fa=e.item.endTime;
if(e.editKind=="resizeStart"){
if(e.item.allDay){
_f9=this.floorToDay(e.item.startTime,false,this.renderData);
}else{
if(this.roundToDay){
_f9=this.floorToDay(e.item.startTime,false,rd);
_f9.setHours(p._itemEditBeginSave.getHours());
_f9.setMinutes(p._itemEditBeginSave.getMinutes());
}else{
_f9=this.floorDate(e.item.startTime,this.snapUnit,this.snapSteps);
}
}
}else{
if(e.editKind=="resizeEnd"){
if(e.item.allDay){
if(!this.isStartOfDay(e.item.endTime)){
_fa=this.floorToDay(e.item.endTime,false,this.renderData);
_fa=cal.add(_fa,"day",1);
}
}else{
if(this.roundToDay){
_fa=this.floorToDay(e.item.endTime,false,rd);
_fa.setHours(p._itemEditEndSave.getHours());
_fa.setMinutes(p._itemEditEndSave.getMinutes());
}else{
_fa=this.floorDate(e.item.endTime,this.snapUnit,this.snapSteps);
if(e.eventSource=="mouse"){
_fa=cal.add(_fa,this.snapUnit,this.snapSteps);
}
}
}
}else{
_f9=this.floorDate(e.item.startTime,this.snapUnit,this.snapSteps);
_fa=this.floorDate(e.item.endTime,this.snapUnit,this.snapSteps);
_fa=cal.add(_fa,this.snapUnit,this.snapSteps);
}
}
e.item.startTime=_f9;
e.item.endTime=_fa;
var _fb=e.item.allDay||p._initDuration>=this._DAY_IN_MILLISECONDS&&!this.allowResizeLessThan24H;
this.ensureMinimalDuration(this.renderData,e.item,_fb?"day":this.minDurationUnit,_fb?1:this.minDurationSteps,e.editKind);
if(!p.inViewOnce){
p.inViewOnce=this._isItemInView(e.item);
}
if(p.inViewOnce&&this.stayInView){
this._ensureItemInView(e.item);
}
}
},onItemEditResizeGesture:function(e){
},_endItemEditingGesture:function(_fc,e){
if(!this._isEditing){
return;
}
this._editingGesture=false;
var p=this._edProps;
var _fd=p.editedItem;
p.itemBeginDispatched=false;
this._onItemEditEndGesture(_2.mixin(this._createItemEditEvent(),{item:_fd,storeItem:p.storeItem,startTime:_fd.startTime,endTime:_fd.endTime,editKind:p.editKind,rendererKind:p.rendererKind,triggerEvent:e,eventSource:_fc}));
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
},ensureMinimalDuration:function(_fe,_ff,unit,_100,_101){
var _102;
var cal=_fe.dateModule;
if(_101=="resizeStart"){
_102=cal.add(_ff.endTime,unit,-_100);
if(cal.compare(_ff.startTime,_102)==1){
_ff.startTime=_102;
}
}else{
_102=cal.add(_ff.startTime,unit,_100);
if(cal.compare(_ff.endTime,_102)==-1){
_ff.endTime=_102;
}
}
},doubleTapDelay:300,snapUnit:"minute",snapSteps:15,minDurationUnit:"hour",minDurationSteps:1,liveLayout:false,stayInView:true,allowStartEndSwap:true,allowResizeLessThan24H:false,allowSubColumnMove:true});
});
