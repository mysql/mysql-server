define("dojox/form/RangeSlider", [
	"dojo/_base/kernel", // kernel.deprecated
	"./_RangeSliderMixin",
	"./HorizontalRangeSlider",
	"./VerticalRangeSlider"
], function(kernel, RangeSliderMixin){

	// module:
	//		dojox/form/RangeSlider

	kernel.deprecated("Call require() for HorizontalRangeSlider / VerticalRangeSlider, explicitly rather than 'dojox.form.RangeSlider' itself", "", "2.0");

	/*=====
	 return {
		 // summary:
		 //		Rollup of all the the Slider related widgets
		 //		For back-compat, remove for 2.0
	 };
	=====*/
	return RangeSliderMixin; // for backward compatibility
});
