//>>built
define("dojox/grid/_RowManager",["dojo/_base/declare","dojo/_base/lang","dojo/dom-class"],function(_1,_2,_3){
var _4=function(_5,_6){
if(_5.style.cssText==undefined){
_5.setAttribute("style",_6);
}else{
_5.style.cssText=_6;
}
};
return _1("dojox.grid._RowManager",null,{constructor:function(_7){
this.grid=_7;
},linesToEms:2,overRow:-2,prepareStylingRow:function(_8,_9){
return {index:_8,node:_9,odd:Boolean(_8&1),selected:!!this.grid.selection.isSelected(_8),over:this.isOver(_8),customStyles:"",customClasses:"dojoxGridRow"};
},styleRowNode:function(_a,_b){
var _c=this.prepareStylingRow(_a,_b);
this.grid.onStyleRow(_c);
this.applyStyles(_c);
},applyStyles:function(_d){
var i=_d;
i.node.className=i.customClasses;
var h=i.node.style.height;
_4(i.node,i.customStyles+";"+(i.node._style||""));
i.node.style.height=h;
},updateStyles:function(_e){
this.grid.updateRowStyles(_e);
},setOverRow:function(_f){
var _10=this.overRow;
this.overRow=_f;
if((_10!=this.overRow)&&(_2.isString(_10)||_10>=0)){
this.updateStyles(_10);
}
this.updateStyles(this.overRow);
},isOver:function(_11){
return (this.overRow==_11&&!_3.contains(this.grid.domNode,"dojoxGridColumnResizing"));
}});
});
