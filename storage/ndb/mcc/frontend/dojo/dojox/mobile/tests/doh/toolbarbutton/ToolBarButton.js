dojo.addOnLoad(function(){
	doh.register("dojox.mobile.test.ToolBarButton", [
		function test_Heading_Verification(){
			var demoWidget = dijit.byId("btn1");

			doh.assertEqual('Edit', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_0");
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertTrue(demoWidget.iconNode.childNodes, "there is no iconNode.childNodes. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblDomButtonWhitePlus mblDomButton', demoWidget.iconNode.childNodes[0].className, "id= "+ demoWidget.domNode.id);
			
			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_1");
			doh.assertEqual('Edit', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_2");
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertTrue(demoWidget.iconNode.childNodes, "there is no iconNode.childNodes. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblDomButtonWhitePlus mblDomButton', demoWidget.iconNode.childNodes[0].className, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_3");
			doh.assertEqual('Speaker', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_4");
			doh.assertEqual('Done', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_5");
			doh.assertEqual('Update All', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_6");
			doh.assertEqual('mblToolBarButton mblToolBarButtonHasLeftArrow', demoWidget.domNode.className, "id= "+ demoWidget.domNode.id);
			doh.assertEqual('Bookmarks', demoWidget.labelNode.innerHTML);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_7");
			doh.assertEqual('Done', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_8");
			doh.assertEqual('Done', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_9");
			doh.assertEqual('New Folder', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_10");
			doh.assertEqual('New', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_11");
			doh.assertEqual('Toggle', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_12");
			if(!dojo.isIE){
				doh.assertTrue(demoWidget.iconNode.src.search(/a-icon-12.png/) != -1, "a-icon-12.png", "id= "+ demoWidget.domNode.id);
			}

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_13");
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertTrue(demoWidget.iconNode.childNodes, "there is no iconNode.childNodes. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblSpriteIcon', demoWidget.iconNode.childNodes[0].className, "id= "+ demoWidget.domNode.id);
			verifyRect(demoWidget.iconNode.childNodes[0], "29px", "29px", "58px", "0px");
			doh.assertEqual('-29px', demoWidget.iconNode.childNodes[0].style.top, "id= "+ demoWidget.domNode.id);
			doh.assertEqual('0px', demoWidget.iconNode.childNodes[0].style.left, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_14");
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertTrue(demoWidget.iconNode.childNodes, "there is no iconNode.childNodes. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblDomButtonWhitePlus mblDomButton', demoWidget.iconNode.childNodes[0].className, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_15");
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertTrue(demoWidget.iconNode.childNodes, "there is no iconNode.childNodes. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblDomButtonWhiteSearch mblDomButton', demoWidget.iconNode.childNodes[0].className, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_16");
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertTrue(demoWidget.iconNode.childNodes, "there is no iconNode.childNodes. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblDomButtonWhitePlus mblDomButton', demoWidget.iconNode.childNodes[0].className, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_17");
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblImageIcon', demoWidget.iconNode.className, "id= "+ demoWidget.domNode.id);
			if(!dojo.isIE){
				doh.assertTrue(demoWidget.iconNode.src.search(/tab-icon-15h.png/) != -1, "tab-icon-15h.png");
			}

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_18");
			doh.assertEqual('mblToolBarButton mblToolBarButtonHasLeftArrow', demoWidget.domNode.className, "id= "+ demoWidget.domNode.id);
			doh.assertEqual('Top', demoWidget.labelNode.innerHTML);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_19");
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertTrue(demoWidget.iconNode.childNodes, "there is no iconNode.childNodes. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblDomButtonWhiteSearch mblDomButton', demoWidget.iconNode.childNodes[0].className, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_20");
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertTrue(demoWidget.iconNode.childNodes, "there is no iconNode.childNodes. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblDomButtonWhiteUpArrow mblDomButton', demoWidget.iconNode.childNodes[0].className, "id= "+ demoWidget.domNode.id);

			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_21");
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertTrue(demoWidget.iconNode.childNodes, "there is no iconNode.childNodes. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblDomButtonWhiteDownArrow mblDomButton', demoWidget.iconNode.childNodes[0].className, "id= "+ demoWidget.domNode.id);
			
			// Test cases for #16771
			// a) 
			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_22");
			doh.assertTrue(dojo.style(demoWidget.bodyNode, "height") > 0, 
				"the height of button's body should be larger than 0! (left arrow) id= "+ 
				demoWidget.domNode.id + 
				" actual height: " + dojo.style(demoWidget.bodyNode, "height"));
			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_23");
			doh.assertTrue(dojo.style(demoWidget.bodyNode, "height") > 0, 
				"the height of button's body should be larger than 0! (right arrow) id= "+ 
				demoWidget.domNode.id + 
				" actual height: " + dojo.style(demoWidget.bodyNode, "height"));
				
			// b) With arrow, icon and label
			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_24");
			var bodyBox = dojo.getMarginBox(demoWidget.bodyNode);
			var arrowBox = dojo.getMarginBox(demoWidget.arrowNode);
			doh.assertTrue(bodyBox.l > arrowBox.l - (dojo.isFF ? 2 : 0), 
				"The body should not cover the arrow! (left arrow) id= "+ demoWidget.domNode.id + " bodyBox.l: " + bodyBox.l + " arrowBox.l: " + arrowBox.l);
			
			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_25");
			bodyBox = dojo.getMarginBox(demoWidget.bodyNode);
			arrowBox = dojo.getMarginBox(demoWidget.arrowNode);
			doh.assertTrue(arrowBox.l + arrowBox.w > bodyBox.l + bodyBox.w, 
				"The body should not cover the arrow! (right arrow) id= "+ demoWidget.domNode.id);
			// end of test cases for #16771
		},
		function test_Heading_Set(){
			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_4");
			demoWidget.set({label:"New Value"})
			doh.assertEqual('New Value', demoWidget.labelNode.innerHTML, "id= "+ demoWidget.domNode.id);
			
			demoWidget = dijit.byId("dojox_mobile_ToolBarButton_2");
			demoWidget.set({icon:"mblDomButtonBlueCirclePlus"})
			doh.assertTrue(demoWidget.iconNode, "there is no iconNode. id= "+ demoWidget.domNode.id);
			doh.assertTrue(demoWidget.iconNode.childNodes, "there is no iconNode.childNodes. id= "+ demoWidget.domNode.id);
			doh.assertEqual('mblDomButtonBlueCirclePlus mblDomButton', demoWidget.iconNode.childNodes[0].className, "id= "+ demoWidget.domNode.id);
		}
	]);
	doh.run();
});

