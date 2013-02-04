//>>built
define("dojox/grid/enhanced/_FocusManager",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojo/_base/event","dojo/_base/sniff","dojo/_base/html","dojo/keys","dijit/a11y","dijit/focus","../_FocusManager"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_3("dojox.grid.enhanced._FocusArea",null,{constructor:function(_e,_f){
this._fm=_f;
this._evtStack=[_e.name];
var _10=function(){
return true;
};
_e.onFocus=_e.onFocus||_10;
_e.onBlur=_e.onBlur||_10;
_e.onMove=_e.onMove||_10;
_e.onKeyUp=_e.onKeyUp||_10;
_e.onKeyDown=_e.onKeyDown||_10;
_2.mixin(this,_e);
},move:function(_11,_12,evt){
if(this.name){
var i,len=this._evtStack.length;
for(i=len-1;i>=0;--i){
if(this._fm._areas[this._evtStack[i]].onMove(_11,_12,evt)===false){
return false;
}
}
}
return true;
},_onKeyEvent:function(evt,_13){
if(this.name){
var i,len=this._evtStack.length;
for(i=len-1;i>=0;--i){
if(this._fm._areas[this._evtStack[i]][_13](evt,false)===false){
return false;
}
}
for(i=0;i<len;++i){
if(this._fm._areas[this._evtStack[i]][_13](evt,true)===false){
return false;
}
}
}
return true;
},keydown:function(evt){
return this._onKeyEvent(evt,"onKeyDown");
},keyup:function(evt){
return this._onKeyEvent(evt,"onKeyUp");
},contentMouseEventPlanner:function(){
return 0;
},headerMouseEventPlanner:function(){
return 0;
}});
return _3("dojox.grid.enhanced._FocusManager",_c,{_stopEvent:function(evt){
try{
if(evt&&evt.preventDefault){
_6.stop(evt);
}
}
catch(e){
}
},constructor:function(_14){
this.grid=_14;
this._areas={};
this._areaQueue=[];
this._contentMouseEventHandlers=[];
this._headerMouseEventHandlers=[];
this._currentAreaIdx=-1;
this._gridBlured=true;
this._connects.push(_5.connect(_14,"onBlur",this,"_doBlur"));
this._connects.push(_5.connect(_14.scroller,"renderPage",this,"_delayedCellFocus"));
this.addArea({name:"header",onFocus:_2.hitch(this,this.focusHeader),onBlur:_2.hitch(this,this._blurHeader),onMove:_2.hitch(this,this._navHeader),getRegions:_2.hitch(this,this._findHeaderCells),onRegionFocus:_2.hitch(this,this.doColHeaderFocus),onRegionBlur:_2.hitch(this,this.doColHeaderBlur),onKeyDown:_2.hitch(this,this._onHeaderKeyDown)});
this.addArea({name:"content",onFocus:_2.hitch(this,this._focusContent),onBlur:_2.hitch(this,this._blurContent),onMove:_2.hitch(this,this._navContent),onKeyDown:_2.hitch(this,this._onContentKeyDown)});
this.addArea({name:"editableCell",onFocus:_2.hitch(this,this._focusEditableCell),onBlur:_2.hitch(this,this._blurEditableCell),onKeyDown:_2.hitch(this,this._onEditableCellKeyDown),onContentMouseEvent:_2.hitch(this,this._onEditableCellMouseEvent),contentMouseEventPlanner:function(evt,_15){
return -1;
}});
this.placeArea("header");
this.placeArea("content");
this.placeArea("editableCell");
this.placeArea("editableCell","above","content");
},destroy:function(){
for(var _16 in this._areas){
var _17=this._areas[_16];
_4.forEach(_17._connects,_5.disconnect);
_17._connects=null;
if(_17.uninitialize){
_17.uninitialize();
}
}
this.inherited(arguments);
},addArea:function(_18){
if(_18.name&&_2.isString(_18.name)){
if(this._areas[_18.name]){
_4.forEach(_18._connects,_5.disconnect);
}
this._areas[_18.name]=new _d(_18,this);
if(_18.onHeaderMouseEvent){
this._headerMouseEventHandlers.push(_18.name);
}
if(_18.onContentMouseEvent){
this._contentMouseEventHandlers.push(_18.name);
}
}
},getArea:function(_19){
return this._areas[_19];
},_bindAreaEvents:function(){
var _1a,hdl,_1b=this._areas;
_4.forEach(this._areaQueue,function(_1c){
_1a=_1b[_1c];
if(!_1a._initialized&&_2.isFunction(_1a.initialize)){
_1a.initialize();
_1a._initialized=true;
}
if(_1a.getRegions){
_1a._regions=_1a.getRegions()||[];
_4.forEach(_1a._connects||[],_5.disconnect);
_1a._connects=[];
_4.forEach(_1a._regions,function(r){
if(_1a.onRegionFocus){
hdl=_5.connect(r,"onfocus",_1a.onRegionFocus);
_1a._connects.push(hdl);
}
if(_1a.onRegionBlur){
hdl=_5.connect(r,"onblur",_1a.onRegionBlur);
_1a._connects.push(hdl);
}
});
}
});
},removeArea:function(_1d){
var _1e=this._areas[_1d];
if(_1e){
this.ignoreArea(_1d);
var i=_4.indexOf(this._contentMouseEventHandlers,_1d);
if(i>=0){
this._contentMouseEventHandlers.splice(i,1);
}
i=_4.indexOf(this._headerMouseEventHandlers,_1d);
if(i>=0){
this._headerMouseEventHandlers.splice(i,1);
}
_4.forEach(_1e._connects,_5.disconnect);
if(_1e.uninitialize){
_1e.uninitialize();
}
delete this._areas[_1d];
}
},currentArea:function(_1f,_20){
var idx,cai=this._currentAreaIdx;
if(_2.isString(_1f)&&(idx=_4.indexOf(this._areaQueue,_1f))>=0){
if(cai!=idx){
this.tabbingOut=false;
if(_20&&cai>=0&&cai<this._areaQueue.length){
this._areas[this._areaQueue[cai]].onBlur();
}
this._currentAreaIdx=idx;
}
}else{
return (cai<0||cai>=this._areaQueue.length)?new _d({},this):this._areas[this._areaQueue[this._currentAreaIdx]];
}
return null;
},placeArea:function(_21,pos,_22){
if(!this._areas[_21]){
return;
}
var idx=_4.indexOf(this._areaQueue,_22);
switch(pos){
case "after":
if(idx>=0){
++idx;
}
case "before":
if(idx>=0){
this._areaQueue.splice(idx,0,_21);
break;
}
default:
this._areaQueue.push(_21);
break;
case "above":
var _23=true;
case "below":
var _24=this._areas[_22];
if(_24){
if(_23){
_24._evtStack.push(_21);
}else{
_24._evtStack.splice(0,0,_21);
}
}
}
},ignoreArea:function(_25){
this._areaQueue=_4.filter(this._areaQueue,function(_26){
return _26!=_25;
});
},focusArea:function(_27,evt){
var idx;
if(typeof _27=="number"){
idx=_27<0?this._areaQueue.length+_27:_27;
}else{
idx=_4.indexOf(this._areaQueue,_2.isString(_27)?_27:(_27&&_27.name));
}
if(idx<0){
idx=0;
}
var _28=idx-this._currentAreaIdx;
this._gridBlured=false;
if(_28){
this.tab(_28,evt);
}else{
this.currentArea().onFocus(evt,_28);
}
},tab:function(_29,evt){
this._gridBlured=false;
this.tabbingOut=false;
if(_29===0){
return;
}
var cai=this._currentAreaIdx;
var dir=_29>0?1:-1;
if(cai<0||cai>=this._areaQueue.length){
cai=(this._currentAreaIdx+=_29);
}else{
var _2a=this._areas[this._areaQueue[cai]].onBlur(evt,_29);
if(_2a===true){
cai=(this._currentAreaIdx+=_29);
}else{
if(_2.isString(_2a)&&this._areas[_2a]){
cai=this._currentAreaIdx=_4.indexOf(this._areaQueue,_2a);
}
}
}
for(;cai>=0&&cai<this._areaQueue.length;cai+=dir){
this._currentAreaIdx=cai;
if(this._areaQueue[cai]&&this._areas[this._areaQueue[cai]].onFocus(evt,_29)){
return;
}
}
this.tabbingOut=true;
if(_29<0){
this._currentAreaIdx=-1;
_b.focus(this.grid.domNode);
}else{
this._currentAreaIdx=this._areaQueue.length;
_b.focus(this.grid.lastFocusNode);
}
},_onMouseEvent:function(_2b,evt){
var _2c=_2b.toLowerCase(),_2d=this["_"+_2c+"MouseEventHandlers"],res=_4.map(_2d,function(_2e){
return {"area":_2e,"idx":this._areas[_2e][_2c+"MouseEventPlanner"](evt,_2d)};
},this).sort(function(a,b){
return b.idx-a.idx;
}),_2f=_4.map(res,function(_30){
return res.area;
}),i=res.length;
while(--i>=0){
if(this._areas[res[i].area]["on"+_2b+"MouseEvent"](evt,_2f)===false){
return;
}
}
},contentMouseEvent:function(evt){
this._onMouseEvent("Content",evt);
},headerMouseEvent:function(evt){
this._onMouseEvent("Header",evt);
},initFocusView:function(){
this.focusView=this.grid.views.getFirstScrollingView()||this.focusView||this.grid.views.views[0];
this._bindAreaEvents();
},isNavHeader:function(){
return this._areaQueue[this._currentAreaIdx]=="header";
},previousKey:function(e){
this.tab(-1,e);
},nextKey:function(e){
this.tab(1,e);
},setFocusCell:function(_31,_32){
if(_31){
this.currentArea(this.grid.edit.isEditing()?"editableCell":"content",true);
this._focusifyCellNode(false);
this.cell=_31;
this.rowIndex=_32;
this._focusifyCellNode(true);
}
this.grid.onCellFocus(this.cell,this.rowIndex);
},doFocus:function(e){
if(e&&e.target==e.currentTarget&&!this.tabbingOut){
if(this._gridBlured){
this._gridBlured=false;
if(this._currentAreaIdx<0||this._currentAreaIdx>=this._areaQueue.length){
this.focusArea(0,e);
}else{
this.focusArea(this._currentAreaIdx,e);
}
}
}else{
this.tabbingOut=false;
}
_6.stop(e);
},_doBlur:function(){
this._gridBlured=true;
},doLastNodeFocus:function(e){
if(this.tabbingOut){
this.tabbingOut=false;
}else{
this.focusArea(-1,e);
}
},_delayedHeaderFocus:function(){
if(this.isNavHeader()){
this.focusHeader();
}
},_delayedCellFocus:function(){
this.currentArea("header",true);
this.focusArea(this._currentAreaIdx);
},_changeMenuBindNode:function(_33,_34){
var hm=this.grid.headerMenu;
if(hm&&this._contextMenuBindNode==_33){
hm.unBindDomNode(_33);
hm.bindDomNode(_34);
this._contextMenuBindNode=_34;
}
},focusHeader:function(evt,_35){
var _36=false;
this.inherited(arguments);
if(this._colHeadNode&&_8.style(this._colHeadNode,"display")!="none"){
_b.focus(this._colHeadNode);
this._stopEvent(evt);
_36=true;
}
return _36;
},_blurHeader:function(evt,_37){
if(this._colHeadNode){
_8.removeClass(this._colHeadNode,this.focusClass);
}
_8.removeAttr(this.grid.domNode,"aria-activedescendant");
this._changeMenuBindNode(this.grid.domNode,this.grid.viewsHeaderNode);
this._colHeadNode=this._colHeadFocusIdx=null;
return true;
},_navHeader:function(_38,_39,evt){
var _3a=_39<0?-1:1,_3b=_4.indexOf(this._findHeaderCells(),this._colHeadNode);
if(_3b>=0&&(evt.shiftKey&&evt.ctrlKey)){
this.colSizeAdjust(evt,_3b,_3a*5);
return;
}
this.move(_38,_39);
},_onHeaderKeyDown:function(e,_3c){
if(_3c){
var dk=_9;
switch(e.keyCode){
case dk.ENTER:
case dk.SPACE:
var _3d=this.getHeaderIndex();
if(_3d>=0&&!this.grid.pluginMgr.isFixedCell(e.cell)){
this.grid.setSortIndex(_3d,null,e);
_6.stop(e);
}
break;
}
}
return true;
},_setActiveColHeader:function(){
this.inherited(arguments);
_b.focus(this._colHeadNode);
},findAndFocusGridCell:function(){
this._focusContent();
},_focusContent:function(evt,_3e){
var _3f=true;
var _40=(this.grid.rowCount===0);
if(this.isNoFocusCell()&&!_40){
for(var i=0,_41=this.grid.getCell(0);_41&&_41.hidden;_41=this.grid.getCell(++i)){
}
this.setFocusIndex(0,_41?i:0);
}else{
if(this.cell&&!_40){
if(this.focusView&&!this.focusView.rowNodes[this.rowIndex]){
this.grid.scrollToRow(this.rowIndex);
this.focusGrid();
}else{
this.setFocusIndex(this.rowIndex,this.cell.index);
}
}else{
_3f=false;
}
}
if(_3f){
this._stopEvent(evt);
}
return _3f;
},_blurContent:function(evt,_42){
this._focusifyCellNode(false);
return true;
},_navContent:function(_43,_44,evt){
if((this.rowIndex===0&&_43<0)||(this.rowIndex===this.grid.rowCount-1&&_43>0)){
return;
}
this._colHeadNode=null;
this.move(_43,_44,evt);
if(evt){
_6.stop(evt);
}
},_onContentKeyDown:function(e,_45){
if(_45){
var dk=_9,s=this.grid.scroller;
switch(e.keyCode){
case dk.ENTER:
case dk.SPACE:
var g=this.grid;
if(g.indirectSelection){
break;
}
g.selection.clickSelect(this.rowIndex,_5.isCopyKey(e),e.shiftKey);
g.onRowClick(e);
_6.stop(e);
break;
case dk.PAGE_UP:
if(this.rowIndex!==0){
if(this.rowIndex!=s.firstVisibleRow+1){
this._navContent(s.firstVisibleRow-this.rowIndex,0);
}else{
this.grid.setScrollTop(s.findScrollTop(this.rowIndex-1));
this._navContent(s.firstVisibleRow-s.lastVisibleRow+1,0);
}
_6.stop(e);
}
break;
case dk.PAGE_DOWN:
if(this.rowIndex+1!=this.grid.rowCount){
_6.stop(e);
if(this.rowIndex!=s.lastVisibleRow-1){
this._navContent(s.lastVisibleRow-this.rowIndex-1,0);
}else{
this.grid.setScrollTop(s.findScrollTop(this.rowIndex+1));
this._navContent(s.lastVisibleRow-s.firstVisibleRow-1,0);
}
_6.stop(e);
}
break;
}
}
return true;
},_blurFromEditableCell:false,_isNavigating:false,_navElems:null,_focusEditableCell:function(evt,_46){
var _47=false;
if(this._isNavigating){
_47=true;
}else{
if(this.grid.edit.isEditing()&&this.cell){
if(this._blurFromEditableCell||!this._blurEditableCell(evt,_46)){
this.setFocusIndex(this.rowIndex,this.cell.index);
_47=true;
}
this._stopEvent(evt);
}
}
return _47;
},_applyEditableCell:function(){
try{
this.grid.edit.apply();
}
catch(e){
console.warn("_FocusManager._applyEditableCell() error:",e);
}
},_blurEditableCell:function(evt,_48){
this._blurFromEditableCell=false;
if(this._isNavigating){
var _49=true;
if(evt){
var _4a=this._navElems;
var _4b=_4a.lowest||_4a.first;
var _4c=_4a.last||_4a.highest||_4b;
var _4d=_7("ie")?evt.srcElement:evt.target;
_49=_4d==(_48>0?_4c:_4b);
}
if(_49){
this._isNavigating=false;
_8.setSelectable(this.cell.getNode(this.rowIndex),false);
return "content";
}
return false;
}else{
if(this.grid.edit.isEditing()&&this.cell){
if(!_48||typeof _48!="number"){
return false;
}
var dir=_48>0?1:-1;
var cc=this.grid.layout.cellCount;
for(var _4e,col=this.cell.index+dir;col>=0&&col<cc;col+=dir){
_4e=this.grid.getCell(col);
if(_4e.editable){
this.cell=_4e;
this._blurFromEditableCell=true;
return false;
}
}
if((this.rowIndex>0||dir==1)&&(this.rowIndex<this.grid.rowCount||dir==-1)){
this.rowIndex+=dir;
for(col=dir>0?0:cc-1;col>=0&&col<cc;col+=dir){
_4e=this.grid.getCell(col);
if(_4e.editable){
this.cell=_4e;
break;
}
}
this._applyEditableCell();
return "content";
}
}
}
return true;
},_initNavigatableElems:function(){
this._navElems=_a._getTabNavigable(this.cell.getNode(this.rowIndex));
},_onEditableCellKeyDown:function(e,_4f){
var dk=_9,g=this.grid,_50=g.edit,_51=false,_52=true;
switch(e.keyCode){
case dk.ENTER:
if(_4f&&_50.isEditing()){
this._applyEditableCell();
_51=true;
_6.stop(e);
}
case dk.SPACE:
if(!_4f&&this._isNavigating){
_52=false;
break;
}
if(_4f){
if(!this.cell.editable&&this.cell.navigatable){
this._initNavigatableElems();
var _53=this._navElems.lowest||this._navElems.first;
if(_53){
this._isNavigating=true;
_8.setSelectable(this.cell.getNode(this.rowIndex),true);
_b.focus(_53);
_6.stop(e);
this.currentArea("editableCell",true);
break;
}
}
if(!_51&&!_50.isEditing()&&!g.pluginMgr.isFixedCell(this.cell)){
_50.setEditCell(this.cell,this.rowIndex);
}
if(_51){
this.currentArea("content",true);
}else{
if(this.cell.editable&&g.canEdit()){
this.currentArea("editableCell",true);
}
}
}
break;
case dk.PAGE_UP:
case dk.PAGE_DOWN:
if(!_4f&&_50.isEditing()){
_52=false;
}
break;
case dk.ESCAPE:
if(!_4f){
_50.cancel();
this.currentArea("content",true);
}
}
return _52;
},_onEditableCellMouseEvent:function(evt){
if(evt.type=="click"){
var _54=this.cell||evt.cell;
if(_54&&!_54.editable&&_54.navigatable){
this._initNavigatableElems();
if(this._navElems.lowest||this._navElems.first){
var _55=_7("ie")?evt.srcElement:evt.target;
if(_55!=_54.getNode(evt.rowIndex)){
this._isNavigating=true;
this.focusArea("editableCell",evt);
_8.setSelectable(_54.getNode(evt.rowIndex),true);
_b.focus(_55);
return false;
}
}
}else{
if(this.grid.singleClickEdit){
this.currentArea("editableCell");
return false;
}
}
}
return true;
}});
});
