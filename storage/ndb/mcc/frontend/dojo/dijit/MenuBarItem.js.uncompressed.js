//>>built
require({cache:{
'url:dijit/templates/MenuBarItem.html':"<div class=\"dijitReset dijitInline dijitMenuItem dijitMenuItemLabel\" data-dojo-attach-point=\"focusNode\" role=\"menuitem\" tabIndex=\"-1\"\n\t\tdata-dojo-attach-event=\"onmouseenter:_onHover,onmouseleave:_onUnhover,ondijitclick:_onClick\">\n\t<span data-dojo-attach-point=\"containerNode\"></span>\n</div>\n"}});
define("dijit/MenuBarItem", [
	"dojo/_base/declare", // declare
	"./MenuItem",
	"dojo/text!./templates/MenuBarItem.html"
], function(declare, MenuItem, template){

/*=====
	var MenuItem = dijit.MenuItem;
=====*/

	// module:
	//		dijit/MenuBarItem
	// summary:
	//		Item in a MenuBar that's clickable, and doesn't spawn a submenu when pressed (or hovered)


	var _MenuBarItemMixin = declare("dijit._MenuBarItemMixin", null, {
		templateString: template,

		// Map widget attributes to DOMNode attributes.
		_setIconClassAttr: null	// cancel MenuItem setter because we don't have a place for an icon
	});

	var MenuBarItem = declare("dijit.MenuBarItem", [MenuItem, _MenuBarItemMixin], {
		// summary:
		//		Item in a MenuBar that's clickable, and doesn't spawn a submenu when pressed (or hovered)

	});
	MenuBarItem._MenuBarItemMixin = _MenuBarItemMixin;	// dojox.mobile is accessing this


	return MenuBarItem;
});
