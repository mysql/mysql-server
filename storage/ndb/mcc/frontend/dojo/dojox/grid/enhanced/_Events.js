//>>built
define("dojox/grid/enhanced/_Events",["dojo/_base/kernel","dojo/_base/declare","dojo/keys","dojo/_base/html","dojo/_base/event","dojox/grid/_Events"],function(_1,_2,_3,_4,_5,_6){
return _2("dojox.grid.enhanced._Events",null,{_events:null,headerCellActiveClass:"dojoxGridHeaderActive",cellActiveClass:"dojoxGridCellActive",rowActiveClass:"dojoxGridRowActive",constructor:function(_7){
this._events=new _6();
_7.mixin(_7,this);
},dokeyup:function(e){
this.focus.currentArea().keyup(e);
},onKeyDown:function(e){
if(e.altKey||e.metaKey){
return;
}
var _8=this.focus;
var _9=this.edit.isEditing();
switch(e.keyCode){
case _3.TAB:
if(e.ctrlKey){
return;
}
_8.tab(e.shiftKey?-1:1,e);
break;
case _3.UP_ARROW:
case _3.DOWN_ARROW:
if(_9){
return;
}
_8.currentArea().move(e.keyCode==_3.UP_ARROW?-1:1,0,e);
break;
case _3.LEFT_ARROW:
case _3.RIGHT_ARROW:
if(_9){
return;
}
var _a=(e.keyCode==_3.LEFT_ARROW)?1:-1;
if(_4._isBodyLtr()){
_a*=-1;
}
_8.currentArea().move(0,_a,e);
break;
case _3.F10:
if(this.menus&&e.shiftKey){
this.onRowContextMenu(e);
}
break;
default:
_8.currentArea().keydown(e);
break;
}
},domouseup:function(e){
if(e.cellNode){
this.onMouseUp(e);
}else{
this.onRowSelectorMouseUp(e);
}
},domousedown:function(e){
if(!e.cellNode){
this.onRowSelectorMouseDown(e);
}
},onMouseUp:function(e){
this[e.rowIndex==-1?"onHeaderCellMouseUp":"onCellMouseUp"](e);
},onCellMouseDown:function(e){
_4.addClass(e.cellNode,this.cellActiveClass);
_4.addClass(e.rowNode,this.rowActiveClass);
},onCellMouseUp:function(e){
_4.removeClass(e.cellNode,this.cellActiveClass);
_4.removeClass(e.rowNode,this.rowActiveClass);
},onCellClick:function(e){
this._events.onCellClick.call(this,e);
this.focus.contentMouseEvent(e);
},onCellDblClick:function(e){
if(this.pluginMgr.isFixedCell(e.cell)){
return;
}
if(this._click.length>1&&(!this._click[0]||!this._click[1])){
this._click[0]=this._click[1]=e;
}
this._events.onCellDblClick.call(this,e);
},onRowClick:function(e){
this.edit.rowClick(e);
if(!e.cell||!this.plugin("indirectSelection")){
this.selection.clickSelectEvent(e);
}
},onRowContextMenu:function(e){
if(!this.edit.isEditing()&&this.menus){
this.showMenu(e);
}
},onSelectedRegionContextMenu:function(e){
if(this.selectedRegionMenu){
this.selectedRegionMenu._openMyself({target:e.target,coords:e.keyCode!==_3.F10&&"pageX" in e?{x:e.pageX,y:e.pageY}:null});
_5.stop(e);
}
},onHeaderCellMouseOut:function(e){
if(e.cellNode){
_4.removeClass(e.cellNode,this.cellOverClass);
_4.removeClass(e.cellNode,this.headerCellActiveClass);
}
},onHeaderCellMouseDown:function(e){
if(e.cellNode){
_4.addClass(e.cellNode,this.headerCellActiveClass);
}
},onHeaderCellMouseUp:function(e){
if(e.cellNode){
_4.removeClass(e.cellNode,this.headerCellActiveClass);
}
},onHeaderCellClick:function(e){
this.focus.currentArea("header");
if(!e.cell.isRowSelector){
this._events.onHeaderCellClick.call(this,e);
}
this.focus.headerMouseEvent(e);
},onRowSelectorMouseDown:function(e){
this.focus.focusArea("rowHeader",e);
},onRowSelectorMouseUp:function(e){
},onMouseUpRow:function(e){
if(e.rowIndex!=-1){
this.onRowMouseUp(e);
}
},onRowMouseUp:function(e){
}});
});
