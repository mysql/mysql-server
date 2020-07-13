//>>built
require({cache:{"url:dojox/calendar/templates/ColumnView.html":"<div data-dojo-attach-events=\"keydown:_onKeyDown\">\n\t\n\t<div data-dojo-attach-point=\"header\" class=\"dojoxCalendarHeader\">\n\t\t<div class=\"dojoxCalendarYearColumnHeader\" data-dojo-attach-point=\"yearColumnHeader\">\n\t\t\t<table cellspacing=\"0\" cellpadding=\"0\"><tr><td><span data-dojo-attach-point=\"yearColumnHeaderContent\"></span></td></tr></table>\t\t\n\t\t</div>\n\t\t<div data-dojo-attach-point=\"columnHeader\" class=\"dojoxCalendarColumnHeader\">\n\t\t\t<table data-dojo-attach-point=\"columnHeaderTable\" class=\"dojoxCalendarColumnHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t\t</div>\n\t</div>\n\t\n\t<div data-dojo-attach-point=\"secondarySheetNode\"></div>\n\t\n\t<div data-dojo-attach-point=\"subHeader\" class=\"dojoxCalendarSubHeader\">\n\t\t<div class=\"dojoxCalendarSubRowHeader\">\n\t\t\t<table cellspacing=\"0\" cellpadding=\"0\"><tr><td></td></tr></table>\t\t\n\t\t</div>\n\t\t<div data-dojo-attach-point=\"subColumnHeader\" class=\"dojoxCalendarSubColumnHeader\">\n\t\t\t<table data-dojo-attach-point=\"subColumnHeaderTable\" class=\"dojoxCalendarSubColumnHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t\t</div>\n\t</div>\n\t\n\t<div data-dojo-attach-point=\"scrollContainer\" class=\"dojoxCalendarScrollContainer\">\n\t\t<div data-dojo-attach-point=\"sheetContainer\" style=\"position:relative;left:0;right:0;margin:0;padding:0\">\n\t\t\t<div data-dojo-attach-point=\"rowHeader\" class=\"dojoxCalendarRowHeader\">\n\t\t\t\t<table data-dojo-attach-point=\"rowHeaderTable\" class=\"dojoxCalendarRowHeaderTable\" cellpadding=\"0\" cellspacing=\"0\"></table>\n\t\t\t</div>\n\t\t\t<div data-dojo-attach-point=\"grid\" class=\"dojoxCalendarGrid\">\n\t\t\t\t<table data-dojo-attach-point=\"gridTable\" class=\"dojoxCalendarGridTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t\t\t</div>\n\t\t\t<div data-dojo-attach-point=\"itemContainer\" class=\"dojoxCalendarContainer\" data-dojo-attach-event=\"mousedown:_onGridMouseDown,mouseup:_onGridMouseUp,ondblclick:_onGridDoubleClick,touchstart:_onGridTouchStart,touchmove:_onGridTouchMove,touchend:_onGridTouchEnd\">\n\t\t\t\t<table data-dojo-attach-point=\"itemContainerTable\" class=\"dojoxCalendarContainerTable\" cellpadding=\"0\" cellspacing=\"0\" style=\"width:100%\"></table>\n\t\t\t</div>\n\t\t</div> \n\t</div>\n\t\n\t<div data-dojo-attach-point=\"vScrollBar\" class=\"dojoxCalendarVScrollBar\">\n\t\t<div data-dojo-attach-point=\"vScrollBarContent\" style=\"visibility:hidden;position:relative;width:1px;height:1px;\" ></div>\n\t</div>\n\t\n\t<div data-dojo-attach-point=\"hScrollBar\" class=\"dojoxCalendarHScrollBar\">\n\t\t<div data-dojo-attach-point=\"hScrollBarContent\" style=\"visibility:hidden;position:relative;width:1px;height:1px;\" ></div>\n\t</div>\n\t\n</div>\n"}});
define("dojox/calendar/ColumnView",["dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/sniff","dojo/_base/fx","dojo/dom","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/dom-construct","dojo/on","dojo/date","dojo/date/locale","dojo/query","./SimpleColumnView","dojo/text!./templates/ColumnView.html","./ColumnViewSecondarySheet"],function(_1,_2,_3,_4,_5,fx,_6,_7,_8,_9,_a,on,_b,_c,_d,_e,_f,_10){
return _2("dojox.calendar.ColumnView",_e,{templateString:_f,baseClass:"dojoxCalendarColumnView",secondarySheetClass:_10,secondarySheetProps:null,headerPadding:3,_showSecondarySheet:true,buildRendering:function(){
this.inherited(arguments);
if(this.secondarySheetNode){
var _11=_4.mixin({owner:this},this.secondarySheetProps);
this.secondarySheet=new this.secondarySheetClass(_11,this.secondarySheetNode);
this.secondarySheetNode=this.secondarySheet.domNode;
}
},destroy:function(_12){
if(this.secondarySheet){
this.secondarySheet.destroy(_12);
}
this.inherited(arguments);
},_setVisibility:function(_13){
this.inherited(arguments);
if(this.secondarySheet){
this.secondarySheet._setVisibility(_13);
}
},resize:function(_14){
this.inherited(arguments);
if(this.secondarySheet){
this.secondarySheet.resize();
}
},invalidateLayout:function(){
this._layoutRenderers(this.renderData);
if(this.secondarySheet){
this.secondarySheet._layoutRenderers(this.secondarySheet.renderData);
}
},onRowHeaderClick:function(e){
},_setSubColumnsAttr:function(_15){
var old=this.get("subColumns");
if(old!=_15){
this._secondaryHeightInvalidated=true;
}
this._set("subColumns",_15);
},refreshRendering:function(_16){
this.inherited(arguments);
if(this._secondaryHeightInvalidated){
this._secondaryHeightInvalidated=false;
var h=_9.getMarginBox(this.secondarySheetNode).h;
this.resizeSecondarySheet(h);
}
if(_16&&this.secondarySheet){
this.secondarySheet.refreshRendering(true);
}
},resizeSecondarySheet:function(_17){
if(this.secondarySheetNode){
var _18=_9.getMarginBox(this.header).h;
_8.set(this.secondarySheetNode,"height",_17+"px");
this.secondarySheet._resizeHandler(null,true);
var top=(_17+_18+this.headerPadding);
if(this.subHeader&&this.subColumns){
_8.set(this.subHeader,"top",top+"px");
top+=_9.getMarginBox(this.subHeader).h;
}
_8.set(this.scrollContainer,"top",top+"px");
if(this.vScrollBar){
_8.set(this.vScrollBar,"top",top+"px");
}
}
},updateRenderers:function(obj,_19){
this.inherited(arguments);
if(this.secondarySheet){
this.secondarySheet.updateRenderers(obj,_19);
}
},_setItemsAttr:function(_1a){
this.inherited(arguments);
if(this.secondarySheet){
this.secondarySheet.set("items",_1a);
}
},_setDecorationItemsAttr:function(_1b){
this.inherited(arguments);
if(this.secondarySheet){
this.secondarySheet.set("decorationItems",_1b);
}
},_setStartDateAttr:function(_1c){
this.inherited(arguments);
if(this.secondarySheet){
this.secondarySheet.set("startDate",_1c);
}
},_setColumnCountAttr:function(_1d){
this.inherited(arguments);
if(this.secondarySheet){
this.secondarySheet.set("columnCount",_1d);
}
},_setHorizontalRendererAttr:function(_1e){
if(this.secondarySheet){
this.secondarySheet.set("horizontalRenderer",_1e);
}
},_getHorizontalRendererAttr:function(){
if(this.secondarySheet){
return this.secondarySheet.get("horizontalRenderer");
}
return null;
},_setHorizontalDecorationRendererAttr:function(_1f){
this.inherited(arguments);
if(this.secondarySheet){
this.secondarySheet.set("horizontalDecorationRenderer",_1f);
}
},_getHorizontalRendererAttr:function(){
if(this.secondarySheet){
return this.secondarySheet.get("horizontalDecorationRenderer");
}
return null;
},_setExpandRendererAttr:function(_20){
if(this.secondarySheet){
this.secondarySheet.set("expandRenderer",_20);
}
},_getExpandRendererAttr:function(){
if(this.secondarySheet){
return this.secondarySheet.get("expandRenderer");
}
return null;
},_setTextDirAttr:function(_21){
this.secondarySheet.set("textDir",_21);
this._set("textDir",_21);
},_defaultItemToRendererKindFunc:function(_22){
return _22.allDay?null:"vertical";
},getSecondarySheet:function(){
return this.secondarySheet;
},_onGridTouchStart:function(e){
this.inherited(arguments);
this._doEndItemEditing(this.secondarySheet,"touch");
},_onGridMouseDown:function(e){
this.inherited(arguments);
this._doEndItemEditing(this.secondarySheet,"mouse");
},_configureScrollBar:function(_23){
this.inherited(arguments);
if(this.secondarySheetNode){
var _24=this.isLeftToRight()?true:this.scrollBarRTLPosition=="right";
_8.set(this.secondarySheetNode,_24?"right":"left",_23.scrollbarWidth+"px");
_8.set(this.secondarySheetNode,_24?"left":"right","0");
_1.forEach(this.secondarySheet._hScrollNodes,function(elt){
_7[_23.hScrollBarEnabled?"add":"remove"](elt.parentNode,"dojoxCalendarHorizontalScroll");
},this);
}
},_configureHScrollDomNodes:function(_25){
this.inherited(arguments);
if(this.secondarySheet&&this.secondarySheet._configureHScrollDomNodes){
this.secondarySheet._configureHScrollDomNodes(_25);
}
},_setHScrollPosition:function(pos){
this.inherited(arguments);
if(this.secondarySheet){
this.secondarySheet._setHScrollPosition(pos);
}
},_refreshItemsRendering:function(){
this.inherited(arguments);
if(this.secondarySheet){
var rd=this.secondarySheet.renderData;
this.secondarySheet._computeVisibleItems(rd);
this.secondarySheet._layoutRenderers(rd);
}
},_layoutRenderers:function(_26){
if(!this.secondarySheet._domReady){
this.secondarySheet._domReady=true;
this.secondarySheet._layoutRenderers(this.secondarySheet.renderData);
}
this.inherited(arguments);
},_layoutDecorationRenderers:function(_27){
if(!this.secondarySheet._decDomReady){
this.secondarySheet._decDomReady=true;
this.secondarySheet._layoutDecorationRenderers(this.secondarySheet.renderData);
}
this.inherited(arguments);
},invalidateRendering:function(){
if(this.secondarySheet){
this.secondarySheet.invalidateRendering();
}
this.inherited(arguments);
}});
});
