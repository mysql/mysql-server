require({cache:{
'url:dijit/form/templates/VerticalSlider.html':"<table class=\"dijit dijitReset dijitSlider dijitSliderV\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\" rules=\"none\" data-dojo-attach-event=\"onkeydown:_onKeyDown,onkeyup:_onKeyUp\"\n\trole=\"presentation\"\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\"></td\n\t\t><td class=\"dijitReset dijitSliderButtonContainer dijitSliderButtonContainerV\"\n\t\t\t><div class=\"dijitSliderIncrementIconV\" style=\"display:none\" data-dojo-attach-point=\"decrementButton\"><span class=\"dijitSliderButtonInner\">+</span></div\n\t\t></td\n\t\t><td class=\"dijitReset\"></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\"></td\n\t\t><td class=\"dijitReset\"\n\t\t\t><center><div class=\"dijitSliderBar dijitSliderBumper dijitSliderBumperV dijitSliderTopBumper\" data-dojo-attach-event=\"press:_onClkIncBumper\"></div></center\n\t\t></td\n\t\t><td class=\"dijitReset\"></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td data-dojo-attach-point=\"leftDecoration\" class=\"dijitReset dijitSliderDecoration dijitSliderDecorationL dijitSliderDecorationV\"></td\n\t\t><td class=\"dijitReset dijitSliderDecorationC\" style=\"height:100%;\"\n\t\t\t><input data-dojo-attach-point=\"valueNode\" type=\"hidden\" ${!nameAttrSetting}\n\t\t\t/><center class=\"dijitReset dijitSliderBarContainerV\" role=\"presentation\" data-dojo-attach-point=\"sliderBarContainer\"\n\t\t\t\t><div role=\"presentation\" data-dojo-attach-point=\"remainingBar\" class=\"dijitSliderBar dijitSliderBarV dijitSliderRemainingBar dijitSliderRemainingBarV\" data-dojo-attach-event=\"press:_onBarClick\"><!--#5629--></div\n\t\t\t\t><div role=\"presentation\" data-dojo-attach-point=\"progressBar\" class=\"dijitSliderBar dijitSliderBarV dijitSliderProgressBar dijitSliderProgressBarV\" data-dojo-attach-event=\"press:_onBarClick\"\n\t\t\t\t\t><div class=\"dijitSliderMoveable dijitSliderMoveableV\" style=\"vertical-align:top;\"\n\t\t\t\t\t\t><div data-dojo-attach-point=\"sliderHandle,focusNode\" class=\"dijitSliderImageHandle dijitSliderImageHandleV\" data-dojo-attach-event=\"press:_onHandleClick\" role=\"slider\"></div\n\t\t\t\t\t></div\n\t\t\t\t></div\n\t\t\t></center\n\t\t></td\n\t\t><td data-dojo-attach-point=\"containerNode,rightDecoration\" class=\"dijitReset dijitSliderDecoration dijitSliderDecorationR dijitSliderDecorationV\"></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\"></td\n\t\t><td class=\"dijitReset\"\n\t\t\t><center><div class=\"dijitSliderBar dijitSliderBumper dijitSliderBumperV dijitSliderBottomBumper\" data-dojo-attach-event=\"press:_onClkDecBumper\"></div></center\n\t\t></td\n\t\t><td class=\"dijitReset\"></td\n\t></tr\n\t><tr class=\"dijitReset\"\n\t\t><td class=\"dijitReset\"></td\n\t\t><td class=\"dijitReset dijitSliderButtonContainer dijitSliderButtonContainerV\"\n\t\t\t><div class=\"dijitSliderDecrementIconV\" style=\"display:none\" data-dojo-attach-point=\"incrementButton\"><span class=\"dijitSliderButtonInner\">-</span></div\n\t\t></td\n\t\t><td class=\"dijitReset\"></td\n\t></tr\n></table>\n"}});
define("dijit/form/VerticalSlider", [
	"dojo/_base/declare", // declare
	"./HorizontalSlider",
	"dojo/text!./templates/VerticalSlider.html"
], function(declare, HorizontalSlider, template){

	// module:
	//		dijit/form/VerticalSlider

	return declare("dijit.form.VerticalSlider", HorizontalSlider, {
		// summary:
		//		A form widget that allows one to select a value with a vertically draggable handle

		templateString: template,
		_mousePixelCoord: "pageY",
		_pixelCount: "h",
		_startingPixelCoord: "y",
		_handleOffsetCoord: "top",
		_progressPixelSize: "height",

		// _descending: Boolean
		//		Specifies if the slider values go from high-on-top (true), or low-on-top (false)
		//		TODO: expose this in 1.2 - the css progress/remaining bar classes need to be reversed
		_descending: true,

		_isReversed: function(){
			// summary:
			//		Overrides HorizontalSlider._isReversed.
			//		Indicates if values are high on top (with low numbers on the bottom).
			return this._descending;
		}
	});
});
