define(["dojox/main", "dojo/_base/declare", "dojo/_base/lang", "dojo/dom-style", "dojo/_base/array", "dojo/sniff",
	"dojo/dom","dojo/dom-construct",
	"dojox/gfx", "dojox/gfx/_gfxBidiSupport", "../axis2d/common", "dojox/string/BidiEngine",
	"dojox/lang/functional","dojo/dom-attr","./_bidiutils"],
	function(dojox, declare, lang, domStyle, arr, has, dom, domConstruct, g, gBidi, da, BidiEngine, df, domAttr,utils){
	// module:
	//		dojox/charting/bidi/Chart							
	var bidiEngine = new BidiEngine();
	var dc = lang.getObject("charting", true, dojox);
	function validateTextDir(textDir){
		return /^(ltr|rtl|auto)$/.test(textDir) ? textDir : null;
	};
	
	return declare(null, {
		// textDir: String
		//		Bi-directional support,	the main variable which is responsible for the direction of the text.
		//		The text direction can be different than the GUI direction by using this parameter.
		//		Allowed values:
		//
		//		1. "ltr"
		//		2. "rtl"
		//		3. "auto" - contextual the direction of a text defined by first strong letter.
		//
		//		By default is as the page direction.
		textDir:"",
		
		// dir: String
		//		Mirroring support,	the main variable which is responsible for the direction of the chart.
		//
		//		Allowed values:
		//		1. "ltr"
		//		2. "rtl"
		//
		//		By default is ltr.
		dir: "",
		isMirrored: false,
		
		getTextDir: function(text){
			// summary:
			//		Return direction of the text. 
			// description:
			//		If textDir is ltr or rtl returns the value.
			//		If it's auto, calls to another function that responsible 
			//		for checking the value, and defining the direction.			
			// text:
			//		Used in case textDir is "auto", this case the direction is according to the first
			//		strong (directionally - which direction is strong defined) letter.
			// tags:
			//		protected.
			var textDir = this.textDir == "auto" ? bidiEngine.checkContextual(text) : this.textDir;
			// providing default value
			if(!textDir){
				textDir = domStyle.get(this.node, "direction");
			}
			return textDir;
		},

		postscript: function(node,args){
			// summary:
			//		Kicks off chart instantiation.
			// description:
			//		Used for setting the textDir of the chart. 
			// tags:
			//		private

			// validate textDir
			var textDir = args ? (args["textDir"] ? validateTextDir(args["textDir"]) : "") : "";
			// if textDir wasn't defined or was defined wrong, apply default value
			textDir = textDir ? textDir : domStyle.get(this.node, "direction");
			this.textDir = textDir;

			this.surface.textDir = textDir;
			
			// two data structures, used for storing data for further enablement to change
			// textDir dynamically
			this.htmlElementsRegistry = [];
			this.truncatedLabelsRegistry = [];
			// chart mirroring starts
			var chartDir = "ltr";
			if(domAttr.has(node, "direction")){
				chartDir = domAttr.get(node, "direction");
			}
			this.setDir(args ? (args.dir ? args.dir: chartDir) : chartDir);
			// chart mirroring ends
		},

		setTextDir: function(newTextDir, obj){
			// summary:
			//		Setter for the textDir attribute.
			// description:
			//		Allows dynamically set the textDir, goes over all the text-children and  
			//		updates their base text direction.
			// tags:
			//		public
		
			if(newTextDir == this.textDir){
				return this;
			}
			if(validateTextDir(newTextDir) != null){
				this.textDir = newTextDir;
				
				// set automatically all the gfx objects that were created by this surface
				// (groups, text objects)
				this.surface.setTextDir(newTextDir);
			
				// truncated labels that were created with gfx creator need to recalculate dir
				// for case like: "111111A" (A stands for bidi character) and the truncation
				// is "111..." If the textDir is auto, the display should be: "...111" but in gfx
				// case we will get "111...". Because this.surface.setTextDir will calculate the dir of truncated
				// label, which value is "111..." but th real is "111111A".
				// each time we created a gfx truncated label we stored it in the truncatedLabelsRegistry, so update now 
				// the registry.
				if(this.truncatedLabelsRegistry && newTextDir == "auto"){
					arr.forEach(this.truncatedLabelsRegistry, function(elem){
						var tDir = this.getTextDir(elem["label"]);
						if(elem["element"].textDir != tDir){
							elem["element"].setShape({textDir: tDir});
						}
					}, this);
				}
				
				// re-render axes with html labels. for recalculation of the labels
				// positions etc.
				// create array of keys for all the axis in chart 
				var axesKeyArr = df.keys(this.axes);
				if(axesKeyArr.length > 0){
					// iterate over the axes, and for each that have html labels render it.
					arr.forEach(axesKeyArr, function(key, index, arr){
						// get the axis 
						var axis = this.axes[key];
						// if the axis has html labels 
						if(axis.htmlElements[0]){
							axis.dirty = true;
							axis.render(this.dim, this.offsets);
						}
					},this);
					
					// recreate title
					if(this.title){
						this._renderTitle(this.dim, this.offsets);
					}			
				}else{
					// case of pies, spiders etc.
					arr.forEach(this.htmlElementsRegistry, function(elem, index, arr){
						var tDir = newTextDir == "auto" ? this.getTextDir(elem[4]) : newTextDir;
						if(elem[0].children[0] && elem[0].children[0].dir != tDir){
							domConstruct.destroy(elem[0].children[0]);
							elem[0].children[0] = da.createText["html"]
									(this, this.surface, elem[1], elem[2], elem[3], elem[4], elem[5], elem[6]).children[0];
						}
					},this);
				}
			}
			return this;
		},
		
		setDir : function(dir){
			// summary:
			//		Setter for the dir attribute.
			// description:
			//		Allows dynamically set the dri attribute, which will used to
			//		updates the chart rendering direction.
			//	dir : the desired chart direction [rtl: for right to left ,ltr: for left to right]
 
			if(dir == "rtl" || dir == "ltr"){
				if(this.dir != dir){
					this.isMirrored = true;
					this.dirty = true;
				}
				this.dir = dir;
			}			
			return this; 
		},
		
		isRightToLeft: function(){
			// summary:
			//		check the direction of the chart.
			// description:
			//		check the dir attribute to determine the rendering direction
			//		of the chart.
			return this.dir == "rtl";
        },
        
		applyMirroring: function(plot, dim, offsets){
			// summary:
			//		apply the mirroring operation to the current chart plots.
			//
			utils.reverseMatrix(plot, dim, offsets, this.dir == "rtl");
			//force the direction of the node to be ltr to properly render the axes and the plots labels.
			domStyle.set(this.node, "direction", "ltr");
			return this;
		},

		formatTruncatedLabel: function(element, label, labelType){
			this.truncateBidi(element, label, labelType);
		},

		truncateBidi: function(elem, label, labelType){
			// summary:
			//		Enables bidi support for truncated labels.
			// description:
			//		Can be two types of labels: html or gfx.
			//
			//		####gfx labels:
			//
			//		Need to be stored in registry to be used when the textDir will be set dynamically.
			//		Additional work on truncated labels is needed for case as 111111A (A stands for "bidi" character rtl directioned).
			//		let's say in this case the truncation is "111..." If the textDir is auto, the display should be: "...111" but in gfx
			//		case we will get "111...". Because this.surface.setTextDir will calculate the dir of truncated
			//		label, which value is "111..." but th real is "111111A".
			//		each time we created a gfx truncated label we store it in the truncatedLabelsRegistry.
			//
			//		####html labels:
			//
			//		no need for repository (stored in another place). Here we only need to update the current dir according to textDir.
			// tags:
			//		private
		
			if(labelType == "gfx"){
				// store truncated gfx labels in the data structure.
				this.truncatedLabelsRegistry.push({element: elem, label: label});
				if(this.textDir == "auto"){
					elem.setShape({textDir: this.getTextDir(label)});
				}
			}
			if(labelType == "html" && this.textDir == "auto"){
				elem.children[0].dir = this.getTextDir(label);
			}
		},
		
		render: function(){
			this.inherited(arguments);
			this.isMirrored = false;
			return this;
		},
		
		_resetLeftBottom: function(axis){
			if(axis.vertical && this.isMirrored){
				axis.opt.leftBottom = !axis.opt.leftBottom;
			}
		}		
	});
});

