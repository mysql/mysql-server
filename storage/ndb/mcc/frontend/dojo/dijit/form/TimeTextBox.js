define([
	"dojo/_base/declare", // declare
	"dojo/keys", // keys.DOWN_ARROW keys.ENTER keys.ESCAPE keys.TAB keys.UP_ARROW
	"dojo/query",
	"dojo/_base/lang", // lang.hitch
	"../_TimePicker",
	"./_DateTimeTextBox"
], function(declare, keys, query, lang, _TimePicker, _DateTimeTextBox){

	// module:
	//		dijit/form/TimeTextBox


	var TimeTextBox = declare("dijit.form.TimeTextBox", _DateTimeTextBox, {
		// summary:
		//		A validating, serializable, range-bound time text box with a drop down time picker

		baseClass: "dijitTextBox dijitComboBox dijitTimeTextBox",
		popupClass: _TimePicker,
		_selector: "time",

/*=====
		// constraints: TimeTextBox.__Constraints
		//		Despite the name, this parameter specifies both constraints on the input
		//		(including minimum/maximum allowed values) as well as
		//		formatting options.  See `dijit/form/TimeTextBox.__Constraints` for details.
		constraints:{},
=====*/

		// value: Date
		//		The value of this widget as a JavaScript Date object.  Note that the date portion implies time zone and daylight savings rules.
		//
		//		Example:
		// |	new dijit/form/TimeTextBox({value: stamp.fromISOString("T12:59:59", new Date())})
		//
		//		When passed to the parser in markup, must be specified according to locale-independent
		//		`stamp.fromISOString` format.
		//
		//		Example:
		// |	<input data-dojo-type='dijit/form/TimeTextBox' value='T12:34:00'>
		value: new Date(""),		// value.toString()="NaN"
		//FIXME: in markup, you have no control over daylight savings

		// Add scrollbars if necessary so that dropdown doesn't cover the <input>
		maxHeight: -1,

		openDropDown: function(/*Function*/ callback){
			this.inherited(arguments);

			// Fix #18683
			var selectedNode = query(".dijitTimePickerItemSelected", this.dropDown.domNode),
				parentNode=this.dropDown.domNode.parentNode;
			if(selectedNode[0]){
				// Center the selected node in the client area of the popup.
				parentNode.scrollTop=selectedNode[0].offsetTop-(parentNode.clientHeight-selectedNode[0].clientHeight)/2;
			}else{
				// There is no currently selected value. Position the list so that the median
				// node is visible.
				parentNode.scrollTop=(parentNode.scrollHeight-parentNode.clientHeight)/2;
            }

			// For screen readers, as user arrows through values, populate <input> with latest value.
			this.dropDown.on("input", lang.hitch(this, function(){
				this.set('value', this.dropDown.get("value"), false);
			}));
		},

		_onInput: function(){
			this.inherited(arguments);

			// set this.filterString to the filter to apply to the drop down list;
			// it will be used in openDropDown()
			var val = this.get('displayedValue');
			this.filterString = (val && !this.parse(val, this.constraints)) ? val.toLowerCase() : "";

			// close the drop down and reopen it, in order to filter the items shown in the list
			// and also since the drop down may need to be repositioned if the number of list items has changed
			// and it's being displayed above the <input>
			if(this._opened){
				this.closeDropDown();
			}
			this.openDropDown();
		}
	});

	/*=====
	 TimeTextBox.__Constraints = declare([_DateTimeTextBox.__Constraints, _TimePicker.__Constraints], {
		 // summary:
		 //		Specifies both the rules on valid/invalid values (first/last time allowed),
		 //		and also formatting options for how the time is displayed.
	 });
	 =====*/

	return TimeTextBox;
});
