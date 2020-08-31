define("dojox/app/controllers/Transition", ["require", "dojo/_base/lang", "dojo/_base/declare", "dojo/has", "dojo/on", "dojo/Deferred", "dojo/when",
	"dojo/dom-style", "../Controller", "../utils/constraints"],
	function(require, lang, declare, has, on, Deferred, when, domStyle, Controller, constraints){

	var transit;
	var MODULE = "app/controllers/Transition";
	var LOGKEY = "logTransitions:";

	// module:
	//		dojox/app/controllers/Transition
	//		Bind "app-transition" event on dojox/app application instance.
	//		Do transition from one view to another view.
	return declare("dojox.app.controllers.Transition", Controller, {

		proceeding: false,

		waitingQueue:[],

		constructor: function(app, events){
			// summary:
			//		bind "app-transition" event on application instance.
			//
			// app:
			//		dojox/app application instance.
			// events:
			//		{event : handler}
			this.events = {
				"app-transition": this.transition,
				"app-domNode": this.onDomNodeChange
			};
			require([this.app.transit || "dojox/css3/transit"], function(t){
				transit = t;
			});
			if(this.app.domNode){
				this.onDomNodeChange({oldNode: null, newNode: this.app.domNode});
			}
		},

		transition: function(event){
			// summary:
			//		Response to dojox/app "app-transition" event.
			//
			// example:
			//		Use emit to trigger "app-transition" event, and this function will response to the event. For example:
			//		|	this.app.emit("app-transition", {"viewId": viewId, "opts": opts});
			//
			// event: Object
			//		"app-transition" event parameter. It should be like: {"viewId": viewId, "opts": opts}
			var F = MODULE+":transition";
			this.app.log(LOGKEY,F," ");
			this.app.log(LOGKEY,F,"New Transition event.viewId=["+event.viewId+"]");
			this.app.log(F,"event.viewId=["+event.viewId+"]","event.opts=",event.opts);

			var viewsId = event.viewId || "";
			this.proceedingSaved = this.proceeding;	
			var parts = viewsId.split('+');
			var removePartsTest = viewsId.split('-');
			var viewId, newEvent;
			if(parts.length > 0 || removePartsTest.length > 0){
				while(parts.length > 1){ 	
					viewId = parts.shift();
					newEvent = lang.clone(event);
					if(viewId.indexOf("-") >= 0){ // there is a remove
						var removeParts = viewId.split('-');
						if(removeParts.length > 0){
							viewId = removeParts.shift();
							if(viewId){
								newEvent._removeView = false;
								newEvent.viewId = viewId;
								this.proceeding = true;
								this.proceedTransition(newEvent);
								newEvent = lang.clone(event);
							}
							viewId = removeParts.shift();
							if(viewId){
								newEvent._removeView = true;
								newEvent.viewId = viewId;
								this.proceeding = true;
								this.proceedTransition(newEvent);
							}
						}
					}else{
						newEvent._removeView = false;
						newEvent.viewId = viewId;
						this.proceeding = true;
						this.proceedTransition(newEvent);
					}
				}
				viewId = parts.shift();
				var removeParts = viewId.split('-');
				if(removeParts.length > 0){
					viewId = removeParts.shift();
				}
				if(viewId.length > 0){ // check viewId.length > 0 to skip this section for a transition with only -viewId
					this.proceeding = this.proceedingSaved;
					event.viewId = viewId;
					event._doResize = true; // at the end of the last transition call resize
					event._removeView = false;
					this.proceedTransition(event);
				}
				if(removeParts.length > 0){
					while(removeParts.length > 0){
						var remViewId = removeParts.shift();
						newEvent = lang.clone(event);
						newEvent.viewId = remViewId;
						newEvent._removeView = true;
						newEvent._doResize = true; // at the end of the last transition call resize
						this.proceedTransition(newEvent);
					}
				}
			}else{
				event._doResize = true; // at the end of the last transition call resize
				event._removeView = false;
				this.proceedTransition(event);
			}
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
			//		|		data: {}
			//		|	};
			//		|	new TransitionEvent(domNode, transOpts, e).dispatch();
			//
			// evt: Object
			//		transition options parameter

			// prevent event from bubbling to window and being
			// processed by dojox/mobile/ViewController
			if(evt.preventDefault){
				evt.preventDefault();
			}
			evt.cancelBubble = true;
			if(evt.stopPropagation){
				evt.stopPropagation();
			}

			var target = evt.detail.target;
			var regex = /#(.+)/;
			if(!target && regex.test(evt.detail.href)){
				target = evt.detail.href.match(regex)[1];
			}

			// transition to the target view
			this.transition({ "viewId":target, opts: lang.mixin({}, evt.detail), data: evt.detail.data });
		},

		_addTransitionEventToWaitingQueue: function(transitionEvt){
			if(transitionEvt.defaultView && this.waitingQueue.length > 0){ // need to test for defaultView to position this view correctly
				var addedEvt = false;
				for(var i = 0; i < this.waitingQueue.length; i++){
					var evt = this.waitingQueue[i];
					if(!evt.defaultView){
						this.waitingQueue.splice(i,0,transitionEvt); // insert before first non defaultView
						addedEvt = true;
						break;
					}
				}
				if(!addedEvt){
					this.waitingQueue.push(transitionEvt);
				}
			}else{
				this.waitingQueue.push(transitionEvt);
			}
		},
		proceedTransition: function(transitionEvt){
			// summary:
			//		Proceed transition queue by FIFO by default.
			//		If transition is in proceeding, add the next transition to waiting queue.
			//
			// transitionEvt: Object
			//		"app-transition" event parameter. It should be like: {"viewId":viewId, "opts":opts}
			var F = MODULE+":proceedTransition";

			if(this.proceeding){
				this._addTransitionEventToWaitingQueue(transitionEvt);
				this.app.log(F+" added this event to waitingQueue", transitionEvt);
				this.processingQueue = false;
				return;
			}
			// If there are events waiting, needed to have the last in be the last processed, so add it to waitingQueue
			// process the events in order.
			this.app.log(F+" this.waitingQueue.length ="+ this.waitingQueue.length+ " this.processingQueue="+this.processingQueue);
			if(this.waitingQueue.length > 0 && !this.processingQueue){
				this.processingQueue = true;
				this._addTransitionEventToWaitingQueue(transitionEvt);
				this.app.log(F+" added this event to waitingQueue passed proceeding", transitionEvt);
				transitionEvt = this.waitingQueue.shift();
				this.app.log(F+" shifted waitingQueue to process", transitionEvt);
			}
			
			this.proceeding = true;

			this.app.log(F+" calling trigger load", transitionEvt);
			if(!transitionEvt.opts){
				transitionEvt.opts = {};
			}
			var params = transitionEvt.params || transitionEvt.opts.params;
			this.app.emit("app-load", {
				"viewId": transitionEvt.viewId,
				"params": params,
				"forceTransitionNone": transitionEvt.forceTransitionNone,
				"callback": lang.hitch(this, function(needToHandleDefaultView, defaultHasPlus){
					if(needToHandleDefaultView){ // do not process this view if needToHandleDefaultView true
						this.proceeding = false;
						this.processingQueue = true;
						// use pop instead of shift here to get the last event for the defaultView when the default does not have a +
						// use shift when it has a + or the defaults will be out of order but it can move the default to be after other views if we are processing views with a +
						var nextEvt = (defaultHasPlus) ? this.waitingQueue.shift() : this.waitingQueue.pop();
						if(nextEvt){
							this.proceedTransition(nextEvt);
						}
					}else{
						var transitionDef = this._doTransition(transitionEvt.viewId, transitionEvt.opts, params, transitionEvt.opts.data, this.app, transitionEvt._removeView, transitionEvt._doResize, transitionEvt.forceTransitionNone);
						when(transitionDef, lang.hitch(this, function(){
							this.proceeding = false;
							this.processingQueue = true;
							var nextEvt = this.waitingQueue.shift();
							if(nextEvt){
								this.proceedTransition(nextEvt);
							}
						}));
					}
				})
			});
		},

		_getTransition: function(nextView, parent, transitionTo, opts, forceTransitionNone){
			// summary:
			//		Get view's transition type from the config for the view or from the parent view recursively.
			//		If not available use the transition option otherwise get view default transition type in the
			//		config from parent view.
			//
			// parent: Object
			//		view's parent
			// transitionTo: Object
			//		view to transition to
			//	opts: Object
			//		transition options
			// forceTransitionNone: boolean
			//		true if the transition type should be forced to none, used for the initial defaultView
			//
			// returns:
			//		transition type like "slide", "fade", "flip" or "none".
			if(forceTransitionNone){
				return "none";
			}
			var parentView = parent;
			var transition = null;
			if(nextView){
				transition = nextView.transition;
			}
			if(!transition && parentView.views[transitionTo]){
				transition = parentView.views[transitionTo].transition;
			} 
			if(!transition){
				transition = parentView.transition;
			}
			var defaultTransition = (nextView && nextView.defaultTransition) ?  nextView.defaultTransition : parentView.defaultTransition;
			while(!transition && parentView.parent){
				parentView = parentView.parent;
				transition = parentView.transition;
				if(!defaultTransition){
					defaultTransition = parentView.defaultTransition;
				}
			}
			return transition || opts.transition || defaultTransition || "none";
		},


		_getParamsForView: function(view, params){
			// summary:
			//		Get view's params only include view specific params if they are for this view.
			//
			// view: String
			//		the view's name
			// params: Object
			//		the params
			//
			// returns:
			//		params Object for this view
			var viewParams = {};
			for(var item in params){
				var value = params[item];
				if(lang.isObject(value)){	// view specific params
					if(item == view){		// it is for this view
						// need to add these params for the view
						viewParams = lang.mixin(viewParams, value);
					} 
				}else{	// these params are for all views, so add them
					if(item && value != null){
						viewParams[item] = params[item];
					}
				}
			}
			return viewParams;
		},

		_doTransition: function(transitionTo, opts, params, data, parent, removeView, doResize, forceTransitionNone, nested){
			// summary:
			//		Transitions from the currently visible view to the defined view.
			//		It should determine what would be the best transition unless
			//		an override in opts tells it to use a specific transitioning methodology
			//		the transitionTo is a string in the form of [view1,view2].
			//
			// transitionTo: Object
			//		transition to view id. It looks like #tabView,tab1
			// opts: Object
			//		transition options
			// params: Object
			//		params
			// data: Object
			//		data object that will be passed on activate & de-activate methods of the view
			// parent: Object
			//		view's parent
			// removeView: Boolean
			//		remove the view instead of transition to it
			// doResize: Boolean
			//		emit a resize event
			// forceTransitionNone: Boolean
			//		force the transition type to be none, used for the initial default view
			// nested: Boolean
			//		whether the method is called from the transitioning of a parent view
			//
			// returns:
			//		transit dojo/promise/all object.
			var F = MODULE+":_doTransition";

			if(!parent){
				throw Error("view parent not found in transition.");
			}

			this.app.log(F+" transitionTo=[",transitionTo,"], removeView=[",removeView,"] parent.name=[",parent.name,"], opts=",opts);

			var parts, toId, subIds, next;
			if(transitionTo){
				parts = transitionTo.split(",");
			}else{
				// If parent.defaultView is like "main,main", we also need to split it and set the value to toId and subIds.
				// Or cannot get the next view by "parent.children[parent.id + '_' + toId]"
				parts = parent.defaultView.split(",");
			}
			toId = parts.shift();
			subIds = parts.join(',');

			// next is loaded and ready for transition
			next = parent.children[parent.id + '_' + toId];
			if(!next){
				if(removeView){
					this.app.log(F+" called with removeView true, but that view is not available to remove");
					return;	// trying to remove a view which is not showing
				}
				throw Error("child view must be loaded before transition.");
			}
			// if no subIds and next has default view,
			// set the subIds to the default view and transition to default view.
			if(!subIds && next.defaultView){
				subIds = next.defaultView;
			}

			var nextSubViewArray = [next || parent];
			if(subIds){
				nextSubViewArray = this._getNextSubViewArray(subIds, next, parent);
			}

			var current = constraints.getSelectedChild(parent, next.constraint);
			var currentSubViewArray = this._getCurrentSubViewArray(parent, nextSubViewArray, removeView);

			var currentSubNames = this._getNamesFromArray(currentSubViewArray, false);
			var nextSubNames = this._getNamesFromArray(nextSubViewArray, true);

			// set params on next view.
			next.params = this._getParamsForView(next.name, params);

			if(removeView){
				if(next !== current){ // nothing to remove
					this.app.log(F+" called with removeView true, but that view is not available to remove");
					return;	// trying to remove a view which is not showing
				}	
				this.app.log(LOGKEY,F,"Transition Remove current From=["+currentSubNames+"]");
				// if next == current we will set next to null and remove the view with out a replacement
				next = null;
			}

			if(nextSubNames == currentSubNames && next == current){ // new test to see if current matches next
				this.app.log(LOGKEY,F,"Transition current and next DO MATCH From=["+currentSubNames+"] TO=["+nextSubNames+"]");
				this._handleMatchingViews(nextSubViewArray, next, current, parent, data, removeView, doResize, subIds, currentSubNames, toId, forceTransitionNone, opts);

			}else{
				this.app.log(LOGKEY,F,"Transition current and next DO NOT MATCH From=["+currentSubNames+"] TO=["+nextSubNames+"]");
				//When clicking fast, history module will cache the transition request que
				//and prevent the transition conflicts.
				//Originally when we conduct transition, selectedChild will not be the
				//view we want to start transition. For example, during transition 1 -> 2
				//if user click button to transition to 3 and then transition to 1. After
				//1->2 completes, it will perform transition 2 -> 3 and 2 -> 1 because
				//selectedChild is always point to 2 during 1 -> 2 transition and transition
				//will record 2->3 and 2->1 right after the button is clicked.

				//assume next is already loaded so that this.set(...) will not return
				//a promise object. this.set(...) will handles the this.selectedChild,
				//activate or deactivate views and refresh layout.

				//necessary, to avoid a flash when the layout sets display before resize
				if(!removeView && next){
					var nextLastSubChild = this.nextLastSubChildMatch || next;
					var startHiding = false; // only hide views which will transition in
					for(var i = nextSubViewArray.length-1; i >= 0; i--){
						var v = nextSubViewArray[i];
						if(startHiding || v.id == nextLastSubChild.id){
							startHiding = true;
							if(!v._needsResize && v.domNode){
								this.app.log(LOGKEY,F," setting domStyle visibility hidden for v.id=["+v.id+"], display=["+v.domNode.style.display+"], visibility=["+v.domNode.style.visibility+"]");
								this._setViewVisible(v, false);
							}
						}
					}
				}

				if(current && current._active){
					this._handleBeforeDeactivateCalls(currentSubViewArray, this.nextLastSubChildMatch || next, current, data, subIds);
				}
				if(next){
					this.app.log(F+" calling _handleBeforeActivateCalls next name=[",next.name,"], parent.name=[",next.parent.name,"]");
					this._handleBeforeActivateCalls(nextSubViewArray, this.currentLastSubChildMatch || current, data, subIds);
				}
				if(!removeView){
					var nextLastSubChild = this.nextLastSubChildMatch || next;
					var trans = this._getTransition(nextLastSubChild, parent, toId, opts, forceTransitionNone)
					this.app.log(F+" calling _handleLayoutAndResizeCalls trans="+trans);
					this._handleLayoutAndResizeCalls(nextSubViewArray, removeView, doResize, subIds, forceTransitionNone, trans);
				}else{
					// for removeView need to set visible before transition do it here
					for(var i = 0; i < nextSubViewArray.length; i++){
						var v = nextSubViewArray[i];
						this.app.log(LOGKEY,F,"setting visibility visible for v.id=["+v.id+"]");
						if(v.domNode){
							this.app.log(LOGKEY,F,"  setting domStyle for removeView visibility visible for v.id=["+v.id+"], display=["+v.domNode.style.display+"]");
							this._setViewVisible(v, true);
						}
					}
				}
				var result = true;

				// this.currentLastSubChildMatch holds the view to transition from
				if(transit && (!nested || this.currentLastSubChildMatch != null) && this.currentLastSubChildMatch !== next){
					// css3 transit has the check for IE so it will not try to do it on ie, so we do not need to check it here.
					// We skip in we are transitioning to a nested view from a parent view and that nested view
					// did not have any current
					result = this._handleTransit(next, parent, this.currentLastSubChildMatch, opts, toId, removeView, forceTransitionNone, doResize);
				}
				when(result, lang.hitch(this, function(){
					if(next){
						this.app.log(F+" back from transit for next ="+next.name);
					}
					if(removeView){
						var nextLastSubChild = this.nextLastSubChildMatch || next;
						var trans = this._getTransition(nextLastSubChild, parent, toId, opts, forceTransitionNone)
						this._handleLayoutAndResizeCalls(nextSubViewArray, removeView, doResize, subIds, forceTransitionNone, trans);
					}

					// Add call to handleAfterDeactivate and handleAfterActivate here!
					this._handleAfterDeactivateCalls(currentSubViewArray, this.nextLastSubChildMatch || next, current, data, subIds);
					this._handleAfterActivateCalls(nextSubViewArray, removeView, this.currentLastSubChildMatch || current, data, subIds);
				}));
				return result; // dojo/promise/all
			}
		},

		_handleMatchingViews: function(subs, next, current, parent, data, removeView, doResize, subIds, currentSubNames, toId, forceTransitionNone, opts){
			// summary:
			//		Called when the current views and the next views match
			var F = MODULE+":_handleMatchingViews";

			this._handleBeforeDeactivateCalls(subs, this.nextLastSubChildMatch || next, current, data, subIds);
			// this is the order that things were being done before on a reload of the same views, so I left it
			// calling _handleAfterDeactivateCalls here instead of after _handleLayoutAndResizeCalls
			this._handleAfterDeactivateCalls(subs, this.nextLastSubChildMatch || next, current, data, subIds);
			this._handleBeforeActivateCalls(subs, this.currentLastSubChildMatch || current, data, subIds);
			var nextLastSubChild = this.nextLastSubChildMatch || next;
			var trans = this._getTransition(nextLastSubChild, parent, toId, opts, forceTransitionNone)
			this._handleLayoutAndResizeCalls(subs, removeView, doResize, subIds, trans);
			this._handleAfterActivateCalls(subs, removeView, this.currentLastSubChildMatch || current, data, subIds);
		},

		_handleBeforeDeactivateCalls: function(subs, next, current, /*parent,*/ data, /*removeView, doResize,*/ subIds/*, currentSubNames*/){
			// summary:
			//		Call beforeDeactivate for each of the current views which are about to be deactivated
			var F = MODULE+":_handleBeforeDeactivateCalls";
			if(current._active){
				//now we need to loop backwards thru subs calling beforeDeactivate
				for(var i = subs.length-1; i >= 0; i--){
					var v = subs[i];
					if(v && v.beforeDeactivate && v._active){
						this.app.log(LOGKEY,F,"beforeDeactivate for v.id="+v.id);
						v.beforeDeactivate(next, data);
					}
				}
			}
		},

		_handleAfterDeactivateCalls: function(subs, next, current, data, subIds){
			// summary:
			//		Call afterDeactivate for each of the current views which have been deactivated
			var F = MODULE+":_handleAfterDeactivateCalls";
			if(current && current._active){
				//now we need to loop forwards thru subs calling afterDeactivate
				for(var i = 0; i < subs.length; i++){
					var v = subs[i];
					if(v && v.beforeDeactivate && v._active){
						this.app.log(LOGKEY,F,"afterDeactivate for v.id="+v.id);
						v.afterDeactivate(next, data);
						v._active = false;
					}
				}

			}
		},

		_handleBeforeActivateCalls: function(subs, current, data, subIds){
			// summary:
			//		Call beforeActivate for each of the next views about to be activated
			var F = MODULE+":_handleBeforeActivateCalls";
			//now we need to loop backwards thru subs calling beforeActivate (ok since next matches current)
			for(var i = subs.length-1; i >= 0; i--){
				var v = subs[i];
				this.app.log(LOGKEY,F,"beforeActivate for v.id="+v.id);
				v.beforeActivate(current, data);
			}
		},

		_handleLayoutAndResizeCalls: function(subs, removeView, doResize, subIds, forceTransitionNone, transition){
			// summary:
			//		fire app-layoutView for each of the next views about to be activated, and fire app-resize if doResize is true
			var F = MODULE+":_handleLayoutAndResizeCalls";
			var remove = removeView;
			for(var i = 0; i < subs.length; i++){
				var v = subs[i];
				this.app.log(LOGKEY,F,"emit layoutView v.id=["+v.id+"] removeView=["+remove+"]");
				// it seems like we should be able to minimize calls to resize by passing doResize: false and only doing resize on the app-resize emit
				this.app.emit("app-layoutView", {"parent": v.parent, "view": v, "removeView": remove, "doResize": false, "transition": transition, "currentLastSubChildMatch": this.currentLastSubChildMatch});
				remove = false;
			}
			if(doResize){
				this.app.log(LOGKEY,F,"emit doResize called");
				this.app.emit("app-resize"); // after last layoutView fire app-resize
				if(transition == "none"){
					this._showSelectedChildren(this.app); // Need to set visible too before transition do it now.
				}
			}

		},

		_showSelectedChildren: function(w){
			var F = MODULE+":_showSelectedChildren";
			this.app.log(LOGKEY,F," setting domStyle visibility visible for w.id=["+w.id+"], display=["+w.domNode.style.display+"], visibility=["+w.domNode.style.visibility+"]");
			this._setViewVisible(w, true);
			w._needsResize = false;
			for(var hash in w.selectedChildren){	// need this to handle all selectedChildren
				if(w.selectedChildren[hash] && w.selectedChildren[hash].domNode){
					this.app.log(LOGKEY,F," calling _showSelectedChildren for w.selectedChildren[hash].id="+w.selectedChildren[hash].id);
					this._showSelectedChildren(w.selectedChildren[hash]);
				}
			}
		},

		_setViewVisible: function(v, visible){
			if(visible){
				domStyle.set(v.domNode, "visibility", "visible");
			}else{
				domStyle.set(v.domNode, "visibility", "hidden");
			}
		},


		_handleAfterActivateCalls: function(subs, removeView, current, data, subIds){
			// summary:
			//		Call afterActivate for each of the next views which have been activated
			var F = MODULE+":_handleAfterActivateCalls";
			//now we need to loop backwards thru subs calling beforeActivate (ok since next matches current)
			var startInt = 0;
			if(removeView && subs.length > 1){
				startInt = 1;
			}
			for(var i = startInt; i < subs.length; i++){
				var v = subs[i];
				if(v.afterActivate){
					this.app.log(LOGKEY,F,"afterActivate for v.id="+v.id);
					v.afterActivate(current, data);
					v._active = true;
				}
			}
		},

		_getNextSubViewArray: function(subIds, next, parent){
			// summary:
			//		Get next sub view array, this array will hold the views which are about to be transitioned to
			//
			// subIds: String
			//		the subids, the views are separated with a comma
			// next: Object
			//		the next view to be transitioned to.
			// parent: Object
			//		the parent view used in place of next if next is not set.
			//
			// returns:
			//		Array of views which will be transitioned to during this transition
			var F = MODULE+":_getNextSubViewArray";
			var parts = [];
			var p = next || parent;
			if(subIds){
				parts = subIds.split(",");
			}
			var nextSubViewArray = [p];
			//now we need to loop forwards thru subIds calling beforeActivate
			for(var i = 0; i < parts.length; i++){
				toId = parts[i];
				var v = p.children[p.id + '_' + toId];
				if(v){
					nextSubViewArray.push(v);
					p = v;
				}
			}
			nextSubViewArray.reverse();
			return nextSubViewArray;
		},

		_getCurrentSubViewArray: function(parent, nextSubViewArray, removeView){
			// summary:
			//		Get current sub view array which will be replaced by the views in the nextSubViewArray
			//
			// parent: String
			//		the parent view whose selected children will be replaced
			// nextSubViewArray: Array
			//		the array of views which are to be transitioned to.
			//
			// returns:
			//		Array of views which will be deactivated during this transition
			var F = MODULE+":_getCurrentSubViewArray";
			var currentSubViewArray = [];
			var constraint, type, hash;
			var p = parent;
			this.currentLastSubChildMatch = null;
			this.nextLastSubChildMatch = null;

			for(var i = nextSubViewArray.length-1; i >= 0; i--){
				constraint = nextSubViewArray[i].constraint;
				type = typeof(constraint);
				hash = (type == "string" || type == "number") ? constraint : constraint.__hash;
				// if there is a selected child for this constraint, and the child matches this view, push it.
				if(p && p.selectedChildren && p.selectedChildren[hash]){
					if(p.selectedChildren[hash] == nextSubViewArray[i]){
						this.currentLastSubChildMatch = p.selectedChildren[hash];
						this.nextLastSubChildMatch = nextSubViewArray[i];
						currentSubViewArray.push(this.currentLastSubChildMatch);
						p = this.currentLastSubChildMatch;
					}else{
						this.currentLastSubChildMatch = p.selectedChildren[hash];
						currentSubViewArray.push(this.currentLastSubChildMatch);
						this.nextLastSubChildMatch = nextSubViewArray[i]; // setting this means the transition will be done to the child instead of the parent
						// since the constraint was set, but it did not match, need to deactivate all selected children of this.currentLastSubChildMatch
						if(!removeView){
							var selChildren = constraints.getAllSelectedChildren(this.currentLastSubChildMatch);
							currentSubViewArray = currentSubViewArray.concat(selChildren);
						}
						break;
					}
				}else{ // the else is for the constraint not matching which means no more to deactivate.
					this.currentLastSubChildMatch = null; // there was no view selected for this constraint
					this.nextLastSubChildMatch = nextSubViewArray[i]; // set this to the next view for transition to an empty constraint
					break;
				}

			}
			// Here since they had the constraint but it was not the same I need to deactivate all children of p
			if(removeView){
				var selChildren = constraints.getAllSelectedChildren(p);
				currentSubViewArray = currentSubViewArray.concat(selChildren);
			}

			return currentSubViewArray;
		},

		_getNamesFromArray: function(subViewArray, backward){
			// summary:
			//		Get names from the sub view names array
			//
			// subViewArray: Array
			//		the array of views to get the names from.
			// backward: boolean
			//		the direction to loop thru the array to get the names.
			//
			// returns:
			//		String of view names separated by a comma
			//
			var F = MODULE+":_getNamesFromArray";
			var subViewNames = "";
			if(backward){
				for (var i = subViewArray.length - 1; i >= 0; i--) {
					subViewNames = subViewNames ? subViewNames+","+subViewArray[i].name : subViewArray[i].name;
				}
			}else{
				for(var i = 0; i < subViewArray.length; i++){
					subViewNames = subViewNames ? subViewNames+","+subViewArray[i].name : subViewArray[i].name;
				}
			}
			return subViewNames;
		},

		_handleTransit: function(next, parent, currentLastSubChild, opts, toId, removeView, forceTransitionNone, resizeDone){
			// summary:
			//		Setup the options and call transit to do the transition
			//
			// next: Object
			//		the next view, the view which will be transitioned to
			// parent: Object
			//		the parent view which is used to get the transition type to be used
			// currentLastSubChild: Object
			//		the current view which is being transitioned away from
			// opts: Object
			//		the options used for the transition
			// toId: String
			//		the id of the view being transitioned to
			// removeView: boolean
			//		true if the view is being removed
			// forceTransitionNone: boolean
			//		true if the transition type should be forced to none, used for the initial defaultView
			// resizeDone: boolean
			//		true if resize was called before this transition
			//
			// returns:
			//		the promise returned by the call to transit
			var F = MODULE+":_handleTransit";

			var nextLastSubChild = this.nextLastSubChildMatch || next;

			var mergedOpts = lang.mixin({}, opts); // handle reverse from mergedOpts or transitionDir
			mergedOpts = lang.mixin({}, mergedOpts, {
				reverse: (mergedOpts.reverse || mergedOpts.transitionDir === -1)?true:false,
				// if transition is set for the view (or parent) in the config use it, otherwise use it from the event or defaultTransition from the config
				transition: this._getTransition(nextLastSubChild, parent, toId, mergedOpts, forceTransitionNone)
			});

			if(removeView){
				nextLastSubChild = null;
			}
			if(currentLastSubChild){
				this.app.log(LOGKEY,F,"transit FROM currentLastSubChild.id=["+currentLastSubChild.id+"]");
			}
			if(nextLastSubChild){
				if(mergedOpts.transition !== "none"){
					if(!resizeDone && nextLastSubChild._needsResize){ // need to resize if not done yet or things will not be positioned correctly
						this.app.log(LOGKEY,F,"emit doResize called from _handleTransit");
						this.app.emit("app-resize"); // after last layoutView fire app-resize
					}
					this.app.log(LOGKEY,F,"  calling _showSelectedChildren for w3.id=["+nextLastSubChild.id+"], display=["+nextLastSubChild.domNode.style.display+"], visibility=["+nextLastSubChild.domNode.style.visibility+"]");
					this._showSelectedChildren(this.app); // Need to set visible too before transition do it now.
				}
				this.app.log(LOGKEY,F,"transit TO nextLastSubChild.id=["+nextLastSubChild.id+"] transition=["+mergedOpts.transition+"]");
			}else{
				this._showSelectedChildren(this.app); // Need to set visible too before transition do it now.
			}
			return transit(currentLastSubChild && currentLastSubChild.domNode, nextLastSubChild && nextLastSubChild.domNode, mergedOpts);
		}

	});
});
