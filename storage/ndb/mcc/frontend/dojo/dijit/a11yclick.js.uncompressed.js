define("dijit/a11yclick", [
	"dojo/_base/array", // array.forEach
	"dojo/_base/declare", // declare
	"dojo/has", // has("dom-addeventlistener")
	"dojo/keys", // keys.ENTER keys.SPACE
	"dojo/on"
], function(array, declare, has, keys, on){

	// module:
	//		dijit/a11yclick

	function marked(/*DOMNode*/ node){
		// Test if a node or its ancestor has been marked with the dojoClick property to indicate special processing
		do{
			if(node.dojoClick){ return true; }
		}while(node = node.parentNode);
	}

	function clickKey(/*Event*/ e){
		// Test if this keyboard event should be tracked as the start (if keyup) or end (if keydown) of a click event.
		// Only track for nodes marked to be tracked, and not for buttons or inputs since they handle keyboard click
		// natively.
		return (e.keyCode === keys.ENTER || e.keyCode === keys.SPACE) &&
			!e.ctrlKey && !e.shiftKey && !e.altKey && !e.metaKey && !/input|button/i.test(e.target.nodeName) &&
			marked(e.target);
	}

	var lastKeyDownNode;

	on(document, "keydown", function(e){
		//console.log(this.id + ": onkeydown, e.target = ", e.target, ", lastKeyDownNode was ", lastKeyDownNode, ", equality is ", (e.target === lastKeyDownNode));
		if(clickKey(e)){
			// needed on IE for when focus changes between keydown and keyup - otherwise dropdown menus do not work
			lastKeyDownNode = e.target;

			// Prevent viewport scrolling on space key in IE<9.
			// (Reproducible on test_Button.html on any of the first dijit/form/Button examples)
			e.preventDefault();
		}
	});

	on(document, "keyup", function(e){
		//console.log(this.id + ": onkeyup, e.target = ", e.target, ", lastKeyDownNode was ", lastKeyDownNode, ", equality is ", (e.target === lastKeyDownNode));
		if(clickKey(e) && e.target == lastKeyDownNode){	// === breaks greasemonkey
			//need reset here or have problems in FF when focus returns to trigger element after closing popup/alert
			lastKeyDownNode = null;

			on.emit(e.target, "click", {
				cancelable: true,
				bubbles: true
			});
		}
	});

	if(has("touch")){
		// touchstart-->touchend will automatically generate a click event, but there are problems
		// on iOS after focus has been programatically shifted (#14604, #14918), so setup a failsafe
		// if click doesn't fire naturally.

		var clickTimer;

		on(document, "touchend", function(e){
			var target = e.target;
			if(marked(target)){
				var naturalClickListener = on.once(target, "click", function(e){
					// If browser generates a click naturally, clear the timer to fire a synthetic click event
					if(clickTimer){
						clearTimeout(clickTimer);
						clickTimer = null;
					}
				});

				if(clickTimer){
					clearTimeout(clickTimer);
				}
				clickTimer = setTimeout(function(){
					clickTimer = null;
					naturalClickListener.remove();
					on.emit(target, "click", {
						cancelable: true,
						bubbles: true
					});
				}, 600);
			}
		});

		// TODO: if the touchstart and touchend were <100ms apart, and then there's another touchstart
		// event <300ms after the touchend event, then clear the synthetic click timer, because user
		// is doing a zoom.   Alternately monitor screen.deviceXDPI (or something similar) to see if
		// zoom level has changed.
	}

	return function(node, listener){
		// summary:
		//		Custom a11yclick (a.k.a. ondijitclick) event
		//		which triggers on a mouse click, touch, or space/enter keyup.

		node.dojoClick = true;

		return on(node, "click", listener);
	};
});
