define("dojox/form/Rating", [
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/dom-attr",
	"dojo/dom-class",
	"dojo/mouse",
	"dojo/on",
	"dojo/string",
	"dojo/query",
	"dijit/form/_FormWidget"
], function(declare, lang, domAttr, domClass, mouse, on, string, query, FormWidget){


	return declare("dojox.form.Rating", FormWidget, {
		// summary:
		//		A widget for rating using stars.

		/*=====
		// required: Boolean
		//		TODO: Can be true or false, default is false.
		required: false,
		=====*/

		templateString: null,

		// numStars: Integer|Float
		//		The number of stars to show, default is 3.
		numStars: 3,

		// value: Integer|Float
		//		The current value of the Rating
		value: 0,

		buildRendering: function(/*Object*/ params){
			// summary:
			//		Build the templateString. The number of stars is given by this.numStars,
			//		which is normally an attribute to the widget node.

			var radioName = 'rating-' + Math.random().toString(36).substring(2);

			// The radio input used to display and select stars
			var starTpl = '<label class="dojoxRatingStar dijitInline ${hidden}">' +
				'<span class="dojoxRatingLabel">${value} stars</span>' +
			 	'<input type="radio" name="' + radioName + '" value="${value}" dojoAttachPoint="focusNode" class="dojoxRatingInput">' +
				'</label>';

			// The hidden value node is attached as "focusNode" because tabIndex, id, etc. are getting mapped there.
			var tpl = '<div dojoAttachPoint="domNode" class="dojoxRating dijitInline">' +
				'<div data-dojo-attach-point="list">' +
				string.substitute(starTpl, {value:0, hidden: 'dojoxRatingHidden'}) +
				'${stars}' +
				'</div></div>';

			var rendered = "";
			for(var i = 0; i < this.numStars; i++){
				rendered += string.substitute(starTpl, {value:i + 1, hidden: ''});
			}
			this.templateString = string.substitute(tpl, {stars:rendered});

			this.inherited(arguments);
		},

		postCreate: function(){
			this.inherited(arguments);
			this._renderStars(this.value);
			this.own(
				// Fire when mouse is moved over one of the stars.
				on(this.list, on.selector(".dojoxRatingStar", "mouseover"), lang.hitch(this, "_onMouse")),
				on(this.list, on.selector(".dojoxRatingStar", "click"), lang.hitch(this, "_onClick")),
				on(this.list, on.selector(".dojoxRatingInput", "change"), lang.hitch(this, "onStarChange")),
				on(this.list, mouse.leave, lang.hitch(this, function(){
					// go from hover display back to dormant display
					this._renderStars(this.value);
				}))
			);
		},

		_onMouse: function(evt){
			// summary:
			//		Called when mouse is moved over one of the stars
			var hoverValue = +domAttr.get(evt.target.querySelector('input'), "value");
			this._renderStars(hoverValue, true);
			this.onMouseOver(evt, hoverValue);
		},

		_onClick: function(evt) {
			if (evt.target.tagName === 'LABEL') {
				var clickedValue = +domAttr.get(evt.target.querySelector('input'), "value");
				// for backwards compatibility with previous dojo versions' onStarClick event
				evt.target.value = clickedValue;
				this.onStarClick(evt, clickedValue);

				// check for clicking current value
				if (clickedValue == this.value) {
					evt.preventDefault();
					this.onStarChange(evt);
				}
			}
		},

		_renderStars: function(value, hover){
			// summary:
			//		Render the stars depending on the value.
			query(".dojoxRatingStar", this.domNode).forEach(function(star, i){
				if(i > value){
					domClass.remove(star, "dojoxRatingStarHover");
					domClass.remove(star, "dojoxRatingStarChecked");
				}else{
					domClass.remove(star, "dojoxRatingStar" + (hover ? "Checked" : "Hover"));
					domClass.add(star, "dojoxRatingStar" + (hover ? "Hover" : "Checked"));
				}
			});
		},

		onStarChange: function(evt) {
			var newVal = +domAttr.get(evt.target, "value");
			this.setAttribute("value", newVal == this.value ? 0 : newVal);
			this._renderStars(this.value);

			this.onChange(this.value);
		},

		onStarClick: function(/*Event*/ evt, /*Number*/ value){
			// summary:
			//		Connect on this method to get noticed when the star value was clicked.
		},

		onMouseOver: function(/*=====evt, value=====*/ ){
			// summary:
			//		Connect here, the value is passed to this function as the second parameter!
		},

		setAttribute: function(/*String*/ key, /*Number*/ value){
			// summary:
			//		Deprecated.   Use set("value", ...) instead.
			this.set(key, value);
		},

		_setValueAttr: function(val){
			this._set("value", val);
			this._renderStars(val);
			var input = query("input[type=radio]", this.domNode)[val];
			if (input) {
				input.checked = true;
			}
			this.onChange(val);
		}
	});
});
