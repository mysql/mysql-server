//>>built
define("dojox/grid/_RowSelector",["dojo/_base/declare","./_View"],function(_1,_2){
return _1("dojox.grid._RowSelector",_2,{defaultWidth:"2em",noscroll:true,padBorderWidth:2,buildRendering:function(){
this.inherited("buildRendering",arguments);
this.scrollboxNode.style.overflow="hidden";
this.headerNode.style.visibility="hidden";
},getWidth:function(){
return this.viewWidth||this.defaultWidth;
},buildRowContent:function(_3,_4){
var w=this.contentWidth||0;
_4.innerHTML="<table class=\"dojoxGridRowbarTable\" style=\"width:"+w+"px;height:1px;\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\" role=\"presentation\"><tr><td class=\"dojoxGridRowbarInner\">&nbsp;</td></tr></table>";
},renderHeader:function(){
},updateRow:function(){
},resize:function(){
this.adaptHeight();
},adaptWidth:function(){
if(!("contentWidth" in this)&&this.contentNode&&this.contentNode.offsetWidth>0){
this.contentWidth=this.contentNode.offsetWidth-this.padBorderWidth;
}
},doStyleRowNode:function(_5,_6){
var n=["dojoxGridRowbar dojoxGridNonNormalizedCell"];
if(this.grid.rows.isOver(_5)){
n.push("dojoxGridRowbarOver");
}
if(this.grid.selection.isSelected(_5)){
n.push("dojoxGridRowbarSelected");
}
_6.className=n.join(" ");
},domouseover:function(e){
this.grid.onMouseOverRow(e);
},domouseout:function(e){
if(!this.isIntraRowEvent(e)){
this.grid.onMouseOutRow(e);
}
}});
});
