//>>built
define("dojox/av/widget/PlayButton", ['dojo', 'dijit', 'dijit/_Widget', 'dijit/_TemplatedMixin'],function(dojo, dijit){

dojo.declare("dojox.av.widget.PlayButton", [dijit._Widget, dijit._TemplatedMixin], {
	// summary:
	//		A Play/Pause button widget to use with dojox.av.widget.Player
	//
	templateString: dojo.cache("dojox.av.widget","resources/PlayButton.html"),
	//
	postCreate: function(){
		// summary:
		//		Intialize button.
		this.showPlay();
	},

	setMedia: function(/* Object */med){
		// summary:
		//		A common method to set the media in all Player widgets.
		//		May do connections and initializations.
		//
		this.media = med;
		dojo.connect(this.media, "onEnd", this, "showPlay");
		dojo.connect(this.media, "onStart", this, "showPause");
	},

	onClick: function(){
		// summary:
		//		Fired on play or pause click.
		//
		if(this._mode=="play"){
			this.onPlay();
		}else{
			this.onPause();
		}
	},

	onPlay: function(){
		// summary:
		//		Fired on play click.
		//
		if(this.media){
			this.media.play();
		}
		this.showPause();
	},
	onPause: function(){
		// summary:
		//		Fired on pause click.
		//
		if(this.media){
			this.media.pause();
		}
		this.showPlay();
	},
	showPlay: function(){
		// summary:
		//		Toggles the pause button invisible and the play
		//		button visible..
		//
		this._mode = "play";
		dojo.removeClass(this.domNode, "Pause");
		dojo.addClass(this.domNode, "Play");
	},
	showPause: function(){
		// summary:
		//		Toggles the play button invisible and the pause
		//		button visible.
		//
		this._mode = "pause";
		dojo.addClass(this.domNode, "Pause");
		dojo.removeClass(this.domNode, "Play");
	}
});
return dojox.av.widget.PlayButton;
});
