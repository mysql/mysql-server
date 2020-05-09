dojo.addOnLoad(function(){
	var CLASSNAMES1 = ["mblTabBarButton", "mblTabBarButtonSelected"];
	var CLASSNAMES2 = ["mblTabBarButton", "mblTabBarButtonHasIcon", "mblTabBarButtonSelected"];
	var geom = require("dojo/dom-geometry");
	var checkSpreadedChildren = function(/* String */ tabBarId, /* Number */ nbOfButtons, /* Number */ tabBarPaddingPlusBorderPlusMarginWidth){
		var tabBarPosition = geom.position(tabBarId);
		var tabBarButtonsPositions = [];
		for (var i=1; i <= nbOfButtons; i++){
			var node = dojo.byId(tabBarId + "-button" + i);
			tabBarButtonsPositions[i] = geom.position(node);
		}
		console.log(tabBarId);
		console.log(tabBarPosition);
		console.log(tabBarButtonsPositions);
		doh.assertEqual(Math.round(tabBarPosition.w - tabBarPaddingPlusBorderPlusMarginWidth), Math.round(tabBarButtonsPositions[1].w * nbOfButtons), tabBarId + " > button 1 size");
		for (var j=2; j <= nbOfButtons; j++){
			doh.assertEqual(Math.round(tabBarButtonsPositions[1].w), Math.round(tabBarButtonsPositions[j].w), tabBarId + " > button " + j + " size");
		}
	};
	doh.register("dojox.mobile.test.doh.TabBar", [
		{
			name: "TabBar and TabBarButton Verification",
			timeout: 4000,
			runTest: function(){
				var d = new doh.Deferred();
				setTimeout(d.getTestCallback(function(){
					var demoWidget = dijit.byId("dojox_mobile_TabBar_0");
					doh.assertTrue(dojo.hasClass(demoWidget.domNode, 'mblTabBar'),'mblTabBar ' + " id=" + demoWidget.id + " value=" + demoWidget.domNode.className);
					doh.assertTrue(dojo.hasClass(demoWidget.domNode, 'mblTabBarSegmentedControl'), 'mblTabBarSegmentedControl ' + " id=" + demoWidget.id + " value=" + demoWidget.domNode.className);

					verifyTabBarButton("dojox_mobile_TabBarButton_0", 'New', CLASSNAMES1, 'hidden', '', /tab-icon-16.png/i, /tab-icon-16h.png/);

					demoWidget = dijit.byId("dojox_mobile_TabBar_1");
					doh.assertTrue(dojo.hasClass(demoWidget.domNode, 'mblTabBar'),'mblTabBar ' + " id=" + demoWidget.id + " value=" + demoWidget.domNode.className);
					doh.assertTrue(dojo.hasClass(demoWidget.domNode, 'mblTabBarTabBar'), 'mblTabBarTabBar ' + " id=" + demoWidget.id + " value=" + demoWidget.domNode.className);

					verifyTabBarButton("dojox_mobile_TabBarButton_3", 'New', CLASSNAMES2, 'hidden', '', /tab-icon-16.png/i, /tab-icon-16h.png/);

					demoWidget = dijit.byId("dojox_mobile_TabBar_2");
					doh.assertTrue(dojo.hasClass(demoWidget.domNode, 'mblTabBar'),'mblTabBar ' + " id=" + demoWidget.id + " value=" + demoWidget.domNode.className);
					doh.assertTrue(dojo.hasClass(demoWidget.domNode, 'mblTabBarTabBar'), 'mblTabBarTabBar ' + " id=" + demoWidget.id + " value=" + demoWidget.domNode.className);

					verifyTabBarButton("dojox_mobile_TabBarButton_6", 'Featured', CLASSNAMES2, 'hidden', '', /tab-icons.png/i, /tab-icons.png/i, true);
					demoWidget = dijit.byId("dojox_mobile_TabBarButton_6");
					verifyRect(demoWidget.iconNode1.childNodes[0], "0px", "29px", "29px", "0px");
					verifyRect(demoWidget.iconNode2.childNodes[0], "29px", "29px", "58px", "0px");

					verifyTabBarButton("tbbBadge1", 'Foo', ["mblTabBarButton", "mblTabBarButtonHasIcon"], '', '', /tab-icon-16.png/i, /tab-icon-16h.png/i);
					verifyTabBarButton("tbbBadge2", 'Bar', ["mblTabBarButton", "mblTabBarButtonHasIcon"], '', '', /tab-icon-15.png/i, /tab-icon-15h.png/i);

				}),500);
				return d;
			}
		},
		{
			name: "TabBar and TabBarButton set",
			timeout: 4000,
			runTest: function(){
				var d = new doh.Deferred();
				setTimeout(d.getTestCallback(function(){
					var demoWidget = dijit.byId("dojox_mobile_TabBar_0");


					demoWidget = dijit.byId("dojox_mobile_TabBarButton_2");
					demoWidget.set({label:"New Value"});
//					demoWidget.select();
					demoWidget.set({selected:true});

					verifyTabBarButton("dojox_mobile_TabBarButton_2", 'New Value', CLASSNAMES1, 'hidden', '', /tab-icon-10.png/i, /tab-icon-10h.png/);
					demoWidget = dijit.byId("dojox_mobile_TabBarButton_5");
					demoWidget.set({label:"New Value", icon1:"../../images/tab-icon-11.png", icon2:"../../images/tab-icon-11h.png"});
//					demoWidget.select();
					demoWidget.set("selected",true);

					verifyTabBarButton("dojox_mobile_TabBarButton_5", 'New Value', CLASSNAMES2, 'hidden', '', /tab-icon-11.png/i, /tab-icon-11h.png/)

					demoWidget = dijit.byId("dojox_mobile_TabBarButton_4");
					demoWidget.set({icon1:null, icon2:null});
					doh.assertEqual(null, demoWidget.iconNode1, demoWidget.domNode.id);
					doh.assertEqual(null, demoWidget.iconNode2, demoWidget.domNode.id);

				}),500);
				return d;
			}
		},
		{
			name: "fill='always' on segmented control",
			timeout: 4000,
			runTest: function(){
				var d = new doh.Deferred();
				setTimeout(d.getTestCallback(function(){
					checkSpreadedChildren("spread-sc", 3, 12);
				}),500);
				return d;
			}
		},
		{
			name: "fill='always' on tab Bar",
			timeout: 4000,
			runTest: function(){
				var d = new doh.Deferred();
				setTimeout(d.getTestCallback(function(){
					checkSpreadedChildren("spread-tb", 3, 12);
				}),500);
				return d;
			}
		},
		{
			name: "fill='always' on tab Bar (CSS Sprite)",
			timeout: 4000,
			runTest: function(){
				var d = new doh.Deferred();
				setTimeout(d.getTestCallback(function(){
					checkSpreadedChildren("spread-tb-css", 5, 12);
				}),500);
				return d;
			}
		},
		{
			name: "fill='always' on slim tab",
			timeout: 4000,
			runTest: function(){
				var d = new doh.Deferred();
				setTimeout(d.getTestCallback(function(){
					checkSpreadedChildren("spread-st", 4, 2);
				}),500);
				return d;
			}
		},
		{
			name: "fill='always' on standard bar",
			timeout: 4000,
			runTest: function(){
				var d = new doh.Deferred();
				setTimeout(d.getTestCallback(function(){
					checkSpreadedChildren("spread-std", 4, 12);
				}),500);
				return d;
			}
		},
		{
			name: "fill='always' on flat bar",
			timeout: 4000,
			runTest: function(){
				var d = new doh.Deferred();
				setTimeout(d.getTestCallback(function(){
					checkSpreadedChildren("spread-flat", 4, 0);
				}),500);
				return d;
			}
		},
		{
			name: "fill='always' on tall bar",
			timeout: 4000,
			runTest: function(){
				var d = new doh.Deferred();
				setTimeout(d.getTestCallback(function(){
					checkSpreadedChildren("spread-tall", 4, 12);
				}),500);
				return d;
			}
		}
	]);
	doh.run();
});
