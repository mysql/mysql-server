//>>built
require({cache:{"url:dojox/calendar/templates/MobileCalendar.html":"<div>\n\t<div data-dojo-attach-point=\"viewContainer\" class=\"viewContainer\"></div>\n\t<div data-dojo-attach-point=\"buttonContainer\" class=\"buttonContainer\">\n\t\t\t<button data-dojo-attach-point=\"previousButton\" data-dojo-type=\"dojox.mobile.Button\" >◄</button>\n\t\t\t<button data-dojo-attach-point=\"todayButton\" data-dojo-type=\"dojox.mobile.Button\" >Today</button>\n\t\t\t<button data-dojo-attach-point=\"dayButton\" data-dojo-type=\"dojox.mobile.Button\" >Day</button>\n\t\t\t<button data-dojo-attach-point=\"weekButton\" data-dojo-type=\"dojox.mobile.Button\" >Week</button>\t\t\t\n\t\t\t<button data-dojo-attach-point=\"monthButton\" data-dojo-type=\"dojox.mobile.Button\" >Month</button>\n\t\t<button data-dojo-attach-point=\"nextButton\" data-dojo-type=\"dojox.mobile.Button\" >►</button>\n\t</div>\n</div>\n"}});
define("dojox/calendar/MobileCalendar",["dojo/_base/declare","dojo/_base/lang","./CalendarBase","./ColumnView","./ColumnViewSecondarySheet","./MobileVerticalRenderer","./MatrixView","./MobileHorizontalRenderer","./LabelRenderer","./ExpandRenderer","./Touch","dojo/text!./templates/MobileCalendar.html","dojox/mobile/Button"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _1("dojox.calendar.MobileCalendar",_3,{templateString:_c,_createDefaultViews:function(){
var _d=_1([_5,_b]);
var _e=_1([_4,_b])(_2.mixin({secondarySheetClass:_d,verticalRenderer:_6,horizontalRenderer:_8,expandRenderer:_a},this.columnViewProps));
var _f=_1([_7,_b])(_2.mixin({horizontalRenderer:_8,labelRenderer:_9,expandRenderer:_a},this.matrixViewProps));
this.columnView=_e;
this.matrixView=_f;
var _10=[_e,_f];
this.installDefaultViewsActions(_10);
return _10;
},installDefaultViewsActions:function(_11){
this.matrixView.on("rowHeaderClick",_2.hitch(this,this.matrixViewRowHeaderClick));
this.columnView.on("columnHeaderClick",_2.hitch(this,this.columnViewColumnHeaderClick));
}});
});
