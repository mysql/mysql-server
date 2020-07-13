//>>built
define("dojox/grid/cells/tree",["dojo/_base/kernel","../../main","dojo/_base/lang","../cells"],function(_1,_2,_3){
_2.grid.cells.TreeCell={formatAggregate:function(_4,_5,_6){
var f,g=this.grid,i=g.edit.info,d=g.aggregator?g.aggregator.getForCell(this,_5,_4,_5===this.level?"cnt":this.parentCell.aggregate):(this.value||this.defaultValue);
var _7=this._defaultFormat(d,[d,_5-this.level,_6,this]);
var _8=this.textDir||this.grid.textDir;
if(_8&&this._enforceTextDirWithUcc){
_7=this._enforceTextDirWithUcc(_8,_7);
}
return _7;
},formatIndexes:function(_9,_a){
var f,g=this.grid,i=g.edit.info,d=this.get?this.get(_9[0],_a,_9):(this.value||this.defaultValue);
if(this.editable&&(this.alwaysEditing||(i.rowIndex==_9[0]&&i.cell==this))){
return this.formatEditing(d,_9[0],_9);
}else{
var _b=this._defaultFormat(d,[d,_9[0],_9,this]);
var _c=this.textDir||this.grid.textDir;
if(_c&&this._enforceTextDirWithUcc){
_b=this._enforceTextDirWithUcc(_c,_b);
}
return _b;
}
},getOpenState:function(_d){
var _e=this.grid,_f=_e.store,itm=null;
if(_f.isItem(_d)){
itm=_d;
_d=_f.getIdentity(_d);
}
if(!this.openStates){
this.openStates={};
}
if(typeof _d!="string"||!(_d in this.openStates)){
this.openStates[_d]=_e.getDefaultOpenState(this,itm);
}
return this.openStates[_d];
},formatAtLevel:function(_10,_11,_12,_13,_14,_15){
if(!_3.isArray(_10)){
_10=[_10];
}
var _16="";
if(_12>this.level||(_12===this.level&&_13)){
_15.push("dojoxGridSpacerCell");
if(_12===this.level){
_15.push("dojoxGridTotalCell");
}
_16="<span></span>";
}else{
if(_12<this.level){
_15.push("dojoxGridSummaryCell");
_16="<span class=\"dojoxGridSummarySpan\">"+this.formatAggregate(_11,_12,_10)+"</span>";
}else{
var ret="";
if(this.isCollapsable){
var _17=this.grid.store,id="";
if(_17.isItem(_11)){
id=_17.getIdentity(_11);
}
_15.push("dojoxGridExpandoCell");
ret="<span "+_1._scopeName+"Type=\"dojox.grid._Expando\" level=\""+_12+"\" class=\"dojoxGridExpando\""+"\" toggleClass=\""+_14+"\" itemId=\""+id+"\" cellIdx=\""+this.index+"\"></span>";
}
_16=ret+this.formatIndexes(_10,_11);
}
}
if(this.grid.focus.cell&&this.index==this.grid.focus.cell.index&&_10.join("/")==this.grid.focus.rowIndex){
_15.push(this.grid.focus.focusClass);
}
return _16;
}};
return _2.grid.cells.TreeCell;
});
