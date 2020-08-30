define("dojox/app/controllers/History", ["dojo/_base/lang", "dojo/_base/declare", "dojo/on", "../Controller", "../utils/hash", "dojo/topic"],
function(lang, declare, on, Controller, hash, topic){
	// module:
	//		dojox/app/controllers/History
	// summary:
	//		Bind "app-domNode" event on dojox/app application instance.
	//		Bind "startTransition" event on dojox/app application domNode.
	//		Bind "popstate" event on window object.
	//		Maintain history by HTML5 "pushState" method and "popstate" event.

	return declare("dojox.app.controllers.History", Controller, {
		// _currentPosition:     Integer
		//              Persistent variable which indicates the current position/index in the history
		//              (so as to be able to figure out whether the popState event was triggerd by
		//              a backward or forward action).
		_currentPosition: 0,

		// currentState: Object
		//              Current state
		currentState: {},

		constructor: function(){
			// summary:
			//		Bind "app-domNode" event on dojox/app application instance.
			//		Bind "startTransition" event on dojox/app application domNode.
			//		Bind "popstate" event on window object.
			//

			this.events = {
				"app-domNode": this.onDomNodeChange
			};
			if(this.app.domNode){
				this.onDomNodeChange({oldNode: null, newNode: this.app.domNode});
			}
			this.bind(window, "popstate", lang.hitch(this, this.onPopState));
		},

		onDomNodeChange: function(evt){
			if(evt.oldNode != null){
				this.unbind(evt.oldNode, "startTransition");
			}
			this.bind(evt.newNode, "startTransition", lang.hitch(this, this.onStartTransition));
		},

		onStartTransition: function(evt){
			// summary:
			//		Response to dojox/app "startTransition" event.
			//
			// example:
			//		Use "dojox/mobile/TransitionEvent" to trigger "startTransition" event, and this function will response the event. For example:
			//		|	var transOpts = {
			//		|		title:"List",
			//		|		target:"items,list",
			//		|		url: "#items,list",
			//		|		params: {"param1":"p1value"}
			//		|	};
			//		|	new TransitionEvent(domNode, transOpts, e).dispatch();
			//
			// evt: Object
			//		Transition options parameter
			var currentHash = window.location.hash;
			var currentView = hash.getTarget(currentHash, this.app.defaultView);
			var currentParams =  hash.getParams(currentHash);
			var _detail = lang.clone(evt.detail);
			_detail.target = _detail.title = currentView;
			_detail.url = currentHash;
			_detail.params = currentParams;
			_detail.id = this._currentPosition;

			// Create initial state if necessary
			if(history.length == 1){
				history.pushState(_detail, _detail.href, currentHash);
			}

			// Update the current state
			_detail.bwdTransition = _detail.transition;
			lang.mixin(this.currentState, _detail);
			history.replaceState(this.currentState, this.currentState.href, currentHash);

			// Create a new "current state" history entry
			this._currentPosition += 1;
			evt.detail.id = this._currentPosition;

			var newHash = evt.detail.url || "#" + evt.detail.target;

			if(evt.detail.params){
				newHash = hash.buildWithParams(newHash, evt.detail.params);
			}

			evt.detail.fwdTransition = evt.detail.transition;
			history.pushState(evt.detail, evt.detail.href, newHash);
			this.currentState = lang.clone(evt.detail);

			// Finally: Publish pushState topic
			topic.publish("/app/history/pushState", evt.detail.target);
		},

		onPopState: function(evt){
			// summary:
			//		Response to dojox/app "popstate" event.
			//
			// evt: Object
			//		Transition options parameter

			// Clean browser's cache and refresh the current page will trigger popState event,
			// but in this situation the application has not started and throws an error.
			// So we need to check application status, if application not STARTED, do nothing.
			if((this.app.getStatus() !== this.app.lifecycle.STARTED) || !evt.state ){
				return;
			}

			// Get direction of navigation and update _currentPosition accordingly
			var backward = evt.state.id < this._currentPosition;
			backward ? this._currentPosition -= 1 : this._currentPosition += 1;

			// Publish popState topic and transition to the target view. Important: Use correct transition.
			// Reverse transitionDir only if the user navigates backwards.
			var opts = lang.mixin({reverse: backward ? true : false}, evt.state);
			opts.transition = backward ? opts.bwdTransition : opts.fwdTransition;
			this.app.emit("app-transition", {
				viewId: evt.state.target,
				opts: opts
			});
			topic.publish("/app/history/popState", evt.state.target);
		}
	});
});
