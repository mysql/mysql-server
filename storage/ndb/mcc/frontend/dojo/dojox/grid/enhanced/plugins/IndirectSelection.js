//>>built
define("dojox/grid/enhanced/plugins/IndirectSelection",["dojo/_base/declare","dojo/_base/array","dojo/_base/event","dojo/_base/lang","dojo/_base/html","dojo/_base/window","dojo/_base/connect","dojo/_base/sniff","dojo/query","dojo/keys","dojo/string","../_Plugin","../../EnhancedGrid","../../cells/dijit"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
var _e=_4.getObject("dojox.grid.cells");
var _f=_1("dojox.grid.cells.RowSelector",_e._Widget,{inputType:"",map:null,disabledMap:null,isRowSelector:true,_connects:null,_subscribes:null,checkedText:"&#10003;",unCheckedText:"O",constructor:function(){
this.map={};
this.disabledMap={};
this.disabledCount=0;
this._connects=[];
this._subscribes=[];
this.inA11YMode=_5.hasClass(_6.body(),"dijit_a11y");
this.baseClass="dojoxGridRowSelector dijitReset dijitInline dijit"+this.inputType;
this.checkedClass=" dijit"+this.inputType+"Checked";
this.disabledClass=" dijit"+this.inputType+"Disabled";
this.checkedDisabledClass=" dijit"+this.inputType+"CheckedDisabled";
this.statusTextClass=" dojoxGridRowSelectorStatusText";
this._connects.push(_7.connect(this.grid,"dokeyup",this,"_dokeyup"));
this._connects.push(_7.connect(this.grid.selection,"onSelected",this,"_onSelected"));
this._connects.push(_7.connect(this.grid.selection,"onDeselected",this,"_onDeselected"));
this._connects.push(_7.connect(this.grid.scroller,"invalidatePageNode",this,"_pageDestroyed"));
this._connects.push(_7.connect(this.grid,"onCellClick",this,"_onClick"));
this._connects.push(_7.connect(this.grid,"updateRow",this,"_onUpdateRow"));
},formatter:function(_10,_11,_12){
var _13=_12;
var _14=_13.baseClass;
var _15=!!_13.getValue(_11);
var _16=!!_13.disabledMap[_11];
if(_15){
_14+=_13.checkedClass;
if(_16){
_14+=_13.checkedDisabledClass;
}
}else{
if(_16){
_14+=_13.disabledClass;
}
}
return ["<div tabindex = -1 ","id = '"+_13.grid.id+"_rowSelector_"+_11+"' ","name = '"+_13.grid.id+"_rowSelector' class = '"+_14+"' ","role = "+_13.inputType.toLowerCase()+" aria-checked = '"+_15+"' aria-disabled = '"+_16+"' aria-label = '"+_b.substitute(_13.grid._nls["indirectSelection"+_13.inputType],[_11+1])+"'>","<span class = '"+_13.statusTextClass+"'>"+(_15?_13.checkedText:_13.unCheckedText)+"</span>","</div>"].join("");
},setValue:function(_17,_18){
},getValue:function(_19){
return this.grid.selection.isSelected(_19);
},toggleRow:function(_1a,_1b){
this._nativeSelect(_1a,_1b);
},setDisabled:function(_1c,_1d){
if(_1c<0){
return;
}
this._toggleDisabledStyle(_1c,_1d);
},disabled:function(_1e){
return !!this.disabledMap[_1e];
},_onClick:function(e){
if(e.cell===this){
this._selectRow(e);
}
},_dokeyup:function(e){
if(e.cellIndex==this.index&&e.rowIndex>=0&&e.keyCode==_a.SPACE){
this._selectRow(e);
}
},focus:function(_1f){
var _20=this.map[_1f];
if(_20){
_20.focus();
}
},_focusEndingCell:function(_21,_22){
var _23=this.grid.getCell(_22);
this.grid.focus.setFocusCell(_23,_21);
},_nativeSelect:function(_24,_25){
this.grid.selection[_25?"select":"deselect"](_24);
},_onSelected:function(_26){
this._toggleCheckedStyle(_26,true);
},_onDeselected:function(_27){
this._toggleCheckedStyle(_27,false);
},_onUpdateRow:function(_28){
delete this.map[_28];
},_toggleCheckedStyle:function(_29,_2a){
var _2b=this._getSelector(_29);
if(_2b){
_5.toggleClass(_2b,this.checkedClass,_2a);
if(this.disabledMap[_29]){
_5.toggleClass(_2b,this.checkedDisabledClass,_2a);
}
_2b.setAttribute("aria-checked",_2a);
if(this.inA11YMode){
_2b.firstChild.innerHTML=(_2a?this.checkedText:this.unCheckedText);
}
}
},_toggleDisabledStyle:function(_2c,_2d){
var _2e=this._getSelector(_2c);
if(_2e){
_5.toggleClass(_2e,this.disabledClass,_2d);
if(this.getValue(_2c)){
_5.toggleClass(_2e,this.checkedDisabledClass,_2d);
}
_2e.setAttribute("aria-disabled",_2d);
}
this.disabledMap[_2c]=_2d;
if(_2c>=0){
this.disabledCount+=_2d?1:-1;
}
},_getSelector:function(_2f){
var _30=this.map[_2f];
if(!_30){
var _31=this.view.rowNodes[_2f];
if(_31){
_30=_9(".dojoxGridRowSelector",_31)[0];
if(_30){
this.map[_2f]=_30;
}
}
}
return _30;
},_pageDestroyed:function(_32){
var _33=this.grid.scroller.rowsPerPage;
var _34=_32*_33,end=_34+_33-1;
for(var i=_34;i<=end;i++){
if(!this.map[i]){
continue;
}
_5.destroy(this.map[i]);
delete this.map[i];
}
},destroy:function(){
for(var i in this.map){
_5.destroy(this.map[i]);
delete this.map[i];
}
for(i in this.disabledMap){
delete this.disabledMap[i];
}
_2.forEach(this._connects,_7.disconnect);
_2.forEach(this._subscribes,_7.unsubscribe);
delete this._connects;
delete this._subscribes;
}});
var _35=_1("dojox.grid.cells.SingleRowSelector",_f,{inputType:"Radio",_selectRow:function(e){
var _36=e.rowIndex;
if(this.disabledMap[_36]){
return;
}
this._focusEndingCell(_36,e.cellIndex);
this._nativeSelect(_36,!this.grid.selection.selected[_36]);
}});
var _37=_1("dojox.grid.cells.MultipleRowSelector",_f,{inputType:"CheckBox",swipeStartRowIndex:-1,swipeMinRowIndex:-1,swipeMaxRowIndex:-1,toSelect:false,lastClickRowIdx:-1,unCheckedText:"&#9633;",constructor:function(){
this._connects.push(_7.connect(_6.doc,"onmouseup",this,"_domouseup"));
this._connects.push(_7.connect(this.grid,"onRowMouseOver",this,"_onRowMouseOver"));
this._connects.push(_7.connect(this.grid.focus,"move",this,"_swipeByKey"));
this._connects.push(_7.connect(this.grid,"onCellMouseDown",this,"_onMouseDown"));
if(this.headerSelector){
this._connects.push(_7.connect(this.grid.views,"render",this,"_addHeaderSelector"));
this._connects.push(_7.connect(this.grid,"_onFetchComplete",this,"_addHeaderSelector"));
this._connects.push(_7.connect(this.grid,"onSelectionChanged",this,"_onSelectionChanged"));
this._connects.push(_7.connect(this.grid,"onKeyDown",this,function(e){
if(e.rowIndex==-1&&e.cellIndex==this.index&&e.keyCode==_a.SPACE){
this._toggletHeader();
}
}));
}
},toggleAllSelection:function(_38){
var _39=this.grid,_3a=_39.selection;
if(_38){
_3a.selectRange(0,_39.rowCount-1);
}else{
_3a.deselectAll();
}
},_onMouseDown:function(e){
if(e.cell==this){
this._startSelection(e.rowIndex);
_3.stop(e);
}
},_onRowMouseOver:function(e){
this._updateSelection(e,0);
},_domouseup:function(e){
if(_8("ie")){
this.view.content.decorateEvent(e);
}
var _3b=e.cellIndex>=0&&this.inSwipeSelection()&&!this.grid.edit.isEditRow(e.rowIndex);
if(_3b){
this._focusEndingCell(e.rowIndex,e.cellIndex);
}
this._finishSelect();
},_dokeyup:function(e){
this.inherited(arguments);
if(!e.shiftKey){
this._finishSelect();
}
},_startSelection:function(_3c){
this.swipeStartRowIndex=this.swipeMinRowIndex=this.swipeMaxRowIndex=_3c;
this.toSelect=!this.getValue(_3c);
},_updateSelection:function(e,_3d){
if(!this.inSwipeSelection()){
return;
}
var _3e=_3d!==0;
var _3f=e.rowIndex,_40=_3f-this.swipeStartRowIndex+_3d;
if(_40>0&&this.swipeMaxRowIndex<_3f+_3d){
this.swipeMaxRowIndex=_3f+_3d;
}
if(_40<0&&this.swipeMinRowIndex>_3f+_3d){
this.swipeMinRowIndex=_3f+_3d;
}
var min=_40>0?this.swipeStartRowIndex:_3f+_3d;
var max=_40>0?_3f+_3d:this.swipeStartRowIndex;
for(var i=this.swipeMinRowIndex;i<=this.swipeMaxRowIndex;i++){
if(this.disabledMap[i]||i<0){
continue;
}
if(i>=min&&i<=max){
this._nativeSelect(i,this.toSelect);
}else{
if(!_3e){
this._nativeSelect(i,!this.toSelect);
}
}
}
},_swipeByKey:function(_41,_42,e){
if(!e||_41===0||!e.shiftKey||e.cellIndex!=this.index||this.grid.focus.rowIndex<0){
return;
}
var _43=e.rowIndex;
if(this.swipeStartRowIndex<0){
this.swipeStartRowIndex=_43;
if(_41>0){
this.swipeMaxRowIndex=_43+_41;
this.swipeMinRowIndex=_43;
}else{
this.swipeMinRowIndex=_43+_41;
this.swipeMaxRowIndex=_43;
}
this.toSelect=this.getValue(_43);
}
this._updateSelection(e,_41);
},_finishSelect:function(){
this.swipeStartRowIndex=-1;
this.swipeMinRowIndex=-1;
this.swipeMaxRowIndex=-1;
this.toSelect=false;
},inSwipeSelection:function(){
return this.swipeStartRowIndex>=0;
},_nativeSelect:function(_44,_45){
this.grid.selection[_45?"addToSelection":"deselect"](_44);
},_selectRow:function(e){
var _46=e.rowIndex;
if(this.disabledMap[_46]){
return;
}
_3.stop(e);
this._focusEndingCell(_46,e.cellIndex);
var _47=_46-this.lastClickRowIdx;
var _48=!this.grid.selection.selected[_46];
if(this.lastClickRowIdx>=0&&!e.ctrlKey&&!e.altKey&&e.shiftKey){
var min=_47>0?this.lastClickRowIdx:_46;
var max=_47>0?_46:this.lastClickRowIdx;
for(var i=min;i>=0&&i<=max;i++){
this._nativeSelect(i,_48);
}
}else{
this._nativeSelect(_46,_48);
}
this.lastClickRowIdx=_46;
},getValue:function(_49){
if(_49==-1){
var g=this.grid;
return g.rowCount>0&&g.rowCount<=g.selection.getSelectedCount();
}
return this.inherited(arguments);
},_addHeaderSelector:function(){
var _4a=this.view.getHeaderCellNode(this.index);
if(!_4a){
return;
}
_5.empty(_4a);
var g=this.grid;
var _4b=_4a.appendChild(_5.create("div",{"aria-label":g._nls["selectAll"],"tabindex":-1,"id":g.id+"_rowSelector_-1","class":this.baseClass,"role":"Checkbox","innerHTML":"<span class = '"+this.statusTextClass+"'></span><span style='height: 0; width: 0; overflow: hidden; display: block;'>"+g._nls["selectAll"]+"</span>"}));
this.map[-1]=_4b;
var idx=this._headerSelectorConnectIdx;
if(idx!==undefined){
_7.disconnect(this._connects[idx]);
this._connects.splice(idx,1);
}
this._headerSelectorConnectIdx=this._connects.length;
this._connects.push(_7.connect(_4b,"onclick",this,"_toggletHeader"));
this._onSelectionChanged();
},_toggletHeader:function(){
if(!!this.disabledMap[-1]){
return;
}
this.grid._selectingRange=true;
this.toggleAllSelection(!this.getValue(-1));
this._onSelectionChanged();
this.grid._selectingRange=false;
},_onSelectionChanged:function(){
var g=this.grid;
if(!this.map[-1]||g._selectingRange){
return;
}
g.allItemsSelected=this.getValue(-1);
this._toggleCheckedStyle(-1,g.allItemsSelected);
},_toggleDisabledStyle:function(_4c,_4d){
this.inherited(arguments);
if(this.headerSelector){
var _4e=(this.grid.rowCount==this.disabledCount);
if(_4e!=!!this.disabledMap[-1]){
arguments[0]=-1;
arguments[1]=_4e;
this.inherited(arguments);
}
}
}});
var _4f=_1("dojox.grid.enhanced.plugins.IndirectSelection",_c,{name:"indirectSelection",constructor:function(){
var _50=this.grid.layout;
this.connect(_50,"setStructure",_4.hitch(_50,this.addRowSelectCell,this.option));
},addRowSelectCell:function(_51){
if(!this.grid.indirectSelection||this.grid.selectionMode=="none"){
return;
}
var _52=false,_53=["get","formatter","field","fields"],_54={type:_37,name:"",width:"30px",styles:"text-align: center;"};
if(_51.headerSelector){
_51.name="";
}
if(this.grid.rowSelectCell){
this.grid.rowSelectCell.destroy();
}
_2.forEach(this.structure,function(_55){
var _56=_55.cells;
if(_56&&_56.length>0&&!_52){
var _57=_56[0];
if(_57[0]&&_57[0].isRowSelector){
_52=true;
return;
}
var _58,_59=this.grid.selectionMode=="single"?_35:_37;
_58=_4.mixin(_54,_51,{type:_59,editable:false,notselectable:true,filterable:false,navigatable:true,nosort:true});
_2.forEach(_53,function(_5a){
if(_5a in _58){
delete _58[_5a];
}
});
if(_56.length>1){
_58.rowSpan=_56.length;
}
_2.forEach(this.cells,function(_5b,i){
if(_5b.index>=0){
_5b.index+=1;
}else{
console.warn("Error:IndirectSelection.addRowSelectCell()-  cell "+i+" has no index!");
}
});
var _5c=this.addCellDef(0,0,_58);
_5c.index=0;
_57.unshift(_5c);
this.cells.unshift(_5c);
this.grid.rowSelectCell=_5c;
_52=true;
}
},this);
this.cellCount=this.cells.length;
},destroy:function(){
this.grid.rowSelectCell.destroy();
delete this.grid.rowSelectCell;
this.inherited(arguments);
}});
_d.registerPlugin(_4f,{"preInit":true});
return _4f;
});
