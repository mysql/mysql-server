//>>built
define("dojox/grid/_Selector",["../main","dojo/_base/declare","dojo/_base/lang","dojo/query","dojo/dom-class","./Selection","./_View","./_Builder","./util"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_1.grid._InputSelectorHeaderBuilder=_3.extend(function(_b){
_8._HeaderBuilder.call(this,_b);
},_8._HeaderBuilder.prototype,{generateHtml:function(){
var w=this.view.contentWidth||0;
var _c=this.view.grid.selection.getSelectedCount();
var _d=(_c&&_c==this.view.grid.rowCount)?" dijitCheckBoxChecked dijitChecked":"";
return "<table style=\"width:"+w+"px;\" "+"border=\"0\" cellspacing=\"0\" cellpadding=\"0\" "+"role=\"presentation\"><tr><th style=\"text-align: center;\">"+"<div class=\"dojoxGridCheckSelector dijitReset dijitInline dijitCheckBox"+_d+"\"></div></th></tr></table>";
},doclick:function(e){
var _e=this.view.grid.selection.getSelectedCount();
this.view._selectionChanging=true;
if(_e==this.view.grid.rowCount){
this.view.grid.selection.deselectAll();
}else{
this.view.grid.selection.selectRange(0,this.view.grid.rowCount-1);
}
this.view._selectionChanging=false;
this.view.onSelectionChanged();
return true;
}});
var _f=_1.grid._SelectorContentBuilder=_3.extend(function(_10){
_8._ContentBuilder.call(this,_10);
},_8._ContentBuilder.prototype,{generateHtml:function(_11,_12){
var w=this.view.contentWidth||0;
return "<table class=\"dojoxGridRowbarTable\" style=\"width:"+w+"px;\" border=\"0\" "+"cellspacing=\"0\" cellpadding=\"0\" role=\"presentation\"><tr>"+"<td  style=\"text-align: center;\" class=\"dojoxGridRowbarInner\">"+this.getCellContent(_12)+"</td></tr></table>";
},getCellContent:function(_13){
return "&nbsp;";
},findTarget:function(){
var t=_8._ContentBuilder.prototype.findTarget.apply(this,arguments);
return t;
},domouseover:function(e){
this.view.grid.onMouseOverRow(e);
},domouseout:function(e){
if(!this.isIntraRowEvent(e)){
this.view.grid.onMouseOutRow(e);
}
},doclick:function(e){
var idx=e.rowIndex;
var _14=this.view.grid.selection.isSelected(idx);
var _15=this.view.grid.selection.mode;
if(!_14){
if(_15=="single"){
this.view.grid.selection.select(idx);
}else{
if(_15!="none"){
this.view.grid.selection.addToSelection(idx);
}
}
}else{
this.view.grid.selection.deselect(idx);
}
return true;
}});
var _16=_1.grid._InputSelectorContentBuilder=_3.extend(function(_17){
_f.call(this,_17);
},_f.prototype,{getCellContent:function(_18){
var v=this.view;
var _19=v.inputType=="checkbox"?"CheckBox":"Radio";
var _1a=!!v.grid.selection.isSelected(_18)?" dijit"+_19+"Checked dijitChecked":"";
return "<div class=\"dojoxGridCheckSelector dijitReset dijitInline dijit"+_19+_1a+"\"></div>";
}});
var _1b=_2("dojox.grid._Selector",_7,{inputType:"",selectionMode:"",defaultWidth:"2em",noscroll:true,padBorderWidth:2,_contentBuilderClass:_f,postCreate:function(){
this.inherited(arguments);
if(this.selectionMode){
this.grid.selection.mode=this.selectionMode;
}
this.connect(this.grid.selection,"onSelected","onSelected");
this.connect(this.grid.selection,"onDeselected","onDeselected");
},buildRendering:function(){
this.inherited(arguments);
this.scrollboxNode.style.overflow="hidden";
},getWidth:function(){
return this.viewWidth||this.defaultWidth;
},resize:function(){
this.adaptHeight();
},setStructure:function(s){
this.inherited(arguments);
if(s.defaultWidth){
this.defaultWidth=s.defaultWidth;
}
},adaptWidth:function(){
if(!("contentWidth" in this)&&this.contentNode){
this.contentWidth=this.contentNode.offsetWidth-this.padBorderWidth;
}
},doStyleRowNode:function(_1c,_1d){
var n=["dojoxGridRowbar dojoxGridNonNormalizedCell"];
if(this.grid.rows.isOver(_1c)){
n.push("dojoxGridRowbarOver");
}
if(this.grid.selection.isSelected(_1c)){
n.push("dojoxGridRowbarSelected");
}
_1d.className=n.join(" ");
},onSelected:function(_1e){
this.grid.updateRow(_1e);
},onDeselected:function(_1f){
this.grid.updateRow(_1f);
}});
if(!_7.prototype._headerBuilderClass&&!_7.prototype._contentBuilderClass){
_1b.prototype.postCreate=function(){
this.connect(this.scrollboxNode,"onscroll","doscroll");
_9.funnelEvents(this.contentNode,this,"doContentEvent",["mouseover","mouseout","click","dblclick","contextmenu","mousedown"]);
_9.funnelEvents(this.headerNode,this,"doHeaderEvent",["dblclick","mouseover","mouseout","mousemove","mousedown","click","contextmenu"]);
if(this._contentBuilderClass){
this.content=new this._contentBuilderClass(this);
}else{
this.content=new _8._ContentBuilder(this);
}
if(this._headerBuilderClass){
this.header=new this._headerBuilderClass(this);
}else{
this.header=new _8._HeaderBuilder(this);
}
if(!this.grid.isLeftToRight()){
this.headerNodeContainer.style.width="";
}
this.connect(this.grid.selection,"onSelected","onSelected");
this.connect(this.grid.selection,"onDeselected","onDeselected");
};
}
_2("dojox.grid._RadioSelector",_1b,{inputType:"radio",selectionMode:"single",_contentBuilderClass:_16,buildRendering:function(){
this.inherited(arguments);
this.headerNode.style.visibility="hidden";
},renderHeader:function(){
}});
_2("dojox.grid._CheckBoxSelector",_1b,{inputType:"checkbox",_headerBuilderClass:_a,_contentBuilderClass:_16,postCreate:function(){
this.inherited(arguments);
this.connect(this.grid,"onSelectionChanged","onSelectionChanged");
this.connect(this.grid,"updateRowCount","_updateVisibility");
},renderHeader:function(){
this.inherited(arguments);
this._updateVisibility(this.grid.rowCount);
},_updateVisibility:function(_20){
this.headerNode.style.visibility=_20?"":"hidden";
},onSelectionChanged:function(){
if(this._selectionChanging){
return;
}
var _21=_4(".dojoxGridCheckSelector",this.headerNode)[0];
var g=this.grid;
var s=(g.rowCount&&g.rowCount==g.selection.getSelectedCount());
g.allItemsSelected=s||false;
_5.toggle(_21,"dijitChecked",g.allItemsSelected);
_5.toggle(_21,"dijitCheckBoxChecked",g.allItemsSelected);
}});
return _1b;
});
