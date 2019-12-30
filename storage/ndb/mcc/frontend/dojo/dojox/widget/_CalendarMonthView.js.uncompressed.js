require({cache:{
'url:dojox/widget/Calendar/CalendarMonth.html':"<div class=\"dojoxCalendarMonthLabels\" style=\"left: 0px;\"  \n\tdojoAttachPoint=\"monthContainer\" dojoAttachEvent=\"onclick: onClick\">\n    <table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" style=\"margin: auto;\">\n        <tbody>\n            <tr class=\"dojoxCalendarMonthGroupTemplate\">\n                <td class=\"dojoxCalendarMonthTemplate\">\n                    <div class=\"dojoxCalendarMonthLabel\"></div>\n                </td>\n             </tr>\n        </tbody>\n    </table>\n</div>\n"}});
define("dojox/widget/_CalendarMonthView", [
	"dojo/_base/declare",
	"./_CalendarView",
	"dijit/_TemplatedMixin",
	"./_CalendarMonthYearView",
	"dojo/dom-class",
	"dojo/_base/event",
	"dojo/text!./Calendar/CalendarMonth.html"
], function(declare, _CalendarView, _TemplatedMixin, _CalendarMonthYearView, domClass, event, template){
	return declare("dojox.widget._CalendarMonthView", [_CalendarView, _TemplatedMixin], {
		// summary:
		//		A Calendar view listing the 12 months of the year

		// templateString: String
		//		The template to be used to construct the widget.
		templateString: template,

		// datePart: String
		//		Specifies how much to increment the displayed date when the user
		//		clicks the array button to increment of decrement the view.
		datePart: "year",

		// headerClass: String
		//		Specifies the CSS class to apply to the header node for this view.
		headerClass: "dojoxCalendarMonthHeader",

		postCreate: function(){
			// summary:
			//		Constructs the view
			this.cloneClass(".dojoxCalendarMonthTemplate", 3);
			this.cloneClass(".dojoxCalendarMonthGroupTemplate", 2);
			this._populateMonths();

			// Add visual effects to the view, if any have been mixed in
			this.addFx(".dojoxCalendarMonthLabel", this.domNode);
		},

		_setValueAttr: function(value){
			this.header.innerHTML = value.getFullYear();
		},

		_getMonthNames: _CalendarMonthYearView.prototype._getMonthNames,

		_populateMonths: _CalendarMonthYearView.prototype._populateMonths,

		onClick: function(evt){
			// summary:
			//		Handles clicks on month names
			if(!domClass.contains(evt.target, "dojoxCalendarMonthLabel")){event.stop(evt); return;}
			var parentNode = evt.target.parentNode;
			var month = parentNode.cellIndex + (parentNode.parentNode.rowIndex * 4);
			var date = this.get("value");

			// Seeing a really strange bug in FF3.6 where this has to be called twice
			// in order to take affect
			date.setMonth(month);
			date.setMonth(month);
			this.onValueSelected(date, month);
		}
	});
});