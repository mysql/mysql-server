define("dojox/dtl/render/dom", [
	"dojo/_base/lang",
	"dojo/dom",
	"../Context",
	"../dom",
	"../_base"
], function(lang,dom,ddc,dddom,dd){

	var ddrd = lang.getObject("render.dom", true, dd);
	/*=====
	 ddrd = {
	 	// TODO: summary
	 };
	 =====*/

	ddrd.Render = function(/*DOMNode?*/ attachPoint, /*dojox/dtl/DomTemplate?*/ tpl){
		this._tpl = tpl;
		this.domNode = dom.byId(attachPoint);
	};
	lang.extend(ddrd.Render, {
		setAttachPoint: function(/*Node*/ node){
			this.domNode = node;
		},
		render: function(/*Object*/ context, /*dojox/dtl/DomTemplate?*/ tpl, /*dojox/dtl/DomBuffer?*/ buffer){
			if(!this.domNode){
				throw new Error("You cannot use the Render object without specifying where you want to render it");
			}

			this._tpl = tpl = tpl || this._tpl;
			buffer = buffer || tpl.getBuffer();
			context = context || new ddc();

			var frag = tpl.render(context, buffer).getParent();
			if(!frag){
				throw new Error("Rendered template does not have a root node");
			}

			if(this.domNode !== frag){
				if(this.domNode.parentNode){
					this.domNode.parentNode.replaceChild(frag, this.domNode);
				}
				this.domNode = frag;
			}
		}
	});

	return ddrd;
});
