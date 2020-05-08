define([
	"dojo/_base/lang",
	"./_PickerChooser!DatePicker"
], function(lang, DatePicker){

	// module:
	//		dojox/mobile/DatePicker

	// TODO: need to list all the properties/methods in the interface provided by
	// SpinWheelDatePicker / ValuePickerDatePicker
		
	/*=====
	return function(){
		// summary:
		//		A wrapper widget around SpinWheelDatePicker or ValuePickerDatePicker.
		//		It should be used with the automatic theme loader, dojox/mobile/deviceTheme.
		//		Returns ValuePickerDatePicker when the current theme is "android" or "holodark".
		//		Returns SpinWheelDatePicker otherwise.
	};
	=====*/
	return lang.setObject("dojox.mobile.DatePicker", DatePicker);
});
