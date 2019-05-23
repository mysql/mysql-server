//>>built
define("dojox/grid/Selection",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/dom-attr"],function(_1,_2,_3,_4){
return _1("dojox.grid.Selection",null,{constructor:function(_5){
this.grid=_5;
this.selected=[];
this.setMode(_5.selectionMode);
},mode:"extended",selected:null,updating:0,selectedIndex:-1,rangeStartIndex:-1,setMode:function(_6){
if(this.selected.length){
this.deselectAll();
}
if(_6!="extended"&&_6!="multiple"&&_6!="single"&&_6!="none"){
this.mode="extended";
}else{
this.mode=_6;
}
},onCanSelect:function(_7){
return this.grid.onCanSelect(_7);
},onCanDeselect:function(_8){
return this.grid.onCanDeselect(_8);
},onSelected:function(_9){
},onDeselected:function(_a){
},onChanging:function(){
},onChanged:function(){
},isSelected:function(_b){
if(this.mode=="none"){
return false;
}
return this.selected[_b];
},getFirstSelected:function(){
if(!this.selected.length||this.mode=="none"){
return -1;
}
for(var i=0,l=this.selected.length;i<l;i++){
if(this.selected[i]){
return i;
}
}
return -1;
},getNextSelected:function(_c){
if(this.mode=="none"){
return -1;
}
for(var i=_c+1,l=this.selected.length;i<l;i++){
if(this.selected[i]){
return i;
}
}
return -1;
},getSelected:function(){
var _d=[];
for(var i=0,l=this.selected.length;i<l;i++){
if(this.selected[i]){
_d.push(i);
}
}
return _d;
},getSelectedCount:function(){
var c=0;
for(var i=0;i<this.selected.length;i++){
if(this.selected[i]){
c++;
}
}
return c;
},_beginUpdate:function(){
if(this.updating===0){
this.onChanging();
}
this.updating++;
},_endUpdate:function(){
this.updating--;
if(this.updating===0){
this.onChanged();
}
},select:function(_e){
if(this.mode=="none"){
return;
}
if(this.mode!="multiple"){
this.deselectAll(_e);
this.addToSelection(_e);
}else{
this.toggleSelect(_e);
}
},addToSelection:function(_f){
if(this.mode=="none"){
return;
}
if(_3.isArray(_f)){
_2.forEach(_f,this.addToSelection,this);
return;
}
_f=Number(_f);
if(this.selected[_f]){
this.selectedIndex=_f;
}else{
if(this.onCanSelect(_f)!==false){
this.selectedIndex=_f;
var _10=this.grid.getRowNode(_f);
if(_10){
_4.set(_10,"aria-selected","true");
}
this._beginUpdate();
this.selected[_f]=true;
this.onSelected(_f);
this._endUpdate();
}
}
},deselect:function(_11){
if(this.mode=="none"){
return;
}
if(_3.isArray(_11)){
_2.forEach(_11,this.deselect,this);
return;
}
_11=Number(_11);
if(this.selectedIndex==_11){
this.selectedIndex=-1;
}
if(this.selected[_11]){
if(this.onCanDeselect(_11)===false){
return;
}
var _12=this.grid.getRowNode(_11);
if(_12){
_4.set(_12,"aria-selected","false");
}
this._beginUpdate();
delete this.selected[_11];
this.onDeselected(_11);
this._endUpdate();
}
},setSelected:function(_13,_14){
this[(_14?"addToSelection":"deselect")](_13);
},toggleSelect:function(_15){
if(_3.isArray(_15)){
_2.forEach(_15,this.toggleSelect,this);
return;
}
this.setSelected(_15,!this.selected[_15]);
},_range:function(_16,_17,_18){
var s=(_16>=0?_16:_17),e=_17;
if(s>e){
e=s;
s=_17;
}
for(var i=s;i<=e;i++){
_18(i);
}
},selectRange:function(_19,_1a){
this._range(_19,_1a,_3.hitch(this,"addToSelection"));
},deselectRange:function(_1b,_1c){
this._range(_1b,_1c,_3.hitch(this,"deselect"));
},insert:function(_1d){
this.selected.splice(_1d,0,false);
if(this.selectedIndex>=_1d){
this.selectedIndex++;
}
},remove:function(_1e){
this.selected.splice(_1e,1);
if(this.selectedIndex>=_1e){
this.selectedIndex--;
}
},deselectAll:function(_1f){
for(var i in this.selected){
if((i!=_1f)&&(this.selected[i]===true)){
this.deselect(i);
}
}
},clickSelect:function(_20,_21,_22){
if(this.mode=="none"){
return;
}
this._beginUpdate();
if(this.mode!="extended"){
this.select(_20);
}else{
if(!_22||this.rangeStartIndex<0){
this.rangeStartIndex=_20;
}
if(!_21){
this.deselectAll(_20);
}
if(_22){
this.selectRange(this.rangeStartIndex,_20);
}else{
if(_21){
this.toggleSelect(_20);
}else{
this.addToSelection(_20);
}
}
}
this._endUpdate();
},clickSelectEvent:function(e){
this.clickSelect(e.rowIndex,dojo.isCopyKey(e),e.shiftKey);
},clear:function(){
this._beginUpdate();
this.deselectAll();
this._endUpdate();
}});
});
