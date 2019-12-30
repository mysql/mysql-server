require({cache:{
'url:dojox/calendar/templates/MobileCalendar.html':"<div>\n\t<div data-dojo-attach-point=\"viewContainer\" class=\"viewContainer\"></div>\n\t<div data-dojo-attach-point=\"buttonContainer\" class=\"buttonContainer\">\n\t\t\t<button data-dojo-attach-point=\"previousButton\" data-dojo-type=\"dojox.mobile.Button\" >◄</button>\n\t\t\t<button data-dojo-attach-point=\"todayButton\" data-dojo-type=\"dojox.mobile.Button\" />Today</button>\n\t\t\t<button data-dojo-attach-point=\"dayButton\" data-dojo-type=\"dojox.mobile.Button\" >Day</button>\n\t\t\t<button data-dojo-attach-point=\"weekButton\" data-dojo-type=\"dojox.mobile.Button\" >Week</button>\t\t\t\n\t\t\t<button data-dojo-attach-point=\"monthButton\" data-dojo-type=\"dojox.mobile.Button\" >Month</button>\n\t\t<button data-dojo-attach-point=\"nextButton\" data-dojo-type=\"dojox.mobile.Button\" >►</button>\n\t</div>\n</div>\n"}});
define("dojox/calendar/MobileCalendar", ["dojo/_base/declare", "dojo/_base/lang", "./CalendarBase", "./ColumnView", "./ColumnViewSecondarySheet", 
				"./MobileVerticalRenderer", "./MatrixView",	"./MobileHorizontalRenderer", "./LabelRenderer", 
				"./ExpandRenderer", "./Touch", "dojo/text!./templates/MobileCalendar.html", "dojox/mobile/Button"],
	
	function(declare, lang, CalendarBase, ColumnView, ColumnViewSecondarySheet, VerticalRenderer, 
					 MatrixView, HorizontalRenderer, LabelRenderer, ExpandRenderer, Touch, template){
	
	return declare("dojox.calendar.MobileCalendar", CalendarBase, {
		
		// summary:
		//		This class defines a calendar widget that display events in time designed to be used in mobile environment.
		
		templateString: template,
		
		_createDefaultViews: function(){
			// summary:
			//		Creates the default views:
			//		- A dojox.calendar.ColumnView instance used to display one day to seven days time intervals,
			//		- A dojox.calendar.MatrixView instance used to display the other time intervals.
			//		The views are mixed with Mouse and Keyboard to allow editing items using mouse and keyboard.

			var secondarySheetClass = declare([ColumnViewSecondarySheet, Touch]);
			
			var colView = declare([ColumnView, Touch])(lang.mixin({
				secondarySheetClass: secondarySheetClass,
				verticalRenderer: VerticalRenderer,
				horizontalRenderer: HorizontalRenderer,
				expandRenderer: ExpandRenderer
			}, this.columnViewProps));
			
			var matrixView = declare([MatrixView, Touch])(lang.mixin({
				horizontalRenderer: HorizontalRenderer,
				labelRenderer: LabelRenderer,
				expandRenderer: ExpandRenderer
			}, this.matrixViewProps));
								
			this.columnView = colView;
			this.matrixView = matrixView;
			
			var views = [colView, matrixView];
			
			this.installDefaultViewsActions(views);
			
			return views;
		},
		
		installDefaultViewsActions: function(views){
			// summary:
			//		Installs the default actions on newly created default views.
			//		By default this action is registering:
			//		- the matrixViewRowHeaderClick method	on the rowHeaderClick event of the matrix view.
			//		- the columnViewColumnHeaderClick method	on the columnHeaderClick event of the column view.
			this.matrixView.on("rowHeaderClick", lang.hitch(this, this.matrixViewRowHeaderClick));
			this.columnView.on("columnHeaderClick", lang.hitch(this, this.columnViewColumnHeaderClick));			
		}
		
	}) 
});
