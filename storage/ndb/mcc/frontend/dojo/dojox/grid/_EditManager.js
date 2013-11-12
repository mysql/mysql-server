//>>built
define("dojox/grid/_EditManager",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/connect","dojo/_base/sniff","./util"],function(_1,_2,_3,_4,_5,_6){
return _3("dojox.grid._EditManager",null,{constructor:function(_7){
this.grid=_7;
if(_5("ie")){
this.connections=[_4.connect(document.body,"onfocus",_1.hitch(this,"_boomerangFocus"))];
}else{
this.connections=[_4.connect(this.grid,"onBlur",this,"apply")];
}
},info:{},destroy:function(){
_2.forEach(this.connections,_4.disconnect);
},cellFocus:function(_8,_9){
if(this.grid.singleClickEdit||this.isEditRow(_9)){
this.setEditCell(_8,_9);
}else{
this.apply();
}
if(this.isEditing()||(_8&&_8.editable&&_8.alwaysEditing)){
this._focusEditor(_8,_9);
}
},rowClick:function(e){
if(this.isEditing()&&!this.isEditRow(e.rowIndex)){
this.apply();
}
},styleRow:function(_a){
if(_a.index==this.info.rowIndex){
_a.customClasses+=" dojoxGridRowEditing";
}
},dispatchEvent:function(e){
var c=e.cell,ed=(c&&c["editable"])?c:0;
return ed&&ed.dispatchEvent(e.dispatch,e);
},isEditing:function(){
return this.info.rowIndex!==undefined;
},isEditCell:function(_b,_c){
return (this.info.rowIndex===_b)&&(this.info.cell.index==_c);
},isEditRow:function(_d){
return this.info.rowIndex===_d;
},setEditCell:function(_e,_f){
if(!this.isEditCell(_f,_e.index)&&this.grid.canEdit&&this.grid.canEdit(_e,_f)){
this.start(_e,_f,this.isEditRow(_f)||_e.editable);
}
},_focusEditor:function(_10,_11){
_6.fire(_10,"focus",[_11]);
},focusEditor:function(){
if(this.isEditing()){
this._focusEditor(this.info.cell,this.info.rowIndex);
}
},_boomerangWindow:500,_shouldCatchBoomerang:function(){
return this._catchBoomerang>new Date().getTime();
},_boomerangFocus:function(){
if(this._shouldCatchBoomerang()){
this.grid.focus.focusGrid();
this.focusEditor();
this._catchBoomerang=0;
}
},_doCatchBoomerang:function(){
if(_5("ie")){
this._catchBoomerang=new Date().getTime()+this._boomerangWindow;
}
},start:function(_12,_13,_14){
if(!this._isValidInput()){
return;
}
this.grid.beginUpdate();
this.editorApply();
if(this.isEditing()&&!this.isEditRow(_13)){
this.applyRowEdit();
this.grid.updateRow(_13);
}
if(_14){
this.info={cell:_12,rowIndex:_13};
this.grid.doStartEdit(_12,_13);
this.grid.updateRow(_13);
}else{
this.info={};
}
this.grid.endUpdate();
this.grid.focus.focusGrid();
this._focusEditor(_12,_13);
this._doCatchBoomerang();
},_editorDo:function(_15){
var c=this.info.cell;
if(c&&c.editable){
c[_15](this.info.rowIndex);
}
},editorApply:function(){
this._editorDo("apply");
},editorCancel:function(){
this._editorDo("cancel");
},applyCellEdit:function(_16,_17,_18){
if(this.grid.canEdit(_17,_18)){
this.grid.doApplyCellEdit(_16,_18,_17.field);
}
},applyRowEdit:function(){
this.grid.doApplyEdit(this.info.rowIndex,this.info.cell.field);
},apply:function(){
if(this.isEditing()&&this._isValidInput()){
this.grid.beginUpdate();
this.editorApply();
this.applyRowEdit();
this.info={};
this.grid.endUpdate();
this.grid.focus.focusGrid();
this._doCatchBoomerang();
}
},cancel:function(){
if(this.isEditing()){
this.grid.beginUpdate();
this.editorCancel();
this.info={};
this.grid.endUpdate();
this.grid.focus.focusGrid();
this._doCatchBoomerang();
}
},save:function(_19,_1a){
var c=this.info.cell;
if(this.isEditRow(_19)&&(!_1a||c.view==_1a)&&c.editable){
c.save(c,this.info.rowIndex);
}
},restore:function(_1b,_1c){
var c=this.info.cell;
if(this.isEditRow(_1c)&&c.view==_1b&&c.editable){
c.restore(this.info.rowIndex);
}
},_isValidInput:function(){
var w=(this.info.cell||{}).widget;
if(!w||!w.isValid){
return true;
}
w.focused=true;
return w.isValid(true);
}});
});
