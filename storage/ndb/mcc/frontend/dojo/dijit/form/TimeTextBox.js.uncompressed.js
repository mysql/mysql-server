//>>built
define("dijit/form/TimeTextBox", [
	"dojo/_base/declare", // declare
	"dojo/keys", // keys.DOWN_ARROW keys.ENTER keys.ESCAPE keys.TAB keys.UP_ARROW
	"dojo/_base/lang", // lang.hitch
	"../_TimePicker",
	"./_DateTimeTextBox"
], function(declare, keys, lang, _TimePicker, _DateTimeTextBox){

/*=====
	var _TimePicker = dijit._TimePicker;
	var _DateTimeTextBox = dijit.form._DateTimeTextBox;
=====*/

	// module:
	//		dijit/form/TimeTextBox
	// summary:
	//		A validating, serializable, range-bound time text box with a drop down time picker


	/*=====
	declare(
		"dijit.form.TimeTextBox.__Constraints",
		[dijit.form._DateTimeTextBox.__Constraints, dijit._TimePicker.__Constraints]
	);
	=====*/

	return declare("dijit.form.TimeTextBox", _DateTimeTextBox, {
		// summary:
		//		A validating, serializable, range-bound time text box with a drop down time picker

		baseClass: "dijitTextBox dijitComboBox dijitTimeTextBox",
		popupClass: _TimePicker,
		_selector: "time",

/*=====
		// constraints: dijit.form.TimeTextBox.__Constraints
		constraints:{},
=====*/

		// value: Date
		//		The value of this widget as a JavaScript Date object.  Note that the date portion implies time zone and daylight savings rules.
		//
		//		Example:
		// |	new dijit.form.TimeTextBox({value: stamp.fromISOString("T12:59:59", new Date())})
		//
		//		When passed to the parser in markup, must be specified according to locale-independent
		//		`stamp.fromISOString` format.
		//
		//		Example:
		// |	<input data-dojo-type='dijit.form.TimeTextBox' value='T12:34:00'>
		value: new Date(""),		// value.toString()="NaN"
		//FIXME: in markup, you have no control over daylight savings

		_onKey: function(evt){
			if(this.disabled || this.readOnly){ return; }
			this.inherited(arguments);

			// If the user has backspaced or typed some numbers, then filter the result list
			// by what they typed.  Maybe there's a better way to detect this, like _handleOnChange()?
			switch(evt.keyCode){
				case keys.ENTER:
				case keys.TAB:
				case keys.ESCAPE:
				case keys.DOWN_ARROW:
				case keys.UP_ARROW:
					// these keys have special meaning
					break;
				default:
					// setTimeout() because the keystroke hasn't yet appeared in the <input>,
					// so the get('displayedValue') call below won't give the result we want.
					setTimeout(lang.hitch(this, function(){
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
					}), 0);
			}
		}
	});
});
