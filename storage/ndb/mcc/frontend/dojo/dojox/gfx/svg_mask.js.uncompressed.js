define("dojox/gfx/svg_mask", [
	"dojo/_base/declare",
	"dojo/_base/lang",
	"./_base",
	"./shape",
	"./svg"
], function(declare, lang, gfx, gfxShape, svg){

	/*=====
	return {
		// summary:
		//		An svg-specific module that adds SVG mask support to the gfx api.
		//		You may require this module when your application specifically targets the SVG renderer.
	}
	=====*/

	lang.extend(svg.Shape, {
		mask: null,
		setMask: function(/*dojox.gfx.svg.Mask*/mask){
			// summary:
			//		Sets a mask object (SVG)
			// mask:
			//		The mask object

			var rawNode = this.rawNode;
			if(mask){
				rawNode.setAttribute("mask", "url(#"+mask.shape.id+")");
				this.mask = mask;
			}else{
				rawNode.removeAttribute("mask");
				this.mask = null;
			}

			return this;
		},
		getMask: function(){
			// summary:
			//		Returns the current mask object or null
			return this.mask;
		}
	});

	var Mask = svg.Mask = declare("dojox.gfx.svg.Mask", svg.Shape, {
		// summary:
		//		An SVG mask object
		// description:
		//		This object represents an SVG mask. Much like `dojox/gfx.Group`,
		//		a Mask's geometry is defined by its children.

		constructor: function(){
			gfxShape.Container._init.call(this);
			this.shape = Mask.defaultMask;
		},

		setRawNode: function(rawNode){
			this.rawNode = rawNode;
		},

		setShape: function(shape){
			if(!shape.id){
				shape = lang.mixin({ id: gfx._base._getUniqueId() }, shape);
			}
			this.inherited(arguments, [shape]);
		}
	});
	Mask.nodeType = 'mask';
	Mask.defaultMask = {
		// summary:
		//		Defines the default Mask prototype.

		// id: String
		//		The mask identifier. If none is provided, a generated id will be used.
		id: null,

		// x: Number
		//		The x coordinate of the top-left corner of the mask
		x: 0,

		// y: Number
		//		The y coordinate of the top-left corner of the mask
		y: 0,

		// width: Number
		//		The width of the mask. Defaults to 1 which is 100% of the bounding
		//		box width of the object applying the mask.
		width: 1,

		// height: Number
		//		The height of the mask. Defaults to 1 which is 100% of the bounding
		//		box height of the object applying the mask.
		height: 1,

		// maskUnits: String
		//		The coordinate system of the mask's `x`, `y`, `width`, and `height` properties.
		//		The default is "objectBoundingBox" where coordinates are fractions of the bounding box
		//		of the shape referencing the mask.
		maskUnits: "objectBoundingBox",

		// maskContentUnits: String
		//		The coordinate system for the mask's children. The default is "userSpaceOnUse"
		//		(i.e., the coordinate system of the shape referencing the mask).
		maskContentUnits: "userSpaceOnUse"
	};

	lang.extend(Mask, svg.Container);
	lang.extend(Mask, gfxShape.Creator);
	lang.extend(Mask, svg.Creator);

	var Surface = svg.Surface,
		surfaceAdd = Surface.prototype.add,
		surfaceRemove = Surface.prototype.remove;
	lang.extend(Surface, {
		createMask: function(mask){
			// summary:
			//		Creates a mask object
			// mask: Object
			//		A mask object (see dojox/gfx.svg.Mask.defaultMask)
			//
			// example:
			//		Define a mask where content coordinates are fractions of the bounding box
			//		of the object using the mask:
			//	|	var mask = surface.createMask({ maskContentUnits: "objectBoundingBox" });
			//	|	mask.createRect({ width: 1, height: 1 });
			//	|	mask.setFill({
			//	|		type: 'linear',
			//	|		x2: 1,
			//	|		y2: 0,
			//	|		colors: [
			//	|			{ offset: 0, color: '#111' },
			//	|			{ offset: 1, color: '#ddd' }
			//	|		]
			//	|	});
			//
			// example:
			//		A mask with dimensions in user coordinates of element referring to mask
			//	|	var mask = {
			//	|		maskUnits: 'userSpaceOnUse'
			//	|	};
			return this.createObject(Mask, mask);	// dojox.gfx.svg.Mask
		},
		add: function(shape){
			if(shape instanceof Mask){
				this.defNode.appendChild(shape.rawNode);
				shape.parent = this;
			}else{
				surfaceAdd.apply(this, arguments);
			}
			return this;
		},
		remove: function(shape, silently){
			if(shape instanceof Mask && this.defNode == shape.rawNode.parentNode){
				this.defNode.removeChild(shape.rawNode);
				shape.parent = null;
			}else{
				surfaceRemove.apply(this, arguments);
			}
			return this;
		}
	});
});
