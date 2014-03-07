//>>built
define("dojox/grid/cells/tree",["dojo/_base/kernel","../../main","dojo/_base/lang","../cells"],function(_1,_2,_3){
_2.grid.cells.TreeCell={formatAggregate:function(_4,_5,_6){
var f,g=this.grid,i=g.edit.info,d=g.aggregator?g.aggregator.getForCell(this,_5,_4,_5===this.level?"cnt":this.parentCell.aggregate):(this.value||this.defaultValue);
return this._defaultFormat(d,[d,_5-this.level,_6,this]);
},formatIndexes:function(_7,_8){
var f,g=this.grid,i=g.edit.info,d=this.get?this.get(_7[0],_8,_7):(this.value||this.defaultValue);
if(this.editable&&(this.alwaysEditing||(i.rowIndex==_7[0]&&i.cell==this))){
return this.formatEditing(d,_7[0],_7);
}else{
return this._defaultFormat(d,[d,_7[0],_7,this]);
}
},getOpenState:function(_9){
var _a=this.grid,_b=_a.store,_c=null;
if(_b.isItem(_9)){
_c=_9;
_9=_b.getIdentity(_9);
}
if(!this.openStates){
this.openStates={};
}
if(typeof _9!="string"||!(_9 in this.openStates)){
this.openStates[_9]=_a.getDefaultOpenState(this,_c);
}
return this.openStates[_9];
},formatAtLevel:function(_d,_e,_f,_10,_11,_12){
if(!_3.isArray(_d)){
_d=[_d];
}
var _13="";
if(_f>this.level||(_f===this.level&&_10)){
_12.push("dojoxGridSpacerCell");
if(_f===this.level){
_12.push("dojoxGridTotalCell");
}
_13="<span></span>";
}else{
if(_f<this.level){
_12.push("dojoxGridSummaryCell");
_13="<span class=\"dojoxGridSummarySpan\">"+this.formatAggregate(_e,_f,_d)+"</span>";
}else{
var ret="";
if(this.isCollapsable){
var _14=this.grid.store,id="";
if(_14.isItem(_e)){
id=_14.getIdentity(_e);
}
_12.push("dojoxGridExpandoCell");
ret="<span "+_1._scopeName+"Type=\"dojox.grid._Expando\" level=\""+_f+"\" class=\"dojoxGridExpando\""+"\" toggleClass=\""+_11+"\" itemId=\""+id+"\" cellIdx=\""+this.index+"\"></span>";
}
_13=ret+this.formatIndexes(_d,_e);
}
}
if(this.grid.focus.cell&&this.index==this.grid.focus.cell.index&&_d.join("/")==this.grid.focus.rowIndex){
_12.push(this.grid.focus.focusClass);
}
return _13;
}};
return _2.grid.cells.TreeCell;
});
