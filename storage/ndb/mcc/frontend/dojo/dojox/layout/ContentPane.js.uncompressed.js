//>>built
define("dojox/layout/ContentPane", [
	"dojo/_base/lang",
	"dojo/_base/xhr",
	"dijit/layout/ContentPane",
	"dojox/html/_base",
	"dojo/_base/declare"
], function (lang, xhrUtil, ContentPane, htmlUtil, declare) {

/*===== var ContentPane = dijit.layout.ContentPane =====*/
return declare("dojox.layout.ContentPane", ContentPane, {
	// summary:
	//		An extended version of dijit.layout.ContentPane.
	//		Supports infile scripts and external ones declared by <script src=''
	//		relative path adjustments (content fetched from a different folder)
	//		<style> and <link rel='stylesheet' href='..'> tags,
	//		css paths inside cssText is adjusted (if you set adjustPaths = true)
	//
	//		NOTE that dojo.require in script in the fetched file isn't recommended
	//		Many widgets need to be required at page load to work properly

	// adjustPaths: Boolean
	//		Adjust relative paths in html string content to point to this page.
	//		Only useful if you grab content from a another folder then the current one
	adjustPaths: false,

	// cleanContent: Boolean
	//	summary:
	//		cleans content to make it less likely to generate DOM/JS errors.
	//	description:
	//		useful if you send ContentPane a complete page, instead of a html fragment
	//		scans for
	//
	//			* title Node, remove
	//			* DOCTYPE tag, remove
	cleanContent: false,

	// renderStyles: Boolean
	//		trigger/load styles in the content
	renderStyles: false,

	// executeScripts: Boolean
	//		Execute (eval) scripts that is found in the content
	executeScripts: true,

	// scriptHasHooks: Boolean
	//		replace keyword '_container_' in scripts with 'dijit.byId(this.id)'
	// NOTE this name might change in the near future
	scriptHasHooks: false,

	constructor: function(){
		// init per instance properties, initializer doesn't work here because how things is hooked up in dijit._Widget
		this.ioArgs = {};
		this.ioMethod = xhrUtil.get;
	},

	onExecError: function(e){
		// summary:
		//		event callback, called on script error or on java handler error
		//		overide and return your own html string if you want a some text
		//		displayed within the ContentPane
	},

	_setContent: function(cont){
		// override dijit.layout.ContentPane._setContent, to enable path adjustments
		
		var setter = this._contentSetter;
		if(! (setter && setter instanceof htmlUtil._ContentSetter)) {
			setter = this._contentSetter = new htmlUtil._ContentSetter({
				node: this.containerNode,
				_onError: lang.hitch(this, this._onError),
				onContentError: lang.hitch(this, function(e){
					// fires if a domfault occurs when we are appending this.errorMessage
					// like for instance if domNode is a UL and we try append a DIV
					var errMess = this.onContentError(e);
					try{
						this.containerNode.innerHTML = errMess;
					}catch(e){
						console.error('Fatal '+this.id+' could not change content due to '+e.message, e);
					}
				})/*,
				_onError */
			});
		};

		// stash the params for the contentSetter to allow inheritance to work for _setContent
		this._contentSetterParams = {
			adjustPaths: Boolean(this.adjustPaths && (this.href||this.referencePath)),
			referencePath: this.href || this.referencePath,
			renderStyles: this.renderStyles,
			executeScripts: this.executeScripts,
			scriptHasHooks: this.scriptHasHooks,
			scriptHookReplacement: "dijit.byId('"+this.id+"')"
		};

		this.inherited("_setContent", arguments);
	}
	// could put back _renderStyles by wrapping/aliasing dojox.html._ContentSetter.prototype._renderStyles
});
});