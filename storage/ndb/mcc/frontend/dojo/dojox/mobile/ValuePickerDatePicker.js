define([
	"dojo/_base/declare",
	"dojo/dom-class",
	"dojo/dom-attr",
	"./_DatePickerMixin",
	"./ValuePicker",
	"./ValuePickerSlot"
], function(declare, domClass, domAttr, DatePickerMixin, ValuePicker, ValuePickerSlot){

	// module:
	//		dojox/mobile/ValuePickerDatePicker

	return declare("dojox.mobile.ValuePickerDatePicker", [ValuePicker, DatePickerMixin], {
		// summary:
		//		A ValuePicker-based date picker widget.
		// description:
		//		ValuePickerDatePicker is a date picker widget. It is a subclass of
		//		dojox/mobile/ValuePicker. It has 3 slots: day, month and year.
		
		// readOnly: [const] Boolean
		//		If true, slot input fields are read-only. Only the plus and
		//		minus buttons can be used to change the values.
		//		Note that changing the value of the property after the widget 
		//		creation has no effect.
		readOnly: false,
		
		// yearPlusBtnLabel: String
		//		(Accessibility) Label for year plus button
		yearPlusBtnLabel: "",
		
		// yearPlusBtnLabelRef: String
		//		(Accessibility) Reference to a node id containing text label for the year plus button
		yearPlusBtnLabelRef: "",
		
		// yearPlusBtnLabel: String
		//		(Accessibility) Label for year minus button
		yearMinusBtnLabel: "",
		
		// yearPlusBtnLabelRef: String
		//		(Accessibility) Reference to a node id containing text label for the year minus button
		yearMinusBtnLabelRef: "",

		// monthPlusBtnLabel: String
		//		(Accessibility) Label for month plus button
		monthPlusBtnLabel: "",
		
		// monthPlusBtnLabelRef: String
		//		(Accessibility) Reference to a node id containing text label for the month plus button
		monthPlusBtnLabelRef: "",
		
		// monthMinusBtnLabel: String
		//		(Accessibility) Label for month minus button
		monthMinusBtnLabel: "",
		
		// monthMinusBtnLabelRef: String
		//		(Accessibility) Reference to a node id containing text label for the month minus button
		monthMinusBtnLabelRef: "",

		// dayPlusBtnLabel: String
		//		(Accessibility) Label for day plus button
		dayPlusBtnLabel: "",
		
		// dayPlusBtnLabelRef: String
		//		(Accessibility) Reference to a node id containing text label for the day plus button
		dayPlusBtnLabelRef: "",
		
		// dayMinusBtnLabel: String
		//		(Accessibility) Label for day minus button
		dayMinusBtnLabel: "",
		
		// dayMinusBtnLabelRef: String
		//		(Accessibility) Reference to a node id containing text label for the day minus button
		dayMinusBtnLabelRef: "",

		slotClasses: [
			ValuePickerSlot,
			ValuePickerSlot,
			ValuePickerSlot
		],

		slotProps: [
			{labelFrom:1970, labelTo:2038, style:{width:"87px"}},
			{style:{width:"72px"}},
			{style:{width:"72px"}}
		],

		buildRendering: function(){
			var p = this.slotProps;
			p[0].readOnly = p[1].readOnly = p[2].readOnly = this.readOnly;
			this._setBtnLabels(p);
			this.initSlots();
			this.inherited(arguments);
			domClass.add(this.domNode, "mblValuePickerDatePicker");
			this._conn = [
				this.connect(this.slots[0], "_spinToValue", "_onYearSet"),
				this.connect(this.slots[1], "_spinToValue", "_onMonthSet"),
				this.connect(this.slots[2], "_spinToValue", "_onDaySet")
			];
		},

		disableValues: function(/*Number*/daysInMonth){
			// summary:
			//		Disables the end days of the month to match the specified
			//		number of days of the month.
			var items = this.slots[2].items;
			if(this._tail){
				this.slots[2].items = items = items.concat(this._tail);
			}
			this._tail = items.slice(daysInMonth);
			items.splice(daysInMonth);
		},
		
		_setBtnLabels: function(slotProps){
		    //summary:
		    // Set a11y labels on the plus/minus buttons
			slotProps[0].plusBtnLabel = this.yearPlusBtnLabel;
			slotProps[0].plusBtnLabelRef = this.yearPlusBtnLabelRef;
			slotProps[0].minusBtnLabel = this.yearMinusBtnLabel;
			slotProps[0].minusBtnLabelRef = this.yearMinusBtnLabelRef;
			slotProps[1].plusBtnLabel = this.monthPlusBtnLabel; 
			slotProps[1].plusBtnLabelRef = this.monthPlusBtnLabelRef;
			slotProps[1].minusBtnLabel = this.monthMinusBtnLabel;
			slotProps[1].minusBtnLabelRef = this.monthMinusBtnLabelRef;
			slotProps[2].plusBtnLabel = this.dayPlusBtnLabel;
			slotProps[2].plusBtnLabelRef = this.dayPlusBtnLabelRef;
			slotProps[2].minusBtnLabel = this.dayMinusBtnLabel;
			slotProps[2].minusBtnLabelRef = this.dayMinusBtnLabelRef;
		}
	});
});
