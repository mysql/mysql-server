define([
	"dojo/_base/lang",
	"dojo/dom-attr",
	"dojo/dom-class",
        "dojo/dom-construct",
	"dijit/form/Button",
	"dijit/form/DropDownButton",
	"dijit/form/ComboButton",
	"dojo/i18n",
	"dojo/i18n!dijit/nls/loading",
	"dojo/_base/declare"
], function(lang, domAttr, domClass, domConstruct, Button, DropDownButton, ComboButton, i18n, nlsLoading, declare){
return declare("dojox.form._BusyButtonMixin", null, {

	// isBusy: Boolean
	isBusy: false,

	// isCancelled: Boolean 
	isCancelled: false,

	// busyLabel: String
	//		text while button is busy
	busyLabel: "",

	timeout: null, // timeout, should be controlled by xhr call

	// useIcon: Boolean
	//		use a busy icon
	useIcon: true,

	postMixInProperties: function(){
		this.inherited(arguments);
		if(!this.busyLabel){
			this.busyLabel = i18n.getLocalization("dijit", "loading", this.lang).loadingState;
		}
	},

	postCreate: function(){
		// summary:
		//		stores initial label and timeout for reference
		this.inherited(arguments);
		this._label = this.containerNode.innerHTML;
		this._initTimeout = this.timeout;

		// for initial busy buttons
		if(this.isBusy){
			this.makeBusy();
		}
	},

	makeBusy: function(){
		// summary:
		//		sets state from idle to busy
		this.isBusy = true;
		this.isCancelled = false;

		// Webkit does not submit the form if the submit button is disabled when
		// clicked ( https://bugs.webkit.org/show_bug.cgi?id=14443 ), so disable the button later
		if(this._disableHandle) {
			this._disableHandle.remove();
		}
		this._disableHandle = this.defer(function() {
			this.set("disabled", true);
		});

		this.setLabel(this.busyLabel, this.timeout);
	},

	cancel: function(){
		// summary:
		//		if no timeout is set or for other reason the user can put the button back
		//		to being idle
		this.isCancelled = true; 
		if(this._disableHandle) {
			this._disableHandle.remove();
		}
		this.set("disabled", false);
		this.isBusy = false;
		this.setLabel(this._label);
		if(this._timeout){	clearTimeout(this._timeout); }
		this.timeout = this._initTimeout;
	},

	resetTimeout: function(/*Int*/ timeout){
		// summary:
		//		to reset existing timeout and setting a new timeout
		if(this._timeout){
			clearTimeout(this._timeout);
		}

		// new timeout
		if(timeout){
			this._timeout = setTimeout(lang.hitch(this, function(){
				this.cancel();
			}), timeout);
		}else if(timeout == undefined || timeout === 0){
			this.cancel();
		}
	},

	setLabel: function(/*String*/ content, /*Int*/ timeout){
		// summary:
		//		setting a label and optional timeout of the labels state

		// this.inherited(arguments); FIXME: throws an Unknown runtime error

		// Begin IE hack
		this.label = content;
		// remove children
		while(this.containerNode.firstChild){
			this.containerNode.removeChild(this.containerNode.firstChild);
		}
		this.containerNode.appendChild(domConstruct.toDom(this.label));

		if(this.showLabel == false && !domAttr.get(this.domNode, "title")){
			this.titleNode.title=lang.trim(this.containerNode.innerText || this.containerNode.textContent || '');
		}
		// End IE hack

		// setting timeout
		if(timeout){
			this.resetTimeout(timeout);
		}else{
			this.timeout = null;
		}

		// create optional busy image
		if(this.useIcon && this.isBusy){
			var node = new Image();
			node.src = this._blankGif;
			domAttr.set(node, "id", this.id+"_icon");
			domClass.add(node, "dojoxBusyButtonIcon");
			this.containerNode.appendChild(node);
		}
	},

	_onClick: function(e){
		// summary:
		//		on button click the button state gets changed

		// only do something if button is not busy
		if(!this.isBusy){
			this.inherited(arguments);	// calls onClick()
			// If we still aren't cancelled due to click handler... 
			if (!this.isCancelled) {
				this.makeBusy();
			}
		}
	}
});
});
