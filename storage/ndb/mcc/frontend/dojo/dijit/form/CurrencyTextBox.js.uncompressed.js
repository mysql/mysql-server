//>>built
define("dijit/form/CurrencyTextBox", [
	"dojo/currency", // currency._mixInDefaults currency.format currency.parse currency.regexp
	"dojo/_base/declare", // declare
	"dojo/_base/lang", // lang.hitch
	"./NumberTextBox"
], function(currency, declare, lang, NumberTextBox){

/*=====
	var NumberTextBox = dijit.form.NumberTextBox;
=====*/

	// module:
	//		dijit/form/CurrencyTextBox
	// summary:
	//		A validating currency textbox


	/*=====
	declare(
		"dijit.form.CurrencyTextBox.__Constraints",
		[dijit.form.NumberTextBox.__Constraints, currency.__FormatOptions, currency.__ParseOptions], {
		// summary:
		//		Specifies both the rules on valid/invalid values (minimum, maximum,
		//		number of required decimal places), and also formatting options for
		//		displaying the value when the field is not focused (currency symbol,
		//		etc.)
		// description:
		//		Follows the pattern of `dijit.form.NumberTextBox.constraints`.
		//		In general developers won't need to set this parameter
		// example:
		//		To ensure that the user types in the cents (for example, 1.00 instead of just 1):
		//	|		{fractional:true}
	});
	=====*/

	return declare("dijit.form.CurrencyTextBox", NumberTextBox, {
		// summary:
		//		A validating currency textbox
		// description:
		//		CurrencyTextBox is similar to `dijit.form.NumberTextBox` but has a few
		//		extra features related to currency:
		//
		//		1. After specifying the currency type (american dollars, euros, etc.) it automatically
		//			sets parse/format options such as how many decimal places to show.
		//		2. The currency mark (dollar sign, euro mark, etc.) is displayed when the field is blurred
		//			but erased during editing, so that the user can just enter a plain number.

		// currency: [const] String
		//		the [ISO4217](http://en.wikipedia.org/wiki/ISO_4217) currency code, a three letter sequence like "USD"
		currency: "",

		/*=====
		// constraints: dijit.form.CurrencyTextBox.__Constraints
		//		Despite the name, this parameter specifies both constraints on the input
		//		(including minimum/maximum allowed values) as well as
		//		formatting options.  See `dijit.form.CurrencyTextBox.__Constraints` for details.
		constraints: {},
		======*/

		baseClass: "dijitTextBox dijitCurrencyTextBox",

		// Override regExpGen ValidationTextBox.regExpGen().... we use a reg-ex generating function rather
		// than a straight regexp to deal with locale  (plus formatting options too?)
		regExpGen: function(constraints){
			// if focused, accept either currency data or NumberTextBox format
			return '(' + (this.focused ? this.inherited(arguments, [ lang.mixin({}, constraints, this.editOptions) ]) + '|' : '')
				+ currency.regexp(constraints) + ')';
		},

		// Override NumberTextBox._formatter to deal with currencies, ex: converts "123.45" to "$123.45"
		_formatter: currency.format,

		_parser: currency.parse,

		parse: function(/*String*/ value, /*Object*/ constraints){
			// summary:
			// 		Parses string value as a Currency, according to the constraints object
			// tags:
			// 		protected extension
			var v = this.inherited(arguments);
			if(isNaN(v) && /\d+/.test(value)){ // currency parse failed, but it could be because they are using NumberTextBox format so try its parse
				v = lang.hitch(lang.mixin({}, this, { _parser: NumberTextBox.prototype._parser }), "inherited")(arguments);
			}
			return v;
		},

		_setConstraintsAttr: function(/*Object*/ constraints){
			if(!constraints.currency && this.currency){
				constraints.currency = this.currency;
			}
			this.inherited(arguments, [ currency._mixInDefaults(lang.mixin(constraints, { exponent: false })) ]); // get places
		}
	});
});
