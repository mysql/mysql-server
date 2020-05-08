define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	var test_robot = has("trident") || has("ff") || has("chrome") < 45;

	// top level widget tests
	doh.register("Bidi", require.toUrl("./Bidi.html"), 999999);

	doh.register("Menu", require.toUrl("./Menu.html"), 999999);
	if(test_robot){
		doh.register("robot.Menu_mouse", require.toUrl("./robot/Menu_mouse.html"), 999999);
		doh.register("robot.Menu_a11y", require.toUrl("./robot/Menu_a11y.html"), 999999);
		doh.register("robot.Menu_iframe", require.toUrl("./robot/Menu_iframe.html"), 999999);
	}

	doh.register("Dialog", require.toUrl("./Dialog.html"), 999999);
	if(test_robot){
		doh.register("robot.Dialog_mouse", require.toUrl("./robot/Dialog_mouse.html"), 999999);
		doh.register("robot.Dialog_a11y", require.toUrl("./robot/Dialog_a11y.html"), 999999);
		doh.register("robot.Dialog_focusDestroy", require.toUrl("./robot/Dialog_focusDestroy.html"), 999999);
	}
	doh.register("ConfirmDialog", require.toUrl("./ConfirmDialog.html"), 999999);
	if(test_robot){
		doh.register("robot.ConfirmDialog_a11y", require.toUrl("./robot/ConfirmDialog_a11y.html"), 999999);
	}

	doh.register("ProgressBar", require.toUrl("./ProgressBar.html"), 999999);

	if(test_robot){
		doh.register("robot.Tooltip_a11y", require.toUrl("./robot/Tooltip_a11y.html"), 999999);
		doh.register("robot.Tooltip_mouse", require.toUrl("./robot/Tooltip_mouse.html"), 999999);
		doh.register("robot.Tooltip_mouse_quirks", require.toUrl("./robot/Tooltip_mouse_quirks.html"), 999999);
	}
	doh.register("Tooltip-placement", require.toUrl("./Tooltip-placement.html"), 999999);

	doh.register("TooltipDialog", require.toUrl("./TooltipDialog.html"), 999999);
	if(test_robot){
		doh.register("robot.TooltipDialog_mouse", require.toUrl("./robot/TooltipDialog_mouse.html"), 999999);
		doh.register("robot.TooltipDialog_a11y", require.toUrl("./robot/TooltipDialog_a11y.html"), 999999);
		doh.register("robot.ConfirmTooltipDialog_a11y", require.toUrl("./robot/ConfirmTooltipDialog_a11y.html"), 999999);
	}

	if(test_robot){
		doh.register("robot.InlineEditBox", require.toUrl("./robot/InlineEditBox.html"), 999999);
	}

	if(test_robot){
		doh.register("robot.ColorPalette", require.toUrl("./robot/ColorPalette.html"), 999999);
	}

	doh.register("CalendarLite", require.toUrl("./CalendarLite.html"), 999999);
	if(test_robot){
		doh.register("robot.Calendar_a11y", require.toUrl("./robot/Calendar_a11y.html"), 999999);
	}

	if(test_robot){
		doh.register("robot.TitlePane", require.toUrl("./robot/TitlePane.html"), 999999);
	}

	doh.register("Fieldset", require.toUrl("./Fieldset.html"), 999999);
	if(test_robot){
		doh.register("robot.Fieldset", require.toUrl("./robot/Fieldset.html"), 999999);
	}

	if(test_robot){
		doh.register("robot.Toolbar", require.toUrl("./robot/Toolbar.html"), 999999);
	}

	doh.register("_TimePicker", require.toUrl("./_TimePicker.html"), 999999);

	doh.register("InlineEditBox", require.toUrl("./InlineEditBox.html"), 999999);

});