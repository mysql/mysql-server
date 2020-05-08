define([
	"dojo/_base/declare",
	"dojo/dom-construct",
	"dojo/sniff",
	"dijit/_Contained",
	"dijit/_WidgetBase"
], function(declare, domConstruct, has, Contained, WidgetBase){
	// module:
	//		dojox/mobile/Audio

	return declare("dojox.mobile.Audio", [WidgetBase, Contained], {
		// summary:
		//		A thin wrapper around the HTML5 `<audio>` element.
		// description:
		//		dojox/mobile/Audio is a widget which plays audio. If all sources cannot 
		//		be played (typically, in desktop browsers that do not support `<audio>`), 
		//		dojox/mobile/Audio automatically replaces `<audio>` with `<embed>`, such 
		//		that the browser tries to play it with a suitable plug-in.
		
		// source: [const] Array
		//		An array of src and type,
		//		ex. [{src:"a.mp3", type:"audio/mpeg"}, {src:"a.ogg", type:"audio/ogg"}, ...].
		//		The src gives the path of the media resource. The type gives the
		//		type of the media resource.
		//		Note that changing the value of the property after the widget
		//		creation has no effect.
		source: null,

		// width: [const] String
		//		The width of the embed element. 
		//		Note that changing the value of the property after the widget
		//		creation has no effect.
		width: "200px",

		// height: [const] String
		//		The height of the embed element.
		//		Note that changing the value of the property after the widget
		//		creation has no effect.
		height: "15px",

		// _playable: [private] Boolean
		//		Internal flag.
		_playable: false,
		
		// _tag: [private] String
		//		The name of the tag ("audio").
		_tag: "audio",

		constructor: function(){
			// summary:
			//		Creates a new instance of the class.
			this.source = [];
		},

		buildRendering: function(){
			this.domNode = this.srcNodeRef || domConstruct.create(this._tag);
		},

		_getEmbedRegExp: function(){
			// tags:
			//		private
			return has('ff') ? /audio\/mpeg/i :
				   has('ie') ? /audio\/wav/i :
				   null;
		},

		startup: function(){
			if(this._started){ return; }
			this.inherited(arguments);
			var i, len, re;
		 	if(this.domNode.canPlayType){
				if(this.source.length > 0){
					for(i = 0, len = this.source.length; i < len; i++){
						domConstruct.create("source", {src:this.source[i].src, type:this.source[i].type}, this.domNode);
						this._playable = this._playable || !!this.domNode.canPlayType(this.source[i].type);
					}
				}else{
					for(i = 0, len = this.domNode.childNodes.length; i < len; i++){
						var n = this.domNode.childNodes[i];
						if(n.nodeType === 1 && n.nodeName === "SOURCE"){
							this.source.push({src:n.src, type:n.type});
							this._playable = this._playable || !!this.domNode.canPlayType(n.type);
						}
					}
				}
			}
			has.add("mobile-embed-audio-video-support", true);	//It should move to staticHasFeatures
		 	if(has("mobile-embed-audio-video-support")){
				if(!this._playable){
					for(i = 0, len = this.source.length, re = this._getEmbedRegExp(); i < len; i++){
					 	if(this.source[i].type.match(re)){
							var node = domConstruct.create("embed", {
								src: this.source[0].src,
								type: this.source[0].type,
								width: this.width,
								height: this.height
							});
							this.domNode.parentNode.replaceChild(node, this.domNode);
							this.domNode = node;
							this._playable = true;
							break;
						}
					}
				}
			}
		}

	});
});
