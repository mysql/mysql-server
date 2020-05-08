define([
	"dojo/_base/declare",
	"dojox/form/_RangeSliderMixin",
	"dojo/text!./resources/HorizontalRangeSlider.html",
	"dijit/form/HorizontalSlider"
], function(declare, RangeSliderMixin, template, baseSlider){

	return declare("dojox.form.HorizontalRangeSlider", [baseSlider, RangeSliderMixin], {
		// summary:
		//		A form widget that allows one to select a range with two horizontally draggable images
		templateString: template
	});
});
