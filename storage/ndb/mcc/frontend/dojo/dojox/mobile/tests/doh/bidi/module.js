define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){
	
	// Accordion
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./Accordion_Rtl.html"),999999);
	// Edge To Edge List
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./EdgeToEdgeCategory_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./EdgeToEdgeDataList_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./EdgeToEdgeStoreList_Rtl.html"),999999);
	// List Item
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./ListItem_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./ListItem2_Rtl.html"),999999);
	// RoundRect List
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./RoundRectDataList_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./RoundRectList_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./RoundRectStoreList_Rtl.html"),999999);
	// ComboBox
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./ComboBoxTests_Rtl.html"),999999);
	// Toggle Button
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./ToggleButtonTests_Rtl.html"),999999);
	// Switch
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./SwitchTests_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./Switch_Rtl.html"),999999); 
	// Heading
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./Heading_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./Heading2_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./HeadingTests_Rtl.html"),999999);
	// Fixed Splitter
	if(!(has("ie") < 10)){
		doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./FixedSplitterTests1_Rtl.html"),999999);
		doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./FixedSplitterTests2_Rtl.html"),999999);
		doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./FixedSplitterTests3_Rtl.html"),999999);
	}
	// TabBar
	if(!(has("ie") < 10)){
		doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./TabBarTests_Rtl.html"),999999);
	}
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./TabBar_Rtl.html"),999999);
	// ToolbarButton
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./ToolBarButton_rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./ToolBarButtonSetter_rtl.html"),999999);
	// Spin wheel
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./SpinWheel_Programmatic_Rtl.html"),999999);
	// Icon Menu
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./IconMenuTests_Rtl.html"),999999);
	// Icon Container
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./IconContainer_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./IconContainer2_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./IconContainer3_Rtl.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./IconContainerTests_Rtl.html"),999999);
	
	// Swap View
	if(!(has("ie") < 10)){
		doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./SwapViewTests1_Rtl.html"),999999);
	}
	// ValuePickerDatePicker
	doh.registerUrl("dojox.mobile.tests.doh.Bidi", require.toUrl("./DatePickerIso_Rtl.html"),999999);

});