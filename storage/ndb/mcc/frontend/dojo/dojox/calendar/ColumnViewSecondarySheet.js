//>>built
require({cache:{"url:dojox/calendar/templates/ColumnViewSecondarySheet.html":"<div data-dojo-attach-events=\"keydown:_onKeyDown\">\n\t<div  data-dojo-attach-point=\"rowHeader\" class=\"dojoxCalendarRowHeader\">\n\t\t<table data-dojo-attach-point=\"rowHeaderTable\" class=\"dojoxCalendarRowHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t</div>\t\n\t<div data-dojo-attach-point=\"grid\" class=\"dojoxCalendarGrid\">\n\t\t<table data-dojo-attach-point=\"gridTable\" class=\"dojoxCalendarGridTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t</div>\n\t<div data-dojo-attach-point=\"itemContainer\" class=\"dojoxCalendarContainer\" data-dojo-attach-event=\"mousedown:_onGridMouseDown,mouseup:_onGridMouseUp,ondblclick:_onGridDoubleClick,touchstart:_onGridTouchStart,touchmove:_onGridTouchMove,touchend:_onGridTouchEnd\">\n\t\t<table data-dojo-attach-point=\"itemContainerTable\" class=\"dojoxCalendarContainerTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t</div>\n</div>\n"}});
define("dojox/calendar/ColumnViewSecondarySheet",["./MatrixView","dojo/text!./templates/ColumnViewSecondarySheet.html","dojo/_base/html","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/sniff","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-construct","dojo/date","dojo/date/locale","dojo/query","dojox/html/metrics","dojo/_base/fx","dojo/on","dojo/i18n","dojo/window"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,fx,on,_10,win){
return _4("dojox.calendar.ColumnViewSecondarySheet",_1,{templateString:_2,rowCount:1,cellPaddingTop:4,roundToDay:false,_defaultHeight:-1,layoutDuringResize:true,_defaultItemToRendererKindFunc:function(_11){
return _11.allDay?"horizontal":null;
},_formatGridCellLabel:function(){
return null;
},_formatRowHeaderLabel:function(){
return null;
},__fixEvt:function(e){
e.sheet="secondary";
e.source=this;
return e;
},_dispatchCalendarEvt:function(e,_12){
e=this.inherited(arguments);
if(this.owner.owner){
this.owner.owner[_12](e);
}
},_layoutExpandRenderers:function(_13,_14,_15){
if(!this.expandRenderer){
return;
}
var h=_a.getMarginBox(this.domNode).h;
if(this._defaultHeight==-1){
this._defaultHeight=h;
}
if(this._defaultHeight!=-1&&this._defaultHeight!=h&&h>=this._getExpandedHeight()){
this._layoutExpandRendererImpl(0,this._expandedRowCol,null,true);
}else{
this.inherited(arguments);
}
},expandRendererClickHandler:function(e,_16){
_5.stop(e);
var h=_a.getMarginBox(this.domNode).h;
if(this._defaultHeight==h||h<this._getExpandedHeight()){
this._expandedRowCol=_16.columnIndex;
this.owner.resizeSecondarySheet(this._getExpandedHeight());
}else{
this.owner.resizeSecondarySheet(this._defaultHeight);
}
},_getExpandedHeight:function(){
return this.naturalRowsHeight[0]+this.expandRendererHeight+this.verticalGap+this.verticalGap;
},_layoutRenderers:function(_17){
if(!this._domReady){
return;
}
this.inherited(arguments);
}});
});
