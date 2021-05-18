require({cache:{
'dojox/main':function(){
define(["dojo/_base/kernel"], function(dojo) {
	// module:
	//		dojox/main

	/*=====
	return {
		// summary:
		//		The dojox package main module; dojox package is somewhat unusual in that the main module currently just provides an empty object.
		//		Apps should require modules from the dojox packages directly, rather than loading this module.
	};
	=====*/

	return dojo.dojox;
});
},
'dojox/grid/_Grid':function(){
define([
	"dojo/_base/kernel",
	"../main",
	"dojo/_base/declare",
	"./_Events",
	"./_Scroller",
	"./_Layout",
	"./_View",
	"./_ViewManager",
	"./_RowManager",
	"./_FocusManager",
	"./_EditManager",
	"./Selection",
	"./_RowSelector",
	"./util",
	"dijit/_Widget",
	"dijit/_TemplatedMixin",
	"dijit/CheckedMenuItem",
	"dojo/text!./resources/_Grid.html",
	"dojo/string",
	"dojo/_base/array",
	"dojo/_base/lang",
	"dojo/_base/sniff",
	"dojox/html/metrics",
	"dojo/_base/html",
	"dojo/query",
	"dojo/dnd/common",
	"dojo/i18n!dijit/nls/loading"
], function(dojo, dojox, declare, _Events, _Scroller, _Layout, _View, _ViewManager,
	_RowManager, _FocusManager, _EditManager, Selection, _RowSelector, util, _Widget,
	 _TemplatedMixin, CheckedMenuItem, template, string, array, lang, has, metrics, html, query){

	// NOTE: this is for backwards compatibility with Dojo 1.3
	if(!dojo.isCopyKey){
		dojo.isCopyKey = dojo.dnd.getCopyKeyState;
	}
	/*=====
	dojox.grid.__CellDef = {
		// name: String?
		//		The text to use in the header of the grid for this cell.
		// get: Function?
		//		function(rowIndex){} rowIndex is of type Integer.  This
		//		function will be called when a cell	requests data.  Returns the
		//		unformatted data for the cell.
		// value: String?
		//		If "get" is not specified, this is used as the data for the cell.
		// defaultValue: String?
		//		If "get" and "value" aren't specified or if "get" returns an undefined
		//		value, this is used as the data for the cell.  "formatter" is not run
		//		on this if "get" returns an undefined value.
		// formatter: Function?
		//		function(data, rowIndex){} data is of type anything, rowIndex
		//		is of type Integer.  This function will be called after the cell
		//		has its data but before it passes it back to the grid to render.
		//		Returns the formatted version of the cell's data.
		// type: dojox.grid.cells._Base|Function?
		//		TODO
		// editable: Boolean?
		//		Whether this cell should be editable or not.
		// hidden: Boolean?
		//		If true, the cell will not be displayed.
		// noresize: Boolean?
		//		If true, the cell will not be able to be resized.
		// width: Integer|String?
		//		A CSS size.  If it's an Integer, the width will be in em's.
		// colSpan: Integer?
		//		How many columns to span this cell.  Will not work in the first
		//		sub-row of cells.
		// rowSpan: Integer?
		//		How many sub-rows to span this cell.
		// styles: String?
		//		A string of styles to apply to both the header cell and main
		//		grid cells.  Must end in a ';'.
		// headerStyles: String?
		//		A string of styles to apply to just the header cell.  Must end
		//		in a ';'
		// cellStyles: String?
		//		A string of styles to apply to just the main grid cells.  Must
		//		end in a ';'
		// classes: String?
		//		A space separated list of classes to apply to both the header
		//		cell and the main grid cells.
		// headerClasses: String?
		//		A space separated list of classes to apply to just the header
		//		cell.
		// cellClasses: String?
		//		A space separated list of classes to apply to just the main
		//		grid cells.
		// attrs: String?
		//		A space separated string of attribute='value' pairs to add to
		//		the header cell element and main grid cell elements.
	};
	=====*/

	/*=====
	dojox.grid.__ViewDef = {
		// noscroll: Boolean?
		//		If true, no scrollbars will be rendered without scrollbars.
		// width: Integer|String?
		//		A CSS size.  If it's an Integer, the width will be in em's. If
		//		"noscroll" is true, this value is ignored.
		// cells: dojox.grid.__CellDef[]|Array[dojox.grid.__CellDef[]]?
		//		The structure of the cells within this grid.
		// type: String?
		//		A string containing the constructor of a subclass of
		//		dojox.grid._View.  If this is not specified, dojox.grid._View
		//		is used.
		// defaultCell: dojox.grid.__CellDef?
		//		A cell definition with default values for all cells in this view.  If
		//		a property is defined in a cell definition in the "cells" array and
		//		this property, the cell definition's property will override this
		//		property's property.
		// onBeforeRow: Function?
		//		function(rowIndex, cells){} rowIndex is of type Integer, cells
		//		is of type Array[dojox.grid.__CellDef[]].  This function is called
		//		before each row of data is rendered.  Before the header is
		//		rendered, rowIndex will be -1.  "cells" is a reference to the
		//		internal structure of this view's cells so any changes you make to
		//		it will persist between calls.
		// onAfterRow: Function?
		//		function(rowIndex, cells, rowNode){} rowIndex is of type Integer, cells
		//		is of type Array[dojox.grid.__CellDef[]], rowNode is of type DOMNode.
		//		This function is called	after each row of data is rendered.  After the
		//		header is rendered, rowIndex will be -1.  "cells" is a reference to the
		//		internal structure of this view's cells so any changes you make to
		//		it will persist between calls.
	};
	=====*/

	var _Grid = declare('dojox.grid._Grid',
		[ _Widget, _TemplatedMixin, _Events ],
		{
		// summary:
		//		A grid widget with virtual scrolling, cell editing, complex rows,
		//		sorting, fixed columns, sizeable columns, etc.
		//
		// description:
		//		_Grid provides the full set of grid features without any
		//		direct connection to a data store.
		//
		//		The grid exposes a get function for the grid, or optionally
		//		individual columns, to populate cell contents.
		//
		//		The grid is rendered based on its structure, an object describing
		//		column and cell layout.
		//
		// example:
		//		A quick sample:
		//
		//		define a get function
		//	|	function get(inRowIndex){ // called in cell context
		//	|		return [this.index, inRowIndex].join(', ');
		//	|	}
		//
		//		define the grid structure:
		//	|	var structure = [ // array of view objects
		//	|		{ cells: [// array of rows, a row is an array of cells
		//	|			[
		//	|				{ name: "Alpha", width: 6 },
		//	|				{ name: "Beta" },
		//	|				{ name: "Gamma", get: get }]
		//	|		]}
		//	|	];
		//
		//	|	<div id="grid"
		//	|		rowCount="100" get="get"
		//	|		structure="structure"
		//	|		dojoType="dojox.grid._Grid"></div>

		templateString: template,

		// classTag: String
		//		CSS class applied to the grid's domNode
		classTag: 'dojoxGrid',

		// settings
		// rowCount: Integer
		//		Number of rows to display.
		rowCount: 5,

		// keepRows: Integer
		//		Number of rows to keep in the rendering cache.
		keepRows: 75,

		// rowsPerPage: Integer
		//		Number of rows to render at a time.
		rowsPerPage: 25,

		// autoWidth: Boolean
		//		If autoWidth is true, grid width is automatically set to fit the data.
		autoWidth: false,
		
		// initialWidth: String
		//		A css string to use to set our initial width (only used if autoWidth
		//		is true).  The first rendering of the grid will be this width, any
		//		resizing of columns, etc will result in the grid switching to
		//		autoWidth mode.  Note, this width will override any styling in a
		//		stylesheet or directly on the node.
		initialWidth: "",

		// autoHeight: Boolean|Integer
		//		If autoHeight is true, grid height is automatically set to fit the data.
		//		If it is an integer, the height will be automatically set to fit the data
		//		if there are fewer than that many rows - and the height will be set to show
		//		that many rows if there are more
		autoHeight: '',

		// rowHeight: Integer
		//		If rowHeight is set to a positive number, it will define the height of the rows
		//		in pixels. This can provide a significant performance advantage, since it
		//		eliminates the need to measure row sizes during rendering, which is one
		//		the primary bottlenecks in the DataGrid's performance.
		rowHeight: 0,
		
		// autoRender: Boolean
		//		If autoRender is true, grid will render itself after initialization.
		autoRender: true,

		// defaultHeight: String
		//		default height of the grid, measured in any valid css unit.
		defaultHeight: '15em',
		
		// height: String
		//		explicit height of the grid, measured in any valid css unit.  This will be populated (and overridden)
		//		if the height: css attribute exists on the source node.
		height: '',

		// structure: dojox.grid.__ViewDef|dojox.grid.__ViewDef[]|dojox.grid.__CellDef[]|Array[dojox.grid.__CellDef[]]
		//		View layout definition.
		structure: null,

		// elasticView: Integer
		//	Override defaults and make the indexed grid view elastic, thus filling available horizontal space.
		elasticView: -1,

		// singleClickEdit: boolean
		//		Single-click starts editing. Default is double-click
		singleClickEdit: false,

		// selectionMode: String
		//		Set the selection mode of grid's Selection.  Value must be 'single', 'multiple',
		//		or 'extended'.  Default is 'extended'.
		selectionMode: 'extended',

		// rowSelector: Boolean|String
		//		If set to true, will add a row selector view to this grid.  If set to a CSS width, will add
		//		a row selector of that width to this grid.
		rowSelector: '',

		// columnReordering: Boolean
		//		If set to true, will add drag and drop reordering to views with one row of columns.
		columnReordering: false,

		// headerMenu: dijit.Menu
		//		If set to a dijit.Menu, will use this as a context menu for the grid headers.
		headerMenu: null,

		// placeholderLabel: String
		//		Label of placeholders to search for in the header menu to replace with column toggling
		//		menu items.
		placeholderLabel: "GridColumns",
		
		// selectable: Boolean
		//		Set to true if you want to be able to select the text within the grid.
		selectable: false,
		
		// Used to store the last two clicks, to ensure double-clicking occurs based on the intended row
		_click: null,
		
		// loadingMessage: String
		//		Message that shows while the grid is loading
		loadingMessage: "<span class='dojoxGridLoading'>${loadingState}</span>",

		// errorMessage: String
		//		Message that shows when the grid encounters an error loading
		errorMessage: "<span class='dojoxGridError'>${errorState}</span>",

		// noDataMessage: String
		//		Message that shows if the grid has no data - wrap it in a
		//		span with class 'dojoxGridNoData' if you want it to be
		//		styled similar to the loading and error messages
		noDataMessage: "",
		
		// escapeHTMLInData: Boolean
		//		This will escape HTML brackets from the data to prevent HTML from
		//		user-inputted data being rendered with may contain JavaScript and result in
		//		XSS attacks. This is true by default, and it is recommended that it remain
		//		true. Setting this to false will allow data to be displayed in the grid without
		//		filtering, and should be only used if it is known that the data won't contain
		//		malicious scripts. If HTML is needed in grid cells, it is recommended that
		//		you use the formatter function to generate the HTML (the output of
		//		formatter functions is not filtered, even with escapeHTMLInData set to true).
		escapeHTMLInData: true,
		
		// formatterScope: Object
		//		An object to execute format functions within.  If not set, the
		//		format functions will execute within the scope of the cell that
		//		has a format function.
		formatterScope: null,
		
		// editable: boolean
		//		indicates if the grid contains editable cells, default is false
		//		set to true if editable cell encountered during rendering
		editable: false,

		// summary: String
		//		Customizable summary descriptions which will be added to grid.domNode
		summary: '',
		_setSummaryAttr: 'domNode',
		
		// sortInfo: [private] Number
		sortInfo: 0,

		// _placeholders: [private] Array
		_placeholders: null,

		// _layoutClass: Object
		//	The class to use for our layout - can be overridden by grid subclasses
		_layoutClass: _Layout,

		// initialization
		buildRendering: function(){
			this.inherited(arguments);
			if(!this.domNode.getAttribute('tabIndex')){
				this.domNode.tabIndex = "0";
			}
			this.createScroller();
			this.createLayout();
			this.createViews();
			this.createManagers();

			this.createSelection();

			this.connect(this.selection, "onSelected", "onSelected");
			this.connect(this.selection, "onDeselected", "onDeselected");
			this.connect(this.selection, "onChanged", "onSelectionChanged");

			metrics.initOnFontResize();
			this.connect(metrics, "onFontResize", "textSizeChanged");
			util.funnelEvents(this.domNode, this, 'doKeyEvent', util.keyEvents);
			if (this.selectionMode != "none") {
				this.domNode.setAttribute("aria-multiselectable", this.selectionMode == "single" ? "false" : "true");
			}

			html.addClass(this.domNode, this.classTag);
			if(!this.isLeftToRight()){
				html.addClass(this.domNode, this.classTag+"Rtl");
			}
			if(this.rowHeight > 0){
				html.addClass(this.viewsNode, this.classTag + "FixedRowHeight");
			}
		},
		
		postMixInProperties: function(){
			this.inherited(arguments);
			var messages = dojo.i18n.getLocalization("dijit", "loading", this.lang);
			this.loadingMessage = string.substitute(this.loadingMessage, messages);
			this.errorMessage = string.substitute(this.errorMessage, messages);
			if(this.srcNodeRef && this.srcNodeRef.style.height){
				this.height = this.srcNodeRef.style.height;
			}
			// Call this to update our autoheight to start out
			this._setAutoHeightAttr(this.autoHeight, true);
			this.lastScrollTop = this.scrollTop = 0;
		},
		
		postCreate: function(){
			this._placeholders = [];
			this._setHeaderMenuAttr(this.headerMenu);
			this._setStructureAttr(this.structure);
			this._click = [];
			this.inherited(arguments);
			if(this.domNode && this.autoWidth && this.initialWidth){
				this.domNode.style.width = this.initialWidth;
			}
			if (this.domNode && !this.editable){
				// default value for aria-readonly is false, set to true if grid is not editable
				html.attr(this.domNode,"aria-readonly", "true");
			}
		},

		destroy: function(){
			this.domNode.onReveal = null;
			this.domNode.onSizeChange = null;

			// Fixes IE domNode leak
			delete this._click;

			if(this.scroller){
				this.scroller.destroy();
				delete this.scroller;
			}
			this.edit.destroy();
			delete this.edit;
			this.views.destroyViews();
			if(this.focus){
				this.focus.destroy();
				delete this.focus;
			}
			if(this.headerMenu&&this._placeholders.length){
				array.forEach(this._placeholders, function(p){ p.unReplace(true); });
				this.headerMenu.unBindDomNode(this.viewsHeaderNode);
			}
			this.inherited(arguments);
		},

		_setAutoHeightAttr: function(ah, skipRender){
			// Calculate our autoheight - turn it into a boolean or an integer
			if(typeof ah == "string"){
				if(!ah || ah == "false"){
					ah = false;
				}else if (ah == "true"){
					ah = true;
				}else{
					ah = window.parseInt(ah, 10);
				}
			}
			if(typeof ah == "number"){
				if(isNaN(ah)){
					ah = false;
				}
				// Autoheight must be at least 1, if it's a number.  If it's
				// less than 0, we'll take that to mean "all" rows (same as
				// autoHeight=true - if it is equal to zero, we'll take that
				// to mean autoHeight=false
				if(ah < 0){
					ah = true;
				}else if (ah === 0){
					ah = false;
				}
			}
			this.autoHeight = ah;
			if(typeof ah == "boolean"){
				this._autoHeight = ah;
			}else if(typeof ah == "number"){
				this._autoHeight = (ah >= this.get('rowCount'));
			}else{
				this._autoHeight = false;
			}
			if(this._started && !skipRender){
				this.render();
			}
		},

		_getRowCountAttr: function(){
			return this.updating && this.invalidated && this.invalidated.rowCount != undefined ?
				this.invalidated.rowCount : this.rowCount;
		},
		
		textSizeChanged: function(){
			this.render();
		},

		sizeChange: function(){
			this.update();
		},

		createManagers: function(){
			// summary:
			//		create grid managers for various tasks including rows, focus, selection, editing

			// row manager
			this.rows = new _RowManager(this);
			// focus manager
			this.focus = new _FocusManager(this);
			// edit manager
			this.edit = new _EditManager(this);
		},

		createSelection: function(){
			// summary:	Creates a new Grid selection manager.

			// selection manager
			this.selection = new Selection(this);
		},

		createScroller: function(){
			// summary:
			//		Creates a new virtual scroller
			this.scroller = new _Scroller();
			this.scroller.grid = this;
			this.scroller.renderRow = lang.hitch(this, "renderRow");
			this.scroller.removeRow = lang.hitch(this, "rowRemoved");
		},

		createLayout: function(){
			// summary:
			//		Creates a new Grid layout
			this.layout = new this._layoutClass(this);
			this.connect(this.layout, "moveColumn", "onMoveColumn");
		},

		onMoveColumn: function(){
			this.update();
		},
		
		onResizeColumn: function(/*int*/ cellIdx){
			// Called when a column is resized.
		},

		// views
		createViews: function(){
			this.views = new _ViewManager(this);
			this.views.createView = lang.hitch(this, "createView");
		},

		createView: function(inClass, idx){
			var c = lang.getObject(inClass);
			var view = new c({ grid: this, index: idx });
			this.viewsNode.appendChild(view.domNode);
			this.viewsHeaderNode.appendChild(view.headerNode);
			this.views.addView(view);
			html.attr(this.domNode, "align", this.isLeftToRight() ? 'left' : 'right');
			return view;
		},

		buildViews: function(){
			for(var i=0, vs; (vs=this.layout.structure[i]); i++){
				this.createView(vs.type || dojox._scopeName + ".grid._View", i).setStructure(vs);
			}
			this.scroller.setContentNodes(this.views.getContentNodes());
		},

		_setStructureAttr: function(structure){
			var s = structure;
			if(s && lang.isString(s)){
				dojo.deprecated("dojox.grid._Grid.set('structure', 'objVar')", "use dojox.grid._Grid.set('structure', objVar) instead", "2.0");
				s=lang.getObject(s);
			}
			this.structure = s;
			if(!s){
				if(this.layout.structure){
					s = this.layout.structure;
				}else{
					return;
				}
			}
			this.views.destroyViews();
			this.focus.focusView = null;
			if(s !== this.layout.structure){
				this.layout.setStructure(s);
			}
			this._structureChanged();
		},

		setStructure: function(/* dojox.grid.__ViewDef|dojox.grid.__ViewDef[]|dojox.grid.__CellDef[]|Array[dojox.grid.__CellDef[]] */ inStructure){
			// summary:
			//		Install a new structure and rebuild the grid.
			dojo.deprecated("dojox.grid._Grid.setStructure(obj)", "use dojox.grid._Grid.set('structure', obj) instead.", "2.0");
			this._setStructureAttr(inStructure);
		},
		
		getColumnTogglingItems: function(){
			// summary:
			//		returns an array of dijit.CheckedMenuItem widgets that can be
			//		added to a menu for toggling columns on and off.
			var items, checkedItems = [];
			items = array.map(this.layout.cells, function(cell){
				if(!cell.menuItems){ cell.menuItems = []; }

				var self = this;
				var item = new CheckedMenuItem({
					label: cell.name,
					checked: !cell.hidden,
					_gridCell: cell,
					onChange: function(checked){
						if(self.layout.setColumnVisibility(this._gridCell.index, checked)){
							var items = this._gridCell.menuItems;
							if(items.length > 1){
								array.forEach(items, function(item){
									if(item !== this){
										item.setAttribute("checked", checked);
									}
								}, this);
							}
							checked = array.filter(self.layout.cells, function(c){
								if(c.menuItems.length > 1){
									array.forEach(c.menuItems, "item.set('disabled', false);");
								}else{
									c.menuItems[0].set('disabled', false);
								}
								return !c.hidden;
							});
							if(checked.length == 1){
								array.forEach(checked[0].menuItems, "item.set('disabled', true);");
							}
						}
					},
					destroy: function(){
						var index = array.indexOf(this._gridCell.menuItems, this);
						this._gridCell.menuItems.splice(index, 1);
						delete this._gridCell;
						CheckedMenuItem.prototype.destroy.apply(this, arguments);
					}
				});
				cell.menuItems.push(item);
				if(!cell.hidden) {
					checkedItems.push(item);
				}
				return item;
			}, this); // dijit.CheckedMenuItem[]
			if(checkedItems.length == 1) {
				checkedItems[0].set('disabled', true);
			}
			return items;
		},

		_setHeaderMenuAttr: function(menu){
			if(this._placeholders && this._placeholders.length){
				array.forEach(this._placeholders, function(p){
					p.unReplace(true);
				});
				this._placeholders = [];
			}
			if(this.headerMenu){
				this.headerMenu.unBindDomNode(this.viewsHeaderNode);
			}
			this.headerMenu = menu;
			if(!menu){ return; }

			this.headerMenu.bindDomNode(this.viewsHeaderNode);
			if(this.headerMenu.getPlaceholders){
				this._placeholders = this.headerMenu.getPlaceholders(this.placeholderLabel);
			}
		},

		setHeaderMenu: function(/* dijit.Menu */ menu){
			dojo.deprecated("dojox.grid._Grid.setHeaderMenu(obj)", "use dojox.grid._Grid.set('headerMenu', obj) instead.", "2.0");
			this._setHeaderMenuAttr(menu);
		},
		
		setupHeaderMenu: function(){
			if(this._placeholders && this._placeholders.length){
				array.forEach(this._placeholders, function(p){
					if(p._replaced){
						p.unReplace(true);
					}
					p.replace(this.getColumnTogglingItems());
				}, this);
			}
		},

		_fetch: function(start){
			this.setScrollTop(0);
		},

		getItem: function(inRowIndex){
			return null;
		},
		
		showMessage: function(message){
			if(message){
				this.messagesNode.innerHTML = message;
				this.messagesNode.style.display = "";
			}else{
				this.messagesNode.innerHTML = "";
				this.messagesNode.style.display = "none";
			}
		},

		_structureChanged: function() {
			this.buildViews();
			if(this.autoRender && this._started){
				this.render();
			}
		},

		hasLayout: function() {
			return this.layout.cells.length;
		},

		// sizing
		resize: function(changeSize, resultSize){
			// summary:
			//		Update the grid's rendering dimensions and resize it
			
			// Calling sizeChange calls update() which calls _resize...so let's
			// save our input values, if any, and use them there when it gets
			// called.  This saves us an extra call to _resize(), which can
			// get kind of heavy.
			
			this._pendingChangeSize = changeSize;
			this._pendingResultSize = resultSize;
			this.sizeChange();
		},

		_getPadBorder: function() {
			this._padBorder = this._padBorder || html._getPadBorderExtents(this.domNode);
			return this._padBorder;
		},

		_getHeaderHeight: function(){
			var vns = this.viewsHeaderNode.style, t = vns.display == "none" ? 0 : this.views.measureHeader();
			vns.height = t + 'px';
			// header heights are reset during measuring so must be normalized after measuring.
			this.views.normalizeHeaderNodeHeight();
			return t;
		},
		
		_resize: function(changeSize, resultSize){
			// Restore our pending values, if any
			changeSize = changeSize || this._pendingChangeSize;
			resultSize = resultSize || this._pendingResultSize;
			delete this._pendingChangeSize;
			delete this._pendingResultSize;
			// if we have set up everything except the DOM, we cannot resize
			if(!this.domNode){ return; }
			var pn = this.domNode.parentNode;
			if(!pn || pn.nodeType != 1 || !this.hasLayout() || pn.style.visibility == "hidden" || pn.style.display == "none"){
				return;
			}
			// useful measurement
			var padBorder = this._getPadBorder();
			var hh = undefined;
			var h;
			// grid height
			if(this._autoHeight){
				this.domNode.style.height = 'auto';
			}else if(typeof this.autoHeight == "number"){
				h = hh = this._getHeaderHeight();
				h += (this.scroller.averageRowHeight * this.autoHeight);
				this.domNode.style.height = h + "px";
			}else if(this.domNode.clientHeight <= padBorder.h){
				if(pn == document.body){
					this.domNode.style.height = this.defaultHeight;
				}else if(this.height){
					this.domNode.style.height = this.height;
				}else{
					this.fitTo = "parent";
				}
			}
			// if we are given dimensions, size the grid's domNode to those dimensions
			if(resultSize){
				changeSize = resultSize;
			}
			if(!this._autoHeight && changeSize){
				html.marginBox(this.domNode, changeSize);
				this.height = this.domNode.style.height;
				delete this.fitTo;
			}else if(this.fitTo == "parent"){
				h = this._parentContentBoxHeight = (this._parentContentBoxHeight > 0 ? this._parentContentBoxHeight : html._getContentBox(pn).h);
				this.domNode.style.height = Math.max(0, h) + "px";
			}
			
			var hasFlex = array.some(this.views.views, function(v){ return v.flexCells; });

			if(!this._autoHeight && (h || html._getContentBox(this.domNode).h) === 0){
				// We need to hide the header, since the Grid is essentially hidden.
				this.viewsHeaderNode.style.display = "none";
			}else{
				// Otherwise, show the header and give it an appropriate height.
				this.viewsHeaderNode.style.display = "block";
				if(!hasFlex && hh === undefined){
					hh = this._getHeaderHeight();
				}
			}
			if(hasFlex){
				hh = undefined;
			}

			// NOTE: it is essential that width be applied before height
			// Header height can only be calculated properly after view widths have been set.
			// This is because flex column width is naturally 0 in Firefox.
			// Therefore prior to width sizing flex columns with spaces are maximally wrapped
			// and calculated to be too tall.
			this.adaptWidth();
			this.adaptHeight(hh);

			this.postresize();
		},

		adaptWidth: function() {
			// summary:
			//		sets width and position for views and update grid width if necessary
			// tags:
			//		private
			var doAutoWidth = (!this.initialWidth && this.autoWidth);
			var w = doAutoWidth ? 0 : this.domNode.clientWidth || (this.domNode.offsetWidth - this._getPadBorder().w),
				vw = this.views.arrange(1, w);
			this.views.onEach("adaptWidth");
			if(doAutoWidth){
				this.domNode.style.width = vw + "px";
			}
		},

		adaptHeight: function(inHeaderHeight){
			// summary:
			//		measures and normalizes header height, then sets view heights, and then updates scroller
			//		content extent
			// tags:
			//		private
			var t = inHeaderHeight === undefined ? this._getHeaderHeight() : inHeaderHeight;
			var h = (this._autoHeight ? -1 : Math.max(this.domNode.clientHeight - t, 0) || 0);
			this.views.onEach('setSize', [0, h]);
			this.views.onEach('adaptHeight');
			if(!this._autoHeight){
				var numScroll = 0, numNoScroll = 0;
				var noScrolls = array.filter(this.views.views, function(v){
					var has = v.hasHScrollbar();
					if(has){ numScroll++; }else{ numNoScroll++; }
					return (!has);
				});
				if(numScroll > 0 && numNoScroll > 0){
					array.forEach(noScrolls, function(v){
						v.adaptHeight(true);
					});
				}
			}
			if(this.autoHeight === true || h != -1 || (typeof this.autoHeight == "number" && this.autoHeight >= this.get('rowCount'))){
				this.scroller.windowHeight = h;
			}else{
				this.scroller.windowHeight = Math.max(this.domNode.clientHeight - t, 0);
			}
		},

		// startup
		startup: function(){
			if(this._started){return;}
			this.inherited(arguments);
			if(this.autoRender){
				this.render();
			}
		},

		// render
		render: function(){
			// summary:
			//	Render the grid, headers, and views. Edit and scrolling states are reset. To retain edit and
			//	scrolling states, see Update.

			if(!this.domNode){return;}
			if(!this._started){return;}

			if(!this.hasLayout()) {
				this.scroller.init(0, this.keepRows, this.rowsPerPage);
				return;
			}
			//
			this.update = this.defaultUpdate;
			this._render();
		},

		_render: function(){
			this.scroller.init(this.get('rowCount'), this.keepRows, this.rowsPerPage);
			this.prerender();
			this.setScrollTop(0);
			this.postrender();
		},

		prerender: function(){
			// if autoHeight, make sure scroller knows not to virtualize; everything must be rendered.
			this.keepRows = this._autoHeight ? 0 : this.keepRows;
			this.scroller.setKeepInfo(this.keepRows);
			this.views.render();
			this._resize();
		},

		postrender: function(){
			this.postresize();
			this.focus.initFocusView();
			// make rows unselectable
			html.setSelectable(this.domNode, this.selectable);
		},

		postresize: function(){
			// views are position absolute, so they do not inflate the parent
			if(this._autoHeight){
				var size = Math.max(this.views.measureContent()) + 'px';
				
				this.viewsNode.style.height = size;
			}
		},

		renderRow: function(inRowIndex, inNodes){
			// summary:
			//		used internally to render rows
			// tags:
			//		private
			this.views.renderRow(inRowIndex, inNodes, this._skipRowRenormalize);
		},

		rowRemoved: function(inRowIndex){
			// summary:
			//		used internally to remove rows
			// tags:
			//		private
			this.views.rowRemoved(inRowIndex);
		},

		invalidated: null,

		updating: false,

		beginUpdate: function(){
			// summary:
			//		Use to make multiple changes to rows while queueing row updating.
			// NOTE: not currently supporting nested begin/endUpdate calls
			this.invalidated = [];
			this.updating = true;
		},

		endUpdate: function(){
			// summary:
			//		Use after calling beginUpdate to render any changes made to rows.
			this.updating = false;
			var i = this.invalidated, r;
			if(i.all){
				this.update();
			}else if(i.rowCount != undefined){
				this.updateRowCount(i.rowCount);
			}else{
				for(r in i){
					this.updateRow(Number(r));
				}
			}
			this.invalidated = [];
		},

		// update
		defaultUpdate: function(){
			// note: initial update calls render and subsequently this function.
			if(!this.domNode){return;}
			if(this.updating){
				this.invalidated.all = true;
				return;
			}
			//this.edit.saveState(inRowIndex);
			this.lastScrollTop = this.scrollTop;
			this.prerender();
			this.scroller.invalidateNodes();
			this.setScrollTop(this.lastScrollTop);
			this.postrender();
			//this.edit.restoreState(inRowIndex);
		},

		update: function(){
			// summary:
			//		Update the grid, retaining edit and scrolling states.
			this.render();
		},

		updateRow: function(inRowIndex){
			// summary:
			//		Render a single row.
			// inRowIndex: Integer
			//		Index of the row to render
			inRowIndex = Number(inRowIndex);
			if(this.updating){
				this.invalidated[inRowIndex]=true;
			}else{
				this.views.updateRow(inRowIndex);
				this.scroller.rowHeightChanged(inRowIndex);
			}
		},

		updateRows: function(startIndex, howMany){
			// summary:
			//		Render consecutive rows at once.
			// startIndex: Integer
			//		Index of the starting row to render
			// howMany: Integer
			//		How many rows to update.
			startIndex = Number(startIndex);
			howMany = Number(howMany);
			var i;
			if(this.updating){
				for(i=0; i<howMany; i++){
					this.invalidated[i+startIndex]=true;
				}
			}else{
				for(i=0; i<howMany; i++){
					this.views.updateRow(i+startIndex, this._skipRowRenormalize);
				}
				this.scroller.rowHeightChanged(startIndex);
			}
		},

		updateRowCount: function(inRowCount){
			// summary:
			//		Change the number of rows.
			// inRowCount: int
			//		Number of rows in the grid.
			if(this.updating){
				this.invalidated.rowCount = inRowCount;
			}else{
				this.rowCount = inRowCount;
				this._setAutoHeightAttr(this.autoHeight, true);
				if(this.layout.cells.length){
					this.scroller.updateRowCount(inRowCount);
				}
				if(this.layout.cells.length){
					this.setScrollTop(this.scrollTop);
				}
				this._resize();
			}
		},

		updateRowStyles: function(inRowIndex){
			// summary:
			//		Update the styles for a row after it's state has changed.
			this.views.updateRowStyles(inRowIndex);
		},
		getRowNode: function(inRowIndex){
			// summary:
			//		find the rowNode that is not a rowSelector
			if (this.focus.focusView && !(this.focus.focusView instanceof _RowSelector)){
					return this.focus.focusView.rowNodes[inRowIndex];
			}else{ // search through views
				for (var i = 0, cView; (cView = this.views.views[i]); i++) {
					if (!(cView instanceof _RowSelector)) {
						return cView.rowNodes[inRowIndex];
					}
				}
			}
			return null;
		},
		rowHeightChanged: function(inRowIndex){
			// summary:
			//		Update grid when the height of a row has changed. Row height is handled automatically as rows
			//		are rendered. Use this function only to update a row's height outside the normal rendering process.
			// inRowIndex: Integer
			//		index of the row that has changed height

			this.views.renormalizeRow(inRowIndex);
			this.scroller.rowHeightChanged(inRowIndex);
		},

		// fastScroll: Boolean
		//		flag modifies vertical scrolling behavior. Defaults to true but set to false for slower
		//		scroll performance but more immediate scrolling feedback
		fastScroll: true,

		delayScroll: false,

		// scrollRedrawThreshold: int
		//	pixel distance a user must scroll vertically to trigger grid scrolling.
		scrollRedrawThreshold: (has('ie') ? 100 : 50),

		// scroll methods
		scrollTo: function(inTop){
			// summary:
			//		Vertically scroll the grid to a given pixel position
			// inTop: Integer
			//		vertical position of the grid in pixels
			if(!this.fastScroll){
				this.setScrollTop(inTop);
				return;
			}
			var delta = Math.abs(this.lastScrollTop - inTop);
			this.lastScrollTop = inTop;
			if(delta > this.scrollRedrawThreshold || this.delayScroll){
				this.delayScroll = true;
				this.scrollTop = inTop;
				this.views.setScrollTop(inTop);
				if(this._pendingScroll){
					window.clearTimeout(this._pendingScroll);
				}
				var _this = this;
				this._pendingScroll = window.setTimeout(function(){
					delete _this._pendingScroll;
					_this.finishScrollJob();
				}, 200);
			}else{
				this.setScrollTop(inTop);
			}
		},

		finishScrollJob: function(){
			this.delayScroll = false;
			this.setScrollTop(this.scrollTop);
		},

		setScrollTop: function(inTop){
			this.scroller.scroll(this.views.setScrollTop(inTop));
		},

		scrollToRow: function(inRowIndex){
			// summary:
			//		Scroll the grid to a specific row.
			// inRowIndex: Integer
			//		grid row index
			this.setScrollTop(this.scroller.findScrollTop(inRowIndex) + 1);
		},

		styleRowNode: function(inRowIndex, inRowNode){
			// summary:
			//		styling (used internally to style individual parts of a row)
			// tags:
			//		private
			if(inRowNode){
				this.rows.styleRowNode(inRowIndex, inRowNode);
			}
		},
		
		// called when the mouse leaves the grid so we can deselect all hover rows
		_mouseOut: function(e){
			this.rows.setOverRow(-2);
		},
	
		// cells
		getCell: function(inIndex){
			// summary:
			//		Retrieves the cell object for a given grid column.
			// inIndex: Integer
			//		Grid column index of cell to retrieve
			// returns:
			//		a grid cell
			return this.layout.cells[inIndex];
		},

		setCellWidth: function(inIndex, inUnitWidth){
			this.getCell(inIndex).unitWidth = inUnitWidth;
		},

		getCellName: function(inCell){
			// summary:
			//		Returns the cell name of a passed cell
			return "Cell " + inCell.index; // String
		},

		// sorting
		canSort: function(inSortInfo){
			// summary:
			//		Determines if the grid can be sorted
			// inSortInfo: Integer
			//		Sort information, 1-based index of column on which to sort, positive for an ascending sort
			//		and negative for a descending sort
			// returns: Boolean
			//		True if grid can be sorted on the given column in the given direction
		},

		sort: function(){
		},

		getSortAsc: function(inSortInfo){
			// summary:
			//		Returns true if grid is sorted in an ascending direction.
			inSortInfo = inSortInfo == undefined ? this.sortInfo : inSortInfo;
			return Boolean(inSortInfo > 0); // Boolean
		},

		getSortIndex: function(inSortInfo){
			// summary:
			//		Returns the index of the column on which the grid is sorted
			inSortInfo = inSortInfo == undefined ? this.sortInfo : inSortInfo;
			return Math.abs(inSortInfo) - 1; // Integer
		},

		setSortIndex: function(inIndex, inAsc){
			// summary:
			//		Sort the grid on a column in a specified direction
			// inIndex: Integer
			//		Column index on which to sort.
			// inAsc: Boolean
			//		If true, sort the grid in ascending order, otherwise in descending order
			var si = inIndex +1;
			if(inAsc != undefined){
				si *= (inAsc ? 1 : -1);
			} else if(this.getSortIndex() == inIndex){
				si = -this.sortInfo;
			}
			this.setSortInfo(si);
		},

		setSortInfo: function(inSortInfo){
			if(this.canSort(inSortInfo)){
				this.sortInfo = inSortInfo;
				this.sort();
				this.update();
			}
		},

		// DOM event handler
		doKeyEvent: function(e){
			e.dispatch = 'do' + e.type;
			this.onKeyEvent(e);
		},

		// event dispatch
		//: protected
		_dispatch: function(m, e){
			if(m in this){
				return this[m](e);
			}
			return false;
		},

		dispatchKeyEvent: function(e){
			this._dispatch(e.dispatch, e);
		},

		dispatchContentEvent: function(e){
			this.edit.dispatchEvent(e) || e.sourceView.dispatchContentEvent(e) || this._dispatch(e.dispatch, e);
		},

		dispatchHeaderEvent: function(e){
			e.sourceView.dispatchHeaderEvent(e) || this._dispatch('doheader' + e.type, e);
		},

		dokeydown: function(e){
			this.onKeyDown(e);
		},

		doclick: function(e){
			if(e.cellNode){
				this.onCellClick(e);
			}else{
				this.onRowClick(e);
			}
		},

		dodblclick: function(e){
			if(e.cellNode){
				this.onCellDblClick(e);
			}else{
				this.onRowDblClick(e);
			}
		},

		docontextmenu: function(e){
			if(e.cellNode){
				this.onCellContextMenu(e);
			}else{
				this.onRowContextMenu(e);
			}
		},

		doheaderclick: function(e){
			if(e.cellNode){
				this.onHeaderCellClick(e);
			}else{
				this.onHeaderClick(e);
			}
		},

		doheaderdblclick: function(e){
			if(e.cellNode){
				this.onHeaderCellDblClick(e);
			}else{
				this.onHeaderDblClick(e);
			}
		},

		doheadercontextmenu: function(e){
			if(e.cellNode){
				this.onHeaderCellContextMenu(e);
			}else{
				this.onHeaderContextMenu(e);
			}
		},

		// override to modify editing process
		doStartEdit: function(inCell, inRowIndex){
			this.onStartEdit(inCell, inRowIndex);
		},

		doApplyCellEdit: function(inValue, inRowIndex, inFieldIndex){
			this.onApplyCellEdit(inValue, inRowIndex, inFieldIndex);
		},

		doCancelEdit: function(inRowIndex){
			this.onCancelEdit(inRowIndex);
		},

		doApplyEdit: function(inRowIndex){
			this.onApplyEdit(inRowIndex);
		},

		// row editing
		addRow: function(){
			// summary:
			//		Add a row to the grid.
			this.updateRowCount(this.get('rowCount')+1);
		},

		removeSelectedRows: function(){
			// summary:
			//		Remove the selected rows from the grid.
			if(this.allItemsSelected){
				this.updateRowCount(0);
			}else{
				this.updateRowCount(Math.max(0, this.get('rowCount') - this.selection.getSelected().length));
			}
			this.selection.clear();
		}

	});

	_Grid.markupFactory = function(props, node, ctor, cellFunc){
		var widthFromAttr = function(n){
			var w = html.attr(n, "width")||"auto";
			if((w != "auto")&&(w.slice(-2) != "em")&&(w.slice(-1) != "%")){
				w = parseInt(w, 10)+"px";
			}
			return w;
		};
		// if(!props.store){ console.debug("no store!"); }
		// if a structure isn't referenced, do we have enough
		// data to try to build one automatically?
		if(	!props.structure &&
			node.nodeName.toLowerCase() == "table"){

			// try to discover a structure
			props.structure = query("> colgroup", node).map(function(cg){
				var sv = html.attr(cg, "span");
				var v = {
					noscroll: (html.attr(cg, "noscroll") == "true") ? true : false,
					__span: (!!sv ? parseInt(sv, 10) : 1),
					cells: []
				};
				if(html.hasAttr(cg, "width")){
					v.width = widthFromAttr(cg);
				}
				return v; // for vendetta
			});
			if(!props.structure.length){
				props.structure.push({
					__span: Infinity,
					cells: [] // catch-all view
				});
			}
			// check to see if we're gonna have more than one view

			// for each tr in our th, create a row of cells
			query("thead > tr", node).forEach(function(tr, tr_idx){
				var cellCount = 0;
				var viewIdx = 0;
				var lastViewIdx;
				var cView = null;
				query("> th", tr).map(function(th){
					// what view will this cell go into?

					// NOTE:
					//		to prevent extraneous iteration, we start counters over
					//		for each row, incrementing over the surface area of the
					//		structure that colgroup processing generates and
					//		creating cell objects for each <th> to place into those
					//		cell groups.  There's a lot of state-keepking logic
					//		here, but it is what it has to be.
					if(!cView){ // current view book keeping
						lastViewIdx = 0;
						cView = props.structure[0];
					}else if(cellCount >= (lastViewIdx+cView.__span)){
						viewIdx++;
						// move to allocating things into the next view
						lastViewIdx += cView.__span;
						var lastView = cView;
						cView = props.structure[viewIdx];
					}

					// actually define the cell from what markup hands us
					var cell = {
						name: lang.trim(html.attr(th, "name")||th.innerHTML),
						colSpan: parseInt(html.attr(th, "colspan")||1, 10),
						type: lang.trim(html.attr(th, "cellType")||""),
						id: lang.trim(html.attr(th,"id")||"")
					};
					cellCount += cell.colSpan;
					var rowSpan = html.attr(th, "rowspan");
					if(rowSpan){
						cell.rowSpan = rowSpan;
					}
					if(html.hasAttr(th, "width")){
						cell.width = widthFromAttr(th);
					}
					if(html.hasAttr(th, "relWidth")){
						cell.relWidth = window.parseInt(html.attr(th, "relWidth"), 10);
					}
					if(html.hasAttr(th, "hidden")){
						cell.hidden = (html.attr(th, "hidden") == "true" || html.attr(th, "hidden") === true/*always boolean true in Chrome*/);
					}

					if(cellFunc){
						cellFunc(th, cell);
					}

					cell.type = cell.type ? lang.getObject(cell.type) : dojox.grid.cells.Cell;

					if(cell.type && cell.type.markupFactory){
						cell.type.markupFactory(th, cell);
					}

					if(!cView.cells[tr_idx]){
						cView.cells[tr_idx] = [];
					}
					cView.cells[tr_idx].push(cell);
				});
			});
		}

		return new ctor(props, node);
	};

	return _Grid;

});

},
'dojox/grid/_Events':function(){
define([
	"dojo/keys",
	"dojo/dom-class",
	"dojo/_base/declare",
	"dojo/_base/event",
	"dojo/_base/sniff"
], function(keys, domClass, declare, event, has){

return declare("dojox.grid._Events", null, {
	// summary:
	//		_Grid mixin that provides default implementations for grid events.
	// description:
	//		Default synthetic events dispatched for _Grid. dojo.connect to events to
	//		retain default implementation or override them for custom handling.

	// cellOverClass: String
	//		css class to apply to grid cells over which the cursor is placed.
	cellOverClass: "dojoxGridCellOver",

	onKeyEvent: function(e){
		// summary:
		//		top level handler for Key Events
		this.dispatchKeyEvent(e);
	},

	onContentEvent: function(e){
		// summary:
		//		Top level handler for Content events
		this.dispatchContentEvent(e);
	},

	onHeaderEvent: function(e){
		// summary:
		//		Top level handler for header events
		this.dispatchHeaderEvent(e);
	},

	onStyleRow: function(inRow){
		// summary:
		//		Perform row styling on a given row. Called whenever row styling is updated.
		// inRow: Object
		//		Object containing row state information: selected, true if the row is selcted; over:
		//		true of the mouse is over the row; odd: true if the row is odd. Use customClasses and
		//		customStyles to control row css classes and styles; both properties are strings.
		// example:
		// |	onStyleRow({ selected: true, over:true, odd:false })
		var i = inRow;
		i.customClasses += (i.odd?" dojoxGridRowOdd":"") + (i.selected?" dojoxGridRowSelected":"") + (i.over?" dojoxGridRowOver":"");
		this.focus.styleRow(inRow);
		this.edit.styleRow(inRow);
	},

	onKeyDown: function(e){
		// summary:
		//		Grid key event handler. By default enter begins editing and applies edits, escape cancels an edit,
		//		tab, shift-tab, and arrow keys move grid cell focus.
		if(e.altKey || e.metaKey){
			return;
		}
		var colIdx;
		switch(e.keyCode){
			case keys.ESCAPE:
				this.edit.cancel();
				break;
			case keys.ENTER:
				if(!this.edit.isEditing()){
					colIdx = this.focus.getHeaderIndex();
					if(colIdx >= 0) {
						this.setSortIndex(colIdx);
						break;
					}else {
						this.selection.clickSelect(this.focus.rowIndex, dojo.isCopyKey(e), e.shiftKey);
					}
					event.stop(e);
				}
				if(!e.shiftKey){
					var isEditing = this.edit.isEditing();
					this.edit.apply();
					if(!isEditing){
						this.edit.setEditCell(this.focus.cell, this.focus.rowIndex);
					}
				}
				if (!this.edit.isEditing()){
					var curView = this.focus.focusView || this.views.views[0];  //if no focusView than only one view
					curView.content.decorateEvent(e);
					this.onRowClick(e);
					event.stop(e);
				}
				break;
			case keys.SPACE:
				if(!this.edit.isEditing()){
					colIdx = this.focus.getHeaderIndex();
					if(colIdx >= 0) {
						this.setSortIndex(colIdx);
						break;
					}else {
						this.selection.clickSelect(this.focus.rowIndex, dojo.isCopyKey(e), e.shiftKey);
						// Set focus back on the cell they were on for keyboard accessibility
						this.focus._focusifyCellNode(true);
						this.focus.setFocusCell(this.focus.cell, this.focus.rowIndex);					}
					event.stop(e);
				}
				break;
			case keys.TAB:
				this.focus[e.shiftKey ? 'previousKey' : 'nextKey'](e);
				break;
			case keys.LEFT_ARROW:
			case keys.RIGHT_ARROW:
				if(!this.edit.isEditing()){
					var keyCode = e.keyCode;  // IE seems to lose after stopEvent when modifier keys
					event.stop(e);
					colIdx = this.focus.getHeaderIndex();
					if (colIdx >= 0 && (e.shiftKey && e.ctrlKey)){
						this.focus.colSizeAdjust(e, colIdx, (keyCode == keys.LEFT_ARROW ? -1 : 1)*5);
					}
					else{
						var offset = (keyCode == keys.LEFT_ARROW) ? 1 : -1;
						if(this.isLeftToRight()){ offset *= -1; }
						this.focus.move(0, offset);
					}
				}
				break;
			case keys.UP_ARROW:
				if(!this.edit.isEditing() && this.focus.rowIndex !== 0){
					event.stop(e);
					this.focus.move(-1, 0);
				}
				break;
			case keys.DOWN_ARROW:
				if(!this.edit.isEditing() && this.focus.rowIndex+1 != this.rowCount){
					event.stop(e);
					this.focus.move(1, 0);
				}
				break;
			case keys.PAGE_UP:
				if(!this.edit.isEditing() && this.focus.rowIndex !== 0){
					event.stop(e);
					if(this.focus.rowIndex != this.scroller.firstVisibleRow+1){
						this.focus.move(this.scroller.firstVisibleRow-this.focus.rowIndex, 0);
					}else{
						this.setScrollTop(this.scroller.findScrollTop(this.focus.rowIndex-1));
						this.focus.move(this.scroller.firstVisibleRow-this.scroller.lastVisibleRow+1, 0);
					}
				}
				break;
			case keys.PAGE_DOWN:
				if(!this.edit.isEditing() && this.focus.rowIndex+1 != this.rowCount){
					event.stop(e);
					if(this.focus.rowIndex != this.scroller.lastVisibleRow-1){
						this.focus.move(this.scroller.lastVisibleRow-this.focus.rowIndex-1, 0);
					}else{
						this.setScrollTop(this.scroller.findScrollTop(this.focus.rowIndex+1));
						this.focus.move(this.scroller.lastVisibleRow-this.scroller.firstVisibleRow-1, 0);
					}
				}
				break;
			default:
				break;
		}
	},

	onMouseOver: function(e){
		// summary:
		//		Event fired when mouse is over the grid.
		// e: Event
		//		Decorated event object contains reference to grid, cell, and rowIndex
		e.rowIndex == -1 ? this.onHeaderCellMouseOver(e) : this.onCellMouseOver(e);
	},

	onMouseOut: function(e){
		// summary:
		//		Event fired when mouse moves out of the grid.
		// e: Event
		//		Decorated event object that contains reference to grid, cell, and rowIndex
		e.rowIndex == -1 ? this.onHeaderCellMouseOut(e) : this.onCellMouseOut(e);
	},

	onMouseDown: function(e){
		// summary:
		//		Event fired when mouse is down inside grid.
		// e: Event
		//		Decorated event object that contains reference to grid, cell, and rowIndex
		e.rowIndex == -1 ? this.onHeaderCellMouseDown(e) : this.onCellMouseDown(e);
	},

	onMouseOverRow: function(e){
		// summary:
		//		Event fired when mouse is over any row (data or header).
		// e: Event
		//		Decorated event object contains reference to grid, cell, and rowIndex
		if(!this.rows.isOver(e.rowIndex)){
			this.rows.setOverRow(e.rowIndex);
			e.rowIndex == -1 ? this.onHeaderMouseOver(e) : this.onRowMouseOver(e);
		}
	},
	onMouseOutRow: function(e){
		// summary:
		//		Event fired when mouse moves out of any row (data or header).
		// e: Event
		//		Decorated event object contains reference to grid, cell, and rowIndex
		if(this.rows.isOver(-1)){
			this.onHeaderMouseOut(e);
		}else if(!this.rows.isOver(-2)){
			this.rows.setOverRow(-2);
			this.onRowMouseOut(e);
		}
	},

	onMouseDownRow: function(e){
		// summary:
		//		Event fired when mouse is down inside grid row
		// e: Event
		//		Decorated event object that contains reference to grid, cell, and rowIndex
		if(e.rowIndex != -1)
			this.onRowMouseDown(e);
	},

	// cell events
	onCellMouseOver: function(e){
		// summary:
		//		Event fired when mouse is over a cell.
		// e: Event
		//		Decorated event object contains reference to grid, cell, and rowIndex
		if(e.cellNode){
			domClass.add(e.cellNode, this.cellOverClass);
		}
	},

	onCellMouseOut: function(e){
		// summary:
		//		Event fired when mouse moves out of a cell.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		if(e.cellNode){
			domClass.remove(e.cellNode, this.cellOverClass);
		}
	},

	onCellMouseDown: function(e){
		// summary:
		//		Event fired when mouse is down in a header cell.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
	},

	onCellClick: function(e){
		// summary:
		//		Event fired when a cell is clicked.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		this._click[0] = this._click[1];
		this._click[1] = e;
		if(!this.edit.isEditCell(e.rowIndex, e.cellIndex)){
			this.focus.setFocusCell(e.cell, e.rowIndex);
		}
		// in some cases click[0] is null which causes false doubeClicks. Fixes #100703
		if(this._click.length > 1 && this._click[0] == null){
			this._click.shift();
		}
		this.onRowClick(e);
	},

	onCellDblClick: function(e){
		// summary:
		//		Event fired when a cell is double-clicked.
		// e: Event
		//		Decorated event object contains reference to grid, cell, and rowIndex
		var event;
		if(this._click.length > 1 && has('ie')){
			event = this._click[1];
		}else if(this._click.length > 1 && this._click[0].rowIndex != this._click[1].rowIndex){
			event = this._click[0];
		}else{
			event = e;
		}
		this.focus.setFocusCell(event.cell, event.rowIndex);
		this.edit.setEditCell(event.cell, event.rowIndex);
		this.onRowDblClick(e);
	},

	onCellContextMenu: function(e){
		// summary:
		//		Event fired when a cell context menu is accessed via mouse right click.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		this.onRowContextMenu(e);
	},

	onCellFocus: function(inCell, inRowIndex){
		// summary:
		//		Event fired when a cell receives focus.
		// inCell: Object
		//		Cell object containing properties of the grid column.
		// inRowIndex: Integer
		//		Index of the grid row
		this.edit.cellFocus(inCell, inRowIndex);
	},

	// row events
	onRowClick: function(e){
		// summary:
		//		Event fired when a row is clicked.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		this.edit.rowClick(e);
		this.selection.clickSelectEvent(e);
	},

	onRowDblClick: function(e){
		// summary:
		//		Event fired when a row is double clicked.
		// e: Event
		//		decorated event object which contains reference to grid, cell, and rowIndex
	},

	onRowMouseOver: function(e){
		// summary:
		//		Event fired when mouse moves over a data row.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
	},

	onRowMouseOut: function(e){
		// summary:
		//		Event fired when mouse moves out of a data row.
		// e: Event
		//		Decorated event object contains reference to grid, cell, and rowIndex
	},

	onRowMouseDown: function(e){
		// summary:
		//		Event fired when mouse is down in a row.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
	},

	onRowContextMenu: function(e){
		// summary:
		//		Event fired when a row context menu is accessed via mouse right click.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		event.stop(e);
	},

	// header events
	onHeaderMouseOver: function(e){
		// summary:
		//		Event fired when mouse moves over the grid header.
		// e: Event
		//		Decorated event object contains reference to grid, cell, and rowIndex
	},

	onHeaderMouseOut: function(e){
		// summary:
		//		Event fired when mouse moves out of the grid header.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
	},

	onHeaderCellMouseOver: function(e){
		// summary:
		//		Event fired when mouse moves over a header cell.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		if(e.cellNode){
			domClass.add(e.cellNode, this.cellOverClass);
		}
	},

	onHeaderCellMouseOut: function(e){
		// summary:
		//		Event fired when mouse moves out of a header cell.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		if(e.cellNode){
			domClass.remove(e.cellNode, this.cellOverClass);
		}
	},

	onHeaderCellMouseDown: function(e) {
		// summary:
		//		Event fired when mouse is down in a header cell.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
	},

	onHeaderClick: function(e){
		// summary:
		//		Event fired when the grid header is clicked.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
	},

	onHeaderCellClick: function(e){
		// summary:
		//		Event fired when a header cell is clicked.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		this.setSortIndex(e.cell.index);
		this.onHeaderClick(e);
	},

	onHeaderDblClick: function(e){
		// summary:
		//		Event fired when the grid header is double clicked.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
	},

	onHeaderCellDblClick: function(e){
		// summary:
		//		Event fired when a header cell is double clicked.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		this.onHeaderDblClick(e);
	},

	onHeaderCellContextMenu: function(e){
		// summary:
		//		Event fired when a header cell context menu is accessed via mouse right click.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		this.onHeaderContextMenu(e);
	},

	onHeaderContextMenu: function(e){
		// summary:
		//		Event fired when the grid header context menu is accessed via mouse right click.
		// e: Event
		//		Decorated event object which contains reference to grid, cell, and rowIndex
		if(!this.headerMenu){
			event.stop(e);
		}
	},

	// editing
	onStartEdit: function(inCell, inRowIndex){
		// summary:
		//		Event fired when editing is started for a given grid cell
		// inCell: Object
		//		Cell object containing properties of the grid column.
		// inRowIndex: Integer
		//		Index of the grid row
	},

	onApplyCellEdit: function(inValue, inRowIndex, inFieldIndex){
		// summary:
		//		Event fired when editing is applied for a given grid cell
		// inValue: String
		//		Value from cell editor
		// inRowIndex: Integer
		//		Index of the grid row
		// inFieldIndex: Integer
		//		Index in the grid's data store
	},

	onCancelEdit: function(inRowIndex){
		// summary:
		//		Event fired when editing is cancelled for a given grid cell
		// inRowIndex: Integer
		//		Index of the grid row
	},

	onApplyEdit: function(inRowIndex){
		// summary:
		//		Event fired when editing is applied for a given grid row
		// inRowIndex: Integer
		//		Index of the grid row
	},

	onCanSelect: function(inRowIndex){
		// summary:
		//		Event to determine if a grid row may be selected
		// inRowIndex: Integer
		//		Index of the grid row
		// returns: Boolean
		//		true if the row can be selected
		return true;
	},

	onCanDeselect: function(inRowIndex){
		// summary:
		//		Event to determine if a grid row may be deselected
		// inRowIndex: Integer
		//		Index of the grid row
		// returns: Boolean
		//		true if the row can be deselected
		return true;
	},

	onSelected: function(inRowIndex){
		// summary:
		//		Event fired when a grid row is selected
		// inRowIndex: Integer
		//		Index of the grid row
		this.updateRowStyles(inRowIndex);
	},

	onDeselected: function(inRowIndex){
		// summary:
		//		Event fired when a grid row is deselected
		// inRowIndex: Integer
		//		Index of the grid row
		this.updateRowStyles(inRowIndex);
	},

	onSelectionChanged: function(){
	}
});
});

},
'dojox/grid/_Scroller':function(){
define([
	"dijit/registry",
	"dojo/_base/declare",
	"dojo/_base/lang",
	"./util",
	"dojo/_base/html"
], function(dijitRegistry, declare, lang, util, html){

	var indexInParent = function(inNode){
		var i=0, n, p=inNode.parentNode;
		while((n = p.childNodes[i++])){
			if(n == inNode){
				return i - 1;
			}
		}
		return -1;
	};
	
	var cleanNode = function(inNode){
		if(!inNode){
			return;
		}
		dojo.forEach(dijitRegistry.toArray(), function(w){
			if(w.domNode && html.isDescendant(w.domNode, inNode, true)){
				w.destroy();
			}
		});
	};

	var getTagName = function(inNodeOrId){
		var node = html.byId(inNodeOrId);
		return (node && node.tagName ? node.tagName.toLowerCase() : '');
	};
	
	var nodeKids = function(inNode, inTag){
		var result = [];
		var i=0, n;
		while((n = inNode.childNodes[i])){
			i++;
			if(getTagName(n) == inTag){
				result.push(n);
			}
		}
		return result;
	};
	
	var divkids = function(inNode){
		return nodeKids(inNode, 'div');
	};

	return declare("dojox.grid._Scroller", null, {
		constructor: function(inContentNodes){
			this.setContentNodes(inContentNodes);
			this.pageHeights = [];
			this.pageNodes = [];
			this.stack = [];
		},
		// specified
		rowCount: 0, // total number of rows to manage
		defaultRowHeight: 32, // default height of a row
		keepRows: 100, // maximum number of rows that should exist at one time
		contentNode: null, // node to contain pages
		scrollboxNode: null, // node that controls scrolling
		// calculated
		defaultPageHeight: 0, // default height of a page
		keepPages: 10, // maximum number of pages that should exists at one time
		pageCount: 0,
		windowHeight: 0,
		firstVisibleRow: 0,
		lastVisibleRow: 0,
		averageRowHeight: 0, // the average height of a row
		// private
		page: 0,
		pageTop: 0,
		// init
		init: function(inRowCount, inKeepRows, inRowsPerPage){
			switch(arguments.length){
				case 3: this.rowsPerPage = inRowsPerPage;
				case 2: this.keepRows = inKeepRows;
				case 1: this.rowCount = inRowCount;
				default: break;
			}
			this.defaultPageHeight = (this.grid.rowHeight > 0 ? this.grid.rowHeight : this.defaultRowHeight) * this.rowsPerPage;
			this.pageCount = this._getPageCount(this.rowCount, this.rowsPerPage);
			this.setKeepInfo(this.keepRows);
			this.invalidate();
			if(this.scrollboxNode){
				this.scrollboxNode.scrollTop = 0;
				this.scroll(0);
				this.scrollboxNode.onscroll = lang.hitch(this, 'onscroll');
			}
		},
		_getPageCount: function(rowCount, rowsPerPage){
			return rowCount ? (Math.ceil(rowCount / rowsPerPage) || 1) : 0;
		},
		destroy: function(){
			this.invalidateNodes();
			delete this.contentNodes;
			delete this.contentNode;
			delete this.scrollboxNode;
		},
		setKeepInfo: function(inKeepRows){
			this.keepRows = inKeepRows;
			this.keepPages = !this.keepRows ? this.keepPages : Math.max(Math.ceil(this.keepRows / this.rowsPerPage), 2);
		},
		// nodes
		setContentNodes: function(inNodes){
			this.contentNodes = inNodes;
			this.colCount = (this.contentNodes ? this.contentNodes.length : 0);
			this.pageNodes = [];
			for(var i=0; i<this.colCount; i++){
				this.pageNodes[i] = [];
			}
		},
		getDefaultNodes: function(){
			return this.pageNodes[0] || [];
		},
		// updating
		invalidate: function(){
			this._invalidating = true;
			this.invalidateNodes();
			this.pageHeights = [];
			this.height = (this.pageCount ? (this.pageCount - 1)* this.defaultPageHeight + this.calcLastPageHeight() : 0);
			this.resize();
			this._invalidating = false;
		},
		updateRowCount: function(inRowCount){
			this.invalidateNodes();
			this.rowCount = inRowCount;
			// update page count, adjust document height
			var oldPageCount = this.pageCount;
			if(oldPageCount === 0){
				//We want to have at least 1px in height to keep scroller.  Otherwise with an
				//empty grid you can't scroll to see the header.
				this.height = 1;
			}
			this.pageCount = this._getPageCount(this.rowCount, this.rowsPerPage);
			if(this.pageCount < oldPageCount){
				for(var i=oldPageCount-1; i>=this.pageCount; i--){
					this.height -= this.getPageHeight(i);
					delete this.pageHeights[i];
				}
			}else if(this.pageCount > oldPageCount){
				this.height += this.defaultPageHeight * (this.pageCount - oldPageCount - 1) + this.calcLastPageHeight();
			}
			this.resize();
		},
		// implementation for page manager
		pageExists: function(inPageIndex){
			return Boolean(this.getDefaultPageNode(inPageIndex));
		},
		measurePage: function(inPageIndex){
			if(this.grid.rowHeight){
				return ((inPageIndex + 1) * this.rowsPerPage > this.rowCount ?
					this.rowCount - inPageIndex * this.rowsPerPage :
					this.rowsPerPage) * this.grid.rowHeight;
					 
			}
			var n = this.getDefaultPageNode(inPageIndex);
			return (n && n.innerHTML) ? n.offsetHeight : undefined;
		},
		positionPage: function(inPageIndex, inPos){
			for(var i=0; i<this.colCount; i++){
				this.pageNodes[i][inPageIndex].style.top = inPos + 'px';
			}
		},
		repositionPages: function(inPageIndex){
			var nodes = this.getDefaultNodes();
			var last = 0;

			for(var i=0; i<this.stack.length; i++){
				last = Math.max(this.stack[i], last);
			}
			//
			var n = nodes[inPageIndex];
			var y = (n ? this.getPageNodePosition(n) + this.getPageHeight(inPageIndex) : 0);
			for(var p=inPageIndex+1; p<=last; p++){
				n = nodes[p];
				if(n){
					if(this.getPageNodePosition(n) == y){
						return;
					}
					this.positionPage(p, y);
				}
				y += this.getPageHeight(p);
			}
		},
		installPage: function(inPageIndex){
			for(var i=0; i<this.colCount; i++){
				this.contentNodes[i].appendChild(this.pageNodes[i][inPageIndex]);
			}
		},
		preparePage: function(inPageIndex, inReuseNode){
			var p = (inReuseNode ? this.popPage() : null);
			for(var i=0; i<this.colCount; i++){
				var nodes = this.pageNodes[i];
				var new_p = (p === null ? this.createPageNode() : this.invalidatePageNode(p, nodes));
				new_p.pageIndex = inPageIndex;
				nodes[inPageIndex] = new_p;
			}
		},
		// rendering implementation
		renderPage: function(inPageIndex){
			var nodes = [];
			var i, j;
			for(i=0; i<this.colCount; i++){
				nodes[i] = this.pageNodes[i][inPageIndex];
			}
			for(i=0, j=inPageIndex*this.rowsPerPage; (i<this.rowsPerPage)&&(j<this.rowCount); i++, j++){
				this.renderRow(j, nodes);
			}
		},
		removePage: function(inPageIndex){
			for(var i=0, j=inPageIndex*this.rowsPerPage; i<this.rowsPerPage; i++, j++){
				this.removeRow(j);
			}
		},
		destroyPage: function(inPageIndex){
			for(var i=0; i<this.colCount; i++){
				var n = this.invalidatePageNode(inPageIndex, this.pageNodes[i]);
				if(n){
					html.destroy(n);
				}
			}
		},
		pacify: function(inShouldPacify){
		},
		// pacification
		pacifying: false,
		pacifyTicks: 200,
		setPacifying: function(inPacifying){
			if(this.pacifying != inPacifying){
				this.pacifying = inPacifying;
				this.pacify(this.pacifying);
			}
		},
		startPacify: function(){
			this.startPacifyTicks = new Date().getTime();
		},
		doPacify: function(){
			var result = (new Date().getTime() - this.startPacifyTicks) > this.pacifyTicks;
			this.setPacifying(true);
			this.startPacify();
			return result;
		},
		endPacify: function(){
			this.setPacifying(false);
		},
		// default sizing implementation
		resize: function(){
			if(this.scrollboxNode){
				this.windowHeight = this.scrollboxNode.clientHeight;
			}
			for(var i=0; i<this.colCount; i++){
				//We want to have 1px in height min to keep scroller.  Otherwise can't scroll
				//and see header in empty grid.
				util.setStyleHeightPx(this.contentNodes[i], Math.max(1,this.height));
			}
			
			// Calculate the average row height and update the defaults (row and page).
			var needPage = (!this._invalidating);
			if(!needPage){
				var ah = this.grid.get("autoHeight");
				if(typeof ah == "number" && ah <= Math.min(this.rowsPerPage, this.rowCount)){
					needPage = true;
				}
			}
			if(needPage){
				this.needPage(this.page, this.pageTop);
			}
			var rowsOnPage = (this.page < this.pageCount - 1) ? this.rowsPerPage : ((this.rowCount % this.rowsPerPage) || this.rowsPerPage);
			var pageHeight = this.getPageHeight(this.page);
			this.averageRowHeight = (pageHeight > 0 && rowsOnPage > 0) ? (pageHeight / rowsOnPage) : 0;
		},
		calcLastPageHeight: function(){
			if(!this.pageCount){
				return 0;
			}
			var lastPage = this.pageCount - 1;
			var lastPageHeight = ((this.rowCount % this.rowsPerPage)||(this.rowsPerPage)) * this.defaultRowHeight;
			this.pageHeights[lastPage] = lastPageHeight;
			return lastPageHeight;
		},
		updateContentHeight: function(inDh){
			this.height += inDh;
			this.resize();
		},
		updatePageHeight: function(inPageIndex, fromBuild, fromAsynRendering){
			if(this.pageExists(inPageIndex)){
				var oh = this.getPageHeight(inPageIndex);
				var h = (this.measurePage(inPageIndex));
				if(h === undefined){
					h = oh;
				}
				this.pageHeights[inPageIndex] = h;
				if(oh != h){
					this.updateContentHeight(h - oh);
					var ah = this.grid.get("autoHeight");
					if((typeof ah == "number" && ah > this.rowCount)||(ah === true && !fromBuild)){
						if(!fromAsynRendering){
							this.grid.sizeChange();
						}else{//fix #11101 by using fromAsynRendering to avoid deadlock
							var ns = this.grid.viewsNode.style;
							ns.height = parseInt(ns.height) + h - oh + 'px';
							this.repositionPages(inPageIndex);
						}
					}else{
						this.repositionPages(inPageIndex);
					}
				}
				return h;
			}
			return 0;
		},
		rowHeightChanged: function(inRowIndex, fromAsynRendering){
			this.updatePageHeight(Math.floor(inRowIndex / this.rowsPerPage), false, fromAsynRendering);
		},
		// scroller core
		invalidateNodes: function(){
			while(this.stack.length){
				this.destroyPage(this.popPage());
			}
		},
		createPageNode: function(){
			var p = document.createElement('div');
			html.attr(p,"role","presentation");
			p.style.position = 'absolute';
			//p.style.width = '100%';
			p.style[this.grid.isLeftToRight() ? "left" : "right"] = '0';
			return p;
		},
		getPageHeight: function(inPageIndex){
			var ph = this.pageHeights[inPageIndex];
			return (ph !== undefined ? ph : this.defaultPageHeight);
		},
		// FIXME: this is not a stack, it's a FIFO list
		pushPage: function(inPageIndex){
			return this.stack.push(inPageIndex);
		},
		popPage: function(){
			return this.stack.shift();
		},
		findPage: function(inTop){
			var i = 0, h = 0;
			for(var ph = 0; i<this.pageCount; i++, h += ph){
				ph = this.getPageHeight(i);
				if(h + ph >= inTop){
					break;
				}
			}
			this.page = i;
			this.pageTop = h;
		},
		buildPage: function(inPageIndex, inReuseNode, inPos){
			this.preparePage(inPageIndex, inReuseNode);
			this.positionPage(inPageIndex, inPos);
			// order of operations is key below
			this.installPage(inPageIndex);
			this.renderPage(inPageIndex);
			// order of operations is key above
			this.pushPage(inPageIndex);
		},
		needPage: function(inPageIndex, inPos){
			var h = this.getPageHeight(inPageIndex), oh = h;
			if(!this.pageExists(inPageIndex)){
				this.buildPage(inPageIndex, (!this.grid._autoHeight/*fix #10543*/ && this.keepPages&&(this.stack.length >= this.keepPages)), inPos);
				h = this.updatePageHeight(inPageIndex, true);
			}else{
				this.positionPage(inPageIndex, inPos);
			}
			return h;
		},
		onscroll: function(){
			this.scroll(this.scrollboxNode.scrollTop);
		},
		scroll: function(inTop){
			this.grid.scrollTop = inTop;
			if(this.colCount){
				this.startPacify();
				this.findPage(inTop);
				var h = this.height;
				var b = this.getScrollBottom(inTop);
				for(var p=this.page, y=this.pageTop; (p<this.pageCount)&&((b<0)||(y<b)); p++){
					y += this.needPage(p, y);
				}
				this.firstVisibleRow = this.getFirstVisibleRow(this.page, this.pageTop, inTop);
				this.lastVisibleRow = this.getLastVisibleRow(p - 1, y, b);
				// indicates some page size has been updated
				if(h != this.height){
					this.repositionPages(p-1);
				}
				this.endPacify();
			}
		},
		getScrollBottom: function(inTop){
			return (this.windowHeight >= 0 ? inTop + this.windowHeight : -1);
		},
		// events
		processNodeEvent: function(e, inNode){
			var t = e.target;
			while(t && (t != inNode) && t.parentNode && (t.parentNode.parentNode != inNode)){
				t = t.parentNode;
			}
			if(!t || !t.parentNode || (t.parentNode.parentNode != inNode)){
				return false;
			}
			var page = t.parentNode;
			e.topRowIndex = page.pageIndex * this.rowsPerPage;
			e.rowIndex = e.topRowIndex + indexInParent(t);
			e.rowTarget = t;
			return true;
		},
		processEvent: function(e){
			return this.processNodeEvent(e, this.contentNode);
		},
		// virtual rendering interface
		renderRow: function(inRowIndex, inPageNode){
		},
		removeRow: function(inRowIndex){
		},
		// page node operations
		getDefaultPageNode: function(inPageIndex){
			return this.getDefaultNodes()[inPageIndex];
		},
		positionPageNode: function(inNode, inPos){
		},
		getPageNodePosition: function(inNode){
			return inNode.offsetTop;
		},
		invalidatePageNode: function(inPageIndex, inNodes){
			var p = inNodes[inPageIndex];
			if(p){
				delete inNodes[inPageIndex];
				this.removePage(inPageIndex, p);
				cleanNode(p);
				p.innerHTML = '';
			}
			return p;
		},
		// scroll control
		getPageRow: function(inPage){
			return inPage * this.rowsPerPage;
		},
		getLastPageRow: function(inPage){
			return Math.min(this.rowCount, this.getPageRow(inPage + 1)) - 1;
		},
		getFirstVisibleRow: function(inPage, inPageTop, inScrollTop){
			if(!this.pageExists(inPage)){
				return 0;
			}
			var row = this.getPageRow(inPage);
			var nodes = this.getDefaultNodes();
			var rows = divkids(nodes[inPage]);
			for(var i=0,l=rows.length; i<l && inPageTop<inScrollTop; i++, row++){
				inPageTop += rows[i].offsetHeight;
			}
			return (row ? row - 1 : row);
		},
		getLastVisibleRow: function(inPage, inBottom, inScrollBottom){
			if(!this.pageExists(inPage)){
				return 0;
			}
			var nodes = this.getDefaultNodes();
			var row = this.getLastPageRow(inPage);
			var rows = divkids(nodes[inPage]);
			for(var i=rows.length-1; i>=0 && inBottom>inScrollBottom; i--, row--){
				inBottom -= rows[i].offsetHeight;
			}
			return row + 1;
		},
		findTopRow: function(inScrollTop){
			var nodes = this.getDefaultNodes();
			var rows = divkids(nodes[this.page]);
			for(var i=0,l=rows.length,t=this.pageTop,h; i<l; i++){
				h = rows[i].offsetHeight;
				t += h;
				if(t >= inScrollTop){
					this.offset = h - (t - inScrollTop);
					return i + this.page * this.rowsPerPage;
				}
			}
			return -1;
		},
		findScrollTop: function(inRow){
			var rowPage = Math.floor(inRow / this.rowsPerPage);
			var t = 0;
			var i, l;
			for(i=0; i<rowPage; i++){
				t += this.getPageHeight(i);
			}
			this.pageTop = t;
			this.page = rowPage;//fix #10543
			this.needPage(rowPage, this.pageTop);

			var nodes = this.getDefaultNodes();
			var rows = divkids(nodes[rowPage]);
			var r = inRow - this.rowsPerPage * rowPage;
			for(i=0,l=rows.length; i<l && i<r; i++){
				t += rows[i].offsetHeight;
			}
			return t;
		},
		dummy: 0
	});
});

},
'dijit/registry':function(){
define([
	"dojo/_base/array", // array.forEach array.map
	"dojo/_base/window", // win.body
	"./main"	// dijit._scopeName
], function(array, win, dijit){

	// module:
	//		dijit/registry

	var _widgetTypeCtr = {}, hash = {};

	var registry =  {
		// summary:
		//		Registry of existing widget on page, plus some utility methods.

		// length: Number
		//		Number of registered widgets
		length: 0,

		add: function(widget){
			// summary:
			//		Add a widget to the registry. If a duplicate ID is detected, a error is thrown.
			// widget: dijit/_WidgetBase
			//		Any dijit/_WidgetBase subclass.
			if(hash[widget.id]){
				throw new Error("Tried to register widget with id==" + widget.id + " but that id is already registered");
			}
			hash[widget.id] = widget;
			this.length++;
		},

		remove: function(/*String*/ id){
			// summary:
			//		Remove a widget from the registry. Does not destroy the widget; simply
			//		removes the reference.
			if(hash[id]){
				delete hash[id];
				this.length--;
			}
		},

		byId: function(/*String|Widget*/ id){
			// summary:
			//		Find a widget by it's id.
			//		If passed a widget then just returns the widget.
			return typeof id == "string" ? hash[id] : id;	// dijit/_WidgetBase
		},

		byNode: function(/*DOMNode*/ node){
			// summary:
			//		Returns the widget corresponding to the given DOMNode
			return hash[node.getAttribute("widgetId")]; // dijit/_WidgetBase
		},

		toArray: function(){
			// summary:
			//		Convert registry into a true Array
			//
			// example:
			//		Work with the widget .domNodes in a real Array
			//		|	array.map(registry.toArray(), function(w){ return w.domNode; });

			var ar = [];
			for(var id in hash){
				ar.push(hash[id]);
			}
			return ar;	// dijit/_WidgetBase[]
		},

		getUniqueId: function(/*String*/widgetType){
			// summary:
			//		Generates a unique id for a given widgetType

			var id;
			do{
				id = widgetType + "_" +
					(widgetType in _widgetTypeCtr ?
						++_widgetTypeCtr[widgetType] : _widgetTypeCtr[widgetType] = 0);
			}while(hash[id]);
			return dijit._scopeName == "dijit" ? id : dijit._scopeName + "_" + id; // String
		},

		findWidgets: function(root, skipNode){
			// summary:
			//		Search subtree under root returning widgets found.
			//		Doesn't search for nested widgets (ie, widgets inside other widgets).
			// root: DOMNode
			//		Node to search under.
			// skipNode: DOMNode
			//		If specified, don't search beneath this node (usually containerNode).

			var outAry = [];

			function getChildrenHelper(root){
				for(var node = root.firstChild; node; node = node.nextSibling){
					if(node.nodeType == 1){
						var widgetId = node.getAttribute("widgetId");
						if(widgetId){
							var widget = hash[widgetId];
							if(widget){	// may be null on page w/multiple dojo's loaded
								outAry.push(widget);
							}
						}else if(node !== skipNode){
							getChildrenHelper(node);
						}
					}
				}
			}

			getChildrenHelper(root);
			return outAry;
		},

		_destroyAll: function(){
			// summary:
			//		Code to destroy all widgets and do other cleanup on page unload

			// Clean up focus manager lingering references to widgets and nodes
			dijit._curFocus = null;
			dijit._prevFocus = null;
			dijit._activeStack = [];

			// Destroy all the widgets, top down
			array.forEach(registry.findWidgets(win.body()), function(widget){
				// Avoid double destroy of widgets like Menu that are attached to <body>
				// even though they are logically children of other widgets.
				if(!widget._destroyed){
					if(widget.destroyRecursive){
						widget.destroyRecursive();
					}else if(widget.destroy){
						widget.destroy();
					}
				}
			});
		},

		getEnclosingWidget: function(/*DOMNode*/ node){
			// summary:
			//		Returns the widget whose DOM tree contains the specified DOMNode, or null if
			//		the node is not contained within the DOM tree of any widget
			while(node){
				var id = node.nodeType == 1 && node.getAttribute("widgetId");
				if(id){
					return hash[id];
				}
				node = node.parentNode;
			}
			return null;
		},

		// In case someone needs to access hash.
		// Actually, this is accessed from WidgetSet back-compatibility code
		_hash: hash
	};

	dijit.registry = registry;

	return registry;
});

},
'dijit/main':function(){
define([
	"dojo/_base/kernel"
], function(dojo){
	// module:
	//		dijit/main

/*=====
return {
	// summary:
	//		The dijit package main module.
	//		Deprecated.   Users should access individual modules (ex: dijit/registry) directly.
};
=====*/

	return dojo.dijit;
});

},
'dojox/grid/util':function(){
define([
	"../main",
	"dojo/_base/lang",
	"dojo/dom",
	"dojo/_base/sniff"
], function(dojox, lang, dom, has){

	var dgu = lang.getObject("grid.util", true, dojox);

/*=====
dgu = {
	// summary:
	//		grid utility library
};
=====*/

	dgu.na = '...';
	dgu.rowIndexTag = "gridRowIndex";
	dgu.gridViewTag = "gridView";

	dgu.fire = function(ob, ev, args){
		// Find parent node that scrolls, either vertically or horizontally
		function getScrollParent(node, horizontal){
			if(node == null) {
				return null;
			}

			var dimension = horizontal ? 'Width' : 'Height';
			if(node['scroll' + dimension] > node['client' + dimension]){
				return node;
			}else{
				return getScrollParent(node.parentNode, horizontal);
			}
		}

		// In Webkit browsers focusing an element will scroll this element into view.
		// This may even happen if the element already is in view, but near the edge.
		// This may move the element away from the mouse cursor on the first click
		// of a double click and you end up hitting a different element.
		// Avoid this by storing the scroll position and restoring it after focusing.
		var verticalScrollParent, horizontalScrollParent, scrollTop, scrollLeft, obNode;
		if(has("webkit") && (ev == "focus")){
			obNode = ob.domNode ? ob.domNode : ob;
			verticalScrollParent = getScrollParent(obNode, false);
			if(verticalScrollParent){
				scrollTop = verticalScrollParent.scrollTop;
			}
			horizontalScrollParent = getScrollParent(obNode, true);
			if(horizontalScrollParent){
				scrollLeft = horizontalScrollParent.scrollLeft;
			}
		}

		var fn = ob && ev && ob[ev];
		var result = fn && (args ? fn.apply(ob, args) : ob[ev]());

		// Restore scrolling position
		if(has("webkit") && (ev == "focus")){
			if(verticalScrollParent){
				verticalScrollParent.scrollTop = scrollTop;
			}
			if(horizontalScrollParent){
				horizontalScrollParent.scrollLeft = scrollLeft;
			}
		}

		return result;
	};

	dgu.setStyleHeightPx = function(inElement, inHeight){
		if(inHeight >= 0){
			var s = inElement.style;
			var v = inHeight + 'px';
			if(inElement && s['height'] != v){
				s['height'] = v;
			}
		}
	};

	dgu.mouseEvents = [ 'mouseover', 'mouseout', /*'mousemove',*/ 'mousedown', 'mouseup', 'click', 'dblclick', 'contextmenu' ];

	dgu.keyEvents = [ 'keyup', 'keydown', 'keypress' ];

	dgu.funnelEvents = function(inNode, inObject, inMethod, inEvents){
		var evts = (inEvents ? inEvents : dgu.mouseEvents.concat(dgu.keyEvents));
		for (var i=0, l=evts.length; i<l; i++){
			inObject.connect(inNode, 'on' + evts[i], inMethod);
		}
	};

	dgu.removeNode = function(inNode){
		inNode = dom.byId(inNode);
		inNode && inNode.parentNode && inNode.parentNode.removeChild(inNode);
		return inNode;
	};

	dgu.arrayCompare = function(inA, inB){
		for(var i=0,l=inA.length; i<l; i++){
			if(inA[i] != inB[i]){return false;}
		}
		return (inA.length == inB.length);
	};

	dgu.arrayInsert = function(inArray, inIndex, inValue){
		if(inArray.length <= inIndex){
			inArray[inIndex] = inValue;
		}else{
			inArray.splice(inIndex, 0, inValue);
		}
	};

	dgu.arrayRemove = function(inArray, inIndex){
		inArray.splice(inIndex, 1);
	};

	dgu.arraySwap = function(inArray, inI, inJ){
		var cache = inArray[inI];
		inArray[inI] = inArray[inJ];
		inArray[inJ] = cache;
	};

	return dgu;

});

},
'dojox/grid/_Layout':function(){
define([
	"dojo/_base/kernel",
	"../main",
	"dojo/_base/declare",
	"dojo/_base/array",
	"dojo/_base/lang",
	"dojo/dom-geometry",
	"./cells",
	"./_RowSelector"
], function(dojo, dojox, declare, array, lang, domGeometry){

return declare("dojox.grid._Layout", null, {
	// summary:
	//	Controls grid cell layout. Owned by grid and used internally.
	constructor: function(inGrid){
		this.grid = inGrid;
	},
	// flat array of grid cells
	cells: [],
	// structured array of grid cells
	structure: null,
	// default cell width
	defaultWidth: '6em',

	// methods
	moveColumn: function(sourceViewIndex, destViewIndex, cellIndex, targetIndex, before){
		var source_cells = this.structure[sourceViewIndex].cells[0];
		var dest_cells = this.structure[destViewIndex].cells[0];

		var cell = null;
		var cell_ri = 0;
		var target_ri = 0;

		for(var i=0, c; c=source_cells[i]; i++){
			if(c.index == cellIndex){
				cell_ri = i;
				break;
			}
		}
		cell = source_cells.splice(cell_ri, 1)[0];
		cell.view = this.grid.views.views[destViewIndex];

		for(i=0, c=null; c=dest_cells[i]; i++){
			if(c.index == targetIndex){
				target_ri = i;
				break;
			}
		}
		if(!before){
			target_ri += 1;
		}
		dest_cells.splice(target_ri, 0, cell);

		var sortedCell = this.grid.getCell(this.grid.getSortIndex());
		if(sortedCell){
			sortedCell._currentlySorted = this.grid.getSortAsc();
		}

		this.cells = [];
		cellIndex = 0;
		var v;
		for(i=0; v=this.structure[i]; i++){
			for(var j=0, cs; cs=v.cells[j]; j++){
				for(var k=0; c=cs[k]; k++){
					c.index = cellIndex;
					this.cells.push(c);
					if("_currentlySorted" in c){
						var si = cellIndex + 1;
						si *= c._currentlySorted ? 1 : -1;
						this.grid.sortInfo = si;
						delete c._currentlySorted;
					}
					cellIndex++;
				}
			}
		}

		//Fix #9481 - reset idx in cell markup
		array.forEach(this.cells, function(c){
			var marks = c.markup[2].split(" ");
			var oldIdx = parseInt(marks[1].substring(5));//get old "idx"
			if(oldIdx != c.index){
				marks[1] = "idx=\"" + c.index + "\"";
				c.markup[2] = marks.join(" ");
			}
		});

		this.grid.setupHeaderMenu();
		//this.grid.renderOnIdle();
	},

	setColumnVisibility: function(columnIndex, visible){
		var cell = this.cells[columnIndex];
		if(cell.hidden == visible){
			cell.hidden = !visible;
			var v = cell.view, w = v.viewWidth;
			if(w && w != "auto"){
				v._togglingColumn = domGeometry.getMarginBox(cell.getHeaderNode()).w || 0;
			}
			v.update();
			return true;
		}else{
			return false;
		}
	},

	addCellDef: function(inRowIndex, inCellIndex, inDef){
		var self = this;
		var getCellWidth = function(inDef){
			var w = 0;
			if(inDef.colSpan > 1){
				w = 0;
			}else{
				w = inDef.width || self._defaultCellProps.width || self.defaultWidth;

				if(!isNaN(w)){
					w = w + "em";
				}
			}
			return w;
		};

		var props = {
			grid: this.grid,
			subrow: inRowIndex,
			layoutIndex: inCellIndex,
			index: this.cells.length
		};

		if(inDef && inDef instanceof dojox.grid.cells._Base){
			var new_cell = lang.clone(inDef);
			props.unitWidth = getCellWidth(new_cell._props);
			new_cell = lang.mixin(new_cell, this._defaultCellProps, inDef._props, props);
			return new_cell;
		}

		var cell_type = inDef.type || inDef.cellType || this._defaultCellProps.type || this._defaultCellProps.cellType || dojox.grid.cells.Cell;
		if(lang.isString(cell_type)){
			cell_type = lang.getObject(cell_type);
		}

		props.unitWidth = getCellWidth(inDef);
		return new cell_type(lang.mixin({}, this._defaultCellProps, inDef, props));
	},

	addRowDef: function(inRowIndex, inDef){
		var result = [];
		var relSum = 0, pctSum = 0, doRel = true;
		for(var i=0, def, cell; (def=inDef[i]); i++){
			cell = this.addCellDef(inRowIndex, i, def);
			result.push(cell);
			this.cells.push(cell);
			// Check and calculate the sum of all relative widths
			if(doRel && cell.relWidth){
				relSum += cell.relWidth;
			}else if(cell.width){
				var w = cell.width;
				if(typeof w == "string" && w.slice(-1) == "%"){
					pctSum += window.parseInt(w, 10);
				}else if(w == "auto"){
					// relative widths doesn't play nice with auto - since we
					// don't have a way of knowing how much space the auto is
					// supposed to take up.
					doRel = false;
				}
			}
		}
		if(relSum && doRel){
			// We have some kind of relWidths specified - so change them to %
			array.forEach(result, function(cell){
				if(cell.relWidth){
					cell.width = cell.unitWidth = ((cell.relWidth / relSum) * (100 - pctSum)) + "%";
				}
			});
		}
		return result;

	},

	addRowsDef: function(inDef){
		var result = [];
		if(lang.isArray(inDef)){
            // inDef[0] could be a NodeList if the Grid is defined in a declarative way.
            // lang.isArray() does not recognize a NodeList as an array, now so the wrong path will be chosen.
            // lang.isArrayLike() does the right match against a NodeList, instead.
			if(lang.isArrayLike(inDef[0])){
				for(var i=0, row; inDef && (row=inDef[i]); i++){
					result.push(this.addRowDef(i, row));
				}
			}else{
				result.push(this.addRowDef(0, inDef));
			}
		}
		return result;
	},

	addViewDef: function(inDef){
		this._defaultCellProps = inDef.defaultCell || {};
		if(inDef.width && inDef.width == "auto"){
			delete inDef.width;
		}
		return lang.mixin({}, inDef, {cells: this.addRowsDef(inDef.rows || inDef.cells)});
	},

	setStructure: function(inStructure){
		this.fieldIndex = 0;
		this.cells = [];
		var s = this.structure = [];

		if(this.grid.rowSelector){
			var sel = { type: dojox._scopeName + ".grid._RowSelector" };

			if(lang.isString(this.grid.rowSelector)){
				var width = this.grid.rowSelector;

				if(width == "false"){
					sel = null;
				}else if(width != "true"){
					sel['width'] = width;
				}
			}else{
				if(!this.grid.rowSelector){
					sel = null;
				}
			}

			if(sel){
				s.push(this.addViewDef(sel));
			}
		}

		var isCell = function(def){
			return ("name" in def || "field" in def || "get" in def);
		};

		var isRowDef = function(def){
			if(lang.isArray(def)){
				if(lang.isArray(def[0]) || isCell(def[0])){
					return true;
				}
			}
			return false;
		};

		var isView = function(def){
			return (def !== null && lang.isObject(def) &&
					("cells" in def || "rows" in def || ("type" in def && !isCell(def))));
		};

		if(lang.isArrayLike(inStructure)){
			var hasViews = false;
			for(var i=0, st; (st=inStructure[i]); i++){
				if(isView(st)){
					hasViews = true;
					break;
				}
			}
			if(!hasViews){
				s.push(this.addViewDef({ cells: inStructure }));
			}else{
				for(i=0; (st=inStructure[i]); i++){
					if(isRowDef(st)){
						s.push(this.addViewDef({ cells: st }));
					}else if(isView(st)){
						s.push(this.addViewDef(st));
					}
				}
			}
		}else if(isView(inStructure)){
			// it's a view object
			s.push(this.addViewDef(inStructure));
		}

		this.cellCount = this.cells.length;
		this.grid.setupHeaderMenu();
	}
});
});
},
'dojox/grid/cells':function(){
define(["../main", "./cells/_base"], function(dojox){
	return dojox.grid.cells;
});
},
'dojox/grid/cells/_base':function(){
define([
	"dojo/_base/kernel",
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/_base/event",
	"dojo/_base/connect",
	"dojo/_base/array",
	"dojo/_base/sniff",
	"dojo/dom",
	"dojo/dom-attr",
	"dojo/dom-construct",
	"dijit/_Widget",
	"../util"
], function(dojo, declare, lang, event, connect, array, has, dom, domAttr, domConstruct, _Widget, util){

	var _DeferredTextWidget = declare("dojox.grid._DeferredTextWidget", _Widget, {
		deferred: null,
		_destroyOnRemove: true,
		postCreate: function(){
			if(this.deferred){
				this.deferred.addBoth(lang.hitch(this, function(text){
					if(this.domNode){
						this.domNode.innerHTML = text;
					}
				}));
			}
		}
	});

	var focusSelectNode = function(inNode){
		try{
			util.fire(inNode, "focus");
			util.fire(inNode, "select");
		}catch(e){// IE sux bad
		}
	};
	
	var whenIdle = function(/*inContext, inMethod, args ...*/){
		setTimeout(lang.hitch.apply(dojo, arguments), 0);
	};

	var BaseCell = declare("dojox.grid.cells._Base", null, {
		// summary:
		//		Represents a grid cell and contains information about column options and methods
		//		for retrieving cell related information.
		//		Each column in a grid layout has a cell object and most events and many methods
		//		provide access to these objects.
		styles: '',
		classes: '',
		editable: false,
		alwaysEditing: false,
		formatter: null,
		defaultValue: '...',
		value: null,
		hidden: false,
		noresize: false,
		draggable: true,
		//private
		_valueProp: "value",
		_formatPending: false,

		constructor: function(inProps){
			this._props = inProps || {};
			lang.mixin(this, inProps);
			if(this.draggable === undefined){
				this.draggable = true;
			}
		},

		_defaultFormat: function(inValue, callArgs){
			var s = this.grid.formatterScope || this;
			var f = this.formatter;
			if(f && s && typeof f == "string"){
				f = this.formatter = s[f];
			}
			var v = (inValue != this.defaultValue && f) ? f.apply(s, callArgs) : inValue;
			if(typeof v == "undefined"){
				return this.defaultValue;
			}
			if(v && v.addBoth){
				// Check if it's a deferred
				v = new _DeferredTextWidget({deferred: v},
									domConstruct.create("span", {innerHTML: this.defaultValue}));
			}
			if(v && v.declaredClass && v.startup){
				return "<div class='dojoxGridStubNode' linkWidget='" +
						v.id +
						"' cellIdx='" +
						this.index +
						"'>" +
						this.defaultValue +
						"</div>";
			}
			return v;
		},
		
		// data source
		format: function(inRowIndex, inItem){
			// summary:
			//		provides the html for a given grid cell.
			// inRowIndex: int
			//		grid row index
			// returns:
			//		html for a given grid cell
			var i = this.grid.edit.info;
			var d = this.get ? this.get(inRowIndex, inItem) : (this.value || this.defaultValue);
			if (d && d.replace && this.grid.escapeHTMLInData) {
				d = d.replace(/&/g, '&amp;').replace(/</g, '&lt;');
			}
			if (this.editable && (this.alwaysEditing || (i.rowIndex==inRowIndex && i.cell==this))){
				return this.formatEditing(i.value ? i.value : d, inRowIndex);
			} else {
				return this._defaultFormat(d, [d, inRowIndex, this]);
			}
		},
		formatEditing: function(inDatum, inRowIndex){
			// summary:
			//		formats the cell for editing
			// inDatum: anything
			//		cell data to edit
			// inRowIndex: int
			//		grid row index
			// returns:
			//		string of html to place in grid cell
		},
		// utility
		getNode: function(inRowIndex){
			// summary:
			//		gets the dom node for a given grid cell.
			// inRowIndex: int
			//		grid row index
			// returns:
			//		dom node for a given grid cell
			return this.view.getCellNode(inRowIndex, this.index);
		},
		getHeaderNode: function(){
			return this.view.getHeaderCellNode(this.index);
		},
		getEditNode: function(inRowIndex){
			return (this.getNode(inRowIndex) || 0).firstChild || 0;
		},
		canResize: function(){
			var uw = this.unitWidth;
			return uw && (uw!=='auto');
		},
		isFlex: function(){
			var uw = this.unitWidth;
			return uw && lang.isString(uw) && (uw=='auto' || uw.slice(-1)=='%');
		},
		// edit support
		applyEdit: function(inValue, inRowIndex){
			if(this.getNode(inRowIndex)){
				this.grid.edit.applyCellEdit(inValue, this, inRowIndex);
			}
		},
		cancelEdit: function(inRowIndex){
			this.grid.doCancelEdit(inRowIndex);
		},
		_onEditBlur: function(inRowIndex){
			if(this.grid.edit.isEditCell(inRowIndex, this.index)){
				//console.log('editor onblur', e);
				this.grid.edit.apply();
			}
		},
		registerOnBlur: function(inNode, inRowIndex){
			if(this.commitOnBlur){
				connect.connect(inNode, "onblur", function(e){
					// hack: if editor still thinks this editor is current some ms after it blurs, assume we've focused away from grid
					setTimeout(lang.hitch(this, "_onEditBlur", inRowIndex), 250);
				});
			}
		},
		//protected
		needFormatNode: function(inDatum, inRowIndex){
			this._formatPending = true;
			whenIdle(this, "_formatNode", inDatum, inRowIndex);
		},
		cancelFormatNode: function(){
			this._formatPending = false;
		},
		//private
		_formatNode: function(inDatum, inRowIndex){
			if(this._formatPending){
				this._formatPending = false;
				// make cell selectable
				if(!has('ie')){
					dom.setSelectable(this.grid.domNode, true);
				}
				this.formatNode(this.getEditNode(inRowIndex), inDatum, inRowIndex);
			}
		},
		//protected
		formatNode: function(inNode, inDatum, inRowIndex){
			// summary:
			//		format the editing dom node. Use when editor is a widget.
			// inNode: dom node
			//		dom node for the editor
			// inDatum: anything
			//		cell data to edit
			// inRowIndex: int
			//		grid row index
			if(has('ie')){
				// IE sux bad
				whenIdle(this, "focus", inRowIndex, inNode);
			}else{
				this.focus(inRowIndex, inNode);
			}
		},
		dispatchEvent: function(m, e){
			if(m in this){
				return this[m](e);
			}
		},
		//public
		getValue: function(inRowIndex){
			// summary:
			//		returns value entered into editor
			// inRowIndex: int
			//		grid row index
			// returns:
			//		value of editor
			return this.getEditNode(inRowIndex)[this._valueProp];
		},
		setValue: function(inRowIndex, inValue){
			// summary:
			//		set the value of the grid editor
			// inRowIndex: int
			//		grid row index
			// inValue: anything
			//		value of editor
			var n = this.getEditNode(inRowIndex);
			if(n){
				n[this._valueProp] = inValue;
			}
		},
		focus: function(inRowIndex, inNode){
			// summary:
			//		focus the grid editor
			// inRowIndex: int
			//		grid row index
			// inNode: dom node
			//		editor node
			focusSelectNode(inNode || this.getEditNode(inRowIndex));
		},
		save: function(inRowIndex){
			// summary:
			//		save editor state
			// inRowIndex: int
			//		grid row index
			this.value = this.value || this.getValue(inRowIndex);
			//console.log("save", this.value, inCell.index, inRowIndex);
		},
		restore: function(inRowIndex){
			// summary:
			//		restore editor state
			// inRowIndex: int
			//		grid row index
			this.setValue(inRowIndex, this.value);
			//console.log("restore", this.value, inCell.index, inRowIndex);
		},
		//protected
		_finish: function(inRowIndex){
			// summary:
			//		called when editing is completed to clean up editor
			// inRowIndex: int
			//		grid row index
			dom.setSelectable(this.grid.domNode, false);
			this.cancelFormatNode();
		},
		//public
		apply: function(inRowIndex){
			// summary:
			//		apply edit from cell editor
			// inRowIndex: int
			//		grid row index
			this.applyEdit(this.getValue(inRowIndex), inRowIndex);
			this._finish(inRowIndex);
		},
		cancel: function(inRowIndex){
			// summary:
			//		cancel cell edit
			// inRowIndex: int
			//		grid row index
			this.cancelEdit(inRowIndex);
			this._finish(inRowIndex);
		}
	});
	BaseCell.markupFactory = function(node, cellDef){
		var formatter = lang.trim(domAttr.get(node, "formatter")||"");
		if(formatter){
			cellDef.formatter = lang.getObject(formatter)||formatter;
		}
		var get = lang.trim(domAttr.get(node, "get")||"");
		if(get){
			cellDef.get = lang.getObject(get);
		}
		var getBoolAttr = function(attr, cell, cellAttr){
			var value = lang.trim(domAttr.get(node, attr)||"");
			if(value){ cell[cellAttr||attr] = !(value.toLowerCase()=="false"); }
		};
		getBoolAttr("sortDesc", cellDef);
		getBoolAttr("editable", cellDef);
		getBoolAttr("alwaysEditing", cellDef);
		getBoolAttr("noresize", cellDef);
		getBoolAttr("draggable", cellDef);

		var value = lang.trim(domAttr.get(node, "loadingText")||domAttr.get(node, "defaultValue")||"");
		if(value){
			cellDef.defaultValue = value;
		}

		var getStrAttr = function(attr, cell, cellAttr){
			var value = lang.trim(domAttr.get(node, attr)||"")||undefined;
			if(value){ cell[cellAttr||attr] = value; }
		};
		getStrAttr("styles", cellDef);
		getStrAttr("headerStyles", cellDef);
		getStrAttr("cellStyles", cellDef);
		getStrAttr("classes", cellDef);
		getStrAttr("headerClasses", cellDef);
		getStrAttr("cellClasses", cellDef);
	};

	var Cell = BaseCell.Cell = declare("dojox.grid.cells.Cell", BaseCell, {
		// summary:
		//		grid cell that provides a standard text input box upon editing
		constructor: function(){
			this.keyFilter = this.keyFilter;
		},
		// keyFilter: RegExp
		//		optional regex for disallowing keypresses
		keyFilter: null,
		formatEditing: function(inDatum, inRowIndex){
			this.needFormatNode(inDatum, inRowIndex);
			if (inDatum && inDatum.replace) {
				// escape quotes to avoid XSS
				inDatum = inDatum.replace(/"/g, '&quot;')
			}
			return '<input class="dojoxGridInput" type="text" value="' + inDatum + '">';
		},
		formatNode: function(inNode, inDatum, inRowIndex){
			this.inherited(arguments);
			// FIXME: feels too specific for this interface
			this.registerOnBlur(inNode, inRowIndex);
		},
		doKey: function(e){
			if(this.keyFilter){
				var key = String.fromCharCode(e.charCode);
				if(key.search(this.keyFilter) == -1){
					event.stop(e);
				}
			}
		},
		_finish: function(inRowIndex){
			this.inherited(arguments);
			var n = this.getEditNode(inRowIndex);
			try{
				util.fire(n, "blur");
			}catch(e){}
		}
	});
	Cell.markupFactory = function(node, cellDef){
		BaseCell.markupFactory(node, cellDef);
		var keyFilter = lang.trim(domAttr.get(node, "keyFilter")||"");
		if(keyFilter){
			cellDef.keyFilter = new RegExp(keyFilter);
		}
	};

	var RowIndex = BaseCell.RowIndex = declare("dojox.grid.cells.RowIndex", Cell, {
		name: 'Row',

		postscript: function(){
			this.editable = false;
		},
		get: function(inRowIndex){
			return inRowIndex + 1;
		}
	});
	RowIndex.markupFactory = function(node, cellDef){
		Cell.markupFactory(node, cellDef);
	};

	var Select = BaseCell.Select = declare("dojox.grid.cells.Select", Cell, {
		// summary:
		//		grid cell that provides a standard select for editing

		// options: Array
		//		text of each item
		options: null,

		// values: Array
		//		value for each item
		values: null,

		// returnIndex: Integer
		//		editor returns only the index of the selected option and not the value
		returnIndex: -1,

		constructor: function(inCell){
			this.values = this.values || this.options;
		},
		formatEditing: function(inDatum, inRowIndex){
			this.needFormatNode(inDatum, inRowIndex);
			var h = [ '<select class="dojoxGridSelect">' ];
			for (var i=0, o, v; ((o=this.options[i]) !== undefined)&&((v=this.values[i]) !== undefined); i++){
				v = v.replace ? v.replace(/&/g, '&amp;').replace(/</g, '&lt;') : v;
				o = o.replace ? o.replace(/&/g, '&amp;').replace(/</g, '&lt;') : o;
				h.push("<option", (inDatum==v ? ' selected' : ''), ' value="' + v + '"', ">", o, "</option>");
			}
			h.push('</select>');
			return h.join('');
		},
		_defaultFormat: function(inValue, callArgs){
			var v = this.inherited(arguments);
			// when 'values' and 'options' both provided and there is no cutomized formatter,
			// then we use 'options' as label in order to be consistent
			if(!this.formatter && this.values && this.options){
				var i = array.indexOf(this.values, v);
				if(i >= 0){
					v = this.options[i];
				}
			}
			return v;
		},
		getValue: function(inRowIndex){
			var n = this.getEditNode(inRowIndex);
			if(n){
				var i = n.selectedIndex, o = n.options[i];
				return this.returnIndex > -1 ? i : o.value || o.innerHTML;
			}
		}
	});
	Select.markupFactory = function(node, cell){
		Cell.markupFactory(node, cell);
		var options = lang.trim(domAttr.get(node, "options")||"");
		if(options){
			var o = options.split(',');
			if(o[0] != options){
				cell.options = o;
			}
		}
		var values = lang.trim(domAttr.get(node, "values")||"");
		if(values){
			var v = values.split(',');
			if(v[0] != values){
				cell.values = v;
			}
		}
	};

	var AlwaysEdit = BaseCell.AlwaysEdit = declare("dojox.grid.cells.AlwaysEdit", Cell, {
		// summary:
		//		grid cell that is always in an editable state, regardless of grid editing state
		alwaysEditing: true,
		_formatNode: function(inDatum, inRowIndex){
			this.formatNode(this.getEditNode(inRowIndex), inDatum, inRowIndex);
		},
		applyStaticValue: function(inRowIndex){
			var e = this.grid.edit;
			e.applyCellEdit(this.getValue(inRowIndex), this, inRowIndex);
			e.start(this, inRowIndex, true);
		}
	});
	AlwaysEdit.markupFactory = function(node, cell){
		Cell.markupFactory(node, cell);
	};

	var Bool = BaseCell.Bool = declare("dojox.grid.cells.Bool", AlwaysEdit, {
		// summary:
		//		grid cell that provides a standard checkbox that is always on for editing
		_valueProp: "checked",
		formatEditing: function(inDatum, inRowIndex){
			return '<input class="dojoxGridInput" type="checkbox"' + (inDatum ? ' checked="checked"' : '') + ' style="width: auto" />';
		},
		doclick: function(e){
			if(e.target.tagName == 'INPUT'){
				this.applyStaticValue(e.rowIndex);
			}
		}
	});
	Bool.markupFactory = function(node, cell){
		AlwaysEdit.markupFactory(node, cell);
	};

	return BaseCell;

});

},
'dijit/_Widget':function(){
define([
	"dojo/aspect",	// aspect.around
	"dojo/_base/config",	// config.isDebug
	"dojo/_base/connect",	// connect.connect
	"dojo/_base/declare", // declare
	"dojo/has",
	"dojo/_base/kernel", // kernel.deprecated
	"dojo/_base/lang", // lang.hitch
	"dojo/query",
	"dojo/ready",
	"./registry",	// registry.byNode
	"./_WidgetBase",
	"./_OnDijitClickMixin",
	"./_FocusMixin",
	"dojo/uacss",		// browser sniffing (included for back-compat; subclasses may be using)
	"./hccss"		// high contrast mode sniffing (included to set CSS classes on <body>, module ret value unused)
], function(aspect, config, connect, declare, has, kernel, lang, query, ready,
			registry, _WidgetBase, _OnDijitClickMixin, _FocusMixin){


// module:
//		dijit/_Widget


function connectToDomNode(){
	// summary:
	//		If user connects to a widget method === this function, then they will
	//		instead actually be connecting the equivalent event on this.domNode
}

// Trap dojo.connect() calls to connectToDomNode methods, and redirect to _Widget.on()
function aroundAdvice(originalConnect){
	return function(obj, event, scope, method){
		if(obj && typeof event == "string" && obj[event] == connectToDomNode){
			return obj.on(event.substring(2).toLowerCase(), lang.hitch(scope, method));
		}
		return originalConnect.apply(connect, arguments);
	};
}
aspect.around(connect, "connect", aroundAdvice);
if(kernel.connect){
	aspect.around(kernel, "connect", aroundAdvice);
}

var _Widget = declare("dijit._Widget", [_WidgetBase, _OnDijitClickMixin, _FocusMixin], {
	// summary:
	//		Old base class for widgets.   New widgets should extend `dijit/_WidgetBase` instead
	// description:
	//		Old Base class for Dijit widgets.
	//
	//		Extends _WidgetBase, adding support for:
	//
	//		- declaratively/programatically specifying widget initialization parameters like
	//			onMouseMove="foo" that call foo when this.domNode gets a mousemove event
	//		- ondijitclick:
	//			Support new data-dojo-attach-event="ondijitclick: ..." that is triggered by a mouse click or a SPACE/ENTER keypress
	//		- focus related functions:
	//			In particular, the onFocus()/onBlur() callbacks.   Driven internally by
	//			dijit/_base/focus.js.
	//		- deprecated methods
	//		- onShow(), onHide(), onClose()
	//
	//		Also, by loading code in dijit/_base, turns on:
	//
	//		- browser sniffing (putting browser class like `dj_ie` on `<html>` node)
	//		- high contrast mode sniffing (add `dijit_a11y` class to `<body>` if machine is in high contrast mode)


	////////////////// DEFERRED CONNECTS ///////////////////

	onClick: connectToDomNode,
	/*=====
	onClick: function(event){
		// summary:
		//		Connect to this function to receive notifications of mouse click events.
		// event:
		//		mouse Event
		// tags:
		//		callback
	},
	=====*/
	onDblClick: connectToDomNode,
	/*=====
	onDblClick: function(event){
		// summary:
		//		Connect to this function to receive notifications of mouse double click events.
		// event:
		//		mouse Event
		// tags:
		//		callback
	},
	=====*/
	onKeyDown: connectToDomNode,
	/*=====
	onKeyDown: function(event){
		// summary:
		//		Connect to this function to receive notifications of keys being pressed down.
		// event:
		//		key Event
		// tags:
		//		callback
	},
	=====*/
	onKeyPress: connectToDomNode,
	/*=====
	onKeyPress: function(event){
		// summary:
		//		Connect to this function to receive notifications of printable keys being typed.
		// event:
		//		key Event
		// tags:
		//		callback
	},
	=====*/
	onKeyUp: connectToDomNode,
	/*=====
	onKeyUp: function(event){
		// summary:
		//		Connect to this function to receive notifications of keys being released.
		// event:
		//		key Event
		// tags:
		//		callback
	},
	=====*/
	onMouseDown: connectToDomNode,
	/*=====
	onMouseDown: function(event){
		// summary:
		//		Connect to this function to receive notifications of when the mouse button is pressed down.
		// event:
		//		mouse Event
		// tags:
		//		callback
	},
	=====*/
	onMouseMove: connectToDomNode,
	/*=====
	onMouseMove: function(event){
		// summary:
		//		Connect to this function to receive notifications of when the mouse moves over nodes contained within this widget.
		// event:
		//		mouse Event
		// tags:
		//		callback
	},
	=====*/
	onMouseOut: connectToDomNode,
	/*=====
	onMouseOut: function(event){
		// summary:
		//		Connect to this function to receive notifications of when the mouse moves off of nodes contained within this widget.
		// event:
		//		mouse Event
		// tags:
		//		callback
	},
	=====*/
	onMouseOver: connectToDomNode,
	/*=====
	onMouseOver: function(event){
		// summary:
		//		Connect to this function to receive notifications of when the mouse moves onto nodes contained within this widget.
		// event:
		//		mouse Event
		// tags:
		//		callback
	},
	=====*/
	onMouseLeave: connectToDomNode,
	/*=====
	onMouseLeave: function(event){
		// summary:
		//		Connect to this function to receive notifications of when the mouse moves off of this widget.
		// event:
		//		mouse Event
		// tags:
		//		callback
	},
	=====*/
	onMouseEnter: connectToDomNode,
	/*=====
	onMouseEnter: function(event){
		// summary:
		//		Connect to this function to receive notifications of when the mouse moves onto this widget.
		// event:
		//		mouse Event
		// tags:
		//		callback
	},
	=====*/
	onMouseUp: connectToDomNode,
	/*=====
	onMouseUp: function(event){
		// summary:
		//		Connect to this function to receive notifications of when the mouse button is released.
		// event:
		//		mouse Event
		// tags:
		//		callback
	},
	=====*/

	constructor: function(params /*===== ,srcNodeRef =====*/){
		// summary:
		//		Create the widget.
		// params: Object|null
		//		Hash of initialization parameters for widget, including scalar values (like title, duration etc.)
		//		and functions, typically callbacks like onClick.
		//		The hash can contain any of the widget's properties, excluding read-only properties.
		// srcNodeRef: DOMNode|String?
		//		If a srcNodeRef (DOM node) is specified:
		//
		//		- use srcNodeRef.innerHTML as my contents
		//		- if this is a behavioral widget then apply behavior to that srcNodeRef
		//		- otherwise, replace srcNodeRef with my generated DOM tree

		// extract parameters like onMouseMove that should connect directly to this.domNode
		this._toConnect = {};
		for(var name in params){
			if(this[name] === connectToDomNode){
				this._toConnect[name.replace(/^on/, "").toLowerCase()] = params[name];
				delete params[name];
			}
		}
	},

	postCreate: function(){
		this.inherited(arguments);

		// perform connection from this.domNode to user specified handlers (ex: onMouseMove)
		for(var name in this._toConnect){
			this.on(name, this._toConnect[name]);
		}
		delete this._toConnect;
	},

	on: function(/*String|Function*/ type, /*Function*/ func){
		if(this[this._onMap(type)] === connectToDomNode){
			// Use connect.connect() rather than on() to get handling for "onmouseenter" on non-IE,
			// normalization of onkeypress/onkeydown to behave like firefox, etc.
			// Also, need to specify context as "this" rather than the default context of the DOMNode
			// Remove in 2.0.
			return connect.connect(this.domNode, type.toLowerCase(), this, func);
		}
		return this.inherited(arguments);
	},

	_setFocusedAttr: function(val){
		// Remove this method in 2.0 (or sooner), just here to set _focused == focused, for back compat
		// (but since it's a private variable we aren't required to keep supporting it).
		this._focused = val;
		this._set("focused", val);
	},

	////////////////// DEPRECATED METHODS ///////////////////

	setAttribute: function(/*String*/ attr, /*anything*/ value){
		// summary:
		//		Deprecated.  Use set() instead.
		// tags:
		//		deprecated
		kernel.deprecated(this.declaredClass+"::setAttribute(attr, value) is deprecated. Use set() instead.", "", "2.0");
		this.set(attr, value);
	},

	attr: function(/*String|Object*/name, /*Object?*/value){
		// summary:
		//		This method is deprecated, use get() or set() directly.
		// name:
		//		The property to get or set. If an object is passed here and not
		//		a string, its keys are used as names of attributes to be set
		//		and the value of the object as values to set in the widget.
		// value:
		//		Optional. If provided, attr() operates as a setter. If omitted,
		//		the current value of the named property is returned.
		// tags:
		//		deprecated

		var args = arguments.length;
		if(args >= 2 || typeof name === "object"){ // setter
			return this.set.apply(this, arguments);
		}else{ // getter
			return this.get(name);
		}
	},

	getDescendants: function(){
		// summary:
		//		Returns all the widgets contained by this, i.e., all widgets underneath this.containerNode.
		//		This method should generally be avoided as it returns widgets declared in templates, which are
		//		supposed to be internal/hidden, but it's left here for back-compat reasons.

		kernel.deprecated(this.declaredClass+"::getDescendants() is deprecated. Use getChildren() instead.", "", "2.0");
		return this.containerNode ? query('[widgetId]', this.containerNode).map(registry.byNode) : []; // dijit/_WidgetBase[]
	},

	////////////////// MISCELLANEOUS METHODS ///////////////////

	_onShow: function(){
		// summary:
		//		Internal method called when this widget is made visible.
		//		See `onShow` for details.
		this.onShow();
	},

	onShow: function(){
		// summary:
		//		Called when this widget becomes the selected pane in a
		//		`dijit/layout/TabContainer`, `dijit/layout/StackContainer`,
		//		`dijit/layout/AccordionContainer`, etc.
		//
		//		Also called to indicate display of a `dijit.Dialog`, `dijit.TooltipDialog`, or `dijit.TitlePane`.
		// tags:
		//		callback
	},

	onHide: function(){
		// summary:
		//		Called when another widget becomes the selected pane in a
		//		`dijit/layout/TabContainer`, `dijit/layout/StackContainer`,
		//		`dijit/layout/AccordionContainer`, etc.
		//
		//		Also called to indicate hide of a `dijit.Dialog`, `dijit.TooltipDialog`, or `dijit.TitlePane`.
		// tags:
		//		callback
	},

	onClose: function(){
		// summary:
		//		Called when this widget is being displayed as a popup (ex: a Calendar popped
		//		up from a DateTextBox), and it is hidden.
		//		This is called from the dijit.popup code, and should not be called directly.
		//
		//		Also used as a parameter for children of `dijit/layout/StackContainer` or subclasses.
		//		Callback if a user tries to close the child.   Child will be closed if this function returns true.
		// tags:
		//		extension

		return true;		// Boolean
	}
});

// For back-compat, remove in 2.0.
if(has("dijit-legacy-requires")){
	ready(0, function(){
		var requires = ["dijit/_base"];
		require(requires);	// use indirection so modules not rolled into a build
	});
}
return _Widget;
});

},
'dijit/_WidgetBase':function(){
define([
	"require", // require.toUrl
	"dojo/_base/array", // array.forEach array.map
	"dojo/aspect",
	"dojo/_base/config", // config.blankGif
	"dojo/_base/connect", // connect.connect
	"dojo/_base/declare", // declare
	"dojo/dom", // dom.byId
	"dojo/dom-attr", // domAttr.set domAttr.remove
	"dojo/dom-class", // domClass.add domClass.replace
	"dojo/dom-construct", // domConstruct.destroy domConstruct.place
	"dojo/dom-geometry", // isBodyLtr
	"dojo/dom-style", // domStyle.set, domStyle.get
	"dojo/has",
	"dojo/_base/kernel",
	"dojo/_base/lang", // mixin(), isArray(), etc.
	"dojo/on",
	"dojo/ready",
	"dojo/Stateful", // Stateful
	"dojo/topic",
	"dojo/_base/window", // win.body()
	"./Destroyable",
	"dojo/has!dojo-bidi?./_BidiMixin",
	"./registry"    // registry.getUniqueId(), registry.findWidgets()
], function(require, array, aspect, config, connect, declare,
			dom, domAttr, domClass, domConstruct, domGeometry, domStyle, has, kernel,
			lang, on, ready, Stateful, topic, win, Destroyable, _BidiMixin, registry){

	// module:
	//		dijit/_WidgetBase

	// Flag to make dijit load modules the app didn't explicitly request, for backwards compatibility
	has.add("dijit-legacy-requires", !kernel.isAsync);

	// Flag to enable support for textdir attribute
	has.add("dojo-bidi", false);


	// For back-compat, remove in 2.0.
	if(has("dijit-legacy-requires")){
		ready(0, function(){
			var requires = ["dijit/_base/manager"];
			require(requires);	// use indirection so modules not rolled into a build
		});
	}

	// Nested hash listing attributes for each tag, all strings in lowercase.
	// ex: {"div": {"style": true, "tabindex" true}, "form": { ...
	var tagAttrs = {};

	function getAttrs(obj){
		var ret = {};
		for(var attr in obj){
			ret[attr.toLowerCase()] = true;
		}
		return ret;
	}

	function nonEmptyAttrToDom(attr){
		// summary:
		//		Returns a setter function that copies the attribute to this.domNode,
		//		or removes the attribute from this.domNode, depending on whether the
		//		value is defined or not.
		return function(val){
			domAttr[val ? "set" : "remove"](this.domNode, attr, val);
			this._set(attr, val);
		};
	}

	function isEqual(a, b){
		//	summary:
		//		Function that determines whether two values are identical,
		//		taking into account that NaN is not normally equal to itself
		//		in JS.

		return a === b || (/* a is NaN */ a !== a && /* b is NaN */ b !== b);
	}

	var _WidgetBase = declare("dijit._WidgetBase", [Stateful, Destroyable], {
		// summary:
		//		Future base class for all Dijit widgets.
		// description:
		//		Future base class for all Dijit widgets.
		//		_Widget extends this class adding support for various features needed by desktop.
		//
		//		Provides stubs for widget lifecycle methods for subclasses to extend, like postMixInProperties(), buildRendering(),
		//		postCreate(), startup(), and destroy(), and also public API methods like set(), get(), and watch().
		//
		//		Widgets can provide custom setters/getters for widget attributes, which are called automatically by set(name, value).
		//		For an attribute XXX, define methods _setXXXAttr() and/or _getXXXAttr().
		//
		//		_setXXXAttr can also be a string/hash/array mapping from a widget attribute XXX to the widget's DOMNodes:
		//
		//		- DOM node attribute
		// |		_setFocusAttr: {node: "focusNode", type: "attribute"}
		// |		_setFocusAttr: "focusNode"	(shorthand)
		// |		_setFocusAttr: ""		(shorthand, maps to this.domNode)
		//		Maps this.focus to this.focusNode.focus, or (last example) this.domNode.focus
		//
		//		- DOM node innerHTML
		//	|		_setTitleAttr: { node: "titleNode", type: "innerHTML" }
		//		Maps this.title to this.titleNode.innerHTML
		//
		//		- DOM node innerText
		//	|		_setTitleAttr: { node: "titleNode", type: "innerText" }
		//		Maps this.title to this.titleNode.innerText
		//
		//		- DOM node CSS class
		// |		_setMyClassAttr: { node: "domNode", type: "class" }
		//		Maps this.myClass to this.domNode.className
		//
		//		- Toggle DOM node CSS class
		// |		_setMyClassAttr: { node: "domNode", type: "toggleClass" }
		//		Toggles myClass on this.domNode by this.myClass
		//
		//		If the value of _setXXXAttr is an array, then each element in the array matches one of the
		//		formats of the above list.
		//
		//		If the custom setter is null, no action is performed other than saving the new value
		//		in the widget (in this).
		//
		//		If no custom setter is defined for an attribute, then it will be copied
		//		to this.focusNode (if the widget defines a focusNode), or this.domNode otherwise.
		//		That's only done though for attributes that match DOMNode attributes (title,
		//		alt, aria-labelledby, etc.)

		// id: [const] String
		//		A unique, opaque ID string that can be assigned by users or by the
		//		system. If the developer passes an ID which is known not to be
		//		unique, the specified ID is ignored and the system-generated ID is
		//		used instead.
		id: "",
		_setIdAttr: "domNode", // to copy to this.domNode even for auto-generated id's

		// lang: [const] String
		//		Rarely used.  Overrides the default Dojo locale used to render this widget,
		//		as defined by the [HTML LANG](http://www.w3.org/TR/html401/struct/dirlang.html#adef-lang) attribute.
		//		Value must be among the list of locales specified during by the Dojo bootstrap,
		//		formatted according to [RFC 3066](http://www.ietf.org/rfc/rfc3066.txt) (like en-us).
		lang: "",
		// set on domNode even when there's a focus node.	but don't set lang="", since that's invalid.
		_setLangAttr: nonEmptyAttrToDom("lang"),

		// dir: [const] String
		//		Bi-directional support, as defined by the [HTML DIR](http://www.w3.org/TR/html401/struct/dirlang.html#adef-dir)
		//		attribute. Either left-to-right "ltr" or right-to-left "rtl".  If undefined, widgets renders in page's
		//		default direction.
		dir: "",
		// set on domNode even when there's a focus node.	but don't set dir="", since that's invalid.
		_setDirAttr: nonEmptyAttrToDom("dir"), // to set on domNode even when there's a focus node

		// class: String
		//		HTML class attribute
		"class": "",
		_setClassAttr: { node: "domNode", type: "class" },

		// Override automatic assigning type --> focusNode, it causes exception on IE6-8.
		// Instead, type must be specified as ${type} in the template, as part of the original DOM.
		_setTypeAttr: null,

		// style: String||Object
		//		HTML style attributes as cssText string or name/value hash
		style: "",

		// title: String
		//		HTML title attribute.
		//
		//		For form widgets this specifies a tooltip to display when hovering over
		//		the widget (just like the native HTML title attribute).
		//
		//		For TitlePane or for when this widget is a child of a TabContainer, AccordionContainer,
		//		etc., it's used to specify the tab label, accordion pane title, etc.  In this case it's
		//		interpreted as HTML.
		title: "",

		// tooltip: String
		//		When this widget's title attribute is used to for a tab label, accordion pane title, etc.,
		//		this specifies the tooltip to appear when the mouse is hovered over that text.
		tooltip: "",

		// baseClass: [protected] String
		//		Root CSS class of the widget (ex: dijitTextBox), used to construct CSS classes to indicate
		//		widget state.
		baseClass: "",

		// srcNodeRef: [readonly] DomNode
		//		pointer to original DOM node
		srcNodeRef: null,

		// domNode: [readonly] DomNode
		//		This is our visible representation of the widget! Other DOM
		//		Nodes may by assigned to other properties, usually through the
		//		template system's data-dojo-attach-point syntax, but the domNode
		//		property is the canonical "top level" node in widget UI.
		domNode: null,

		// containerNode: [readonly] DomNode
		//		Designates where children of the source DOM node will be placed.
		//		"Children" in this case refers to both DOM nodes and widgets.
		//		For example, for myWidget:
		//
		//		|	<div data-dojo-type=myWidget>
		//		|		<b> here's a plain DOM node
		//		|		<span data-dojo-type=subWidget>and a widget</span>
		//		|		<i> and another plain DOM node </i>
		//		|	</div>
		//
		//		containerNode would point to:
		//
		//		|		<b> here's a plain DOM node
		//		|		<span data-dojo-type=subWidget>and a widget</span>
		//		|		<i> and another plain DOM node </i>
		//
		//		In templated widgets, "containerNode" is set via a
		//		data-dojo-attach-point assignment.
		//
		//		containerNode must be defined for any widget that accepts innerHTML
		//		(like ContentPane or BorderContainer or even Button), and conversely
		//		is null for widgets that don't, like TextBox.
		containerNode: null,

		// ownerDocument: [const] Document?
		//		The document this widget belongs to.  If not specified to constructor, will default to
		//		srcNodeRef.ownerDocument, or if no sourceRef specified, then to the document global
		ownerDocument: null,
		_setOwnerDocumentAttr: function(val){
			// this setter is merely to avoid automatically trying to set this.domNode.ownerDocument
			this._set("ownerDocument", val);
		},

		/*=====
		// _started: [readonly] Boolean
		//		startup() has completed.
		_started: false,
		=====*/

		// attributeMap: [protected] Object
		//		Deprecated.	Instead of attributeMap, widget should have a _setXXXAttr attribute
		//		for each XXX attribute to be mapped to the DOM.
		//
		//		attributeMap sets up a "binding" between attributes (aka properties)
		//		of the widget and the widget's DOM.
		//		Changes to widget attributes listed in attributeMap will be
		//		reflected into the DOM.
		//
		//		For example, calling set('title', 'hello')
		//		on a TitlePane will automatically cause the TitlePane's DOM to update
		//		with the new title.
		//
		//		attributeMap is a hash where the key is an attribute of the widget,
		//		and the value reflects a binding to a:
		//
		//		- DOM node attribute
		// |		focus: {node: "focusNode", type: "attribute"}
		//		Maps this.focus to this.focusNode.focus
		//
		//		- DOM node innerHTML
		//	|		title: { node: "titleNode", type: "innerHTML" }
		//		Maps this.title to this.titleNode.innerHTML
		//
		//		- DOM node innerText
		//	|		title: { node: "titleNode", type: "innerText" }
		//		Maps this.title to this.titleNode.innerText
		//
		//		- DOM node CSS class
		// |		myClass: { node: "domNode", type: "class" }
		//		Maps this.myClass to this.domNode.className
		//
		//		If the value is an array, then each element in the array matches one of the
		//		formats of the above list.
		//
		//		There are also some shorthands for backwards compatibility:
		//
		//		- string --> { node: string, type: "attribute" }, for example:
		//
		//	|	"focusNode" ---> { node: "focusNode", type: "attribute" }
		//
		//		- "" --> { node: "domNode", type: "attribute" }
		attributeMap: {},

		// _blankGif: [protected] String
		//		Path to a blank 1x1 image.
		//		Used by `<img>` nodes in templates that really get their image via CSS background-image.
		_blankGif: config.blankGif || require.toUrl("dojo/resources/blank.gif"),

		// textDir: String
		//		Bi-directional support,	the main variable which is responsible for the direction of the text.
		//		The text direction can be different than the GUI direction by using this parameter in creation
		//		of a widget.
		//
		//		This property is only effective when `has("dojo-bidi")` is defined to be true.
		//
		//		Allowed values:
		//
		//		1. "" - default value; text is same direction as widget
		//		2. "ltr"
		//		3. "rtl"
		//		4. "auto" - contextual the direction of a text defined by first strong letter.
		textDir: "",

		//////////// INITIALIZATION METHODS ///////////////////////////////////////

		/*=====
		constructor: function(params, srcNodeRef){
			// summary:
			//		Create the widget.
			// params: Object|null
			//		Hash of initialization parameters for widget, including scalar values (like title, duration etc.)
			//		and functions, typically callbacks like onClick.
			//		The hash can contain any of the widget's properties, excluding read-only properties.
			// srcNodeRef: DOMNode|String?
			//		If a srcNodeRef (DOM node) is specified:
			//
			//		- use srcNodeRef.innerHTML as my contents
			//		- if this is a behavioral widget then apply behavior to that srcNodeRef
			//		- otherwise, replace srcNodeRef with my generated DOM tree
		},
		=====*/

		_introspect: function(){
			// summary:
			//		Collect metadata about this widget (only once per class, not once per instance):
			//
			//			- list of attributes with custom setters, storing in this.constructor._setterAttrs
			//			- generate this.constructor._onMap, mapping names like "mousedown" to functions like onMouseDown

			var ctor = this.constructor;
			if(!ctor._setterAttrs){
				var proto = ctor.prototype,
					attrs = ctor._setterAttrs = [], // attributes with custom setters
					onMap = (ctor._onMap = {});

				// Items in this.attributeMap are like custom setters.  For back-compat, remove for 2.0.
				for(var name in proto.attributeMap){
					attrs.push(name);
				}

				// Loop over widget properties, collecting properties with custom setters and filling in ctor._onMap.
				for(name in proto){
					if(/^on/.test(name)){
						onMap[name.substring(2).toLowerCase()] = name;
					}

					if(/^_set[A-Z](.*)Attr$/.test(name)){
						name = name.charAt(4).toLowerCase() + name.substr(5, name.length - 9);
						if(!proto.attributeMap || !(name in proto.attributeMap)){
							attrs.push(name);
						}
					}
				}

				// Note: this isn't picking up info on properties like aria-label and role, that don't have custom setters
				// but that set() maps to attributes on this.domNode or this.focusNode
			}
		},

		postscript: function(/*Object?*/params, /*DomNode|String*/srcNodeRef){
			// summary:
			//		Kicks off widget instantiation.  See create() for details.
			// tags:
			//		private

			// Note that we skip calling this.inherited(), i.e. dojo/Stateful::postscript(), because 1.x widgets don't
			// expect their custom setters to get called until after buildRendering().  Consider changing for 2.0.

			this.create(params, srcNodeRef);
		},

		create: function(params, srcNodeRef){
			// summary:
			//		Kick off the life-cycle of a widget
			// description:
			//		Create calls a number of widget methods (postMixInProperties, buildRendering, postCreate,
			//		etc.), some of which of you'll want to override. See http://dojotoolkit.org/reference-guide/dijit/_WidgetBase.html
			//		for a discussion of the widget creation lifecycle.
			//
			//		Of course, adventurous developers could override create entirely, but this should
			//		only be done as a last resort.
			// params: Object|null
			//		Hash of initialization parameters for widget, including scalar values (like title, duration etc.)
			//		and functions, typically callbacks like onClick.
			//		The hash can contain any of the widget's properties, excluding read-only properties.
			// srcNodeRef: DOMNode|String?
			//		If a srcNodeRef (DOM node) is specified:
			//
			//		- use srcNodeRef.innerHTML as my contents
			//		- if this is a behavioral widget then apply behavior to that srcNodeRef
			//		- otherwise, replace srcNodeRef with my generated DOM tree
			// tags:
			//		private

			// First time widget is instantiated, scan prototype to figure out info about custom setters etc.
			this._introspect();

			// store pointer to original DOM tree
			this.srcNodeRef = dom.byId(srcNodeRef);

			// No longer used, remove for 2.0.
			this._connects = [];
			this._supportingWidgets = [];

			// this is here for back-compat, remove in 2.0 (but check NodeList-instantiate.html test)
			if(this.srcNodeRef && this.srcNodeRef.id  && (typeof this.srcNodeRef.id == "string")){
				this.id = this.srcNodeRef.id;
			}

			// mix in our passed parameters
			if(params){
				this.params = params;
				lang.mixin(this, params);
			}
			this.postMixInProperties();

			// Generate an id for the widget if one wasn't specified, or it was specified as id: undefined.
			// Do this before buildRendering() because it might expect the id to be there.
			if(!this.id){
				this.id = registry.getUniqueId(this.declaredClass.replace(/\./g, "_"));
				if(this.params){
					// if params contains {id: undefined}, prevent _applyAttributes() from processing it
					delete this.params.id;
				}
			}

			// The document and <body> node this widget is associated with
			this.ownerDocument = this.ownerDocument || (this.srcNodeRef ? this.srcNodeRef.ownerDocument : document);
			this.ownerDocumentBody = win.body(this.ownerDocument);

			registry.add(this);

			this.buildRendering();

			var deleteSrcNodeRef;

			if(this.domNode){
				// Copy attributes listed in attributeMap into the [newly created] DOM for the widget.
				// Also calls custom setters for all attributes with custom setters.
				this._applyAttributes();

				// If srcNodeRef was specified, then swap out original srcNode for this widget's DOM tree.
				// For 2.0, move this after postCreate().  postCreate() shouldn't depend on the
				// widget being attached to the DOM since it isn't when a widget is created programmatically like
				// new MyWidget({}).	See #11635.
				var source = this.srcNodeRef;
				if(source && source.parentNode && this.domNode !== source){
					source.parentNode.replaceChild(this.domNode, source);
					deleteSrcNodeRef = true;
				}

				// Note: for 2.0 may want to rename widgetId to dojo._scopeName + "_widgetId",
				// assuming that dojo._scopeName even exists in 2.0
				this.domNode.setAttribute("widgetId", this.id);
			}
			this.postCreate();

			// If srcNodeRef has been processed and removed from the DOM (e.g. TemplatedWidget) then delete it to allow GC.
			// I think for back-compatibility it isn't deleting srcNodeRef until after postCreate() has run.
			if(deleteSrcNodeRef){
				delete this.srcNodeRef;
			}

			this._created = true;
		},

		_applyAttributes: function(){
			// summary:
			//		Step during widget creation to copy  widget attributes to the
			//		DOM according to attributeMap and _setXXXAttr objects, and also to call
			//		custom _setXXXAttr() methods.
			//
			//		Skips over blank/false attribute values, unless they were explicitly specified
			//		as parameters to the widget, since those are the default anyway,
			//		and setting tabIndex="" is different than not setting tabIndex at all.
			//
			//		For backwards-compatibility reasons attributeMap overrides _setXXXAttr when
			//		_setXXXAttr is a hash/string/array, but _setXXXAttr as a functions override attributeMap.
			// tags:
			//		private

			// Call this.set() for each property that was either specified as parameter to constructor,
			// or is in the list found above.	For correlated properties like value and displayedValue, the one
			// specified as a parameter should take precedence.
			// Particularly important for new DateTextBox({displayedValue: ...}) since DateTextBox's default value is
			// NaN and thus is not ignored like a default value of "".

			// Step 1: Save the current values of the widget properties that were specified as parameters to the constructor.
			// Generally this.foo == this.params.foo, except if postMixInProperties() changed the value of this.foo.
			var params = {};
			for(var key in this.params || {}){
				params[key] = this._get(key);
			}

			// Step 2: Call set() for each property with a non-falsy value that wasn't passed as a parameter to the constructor
			array.forEach(this.constructor._setterAttrs, function(key){
				if(!(key in params)){
					var val = this._get(key);
					if(val){
						this.set(key, val);
					}
				}
			}, this);

			// Step 3: Call set() for each property that was specified as parameter to constructor.
			// Use params hash created above to ignore side effects from step #2 above.
			for(key in params){
				this.set(key, params[key]);
			}
		},

		postMixInProperties: function(){
			// summary:
			//		Called after the parameters to the widget have been read-in,
			//		but before the widget template is instantiated. Especially
			//		useful to set properties that are referenced in the widget
			//		template.
			// tags:
			//		protected
		},

		buildRendering: function(){
			// summary:
			//		Construct the UI for this widget, setting this.domNode.
			//		Most widgets will mixin `dijit._TemplatedMixin`, which implements this method.
			// tags:
			//		protected

			if(!this.domNode){
				// Create root node if it wasn't created by _TemplatedMixin
				this.domNode = this.srcNodeRef || this.ownerDocument.createElement("div");
			}

			// baseClass is a single class name or occasionally a space-separated list of names.
			// Add those classes to the DOMNode.  If RTL mode then also add with Rtl suffix.
			// TODO: make baseClass custom setter
			if(this.baseClass){
				var classes = this.baseClass.split(" ");
				if(!this.isLeftToRight()){
					classes = classes.concat(array.map(classes, function(name){
						return name + "Rtl";
					}));
				}
				domClass.add(this.domNode, classes);
			}
		},

		postCreate: function(){
			// summary:
			//		Processing after the DOM fragment is created
			// description:
			//		Called after the DOM fragment has been created, but not necessarily
			//		added to the document.  Do not include any operations which rely on
			//		node dimensions or placement.
			// tags:
			//		protected
		},

		startup: function(){
			// summary:
			//		Processing after the DOM fragment is added to the document
			// description:
			//		Called after a widget and its children have been created and added to the page,
			//		and all related widgets have finished their create() cycle, up through postCreate().
			//
			//		Note that startup() may be called while the widget is still hidden, for example if the widget is
			//		inside a hidden dijit/Dialog or an unselected tab of a dijit/layout/TabContainer.
			//		For widgets that need to do layout, it's best to put that layout code inside resize(), and then
			//		extend dijit/layout/_LayoutWidget so that resize() is called when the widget is visible.
			if(this._started){
				return;
			}
			this._started = true;
			array.forEach(this.getChildren(), function(obj){
				if(!obj._started && !obj._destroyed && lang.isFunction(obj.startup)){
					obj.startup();
					obj._started = true;
				}
			});
		},

		//////////// DESTROY FUNCTIONS ////////////////////////////////

		destroyRecursive: function(/*Boolean?*/ preserveDom){
			// summary:
			//		Destroy this widget and its descendants
			// description:
			//		This is the generic "destructor" function that all widget users
			//		should call to cleanly discard with a widget. Once a widget is
			//		destroyed, it is removed from the manager object.
			// preserveDom:
			//		If true, this method will leave the original DOM structure
			//		alone of descendant Widgets. Note: This will NOT work with
			//		dijit._TemplatedMixin widgets.

			this._beingDestroyed = true;
			this.destroyDescendants(preserveDom);
			this.destroy(preserveDom);
		},

		destroy: function(/*Boolean*/ preserveDom){
			// summary:
			//		Destroy this widget, but not its descendants.  Descendants means widgets inside of
			//		this.containerNode.   Will also destroy any resources (including widgets) registered via this.own().
			//
			//		This method will also destroy internal widgets such as those created from a template,
			//		assuming those widgets exist inside of this.domNode but outside of this.containerNode.
			//
			//		For 2.0 it's planned that this method will also destroy descendant widgets, so apps should not
			//		depend on the current ability to destroy a widget without destroying its descendants.   Generally
			//		they should use destroyRecursive() for widgets with children.
			// preserveDom: Boolean
			//		If true, this method will leave the original DOM structure alone.
			//		Note: This will not yet work with _TemplatedMixin widgets

			this._beingDestroyed = true;
			this.uninitialize();

			function destroy(w){
				if(w.destroyRecursive){
					w.destroyRecursive(preserveDom);
				}else if(w.destroy){
					w.destroy(preserveDom);
				}
			}

			// Back-compat, remove for 2.0
			array.forEach(this._connects, lang.hitch(this, "disconnect"));
			array.forEach(this._supportingWidgets, destroy);

			// Destroy supporting widgets, but not child widgets under this.containerNode (for 2.0, destroy child widgets
			// here too).   if() statement is to guard against exception if destroy() called multiple times (see #15815).
			if(this.domNode){
				array.forEach(registry.findWidgets(this.domNode, this.containerNode), destroy);
			}

			this.destroyRendering(preserveDom);
			registry.remove(this.id);
			this._destroyed = true;
		},

		destroyRendering: function(/*Boolean?*/ preserveDom){
			// summary:
			//		Destroys the DOM nodes associated with this widget.
			// preserveDom:
			//		If true, this method will leave the original DOM structure alone
			//		during tear-down. Note: this will not work with _Templated
			//		widgets yet.
			// tags:
			//		protected

			if(this.bgIframe){
				this.bgIframe.destroy(preserveDom);
				delete this.bgIframe;
			}

			if(this.domNode){
				if(preserveDom){
					domAttr.remove(this.domNode, "widgetId");
				}else{
					domConstruct.destroy(this.domNode);
				}
				delete this.domNode;
			}

			if(this.srcNodeRef){
				if(!preserveDom){
					domConstruct.destroy(this.srcNodeRef);
				}
				delete this.srcNodeRef;
			}
		},

		destroyDescendants: function(/*Boolean?*/ preserveDom){
			// summary:
			//		Recursively destroy the children of this widget and their
			//		descendants.
			// preserveDom:
			//		If true, the preserveDom attribute is passed to all descendant
			//		widget's .destroy() method. Not for use with _Templated
			//		widgets.

			// get all direct descendants and destroy them recursively
			array.forEach(this.getChildren(), function(widget){
				if(widget.destroyRecursive){
					widget.destroyRecursive(preserveDom);
				}
			});
		},

		uninitialize: function(){
			// summary:
			//		Deprecated. Override destroy() instead to implement custom widget tear-down
			//		behavior.
			// tags:
			//		protected
			return false;
		},

		////////////////// GET/SET, CUSTOM SETTERS, ETC. ///////////////////

		_setStyleAttr: function(/*String||Object*/ value){
			// summary:
			//		Sets the style attribute of the widget according to value,
			//		which is either a hash like {height: "5px", width: "3px"}
			//		or a plain string
			// description:
			//		Determines which node to set the style on based on style setting
			//		in attributeMap.
			// tags:
			//		protected

			var mapNode = this.domNode;

			// Note: technically we should revert any style setting made in a previous call
			// to his method, but that's difficult to keep track of.

			if(lang.isObject(value)){
				domStyle.set(mapNode, value);
			}else{
				if(mapNode.style.cssText){
					mapNode.style.cssText += "; " + value;
				}else{
					mapNode.style.cssText = value;
				}
			}

			this._set("style", value);
		},

		_attrToDom: function(/*String*/ attr, /*String*/ value, /*Object?*/ commands){
			// summary:
			//		Reflect a widget attribute (title, tabIndex, duration etc.) to
			//		the widget DOM, as specified by commands parameter.
			//		If commands isn't specified then it's looked up from attributeMap.
			//		Note some attributes like "type"
			//		cannot be processed this way as they are not mutable.
			// attr:
			//		Name of member variable (ex: "focusNode" maps to this.focusNode) pointing
			//		to DOMNode inside the widget, or alternately pointing to a subwidget
			// tags:
			//		private

			commands = arguments.length >= 3 ? commands : this.attributeMap[attr];

			array.forEach(lang.isArray(commands) ? commands : [commands], function(command){

				// Get target node and what we are doing to that node
				var mapNode = this[command.node || command || "domNode"];	// DOM node
				var type = command.type || "attribute";	// class, innerHTML, innerText, or attribute

				switch(type){
					case "attribute":
						if(lang.isFunction(value)){ // functions execute in the context of the widget
							value = lang.hitch(this, value);
						}

						// Get the name of the DOM node attribute; usually it's the same
						// as the name of the attribute in the widget (attr), but can be overridden.
						// Also maps handler names to lowercase, like onSubmit --> onsubmit
						var attrName = command.attribute ? command.attribute :
							(/^on[A-Z][a-zA-Z]*$/.test(attr) ? attr.toLowerCase() : attr);

						if(mapNode.tagName){
							// Normal case, mapping to a DOMNode.  Note that modern browsers will have a mapNode.set()
							// method, but for consistency we still call domAttr
							domAttr.set(mapNode, attrName, value);
						}else{
							// mapping to a sub-widget
							mapNode.set(attrName, value);
						}
						break;
					case "innerText":
						// Deprecated, use "textContent" instead.
						mapNode.innerHTML = "";
						mapNode.appendChild(this.ownerDocument.createTextNode(value));
						break;
					case "textContent":
						mapNode.textContent = value;
						break;
					case "innerHTML":
						mapNode.innerHTML = value;
						break;
					case "class":
						domClass.replace(mapNode, value, this[attr]);
						break;
					case "toggleClass":
						domClass.toggle(mapNode, command.className || attr, value);
						break;
				}
			}, this);
		},

		get: function(name){
			// summary:
			//		Get a property from a widget.
			// name:
			//		The property to get.
			// description:
			//		Get a named property from a widget. The property may
			//		potentially be retrieved via a getter method. If no getter is defined, this
			//		just retrieves the object's property.
			//
			//		For example, if the widget has properties `foo` and `bar`
			//		and a method named `_getFooAttr()`, calling:
			//		`myWidget.get("foo")` would be equivalent to calling
			//		`widget._getFooAttr()` and `myWidget.get("bar")`
			//		would be equivalent to the expression
			//		`widget.bar2`
			var names = this._getAttrNames(name);
			return this[names.g] ? this[names.g]() : this._get(name);
		},

		set: function(name, value){
			// summary:
			//		Set a property on a widget
			// name:
			//		The property to set.
			// value:
			//		The value to set in the property.
			// description:
			//		Sets named properties on a widget which may potentially be handled by a
			//		setter in the widget.
			//
			//		For example, if the widget has properties `foo` and `bar`
			//		and a method named `_setFooAttr()`, calling
			//		`myWidget.set("foo", "Howdy!")` would be equivalent to calling
			//		`widget._setFooAttr("Howdy!")` and `myWidget.set("bar", 3)`
			//		would be equivalent to the statement `widget.bar = 3;`
			//
			//		set() may also be called with a hash of name/value pairs, ex:
			//
			//	|	myWidget.set({
			//	|		foo: "Howdy",
			//	|		bar: 3
			//	|	});
			//
			//	This is equivalent to calling `set(foo, "Howdy")` and `set(bar, 3)`

			if(typeof name === "object"){
				for(var x in name){
					this.set(x, name[x]);
				}
				return this;
			}
			var names = this._getAttrNames(name),
				setter = this[names.s];
			if(lang.isFunction(setter)){
				// use the explicit setter
				var result = setter.apply(this, Array.prototype.slice.call(arguments, 1));
			}else{
				// Mapping from widget attribute to DOMNode/subwidget attribute/value/etc.
				// Map according to:
				//		1. attributeMap setting, if one exists (TODO: attributeMap deprecated, remove in 2.0)
				//		2. _setFooAttr: {...} type attribute in the widget (if one exists)
				//		3. apply to focusNode or domNode if standard attribute name, excluding funcs like onClick.
				// Checks if an attribute is a "standard attribute" by whether the DOMNode JS object has a similar
				// attribute name (ex: accept-charset attribute matches jsObject.acceptCharset).
				// Note also that Tree.focusNode() is a function not a DOMNode, so test for that.
				var defaultNode = this.focusNode && !lang.isFunction(this.focusNode) ? "focusNode" : "domNode",
					tag = this[defaultNode] && this[defaultNode].tagName,
					attrsForTag = tag && (tagAttrs[tag] || (tagAttrs[tag] = getAttrs(this[defaultNode]))),
					map = name in this.attributeMap ? this.attributeMap[name] :
						names.s in this ? this[names.s] :
							((attrsForTag && names.l in attrsForTag && typeof value != "function") ||
								/^aria-|^data-|^role$/.test(name)) ? defaultNode : null;
				if(map != null){
					this._attrToDom(name, value, map);
				}
				this._set(name, value);
			}
			return result || this;
		},

		_attrPairNames: {}, // shared between all widgets
		_getAttrNames: function(name){
			// summary:
			//		Helper function for get() and set().
			//		Caches attribute name values so we don't do the string ops every time.
			// tags:
			//		private

			var apn = this._attrPairNames;
			if(apn[name]){
				return apn[name];
			}
			var uc = name.replace(/^[a-z]|-[a-zA-Z]/g, function(c){
				return c.charAt(c.length - 1).toUpperCase();
			});
			return (apn[name] = {
				n: name + "Node",
				s: "_set" + uc + "Attr", // converts dashes to camel case, ex: accept-charset --> _setAcceptCharsetAttr
				g: "_get" + uc + "Attr",
				l: uc.toLowerCase()        // lowercase name w/out dashes, ex: acceptcharset
			});
		},

		_set: function(/*String*/ name, /*anything*/ value){
			// summary:
			//		Helper function to set new value for specified property, and call handlers
			//		registered with watch() if the value has changed.
			var oldValue = this[name];
			this[name] = value;
			if(this._created && !isEqual(oldValue, value)){
				if(this._watchCallbacks){
					this._watchCallbacks(name, oldValue, value);
				}
				this.emit("attrmodified-" + name, {
					detail: {
						prevValue: oldValue,
						newValue: value
					}
				});
			}
		},

		_get: function(/*String*/ name){
			// summary:
			//		Helper function to get value for specified property stored by this._set(),
			//		i.e. for properties with custom setters.  Used mainly by custom getters.
			//
			//		For example, CheckBox._getValueAttr() calls this._get("value").

			// future: return name in this.props ? this.props[name] : this[name];
			return this[name];
		},

		emit: function(/*String*/ type, /*Object?*/ eventObj, /*Array?*/ callbackArgs){
			// summary:
			//		Used by widgets to signal that a synthetic event occurred, ex:
			//	|	myWidget.emit("attrmodified-selectedChildWidget", {}).
			//
			//		Emits an event on this.domNode named type.toLowerCase(), based on eventObj.
			//		Also calls onType() method, if present, and returns value from that method.
			//		By default passes eventObj to callback, but will pass callbackArgs instead, if specified.
			//		Modifies eventObj by adding missing parameters (bubbles, cancelable, widget).
			// tags:
			//		protected

			// Specify fallback values for bubbles, cancelable in case they are not set in eventObj.
			// Also set pointer to widget, although since we can't add a pointer to the widget for native events
			// (see #14729), maybe we shouldn't do it here?
			eventObj = eventObj || {};
			if(eventObj.bubbles === undefined){
				eventObj.bubbles = true;
			}
			if(eventObj.cancelable === undefined){
				eventObj.cancelable = true;
			}
			if(!eventObj.detail){
				eventObj.detail = {};
			}
			eventObj.detail.widget = this;

			var ret, callback = this["on" + type];
			if(callback){
				ret = callback.apply(this, callbackArgs ? callbackArgs : [eventObj]);
			}

			// Emit event, but avoid spurious emit()'s as parent sets properties on child during startup/destroy
			if(this._started && !this._beingDestroyed){
				on.emit(this.domNode, type.toLowerCase(), eventObj);
			}

			return ret;
		},

		on: function(/*String|Function*/ type, /*Function*/ func){
			// summary:
			//		Call specified function when event occurs, ex: myWidget.on("click", function(){ ... }).
			// type:
			//		Name of event (ex: "click") or extension event like touch.press.
			// description:
			//		Call specified function when event `type` occurs, ex: `myWidget.on("click", function(){ ... })`.
			//		Note that the function is not run in any particular scope, so if (for example) you want it to run in the
			//		widget's scope you must do `myWidget.on("click", lang.hitch(myWidget, func))`.

			// For backwards compatibility, if there's an onType() method in the widget then connect to that.
			// Remove in 2.0.
			var widgetMethod = this._onMap(type);
			if(widgetMethod){
				return aspect.after(this, widgetMethod, func, true);
			}

			// Otherwise, just listen for the event on this.domNode.
			return this.own(on(this.domNode, type, func))[0];
		},

		_onMap: function(/*String|Function*/ type){
			// summary:
			//		Maps on() type parameter (ex: "mousemove") to method name (ex: "onMouseMove").
			//		If type is a synthetic event like touch.press then returns undefined.
			var ctor = this.constructor, map = ctor._onMap;
			if(!map){
				map = (ctor._onMap = {});
				for(var attr in ctor.prototype){
					if(/^on/.test(attr)){
						map[attr.replace(/^on/, "").toLowerCase()] = attr;
					}
				}
			}
			return map[typeof type == "string" && type.toLowerCase()];	// String
		},

		toString: function(){
			// summary:
			//		Returns a string that represents the widget.
			// description:
			//		When a widget is cast to a string, this method will be used to generate the
			//		output. Currently, it does not implement any sort of reversible
			//		serialization.
			return '[Widget ' + this.declaredClass + ', ' + (this.id || 'NO ID') + ']'; // String
		},

		getChildren: function(){
			// summary:
			//		Returns all direct children of this widget, i.e. all widgets underneath this.containerNode whose parent
			//		is this widget.   Note that it does not return all descendants, but rather just direct children.
			//		Analogous to [Node.childNodes](https://developer.mozilla.org/en-US/docs/DOM/Node.childNodes),
			//		except containing widgets rather than DOMNodes.
			//
			//		The result intentionally excludes internally created widgets (a.k.a. supporting widgets)
			//		outside of this.containerNode.
			//
			//		Note that the array returned is a simple array.  Application code should not assume
			//		existence of methods like forEach().

			return this.containerNode ? registry.findWidgets(this.containerNode) : []; // dijit/_WidgetBase[]
		},

		getParent: function(){
			// summary:
			//		Returns the parent widget of this widget.

			return registry.getEnclosingWidget(this.domNode.parentNode);
		},

		connect: function(/*Object|null*/ obj, /*String|Function*/ event, /*String|Function*/ method){
			// summary:
			//		Deprecated, will be removed in 2.0, use this.own(on(...)) or this.own(aspect.after(...)) instead.
			//
			//		Connects specified obj/event to specified method of this object
			//		and registers for disconnect() on widget destroy.
			//
			//		Provide widget-specific analog to dojo.connect, except with the
			//		implicit use of this widget as the target object.
			//		Events connected with `this.connect` are disconnected upon
			//		destruction.
			// returns:
			//		A handle that can be passed to `disconnect` in order to disconnect before
			//		the widget is destroyed.
			// example:
			//	|	var btn = new Button();
			//	|	// when foo.bar() is called, call the listener we're going to
			//	|	// provide in the scope of btn
			//	|	btn.connect(foo, "bar", function(){
			//	|		console.debug(this.toString());
			//	|	});
			// tags:
			//		protected

			return this.own(connect.connect(obj, event, this, method))[0];	// handle
		},

		disconnect: function(handle){
			// summary:
			//		Deprecated, will be removed in 2.0, use handle.remove() instead.
			//
			//		Disconnects handle created by `connect`.
			// tags:
			//		protected

			handle.remove();
		},

		subscribe: function(t, method){
			// summary:
			//		Deprecated, will be removed in 2.0, use this.own(topic.subscribe()) instead.
			//
			//		Subscribes to the specified topic and calls the specified method
			//		of this object and registers for unsubscribe() on widget destroy.
			//
			//		Provide widget-specific analog to dojo.subscribe, except with the
			//		implicit use of this widget as the target object.
			// t: String
			//		The topic
			// method: Function
			//		The callback
			// example:
			//	|	var btn = new Button();
			//	|	// when /my/topic is published, this button changes its label to
			//	|	// be the parameter of the topic.
			//	|	btn.subscribe("/my/topic", function(v){
			//	|		this.set("label", v);
			//	|	});
			// tags:
			//		protected
			return this.own(topic.subscribe(t, lang.hitch(this, method)))[0];	// handle
		},

		unsubscribe: function(/*Object*/ handle){
			// summary:
			//		Deprecated, will be removed in 2.0, use handle.remove() instead.
			//
			//		Unsubscribes handle created by this.subscribe.
			//		Also removes handle from this widget's list of subscriptions
			// tags:
			//		protected

			handle.remove();
		},

		isLeftToRight: function(){
			// summary:
			//		Return this widget's explicit or implicit orientation (true for LTR, false for RTL)
			// tags:
			//		protected
			return this.dir ? (this.dir.toLowerCase() == "ltr") : domGeometry.isBodyLtr(this.ownerDocument); //Boolean
		},

		isFocusable: function(){
			// summary:
			//		Return true if this widget can currently be focused
			//		and false if not
			return this.focus && (domStyle.get(this.domNode, "display") != "none");
		},

		placeAt: function(/*String|DomNode|DocumentFragment|dijit/_WidgetBase*/ reference, /*String|Int?*/ position){
			// summary:
			//		Place this widget somewhere in the DOM based
			//		on standard domConstruct.place() conventions.
			// description:
			//		A convenience function provided in all _Widgets, providing a simple
			//		shorthand mechanism to put an existing (or newly created) Widget
			//		somewhere in the dom, and allow chaining.
			// reference:
			//		Widget, DOMNode, DocumentFragment, or id of widget or DOMNode
			// position:
			//		If reference is a widget (or id of widget), and that widget has an ".addChild" method,
			//		it will be called passing this widget instance into that method, supplying the optional
			//		position index passed.  In this case position (if specified) should be an integer.
			//
			//		If reference is a DOMNode (or id matching a DOMNode but not a widget),
			//		the position argument can be a numeric index or a string
			//		"first", "last", "before", or "after", same as dojo/dom-construct::place().
			// returns: dijit/_WidgetBase
			//		Provides a useful return of the newly created dijit._Widget instance so you
			//		can "chain" this function by instantiating, placing, then saving the return value
			//		to a variable.
			// example:
			//	|	// create a Button with no srcNodeRef, and place it in the body:
			//	|	var button = new Button({ label:"click" }).placeAt(win.body());
			//	|	// now, 'button' is still the widget reference to the newly created button
			//	|	button.on("click", function(e){ console.log('click'); }));
			// example:
			//	|	// create a button out of a node with id="src" and append it to id="wrapper":
			//	|	var button = new Button({},"src").placeAt("wrapper");
			// example:
			//	|	// place a new button as the first element of some div
			//	|	var button = new Button({ label:"click" }).placeAt("wrapper","first");
			// example:
			//	|	// create a contentpane and add it to a TabContainer
			//	|	var tc = dijit.byId("myTabs");
			//	|	new ContentPane({ href:"foo.html", title:"Wow!" }).placeAt(tc)

			var refWidget = !reference.tagName && registry.byId(reference);
			if(refWidget && refWidget.addChild && (!position || typeof position === "number")){
				// Adding this to refWidget and can use refWidget.addChild() to handle everything.
				refWidget.addChild(this, position);
			}else{
				// "reference" is a plain DOMNode, or we can't use refWidget.addChild().   Use domConstruct.place() and
				// target refWidget.containerNode for nested placement (position==number, "first", "last", "only"), and
				// refWidget.domNode otherwise ("after"/"before"/"replace").  (But not supported officially, see #14946.)
				var ref = refWidget && ("domNode" in refWidget) ?
					(refWidget.containerNode && !/after|before|replace/.test(position || "") ?
						refWidget.containerNode : refWidget.domNode) : dom.byId(reference, this.ownerDocument);
				domConstruct.place(this.domNode, ref, position);

				// Start this iff it has a parent widget that's already started.
				// TODO: for 2.0 maybe it should also start the widget when this.getParent() returns null??
				if(!this._started && (this.getParent() || {})._started){
					this.startup();
				}
			}
			return this;
		},

		defer: function(fcn, delay){
			// summary:
			//		Wrapper to setTimeout to avoid deferred functions executing
			//		after the originating widget has been destroyed.
			//		Returns an object handle with a remove method (that returns null) (replaces clearTimeout).
			// fcn: Function
			//		Function reference.
			// delay: Number?
			//		Delay, defaults to 0.
			// tags:
			//		protected

			var timer = setTimeout(lang.hitch(this,
				function(){
					if(!timer){
						return;
					}
					timer = null;
					if(!this._destroyed){
						lang.hitch(this, fcn)();
					}
				}),
				delay || 0
			);
			return {
				remove: function(){
					if(timer){
						clearTimeout(timer);
						timer = null;
					}
					return null; // so this works well: handle = handle.remove();
				}
			};
		}
	});

	if(has("dojo-bidi")){
		_WidgetBase.extend(_BidiMixin);
	}

	return _WidgetBase;
});

},
'dojo/Stateful':function(){
define(["./_base/declare", "./_base/lang", "./_base/array", "./when"], function(declare, lang, array, when){
	// module:
	//		dojo/Stateful

return declare("dojo.Stateful", null, {
	// summary:
	//		Base class for objects that provide named properties with optional getter/setter
	//		control and the ability to watch for property changes
	//
	//		The class also provides the functionality to auto-magically manage getters
	//		and setters for object attributes/properties.
	//		
	//		Getters and Setters should follow the format of _xxxGetter or _xxxSetter where 
	//		the xxx is a name of the attribute to handle.  So an attribute of "foo" 
	//		would have a custom getter of _fooGetter and a custom setter of _fooSetter.
	//
	// example:
	//	|	require(["dojo/Stateful", function(Stateful) {
	//	|		var obj = new Stateful();
	//	|		obj.watch("foo", function(){
	//	|			console.log("foo changed to " + this.get("foo"));
	//	|		});
	//	|		obj.set("foo","bar");
	//	|	});

	// _attrPairNames: Hash
	//		Used across all instances a hash to cache attribute names and their getter 
	//		and setter names.
	_attrPairNames: {},

	_getAttrNames: function(name){
		// summary:
		//		Helper function for get() and set().
		//		Caches attribute name values so we don't do the string ops every time.
		// tags:
		//		private

		var apn = this._attrPairNames;
		if(apn[name]){ return apn[name]; }
		return (apn[name] = {
			s: "_" + name + "Setter",
			g: "_" + name + "Getter"
		});
	},

	postscript: function(/*Object?*/ params){
		// Automatic setting of params during construction
		if (params){ this.set(params); }
	},

	_get: function(name, names){
		// summary:
		//		Private function that does a get based off a hash of names
		// names:
		//		Hash of names of custom attributes
		return typeof this[names.g] === "function" ? this[names.g]() : this[name];
	},
	get: function(/*String*/name){
		// summary:
		//		Get a property on a Stateful instance.
		// name:
		//		The property to get.
		// returns:
		//		The property value on this Stateful instance.
		// description:
		//		Get a named property on a Stateful object. The property may
		//		potentially be retrieved via a getter method in subclasses. In the base class
		//		this just retrieves the object's property.
		// example:
		//	|	require(["dojo/Stateful", function(Stateful) {
		//	|		var stateful = new Stateful({foo: 3});
		//	|		stateful.get("foo") // returns 3
		//	|		stateful.foo // returns 3
		//	|	});

		return this._get(name, this._getAttrNames(name)); //Any
	},
	set: function(/*String*/name, /*Object*/value){
		// summary:
		//		Set a property on a Stateful instance
		// name:
		//		The property to set.
		// value:
		//		The value to set in the property.
		// returns:
		//		The function returns this dojo.Stateful instance.
		// description:
		//		Sets named properties on a stateful object and notifies any watchers of
		//		the property. A programmatic setter may be defined in subclasses.
		// example:
		//	|	require(["dojo/Stateful", function(Stateful) {
		//	|		var stateful = new Stateful();
		//	|		stateful.watch(function(name, oldValue, value){
		//	|			// this will be called on the set below
		//	|		}
		//	|		stateful.set(foo, 5);
		//	set() may also be called with a hash of name/value pairs, ex:
		//	|		stateful.set({
		//	|			foo: "Howdy",
		//	|			bar: 3
		//	|		});
		//	|	});
		//	This is equivalent to calling set(foo, "Howdy") and set(bar, 3)

		// If an object is used, iterate through object
		if(typeof name === "object"){
			for(var x in name){
				if(name.hasOwnProperty(x) && x !="_watchCallbacks"){
					this.set(x, name[x]);
				}
			}
			return this;
		}

		var names = this._getAttrNames(name),
			oldValue = this._get(name, names),
			setter = this[names.s],
			result;
		if(typeof setter === "function"){
			// use the explicit setter
			result = setter.apply(this, Array.prototype.slice.call(arguments, 1));
		}else{
			// no setter so set attribute directly
			this[name] = value;
		}
		if(this._watchCallbacks){
			var self = this;
			// If setter returned a promise, wait for it to complete, otherwise call watches immediately
			when(result, function(){
				self._watchCallbacks(name, oldValue, value);
			});
		}
		return this; // dojo/Stateful
	},
	_changeAttrValue: function(name, value){
		// summary:
		//		Internal helper for directly changing an attribute value.
		//
		// name: String
		//		The property to set.
		// value: Mixed
		//		The value to set in the property.
		//
		// description:
		//		Directly change the value of an attribute on an object, bypassing any 
		//		accessor setter.  Also handles the calling of watch and emitting events. 
		//		It is designed to be used by descendant class when there are two values 
		//		of attributes that are linked, but calling .set() is not appropriate.

		var oldValue = this.get(name);
		this[name] = value;
		if(this._watchCallbacks){
			this._watchCallbacks(name, oldValue, value);
		}
		return this; // dojo/Stateful
	},
	watch: function(/*String?*/name, /*Function*/callback){
		// summary:
		//		Watches a property for changes
		// name:
		//		Indicates the property to watch. This is optional (the callback may be the
		//		only parameter), and if omitted, all the properties will be watched
		// returns:
		//		An object handle for the watch. The unwatch method of this object
		//		can be used to discontinue watching this property:
		//		|	var watchHandle = obj.watch("foo", callback);
		//		|	watchHandle.unwatch(); // callback won't be called now
		// callback:
		//		The function to execute when the property changes. This will be called after
		//		the property has been changed. The callback will be called with the |this|
		//		set to the instance, the first argument as the name of the property, the
		//		second argument as the old value and the third argument as the new value.

		var callbacks = this._watchCallbacks;
		if(!callbacks){
			var self = this;
			callbacks = this._watchCallbacks = function(name, oldValue, value, ignoreCatchall){
				var notify = function(propertyCallbacks){
					if(propertyCallbacks){
						propertyCallbacks = propertyCallbacks.slice();
						for(var i = 0, l = propertyCallbacks.length; i < l; i++){
							propertyCallbacks[i].call(self, name, oldValue, value);
						}
					}
				};
				notify(callbacks['_' + name]);
				if(!ignoreCatchall){
					notify(callbacks["*"]); // the catch-all
				}
			}; // we use a function instead of an object so it will be ignored by JSON conversion
		}
		if(!callback && typeof name === "function"){
			callback = name;
			name = "*";
		}else{
			// prepend with dash to prevent name conflicts with function (like "name" property)
			name = '_' + name;
		}
		var propertyCallbacks = callbacks[name];
		if(typeof propertyCallbacks !== "object"){
			propertyCallbacks = callbacks[name] = [];
		}
		propertyCallbacks.push(callback);

		// TODO: Remove unwatch in 2.0
		var handle = {};
		handle.unwatch = handle.remove = function(){
			var index = array.indexOf(propertyCallbacks, callback);
			if(index > -1){
				propertyCallbacks.splice(index, 1);
			}
		};
		return handle; //Object
	}

});

});

},
'dijit/Destroyable':function(){
define([
	"dojo/_base/array", // array.forEach array.map
	"dojo/aspect",
	"dojo/_base/declare"
], function(array, aspect, declare){

	// module:
	//		dijit/Destroyable

	return declare("dijit.Destroyable", null, {
		// summary:
		//		Mixin to track handles and release them when instance is destroyed.
		// description:
		//		Call this.own(...) on list of handles (returned from dojo/aspect, dojo/on,
		//		dojo/Stateful::watch, or any class (including widgets) with a destroyRecursive() or destroy() method.
		//		Then call destroy() later to destroy this instance and release the resources.

		destroy: function(/*Boolean*/ preserveDom){
			// summary:
			//		Destroy this class, releasing any resources registered via own().
			this._destroyed = true;
		},

		own: function(){
			// summary:
			//		Track specified handles and remove/destroy them when this instance is destroyed, unless they were
			//		already removed/destroyed manually.
			// tags:
			//		protected
			// returns:
			//		The array of specified handles, so you can do for example:
			//	|		var handle = this.own(on(...))[0];

			var cleanupMethods = [
				"destroyRecursive",
				"destroy",
				"remove"
			];

			array.forEach(arguments, function(handle){
				// When this.destroy() is called, destroy handle.  Since I'm using aspect.before(),
				// the handle will be destroyed before a subclass's destroy() method starts running, before it calls
				// this.inherited() or even if it doesn't call this.inherited() at all.  If that's an issue, make an
				// onDestroy() method and connect to that instead.
				var destroyMethodName;
				var odh = aspect.before(this, "destroy", function (preserveDom){
					handle[destroyMethodName](preserveDom);
				});

				// Callback for when handle is manually destroyed.
				var hdhs = [];
				function onManualDestroy(){
					odh.remove();
					array.forEach(hdhs, function(hdh){
						hdh.remove();
					});
				}

				// Setup listeners for manual destroy of handle.
				// Also computes destroyMethodName, used in listener above.
				if(handle.then){
					// Special path for Promises.  Detect when Promise is resolved, rejected, or
					// canceled (nb: cancelling a Promise causes it to be rejected).
					destroyMethodName = "cancel";
					handle.then(onManualDestroy, onManualDestroy);
				}else{
					// Path for other handles.  Just use AOP to detect when handle is manually destroyed.
					array.forEach(cleanupMethods, function(cleanupMethod){
						if(typeof handle[cleanupMethod] === "function"){
							if(!destroyMethodName){
								// Use first matching method name in above listener (prefer destroyRecursive() to destroy())
								destroyMethodName = cleanupMethod;
							}
							hdhs.push(aspect.after(handle, cleanupMethod, onManualDestroy, true));
						}
					});
				}
			}, this);

			return arguments;		// handle
		}
	});
});

},
'dijit/_OnDijitClickMixin':function(){
define([
	"dojo/on",
	"dojo/_base/array", // array.forEach
	"dojo/keys", // keys.ENTER keys.SPACE
	"dojo/_base/declare", // declare
	"dojo/has", // has("dom-addeventlistener")
	"./a11yclick"
], function(on, array, keys, declare, has, a11yclick){

	// module:
	//		dijit/_OnDijitClickMixin

	var ret = declare("dijit._OnDijitClickMixin", null, {
		// summary:
		//		Deprecated.   New code should access the dijit/a11yclick event directly, ex:
		//		|	this.own(on(node, a11yclick, function(){ ... }));
		//
		//		Mixing in this class will make _WidgetBase.connect(node, "ondijitclick", ...) work.
		//		It also used to be necessary to make templates with ondijitclick work, but now you can just require
		//		dijit/a11yclick.

		connect: function(obj, event, method){
			// override _WidgetBase.connect() to make this.connect(node, "ondijitclick", ...) work
			return this.inherited(arguments, [obj, event == "ondijitclick" ? a11yclick : event, method]);
		}
	});

	ret.a11yclick = a11yclick;	// back compat

	return ret;
});

},
'dijit/a11yclick':function(){
define([
	"dojo/keys", // keys.ENTER keys.SPACE
	"dojo/mouse",
	"dojo/on",
	"dojo/touch" // touch support for click is now there
], function(keys, mouse, on, touch){

	// module:
	//		dijit/a11yclick

	/*=====
	return {
		// summary:
		//		Custom press, release, and click synthetic events
		//		which trigger on a left mouse click, touch, or space/enter keyup.

		click: function(node, listener){
			// summary:
			//		Logical click operation for mouse, touch, or keyboard (space/enter key)
		},
		press: function(node, listener){
			// summary:
			//		Mousedown (left button), touchstart, or keydown (space or enter) corresponding to logical click operation.
		},
		release: function(node, listener){
			// summary:
			//		Mouseup (left button), touchend, or keyup (space or enter) corresponding to logical click operation.
		},
		move: function(node, listener){
			// summary:
			//		Mouse cursor or a finger is dragged over the given node.
		}
	};
	=====*/

	function clickKey(/*Event*/ e){
		// Test if this keyboard event should be tracked as the start (if keydown) or end (if keyup) of a click event.
		// Only track for nodes marked to be tracked, and not for buttons or inputs,
		// since buttons handle keyboard click natively, and text inputs should not
		// prevent typing spaces or newlines.
		if((e.keyCode === keys.ENTER || e.keyCode === keys.SPACE) && !/input|button|textarea/i.test(e.target.nodeName)){

			// Test if a node or its ancestor has been marked with the dojoClick property to indicate special processing
			for(var node = e.target; node; node = node.parentNode){
				if(node.dojoClick){ return true; }
			}
		}
	}

	var lastKeyDownNode;

	on(document, "keydown", function(e){
		//console.log("a11yclick: onkeydown, e.target = ", e.target, ", lastKeyDownNode was ", lastKeyDownNode, ", equality is ", (e.target === lastKeyDownNode));
		if(clickKey(e)){
			// needed on IE for when focus changes between keydown and keyup - otherwise dropdown menus do not work
			lastKeyDownNode = e.target;

			// Prevent viewport scrolling on space key in IE<9.
			// (Reproducible on test_Button.html on any of the first dijit/form/Button examples)
			e.preventDefault();
		}else{
			lastKeyDownNode = null;
		}
	});

	on(document, "keyup", function(e){
		//console.log("a11yclick: onkeyup, e.target = ", e.target, ", lastKeyDownNode was ", lastKeyDownNode, ", equality is ", (e.target === lastKeyDownNode));
		if(clickKey(e) && e.target == lastKeyDownNode){	// === breaks greasemonkey
			//need reset here or have problems in FF when focus returns to trigger element after closing popup/alert
			lastKeyDownNode = null;

			on.emit(e.target, "click", {
				cancelable: true,
				bubbles: true,
				ctrlKey: e.ctrlKey,
				shiftKey: e.shiftKey,
				metaKey: e.metaKey,
				altKey: e.altKey,
				_origType: e.type
			});
		}
	});

	// I want to return a hash of the synthetic events, but for backwards compatibility the main return value
	// needs to be the click event.   Change for 2.0.

	var click = function(node, listener){
		// Set flag on node so that keydown/keyup above emits click event.
		// Also enables fast click processing from dojo/touch.
		node.dojoClick = true;

		return on(node, "click", listener);
	};
	click.click = click;	// forward compatibility with 2.0

	click.press =  function(node, listener){
		var touchListener = on(node, touch.press, function(evt){
			if(evt.type == "mousedown" && !mouse.isLeft(evt)){
				// Ignore right click
				return;
			}
			listener(evt);
		}), keyListener = on(node, "keydown", function(evt){
			if(evt.keyCode === keys.ENTER || evt.keyCode === keys.SPACE){
				listener(evt);
			}
		});
		return {
			remove: function(){
				touchListener.remove();
				keyListener.remove();
			}
		};
	};

	click.release =  function(node, listener){
		var touchListener = on(node, touch.release, function(evt){
			if(evt.type == "mouseup" && !mouse.isLeft(evt)){
				// Ignore right click
				return;
			}
			listener(evt);
		}), keyListener = on(node, "keyup", function(evt){
			if(evt.keyCode === keys.ENTER || evt.keyCode === keys.SPACE){
				listener(evt);
			}
		});
		return {
			remove: function(){
				touchListener.remove();
				keyListener.remove();
			}
		};
	};

	click.move = touch.move;	// just for convenience

	return click;
});

},
'dojo/touch':function(){
define(["./_base/kernel", "./aspect", "./dom", "./dom-class", "./_base/lang", "./on", "./has", "./mouse", "./domReady", "./_base/window"],
function(dojo, aspect, dom, domClass, lang, on, has, mouse, domReady, win){

	// module:
	//		dojo/touch

	var ios4 = has("ios") < 5;

	// Detect if platform supports Pointer Events, and if so, the names of the events (pointerdown vs. MSPointerDown).
	var hasPointer = has("pointer-events") || has("MSPointer"),
		pointer = (function () {
			var pointer = {};
			for (var type in { down: 1, move: 1, up: 1, cancel: 1, over: 1, out: 1 }) {
				pointer[type] = has("MSPointer") ?
					"MSPointer" + type.charAt(0).toUpperCase() + type.slice(1) :
					"pointer" + type;
			}
			return pointer;
		})();

	// Detect if platform supports the webkit touchstart/touchend/... events
	var hasTouch = has("touch-events");

	// Click generation variables
	var clicksInited, clickTracker, useTarget = false, clickTarget, clickX, clickY, clickDx, clickDy, clickTime;

	// Time of most recent touchstart, touchmove, or touchend event
	var lastTouch;

	function dualEvent(mouseType, touchType, pointerType){
		// Returns synthetic event that listens for both the specified mouse event and specified touch event.
		// But ignore fake mouse events that were generated due to the user touching the screen.
		if(hasPointer && pointerType){
			// IE10+: MSPointer* events are designed to handle both mouse and touch in a uniform way,
			// so just use that regardless of hasTouch.
			return function(node, listener){
				return on(node, pointerType, listener);
			};
		}else if(hasTouch){
			return function(node, listener){
				var handle1 = on(node, touchType, function(evt){
						listener.call(this, evt);

						// On slow mobile browsers (see https://bugs.dojotoolkit.org/ticket/17634),
						// a handler for a touch event may take >1s to run.  That time shouldn't
						// be included in the calculation for lastTouch.
						lastTouch = (new Date()).getTime();
					}),
					handle2 = on(node, mouseType, function(evt){
						if(!lastTouch || (new Date()).getTime() > lastTouch + 1000){
							listener.call(this, evt);
						}
					});
				return {
					remove: function(){
						handle1.remove();
						handle2.remove();
					}
				};
			};
		}else{
			// Avoid creating listeners for touch events on performance sensitive older browsers like IE6
			return function(node, listener){
				return on(node, mouseType, listener);
			};
		}
	}

	function marked(/*DOMNode*/ node){
		// Search for node ancestor has been marked with the dojoClick property to indicate special processing.
		// Returns marked ancestor.
		do{
			if(node.dojoClick !== undefined){ return node; }
		}while(node = node.parentNode);
	}

	function doClicks(e, moveType, endType){
		// summary:
		//		Setup touch listeners to generate synthetic clicks immediately (rather than waiting for the browser
		//		to generate clicks after the double-tap delay) and consistently (regardless of whether event.preventDefault()
		//		was called in an event listener. Synthetic clicks are generated only if a node or one of its ancestors has
		//		its dojoClick property set to truthy. If a node receives synthetic clicks because one of its ancestors has its
		//      dojoClick property set to truthy, you can disable synthetic clicks on this node by setting its own dojoClick property
		//      to falsy.

		if(mouse.isRight(e)){
			return;		// avoid spurious dojoclick event on IE10+; right click is just for context menu
		}

		var markedNode = marked(e.target);
		clickTracker  = !e.target.disabled && markedNode && markedNode.dojoClick; // click threshold = true, number, x/y object, or "useTarget"
		if(clickTracker){
			useTarget = (clickTracker == "useTarget");
			clickTarget = (useTarget?markedNode:e.target);
			if(useTarget){
				// We expect a click, so prevent any other
				// default action on "touchpress"
				e.preventDefault();
			}
			clickX = e.changedTouches ? e.changedTouches[0].pageX - win.global.pageXOffset : e.clientX;
			clickY = e.changedTouches ? e.changedTouches[0].pageY - win.global.pageYOffset : e.clientY;
			clickDx = (typeof clickTracker == "object" ? clickTracker.x : (typeof clickTracker == "number" ? clickTracker : 0)) || 4;
			clickDy = (typeof clickTracker == "object" ? clickTracker.y : (typeof clickTracker == "number" ? clickTracker : 0)) || 4;

			// add move/end handlers only the first time a node with dojoClick is seen,
			// so we don't add too much overhead when dojoClick is never set.
			if(!clicksInited){
				clicksInited = true;

				function updateClickTracker(e){
					if(useTarget){
						clickTracker = dom.isDescendant(
							win.doc.elementFromPoint(
								(e.changedTouches ? e.changedTouches[0].pageX - win.global.pageXOffset : e.clientX),
								(e.changedTouches ? e.changedTouches[0].pageY - win.global.pageYOffset : e.clientY)),
							clickTarget);
					}else{
						clickTracker = clickTracker &&
							(e.changedTouches ? e.changedTouches[0].target : e.target) == clickTarget &&
							Math.abs((e.changedTouches ? e.changedTouches[0].pageX - win.global.pageXOffset : e.clientX) - clickX) <= clickDx &&
							Math.abs((e.changedTouches ? e.changedTouches[0].pageY - win.global.pageYOffset : e.clientY) - clickY) <= clickDy;
					}
				}

				win.doc.addEventListener(moveType, function(e){
					if(mouse.isRight(e)){
						return;		// avoid spurious dojoclick event on IE10+; right click is just for context menu
					}
					updateClickTracker(e);
					if(useTarget){
						// prevent native scroll event and ensure touchend is
						// fire after touch moves between press and release.
						e.preventDefault();
					}
				}, true);

				win.doc.addEventListener(endType, function(e){
					if(mouse.isRight(e)){
						return;		// avoid spurious dojoclick event on IE10+; right click is just for context menu
					}
					updateClickTracker(e);
					if(clickTracker){
						clickTime = (new Date()).getTime();
						var target = (useTarget?clickTarget:e.target);
						if(target.tagName === "LABEL"){
							// when clicking on a label, forward click to its associated input if any
							target = dom.byId(target.getAttribute("for")) || target;
						}
						//some attributes can be on the Touch object, not on the Event:
						//http://www.w3.org/TR/touch-events/#touch-interface
						var src = (e.changedTouches) ? e.changedTouches[0] : e;
						function createMouseEvent(type){
							//create the synthetic event.
							//http://www.w3.org/TR/DOM-Level-3-Events/#widl-MouseEvent-initMouseEvent
							var evt = document.createEvent("MouseEvents");
							evt._dojo_click = true;
							evt.initMouseEvent(type,
								true, //bubbles
								true, //cancelable
								e.view,
								e.detail,
								src.screenX,
								src.screenY,
								src.clientX,
								src.clientY,
								e.ctrlKey,
								e.altKey,
								e.shiftKey,
								e.metaKey,
								0, //button
								null //related target
							);
							return evt;
						}
						var mouseDownEvt = createMouseEvent("mousedown");
						var mouseUpEvt = createMouseEvent("mouseup");
						var clickEvt = createMouseEvent("click");

						setTimeout(function(){
							on.emit(target, "mousedown", mouseDownEvt);
							on.emit(target, "mouseup", mouseUpEvt);
							on.emit(target, "click", clickEvt);

							// refresh clickTime in case app-defined click handler took a long time to run
							clickTime = (new Date()).getTime();
						}, 0);
					}
				}, true);

				function stopNativeEvents(type){
					win.doc.addEventListener(type, function(e){
						// Stop native events when we emitted our own click event.  Note that the native click may occur
						// on a different node than the synthetic click event was generated on.  For example,
						// click on a menu item, causing the menu to disappear, and then (~300ms later) the browser
						// sends a click event to the node that was *underneath* the menu.  So stop all native events
						// sent shortly after ours, similar to what is done in dualEvent.
						// The INPUT.dijitOffScreen test is for offscreen inputs used in dijit/form/Button, on which
						// we call click() explicitly, we don't want to stop this event.
						var target = e.target;
						if(clickTracker && !e._dojo_click &&
								(new Date()).getTime() <= clickTime + 1000 &&
								!(target.tagName == "INPUT" && domClass.contains(target, "dijitOffScreen"))){
							e.stopPropagation();
							e.stopImmediatePropagation && e.stopImmediatePropagation();
							if(type == "click" &&
								(target.tagName != "INPUT" ||
								(target.type == "radio" &&
									// #18352 Do not preventDefault for radios that are not dijit or
									// dojox/mobile widgets.
									// (The CSS class dijitCheckBoxInput holds for both checkboxes and radio buttons.)
									(domClass.contains(target, "dijitCheckBoxInput") ||
										domClass.contains(target, "mblRadioButton"))) ||
								(target.type == "checkbox" &&
									// #18352 Do not preventDefault for checkboxes that are not dijit or
									// dojox/mobile widgets.
									(domClass.contains(target, "dijitCheckBoxInput") ||
										domClass.contains(target, "mblCheckBox")))) &&
								target.tagName != "TEXTAREA" && target.tagName != "AUDIO" && target.tagName != "VIDEO"){
								// preventDefault() breaks textual <input>s on android, keyboard doesn't popup,
								// but it is still needed for checkboxes and radio buttons, otherwise in some cases
								// the checked state becomes inconsistent with the widget's state
								e.preventDefault();
							}
						}
					}, true);
				}

				stopNativeEvents("click");

				// We also stop mousedown/up since these would be sent well after with our "fast" click (300ms),
				// which can confuse some dijit widgets.
				stopNativeEvents("mousedown");
				stopNativeEvents("mouseup");
			}
		}
	}

	var hoveredNode;

	if(has("touch")){
		if(hasPointer){
			// MSPointer (IE10+) already has support for over and out, so we just need to init click support
			domReady(function(){
				win.doc.addEventListener(pointer.down, function(evt){
					doClicks(evt, pointer.move, pointer.up);
				}, true);
			});
		}else{
			domReady(function(){
				// Keep track of currently hovered node
				hoveredNode = win.body();	// currently hovered node

				win.doc.addEventListener("touchstart", function(evt){
						lastTouch = (new Date()).getTime();

					// Precede touchstart event with touch.over event.  DnD depends on this.
					// Use addEventListener(cb, true) to run cb before any touchstart handlers on node run,
					// and to ensure this code runs even if the listener on the node does event.stop().
					var oldNode = hoveredNode;
					hoveredNode = evt.target;
					on.emit(oldNode, "dojotouchout", {
						relatedTarget: hoveredNode,
						bubbles: true
					});
					on.emit(hoveredNode, "dojotouchover", {
						relatedTarget: oldNode,
						bubbles: true
					});

					doClicks(evt, "touchmove", "touchend"); // init click generation
				}, true);

				function copyEventProps(evt){
					// Make copy of event object and also set bubbles:true.  Used when calling on.emit().
					var props = lang.delegate(evt, {
						bubbles: true
					});

					if(has("ios") >= 6){
						// On iOS6 "touches" became a non-enumerable property, which
						// is not hit by for...in.  Ditto for the other properties below.
						props.touches = evt.touches;
						props.altKey = evt.altKey;
						props.changedTouches = evt.changedTouches;
						props.ctrlKey = evt.ctrlKey;
						props.metaKey = evt.metaKey;
						props.shiftKey = evt.shiftKey;
						props.targetTouches = evt.targetTouches;
					}

					return props;
				}

				on(win.doc, "touchmove", function(evt){
					lastTouch = (new Date()).getTime();

					var newNode = win.doc.elementFromPoint(
						evt.pageX - (ios4 ? 0 : win.global.pageXOffset), // iOS 4 expects page coords
						evt.pageY - (ios4 ? 0 : win.global.pageYOffset)
					);

					if(newNode){
						// Fire synthetic touchover and touchout events on nodes since the browser won't do it natively.
						if(hoveredNode !== newNode){
							// touch out on the old node
							on.emit(hoveredNode, "dojotouchout", {
								relatedTarget: newNode,
								bubbles: true
							});

							// touchover on the new node
							on.emit(newNode, "dojotouchover", {
								relatedTarget: hoveredNode,
								bubbles: true
							});

							hoveredNode = newNode;
						}

						// Unlike a listener on "touchmove", on(node, "dojotouchmove", listener) fires when the finger
						// drags over the specified node, regardless of which node the touch started on.
						if(!on.emit(newNode, "dojotouchmove", copyEventProps(evt))){
							// emit returns false when synthetic event "dojotouchmove" is cancelled, so we prevent the
							// default behavior of the underlying native event "touchmove".
							evt.preventDefault();
						}
					}
				});

				// Fire a dojotouchend event on the node where the finger was before it was removed from the screen.
				// This is different than the native touchend, which fires on the node where the drag started.
				on(win.doc, "touchend", function(evt){
						lastTouch = (new Date()).getTime();
					var node = win.doc.elementFromPoint(
						evt.pageX - (ios4 ? 0 : win.global.pageXOffset), // iOS 4 expects page coords
						evt.pageY - (ios4 ? 0 : win.global.pageYOffset)
					) || win.body(); // if out of the screen

					on.emit(node, "dojotouchend", copyEventProps(evt));
				});
			});
		}
	}

	//device neutral events - touch.press|move|release|cancel/over/out
	var touch = {
		press: dualEvent("mousedown", "touchstart", pointer.down),
		move: dualEvent("mousemove", "dojotouchmove", pointer.move),
		release: dualEvent("mouseup", "dojotouchend", pointer.up),
		cancel: dualEvent(mouse.leave, "touchcancel", hasPointer ? pointer.cancel : null),
		over: dualEvent("mouseover", "dojotouchover", pointer.over),
		out: dualEvent("mouseout", "dojotouchout", pointer.out),
		enter: mouse._eventHandler(dualEvent("mouseover","dojotouchover", pointer.over)),
		leave: mouse._eventHandler(dualEvent("mouseout", "dojotouchout", pointer.out))
	};

	/*=====
	touch = {
		// summary:
		//		This module provides unified touch event handlers by exporting
		//		press, move, release and cancel which can also run well on desktop.
		//		Based on http://dvcs.w3.org/hg/webevents/raw-file/tip/touchevents.html
		//      Also, if the dojoClick property is set to truthy on a DOM node, dojo/touch generates
		//      click events immediately for this node and its descendants (except for descendants that
		//      have a dojoClick property set to falsy), to avoid the delay before native browser click events,
		//      and regardless of whether evt.preventDefault() was called in a touch.press event listener.
		//
		// example:
		//		Used with dojo/on
		//		|	define(["dojo/on", "dojo/touch"], function(on, touch){
		//		|		on(node, touch.press, function(e){});
		//		|		on(node, touch.move, function(e){});
		//		|		on(node, touch.release, function(e){});
		//		|		on(node, touch.cancel, function(e){});
		// example:
		//		Used with touch.* directly
		//		|	touch.press(node, function(e){});
		//		|	touch.move(node, function(e){});
		//		|	touch.release(node, function(e){});
		//		|	touch.cancel(node, function(e){});
		// example:
		//		Have dojo/touch generate clicks without delay, with a default move threshold of 4 pixels
		//		|	node.dojoClick = true;
		// example:
		//		Have dojo/touch generate clicks without delay, with a move threshold of 10 pixels horizontally and vertically
		//		|	node.dojoClick = 10;
		// example:
		//		Have dojo/touch generate clicks without delay, with a move threshold of 50 pixels horizontally and 10 pixels vertically
		//		|	node.dojoClick = {x:50, y:5};
		// example:
		//		Disable clicks without delay generated by dojo/touch on a node that has an ancestor with property dojoClick set to truthy
		//		|  node.dojoClick = false;

		press: function(node, listener){
			// summary:
			//		Register a listener to 'touchstart'|'mousedown' for the given node
			// node: Dom
			//		Target node to listen to
			// listener: Function
			//		Callback function
			// returns:
			//		A handle which will be used to remove the listener by handle.remove()
		},
		move: function(node, listener){
			// summary:
			//		Register a listener that fires when the mouse cursor or a finger is dragged over the given node.
			// node: Dom
			//		Target node to listen to
			// listener: Function
			//		Callback function
			// returns:
			//		A handle which will be used to remove the listener by handle.remove()
		},
		release: function(node, listener){
			// summary:
			//		Register a listener to releasing the mouse button while the cursor is over the given node
			//		(i.e. "mouseup") or for removing the finger from the screen while touching the given node.
			// node: Dom
			//		Target node to listen to
			// listener: Function
			//		Callback function
			// returns:
			//		A handle which will be used to remove the listener by handle.remove()
		},
		cancel: function(node, listener){
			// summary:
			//		Register a listener to 'touchcancel'|'mouseleave' for the given node
			// node: Dom
			//		Target node to listen to
			// listener: Function
			//		Callback function
			// returns:
			//		A handle which will be used to remove the listener by handle.remove()
		},
		over: function(node, listener){
			// summary:
			//		Register a listener to 'mouseover' or touch equivalent for the given node
			// node: Dom
			//		Target node to listen to
			// listener: Function
			//		Callback function
			// returns:
			//		A handle which will be used to remove the listener by handle.remove()
		},
		out: function(node, listener){
			// summary:
			//		Register a listener to 'mouseout' or touch equivalent for the given node
			// node: Dom
			//		Target node to listen to
			// listener: Function
			//		Callback function
			// returns:
			//		A handle which will be used to remove the listener by handle.remove()
		},
		enter: function(node, listener){
			// summary:
			//		Register a listener to mouse.enter or touch equivalent for the given node
			// node: Dom
			//		Target node to listen to
			// listener: Function
			//		Callback function
			// returns:
			//		A handle which will be used to remove the listener by handle.remove()
		},
		leave: function(node, listener){
			// summary:
			//		Register a listener to mouse.leave or touch equivalent for the given node
			// node: Dom
			//		Target node to listen to
			// listener: Function
			//		Callback function
			// returns:
			//		A handle which will be used to remove the listener by handle.remove()
		}
	};
	=====*/

	 1  && (dojo.touch = touch);

	return touch;
});

},
'dijit/_FocusMixin':function(){
define([
	"./focus",
	"./_WidgetBase",
	"dojo/_base/declare", // declare
	"dojo/_base/lang" // lang.extend
], function(focus, _WidgetBase, declare, lang){

	// module:
	//		dijit/_FocusMixin

	// We don't know where _FocusMixin will occur in the inheritance chain, but we need the _onFocus()/_onBlur() below
	// to be last in the inheritance chain, so mixin to _WidgetBase.
	lang.extend(_WidgetBase, {
		// focused: [readonly] Boolean
		//		This widget or a widget it contains has focus, or is "active" because
		//		it was recently clicked.
		focused: false,

		onFocus: function(){
			// summary:
			//		Called when the widget becomes "active" because
			//		it or a widget inside of it either has focus, or has recently
			//		been clicked.
			// tags:
			//		callback
		},

		onBlur: function(){
			// summary:
			//		Called when the widget stops being "active" because
			//		focus moved to something outside of it, or the user
			//		clicked somewhere outside of it, or the widget was
			//		hidden.
			// tags:
			//		callback
		},

		_onFocus: function(){
			// summary:
			//		This is where widgets do processing for when they are active,
			//		such as changing CSS classes.  See onFocus() for more details.
			// tags:
			//		protected
			this.onFocus();
		},

		_onBlur: function(){
			// summary:
			//		This is where widgets do processing for when they stop being active,
			//		such as changing CSS classes.  See onBlur() for more details.
			// tags:
			//		protected
			this.onBlur();
		}
	});

	return declare("dijit._FocusMixin", null, {
		// summary:
		//		Mixin to widget to provide _onFocus() and _onBlur() methods that
		//		fire when a widget or its descendants get/lose focus

		// flag that I want _onFocus()/_onBlur() notifications from focus manager
		_focusManager: focus
	});

});

},
'dijit/focus':function(){
define([
	"dojo/aspect",
	"dojo/_base/declare", // declare
	"dojo/dom", // domAttr.get dom.isDescendant
	"dojo/dom-attr", // domAttr.get dom.isDescendant
	"dojo/dom-class",
	"dojo/dom-construct", // connect to domConstruct.empty, domConstruct.destroy
	"dojo/Evented",
	"dojo/_base/lang", // lang.hitch
	"dojo/on",
	"dojo/domReady",
	"dojo/sniff", // has("ie")
	"dojo/Stateful",
	"dojo/_base/window", // win.body
	"dojo/window", // winUtils.get
	"./a11y",	// a11y.isTabNavigable
	"./registry",	// registry.byId
	"./main"		// to set dijit.focus
], function(aspect, declare, dom, domAttr, domClass, domConstruct, Evented, lang, on, domReady, has, Stateful, win, winUtils,
			a11y, registry, dijit){

	// module:
	//		dijit/focus

	// Time of the last focusin event
	var lastFocusin;

	// Time of the last touch/mousedown or focusin event
	var lastTouchOrFocusin;

	var FocusManager = declare([Stateful, Evented], {
		// summary:
		//		Tracks the currently focused node, and which widgets are currently "active".
		//		Access via require(["dijit/focus"], function(focus){ ... }).
		//
		//		A widget is considered active if it or a descendant widget has focus,
		//		or if a non-focusable node of this widget or a descendant was recently clicked.
		//
		//		Call focus.watch("curNode", callback) to track the current focused DOMNode,
		//		or focus.watch("activeStack", callback) to track the currently focused stack of widgets.
		//
		//		Call focus.on("widget-blur", func) or focus.on("widget-focus", ...) to monitor when
		//		when widgets become active/inactive
		//
		//		Finally, focus(node) will focus a node, suppressing errors if the node doesn't exist.

		// curNode: DomNode
		//		Currently focused item on screen
		curNode: null,

		// activeStack: dijit/_WidgetBase[]
		//		List of currently active widgets (focused widget and it's ancestors)
		activeStack: [],

		constructor: function(){
			// Don't leave curNode/prevNode pointing to bogus elements
			var check = lang.hitch(this, function(node){
				if(dom.isDescendant(this.curNode, node)){
					this.set("curNode", null);
				}
				if(dom.isDescendant(this.prevNode, node)){
					this.set("prevNode", null);
				}
			});
			aspect.before(domConstruct, "empty", check);
			aspect.before(domConstruct, "destroy", check);
		},

		registerIframe: function(/*DomNode*/ iframe){
			// summary:
			//		Registers listeners on the specified iframe so that any click
			//		or focus event on that iframe (or anything in it) is reported
			//		as a focus/click event on the `<iframe>` itself.
			// description:
			//		Currently only used by editor.
			// returns:
			//		Handle with remove() method to deregister.
			return this.registerWin(iframe.contentWindow, iframe);
		},

		registerWin: function(/*Window?*/targetWindow, /*DomNode?*/ effectiveNode){
			// summary:
			//		Registers listeners on the specified window (either the main
			//		window or an iframe's window) to detect when the user has clicked somewhere
			//		or focused somewhere.
			// description:
			//		Users should call registerIframe() instead of this method.
			// targetWindow:
			//		If specified this is the window associated with the iframe,
			//		i.e. iframe.contentWindow.
			// effectiveNode:
			//		If specified, report any focus events inside targetWindow as
			//		an event on effectiveNode, rather than on evt.target.
			// returns:
			//		Handle with remove() method to deregister.

			// TODO: make this function private in 2.0; Editor/users should call registerIframe(),

			// Listen for blur and focus events on targetWindow's document.
			var _this = this,
				body = targetWindow.document && targetWindow.document.body;

			if(body){
				// Listen for touches or mousedowns... could also use dojo/touch.press here.
				var event = has("pointer-events") ? "pointerdown" : has("MSPointer") ? "MSPointerDown" :
					has("touch-events") ? "mousedown, touchstart" : "mousedown";
				var mdh = on(targetWindow.document, event, function(evt){
					// workaround weird IE bug where the click is on an orphaned node
					// (first time clicking a Select/DropDownButton inside a TooltipDialog).
					// actually, strangely this is happening on latest chrome too.
					if(evt && evt.target && evt.target.parentNode == null){
						return;
					}

					_this._onTouchNode(effectiveNode || evt.target, "mouse");
				});

				var fih = on(body, 'focusin', function(evt){
					// When you refocus the browser window, IE gives an event with an empty srcElement
					if(!evt.target.tagName) { return; }

					// IE reports that nodes like <body> have gotten focus, even though they have tabIndex=-1,
					// ignore those events
					var tag = evt.target.tagName.toLowerCase();
					if(tag == "#document" || tag == "body"){ return; }

					if(a11y.isFocusable(evt.target)){
						_this._onFocusNode(effectiveNode || evt.target);
					}else{
						// Previous code called _onTouchNode() for any activate event on a non-focusable node.   Can
						// probably just ignore such an event as it will be handled by onmousedown handler above, but
						// leaving the code for now.
						_this._onTouchNode(effectiveNode || evt.target);
					}
				});

				var foh = on(body, 'focusout', function(evt){
					_this._onBlurNode(effectiveNode || evt.target);
				});

				return {
					remove: function(){
						mdh.remove();
						fih.remove();
						foh.remove();
						mdh = fih = foh = null;
						body = null;	// prevent memory leak (apparent circular reference via closure)
					}
				};
			}
		},

		_onBlurNode: function(/*DomNode*/ node){
			// summary:
			//		Called when focus leaves a node.
			//		Usually ignored, _unless_ it *isn't* followed by touching another node,
			//		which indicates that we tabbed off the last field on the page,
			//		in which case every widget is marked inactive

			var now = (new Date()).getTime();

			// IE9+ and chrome have a problem where focusout events come after the corresponding focusin event.
			// For chrome problem see https://bugs.dojotoolkit.org/ticket/17668.
			// IE problem happens when moving focus from the Editor's <iframe> to a normal DOMNode.
			if(now < lastFocusin + 100){
				return;
			}

			// If the blur event isn't followed by a focus event, it means the user clicked on something unfocusable,
			// so clear focus.
			if(this._clearFocusTimer){
				clearTimeout(this._clearFocusTimer);
			}
			this._clearFocusTimer = setTimeout(lang.hitch(this, function(){
				this.set("prevNode", this.curNode);
				this.set("curNode", null);
			}), 0);

			// Unset timer to zero-out widget stack; we'll reset it below if appropriate.
			if(this._clearActiveWidgetsTimer){
				clearTimeout(this._clearActiveWidgetsTimer);
			}

			if(now < lastTouchOrFocusin + 100){
				// This blur event is coming late (after the call to _onTouchNode() rather than before.
				// So let _onTouchNode() handle setting the widget stack.
				// See https://bugs.dojotoolkit.org/ticket/17668
				return;
			}

			// If the blur event isn't followed (or preceded) by a focus or touch event then mark all widgets as inactive.
			this._clearActiveWidgetsTimer = setTimeout(lang.hitch(this, function(){
				delete this._clearActiveWidgetsTimer;
				this._setStack([]);
			}), 0);
		},

		_onTouchNode: function(/*DomNode*/ node, /*String*/ by){
			// summary:
			//		Callback when node is focused or touched.
			//		Note that _onFocusNode() calls _onTouchNode().
			// node:
			//		The node that was touched.
			// by:
			//		"mouse" if the focus/touch was caused by a mouse down event

			// Keep track of time of last focusin or touch event.
			lastTouchOrFocusin = (new Date()).getTime();

			if(this._clearActiveWidgetsTimer){
				// forget the recent blur event
				clearTimeout(this._clearActiveWidgetsTimer);
				delete this._clearActiveWidgetsTimer;
			}

			// if the click occurred on the scrollbar of a dropdown, treat it as a click on the dropdown,
			// even though the scrollbar is technically on the popup wrapper (see #10631)
			if(domClass.contains(node, "dijitPopup")){
				node = node.firstChild;
			}

			// compute stack of active widgets (ex: ComboButton --> Menu --> MenuItem)
			var newStack=[];
			try{
				while(node){
					var popupParent = domAttr.get(node, "dijitPopupParent");
					if(popupParent){
						node=registry.byId(popupParent).domNode;
					}else if(node.tagName && node.tagName.toLowerCase() == "body"){
						// is this the root of the document or just the root of an iframe?
						if(node === win.body()){
							// node is the root of the main document
							break;
						}
						// otherwise, find the iframe this node refers to (can't access it via parentNode,
						// need to do this trick instead). window.frameElement is supported in IE/FF/Webkit
						node=winUtils.get(node.ownerDocument).frameElement;
					}else{
						// if this node is the root node of a widget, then add widget id to stack,
						// except ignore clicks on disabled widgets (actually focusing a disabled widget still works,
						// to support MenuItem)
						var id = node.getAttribute && node.getAttribute("widgetId"),
							widget = id && registry.byId(id);
						if(widget && !(by == "mouse" && widget.get("disabled"))){
							newStack.unshift(id);
						}
						node=node.parentNode;
					}
				}
			}catch(e){ /* squelch */ }

			this._setStack(newStack, by);
		},

		_onFocusNode: function(/*DomNode*/ node){
			// summary:
			//		Callback when node is focused

			if(!node){
				return;
			}

			if(node.nodeType == 9){
				// Ignore focus events on the document itself.  This is here so that
				// (for example) clicking the up/down arrows of a spinner
				// (which don't get focus) won't cause that widget to blur. (FF issue)
				return;
			}

			// Keep track of time of last focusin event.
			lastFocusin = (new Date()).getTime();

			// There was probably a blur event right before this event, but since we have a new focus,
			// forget about the blur
			if(this._clearFocusTimer){
				clearTimeout(this._clearFocusTimer);
				delete this._clearFocusTimer;
			}

			this._onTouchNode(node);

			if(node == this.curNode){ return; }
			this.set("prevNode", this.curNode);
			this.set("curNode", node);
		},

		_setStack: function(/*String[]*/ newStack, /*String*/ by){
			// summary:
			//		The stack of active widgets has changed.  Send out appropriate events and records new stack.
			// newStack:
			//		array of widget id's, starting from the top (outermost) widget
			// by:
			//		"mouse" if the focus/touch was caused by a mouse down event

			var oldStack = this.activeStack, lastOldIdx = oldStack.length - 1, lastNewIdx = newStack.length - 1;

			if(newStack[lastNewIdx] == oldStack[lastOldIdx]){
				// no changes, return now to avoid spurious notifications about changes to activeStack
				return;
			}

			this.set("activeStack", newStack);

			var widget, i;

			// for all elements that have gone out of focus, set focused=false
			for(i = lastOldIdx; i >= 0 && oldStack[i] != newStack[i]; i--){
				widget = registry.byId(oldStack[i]);
				if(widget){
					widget._hasBeenBlurred = true;		// TODO: used by form widgets, should be moved there
					widget.set("focused", false);
					if(widget._focusManager == this){
						widget._onBlur(by);
					}
					this.emit("widget-blur", widget, by);
				}
			}

			// for all element that have come into focus, set focused=true
			for(i++; i <= lastNewIdx; i++){
				widget = registry.byId(newStack[i]);
				if(widget){
					widget.set("focused", true);
					if(widget._focusManager == this){
						widget._onFocus(by);
					}
					this.emit("widget-focus", widget, by);
				}
			}
		},

		focus: function(node){
			// summary:
			//		Focus the specified node, suppressing errors if they occur
			if(node){
				try{ node.focus(); }catch(e){/*quiet*/}
			}
		}
	});

	var singleton = new FocusManager();

	// register top window and all the iframes it contains
	domReady(function(){
		var handle = singleton.registerWin(winUtils.get(document));
		if(has("ie")){
			on(window, "unload", function(){
				if(handle){	// because this gets called twice when doh.robot is running
					handle.remove();
					handle = null;
				}
			});
		}
	});

	// Setup dijit.focus as a pointer to the singleton but also (for backwards compatibility)
	// as a function to set focus.   Remove for 2.0.
	dijit.focus = function(node){
		singleton.focus(node);	// indirection here allows dijit/_base/focus.js to override behavior
	};
	for(var attr in singleton){
		if(!/^_/.test(attr)){
			dijit.focus[attr] = typeof singleton[attr] == "function" ? lang.hitch(singleton, attr) : singleton[attr];
		}
	}
	singleton.watch(function(attr, oldVal, newVal){
		dijit.focus[attr] = newVal;
	});

	return singleton;
});

},
'dojo/window':function(){
define(["./_base/lang", "./sniff", "./_base/window", "./dom", "./dom-geometry", "./dom-style", "./dom-construct"],
	function(lang, has, baseWindow, dom, geom, style, domConstruct){

	// feature detection
	/* not needed but included here for future reference
	has.add("rtl-innerVerticalScrollBar-on-left", function(win, doc){
		var	body = baseWindow.body(doc),
			scrollable = domConstruct.create('div', {
				style: {overflow:'scroll', overflowX:'hidden', direction:'rtl', visibility:'hidden', position:'absolute', left:'0', width:'64px', height:'64px'}
			}, body, "last"),
			center = domConstruct.create('center', {
				style: {overflow:'hidden', direction:'ltr'}
			}, scrollable, "last"),
			inner = domConstruct.create('div', {
				style: {overflow:'visible', display:'inline' }
			}, center, "last");
		inner.innerHTML="&nbsp;";
		var midPoint = Math.max(inner.offsetLeft, geom.position(inner).x);
		var ret = midPoint >= 32;
		center.removeChild(inner);
		scrollable.removeChild(center);
		body.removeChild(scrollable);
		return ret;
	});
	*/
	has.add("rtl-adjust-position-for-verticalScrollBar", function(win, doc){
		var	body = baseWindow.body(doc),
			scrollable = domConstruct.create('div', {
				style: {overflow:'scroll', overflowX:'visible', direction:'rtl', visibility:'hidden', position:'absolute', left:'0', top:'0', width:'64px', height:'64px'}
			}, body, "last"),
			div = domConstruct.create('div', {
				style: {overflow:'hidden', direction:'ltr'}
			}, scrollable, "last"),
			ret = geom.position(div).x != 0;
		scrollable.removeChild(div);
		body.removeChild(scrollable);
		return ret;
	});

	has.add("position-fixed-support", function(win, doc){
		// IE6, IE7+quirks, and some older mobile browsers don't support position:fixed
		var	body = baseWindow.body(doc),
			outer = domConstruct.create('span', {
				style: {visibility:'hidden', position:'fixed', left:'1px', top:'1px'}
			}, body, "last"),
			inner = domConstruct.create('span', {
				style: {position:'fixed', left:'0', top:'0'}
			}, outer, "last"),
			ret = geom.position(inner).x != geom.position(outer).x;
		outer.removeChild(inner);
		body.removeChild(outer);
		return ret;
	});

	// module:
	//		dojo/window

	var window = {
		// summary:
		//		TODOC

		getBox: function(/*Document?*/ doc){
			// summary:
			//		Returns the dimensions and scroll position of the viewable area of a browser window

			doc = doc || baseWindow.doc;

			var
				scrollRoot = (doc.compatMode == 'BackCompat') ? baseWindow.body(doc) : doc.documentElement,
				// get scroll position
				scroll = geom.docScroll(doc), // scrollRoot.scrollTop/Left should work
				w, h;

			if(has("touch")){ // if(scrollbars not supported)
				var uiWindow = window.get(doc);   // use UI window, not dojo.global window
				// on mobile, scrollRoot.clientHeight <= uiWindow.innerHeight <= scrollRoot.offsetHeight, return uiWindow.innerHeight
				w = uiWindow.innerWidth || scrollRoot.clientWidth; // || scrollRoot.clientXXX probably never evaluated
				h = uiWindow.innerHeight || scrollRoot.clientHeight;
			}else{
				// on desktops, scrollRoot.clientHeight <= scrollRoot.offsetHeight <= uiWindow.innerHeight, return scrollRoot.clientHeight
				// uiWindow.innerWidth/Height includes the scrollbar and cannot be used
				w = scrollRoot.clientWidth;
				h = scrollRoot.clientHeight;
			}
			return {
				l: scroll.x,
				t: scroll.y,
				w: w,
				h: h
			};
		},

		get: function(/*Document*/ doc){
			// summary:
			//		Get window object associated with document doc.
			// doc:
			//		The document to get the associated window for.

			// In some IE versions (at least 6.0), document.parentWindow does not return a
			// reference to the real window object (maybe a copy), so we must fix it as well
			// We use IE specific execScript to attach the real window reference to
			// document._parentWindow for later use
			if(has("ie") && window !== document.parentWindow){
				/*
				In IE 6, only the variable "window" can be used to connect events (others
				may be only copies).
				*/
				doc.parentWindow.execScript("document._parentWindow = window;", "Javascript");
				//to prevent memory leak, unset it after use
				//another possibility is to add an onUnload handler which seems overkill to me (liucougar)
				var win = doc._parentWindow;
				doc._parentWindow = null;
				return win;	//	Window
			}

			return doc.parentWindow || doc.defaultView;	//	Window
		},

		scrollIntoView: function(/*DomNode*/ node, /*Object?*/ pos){
			// summary:
			//		Scroll the passed node into view using minimal movement, if it is not already.

			// Don't rely on node.scrollIntoView working just because the function is there since
			// it forces the node to the page's bottom or top (and left or right in IE) without consideration for the minimal movement.
			// WebKit's node.scrollIntoViewIfNeeded doesn't work either for inner scrollbars in right-to-left mode
			// and when there's a fixed position scrollable element

			try{ // catch unexpected/unrecreatable errors (#7808) since we can recover using a semi-acceptable native method
				node = dom.byId(node);
				var	doc = node.ownerDocument || baseWindow.doc,	// TODO: why baseWindow.doc?  Isn't node.ownerDocument always defined?
					body = baseWindow.body(doc),
					html = doc.documentElement || body.parentNode,
					isIE = has("ie") || has("trident"),
					isWK = has("webkit");
				// if an untested browser, then use the native method
				if(node == body || node == html){ return; }
				if(!(has("mozilla") || isIE || isWK || has("opera") || has("trident") || has("edge"))
						&& ("scrollIntoView" in node)){
					node.scrollIntoView(false); // short-circuit to native if possible
					return;
				}
				var	backCompat = doc.compatMode == 'BackCompat',
					rootWidth = Math.min(body.clientWidth || html.clientWidth, html.clientWidth || body.clientWidth),
					rootHeight = Math.min(body.clientHeight || html.clientHeight, html.clientHeight || body.clientHeight),
					scrollRoot = (isWK || backCompat) ? body : html,
					nodePos = pos || geom.position(node),
					el = node.parentNode,
					isFixed = function(el){
						return (isIE <= 6 || (isIE == 7 && backCompat))
							? false
							: (has("position-fixed-support") && (style.get(el, 'position').toLowerCase() == "fixed"));
					},
					self = this,
					scrollElementBy = function(el, x, y){
						if(el.tagName == "BODY" || el.tagName == "HTML"){
							self.get(el.ownerDocument).scrollBy(x, y);
						}else{
							x && (el.scrollLeft += x);
							y && (el.scrollTop += y);
						}
					};
				if(isFixed(node)){ return; } // nothing to do
				while(el){
					if(el == body){ el = scrollRoot; }
					var	elPos = geom.position(el),
						fixedPos = isFixed(el),
						rtl = style.getComputedStyle(el).direction.toLowerCase() == "rtl";

					if(el == scrollRoot){
						elPos.w = rootWidth; elPos.h = rootHeight;
						if(scrollRoot == html && (isIE || has("trident")) && rtl){
							elPos.x += scrollRoot.offsetWidth-elPos.w;// IE workaround where scrollbar causes negative x
						}
						elPos.x = 0;
						elPos.y = 0;
					}else{
						var pb = geom.getPadBorderExtents(el);
						elPos.w -= pb.w; elPos.h -= pb.h; elPos.x += pb.l; elPos.y += pb.t;
						var clientSize = el.clientWidth,
							scrollBarSize = elPos.w - clientSize;
						if(clientSize > 0 && scrollBarSize > 0){
							if(rtl && has("rtl-adjust-position-for-verticalScrollBar")){
								elPos.x += scrollBarSize;
							}
							elPos.w = clientSize;
						}
						clientSize = el.clientHeight;
						scrollBarSize = elPos.h - clientSize;
						if(clientSize > 0 && scrollBarSize > 0){
							elPos.h = clientSize;
						}
					}
					if(fixedPos){ // bounded by viewport, not parents
						if(elPos.y < 0){
							elPos.h += elPos.y; elPos.y = 0;
						}
						if(elPos.x < 0){
							elPos.w += elPos.x; elPos.x = 0;
						}
						if(elPos.y + elPos.h > rootHeight){
							elPos.h = rootHeight - elPos.y;
						}
						if(elPos.x + elPos.w > rootWidth){
							elPos.w = rootWidth - elPos.x;
						}
					}
					// calculate overflow in all 4 directions
					var	l = nodePos.x - elPos.x, // beyond left: < 0
//						t = nodePos.y - Math.max(elPos.y, 0), // beyond top: < 0
						t = nodePos.y - elPos.y, // beyond top: < 0
						r = l + nodePos.w - elPos.w, // beyond right: > 0
						bot = t + nodePos.h - elPos.h; // beyond bottom: > 0
					var s, old;
					if(r * l > 0 && (!!el.scrollLeft || el == scrollRoot || el.scrollWidth > el.offsetHeight)){
						s = Math[l < 0? "max" : "min"](l, r);
						if(rtl && ((isIE == 8 && !backCompat) || has("trident") >= 5)){ s = -s; }
						old = el.scrollLeft;
						scrollElementBy(el, s, 0);
						s = el.scrollLeft - old;
						nodePos.x -= s;
					}
					if(bot * t > 0 && (!!el.scrollTop || el == scrollRoot || el.scrollHeight > el.offsetHeight)){
						s = Math.ceil(Math[t < 0? "max" : "min"](t, bot));
						old = el.scrollTop;
						scrollElementBy(el, 0, s);
						s = el.scrollTop - old;
						nodePos.y -= s;
					}
					el = (el != scrollRoot) && !fixedPos && el.parentNode;
				}
			}catch(error){
				console.error('scrollIntoView: ' + error);
				node.scrollIntoView(false);
			}
		}
	};

	 1  && lang.setObject("dojo.window", window);

	return window;
});

},
'dijit/a11y':function(){
define([
	"dojo/_base/array", // array.forEach array.map
	"dojo/dom",			// dom.byId
	"dojo/dom-attr", // domAttr.attr domAttr.has
	"dojo/dom-style", // domStyle.style
	"dojo/_base/lang", // lang.mixin()
	"dojo/sniff", // has("ie")  1 
	"./main"	// for exporting methods to dijit namespace
], function(array, dom, domAttr, domStyle, lang, has, dijit){

	// module:
	//		dijit/a11y

	var undefined;

	var a11y = {
		// summary:
		//		Accessibility utility functions (keyboard, tab stops, etc.)

		_isElementShown: function(/*Element*/ elem){
			var s = domStyle.get(elem);
			return (s.visibility != "hidden")
				&& (s.visibility != "collapsed")
				&& (s.display != "none")
				&& (domAttr.get(elem, "type") != "hidden");
		},

		hasDefaultTabStop: function(/*Element*/ elem){
			// summary:
			//		Tests if element is tab-navigable even without an explicit tabIndex setting

			// No explicit tabIndex setting, need to investigate node type
			switch(elem.nodeName.toLowerCase()){
				case "a":
					// An <a> w/out a tabindex is only navigable if it has an href
					return domAttr.has(elem, "href");
				case "area":
				case "button":
				case "input":
				case "object":
				case "select":
				case "textarea":
					// These are navigable by default
					return true;
				case "iframe":
					// If it's an editor <iframe> then it's tab navigable.
					var body;
					try{
						// non-IE
						var contentDocument = elem.contentDocument;
						if("designMode" in contentDocument && contentDocument.designMode == "on"){
							return true;
						}
						body = contentDocument.body;
					}catch(e1){
						// contentWindow.document isn't accessible within IE7/8
						// if the iframe.src points to a foreign url and this
						// page contains an element, that could get focus
						try{
							body = elem.contentWindow.document.body;
						}catch(e2){
							return false;
						}
					}
					return body && (body.contentEditable == 'true' ||
						(body.firstChild && body.firstChild.contentEditable == 'true'));
				default:
					return elem.contentEditable == 'true';
			}
		},

		effectiveTabIndex: function(/*Element*/ elem){
			// summary:
			//		Returns effective tabIndex of an element, either a number, or undefined if element isn't focusable.

			if(domAttr.get(elem, "disabled")){
				return undefined;
			}else if(domAttr.has(elem, "tabIndex")){
				// Explicit tab index setting
				return +domAttr.get(elem, "tabIndex");// + to convert string --> number
			}else{
				// No explicit tabIndex setting, so depends on node type
				return a11y.hasDefaultTabStop(elem) ? 0 : undefined;
			}
		},

		isTabNavigable: function(/*Element*/ elem){
			// summary:
			//		Tests if an element is tab-navigable

			return a11y.effectiveTabIndex(elem) >= 0;
		},

		isFocusable: function(/*Element*/ elem){
			// summary:
			//		Tests if an element is focusable by tabbing to it, or clicking it with the mouse.

			return a11y.effectiveTabIndex(elem) >= -1;
		},

		_getTabNavigable: function(/*DOMNode*/ root){
			// summary:
			//		Finds descendants of the specified root node.
			// description:
			//		Finds the following descendants of the specified root node:
			//
			//		- the first tab-navigable element in document order
			//		  without a tabIndex or with tabIndex="0"
			//		- the last tab-navigable element in document order
			//		  without a tabIndex or with tabIndex="0"
			//		- the first element in document order with the lowest
			//		  positive tabIndex value
			//		- the last element in document order with the highest
			//		  positive tabIndex value
			var first, last, lowest, lowestTabindex, highest, highestTabindex, radioSelected = {};

			function radioName(node){
				// If this element is part of a radio button group, return the name for that group.
				return node && node.tagName.toLowerCase() == "input" &&
					node.type && node.type.toLowerCase() == "radio" &&
					node.name && node.name.toLowerCase();
			}

			var shown = a11y._isElementShown, effectiveTabIndex = a11y.effectiveTabIndex;
			var walkTree = function(/*DOMNode*/ parent){
				for(var child = parent.firstChild; child; child = child.nextSibling){
					// Skip text elements, hidden elements, and also non-HTML elements (those in custom namespaces) in IE,
					// since show() invokes getAttribute("type"), which crash on VML nodes in IE.
					if(child.nodeType != 1 || (has("ie") <= 9 && child.scopeName !== "HTML") || !shown(child)){
						continue;
					}

					var tabindex = effectiveTabIndex(child);
					if(tabindex >= 0){
						if(tabindex == 0){
							if(!first){
								first = child;
							}
							last = child;
						}else if(tabindex > 0){
							if(!lowest || tabindex < lowestTabindex){
								lowestTabindex = tabindex;
								lowest = child;
							}
							if(!highest || tabindex >= highestTabindex){
								highestTabindex = tabindex;
								highest = child;
							}
						}
						var rn = radioName(child);
						if(domAttr.get(child, "checked") && rn){
							radioSelected[rn] = child;
						}
					}
					if(child.nodeName.toUpperCase() != 'SELECT'){
						walkTree(child);
					}
				}
			};
			if(shown(root)){
				walkTree(root);
			}
			function rs(node){
				// substitute checked radio button for unchecked one, if there is a checked one with the same name.
				return radioSelected[radioName(node)] || node;
			}

			return { first: rs(first), last: rs(last), lowest: rs(lowest), highest: rs(highest) };
		},

		getFirstInTabbingOrder: function(/*String|DOMNode*/ root, /*Document?*/ doc){
			// summary:
			//		Finds the descendant of the specified root node
			//		that is first in the tabbing order
			var elems = a11y._getTabNavigable(dom.byId(root, doc));
			return elems.lowest ? elems.lowest : elems.first; // DomNode
		},

		getLastInTabbingOrder: function(/*String|DOMNode*/ root, /*Document?*/ doc){
			// summary:
			//		Finds the descendant of the specified root node
			//		that is last in the tabbing order
			var elems = a11y._getTabNavigable(dom.byId(root, doc));
			return elems.last ? elems.last : elems.highest; // DomNode
		}
	};

	 1  && lang.mixin(dijit, a11y);

	return a11y;
});

},
'dojo/uacss':function(){
define(["./dom-geometry", "./_base/lang", "./domReady", "./sniff", "./_base/window"],
	function(geometry, lang, domReady, has, baseWindow){

	// module:
	//		dojo/uacss

	/*=====
	return {
		// summary:
		//		Applies pre-set CSS classes to the top-level HTML node, based on:
		//
		//		- browser (ex: dj_ie)
		//		- browser version (ex: dj_ie6)
		//		- box model (ex: dj_contentBox)
		//		- text direction (ex: dijitRtl)
		//
		//		In addition, browser, browser version, and box model are
		//		combined with an RTL flag when browser text is RTL. ex: dj_ie-rtl.
		//
		//		Returns the has() method.
	};
	=====*/

	var
		html = baseWindow.doc.documentElement,
		ie = has("ie"),
		trident = has("trident"),
		opera = has("opera"),
		maj = Math.floor,
		ff = has("ff"),
		boxModel = geometry.boxModel.replace(/-/,''),

		classes = {
			"dj_quirks": has("quirks"),

			// NOTE: Opera not supported by dijit
			"dj_opera": opera,

			"dj_khtml": has("khtml"),

			"dj_webkit": has("webkit"),
			"dj_safari": has("safari"),
			"dj_chrome": has("chrome"),
			"dj_edge": has("edge"),

			"dj_gecko": has("mozilla"),

			"dj_ios": has("ios"),
			"dj_android": has("android")
		}; // no dojo unsupported browsers

	if(ie){
		classes["dj_ie"] = true;
		classes["dj_ie" + maj(ie)] = true;
		classes["dj_iequirks"] = has("quirks");
	}
	if(trident){
		classes["dj_trident"] = true;
		classes["dj_trident" + maj(trident)] = true;
	}
	if(ff){
		classes["dj_ff" + maj(ff)] = true;
	}

	classes["dj_" + boxModel] = true;

	// apply browser, browser version, and box model class names
	var classStr = "";
	for(var clz in classes){
		if(classes[clz]){
			classStr += clz + " ";
		}
	}
	html.className = lang.trim(html.className + " " + classStr);

	// If RTL mode, then add dj_rtl flag plus repeat existing classes with -rtl extension.
	// We can't run the code below until the <body> tag has loaded (so we can check for dir=rtl).
	domReady(function(){
		if(!geometry.isBodyLtr()){
			var rtlClassStr = "dj_rtl dijitRtl " + classStr.replace(/ /g, "-rtl ");
			html.className = lang.trim(html.className + " " + rtlClassStr + "dj_rtl dijitRtl " + classStr.replace(/ /g, "-rtl "));
		}
	});
	return has;
});

},
'dijit/hccss':function(){
define(["dojo/dom-class", "dojo/hccss", "dojo/domReady", "dojo/_base/window"], function(domClass, has, domReady, win){

	// module:
	//		dijit/hccss

	/*=====
	return function(){
		// summary:
		//		Test if computer is in high contrast mode, and sets `dijit_a11y` flag on `<body>` if it is.
		//		Deprecated, use ``dojo/hccss`` instead.
	};
	=====*/

	domReady(function(){
		if(has("highcontrast")){
			domClass.add(win.body(), "dijit_a11y");
		}
	});

	return has;
});

},
'dojo/hccss':function(){
define([
	"require",			// require, require.toUrl
	"./_base/config", // config.blankGif
	"./dom-class", // domClass.add
	"./dom-style", // domStyle.getComputedStyle
	"./has",
	"./domReady",
	"./_base/window" // win.body
], function(require, config, domClass, domStyle, has, domReady, win){

	// module:
	//		dojo/hccss

	/*=====
	return function(){
		// summary:
		//		Test if computer is in high contrast mode (i.e. if browser is not displaying background images).
		//		Defines `has("highcontrast")` and sets `dj_a11y` CSS class on `<body>` if machine is in high contrast mode.
		//		Returns `has()` method;
	};
	=====*/

	// Has() test for when background images aren't displayed.  Don't call has("highcontrast") before dojo/domReady!.
	has.add("highcontrast", function(){
		// note: if multiple documents, doesn't matter which one we use
		var div = win.doc.createElement("div");
		try{
			div.style.cssText = "border: 1px solid; border-color:red green; position: absolute; height: 5px; top: -999px;" +
				"background-image: url(\"" + (config.blankGif || require.toUrl("./resources/blank.gif")) + "\");";
			win.body().appendChild(div);

			var cs = domStyle.getComputedStyle(div),
				bkImg = cs.backgroundImage;
			return cs.borderTopColor == cs.borderRightColor ||
				(bkImg && (bkImg == "none" || bkImg == "url(invalid-url:)" ));
		}catch(e){
			console.warn("hccss: exception detecting high-contrast mode, document is likely hidden: " + e.toString());
			return false;
		}finally{
			if(has("ie") <= 8){
				div.outerHTML = "";		// prevent mixed-content warning, see http://support.microsoft.com/kb/925014
			}else{
				win.body().removeChild(div);
			}
		}
	});

	domReady(function(){
		if(has("highcontrast")){
			domClass.add(win.body(), "dj_a11y");
		}
	});

	return has;
});

},
'dojox/grid/_RowSelector':function(){
define([
	"dojo/_base/declare",
	"./_View"
], function(declare, _View){

return declare('dojox.grid._RowSelector', _View, {
	// summary:
	//	Custom grid view. If used in a grid structure, provides a small selectable region for grid rows.
	defaultWidth: "2em",
	noscroll: true,
	padBorderWidth: 2,
	buildRendering: function(){
		this.inherited('buildRendering', arguments);
		this.scrollboxNode.style.overflow = "hidden";
		this.headerNode.style.visibility = "hidden";
	},
	getWidth: function(){
		return this.viewWidth || this.defaultWidth;
	},
	buildRowContent: function(inRowIndex, inRowNode){
		var w = this.contentWidth || 0;
		inRowNode.innerHTML = '<table class="dojoxGridRowbarTable" style="width:' + w + 'px;height:1px;" border="0" cellspacing="0" cellpadding="0" role="presentation"><tr><td class="dojoxGridRowbarInner">&nbsp;</td></tr></table>';
	},
	renderHeader: function(){
	},
	updateRow: function(){
	},
	resize: function(){
		this.adaptHeight();
	},
	adaptWidth: function(){
		// Only calculate this here - rather than every call to buildRowContent
		if(!("contentWidth" in this) && this.contentNode && this.contentNode.offsetWidth > 0){
			this.contentWidth = this.contentNode.offsetWidth - this.padBorderWidth;
		}
	},
	// styling
	doStyleRowNode: function(inRowIndex, inRowNode){
		var n = [ "dojoxGridRowbar dojoxGridNonNormalizedCell" ];
		if(this.grid.rows.isOver(inRowIndex)){
			n.push("dojoxGridRowbarOver");
		}
		if(this.grid.selection.isSelected(inRowIndex)){
			n.push("dojoxGridRowbarSelected");
		}
		inRowNode.className = n.join(" ");
	},
	// event handlers
	domouseover: function(e){
		this.grid.onMouseOverRow(e);
	},
	domouseout: function(e){
		if(!this.isIntraRowEvent(e)){
			this.grid.onMouseOutRow(e);
		}
	}
});
});
},
'dojox/grid/_View':function(){
define([
	"dojo",
	"dijit/registry",
	"../main",
	"dojo/_base/declare",
	"dojo/_base/array",
	"dojo/_base/lang",
	"dojo/_base/connect",
	"dojo/_base/sniff",
	"dojo/query",
	"dojo/_base/window",
	"dojo/text!./resources/View.html",
	"dojo/dnd/Source",
	"dijit/_Widget",
	"dijit/_TemplatedMixin",
	"dojox/html/metrics",
	"./util",
	"dojo/_base/html",
	"./_Builder",
	"dojo/dnd/Avatar",
	"dojo/dnd/Manager"
], function(dojo, dijit, dojox, declare, array, lang, connect, has, query,
	win, template, Source, _Widget, _TemplatedMixin, metrics, util, html, _Builder, Avatar, Manager){

	// a private function
	var getStyleText = function(inNode, inStyleText){
		return inNode.style.cssText == undefined ? inNode.getAttribute("style") : inNode.style.cssText;
	};

	// some public functions
	var _View = declare('dojox.grid._View', [_Widget, _TemplatedMixin], {
		// summary:
		//		A collection of grid columns. A grid is comprised of a set of views that stack horizontally.
		//		Grid creates views automatically based on grid's layout structure.
		//		Users should typically not need to access individual views directly.
		//
		// defaultWidth: String
		//		Default width of the view
		defaultWidth: "18em",

		// viewWidth: String
		//		Width for the view, in valid css unit
		viewWidth: "",

		templateString: template,

		classTag: 'dojoxGrid',
		marginBottom: 0,
		rowPad: 2,

		// _togglingColumn: int
		//		Width of the column being toggled (-1 for none)
		_togglingColumn: -1,
		
		// _headerBuilderClass: Object
		//		The class to use for our header builder
		_headerBuilderClass: _Builder._HeaderBuilder,
		
		// _contentBuilderClass: Object
		//		The class to use for our content builder
		_contentBuilderClass: _Builder._ContentBuilder,
		
		postMixInProperties: function(){
			this.rowNodes = {};
		},

		postCreate: function(){
			this.connect(this.scrollboxNode,"onscroll","doscroll");
			util.funnelEvents(this.contentNode, this, "doContentEvent", [ 'mouseover', 'mouseout', 'click', 'dblclick', 'contextmenu', 'mousedown' ]);
			util.funnelEvents(this.headerNode, this, "doHeaderEvent", [ 'dblclick', 'mouseover', 'mouseout', 'mousemove', 'mousedown', 'click', 'contextmenu' ]);
			this.content = new this._contentBuilderClass(this);
			this.header = new this._headerBuilderClass(this);
			//BiDi: in RTL case, style width='9000em' causes scrolling problem in head node
			if(!this.grid.isLeftToRight()){
				this.headerNodeContainer.style.width = "";
			}
		},

		destroy: function(){
			html.destroy(this.headerNode);
			delete this.headerNode;
			for(var i in this.rowNodes){
				this._cleanupRowWidgets(this.rowNodes[i]);
				html.destroy(this.rowNodes[i]);
			}
			this.rowNodes = {};
			if(this.source){
				this.source.destroy();
			}
			this.inherited(arguments);
		},

		// focus
		focus: function(){
			if(has('ie') || has('webkit') || has('opera')){
				this.hiddenFocusNode.focus();
			}else{
				this.scrollboxNode.focus();
			}
		},

		setStructure: function(inStructure){
			var vs = (this.structure = inStructure);
			// FIXME: similar logic is duplicated in layout
			if(vs.width && !isNaN(vs.width)){
				this.viewWidth = vs.width + 'em';
			}else{
				this.viewWidth = vs.width || (vs.noscroll ? 'auto' : this.viewWidth); //|| this.defaultWidth;
			}
			this._onBeforeRow = vs.onBeforeRow||function(){};
			this._onAfterRow = vs.onAfterRow||function(){};
			this.noscroll = vs.noscroll;
			if(this.noscroll){
				this.scrollboxNode.style.overflow = "hidden";
			}
			this.simpleStructure = Boolean(vs.cells.length == 1);
			// bookkeeping
			this.testFlexCells();
			// accommodate new structure
			this.updateStructure();
		},
		
		_cleanupRowWidgets: function(inRowNode){
			// Summary:
			//		Cleans up the widgets for the given row node so that
			//		we can reattach them if needed
			if(inRowNode){
				array.forEach(query("[widgetId]", inRowNode).map(dijit.byNode), function(w){
					if(w._destroyOnRemove){
						w.destroy();
						delete w;
					}else if(w.domNode && w.domNode.parentNode){
						w.domNode.parentNode.removeChild(w.domNode);
					}
				});
			}
		},
		
		onBeforeRow: function(inRowIndex, cells){
			this._onBeforeRow(inRowIndex, cells);
			if(inRowIndex >= 0){
				this._cleanupRowWidgets(this.getRowNode(inRowIndex));
			}
		},
		
		onAfterRow: function(inRowIndex, cells, inRowNode){
			this._onAfterRow(inRowIndex, cells, inRowNode);
			var g = this.grid;
			array.forEach(query(".dojoxGridStubNode", inRowNode), function(n){
				if(n && n.parentNode){
					var lw = n.getAttribute("linkWidget");
					var cellIdx = window.parseInt(html.attr(n, "cellIdx"), 10);
					var cellDef = g.getCell(cellIdx);
					var w = dijit.byId(lw);
					if(w){
						n.parentNode.replaceChild(w.domNode, n);
						if(!w._started){
							w.startup();
						}
						dojo.destroy(n);
					}else{
						n.innerHTML = "";
					}
				}
			}, this);
		},

		testFlexCells: function(){
			// FIXME: cheater, this function does double duty as initializer and tester
			this.flexCells = false;
			for(var j=0, row; (row=this.structure.cells[j]); j++){
				for(var i=0, cell; (cell=row[i]); i++){
					cell.view = this;
					this.flexCells = this.flexCells || cell.isFlex();
				}
			}
			return this.flexCells;
		},

		updateStructure: function(){
			// header builder needs to update table map
			this.header.update();
			// content builder needs to update markup cache
			this.content.update();
		},

		getScrollbarWidth: function(){
			var hasScrollSpace = this.hasVScrollbar();
			var overflow = html.style(this.scrollboxNode, "overflow");
			if(this.noscroll || !overflow || overflow == "hidden"){
				hasScrollSpace = false;
			}else if(overflow == "scroll"){
				hasScrollSpace = true;
			}
			return (hasScrollSpace ? metrics.getScrollbar().w : 0); // Integer
		},

		getColumnsWidth: function(){
			var h = this.headerContentNode;
			return h && h.firstChild ? (h.firstChild.offsetWidth || html.style(h.firstChild, 'width')) : 0; // Integer
		},

		setColumnsWidth: function(width){
			this.headerContentNode.firstChild.style.width = width + 'px';
			if(this.viewWidth){
				this.viewWidth = width + 'px';
			}
		},

		getWidth: function(){
			return this.viewWidth || (this.getColumnsWidth()+this.getScrollbarWidth()) +'px'; // String
		},

		getContentWidth: function(){
			return Math.max(0, html._getContentBox(this.domNode).w - this.getScrollbarWidth()) + 'px'; // String
		},

		render: function(){
			this.scrollboxNode.style.height = '';
			this.renderHeader();
			if(this._togglingColumn >= 0){
				this.setColumnsWidth(this.getColumnsWidth() - this._togglingColumn);
				this._togglingColumn = -1;
			}
			var cells = this.grid.layout.cells;
			var getSibling = lang.hitch(this, function(node, before){
				!this.grid.isLeftToRight() && (before = !before);
				var inc = before?-1:1;
				var idx = this.header.getCellNodeIndex(node) + inc;
				var cell = cells[idx];
				while(cell && cell.getHeaderNode() && cell.getHeaderNode().style.display == "none"){
					idx += inc;
					cell = cells[idx];
				}
				if(cell){
					return cell.getHeaderNode();
				}
				return null;
			});
			if(this.grid.columnReordering && this.simpleStructure){
				if(this.source){
					this.source.destroy();
				}
				
				// Create the top and bottom markers
				var bottomMarkerId = "dojoxGrid_bottomMarker";
				var topMarkerId = "dojoxGrid_topMarker";
				if(this.bottomMarker){
					html.destroy(this.bottomMarker);
				}
				this.bottomMarker = html.byId(bottomMarkerId);
				if(this.topMarker){
					html.destroy(this.topMarker);
				}
				this.topMarker = html.byId(topMarkerId);
				if (!this.bottomMarker) {
					this.bottomMarker = html.create("div", {
						"id": bottomMarkerId,
						"class": "dojoxGridColPlaceBottom"
					}, win.body());
					this._hide(this.bottomMarker);

					
					this.topMarker = html.create("div", {
						"id": topMarkerId,
						"class": "dojoxGridColPlaceTop"
					}, win.body());
					this._hide(this.topMarker);
				}
				this.arrowDim = html.contentBox(this.bottomMarker);

				var headerHeight = html.contentBox(this.headerContentNode.firstChild.rows[0]).h;
				
				this.source = new Source(this.headerContentNode.firstChild.rows[0], {
					horizontal: true,
					accept: [ "gridColumn_" + this.grid.id ],
					viewIndex: this.index,
					generateText: false,
					onMouseDown: lang.hitch(this, function(e){
						this.header.decorateEvent(e);
						if((this.header.overRightResizeArea(e) || this.header.overLeftResizeArea(e)) &&
							this.header.canResize(e) && !this.header.moveable){
							this.header.beginColumnResize(e);
						}else{
							if(this.grid.headerMenu){
								this.grid.headerMenu.onCancel(true);
							}
							// IE reports a left click as 1, where everything else reports 0
							if(e.button === (has('ie') < 9 ? 1 : 0)){
								Source.prototype.onMouseDown.call(this.source, e);
							}
						}
					}),
					onMouseOver: lang.hitch(this, function(e){
						var src = this.source;
						if(src._getChildByEvent(e)){
							Source.prototype.onMouseOver.apply(src, arguments);
						}
					}),
					_markTargetAnchor: lang.hitch(this, function(before){
						var src = this.source;
						if(src.current == src.targetAnchor && src.before == before){ return; }
						if(src.targetAnchor && getSibling(src.targetAnchor, src.before)){
							src._removeItemClass(getSibling(src.targetAnchor, src.before), src.before ? "After" : "Before");
						}
						Source.prototype._markTargetAnchor.call(src, before);
						
						var target = before ? src.targetAnchor : getSibling(src.targetAnchor, src.before);
						var endAdd = 0;

						if (!target) {
							target = src.targetAnchor;
							endAdd = html.contentBox(target).w + this.arrowDim.w/2 + 2;
						}

						var pos = html.position(target, true);
						var left = Math.floor(pos.x - this.arrowDim.w/2 + endAdd);

						html.style(this.bottomMarker, "visibility", "visible");
						html.style(this.topMarker, "visibility", "visible");
						html.style(this.bottomMarker, {
							"left": left + "px",
							"top" : (headerHeight + pos.y) + "px"
						});

						html.style(this.topMarker, {
							"left": left + "px",
							"top" : (pos.y - this.arrowDim.h) + "px"
						});

						if(src.targetAnchor && getSibling(src.targetAnchor, src.before)){
							src._addItemClass(getSibling(src.targetAnchor, src.before), src.before ? "After" : "Before");
						}
					}),
					_unmarkTargetAnchor: lang.hitch(this, function(){
						var src = this.source;
						if(!src.targetAnchor){ return; }
						if(src.targetAnchor && getSibling(src.targetAnchor, src.before)){
							src._removeItemClass(getSibling(src.targetAnchor, src.before), src.before ? "After" : "Before");
						}
						this._hide(this.bottomMarker);
						this._hide(this.topMarker);
						Source.prototype._unmarkTargetAnchor.call(src);
					}),
					destroy: lang.hitch(this, function(){
						connect.disconnect(this._source_conn);
						connect.unsubscribe(this._source_sub);
						Source.prototype.destroy.call(this.source);
						if(this.bottomMarker){
							html.destroy(this.bottomMarker);
							delete this.bottomMarker;
						}
						if(this.topMarker){
							html.destroy(this.topMarker);
							delete this.topMarker;
						}
					}),
					onDndCancel: lang.hitch(this, function(){
						Source.prototype.onDndCancel.call(this.source);
						this._hide(this.bottomMarker);
						this._hide(this.topMarker);
					})
				});

				this._source_conn = connect.connect(this.source, "onDndDrop", this, "_onDndDrop");
				this._source_sub = connect.subscribe("/dnd/drop/before", this, "_onDndDropBefore");
				this.source.startup();
			}
		},
		
		_hide: function(node){
			html.style(node, {
				top: "-10000px",
				"visibility": "hidden"
			});
		},

		_onDndDropBefore: function(source, nodes, copy){
			if(Manager.manager().target !== this.source){
				return;
			}
			this.source._targetNode = this.source.targetAnchor;
			this.source._beforeTarget = this.source.before;
			var views = this.grid.views.views;
			var srcView = views[source.viewIndex];
			var tgtView = views[this.index];
			if(tgtView != srcView){
				srcView.convertColPctToFixed();
				tgtView.convertColPctToFixed();
			}
		},

		_onDndDrop: function(source, nodes, copy){
			if(Manager.manager().target !== this.source){
				if(Manager.manager().source === this.source){
					this._removingColumn = true;
				}
				return;
			}
			this._hide(this.bottomMarker);
			this._hide(this.topMarker);

			var getIdx = function(n){
				return n ? html.attr(n, "idx") : null;
			};
			var w = html.marginBox(nodes[0]).w;
			if(source.viewIndex !== this.index){
				var views = this.grid.views.views;
				var srcView = views[source.viewIndex];
				var tgtView = views[this.index];
				if(srcView.viewWidth && srcView.viewWidth != "auto"){
					srcView.setColumnsWidth(srcView.getColumnsWidth() - w);
				}
				if(tgtView.viewWidth && tgtView.viewWidth != "auto"){
					tgtView.setColumnsWidth(tgtView.getColumnsWidth());
				}
			}
			var stn = this.source._targetNode;
			var stb = this.source._beforeTarget;
			!this.grid.isLeftToRight() && (stb = !stb);
			var layout = this.grid.layout;
			var idx = this.index;
			delete this.source._targetNode;
			delete this.source._beforeTarget;
			
			layout.moveColumn(
				source.viewIndex,
				idx,
				getIdx(nodes[0]),
				getIdx(stn),
				stb);
		},

		renderHeader: function(){
			this.headerContentNode.innerHTML = this.header.generateHtml(this._getHeaderContent);
			if(this.flexCells){
				this.contentWidth = this.getContentWidth();
				this.headerContentNode.firstChild.style.width = this.contentWidth;
			}
			util.fire(this, "onAfterRow", [-1, this.structure.cells, this.headerContentNode]);
		},

		// note: not called in 'view' context
		_getHeaderContent: function(inCell){
			var n = inCell.name || inCell.grid.getCellName(inCell);
			if(/^\s+$/.test(n)){
				n = '&nbsp;'//otherwise arrow styles will be messed up
			}
			var ret = [ '<div class="dojoxGridSortNode' ];
			
			if(inCell.index != inCell.grid.getSortIndex()){
				ret.push('">');
			}else{
				ret = ret.concat([ ' ',
							inCell.grid.sortInfo > 0 ? 'dojoxGridSortUp' : 'dojoxGridSortDown',
							'"><div class="dojoxGridArrowButtonChar">',
							inCell.grid.sortInfo > 0 ? '&#9650;' : '&#9660;',
							'</div><div class="dojoxGridArrowButtonNode" role="presentation"></div>',
							'<div class="dojoxGridColCaption">']);
			}
			ret = ret.concat([n, '</div></div>']);
			return ret.join('');
		},

		resize: function(){
			this.adaptHeight();
			this.adaptWidth();
		},

		hasHScrollbar: function(reset){
			var hadScroll = this._hasHScroll||false;
			if(this._hasHScroll == undefined || reset){
				if(this.noscroll){
					this._hasHScroll = false;
				}else{
					var style = html.style(this.scrollboxNode, "overflow");
					if(style == "hidden"){
						this._hasHScroll = false;
					}else if(style == "scroll"){
						this._hasHScroll = true;
					}else{
						this._hasHScroll = (this.scrollboxNode.offsetWidth - this.getScrollbarWidth() < this.contentNode.offsetWidth );
					}
				}
			}
			if(hadScroll !== this._hasHScroll){
				this.grid.update();
			}
			return this._hasHScroll; // Boolean
		},

		hasVScrollbar: function(reset){
			var hadScroll = this._hasVScroll||false;
			if(this._hasVScroll == undefined || reset){
				if(this.noscroll){
					this._hasVScroll = false;
				}else{
					var style = html.style(this.scrollboxNode, "overflow");
					if(style == "hidden"){
						this._hasVScroll = false;
					}else if(style == "scroll"){
						this._hasVScroll = true;
					}else{
						this._hasVScroll = (this.scrollboxNode.scrollHeight > this.scrollboxNode.clientHeight);
					}
				}
			}
			if(hadScroll !== this._hasVScroll){
				this.grid.update();
			}
			return this._hasVScroll; // Boolean
		},
		
		convertColPctToFixed: function(){
			// Fix any percentage widths to be pixel values
			var hasPct = false;
			this.grid.initialWidth = "";
			var cellNodes = query("th", this.headerContentNode);
			var fixedWidths = array.map(cellNodes, function(c, vIdx){
				var w = c.style.width;
				html.attr(c, "vIdx", vIdx);
				if(w && w.slice(-1) == "%"){
					hasPct = true;
				}else if(w && w.slice(-2) == "px"){
					return window.parseInt(w, 10);
				}
				return html.contentBox(c).w;
			});
			if(hasPct){
				array.forEach(this.grid.layout.cells, function(cell, idx){
					if(cell.view == this){
						var cellNode = cell.view.getHeaderCellNode(cell.index);
						if(cellNode && html.hasAttr(cellNode, "vIdx")){
							var vIdx = window.parseInt(html.attr(cellNode, "vIdx"));
							this.setColWidth(idx, fixedWidths[vIdx]);
							html.removeAttr(cellNode, "vIdx");
						}
					}
				}, this);
				return true;
			}
			return false;
		},

		adaptHeight: function(minusScroll){
			if(!this.grid._autoHeight){
				var h = (this.domNode.style.height && parseInt(this.domNode.style.height.replace(/px/,''), 10)) || this.domNode.clientHeight;
				var self = this;
				var checkOtherViewScrollers = function(){
					var v;
					for(var i = 0; i < self.grid.views.views.length; ++i){
						v = self.grid.views.views[i];
						if(v !== self && v.hasHScrollbar()){
							return true;
						}
					}
					return false;
				};
				if(minusScroll || (this.noscroll && checkOtherViewScrollers())){
					h -= metrics.getScrollbar().h;
				}
				util.setStyleHeightPx(this.scrollboxNode, h);
			}
			this.hasVScrollbar(true);
		},

		adaptWidth: function(){
			if(this.flexCells){
				// the view content width
				this.contentWidth = this.getContentWidth();
				this.headerContentNode.firstChild.style.width = this.contentWidth;
			}
			// FIXME: it should be easier to get w from this.scrollboxNode.clientWidth,
			// but clientWidth seemingly does not include scrollbar width in some cases
			var w = this.scrollboxNode.offsetWidth - this.getScrollbarWidth();
			if(!this._removingColumn){
				w = Math.max(w, this.getColumnsWidth()) + 'px';
			}else{
				w = Math.min(w, this.getColumnsWidth()) + 'px';
				this._removingColumn = false;
			}
			var cn = this.contentNode;
			cn.style.width = w;
			this.hasHScrollbar(true);
		},

		setSize: function(w, h){
			var ds = this.domNode.style;
			var hs = this.headerNode.style;

			if(w){
				ds.width = w;
				hs.width = w;
			}
			ds.height = (h >= 0 ? h + 'px' : '');
		},

		renderRow: function(inRowIndex){
			var rowNode = this.createRowNode(inRowIndex);
			this.buildRow(inRowIndex, rowNode);
			//this.grid.edit.restore(this, inRowIndex);
			return rowNode;
		},

		createRowNode: function(inRowIndex){
			var node = document.createElement("div");
			node.className = this.classTag + 'Row';
			if (this instanceof dojox.grid._RowSelector){
				html.attr(node,"role","presentation");
			}else{
				html.attr(node,"role","row");
				if (this.grid.selectionMode != "none") {
					node.setAttribute("aria-selected", "false"); //rows can be selected so add aria-selected prop
				}
			}
			node[util.gridViewTag] = this.id;
			node[util.rowIndexTag] = inRowIndex;
			this.rowNodes[inRowIndex] = node;
			return node;
		},

		buildRow: function(inRowIndex, inRowNode){
			
			this.buildRowContent(inRowIndex, inRowNode);
		  	
			this.styleRow(inRowIndex, inRowNode);
		  
		 
		},

		buildRowContent: function(inRowIndex, inRowNode){
			inRowNode.innerHTML = this.content.generateHtml(inRowIndex, inRowIndex);
			if(this.flexCells && this.contentWidth){
				// FIXME: accessing firstChild here breaks encapsulation
				inRowNode.firstChild.style.width = this.contentWidth;
			}
			util.fire(this, "onAfterRow", [inRowIndex, this.structure.cells, inRowNode]);
		},

		rowRemoved:function(inRowIndex){
			if(inRowIndex >= 0){
				this._cleanupRowWidgets(this.getRowNode(inRowIndex));
			}
			this.grid.edit.save(this, inRowIndex);
			delete this.rowNodes[inRowIndex];
		},

		getRowNode: function(inRowIndex){
			return this.rowNodes[inRowIndex];
		},

		getCellNode: function(inRowIndex, inCellIndex){
			var row = this.getRowNode(inRowIndex);
			if(row){
				return this.content.getCellNode(row, inCellIndex);
			}
		},

		getHeaderCellNode: function(inCellIndex){
			if(this.headerContentNode){
				return this.header.getCellNode(this.headerContentNode, inCellIndex);
			}
		},

		// styling
		styleRow: function(inRowIndex, inRowNode){
			inRowNode._style = getStyleText(inRowNode);
			this.styleRowNode(inRowIndex, inRowNode);
		},

		styleRowNode: function(inRowIndex, inRowNode){
			if(inRowNode){
				this.doStyleRowNode(inRowIndex, inRowNode);
			}
		},

		doStyleRowNode: function(inRowIndex, inRowNode){
			this.grid.styleRowNode(inRowIndex, inRowNode);
		},

		// updating
		updateRow: function(inRowIndex){
			var rowNode = this.getRowNode(inRowIndex);
			if(rowNode){
				rowNode.style.height = '';
				this.buildRow(inRowIndex, rowNode);
			}
			return rowNode;
		},

		updateRowStyles: function(inRowIndex){
			this.styleRowNode(inRowIndex, this.getRowNode(inRowIndex));
		},

		// scrolling
		lastTop: 0,
		firstScroll:0,
		_nativeScroll: false,

		doscroll: function(inEvent){
			if(has('ff') >= 13 || has('chrome')){
				this._nativeScroll = true;
			}
			//var s = dojo.marginBox(this.headerContentNode.firstChild);
			var isLtr = this.grid.isLeftToRight();
			if(this.firstScroll < 2){
				if((!isLtr && this.firstScroll == 1) || (isLtr && this.firstScroll === 0)){
					var s = html.marginBox(this.headerNodeContainer);
					if(has('ie')){
						this.headerNodeContainer.style.width = s.w + this.getScrollbarWidth() + 'px';
					}else if(has('mozilla')){
						//TODO currently only for FF, not sure for safari and opera
						this.headerNodeContainer.style.width = s.w - this.getScrollbarWidth() + 'px';
						//this.headerNodeContainer.style.width = s.w + 'px';
						//set scroll to right in FF
						this.scrollboxNode.scrollLeft = isLtr ?
							this.scrollboxNode.clientWidth - this.scrollboxNode.scrollWidth :
							this.scrollboxNode.scrollWidth - this.scrollboxNode.clientWidth;
					}
				}
				this.firstScroll++;
			}
			this.headerNode.scrollLeft = this.scrollboxNode.scrollLeft;
			// 'lastTop' is a semaphore to prevent feedback-loop with setScrollTop below
			var top = this.scrollboxNode.scrollTop;
			if(top !== this.lastTop){
				this.grid.scrollTo(top);
			}
			this._nativeScroll = false;
		},

		setScrollTop: function(inTop){
			// 'lastTop' is a semaphore to prevent feedback-loop with doScroll above
			this.lastTop = inTop;
			if(!this._nativeScroll){
				//fix #15487
				this.scrollboxNode.scrollTop = inTop;
			}
			return this.scrollboxNode.scrollTop;
		},

		// event handlers (direct from DOM)
		doContentEvent: function(e){
			if(this.content.decorateEvent(e)){
				this.grid.onContentEvent(e);
			}
		},

		doHeaderEvent: function(e){
			if(this.header.decorateEvent(e)){
				this.grid.onHeaderEvent(e);
			}
		},

		// event dispatch(from Grid)
		dispatchContentEvent: function(e){
			return this.content.dispatchEvent(e);
		},

		dispatchHeaderEvent: function(e){
			return this.header.dispatchEvent(e);
		},

		// column resizing
		setColWidth: function(inIndex, inWidth){
			this.grid.setCellWidth(inIndex, inWidth + 'px');
		},

		update: function(){
			if(!this.domNode){
				return;
			}
			this.content.update();
			this.grid.update();
			//get scroll after update or scroll left setting goes wrong on IE.
			//See trac: #8040
			var left = this.scrollboxNode.scrollLeft;
			this.scrollboxNode.scrollLeft = left;
			this.headerNode.scrollLeft = left;
		}
	});

	var _GridAvatar = declare("dojox.grid._GridAvatar", Avatar, {
		construct: function(){
			var dd = win.doc;

			var a = dd.createElement("table");
			a.cellPadding = a.cellSpacing = "0";
			a.className = "dojoxGridDndAvatar";
			a.style.position = "absolute";
			a.style.zIndex = 1999;
			a.style.margin = "0px"; // to avoid dojo.marginBox() problems with table's margins
			var b = dd.createElement("tbody");
			var tr = dd.createElement("tr");
			var td = dd.createElement("td");
			var img = dd.createElement("td");
			tr.className = "dojoxGridDndAvatarItem";
			img.className = "dojoxGridDndAvatarItemImage";
			img.style.width = "16px";
			var source = this.manager.source, node;
			if(source.creator){
				// create an avatar representation of the node
				node = source._normalizedCreator(source.getItem(this.manager.nodes[0].id).data, "avatar").node;
			}else{
				// or just clone the node and hope it works
				node = this.manager.nodes[0].cloneNode(true);
				var table, tbody;
				if(node.tagName.toLowerCase() == "tr"){
					// insert extra table nodes
					table = dd.createElement("table");
					tbody = dd.createElement("tbody");
					tbody.appendChild(node);
					table.appendChild(tbody);
					node = table;
				}else if(node.tagName.toLowerCase() == "th"){
					// insert extra table nodes
					table = dd.createElement("table");
					tbody = dd.createElement("tbody");
					var r = dd.createElement("tr");
					table.cellPadding = table.cellSpacing = "0";
					r.appendChild(node);
					tbody.appendChild(r);
					table.appendChild(tbody);
					node = table;
				}
			}
			node.id = "";
			td.appendChild(node);
			tr.appendChild(img);
			tr.appendChild(td);
			html.style(tr, "opacity", 0.9);
			b.appendChild(tr);

			a.appendChild(b);
			this.node = a;

			var m = Manager.manager();
			this.oldOffsetY = m.OFFSET_Y;
			m.OFFSET_Y = 1;
		},
		destroy: function(){
			Manager.manager().OFFSET_Y = this.oldOffsetY;
			this.inherited(arguments);
		}
	});

	var oldMakeAvatar = Manager.manager().makeAvatar;
	Manager.manager().makeAvatar = function(){
		var src = this.source;
		if(src.viewIndex !== undefined && !html.hasClass(win.body(),"dijit_a11y")){
			return new _GridAvatar(this);
		}
		return oldMakeAvatar.call(Manager.manager());
	};

	return _View;

});
},
'dojo/dnd/Source':function(){
define([
	"../_base/array", "../_base/declare", "../_base/kernel", "../_base/lang",
	"../dom-class", "../dom-geometry", "../mouse", "../ready", "../topic",
	"./common", "./Selector", "./Manager"
], function(array, declare, kernel, lang, domClass, domGeom, mouse, ready, topic,
			dnd, Selector, Manager){

// module:
//		dojo/dnd/Source

/*
	Container property:
		"Horizontal"- if this is the horizontal container
	Source states:
		""			- normal state
		"Moved"		- this source is being moved
		"Copied"	- this source is being copied
	Target states:
		""			- normal state
		"Disabled"	- the target cannot accept an avatar
	Target anchor state:
		""			- item is not selected
		"Before"	- insert point is before the anchor
		"After"		- insert point is after the anchor
*/

/*=====
var __SourceArgs = {
	// summary:
	//		a dict of parameters for DnD Source configuration. Note that any
	//		property on Source elements may be configured, but this is the
	//		short-list
	// isSource: Boolean?
	//		can be used as a DnD source. Defaults to true.
	// accept: Array?
	//		list of accepted types (text strings) for a target; defaults to
	//		["text"]
	// autoSync: Boolean
	//		if true refreshes the node list on every operation; false by default
	// copyOnly: Boolean?
	//		copy items, if true, use a state of Ctrl key otherwise,
	//		see selfCopy and selfAccept for more details
	// delay: Number
	//		the move delay in pixels before detecting a drag; 0 by default
	// horizontal: Boolean?
	//		a horizontal container, if true, vertical otherwise or when omitted
	// selfCopy: Boolean?
	//		copy items by default when dropping on itself,
	//		false by default, works only if copyOnly is true
	// selfAccept: Boolean?
	//		accept its own items when copyOnly is true,
	//		true by default, works only if copyOnly is true
	// withHandles: Boolean?
	//		allows dragging only by handles, false by default
	// generateText: Boolean?
	//		generate text node for drag and drop, true by default
};
=====*/

// For back-compat, remove in 2.0.
if(!kernel.isAsync){
	ready(0, function(){
		var requires = ["dojo/dnd/AutoSource", "dojo/dnd/Target"];
		require(requires);	// use indirection so modules not rolled into a build
	});
}

var Source = declare("dojo.dnd.Source", Selector, {
	// summary:
	//		a Source object, which can be used as a DnD source, or a DnD target

	// object attributes (for markup)
	isSource: true,
	horizontal: false,
	copyOnly: false,
	selfCopy: false,
	selfAccept: true,
	skipForm: false,
	withHandles: false,
	autoSync: false,
	delay: 0, // pixels
	accept: ["text"],
	generateText: true,

	constructor: function(/*DOMNode|String*/ node, /*__SourceArgs?*/ params){
		// summary:
		//		a constructor of the Source
		// node:
		//		node or node's id to build the source on
		// params:
		//		any property of this class may be configured via the params
		//		object which is mixed-in to the `dojo/dnd/Source` instance
		lang.mixin(this, lang.mixin({}, params));
		var type = this.accept;
		if(type.length){
			this.accept = {};
			for(var i = 0; i < type.length; ++i){
				this.accept[type[i]] = 1;
			}
		}
		// class-specific variables
		this.isDragging = false;
		this.mouseDown = false;
		this.targetAnchor = null;
		this.targetBox = null;
		this.before = true;
		this._lastX = 0;
		this._lastY = 0;
		// states
		this.sourceState  = "";
		if(this.isSource){
			domClass.add(this.node, "dojoDndSource");
		}
		this.targetState  = "";
		if(this.accept){
			domClass.add(this.node, "dojoDndTarget");
		}
		if(this.horizontal){
			domClass.add(this.node, "dojoDndHorizontal");
		}
		// set up events
		this.topics = [
			topic.subscribe("/dnd/source/over", lang.hitch(this, "onDndSourceOver")),
			topic.subscribe("/dnd/start",  lang.hitch(this, "onDndStart")),
			topic.subscribe("/dnd/drop",   lang.hitch(this, "onDndDrop")),
			topic.subscribe("/dnd/cancel", lang.hitch(this, "onDndCancel"))
		];
	},

	// methods
	checkAcceptance: function(source, nodes){
		// summary:
		//		checks if the target can accept nodes from this source
		// source: Object
		//		the source which provides items
		// nodes: Array
		//		the list of transferred items
		if(this == source){
			return !this.copyOnly || this.selfAccept;
		}
		for(var i = 0; i < nodes.length; ++i){
			var type = source.getItem(nodes[i].id).type;
			// type instanceof Array
			var flag = false;
			for(var j = 0; j < type.length; ++j){
				if(type[j] in this.accept){
					flag = true;
					break;
				}
			}
			if(!flag){
				return false;	// Boolean
			}
		}
		return true;	// Boolean
	},
	copyState: function(keyPressed, self){
		// summary:
		//		Returns true if we need to copy items, false to move.
		//		It is separated to be overwritten dynamically, if needed.
		// keyPressed: Boolean
		//		the "copy" key was pressed
		// self: Boolean?
		//		optional flag that means that we are about to drop on itself

		if(keyPressed){ return true; }
		if(arguments.length < 2){
			self = this == Manager.manager().target;
		}
		if(self){
			if(this.copyOnly){
				return this.selfCopy;
			}
		}else{
			return this.copyOnly;
		}
		return false;	// Boolean
	},
	destroy: function(){
		// summary:
		//		prepares the object to be garbage-collected
		Source.superclass.destroy.call(this);
		array.forEach(this.topics, function(t){t.remove();});
		this.targetAnchor = null;
	},

	// mouse event processors
	onMouseMove: function(e){
		// summary:
		//		event processor for onmousemove
		// e: Event
		//		mouse event
		if(this.isDragging && this.targetState == "Disabled"){ return; }
		Source.superclass.onMouseMove.call(this, e);
		var m = Manager.manager();
		if(!this.isDragging){
			if(this.mouseDown && this.isSource &&
					(Math.abs(e.pageX - this._lastX) > this.delay || Math.abs(e.pageY - this._lastY) > this.delay)){
				var nodes = this.getSelectedNodes();
				if(nodes.length){
					m.startDrag(this, nodes, this.copyState(dnd.getCopyKeyState(e), true));
				}
			}
		}
		if(this.isDragging){
			// calculate before/after
			var before = false;
			if(this.current){
				if(!this.targetBox || this.targetAnchor != this.current){
					this.targetBox = domGeom.position(this.current, true);
				}
				if(this.horizontal){
					// In LTR mode, the left part of the object means "before", but in RTL mode it means "after".
					before = (e.pageX - this.targetBox.x < this.targetBox.w / 2) == domGeom.isBodyLtr(this.current.ownerDocument);
				}else{
					before = (e.pageY - this.targetBox.y) < (this.targetBox.h / 2);
				}
			}
			if(this.current != this.targetAnchor || before != this.before){
				this._markTargetAnchor(before);
				m.canDrop(!this.current || m.source != this || !(this.current.id in this.selection));
			}
		}
	},
	onMouseDown: function(e){
		// summary:
		//		event processor for onmousedown
		// e: Event
		//		mouse event
		if(!this.mouseDown && this._legalMouseDown(e) && (!this.skipForm || !dnd.isFormElement(e))){
			this.mouseDown = true;
			this._lastX = e.pageX;
			this._lastY = e.pageY;
			Source.superclass.onMouseDown.call(this, e);
		}
	},
	onMouseUp: function(e){
		// summary:
		//		event processor for onmouseup
		// e: Event
		//		mouse event
		if(this.mouseDown){
			this.mouseDown = false;
			Source.superclass.onMouseUp.call(this, e);
		}
	},

	// topic event processors
	onDndSourceOver: function(source){
		// summary:
		//		topic event processor for /dnd/source/over, called when detected a current source
		// source: Object
		//		the source which has the mouse over it
		if(this !== source){
			this.mouseDown = false;
			if(this.targetAnchor){
				this._unmarkTargetAnchor();
			}
		}else if(this.isDragging){
			var m = Manager.manager();
			m.canDrop(this.targetState != "Disabled" && (!this.current || m.source != this || !(this.current.id in this.selection)));
		}
	},
	onDndStart: function(source, nodes, copy){
		// summary:
		//		topic event processor for /dnd/start, called to initiate the DnD operation
		// source: Object
		//		the source which provides items
		// nodes: Array
		//		the list of transferred items
		// copy: Boolean
		//		copy items, if true, move items otherwise
		if(this.autoSync){ this.sync(); }
		if(this.isSource){
			this._changeState("Source", this == source ? (copy ? "Copied" : "Moved") : "");
		}
		var accepted = this.accept && this.checkAcceptance(source, nodes);
		this._changeState("Target", accepted ? "" : "Disabled");
		if(this == source){
			Manager.manager().overSource(this);
		}
		this.isDragging = true;
	},
	onDndDrop: function(source, nodes, copy, target){
		// summary:
		//		topic event processor for /dnd/drop, called to finish the DnD operation
		// source: Object
		//		the source which provides items
		// nodes: Array
		//		the list of transferred items
		// copy: Boolean
		//		copy items, if true, move items otherwise
		// target: Object
		//		the target which accepts items
		if(this == target){
			// this one is for us => move nodes!
			this.onDrop(source, nodes, copy);
		}
		this.onDndCancel();
	},
	onDndCancel: function(){
		// summary:
		//		topic event processor for /dnd/cancel, called to cancel the DnD operation
		if(this.targetAnchor){
			this._unmarkTargetAnchor();
			this.targetAnchor = null;
		}
		this.before = true;
		this.isDragging = false;
		this.mouseDown = false;
		this._changeState("Source", "");
		this._changeState("Target", "");
	},

	// local events
	onDrop: function(source, nodes, copy){
		// summary:
		//		called only on the current target, when drop is performed
		// source: Object
		//		the source which provides items
		// nodes: Array
		//		the list of transferred items
		// copy: Boolean
		//		copy items, if true, move items otherwise

		if(this != source){
			this.onDropExternal(source, nodes, copy);
		}else{
			this.onDropInternal(nodes, copy);
		}
	},
	onDropExternal: function(source, nodes, copy){
		// summary:
		//		called only on the current target, when drop is performed
		//		from an external source
		// source: Object
		//		the source which provides items
		// nodes: Array
		//		the list of transferred items
		// copy: Boolean
		//		copy items, if true, move items otherwise

		var oldCreator = this._normalizedCreator;
		// transferring nodes from the source to the target
		if(this.creator){
			// use defined creator
			this._normalizedCreator = function(node, hint){
				return oldCreator.call(this, source.getItem(node.id).data, hint);
			};
		}else{
			// we have no creator defined => move/clone nodes
			if(copy){
				// clone nodes
				this._normalizedCreator = function(node /*=====, hint =====*/){
					var t = source.getItem(node.id);
					var n = node.cloneNode(true);
					n.id = dnd.getUniqueId();
					return {node: n, data: t.data, type: t.type};
				};
			}else{
				// move nodes
				this._normalizedCreator = function(node /*=====, hint =====*/){
					var t = source.getItem(node.id);
					source.delItem(node.id);
					return {node: node, data: t.data, type: t.type};
				};
			}
		}
		this.selectNone();
		if(!copy && !this.creator){
			source.selectNone();
		}
		this.insertNodes(true, nodes, this.before, this.current);
		if(!copy && this.creator){
			source.deleteSelectedNodes();
		}
		this._normalizedCreator = oldCreator;
	},
	onDropInternal: function(nodes, copy){
		// summary:
		//		called only on the current target, when drop is performed
		//		from the same target/source
		// nodes: Array
		//		the list of transferred items
		// copy: Boolean
		//		copy items, if true, move items otherwise

		var oldCreator = this._normalizedCreator;
		// transferring nodes within the single source
		if(this.current && this.current.id in this.selection){
			// do nothing
			return;
		}
		if(copy){
			if(this.creator){
				// create new copies of data items
				this._normalizedCreator = function(node, hint){
					return oldCreator.call(this, this.getItem(node.id).data, hint);
				};
			}else{
				// clone nodes
				this._normalizedCreator = function(node/*=====, hint =====*/){
					var t = this.getItem(node.id);
					var n = node.cloneNode(true);
					n.id = dnd.getUniqueId();
					return {node: n, data: t.data, type: t.type};
				};
			}
		}else{
			// move nodes
			if(!this.current){
				// do nothing
				return;
			}
			this._normalizedCreator = function(node /*=====, hint =====*/){
				var t = this.getItem(node.id);
				return {node: node, data: t.data, type: t.type};
			};
		}
		this._removeSelection();
		this.insertNodes(true, nodes, this.before, this.current);
		this._normalizedCreator = oldCreator;
	},
	onDraggingOver: function(){
		// summary:
		//		called during the active DnD operation, when items
		//		are dragged over this target, and it is not disabled
	},
	onDraggingOut: function(){
		// summary:
		//		called during the active DnD operation, when items
		//		are dragged away from this target, and it is not disabled
	},

	// utilities
	onOverEvent: function(){
		// summary:
		//		this function is called once, when mouse is over our container
		Source.superclass.onOverEvent.call(this);
		Manager.manager().overSource(this);
		if(this.isDragging && this.targetState != "Disabled"){
			this.onDraggingOver();
		}
	},
	onOutEvent: function(){
		// summary:
		//		this function is called once, when mouse is out of our container
		Source.superclass.onOutEvent.call(this);
		Manager.manager().outSource(this);
		if(this.isDragging && this.targetState != "Disabled"){
			this.onDraggingOut();
		}
	},
	_markTargetAnchor: function(before){
		// summary:
		//		assigns a class to the current target anchor based on "before" status
		// before: Boolean
		//		insert before, if true, after otherwise
		if(this.current == this.targetAnchor && this.before == before){ return; }
		if(this.targetAnchor){
			this._removeItemClass(this.targetAnchor, this.before ? "Before" : "After");
		}
		this.targetAnchor = this.current;
		this.targetBox = null;
		this.before = before;
		if(this.targetAnchor){
			this._addItemClass(this.targetAnchor, this.before ? "Before" : "After");
		}
	},
	_unmarkTargetAnchor: function(){
		// summary:
		//		removes a class of the current target anchor based on "before" status
		if(!this.targetAnchor){ return; }
		this._removeItemClass(this.targetAnchor, this.before ? "Before" : "After");
		this.targetAnchor = null;
		this.targetBox = null;
		this.before = true;
	},
	_markDndStatus: function(copy){
		// summary:
		//		changes source's state based on "copy" status
		this._changeState("Source", copy ? "Copied" : "Moved");
	},
	_legalMouseDown: function(e){
		// summary:
		//		checks if user clicked on "approved" items
		// e: Event
		//		mouse event

		// accept only the left mouse button, or the left finger
		if(e.type != "touchstart" && !mouse.isLeft(e)){ return false; }

		if(!this.withHandles){ return true; }

		// check for handles
		for(var node = e.target; node && node !== this.node; node = node.parentNode){
			if(domClass.contains(node, "dojoDndHandle")){ return true; }
			if(domClass.contains(node, "dojoDndItem") || domClass.contains(node, "dojoDndIgnore")){ break; }
		}
		return false;	// Boolean
	}
});

return Source;

});

},
'dojo/dnd/common':function(){
define(["../sniff", "../_base/kernel", "../_base/lang", "../dom"],
	function(has, kernel, lang, dom){

// module:
//		dojo/dnd/common

var exports = lang.getObject("dojo.dnd", true);
/*=====
// TODO: for 2.0, replace line above with this code.
var exports = {
	// summary:
	//		TODOC
};
=====*/

exports.getCopyKeyState = function(evt){
	return evt[has("mac") ? "metaKey" : "ctrlKey"]
};

exports._uniqueId = 0;
exports.getUniqueId = function(){
	// summary:
	//		returns a unique string for use with any DOM element
	var id;
	do{
		id = kernel._scopeName + "Unique" + (++exports._uniqueId);
	}while(dom.byId(id));
	return id;
};

exports._empty = {};

exports.isFormElement = function(/*Event*/ e){
	// summary:
	//		returns true if user clicked on a form element
	var t = e.target;
	if(t.nodeType == 3 /*TEXT_NODE*/){
		t = t.parentNode;
	}
	return " a button textarea input select option ".indexOf(" " + t.tagName.toLowerCase() + " ") >= 0;	// Boolean
};

return exports;
});

},
'dojo/dnd/Selector':function(){
define([
	"../_base/array", "../_base/declare", "../_base/kernel", "../_base/lang",
	"../dom", "../dom-construct", "../mouse", "../_base/NodeList", "../on", "../touch", "./common", "./Container"
], function(array, declare, kernel, lang, dom, domConstruct, mouse, NodeList, on, touch, dnd, Container){

// module:
//		dojo/dnd/Selector

/*
	Container item states:
		""			- an item is not selected
		"Selected"	- an item is selected
		"Anchor"	- an item is selected, and is an anchor for a "shift" selection
*/

/*=====
var __SelectorArgs = declare([Container.__ContainerArgs], {
	// singular: Boolean
	//		allows selection of only one element, if true
	singular: false,

	// autoSync: Boolean
	//		autosynchronizes the source with its list of DnD nodes,
	autoSync: false
});
=====*/

var Selector = declare("dojo.dnd.Selector", Container, {
	// summary:
	//		a Selector object, which knows how to select its children

	/*=====
	// selection: Set<String>
	//		The set of id's that are currently selected, such that this.selection[id] == 1
	//		if the node w/that id is selected.  Can iterate over selected node's id's like:
	//	|		for(var id in this.selection)
	selection: {},
	=====*/

	constructor: function(node, params){
		// summary:
		//		constructor of the Selector
		// node: Node||String
		//		node or node's id to build the selector on
		// params: __SelectorArgs?
		//		a dictionary of parameters
		if(!params){ params = {}; }
		this.singular = params.singular;
		this.autoSync = params.autoSync;
		// class-specific variables
		this.selection = {};
		this.anchor = null;
		this.simpleSelection = false;
		// set up events
		this.events.push(
			on(this.node, touch.press, lang.hitch(this, "onMouseDown")),
			on(this.node, touch.release, lang.hitch(this, "onMouseUp"))
		);
	},

	// object attributes (for markup)
	singular: false,	// is singular property

	// methods
	getSelectedNodes: function(){
		// summary:
		//		returns a list (an array) of selected nodes
		var t = new NodeList();
		var e = dnd._empty;
		for(var i in this.selection){
			if(i in e){ continue; }
			t.push(dom.byId(i));
		}
		return t;	// NodeList
	},
	selectNone: function(){
		// summary:
		//		unselects all items
		return this._removeSelection()._removeAnchor();	// self
	},
	selectAll: function(){
		// summary:
		//		selects all items
		this.forInItems(function(data, id){
			this._addItemClass(dom.byId(id), "Selected");
			this.selection[id] = 1;
		}, this);
		return this._removeAnchor();	// self
	},
	deleteSelectedNodes: function(){
		// summary:
		//		deletes all selected items
		var e = dnd._empty;
		for(var i in this.selection){
			if(i in e){ continue; }
			var n = dom.byId(i);
			this.delItem(i);
			domConstruct.destroy(n);
		}
		this.anchor = null;
		this.selection = {};
		return this;	// self
	},
	forInSelectedItems: function(/*Function*/ f, /*Object?*/ o){
		// summary:
		//		iterates over selected items;
		//		see `dojo/dnd/Container.forInItems()` for details
		o = o || kernel.global;
		var s = this.selection, e = dnd._empty;
		for(var i in s){
			if(i in e){ continue; }
			f.call(o, this.getItem(i), i, this);
		}
	},
	sync: function(){
		// summary:
		//		sync up the node list with the data map

		Selector.superclass.sync.call(this);

		// fix the anchor
		if(this.anchor){
			if(!this.getItem(this.anchor.id)){
				this.anchor = null;
			}
		}

		// fix the selection
		var t = [], e = dnd._empty;
		for(var i in this.selection){
			if(i in e){ continue; }
			if(!this.getItem(i)){
				t.push(i);
			}
		}
		array.forEach(t, function(i){
			delete this.selection[i];
		}, this);

		return this;	// self
	},
	insertNodes: function(addSelected, data, before, anchor){
		// summary:
		//		inserts new data items (see `dojo/dnd/Container.insertNodes()` method for details)
		// addSelected: Boolean
		//		all new nodes will be added to selected items, if true, no selection change otherwise
		// data: Array
		//		a list of data items, which should be processed by the creator function
		// before: Boolean
		//		insert before the anchor, if true, and after the anchor otherwise
		// anchor: Node
		//		the anchor node to be used as a point of insertion
		var oldCreator = this._normalizedCreator;
		this._normalizedCreator = function(item, hint){
			var t = oldCreator.call(this, item, hint);
			if(addSelected){
				if(!this.anchor){
					this.anchor = t.node;
					this._removeItemClass(t.node, "Selected");
					this._addItemClass(this.anchor, "Anchor");
				}else if(this.anchor != t.node){
					this._removeItemClass(t.node, "Anchor");
					this._addItemClass(t.node, "Selected");
				}
				this.selection[t.node.id] = 1;
			}else{
				this._removeItemClass(t.node, "Selected");
				this._removeItemClass(t.node, "Anchor");
			}
			return t;
		};
		Selector.superclass.insertNodes.call(this, data, before, anchor);
		this._normalizedCreator = oldCreator;
		return this;	// self
	},
	destroy: function(){
		// summary:
		//		prepares the object to be garbage-collected
		Selector.superclass.destroy.call(this);
		this.selection = this.anchor = null;
	},

	// mouse events
	onMouseDown: function(e){
		// summary:
		//		event processor for onmousedown
		// e: Event
		//		mouse event
		if(this.autoSync){ this.sync(); }
		if(!this.current){ return; }
		if(!this.singular && !dnd.getCopyKeyState(e) && !e.shiftKey && (this.current.id in this.selection)){
			this.simpleSelection = true;
			if(mouse.isLeft(e)){
				// Accept the left button and stop the event.   Stopping the event prevents text selection while
				// dragging.   However, don't stop the event on mobile because that prevents a click event,
				// and also prevents scroll (see #15838).
				// For IE we don't stop event when multiple buttons are pressed.
				e.stopPropagation();
				e.preventDefault();
			}
			return;
		}
		if(!this.singular && e.shiftKey){
			if(!dnd.getCopyKeyState(e)){
				this._removeSelection();
			}
			var c = this.getAllNodes();
			if(c.length){
				if(!this.anchor){
					this.anchor = c[0];
					this._addItemClass(this.anchor, "Anchor");
				}
				this.selection[this.anchor.id] = 1;
				if(this.anchor != this.current){
					var i = 0, node;
					for(; i < c.length; ++i){
						node = c[i];
						if(node == this.anchor || node == this.current){ break; }
					}
					for(++i; i < c.length; ++i){
						node = c[i];
						if(node == this.anchor || node == this.current){ break; }
						this._addItemClass(node, "Selected");
						this.selection[node.id] = 1;
					}
					this._addItemClass(this.current, "Selected");
					this.selection[this.current.id] = 1;
				}
			}
		}else{
			if(this.singular){
				if(this.anchor == this.current){
					if(dnd.getCopyKeyState(e)){
						this.selectNone();
					}
				}else{
					this.selectNone();
					this.anchor = this.current;
					this._addItemClass(this.anchor, "Anchor");
					this.selection[this.current.id] = 1;
				}
			}else{
				if(dnd.getCopyKeyState(e)){
					if(this.anchor == this.current){
						delete this.selection[this.anchor.id];
						this._removeAnchor();
					}else{
						if(this.current.id in this.selection){
							this._removeItemClass(this.current, "Selected");
							delete this.selection[this.current.id];
						}else{
							if(this.anchor){
								this._removeItemClass(this.anchor, "Anchor");
								this._addItemClass(this.anchor, "Selected");
							}
							this.anchor = this.current;
							this._addItemClass(this.current, "Anchor");
							this.selection[this.current.id] = 1;
						}
					}
				}else{
					if(!(this.current.id in this.selection)){
						this.selectNone();
						this.anchor = this.current;
						this._addItemClass(this.current, "Anchor");
						this.selection[this.current.id] = 1;
					}
				}
			}
		}
		e.stopPropagation();
		e.preventDefault();
	},
	onMouseUp: function(/*===== e =====*/){
		// summary:
		//		event processor for onmouseup
		// e: Event
		//		mouse event
		if(!this.simpleSelection){ return; }
		this.simpleSelection = false;
		this.selectNone();
		if(this.current){
			this.anchor = this.current;
			this._addItemClass(this.anchor, "Anchor");
			this.selection[this.current.id] = 1;
		}
	},
	onMouseMove: function(/*===== e =====*/){
		// summary:
		//		event processor for onmousemove
		// e: Event
		//		mouse event
		this.simpleSelection = false;
	},

	// utilities
	onOverEvent: function(){
		// summary:
		//		this function is called once, when mouse is over our container
		this.onmousemoveEvent = on(this.node, touch.move, lang.hitch(this, "onMouseMove"));
	},
	onOutEvent: function(){
		// summary:
		//		this function is called once, when mouse is out of our container
		if(this.onmousemoveEvent){
			this.onmousemoveEvent.remove();
			delete this.onmousemoveEvent;
		}
	},
	_removeSelection: function(){
		// summary:
		//		unselects all items
		var e = dnd._empty;
		for(var i in this.selection){
			if(i in e){ continue; }
			var node = dom.byId(i);
			if(node){ this._removeItemClass(node, "Selected"); }
		}
		this.selection = {};
		return this;	// self
	},
	_removeAnchor: function(){
		if(this.anchor){
			this._removeItemClass(this.anchor, "Anchor");
			this.anchor = null;
		}
		return this;	// self
	}
});

return Selector;

});

},
'dojo/dnd/Container':function(){
define([
	"../_base/array",
	"../_base/declare",
	"../_base/kernel",
	"../_base/lang",
	"../_base/window",
	"../dom",
	"../dom-class",
	"../dom-construct",
	"../Evented",
	"../has",
	"../on",
	"../query",
	"../touch",
	"./common"
], function(
	array, declare, kernel, lang, win,
	dom, domClass, domConstruct, Evented, has, on, query, touch, dnd){

// module:
//		dojo/dnd/Container

/*
	Container states:
		""		- normal state
		"Over"	- mouse over a container
	Container item states:
		""		- normal state
		"Over"	- mouse over a container item
*/



var Container = declare("dojo.dnd.Container", Evented, {
	// summary:
	//		a Container object, which knows when mouse hovers over it,
	//		and over which element it hovers

	// object attributes (for markup)
	skipForm: false,
	// allowNested: Boolean
	//		Indicates whether to allow dnd item nodes to be nested within other elements.
	//		By default this is false, indicating that only direct children of the container can
	//		be draggable dnd item nodes
	allowNested: false,
	/*=====
	// current: DomNode
	//		The DOM node the mouse is currently hovered over
	current: null,

	// map: Hash<String, Container.Item>
	//		Map from an item's id (which is also the DOMNode's id) to
	//		the dojo/dnd/Container.Item itself.
	map: {},
	=====*/

	constructor: function(node, params){
		// summary:
		//		a constructor of the Container
		// node: Node
		//		node or node's id to build the container on
		// params: Container.__ContainerArgs
		//		a dictionary of parameters
		this.node = dom.byId(node);
		if(!params){ params = {}; }
		this.creator = params.creator || null;
		this.skipForm = params.skipForm;
		this.parent = params.dropParent && dom.byId(params.dropParent);

		// class-specific variables
		this.map = {};
		this.current = null;

		// states
		this.containerState = "";
		domClass.add(this.node, "dojoDndContainer");

		// mark up children
		if(!(params && params._skipStartup)){
			this.startup();
		}

		// set up events
		this.events = [
			on(this.node, touch.over, lang.hitch(this, "onMouseOver")),
			on(this.node, touch.out,  lang.hitch(this, "onMouseOut")),
			// cancel text selection and text dragging
			on(this.node, "dragstart",   lang.hitch(this, "onSelectStart")),
			on(this.node, "selectstart", lang.hitch(this, "onSelectStart"))
		];
	},

	// object attributes (for markup)
	creator: function(){
		// summary:
		//		creator function, dummy at the moment
	},

	// abstract access to the map
	getItem: function(/*String*/ key){
		// summary:
		//		returns a data item by its key (id)
		return this.map[key];	// Container.Item
	},
	setItem: function(/*String*/ key, /*Container.Item*/ data){
		// summary:
		//		associates a data item with its key (id)
		this.map[key] = data;
	},
	delItem: function(/*String*/ key){
		// summary:
		//		removes a data item from the map by its key (id)
		delete this.map[key];
	},
	forInItems: function(/*Function*/ f, /*Object?*/ o){
		// summary:
		//		iterates over a data map skipping members that
		//		are present in the empty object (IE and/or 3rd-party libraries).
		o = o || kernel.global;
		var m = this.map, e = dnd._empty;
		for(var i in m){
			if(i in e){ continue; }
			f.call(o, m[i], i, this);
		}
		return o;	// Object
	},
	clearItems: function(){
		// summary:
		//		removes all data items from the map
		this.map = {};
	},

	// methods
	getAllNodes: function(){
		// summary:
		//		returns a list (an array) of all valid child nodes
		return query((this.allowNested ? "" : "> ") + ".dojoDndItem", this.parent);	// NodeList
	},
	sync: function(){
		// summary:
		//		sync up the node list with the data map
		var map = {};
		this.getAllNodes().forEach(function(node){
			if(node.id){
				var item = this.getItem(node.id);
				if(item){
					map[node.id] = item;
					return;
				}
			}else{
				node.id = dnd.getUniqueId();
			}
			var type = node.getAttribute("dndType"),
				data = node.getAttribute("dndData");
			map[node.id] = {
				data: data || node.innerHTML,
				type: type ? type.split(/\s*,\s*/) : ["text"]
			};
		}, this);
		this.map = map;
		return this;	// self
	},
	insertNodes: function(data, before, anchor){
		// summary:
		//		inserts an array of new nodes before/after an anchor node
		// data: Array
		//		a list of data items, which should be processed by the creator function
		// before: Boolean
		//		insert before the anchor, if true, and after the anchor otherwise
		// anchor: Node
		//		the anchor node to be used as a point of insertion
		if(!this.parent.firstChild){
			anchor = null;
		}else if(before){
			if(!anchor){
				anchor = this.parent.firstChild;
			}
		}else{
			if(anchor){
				anchor = anchor.nextSibling;
			}
		}
		var i, t;
		if(anchor){
			for(i = 0; i < data.length; ++i){
				t = this._normalizedCreator(data[i]);
				this.setItem(t.node.id, {data: t.data, type: t.type});
				anchor.parentNode.insertBefore(t.node, anchor);
			}
		}else{
			for(i = 0; i < data.length; ++i){
				t = this._normalizedCreator(data[i]);
				this.setItem(t.node.id, {data: t.data, type: t.type});
				this.parent.appendChild(t.node);
			}
		}
		return this;	// self
	},
	destroy: function(){
		// summary:
		//		prepares this object to be garbage-collected
		array.forEach(this.events, function(handle){ handle.remove(); });
		this.clearItems();
		this.node = this.parent = this.current = null;
	},

	// markup methods
	markupFactory: function(params, node, Ctor){
		params._skipStartup = true;
		return new Ctor(node, params);
	},
	startup: function(){
		// summary:
		//		collects valid child items and populate the map

		// set up the real parent node
		if(!this.parent){
			// use the standard algorithm, if not assigned
			this.parent = this.node;
			if(this.parent.tagName.toLowerCase() == "table"){
				var c = this.parent.getElementsByTagName("tbody");
				if(c && c.length){ this.parent = c[0]; }
			}
		}
		this.defaultCreator = dnd._defaultCreator(this.parent);

		// process specially marked children
		this.sync();
	},

	// mouse events
	onMouseOver: function(e){
		// summary:
		//		event processor for onmouseover or touch, to mark that element as the current element
		// e: Event
		//		mouse event
		var n = e.relatedTarget;
		while(n){
			if(n == this.node){ break; }
			try{
				n = n.parentNode;
			}catch(x){
				n = null;
			}
		}
		if(!n){
			this._changeState("Container", "Over");
			this.onOverEvent();
		}
		n = this._getChildByEvent(e);
		if(this.current == n){ return; }
		if(this.current){ this._removeItemClass(this.current, "Over"); }
		if(n){ this._addItemClass(n, "Over"); }
		this.current = n;
	},
	onMouseOut: function(e){
		// summary:
		//		event processor for onmouseout
		// e: Event
		//		mouse event
		for(var n = e.relatedTarget; n;){
			if(n == this.node){ return; }
			try{
				n = n.parentNode;
			}catch(x){
				n = null;
			}
		}
		if(this.current){
			this._removeItemClass(this.current, "Over");
			this.current = null;
		}
		this._changeState("Container", "");
		this.onOutEvent();
	},
	onSelectStart: function(e){
		// summary:
		//		event processor for onselectevent and ondragevent
		// e: Event
		//		mouse event
		if(!this.withHandles && (!this.skipForm || !dnd.isFormElement(e))){
			e.stopPropagation();
			e.preventDefault();
		}
	},

	// utilities
	onOverEvent: function(){
		// summary:
		//		this function is called once, when mouse is over our container
	},
	onOutEvent: function(){
		// summary:
		//		this function is called once, when mouse is out of our container
	},
	_changeState: function(type, newState){
		// summary:
		//		changes a named state to new state value
		// type: String
		//		a name of the state to change
		// newState: String
		//		new state
		var prefix = "dojoDnd" + type;
		var state  = type.toLowerCase() + "State";
		//domClass.replace(this.node, prefix + newState, prefix + this[state]);
		domClass.replace(this.node, prefix + newState, prefix + this[state]);
		this[state] = newState;
	},
	_addItemClass: function(node, type){
		// summary:
		//		adds a class with prefix "dojoDndItem"
		// node: Node
		//		a node
		// type: String
		//		a variable suffix for a class name
		domClass.add(node, "dojoDndItem" + type);
	},
	_removeItemClass: function(node, type){
		// summary:
		//		removes a class with prefix "dojoDndItem"
		// node: Node
		//		a node
		// type: String
		//		a variable suffix for a class name
		domClass.remove(node, "dojoDndItem" + type);
	},
	_getChildByEvent: function(e){
		// summary:
		//		gets a child, which is under the mouse at the moment, or null
		// e: Event
		//		a mouse event
		var node = e.target;
		if(node){
			for(var parent = node.parentNode; parent; node = parent, parent = node.parentNode){
				if((parent == this.parent || this.allowNested) && domClass.contains(node, "dojoDndItem")){ return node; }
			}
		}
		return null;
	},
	_normalizedCreator: function(/*Container.Item*/ item, /*String*/ hint){
		// summary:
		//		adds all necessary data to the output of the user-supplied creator function
		var t = (this.creator || this.defaultCreator).call(this, item, hint);
		if(!lang.isArray(t.type)){ t.type = ["text"]; }
		if(!t.node.id){ t.node.id = dnd.getUniqueId(); }
		domClass.add(t.node, "dojoDndItem");
		return t;
	}
});

dnd._createNode = function(tag){
	// summary:
	//		returns a function, which creates an element of given tag
	//		(SPAN by default) and sets its innerHTML to given text
	// tag: String
	//		a tag name or empty for SPAN
	if(!tag){ return dnd._createSpan; }
	return function(text){	// Function
		return domConstruct.create(tag, {innerHTML: text});	// Node
	};
};

dnd._createTrTd = function(text){
	// summary:
	//		creates a TR/TD structure with given text as an innerHTML of TD
	// text: String
	//		a text for TD
	var tr = domConstruct.create("tr");
	domConstruct.create("td", {innerHTML: text}, tr);
	return tr;	// Node
};

dnd._createSpan = function(text){
	// summary:
	//		creates a SPAN element with given text as its innerHTML
	// text: String
	//		a text for SPAN
	return domConstruct.create("span", {innerHTML: text});	// Node
};

// dnd._defaultCreatorNodes: Object
//		a dictionary that maps container tag names to child tag names
dnd._defaultCreatorNodes = {ul: "li", ol: "li", div: "div", p: "div"};

dnd._defaultCreator = function(node){
	// summary:
	//		takes a parent node, and returns an appropriate creator function
	// node: Node
	//		a container node
	var tag = node.tagName.toLowerCase();
	var c = tag == "tbody" || tag == "thead" ? dnd._createTrTd :
			dnd._createNode(dnd._defaultCreatorNodes[tag]);
	return function(item, hint){	// Function
		var isObj = item && lang.isObject(item), data, type, n;
		if(isObj && item.tagName && item.nodeType && item.getAttribute){
			// process a DOM node
			data = item.getAttribute("dndData") || item.innerHTML;
			type = item.getAttribute("dndType");
			type = type ? type.split(/\s*,\s*/) : ["text"];
			n = item;	// this node is going to be moved rather than copied
		}else{
			// process a DnD item object or a string
			data = (isObj && item.data) ? item.data : item;
			type = (isObj && item.type) ? item.type : ["text"];
			n = (hint == "avatar" ? dnd._createSpan : c)(String(data));
		}
		if(!n.id){
			n.id = dnd.getUniqueId();
		}
		return {node: n, data: data, type: type};
	};
};

/*=====
Container.__ContainerArgs = declare([], {
	creator: function(){
		// summary:
		//		a creator function, which takes a data item, and returns an object like that:
		//		{node: newNode, data: usedData, type: arrayOfStrings}
	},

	// skipForm: Boolean
	//		don't start the drag operation, if clicked on form elements
	skipForm: false,

	// dropParent: Node||String
	//		node or node's id to use as the parent node for dropped items
	//		(must be underneath the 'node' parameter in the DOM)
	dropParent: null,

	// _skipStartup: Boolean
	//		skip startup(), which collects children, for deferred initialization
	//		(this is used in the markup mode)
	_skipStartup: false
});

Container.Item = function(){
	// summary:
	//		Represents (one of) the source node(s) being dragged.
	//		Contains (at least) the "type" and "data" attributes.
	// type: String[]
	//		Type(s) of this item, by default this is ["text"]
	// data: Object
	//		Logical representation of the object being dragged.
	//		If the drag object's type is "text" then data is a String,
	//		if it's another type then data could be a different Object,
	//		perhaps a name/value hash.

	this.type = type;
	this.data = data;
};
=====*/

return Container;
});

},
'dojo/dnd/Manager':function(){
define([
	"../_base/array",  "../_base/declare", "../_base/lang", "../_base/window",
	"../dom-class", "../Evented", "../has", "../keys", "../on", "../topic", "../touch",
	"./common", "./autoscroll", "./Avatar"
], function(array, declare, lang, win, domClass, Evented, has, keys, on, topic, touch,
	dnd, autoscroll, Avatar){

// module:
//		dojo/dnd/Manager

var Manager = declare("dojo.dnd.Manager", [Evented], {
	// summary:
	//		the manager of DnD operations (usually a singleton)
	constructor: function(){
		this.avatar  = null;
		this.source = null;
		this.nodes = [];
		this.copy  = true;
		this.target = null;
		this.canDropFlag = false;
		this.events = [];
	},

	// avatar's offset from the mouse
	OFFSET_X: has("touch") ? 4 : 16,
	OFFSET_Y: has("touch") ? 4 : 16,

	// methods
	overSource: function(source){
		// summary:
		//		called when a source detected a mouse-over condition
		// source: Object
		//		the reporter
		if(this.avatar){
			this.target = (source && source.targetState != "Disabled") ? source : null;
			this.canDropFlag = Boolean(this.target);
			this.avatar.update();
		}
		topic.publish("/dnd/source/over", source);
	},
	outSource: function(source){
		// summary:
		//		called when a source detected a mouse-out condition
		// source: Object
		//		the reporter
		if(this.avatar){
			if(this.target == source){
				this.target = null;
				this.canDropFlag = false;
				this.avatar.update();
				topic.publish("/dnd/source/over", null);
			}
		}else{
			topic.publish("/dnd/source/over", null);
		}
	},
	startDrag: function(source, nodes, copy){
		// summary:
		//		called to initiate the DnD operation
		// source: Object
		//		the source which provides items
		// nodes: Array
		//		the list of transferred items
		// copy: Boolean
		//		copy items, if true, move items otherwise

		// Tell autoscroll that a drag is starting
		autoscroll.autoScrollStart(win.doc);

		this.source = source;
		this.nodes  = nodes;
		this.copy   = Boolean(copy); // normalizing to true boolean
		this.avatar = this.makeAvatar();
		win.body().appendChild(this.avatar.node);
		topic.publish("/dnd/start", source, nodes, this.copy);

		function stopEvent(e){
			e.preventDefault();
			e.stopPropagation();
		}

		this.events = [
			on(win.doc, touch.move, lang.hitch(this, "onMouseMove")),
			on(win.doc, touch.release,   lang.hitch(this, "onMouseUp")),
			on(win.doc, "keydown",   lang.hitch(this, "onKeyDown")),
			on(win.doc, "keyup",     lang.hitch(this, "onKeyUp")),

			// cancel text selection and text dragging
			on(win.doc, "dragstart",   stopEvent),
			on(win.body(), "selectstart", stopEvent)
		];
		var c = "dojoDnd" + (copy ? "Copy" : "Move");
		domClass.add(win.body(), c);
	},
	canDrop: function(flag){
		// summary:
		//		called to notify if the current target can accept items
		var canDropFlag = Boolean(this.target && flag);
		if(this.canDropFlag != canDropFlag){
			this.canDropFlag = canDropFlag;
			this.avatar.update();
		}
	},
	stopDrag: function(){
		// summary:
		//		stop the DnD in progress
		domClass.remove(win.body(), ["dojoDndCopy", "dojoDndMove"]);
		array.forEach(this.events, function(handle){ handle.remove(); });
		this.events = [];
		this.avatar.destroy();
		this.avatar = null;
		this.source = this.target = null;
		this.nodes = [];
	},
	makeAvatar: function(){
		// summary:
		//		makes the avatar; it is separate to be overwritten dynamically, if needed
		return new Avatar(this);
	},
	updateAvatar: function(){
		// summary:
		//		updates the avatar; it is separate to be overwritten dynamically, if needed
		this.avatar.update();
	},

	// mouse event processors
	onMouseMove: function(e){
		// summary:
		//		event processor for onmousemove
		// e: Event
		//		mouse event
		var a = this.avatar;
		if(a){
			autoscroll.autoScrollNodes(e);
			//autoscroll.autoScroll(e);
			var s = a.node.style;
			s.left = (e.pageX + this.OFFSET_X) + "px";
			s.top  = (e.pageY + this.OFFSET_Y) + "px";
			var copy = Boolean(this.source.copyState(dnd.getCopyKeyState(e)));
			if(this.copy != copy){
				this._setCopyStatus(copy);
			}
		}
		if(has("touch")){
			// Prevent page from scrolling so that user can drag instead.
			e.preventDefault();
		}
	},
	onMouseUp: function(e){
		// summary:
		//		event processor for onmouseup
		// e: Event
		//		mouse event
		if(this.avatar){
			if(this.target && this.canDropFlag){
				var copy = Boolean(this.source.copyState(dnd.getCopyKeyState(e)));
				topic.publish("/dnd/drop/before", this.source, this.nodes, copy, this.target, e);
				topic.publish("/dnd/drop", this.source, this.nodes, copy, this.target, e);
			}else{
				topic.publish("/dnd/cancel");
			}
			this.stopDrag();
		}
	},

	// keyboard event processors
	onKeyDown: function(e){
		// summary:
		//		event processor for onkeydown:
		//		watching for CTRL for copy/move status, watching for ESCAPE to cancel the drag
		// e: Event
		//		keyboard event
		if(this.avatar){
			switch(e.keyCode){
				case keys.CTRL:
					var copy = Boolean(this.source.copyState(true));
					if(this.copy != copy){
						this._setCopyStatus(copy);
					}
					break;
				case keys.ESCAPE:
					topic.publish("/dnd/cancel");
					this.stopDrag();
					break;
			}
		}
	},
	onKeyUp: function(e){
		// summary:
		//		event processor for onkeyup, watching for CTRL for copy/move status
		// e: Event
		//		keyboard event
		if(this.avatar && e.keyCode == keys.CTRL){
			var copy = Boolean(this.source.copyState(false));
			if(this.copy != copy){
				this._setCopyStatus(copy);
			}
		}
	},

	// utilities
	_setCopyStatus: function(copy){
		// summary:
		//		changes the copy status
		// copy: Boolean
		//		the copy status
		this.copy = copy;
		this.source._markDndStatus(this.copy);
		this.updateAvatar();
		domClass.replace(win.body(),
			"dojoDnd" + (this.copy ? "Copy" : "Move"),
			"dojoDnd" + (this.copy ? "Move" : "Copy"));
	}
});

// dnd._manager:
//		The manager singleton variable. Can be overwritten if needed.
dnd._manager = null;

Manager.manager = dnd.manager = function(){
	// summary:
	//		Returns the current DnD manager.  Creates one if it is not created yet.
	if(!dnd._manager){
		dnd._manager = new Manager();
	}
	return dnd._manager;	// Object
};

// TODO: for 2.0, store _manager and manager in Manager only.   Don't access dnd or dojo.dnd.

return Manager;
});

},
'dojo/dnd/autoscroll':function(){
define(["../_base/lang", "../sniff", "../_base/window", "../dom-geometry", "../dom-style", "../window"],
	function(lang, has, win, domGeom, domStyle, winUtils){

// module:
//		dojo/dnd/autoscroll

var exports = {
	// summary:
	//		Used by dojo/dnd/Manager to scroll document or internal node when the user
	//		drags near the edge of the viewport or a scrollable node
};
lang.setObject("dojo.dnd.autoscroll", exports);

exports.getViewport = winUtils.getBox;

exports.V_TRIGGER_AUTOSCROLL = 32;
exports.H_TRIGGER_AUTOSCROLL = 32;

exports.V_AUTOSCROLL_VALUE = 16;
exports.H_AUTOSCROLL_VALUE = 16;

// These are set by autoScrollStart().
// Set to default values in case autoScrollStart() isn't called. (back-compat, remove for 2.0)
var viewport,
	doc = win.doc,
	maxScrollTop = Infinity,
	maxScrollLeft = Infinity;

exports.autoScrollStart = function(d){
	// summary:
	//		Called at the start of a drag.
	// d: Document
	//		The document of the node being dragged.

	doc = d;
	viewport = winUtils.getBox(doc);

	// Save height/width of document at start of drag, before it gets distorted by a user dragging an avatar past
	// the document's edge
	var html = win.body(doc).parentNode;
	maxScrollTop = Math.max(html.scrollHeight - viewport.h, 0);
	maxScrollLeft = Math.max(html.scrollWidth - viewport.w, 0);	// usually 0
};

exports.autoScroll = function(e){
	// summary:
	//		a handler for mousemove and touchmove events, which scrolls the window, if
	//		necessary
	// e: Event
	//		mousemove/touchmove event

	// FIXME: needs more docs!
	var v = viewport || winUtils.getBox(doc), // getBox() call for back-compat, in case autoScrollStart() wasn't called
		html = win.body(doc).parentNode,
		dx = 0, dy = 0;
	if(e.clientX < exports.H_TRIGGER_AUTOSCROLL){
		dx = -exports.H_AUTOSCROLL_VALUE;
	}else if(e.clientX > v.w - exports.H_TRIGGER_AUTOSCROLL){
		dx = Math.min(exports.H_AUTOSCROLL_VALUE, maxScrollLeft - html.scrollLeft);	// don't scroll past edge of doc
	}
	if(e.clientY < exports.V_TRIGGER_AUTOSCROLL){
		dy = -exports.V_AUTOSCROLL_VALUE;
	}else if(e.clientY > v.h - exports.V_TRIGGER_AUTOSCROLL){
		dy = Math.min(exports.V_AUTOSCROLL_VALUE, maxScrollTop - html.scrollTop);	// don't scroll past edge of doc
	}
	window.scrollBy(dx, dy);
};

exports._validNodes = {"div": 1, "p": 1, "td": 1};
exports._validOverflow = {"auto": 1, "scroll": 1};

exports.autoScrollNodes = function(e){
	// summary:
	//		a handler for mousemove and touchmove events, which scrolls the first available
	//		Dom element, it falls back to exports.autoScroll()
	// e: Event
	//		mousemove/touchmove event

	// FIXME: needs more docs!

	var b, t, w, h, rx, ry, dx = 0, dy = 0, oldLeft, oldTop;

	for(var n = e.target; n;){
		if(n.nodeType == 1 && (n.tagName.toLowerCase() in exports._validNodes)){
			var s = domStyle.getComputedStyle(n),
				overflowX = (s.overflowX.toLowerCase() in exports._validOverflow),
				overflowY = (s.overflowY.toLowerCase() in exports._validOverflow);
			if(overflowX || overflowY){
				b = domGeom.getContentBox(n, s);
				t = domGeom.position(n, true);
			}
			// overflow-x
			if(overflowX){
				w = Math.min(exports.H_TRIGGER_AUTOSCROLL, b.w / 2);
				rx = e.pageX - t.x;
				if(has("webkit") || has("opera")){
					// FIXME: this code should not be here, it should be taken into account
					// either by the event fixing code, or the domGeom.position()
					// FIXME: this code doesn't work on Opera 9.5 Beta
					rx += win.body().scrollLeft;
				}
				dx = 0;
				if(rx > 0 && rx < b.w){
					if(rx < w){
						dx = -w;
					}else if(rx > b.w - w){
						dx = w;
					}
					oldLeft = n.scrollLeft;
					n.scrollLeft = n.scrollLeft + dx;
				}
			}
			// overflow-y
			if(overflowY){
				//console.log(b.l, b.t, t.x, t.y, n.scrollLeft, n.scrollTop);
				h = Math.min(exports.V_TRIGGER_AUTOSCROLL, b.h / 2);
				ry = e.pageY - t.y;
				if(has("webkit") || has("opera")){
					// FIXME: this code should not be here, it should be taken into account
					// either by the event fixing code, or the domGeom.position()
					// FIXME: this code doesn't work on Opera 9.5 Beta
					ry += win.body().scrollTop;
				}
				dy = 0;
				if(ry > 0 && ry < b.h){
					if(ry < h){
						dy = -h;
					}else if(ry > b.h - h){
						dy = h;
					}
					oldTop = n.scrollTop;
					n.scrollTop  = n.scrollTop  + dy;
				}
			}
			if(dx || dy){ return; }
		}
		try{
			n = n.parentNode;
		}catch(x){
			n = null;
		}
	}
	exports.autoScroll(e);
};

return exports;

});

},
'dojo/dnd/Avatar':function(){
define([
	"../_base/declare",
	"../_base/window",
	"../dom",
	"../dom-attr",
	"../dom-class",
	"../dom-construct",
	"../hccss",
	"../query"
], function(declare, win, dom, domAttr, domClass, domConstruct, has, query){

// module:
//		dojo/dnd/Avatar

return declare("dojo.dnd.Avatar", null, {
	// summary:
	//		Object that represents transferred DnD items visually
	// manager: Object
	//		a DnD manager object

	constructor: function(manager){
		this.manager = manager;
		this.construct();
	},

	// methods
	construct: function(){
		// summary:
		//		constructor function;
		//		it is separate so it can be (dynamically) overwritten in case of need

		var a = domConstruct.create("table", {
				"class": "dojoDndAvatar",
				style: {
					position: "absolute",
					zIndex:   "1999",
					margin:   "0px"
				}
			}),
			source = this.manager.source, node,
			b = domConstruct.create("tbody", null, a),
			tr = domConstruct.create("tr", null, b),
			td = domConstruct.create("td", null, tr),
			k = Math.min(5, this.manager.nodes.length), i = 0;

		if(has("highcontrast")){
			domConstruct.create("span", {
				id : "a11yIcon",
				innerHTML : this.manager.copy ? '+' : "<"
			}, td)
		}
		domConstruct.create("span", {
			innerHTML: source.generateText ? this._generateText() : ""
		}, td);

		// we have to set the opacity on IE only after the node is live
		domAttr.set(tr, {
			"class": "dojoDndAvatarHeader",
			style: {opacity: 0.9}
		});
		for(; i < k; ++i){
			if(source.creator){
				// create an avatar representation of the node
				node = source._normalizedCreator(source.getItem(this.manager.nodes[i].id).data, "avatar").node;
			}else{
				// or just clone the node and hope it works
				node = this.manager.nodes[i].cloneNode(true);
				if(node.tagName.toLowerCase() == "tr"){
					// insert extra table nodes
					var table = domConstruct.create("table"),
						tbody = domConstruct.create("tbody", null, table);
					tbody.appendChild(node);
					node = table;
				}
			}
			node.id = "";
			tr = domConstruct.create("tr", null, b);
			td = domConstruct.create("td", null, tr);
			td.appendChild(node);
			domAttr.set(tr, {
				"class": "dojoDndAvatarItem",
				style: {opacity: (9 - i) / 10}
			});
		}
		this.node = a;
	},
	destroy: function(){
		// summary:
		//		destructor for the avatar; called to remove all references so it can be garbage-collected
		domConstruct.destroy(this.node);
		this.node = false;
	},
	update: function(){
		// summary:
		//		updates the avatar to reflect the current DnD state
		domClass.toggle(this.node, "dojoDndAvatarCanDrop", this.manager.canDropFlag);
		if(has("highcontrast")){
			var icon = dom.byId("a11yIcon");
			var text = '+';   // assume canDrop && copy
			if (this.manager.canDropFlag && !this.manager.copy){
				text = '< '; // canDrop && move
			}else if (!this.manager.canDropFlag && !this.manager.copy){
				text = "o"; //!canDrop && move
			}else if(!this.manager.canDropFlag){
				text = 'x';  // !canDrop && copy
			}
			icon.innerHTML=text;
		}
		// replace text
		query(("tr.dojoDndAvatarHeader td span" +(has("highcontrast") ? " span" : "")), this.node).forEach(
			function(node){
				node.innerHTML = this.manager.source.generateText ? this._generateText() : "";
			}, this);
	},
	_generateText: function(){
		// summary:
		//		generates a proper text to reflect copying or moving of items
		return this.manager.nodes.length.toString();
	}
});

});

},
'dijit/_TemplatedMixin':function(){
define([
	"dojo/cache",	// dojo.cache
	"dojo/_base/declare", // declare
	"dojo/dom-construct", // domConstruct.destroy, domConstruct.toDom
	"dojo/_base/lang", // lang.getObject
	"dojo/on",
	"dojo/sniff", // has("ie")
	"dojo/string", // string.substitute string.trim
	"./_AttachMixin"
], function(cache, declare, domConstruct, lang, on, has, string, _AttachMixin){

	// module:
	//		dijit/_TemplatedMixin

	var _TemplatedMixin = declare("dijit._TemplatedMixin", _AttachMixin, {
		// summary:
		//		Mixin for widgets that are instantiated from a template

		// templateString: [protected] String
		//		A string that represents the widget template.
		//		Use in conjunction with dojo.cache() to load from a file.
		templateString: null,

		// templatePath: [protected deprecated] String
		//		Path to template (HTML file) for this widget relative to dojo.baseUrl.
		//		Deprecated: use templateString with require([... "dojo/text!..."], ...) instead
		templatePath: null,

		// skipNodeCache: [protected] Boolean
		//		If using a cached widget template nodes poses issues for a
		//		particular widget class, it can set this property to ensure
		//		that its template is always re-built from a string
		_skipNodeCache: false,

/*=====
		// _rendered: Boolean
		//		Not normally use, but this flag can be set by the app if the server has already rendered the template,
		//		i.e. already inlining the template for the widget into the main page.   Reduces _TemplatedMixin to
		//		just function like _AttachMixin.
		_rendered: false,
=====*/

		// Set _AttachMixin.searchContainerNode to true for back-compat for widgets that have data-dojo-attach-point's
		// and events inside this.containerNode.   Remove for 2.0.
		searchContainerNode: true,

		_stringRepl: function(tmpl){
			// summary:
			//		Does substitution of ${foo} type properties in template string
			// tags:
			//		private
			var className = this.declaredClass, _this = this;
			// Cache contains a string because we need to do property replacement
			// do the property replacement
			return string.substitute(tmpl, this, function(value, key){
				if(key.charAt(0) == '!'){ value = lang.getObject(key.substr(1), false, _this); }
				if(typeof value == "undefined"){ throw new Error(className+" template:"+key); } // a debugging aide
				if(value == null){ return ""; }

				// Substitution keys beginning with ! will skip the transform step,
				// in case a user wishes to insert unescaped markup, e.g. ${!foo}
				return key.charAt(0) == "!" ? value : this._escapeValue("" + value);
			}, this);
		},

		_escapeValue: function(/*String*/ val){
			// summary:
			//		Escape a value to be inserted into the template, either into an attribute value
			//		(ex: foo="${bar}") or as inner text of an element (ex: <span>${foo}</span>)

			// Safer substitution, see heading "Attribute values" in
			// http://www.w3.org/TR/REC-html40/appendix/notes.html#h-B.3.2
			// and also https://www.owasp.org/index.php/XSS_%28Cross_Site_Scripting%29_Prevention_Cheat_Sheet#RULE_.231_-_HTML_Escape_Before_Inserting_Untrusted_Data_into_HTML_Element_Content
			return val.replace(/["'<>&]/g, function(val){
				return {
					"&": "&amp;",
					"<": "&lt;",
					">": "&gt;",
					"\"": "&quot;",
					"'": "&#x27;"
				}[val];
			});
		},

		buildRendering: function(){
			// summary:
			//		Construct the UI for this widget from a template, setting this.domNode.
			// tags:
			//		protected

			if(!this._rendered){
				if(!this.templateString){
					this.templateString = cache(this.templatePath, {sanitize: true});
				}

				// Lookup cached version of template, and download to cache if it
				// isn't there already.  Returns either a DomNode or a string, depending on
				// whether or not the template contains ${foo} replacement parameters.
				var cached = _TemplatedMixin.getCachedTemplate(this.templateString, this._skipNodeCache, this.ownerDocument);

				var node;
				if(lang.isString(cached)){
					node = domConstruct.toDom(this._stringRepl(cached), this.ownerDocument);
					if(node.nodeType != 1){
						// Flag common problems such as templates with multiple top level nodes (nodeType == 11)
						throw new Error("Invalid template: " + cached);
					}
				}else{
					// if it's a node, all we have to do is clone it
					node = cached.cloneNode(true);
				}

				this.domNode = node;
			}

			// Call down to _WidgetBase.buildRendering() to get base classes assigned
			// TODO: change the baseClass assignment to _setBaseClassAttr
			this.inherited(arguments);

			if(!this._rendered){
				this._fillContent(this.srcNodeRef);
			}

			this._rendered = true;
		},

		_fillContent: function(/*DomNode*/ source){
			// summary:
			//		Relocate source contents to templated container node.
			//		this.containerNode must be able to receive children, or exceptions will be thrown.
			// tags:
			//		protected
			var dest = this.containerNode;
			if(source && dest){
				while(source.hasChildNodes()){
					dest.appendChild(source.firstChild);
				}
			}
		}

	});

	// key is templateString; object is either string or DOM tree
	_TemplatedMixin._templateCache = {};

	_TemplatedMixin.getCachedTemplate = function(templateString, alwaysUseString, doc){
		// summary:
		//		Static method to get a template based on the templatePath or
		//		templateString key
		// templateString: String
		//		The template
		// alwaysUseString: Boolean
		//		Don't cache the DOM tree for this template, even if it doesn't have any variables
		// doc: Document?
		//		The target document.   Defaults to document global if unspecified.
		// returns: Mixed
		//		Either string (if there are ${} variables that need to be replaced) or just
		//		a DOM tree (if the node can be cloned directly)

		// is it already cached?
		var tmplts = _TemplatedMixin._templateCache;
		var key = templateString;
		var cached = tmplts[key];
		if(cached){
			try{
				// if the cached value is an innerHTML string (no ownerDocument) or a DOM tree created within the
				// current document, then use the current cached value
				if(!cached.ownerDocument || cached.ownerDocument == (doc || document)){
					// string or node of the same document
					return cached;
				}
			}catch(e){ /* squelch */ } // IE can throw an exception if cached.ownerDocument was reloaded
			domConstruct.destroy(cached);
		}

		templateString = string.trim(templateString);

		if(alwaysUseString || templateString.match(/\$\{([^\}]+)\}/g)){
			// there are variables in the template so all we can do is cache the string
			return (tmplts[key] = templateString); //String
		}else{
			// there are no variables in the template so we can cache the DOM tree
			var node = domConstruct.toDom(templateString, doc);
			if(node.nodeType != 1){
				throw new Error("Invalid template: " + templateString);
			}
			return (tmplts[key] = node); //Node
		}
	};

	if(has("ie")){
		on(window, "unload", function(){
			var cache = _TemplatedMixin._templateCache;
			for(var key in cache){
				var value = cache[key];
				if(typeof value == "object"){ // value is either a string or a DOM node template
					domConstruct.destroy(value);
				}
				delete cache[key];
			}
		});
	}

	return _TemplatedMixin;
});

},
'dojo/cache':function(){
define(["./_base/kernel", "./text"], function(dojo){
	// module:
	//		dojo/cache

	// dojo.cache is defined in dojo/text
	return dojo.cache;
});

},
'dojo/string':function(){
define([
	"./_base/kernel",	// kernel.global
	"./_base/lang"
], function(kernel, lang){

// module:
//		dojo/string
var ESCAPE_REGEXP = /[&<>'"\/]/g;
var ESCAPE_MAP = {
	'&': '&amp;',
	'<': '&lt;',
	'>': '&gt;',
	'"': '&quot;',
	"'": '&#x27;',
	'/': '&#x2F;'
};
var string = {
	// summary:
	//		String utilities for Dojo
};
lang.setObject("dojo.string", string);

string.escape = function(/*String*/str){
	// summary:
	//		Efficiently escape a string for insertion into HTML (innerHTML or attributes), replacing &, <, >, ", ', and / characters.
	// str:
	//		the string to escape
	if(!str){ return ""; }
	return str.replace(ESCAPE_REGEXP, function(c) {
		return ESCAPE_MAP[c];
	});
};

string.rep = function(/*String*/str, /*Integer*/num){
	// summary:
	//		Efficiently replicate a string `n` times.
	// str:
	//		the string to replicate
	// num:
	//		number of times to replicate the string

	if(num <= 0 || !str){ return ""; }

	var buf = [];
	for(;;){
		if(num & 1){
			buf.push(str);
		}
		if(!(num >>= 1)){ break; }
		str += str;
	}
	return buf.join("");	// String
};

string.pad = function(/*String*/text, /*Integer*/size, /*String?*/ch, /*Boolean?*/end){
	// summary:
	//		Pad a string to guarantee that it is at least `size` length by
	//		filling with the character `ch` at either the start or end of the
	//		string. Pads at the start, by default.
	// text:
	//		the string to pad
	// size:
	//		length to provide padding
	// ch:
	//		character to pad, defaults to '0'
	// end:
	//		adds padding at the end if true, otherwise pads at start
	// example:
	//	|	// Fill the string to length 10 with "+" characters on the right.  Yields "Dojo++++++".
	//	|	string.pad("Dojo", 10, "+", true);

	if(!ch){
		ch = '0';
	}
	var out = String(text),
		pad = string.rep(ch, Math.ceil((size - out.length) / ch.length));
	return end ? out + pad : pad + out;	// String
};

string.substitute = function(	/*String*/		template,
									/*Object|Array*/map,
									/*Function?*/	transform,
									/*Object?*/		thisObject){
	// summary:
	//		Performs parameterized substitutions on a string. Throws an
	//		exception if any parameter is unmatched.
	// template:
	//		a string with expressions in the form `${key}` to be replaced or
	//		`${key:format}` which specifies a format function. keys are case-sensitive.
	//		The special sequence `${}` can be used escape `$`.
	// map:
	//		hash to search for substitutions
	// transform:
	//		a function to process all parameters before substitution takes
	//		place, e.g. mylib.encodeXML
	// thisObject:
	//		where to look for optional format function; default to the global
	//		namespace
	// example:
	//		Substitutes two expressions in a string from an Array or Object
	//	|	// returns "File 'foo.html' is not found in directory '/temp'."
	//	|	// by providing substitution data in an Array
	//	|	string.substitute(
	//	|		"File '${0}' is not found in directory '${1}'.",
	//	|		["foo.html","/temp"]
	//	|	);
	//	|
	//	|	// also returns "File 'foo.html' is not found in directory '/temp'."
	//	|	// but provides substitution data in an Object structure.  Dotted
	//	|	// notation may be used to traverse the structure.
	//	|	string.substitute(
	//	|		"File '${name}' is not found in directory '${info.dir}'.",
	//	|		{ name: "foo.html", info: { dir: "/temp" } }
	//	|	);
	// example:
	//		Use a transform function to modify the values:
	//	|	// returns "file 'foo.html' is not found in directory '/temp'."
	//	|	string.substitute(
	//	|		"${0} is not found in ${1}.",
	//	|		["foo.html","/temp"],
	//	|		function(str){
	//	|			// try to figure out the type
	//	|			var prefix = (str.charAt(0) == "/") ? "directory": "file";
	//	|			return prefix + " '" + str + "'";
	//	|		}
	//	|	);
	// example:
	//		Use a formatter
	//	|	// returns "thinger -- howdy"
	//	|	string.substitute(
	//	|		"${0:postfix}", ["thinger"], null, {
	//	|			postfix: function(value, key){
	//	|				return value + " -- howdy";
	//	|			}
	//	|		}
	//	|	);

	thisObject = thisObject || kernel.global;
	transform = transform ?
		lang.hitch(thisObject, transform) : function(v){ return v; };

	return template.replace(/\$\{([^\s\:\}]*)(?:\:([^\s\:\}]+))?\}/g,
		function(match, key, format){
			if (key == ''){
				return '$';
			}
			var value = lang.getObject(key, false, map);
			if(format){
				value = lang.getObject(format, false, thisObject).call(thisObject, value, key);
			}
			var result = transform(value, key);

			if (typeof result === 'undefined') {
				throw new Error('string.substitute could not find key "' + key + '" in template');
			}

			return result.toString();
		}); // String
};

string.trim = String.prototype.trim ?
	lang.trim : // aliasing to the native function
	function(str){
		str = str.replace(/^\s+/, '');
		for(var i = str.length - 1; i >= 0; i--){
			if(/\S/.test(str.charAt(i))){
				str = str.substring(0, i + 1);
				break;
			}
		}
		return str;
	};

/*=====
 string.trim = function(str){
	 // summary:
	 //		Trims whitespace from both sides of the string
	 // str: String
	 //		String to be trimmed
	 // returns: String
	 //		Returns the trimmed string
	 // description:
	 //		This version of trim() was taken from [Steven Levithan's blog](http://blog.stevenlevithan.com/archives/faster-trim-javascript).
	 //		The short yet performant version of this function is dojo/_base/lang.trim(),
	 //		which is part of Dojo base.  Uses String.prototype.trim instead, if available.
	 return "";	// String
 };
 =====*/

	return string;
});

},
'dijit/_AttachMixin':function(){
define([
	"require",
	"dojo/_base/array", // array.forEach
	"dojo/_base/connect",	// remove for 2.0
	"dojo/_base/declare", // declare
	"dojo/_base/lang", // lang.getObject
	"dojo/mouse",
	"dojo/on",
	"dojo/touch",
	"./_WidgetBase"
], function(require, array, connect, declare, lang, mouse, on, touch, _WidgetBase){

	// module:
	//		dijit/_AttachMixin

	// Map from string name like "mouseenter" to synthetic event like mouse.enter
	var synthEvents = lang.delegate(touch, {
		"mouseenter": mouse.enter,
		"mouseleave": mouse.leave,
		"keypress": connect._keypress	// remove for 2.0
	});

	// To be lightweight, _AttachMixin doesn't require() dijit/a11yclick.
	// If the subclass has a template using "ondijitclick", it must load dijit/a11yclick itself.
	// In that case, the a11yclick variable below will get set to point to that synthetic event.
	var a11yclick;

	var _AttachMixin = declare("dijit._AttachMixin", null, {
		// summary:
		//		Mixin for widgets to attach to dom nodes and setup events via
		//		convenient data-dojo-attach-point and data-dojo-attach-event DOM attributes.
		//
		//		Superclass of _TemplatedMixin, and can also be used standalone when templates are pre-rendered on the
		//		server.
		//
		//		Does not [yet] handle widgets like ContentPane with this.containerNode set.   It should skip
		//		scanning for data-dojo-attach-point and data-dojo-attach-event inside this.containerNode, but it
		//		doesn't.

/*=====
		// _attachPoints: [private] String[]
		//		List of widget attribute names associated with data-dojo-attach-point=... in the
		//		template, ex: ["containerNode", "labelNode"]
		_attachPoints: [],

		// _attachEvents: [private] Handle[]
		//		List of connections associated with data-dojo-attach-event=... in the
		//		template
		_attachEvents: [],

		// attachScope: [public] Object
		//		Object to which attach points and events will be scoped.  Defaults
		//		to 'this'.
		attachScope: undefined,

		// searchContainerNode: [protected] Boolean
		//		Search descendants of this.containerNode for data-dojo-attach-point and data-dojo-attach-event.
		//		Should generally be left false (the default value) both for performance and to avoid failures when
		//		this.containerNode holds other _AttachMixin instances with their own attach points and events.
 		searchContainerNode: false,
 =====*/

		constructor: function(/*===== params, srcNodeRef =====*/){
			// summary:
			//		Create the widget.
			// params: Object|null
			//		Hash of initialization parameters for widget, including scalar values (like title, duration etc.)
			//		and functions, typically callbacks like onClick.
			//		The hash can contain any of the widget's properties, excluding read-only properties.
			// srcNodeRef: DOMNode|String?
			//		If a srcNodeRef (DOM node) is specified, replace srcNodeRef with my generated DOM tree.

			this._attachPoints = [];
			this._attachEvents = [];
		},


		buildRendering: function(){
			// summary:
			//		Attach to DOM nodes marked with special attributes.
			// tags:
			//		protected

			this.inherited(arguments);

			// recurse through the node, looking for, and attaching to, our
			// attachment points and events, which should be defined on the template node.
			this._attachTemplateNodes(this.domNode);

			this._beforeFillContent();		// hook for _WidgetsInTemplateMixin
		},

		_beforeFillContent: function(){
		},

		_attachTemplateNodes: function(rootNode){
			// summary:
			//		Iterate through the dom nodes and attach functions and nodes accordingly.
			// description:
			//		Map widget properties and functions to the handlers specified in
			//		the dom node and it's descendants. This function iterates over all
			//		nodes and looks for these properties:
			//
			//		- dojoAttachPoint/data-dojo-attach-point
			//		- dojoAttachEvent/data-dojo-attach-event
			// rootNode: DomNode
			//		The node to search for properties. All descendants will be searched.
			// tags:
			//		private

			// DFS to process all nodes except those inside of this.containerNode
			var node = rootNode;
			while(true){
				if(node.nodeType == 1 && (this._processTemplateNode(node, function(n,p){ return n.getAttribute(p); },
						this._attach) || this.searchContainerNode) && node.firstChild){
					node = node.firstChild;
				}else{
					if(node == rootNode){ return; }
					while(!node.nextSibling){
						node = node.parentNode;
						if(node == rootNode){ return; }
					}
					node = node.nextSibling;
				}
			}
		},

		_processTemplateNode: function(/*DOMNode|Widget*/ baseNode, getAttrFunc, attachFunc){
			// summary:
			//		Process data-dojo-attach-point and data-dojo-attach-event for given node or widget.
			//		Returns true if caller should process baseNode's children too.

			var ret = true;

			// Process data-dojo-attach-point
			var _attachScope = this.attachScope || this,
				attachPoint = getAttrFunc(baseNode, "dojoAttachPoint") || getAttrFunc(baseNode, "data-dojo-attach-point");
			if(attachPoint){
				var point, points = attachPoint.split(/\s*,\s*/);
				while((point = points.shift())){
					if(lang.isArray(_attachScope[point])){
						_attachScope[point].push(baseNode);
					}else{
						_attachScope[point] = baseNode;
					}
					ret = (point != "containerNode");
					this._attachPoints.push(point);
				}
			}

			// Process data-dojo-attach-event
			var attachEvent = getAttrFunc(baseNode, "dojoAttachEvent") || getAttrFunc(baseNode, "data-dojo-attach-event");
			if(attachEvent){
				// NOTE: we want to support attributes that have the form
				// "domEvent: nativeEvent, ..."
				var event, events = attachEvent.split(/\s*,\s*/);
				var trim = lang.trim;
				while((event = events.shift())){
					if(event){
						var thisFunc = null;
						if(event.indexOf(":") != -1){
							// oh, if only JS had tuple assignment
							var funcNameArr = event.split(":");
							event = trim(funcNameArr[0]);
							thisFunc = trim(funcNameArr[1]);
						}else{
							event = trim(event);
						}
						if(!thisFunc){
							thisFunc = event;
						}

						this._attachEvents.push(attachFunc(baseNode, event, lang.hitch(_attachScope, thisFunc)));
					}
				}
			}

			return ret;
		},

		_attach: function(node, type, func){
			// summary:
			//		Roughly corresponding to dojo/on, this is the default function for processing a
			//		data-dojo-attach-event.  Meant to attach to DOMNodes, not to widgets.
			// node: DOMNode
			//		The node to setup a listener on.
			// type: String
			//		Event name like "click".
			// getAttrFunc: Function
			//		Function to get the specified property for a given DomNode/Widget.
			// attachFunc: Function?
			//		Attaches an event handler from the specified node/widget to specified function.

			// Map special type names like "mouseenter" to synthetic events.
			// Subclasses are responsible to require() dijit/a11yclick if they want to use it.
			type = type.replace(/^on/, "").toLowerCase();
			if(type == "dijitclick"){
				type = a11yclick || (a11yclick = require("./a11yclick"));
			}else{
				type = synthEvents[type] || type;
			}

			return on(node, type, func);
		},

		_detachTemplateNodes: function() {
			// summary:
			//		Detach and clean up the attachments made in _attachtempalteNodes.

			// Delete all attach points to prevent IE6 memory leaks.
			var _attachScope = this.attachScope || this;
			array.forEach(this._attachPoints, function(point){
				delete _attachScope[point];
			});
			this._attachPoints = [];

			// And same for event handlers
			array.forEach(this._attachEvents, function(handle){ handle.remove(); });
			this._attachEvents = [];
		},

		destroyRendering: function(){
			this._detachTemplateNodes();
			this.inherited(arguments);
		}
	});

	// These arguments can be specified for widgets which are used in templates.
	// Since any widget can be specified as sub widgets in template, mix it
	// into the base widget class.  (This is a hack, but it's effective.).
	// Remove for 2.0.   Also, hide from API doc parser.
	lang.extend(_WidgetBase, /*===== {} || =====*/ {
		dojoAttachEvent: "",
		dojoAttachPoint: ""
	});
	
	return _AttachMixin;
});

},
'dojox/html/metrics':function(){
define(["dojo/_base/kernel","dojo/_base/lang", "dojo/_base/sniff", "dojo/ready", "dojo/_base/unload",
		"dojo/_base/window", "dojo/dom-geometry"],
  function(kernel,lang,has,ready,UnloadUtil,Window,DOMGeom){
	var dhm = lang.getObject("dojox.html.metrics",true);
	var dojox = lang.getObject("dojox");

	//	derived from Morris John's emResized measurer
	dhm.getFontMeasurements = function(){
		// summary:
		//		Returns an object that has pixel equivilents of standard font size values.
		var heights = {
			'1em':0, '1ex':0, '100%':0, '12pt':0, '16px':0, 'xx-small':0, 'x-small':0,
			'small':0, 'medium':0, 'large':0, 'x-large':0, 'xx-large':0
		};
	
		var oldStyle;	
		if(has("ie")){
			//	We do a font-size fix if and only if one isn't applied already.
			// NOTE: If someone set the fontSize on the HTML Element, this will kill it.
			oldStyle = Window.doc.documentElement.style.fontSize || "";
			if(!oldStyle){
				Window.doc.documentElement.style.fontSize="100%";
			}
		}		
	
		//	set up the measuring node.
		var div=Window.doc.createElement("div");
		var ds = div.style;
		ds.position="absolute";
		ds.left="-100px";
		ds.top="0";
		ds.width="30px";
		ds.height="1000em";
		ds.borderWidth="0";
		ds.margin="0";
		ds.padding="0";
		ds.outline="0";
		ds.lineHeight="1";
		ds.overflow="hidden";
		Window.body().appendChild(div);
	
		//	do the measurements.
		for(var p in heights){
			ds.fontSize = p;
			heights[p] = Math.round(div.offsetHeight * 12/16) * 16/12 / 1000;
		}

		if(has("ie")){
			// Restore the font to its old style.
			Window.doc.documentElement.style.fontSize = oldStyle;
		}
		
		Window.body().removeChild(div);
		div = null;
		return heights; 	//	object
	};

	var fontMeasurements = null;
	
	dhm.getCachedFontMeasurements = function(recalculate){
		if(recalculate || !fontMeasurements){
			fontMeasurements = dhm.getFontMeasurements();
		}
		return fontMeasurements;
	};

	var measuringNode = null, empty = {};
	dhm.getTextBox = function(/* String */ text, /* Object */ style, /* String? */ className){
		var m, s;
		if(!measuringNode){
			m = measuringNode = Window.doc.createElement("div");
			// Due to fixing the parent node's width below, texts which contain white-spaces would be wrapped. Avoid this.
			m.style.whiteSpace = "nowrap";
			// Container that we can set constraints on so that it doesn't
			// trigger a scrollbar.
			var c = Window.doc.createElement("div");
			c.appendChild(m);
			s = c.style;
			s.overflow='scroll';
			s.position = "absolute";
			s.left = "0px";
			s.top = "-10000px";
			s.width = "1px";
			s.height = "1px";
			s.visibility = "hidden";
			s.borderWidth = "0";
			s.margin = "0";
			s.padding = "0";
			s.outline = "0";
			Window.body().appendChild(c);
		}else{
			m = measuringNode;
		}
		// reset styles
		m.className = "";
		s = m.style;
		s.borderWidth = "0";
		s.margin = "0";
		s.padding = "0";
		s.outline = "0";
		// set new style
		if(arguments.length > 1 && style){
			for(var i in style){
				if(i in empty){ continue; }
				s[i] = style[i];
			}
		}
		// set classes
		if(arguments.length > 2 && className){
			m.className = className;
		}
		// take a measure
		m.innerHTML = text;
		var box = DOMGeom.position(m);
		// position doesn't report right (reports 1, since parent is 1)
		// So we have to look at the scrollWidth to get the real width
		// Height is right.
		box.w = m.parentNode.scrollWidth;
		return box;
	};

	//	determine the scrollbar sizes on load.
	var scroll={ w:16, h:16 };
	dhm.getScrollbar=function(){ return { w:scroll.w, h:scroll.h }; };

	dhm._fontResizeNode = null;

	dhm.initOnFontResize = function(interval){
		var f = dhm._fontResizeNode = Window.doc.createElement("iframe");
		var fs = f.style;
		fs.position = "absolute";
		fs.width = "5em";
		fs.height = "10em";
		fs.top = "-10000px";
		fs.display = "none";
		if(has("ie")){
			f.onreadystatechange = function(){
				if(f.contentWindow.document.readyState == "complete"){
					f.onresize = f.contentWindow.parent[dojox._scopeName].html.metrics._fontresize;
				}
			};
		}else{
			f.onload = function(){
				f.contentWindow.onresize = f.contentWindow.parent[dojox._scopeName].html.metrics._fontresize;
			};
		}
		//The script tag is to work around a known firebug race condition.  See comments in bug #9046
		f.setAttribute("src", "javascript:'<html><head><script>if(\"loadFirebugConsole\" in window){window.loadFirebugConsole();}</script></head><body></body></html>'");
		Window.body().appendChild(f);
		dhm.initOnFontResize = function(){};
	};

	dhm.onFontResize = function(){};
	dhm._fontresize = function(){
		dhm.onFontResize();
	};

	UnloadUtil.addOnUnload(function(){
		// destroy our font resize iframe if we have one
		var f = dhm._fontResizeNode;
		if(f){
			if(has("ie") && f.onresize){
				f.onresize = null;
			}else if(f.contentWindow && f.contentWindow.onresize){
				f.contentWindow.onresize = null;
			}
			dhm._fontResizeNode = null;
		}
	});

	ready(function(){
		// getScrollbar metrics node
		try{
			var n=Window.doc.createElement("div");
			n.style.cssText = "top:0;left:0;width:100px;height:100px;overflow:scroll;position:absolute;visibility:hidden;";
			Window.body().appendChild(n);
			scroll.w = n.offsetWidth - n.clientWidth;
			scroll.h = n.offsetHeight - n.clientHeight;
			Window.body().removeChild(n);
			//console.log("Scroll bar dimensions: ", scroll);
			delete n;
		}catch(e){}

		// text size poll setup
		if("fontSizeWatch" in kernel.config && !!kernel.config.fontSizeWatch){
			dhm.initOnFontResize();
		}
	});
	return dhm;
});

},
'dojox/grid/_Builder':function(){
define([
	"../main",
	"dojo/_base/array",
	"dojo/_base/lang",
	"dojo/_base/window",
	"dojo/_base/event",
	"dojo/_base/sniff",
	"dojo/_base/connect",
	"dojo/dnd/Moveable",
	"dojox/html/metrics",
	"./util",
	"dojo/_base/html",
	"dojo/dom-geometry"
], function(dojox, array, lang, win, event, has, connect, Moveable, metrics, util, html, domGeometry){

	var dg = dojox.grid;

	var getTdIndex = function(td){
		return td.cellIndex >=0 ? td.cellIndex : array.indexOf(td.parentNode.cells, td);
	};
	
	var getTrIndex = function(tr){
		return tr.rowIndex >=0 ? tr.rowIndex : array.indexOf(tr.parentNode.childNodes, tr);
	};
	
	var getTr = function(rowOwner, index){
		return rowOwner && ((rowOwner.rows||0)[index] || rowOwner.childNodes[index]);
	};

	var findTable = function(node){
		for(var n=node; n && n.tagName!='TABLE'; n=n.parentNode){}
		return n;
	};
	
	var ascendDom = function(inNode, inWhile){
		for(var n=inNode; n && inWhile(n); n=n.parentNode){}
		return n;
	};
	
	var makeNotTagName = function(inTagName){
		var name = inTagName.toUpperCase();
		return function(node){ return node.tagName != name; };
	};

	var rowIndexTag = util.rowIndexTag;
	var gridViewTag = util.gridViewTag;

	// base class for generating markup for the views
	var _Builder = dg._Builder = lang.extend(function(view){
		if(view){
			this.view = view;
			this.grid = view.grid;
		}
	},{
		view: null,
		// boilerplate HTML
		_table: '<table class="dojoxGridRowTable" border="0" cellspacing="0" cellpadding="0" role="presentation"',

		// Returns the table variable as an array - and with the view width, if specified
		getTableArray: function(){
			var html = [this._table];
			if(this.view.viewWidth){
				html.push([' style="width:', this.view.viewWidth, ';"'].join(''));
			}
			html.push('>');
			return html;
		},
		
		// generate starting tags for a cell
		generateCellMarkup: function(inCell, inMoreStyles, inMoreClasses, isHeader){
			var result = [], html;
			if(isHeader){
				var sortInfo = inCell.index != inCell.grid.getSortIndex() ? "" : inCell.grid.sortInfo > 0 ? 'aria-sort="ascending"' : 'aria-sort="descending"';
				if (!inCell.id){
					inCell.id = this.grid.id + "Hdr" + inCell.index;
				}
				// column headers are not editable, mark as aria-readonly=true
				html = ['<th tabIndex="-1" aria-readonly="true" role="columnheader"', sortInfo, ' id="', inCell.id, '"'];
			}else{
				// cells inherit grid aria-readonly property; default value for aria-readonly is false(grid is editable)
				// if grid is editable (had any editable cells), mark non editable cells as aria-readonly=true
				// if no editable cells, grid's aria-readonly value will have been set to true and cells will inherit
				var editInfo = this.grid.editable && !inCell.editable ? 'aria-readonly="true"' : "";
				html = ['<td tabIndex="-1" role="gridcell"', editInfo];
			}
			if(inCell.colSpan){
				html.push(' colspan="', inCell.colSpan, '"');
			}
			if(inCell.rowSpan){
				html.push(' rowspan="', inCell.rowSpan, '"');
			}
			html.push(' class="dojoxGridCell ');
			if(inCell.classes){
				html.push(inCell.classes, ' ');
			}
			if(inMoreClasses){
				html.push(inMoreClasses, ' ');
			}
			// result[0] => td opener, style
			result.push(html.join(''));
			// SLOT: result[1] => td classes
			result.push('');
			html = ['" idx="', inCell.index, '" style="'];
			if(inMoreStyles && inMoreStyles[inMoreStyles.length-1] != ';'){
				inMoreStyles += ';';
			}
			html.push(inCell.styles, inMoreStyles||'', inCell.hidden?'display:none;':'');
			if(inCell.unitWidth){
				html.push('width:', inCell.unitWidth, ';');
			}
			// result[2] => markup
			result.push(html.join(''));
			// SLOT: result[3] => td style
			result.push('');
			html = [ '"' ];
			if(inCell.attrs){
				html.push(" ", inCell.attrs);
			}
			html.push('>');
			// result[4] => td postfix
			result.push(html.join(''));
			// SLOT: result[5] => content
			result.push('');
			// result[6] => td closes
			result.push(isHeader?'</th>':'</td>');
			return result; // Array
		},

		// cell finding
		isCellNode: function(inNode){
			return Boolean(inNode && inNode!=win.doc && html.attr(inNode, "idx"));
		},
		
		getCellNodeIndex: function(inCellNode){
			return inCellNode ? Number(html.attr(inCellNode, "idx")) : -1;
		},
		
		getCellNode: function(inRowNode, inCellIndex){
			for(var i=0, row; ((row = getTr(inRowNode.firstChild, i)) && row.cells); i++){
				for(var j=0, cell; (cell = row.cells[j]); j++){
					if(this.getCellNodeIndex(cell) == inCellIndex){
						return cell;
					}
				}
			}
			return null;
		},
		
		findCellTarget: function(inSourceNode, inTopNode){
			var n = inSourceNode;
			while(n && (!this.isCellNode(n) || (n.offsetParent && gridViewTag in n.offsetParent.parentNode && n.offsetParent.parentNode[gridViewTag] != this.view.id)) && (n!=inTopNode)){
				n = n.parentNode;
			}
			return n!=inTopNode ? n : null;
		},
		
		// event decoration
		baseDecorateEvent: function(e){
			e.dispatch = 'do' + e.type;
			e.grid = this.grid;
			e.sourceView = this.view;
			e.cellNode = this.findCellTarget(e.target, e.rowNode);
			e.cellIndex = this.getCellNodeIndex(e.cellNode);
			e.cell = (e.cellIndex >= 0 ? this.grid.getCell(e.cellIndex) : null);
		},
		
		// event dispatch
		findTarget: function(inSource, inTag){
			var n = inSource;
			while(n && (n!=this.domNode) && (!(inTag in n) || (gridViewTag in n && n[gridViewTag] != this.view.id))){
				n = n.parentNode;
			}
			return (n != this.domNode) ? n : null;
		},

		findRowTarget: function(inSource){
			return this.findTarget(inSource, rowIndexTag);
		},

		isIntraNodeEvent: function(e){
			try{
				return (e.cellNode && e.relatedTarget && html.isDescendant(e.relatedTarget, e.cellNode));
			}catch(x){
				// e.relatedTarget has permission problem in FF if it's an input: https://bugzilla.mozilla.org/show_bug.cgi?id=208427
				return false;
			}
		},

		isIntraRowEvent: function(e){
			try{
				var row = e.relatedTarget && this.findRowTarget(e.relatedTarget);
				return !row && (e.rowIndex==-1) || row && (e.rowIndex==row.gridRowIndex);
			}catch(x){
				// e.relatedTarget on INPUT has permission problem in FF: https://bugzilla.mozilla.org/show_bug.cgi?id=208427
				return false;
			}
		},

		dispatchEvent: function(e){
			if(e.dispatch in this){
				return this[e.dispatch](e);
			}
			return false;
		},

		// dispatched event handlers
		domouseover: function(e){
			if(e.cellNode && (e.cellNode!=this.lastOverCellNode)){
				this.lastOverCellNode = e.cellNode;
				this.grid.onMouseOver(e);
			}
			this.grid.onMouseOverRow(e);
		},

		domouseout: function(e){
			if(e.cellNode && (e.cellNode==this.lastOverCellNode) && !this.isIntraNodeEvent(e, this.lastOverCellNode)){
				this.lastOverCellNode = null;
				this.grid.onMouseOut(e);
				if(!this.isIntraRowEvent(e)){
					this.grid.onMouseOutRow(e);
				}
			}
		},
		
		domousedown: function(e){
			if (e.cellNode)
				this.grid.onMouseDown(e);
			this.grid.onMouseDownRow(e);
		},

		_getTextDirStyle: function(textDir, inCell, inRowIndex){
			// summary:
			//		 Get BiDi text dir, just a placeholder, defined in dojox/grid/bidi/_BidiMixin
			return "";
		}
	});

	// Produces html for grid data content. Owned by grid and used internally
	// for rendering data. Override to implement custom rendering.
	var _ContentBuilder = dg._ContentBuilder = lang.extend(function(view){
		_Builder.call(this, view);
	},_Builder.prototype,{
		update: function(){
			this.prepareHtml();
		},

		// cache html for rendering data rows
		prepareHtml: function(){
			var defaultGet=this.grid.get, cells=this.view.structure.cells;
			for(var j=0, row; (row=cells[j]); j++){
				for(var i=0, cell; (cell=row[i]); i++){
					cell.get = cell.get || (cell.value == undefined) && defaultGet;
					cell.markup = this.generateCellMarkup(cell, cell.cellStyles, cell.cellClasses, false);
					if (!this.grid.editable && cell.editable){
						this.grid.editable = true;
					}
				}
			}
		},

		// time critical: generate html using cache and data source
		generateHtml: function(inDataIndex, inRowIndex){
			var html = this.getTableArray();
			var v = this.view;
			var cells = v.structure.cells;
			var item = this.grid.getItem(inRowIndex);
			var dir;

			util.fire(this.view, "onBeforeRow", [inRowIndex, cells]);
			for(var j=0, row; (row = cells[j]); j++){
				if(row.hidden || row.header){
					continue;
				}
				html.push(!row.invisible ? '<tr>' : '<tr class="dojoxGridInvisible">');
				for(var i=0, cell, m, cc, cs; (cell=row[i]); i++){
					m = cell.markup;
					cc = cell.customClasses = [];
					cs = cell.customStyles = [];

					// content (format can fill in cc and cs as side-effects)
					m[5] = cell.format(inRowIndex, item);
					// classes
					m[1] = cc.join(' ');
					// styles
					m[3] = cs.join(';');
					dir = cell.textDir || this.grid.textDir;
					if (dir) {
					    m[3] += this._getTextDirStyle(dir, cell, inRowIndex);
					}
					// in-place concat
					html.push.apply(html, m);
				}
				html.push('</tr>');
			}
			html.push('</table>');
			return html.join(''); // String
		},

		decorateEvent: function(e){
			e.rowNode = this.findRowTarget(e.target);
			if(!e.rowNode){return false;}
			e.rowIndex = e.rowNode[rowIndexTag];
			this.baseDecorateEvent(e);
			e.cell = this.grid.getCell(e.cellIndex);
			return true; // Boolean
		}
	});

	// Produces html for grid header content. Owned by grid and used internally
	// for rendering data. Override to implement custom rendering.
	var _HeaderBuilder = dg._HeaderBuilder = lang.extend(function(view){
		this.moveable = null;
		_Builder.call(this, view);
	},_Builder.prototype,{
		_skipBogusClicks: false,
		overResizeWidth: 4,
		minColWidth: 1,
		
		update: function(){
			if(this.tableMap){
				this.tableMap.mapRows(this.view.structure.cells);
			}else{
				this.tableMap = new dg._TableMap(this.view.structure.cells);
			}
		},

		generateHtml: function(inGetValue, inValue){
			var dir, html = this.getTableArray(), cells = this.view.structure.cells;
			
			util.fire(this.view, "onBeforeRow", [-1, cells]);
			for(var j=0, row; (row=cells[j]); j++){
				if(row.hidden){
					continue;
				}
				html.push(!row.invisible ? '<tr>' : '<tr class="dojoxGridInvisible">');
				for(var i=0, cell, markup; (cell=row[i]); i++){
					cell.customClasses = [];
					cell.customStyles = [];
					if(this.view.simpleStructure){
						if(cell.draggable){
							if(cell.headerClasses){
								if(cell.headerClasses.indexOf('dojoDndItem') == -1){
									cell.headerClasses += ' dojoDndItem';
								}
							}else{
								cell.headerClasses = 'dojoDndItem';
							}
						}
						if(cell.attrs){
							if(cell.attrs.indexOf("dndType='gridColumn_") == -1){
								cell.attrs += " dndType='gridColumn_" + this.grid.id + "'";
							}
						}else{
							cell.attrs = "dndType='gridColumn_" + this.grid.id + "'";
						}
					}
					markup = this.generateCellMarkup(cell, cell.headerStyles, cell.headerClasses, true);
					// content
					markup[5] = (inValue != undefined ? inValue : inGetValue(cell));
					// styles
					markup[3] = cell.customStyles.join(';');
					dir = cell.textDir || this.grid.textDir;
					if(dir){
					    markup[3] += this._getTextDirStyle(dir, cell, inValue);
					}
					// classes
					markup[1] = cell.customClasses.join(' '); //(cell.customClasses ? ' ' + cell.customClasses : '');
					html.push(markup.join(''));
				}
				html.push('</tr>');
			}
			html.push('</table>');
			return html.join('');
		},

		// event helpers
		getCellX: function(e){
			var n, x, pos;
			// Calculate starting x position
			n = ascendDom(e.target, makeNotTagName("th"));
			if(n){
				// We have a proper parent node, use that for position
				pos = domGeometry.position(n);
				x = e.clientX - pos.x;
			}else{
				// Fall back to layerX
				x = e.layerX;
			}
			return x;
		},

		// event decoration
		decorateEvent: function(e){
			this.baseDecorateEvent(e);
			e.rowIndex = -1;
			e.cellX = this.getCellX(e);
			return true;
		},

		// event handlers
		// resizing
		prepareResize: function(e, mod){
			do{
				var i = e.cellIndex;
				e.cellNode = (i ? e.cellNode.parentNode.cells[i+mod] : null);
				e.cellIndex = (e.cellNode ? this.getCellNodeIndex(e.cellNode) : -1);
			}while(e.cellNode && e.cellNode.style.display == "none");
			return Boolean(e.cellNode);
		},

		canResize: function(e){
			if(!e.cellNode || e.cellNode.colSpan > 1){
				return false;
			}
			var cell = this.grid.getCell(e.cellIndex);
			return !cell.noresize && cell.canResize();
		},

		overLeftResizeArea: function(e){
			// We are never over a resize area if we are in the process of moving
			if(html.hasClass(win.body(), "dojoDndMove")){
				return false;
			}
			//Bugfix for crazy IE problem (#8807).  IE returns position information for the icon and text arrow divs
			//as if they were still on the left instead of returning the position they were 'float: right' to.
			//So, the resize check ends up checking the wrong adjacent cell.  This checks to see if the hover was over
			//the image or text nodes, then just ignored them/treat them not in scale range.
			if(has('ie')){
				var tN = e.target;
				if(html.hasClass(tN, "dojoxGridArrowButtonNode") ||
					html.hasClass(tN, "dojoxGridArrowButtonChar") ||
					html.hasClass(tN, "dojoxGridColCaption")){
					return false;
				}
			}

			if(this.grid.isLeftToRight()){
				return (e.cellIndex>0) && (e.cellX > 0 && e.cellX < this.overResizeWidth) && this.prepareResize(e, -1);
			}
			var t = e.cellNode && (e.cellX > 0 && e.cellX < this.overResizeWidth);
			return t;
		},

		overRightResizeArea: function(e){
			// We are never over a resize area if we are in the process of moving
			if(html.hasClass(win.body(), "dojoDndMove")){
				return false;
			}
			//Bugfix for crazy IE problem (#8807).  IE returns position information for the icon and text arrow divs
			//as if they were still on the left instead of returning the position they were 'float: right' to.
			//So, the resize check ends up checking the wrong adjacent cell.  This checks to see if the hover was over
			//the image or text nodes, then just ignored them/treat them not in scale range.
			if(has('ie')){
				var tN = e.target;
				if(html.hasClass(tN, "dojoxGridArrowButtonNode") ||
					html.hasClass(tN, "dojoxGridArrowButtonChar") ||
					html.hasClass(tN, "dojoxGridColCaption")){
					return false;
				}
			}

			if(this.grid.isLeftToRight()){
				return e.cellNode && (e.cellX >= e.cellNode.offsetWidth - this.overResizeWidth);
			}
			return (e.cellIndex>0) && (e.cellX >= e.cellNode.offsetWidth - this.overResizeWidth) && this.prepareResize(e, -1);
		},

		domousemove: function(e){
			//console.log(e.cellIndex, e.cellX, e.cellNode.offsetWidth);
			if(!this.moveable){
				var c = (this.overRightResizeArea(e) ? 'dojoxGridColResize' : (this.overLeftResizeArea(e) ? 'dojoxGridColResize' : ''));
				if(c && !this.canResize(e)){
					c = 'dojoxGridColNoResize';
				}
				html.toggleClass(e.sourceView.headerNode, "dojoxGridColNoResize", (c == "dojoxGridColNoResize"));
				html.toggleClass(e.sourceView.headerNode, "dojoxGridColResize", (c == "dojoxGridColResize"));
				if(c){
					event.stop(e);
				}
			}
		},

		domousedown: function(e){
			if(!this.moveable){
				if((this.overRightResizeArea(e) || this.overLeftResizeArea(e)) && this.canResize(e)){
					this.beginColumnResize(e);
				}else{
					this.grid.onMouseDown(e);
					this.grid.onMouseOverRow(e);
				}
				//else{
				//	this.beginMoveColumn(e);
				//}
			}
		},

		doclick: function(e) {
			if(this._skipBogusClicks){
				event.stop(e);
				return true;
			}
			return false;
		},

		// column resizing
		colResizeSetup: function(/*Event Object*/e, /*boolean*/ isMouse ){
			//Set up the drag object for column resizing
			// Called with mouse event in case of drag and drop,
			// Also called from keyboard shift-arrow event when focus is on a header
			var headContentBox = html.contentBox(e.sourceView.headerNode);
			
			if(isMouse){  //IE draws line even with no mouse down so separate from keyboard
				this.lineDiv = document.createElement('div');

				var vw = html.position(e.sourceView.headerNode, true);
				var bodyContentBox = html.contentBox(e.sourceView.domNode);
				//fix #11340
				var l = e.pageX;
				if(!this.grid.isLeftToRight() && has('ie') < 8){
					l -= metrics.getScrollbar().w;
				}
				html.style(this.lineDiv, {
					top: vw.y + "px",
					left: l + "px",
					height: (bodyContentBox.h + headContentBox.h) + "px"
				});
				html.addClass(this.lineDiv, "dojoxGridResizeColLine");
				this.lineDiv._origLeft = l;
				win.body().appendChild(this.lineDiv);
			}
			var spanners = [], nodes = this.tableMap.findOverlappingNodes(e.cellNode);
			for(var i=0, cell; (cell=nodes[i]); i++){
				spanners.push({ node: cell, index: this.getCellNodeIndex(cell), width: cell.offsetWidth });
				//console.log("spanner: " + this.getCellNodeIndex(cell));
			}

			var view = e.sourceView;
			var adj = this.grid.isLeftToRight() ? 1 : -1;
			var views = e.grid.views.views;
			var followers = [];
			for(var j=view.idx+adj, cView; (cView=views[j]); j=j+adj){
				followers.push({ node: cView.headerNode, left: window.parseInt(cView.headerNode.style.left) });
			}
			var table = view.headerContentNode.firstChild;
			var drag = {
				scrollLeft: e.sourceView.headerNode.scrollLeft,
				view: view,
				node: e.cellNode,
				index: e.cellIndex,
				w: html.contentBox(e.cellNode).w,
				vw: headContentBox.w,
				table: table,
				tw: html.contentBox(table).w,
				spanners: spanners,
				followers: followers
			};
			return drag;
		},
		beginColumnResize: function(e){
			this.moverDiv = document.createElement("div");
			html.style(this.moverDiv,{position: "absolute", left:0}); // to make DnD work with dir=rtl
			win.body().appendChild(this.moverDiv);
			html.addClass(this.grid.domNode, "dojoxGridColumnResizing");
			var m = (this.moveable = new Moveable(this.moverDiv));

			var drag = this.colResizeSetup(e,true);

			m.onMove = lang.hitch(this, "doResizeColumn", drag);

			connect.connect(m, "onMoveStop", lang.hitch(this, function(){
				this.endResizeColumn(drag);
				if(drag.node.releaseCapture){
					drag.node.releaseCapture();
				}
				this.moveable.destroy();
				delete this.moveable;
				this.moveable = null;
				html.removeClass(this.grid.domNode, "dojoxGridColumnResizing");
			}));

			if(e.cellNode.setCapture){
				e.cellNode.setCapture();
			}
			m.onMouseDown(e);
		},

		doResizeColumn: function(inDrag, mover, leftTop){
			var changeX = leftTop.l;
			var data = {
				deltaX: changeX,
				w: inDrag.w + (this.grid.isLeftToRight() ? changeX : -changeX),//fix #11341
				vw: inDrag.vw + changeX,
				tw: inDrag.tw + changeX
			};
			
			this.dragRecord = {inDrag: inDrag, mover: mover, leftTop:leftTop};
			
			if(data.w >= this.minColWidth){
				if (!mover) { // we are using keyboard do immediate resize
					this.doResizeNow(inDrag, data);
				}
				else{
					html.style(this.lineDiv, "left", (this.lineDiv._origLeft + data.deltaX) + "px");
				}
			}
		},

		endResizeColumn: function(inDrag){
			if(this.dragRecord){
				var leftTop = this.dragRecord.leftTop;
				var changeX = this.grid.isLeftToRight() ? leftTop.l : -leftTop.l;
				// Make sure we are not under our minimum
				// http://bugs.dojotoolkit.org/ticket/9390
				changeX += Math.max(inDrag.w + changeX, this.minColWidth) - (inDrag.w + changeX);
				if(has('webkit') && inDrag.spanners.length){
					// Webkit needs the pad border extents back in
					changeX += html._getPadBorderExtents(inDrag.spanners[0].node).w;
				}
				var data = {
					deltaX: changeX,
					w: inDrag.w + changeX,
					vw: inDrag.vw + changeX,
					tw: inDrag.tw + changeX
				};
				// Only resize the columns when the drag has finished
				this.doResizeNow(inDrag, data);
				delete this.dragRecord;
			}
			
			html.destroy(this.lineDiv);
 			html.destroy(this.moverDiv);
			html.destroy(this.moverDiv);
			delete this.moverDiv;
			this._skipBogusClicks = true;
			inDrag.view.update();
			this._skipBogusClicks = false;
			this.grid.onResizeColumn(inDrag.index);
		},
		doResizeNow: function(inDrag, data){
			inDrag.view.convertColPctToFixed();
			if(inDrag.view.flexCells && !inDrag.view.testFlexCells()){
				var t = findTable(inDrag.node);
				if(t){
					(t.style.width = '');
				}
			}
			var i, s, sw, f, fl;
			for(i=0; (s=inDrag.spanners[i]); i++){
				sw = s.width + data.deltaX;
				if(sw > 0){
					s.node.style.width = sw + 'px';
					inDrag.view.setColWidth(s.index, sw);
				}
			}
			if(this.grid.isLeftToRight() || !has('ie')){//fix #11339
				for(i=0; (f=inDrag.followers[i]); i++){
					fl = f.left + data.deltaX;
					f.node.style.left = fl + 'px';
				}
			}
			inDrag.node.style.width = data.w + 'px';
			inDrag.view.setColWidth(inDrag.index, data.w);
			inDrag.view.headerNode.style.width = data.vw + 'px';
			inDrag.view.setColumnsWidth(data.tw);
			if(!this.grid.isLeftToRight()){
				inDrag.view.headerNode.scrollLeft = inDrag.scrollLeft + data.deltaX;
			}
		}
	});

	// Maps an html table into a structure parsable for information about cell row and col spanning.
	// Used by HeaderBuilder.
	dg._TableMap = lang.extend(function(rows){
		this.mapRows(rows);
	},{
		map: null,

		mapRows: function(inRows){
			// summary:
			//		Map table topography

			//console.log('mapRows');
			// # of rows
			var rowCount = inRows.length;
			if(!rowCount){
				return;
			}
			// map which columns and rows fill which cells
			this.map = [];
			var row;
			for(var k=0; (row=inRows[k]); k++){
				this.map[k] = [];
			}
			for(var j=0; (row=inRows[j]); j++){
				for(var i=0, x=0, cell, colSpan, rowSpan; (cell=row[i]); i++){
					while(this.map[j][x]){x++;}
					this.map[j][x] = { c: i, r: j };
					rowSpan = cell.rowSpan || 1;
					colSpan = cell.colSpan || 1;
					for(var y=0; y<rowSpan; y++){
						for(var s=0; s<colSpan; s++){
							this.map[j+y][x+s] = this.map[j][x];
						}
					}
					x += colSpan;
				}
			}
			//this.dumMap();
		},

		dumpMap: function(){
			for(var j=0, row, h=''; (row=this.map[j]); j++,h=''){
				for(var i=0, cell; (cell=row[i]); i++){
					h += cell.r + ',' + cell.c + '   ';
				}
			}
		},

		getMapCoords: function(inRow, inCol){
			// summary:
			//		Find node's map coords by it's structure coords
			for(var j=0, row; (row=this.map[j]); j++){
				for(var i=0, cell; (cell=row[i]); i++){
					if(cell.c==inCol && cell.r == inRow){
						return { j: j, i: i };
					}
					//else{console.log(inRow, inCol, ' : ', i, j, " : ", cell.r, cell.c); };
				}
			}
			return { j: -1, i: -1 };
		},
		
		getNode: function(inTable, inRow, inCol){
			// summary:
			//		Find a node in inNode's table with the given structure coords
			var row = inTable && inTable.rows[inRow];
			return row && row.cells[inCol];
		},
		
		_findOverlappingNodes: function(inTable, inRow, inCol){
			var nodes = [];
			var m = this.getMapCoords(inRow, inCol);
			//console.log("node j: %d, i: %d", m.j, m.i);
			for(var j=0, row; (row=this.map[j]); j++){
				if(j == m.j){ continue; }
				var rw = row[m.i];
				//console.log("overlaps: r: %d, c: %d", rw.r, rw.c);
				var n = (rw?this.getNode(inTable, rw.r, rw.c):null);
				if(n){ nodes.push(n); }
			}
			//console.log(nodes);
			return nodes;
		},
		
		findOverlappingNodes: function(inNode){
			return this._findOverlappingNodes(findTable(inNode), getTrIndex(inNode.parentNode), getTdIndex(inNode));
		}
	});

	return {
		_Builder: _Builder,
		_HeaderBuilder: _HeaderBuilder,
		_ContentBuilder: _ContentBuilder
	};
});

},
'dojo/dnd/Moveable':function(){
define([
	"../_base/array", "../_base/declare", "../_base/lang", "../dom", "../dom-class", "../Evented",
	"../has", "../on", "../topic", "../touch", "./common", "./Mover", "../_base/window"
], function(array, declare, lang, dom, domClass, Evented, has, on, topic, touch, dnd, Mover, win){

// module:
//		dojo/dnd/Moveable

var touchActionPropertyName;
var setTouchAction = function () {};

function setTouchActionPropertyName() {
	if ("touchAction" in document.body.style) {
		touchActionPropertyName = "touchAction";
	}
	else if ("msTouchAction" in document.body.style) {
		touchActionPropertyName = "msTouchAction";
	}
	setTouchAction = function setTouchAction(/* Node */ node, /* string */ action) {
		node.style[touchActionPropertyName] = action;
	}
	setTouchAction(arguments[0], arguments[1]);
}

if (has("touch-action")) {
	// Ensure that the logic to determine "touchActionPropertyName" runs
	setTouchAction = setTouchActionPropertyName;
}

var Moveable = declare("dojo.dnd.Moveable", [Evented], {
	// summary:
	//		an object, which makes a node movable

	// object attributes (for markup)
	handle: "",
	delay: 0,
	skip: false,

	constructor: function(node, params){
		// node: Node
		//		a node (or node's id) to be moved
		// params: Moveable.__MoveableArgs?
		//		optional parameters
		this.node = dom.byId(node);
		setTouchAction(this.node, "none");
		if(!params){ params = {}; }
		this.handle = params.handle ? dom.byId(params.handle) : null;
		if(!this.handle){ this.handle = this.node; }
		this.delay = params.delay > 0 ? params.delay : 0;
		this.skip  = params.skip;
		this.mover = params.mover ? params.mover : Mover;
		this.events = [
			on(this.handle, touch.press, lang.hitch(this, "onMouseDown")),
			// cancel text selection and text dragging
			on(this.handle, "dragstart",   lang.hitch(this, "onSelectStart")),
			on(this.handle, "selectstart",   lang.hitch(this, "onSelectStart"))
		];
	},

	// markup methods
	markupFactory: function(params, node, Ctor){
		return new Ctor(node, params);
	},

	// methods
	destroy: function(){
		// summary:
		//		stops watching for possible move, deletes all references, so the object can be garbage-collected
		array.forEach(this.events, function(handle){ handle.remove(); });
		setTouchAction(this.node, "");
		this.events = this.node = this.handle = null;
	},

	// mouse event processors
	onMouseDown: function(e){
		// summary:
		//		event processor for onmousedown/ontouchstart, creates a Mover for the node
		// e: Event
		//		mouse/touch event
		if(this.skip && dnd.isFormElement(e)){ return; }
		if(this.delay){
			this.events.push(
				on(this.handle, touch.move, lang.hitch(this, "onMouseMove")),
				on(this.handle.ownerDocument, touch.release, lang.hitch(this, "onMouseUp"))
			);
			this._lastX = e.pageX;
			this._lastY = e.pageY;
		}else{
			this.onDragDetected(e);
		}
		e.stopPropagation();
		e.preventDefault();
	},
	onMouseMove: function(e){
		// summary:
		//		event processor for onmousemove/ontouchmove, used only for delayed drags
		// e: Event
		//		mouse/touch event
		if(Math.abs(e.pageX - this._lastX) > this.delay || Math.abs(e.pageY - this._lastY) > this.delay){
			this.onMouseUp(e);
			this.onDragDetected(e);
		}
		e.stopPropagation();
		e.preventDefault();
	},
	onMouseUp: function(e){
		// summary:
		//		event processor for onmouseup, used only for delayed drags
		// e: Event
		//		mouse event
		for(var i = 0; i < 2; ++i){
			this.events.pop().remove();
		}
		e.stopPropagation();
		e.preventDefault();
	},
	onSelectStart: function(e){
		// summary:
		//		event processor for onselectevent and ondragevent
		// e: Event
		//		mouse event
		if(!this.skip || !dnd.isFormElement(e)){
			e.stopPropagation();
			e.preventDefault();
		}
	},

	// local events
	onDragDetected: function(/*Event*/ e){
		// summary:
		//		called when the drag is detected;
		//		responsible for creation of the mover
		new this.mover(this.node, e, this);
	},
	onMoveStart: function(/*Mover*/ mover){
		// summary:
		//		called before every move operation
		topic.publish("/dnd/move/start", mover);
		domClass.add(win.body(), "dojoMove");
		domClass.add(this.node, "dojoMoveItem");
	},
	onMoveStop: function(/*Mover*/ mover){
		// summary:
		//		called after every move operation
		topic.publish("/dnd/move/stop", mover);
		domClass.remove(win.body(), "dojoMove");
		domClass.remove(this.node, "dojoMoveItem");
	},
	onFirstMove: function(/*===== mover, e =====*/){
		// summary:
		//		called during the very first move notification;
		//		can be used to initialize coordinates, can be overwritten.
		// mover: Mover
		// e: Event

		// default implementation does nothing
	},
	onMove: function(mover, leftTop /*=====, e =====*/){
		// summary:
		//		called during every move notification;
		//		should actually move the node; can be overwritten.
		// mover: Mover
		// leftTop: Object
		// e: Event
		this.onMoving(mover, leftTop);
		var s = mover.node.style;
		s.left = leftTop.l + "px";
		s.top  = leftTop.t + "px";
		this.onMoved(mover, leftTop);
	},
	onMoving: function(/*===== mover, leftTop =====*/){
		// summary:
		//		called before every incremental move; can be overwritten.
		// mover: Mover
		// leftTop: Object

		// default implementation does nothing
	},
	onMoved: function(/*===== mover, leftTop =====*/){
		// summary:
		//		called after every incremental move; can be overwritten.
		// mover: Mover
		// leftTop: Object

		// default implementation does nothing
	}
});

/*=====
Moveable.__MoveableArgs = declare([], {
	// handle: Node||String
	//		A node (or node's id), which is used as a mouse handle.
	//		If omitted, the node itself is used as a handle.
	handle: null,

	// delay: Number
	//		delay move by this number of pixels
	delay: 0,

	// skip: Boolean
	//		skip move of form elements
	skip: false,

	// mover: Object
	//		a constructor of custom Mover
	mover: dnd.Mover
});
=====*/

return Moveable;
});

},
'dojo/dnd/Mover':function(){
define([
	"../_base/array", "../_base/declare", "../_base/lang", "../sniff", "../_base/window",
	"../dom", "../dom-geometry", "../dom-style", "../Evented", "../on", "../touch", "./common", "./autoscroll"
], function(array, declare, lang, has, win, dom, domGeom, domStyle, Evented, on, touch, dnd, autoscroll){

// module:
//		dojo/dnd/Mover

return declare("dojo.dnd.Mover", [Evented], {
	// summary:
	//		an object which makes a node follow the mouse, or touch-drag on touch devices.
	//		Used as a default mover, and as a base class for custom movers.

	constructor: function(node, e, host){
		// node: Node
		//		a node (or node's id) to be moved
		// e: Event
		//		a mouse event, which started the move;
		//		only pageX and pageY properties are used
		// host: Object?
		//		object which implements the functionality of the move,
		//	 	and defines proper events (onMoveStart and onMoveStop)
		this.node = dom.byId(node);
		this.marginBox = {l: e.pageX, t: e.pageY};
		this.mouseButton = e.button;
		var h = (this.host = host), d = node.ownerDocument;

		function stopEvent(e){
			e.preventDefault();
			e.stopPropagation();
		}

		this.events = [
			// At the start of a drag, onFirstMove is called, and then the following
			// listener is disconnected.
			on(d, touch.move, lang.hitch(this, "onFirstMove")),

			// These are called continually during the drag
			on(d, touch.move, lang.hitch(this, "onMouseMove")),

			// And these are called at the end of the drag
			on(d, touch.release,  lang.hitch(this, "onMouseUp")),

			// cancel text selection and text dragging
			on(d, "dragstart",   stopEvent),
			on(d.body, "selectstart", stopEvent)
		];

		// Tell autoscroll that a drag is starting
		autoscroll.autoScrollStart(d);

		// notify that the move has started
		if(h && h.onMoveStart){
			h.onMoveStart(this);
		}
	},
	// mouse event processors
	onMouseMove: function(e){
		// summary:
		//		event processor for onmousemove/ontouchmove
		// e: Event
		//		mouse/touch event
		autoscroll.autoScroll(e);
		var m = this.marginBox;
		this.host.onMove(this, {l: m.l + e.pageX, t: m.t + e.pageY}, e);
		e.preventDefault();
		e.stopPropagation();
	},
	onMouseUp: function(e){
		if(has("webkit") && has("mac") && this.mouseButton == 2 ?
				e.button == 0 : this.mouseButton == e.button){ // TODO Should condition be met for touch devices, too?
			this.destroy();
		}
		e.preventDefault();
		e.stopPropagation();
	},
	// utilities
	onFirstMove: function(e){
		// summary:
		//		makes the node absolute; it is meant to be called only once.
		//		relative and absolutely positioned nodes are assumed to use pixel units
		var s = this.node.style, l, t, h = this.host;
		switch(s.position){
			case "relative":
			case "absolute":
				// assume that left and top values are in pixels already
				l = Math.round(parseFloat(s.left)) || 0;
				t = Math.round(parseFloat(s.top)) || 0;
				break;
			default:
				s.position = "absolute";	// enforcing the absolute mode
				var m = domGeom.getMarginBox(this.node);
				// event.pageX/pageY (which we used to generate the initial
				// margin box) includes padding and margin set on the body.
				// However, setting the node's position to absolute and then
				// doing domGeom.marginBox on it *doesn't* take that additional
				// space into account - so we need to subtract the combined
				// padding and margin.  We use getComputedStyle and
				// _getMarginBox/_getContentBox to avoid the extra lookup of
				// the computed style.
				var b = win.doc.body;
				var bs = domStyle.getComputedStyle(b);
				var bm = domGeom.getMarginBox(b, bs);
				var bc = domGeom.getContentBox(b, bs);
				l = m.l - (bc.l - bm.l);
				t = m.t - (bc.t - bm.t);
				break;
		}
		this.marginBox.l = l - this.marginBox.l;
		this.marginBox.t = t - this.marginBox.t;
		if(h && h.onFirstMove){
			h.onFirstMove(this, e);
		}

		// Disconnect touch.move that call this function
		this.events.shift().remove();
	},
	destroy: function(){
		// summary:
		//		stops the move, deletes all references, so the object can be garbage-collected
		array.forEach(this.events, function(handle){ handle.remove(); });
		// undo global settings
		var h = this.host;
		if(h && h.onMoveStop){
			h.onMoveStop(this);
		}
		// destroy objects
		this.events = this.node = this.host = null;
	}
});

});

},
'dojox/grid/_ViewManager':function(){
define([
	"dojo/_base/declare",
	"dojo/_base/sniff",
	"dojo/dom-class"
], function(declare, has, domClass){

return declare('dojox.grid._ViewManager', null, {
	// summary:
	//		A collection of grid views. Owned by grid and used internally for managing grid views.
	// description:
	//		Grid creates views automatically based on grid's layout structure.
	//		Users should typically not need to access individual views or the views collection directly.
	constructor: function(inGrid){
		this.grid = inGrid;
	},

	defaultWidth: 200,

	views: [],

	// operations
	resize: function(){
		this.onEach("resize");
	},

	render: function(){
		this.onEach("render");
	},

	// views
	addView: function(inView){
		inView.idx = this.views.length;
		this.views.push(inView);
	},

	destroyViews: function(){
		for(var i=0, v; v=this.views[i]; i++){
			v.destroy();
		}
		this.views = [];
	},

	getContentNodes: function(){
		var nodes = [];
		for(var i=0, v; v=this.views[i]; i++){
			nodes.push(v.contentNode);
		}
		return nodes;
	},

	forEach: function(inCallback){
		for(var i=0, v; v=this.views[i]; i++){
			inCallback(v, i);
		}
	},

	onEach: function(inMethod, inArgs){
		inArgs = inArgs || [];
		for(var i=0, v; v=this.views[i]; i++){
			if(inMethod in v){
				v[inMethod].apply(v, inArgs);
			}
		}
	},

	// layout
	normalizeHeaderNodeHeight: function(){
		var rowNodes = [];
		for(var i=0, v; (v=this.views[i]); i++){
			if(v.headerContentNode.firstChild){
				rowNodes.push(v.headerContentNode);
			}
		}
		this.normalizeRowNodeHeights(rowNodes);
	},

	normalizeRowNodeHeights: function(inRowNodes){
		var h = 0;
		var currHeights = [];
		if(this.grid.rowHeight){
			h = this.grid.rowHeight;
		}else{
			if(inRowNodes.length <= 1){
				// no need to normalize if we are the only one...
				return;
			}
			for(var i=0, n; (n=inRowNodes[i]); i++){
				// We only care about the height - so don't use marginBox.  This
				// depends on the container not having any margin (which it shouldn't)
				// Also - we only look up the height if the cell doesn't have the
				// dojoxGridNonNormalizedCell class (like for row selectors)
				if(!domClass.contains(n, "dojoxGridNonNormalizedCell")){
					currHeights[i] = n.firstChild.offsetHeight;
					h =  Math.max(h, currHeights[i]);
				}
			}
			h = (h >= 0 ? h : 0);
	
			//Work around odd FF3 rendering bug: #8864.
			//A one px increase fixes FireFox 3's rounding bug for fractional font sizes.
			if((has('mozilla') || has('ie') > 8 ) && h){h++;}
		}
		for(i=0; (n=inRowNodes[i]); i++){
			if(currHeights[i] != h){
				n.firstChild.style.height = h + "px";
			}
		}
	},
	
	resetHeaderNodeHeight: function(){
		for(var i=0, v, n; (v=this.views[i]); i++){
			n = v.headerContentNode.firstChild;
			if(n){
				n.style.height = "";
			}
		}
	},

	renormalizeRow: function(inRowIndex){
		var rowNodes = [];
		for(var i=0, v, n; (v=this.views[i])&&(n=v.getRowNode(inRowIndex)); i++){
			n.firstChild.style.height = '';
			rowNodes.push(n);
		}
		this.normalizeRowNodeHeights(rowNodes);
	},

	getViewWidth: function(inIndex){
		return this.views[inIndex].getWidth() || this.defaultWidth;
	},

	// must be called after view widths are properly set or height can be miscalculated
	// if there are flex columns
	measureHeader: function(){
		// need to reset view header heights so they are properly measured.
		this.resetHeaderNodeHeight();
		this.forEach(function(inView){
			inView.headerContentNode.style.height = '';
		});
		var h = 0;
		// calculate maximum view header height
		this.forEach(function(inView){
			h = Math.max(inView.headerNode.offsetHeight, h);
		});
		return h;
	},

	measureContent: function(){
		var h = 0;
		this.forEach(function(inView){
			h = Math.max(inView.domNode.offsetHeight, h);
		});
		return h;
	},

	findClient: function(inAutoWidth){
		// try to use user defined client
		var c = this.grid.elasticView || -1;
		// attempt to find implicit client
		if(c < 0){
			for(var i=1, v; (v=this.views[i]); i++){
				if(v.viewWidth){
					for(i=1; (v=this.views[i]); i++){
						if(!v.viewWidth){
							c = i;
							break;
						}
					}
					break;
				}
			}
		}
		// client is in the middle by default
		if(c < 0){
			c = Math.floor(this.views.length / 2);
		}
		return c;
	},

	arrange: function(l, w){
		var i, v, vw, len = this.views.length, self = this;
		// find the client
		var c = (w <= 0 ? len : this.findClient());
		// layout views
		var setPosition = function(v, l){
			var ds = v.domNode.style;
			var hs = v.headerNode.style;

			if(!self.grid.isLeftToRight()){
				ds.right = l + 'px';
				// fixed rtl, the scrollbar is on the right side in FF < 4
				if(has('ff') < 4){
					hs.right = l + v.getScrollbarWidth() + 'px';
				}else{
					hs.right = l + 'px';
				}
				if(!has('webkit') && hs.width != 'auto'){
					hs.width = parseInt(hs.width, 10) - v.getScrollbarWidth() + 'px';					
				}
			}else{
				ds.left = l + 'px';
				hs.left = l + 'px';
			}
			ds.top = 0 + 'px';
			hs.top = 0;
		};
		// for views left of the client
		//BiDi TODO: The left and right should not appear in BIDI environment. Should be replaced with
		//leading and tailing concept.
		for(i=0; (v=this.views[i])&&(i<c); i++){
			// get width
			vw = this.getViewWidth(i);
			// process boxes
			v.setSize(vw, 0);
			setPosition(v, l);
			if(v.headerContentNode && v.headerContentNode.firstChild){
				vw = v.getColumnsWidth()+v.getScrollbarWidth();
			}else{
				vw = v.domNode.offsetWidth;
			}
			// update position
			l += vw;
		}
		// next view (is the client, i++ == c)
		i++;
		// start from the right edge
		var r = w;
		// for views right of the client (iterated from the right)
		for(var j=len-1; (v=this.views[j])&&(i<=j); j--){
			// get width
			vw = this.getViewWidth(j);
			// set size
			v.setSize(vw, 0);
			// measure in pixels
			vw = v.domNode.offsetWidth;
			// update position
			r -= vw;
			// set position
			setPosition(v, r);
		}
		if(c<len){
			v = this.views[c];
			// position the client box between left and right boxes
			vw = Math.max(1, r-l);
			// set size
			v.setSize(vw + 'px', 0);
			setPosition(v, l);
		}
		return l;
	},

	// rendering
	renderRow: function(inRowIndex, inNodes, skipRenorm){
		var rowNodes = [];
		for(var i=0, v, n, rowNode; (v=this.views[i])&&(n=inNodes[i]); i++){
			rowNode = v.renderRow(inRowIndex);
			n.appendChild(rowNode);
			rowNodes.push(rowNode);
		}
		if(!skipRenorm){
			this.normalizeRowNodeHeights(rowNodes);
		}
	},
	
	rowRemoved: function(inRowIndex){
		this.onEach("rowRemoved", [ inRowIndex ]);
	},
	
	// updating
	updateRow: function(inRowIndex, skipRenorm){
		for(var i=0, v; v=this.views[i]; i++){
			v.updateRow(inRowIndex);
		}
		if(!skipRenorm){
			this.renormalizeRow(inRowIndex);
		}
	},
	
	updateRowStyles: function(inRowIndex){
		this.onEach("updateRowStyles", [ inRowIndex ]);
	},
	
	// scrolling
	setScrollTop: function(inTop){
		var top = inTop;
		for(var i=0, v; v=this.views[i]; i++){
			top = v.setScrollTop(inTop);
			// Work around IE not firing scroll events that cause header offset
			// issues to occur.
			if(has('ie') && v.headerNode && v.scrollboxNode){
				v.headerNode.scrollLeft = v.scrollboxNode.scrollLeft;
			}
		}
		return top;
		//this.onEach("setScrollTop", [ inTop ]);
	},
	
	getFirstScrollingView: function(){
		// summary:
		//		Returns the first grid view with a scroll bar
		for(var i=0, v; (v=this.views[i]); i++){
			if(v.hasHScrollbar() || v.hasVScrollbar()){
				return v;
			}
		}
		return null;
	}
});
});
},
'dojox/grid/_RowManager':function(){
define([
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/dom-class"
], function(declare, lang, domClass){

	var setStyleText = function(inNode, inStyleText){
		if(inNode.style.cssText == undefined){
			inNode.setAttribute("style", inStyleText);
		}else{
			inNode.style.cssText = inStyleText;
		}
	};

	return declare("dojox.grid._RowManager", null, {
		//	Stores information about grid rows. Owned by grid and used internally.
		constructor: function(inGrid){
			this.grid = inGrid;
		},
		linesToEms: 2,
		overRow: -2,
		// styles
		prepareStylingRow: function(inRowIndex, inRowNode){
			return {
				index: inRowIndex,
				node: inRowNode,
				odd: Boolean(inRowIndex&1),
				selected: !!this.grid.selection.isSelected(inRowIndex),
				over: this.isOver(inRowIndex),
				customStyles: "",
				customClasses: "dojoxGridRow"
			};
		},
		styleRowNode: function(inRowIndex, inRowNode){
			var row = this.prepareStylingRow(inRowIndex, inRowNode);
			this.grid.onStyleRow(row);
			this.applyStyles(row);
		},
		applyStyles: function(inRow){
			var i = inRow;

			i.node.className = i.customClasses;
			var h = i.node.style.height;
			setStyleText(i.node, i.customStyles + ';' + (i.node._style||''));
			i.node.style.height = h;
		},
		updateStyles: function(inRowIndex){
			this.grid.updateRowStyles(inRowIndex);
		},
		// states and events
		setOverRow: function(inRowIndex){
			var last = this.overRow;
			this.overRow = inRowIndex;
			if((last!=this.overRow)&&(lang.isString(last) || last >= 0)){
				this.updateStyles(last);
			}
			this.updateStyles(this.overRow);
		},
		isOver: function(inRowIndex){
			return (this.overRow == inRowIndex && !domClass.contains(this.grid.domNode, "dojoxGridColumnResizing"));
		}
	});
});
},
'dojox/grid/_FocusManager':function(){
define([
	"dojo/_base/array",
	"dojo/_base/lang",
	"dojo/_base/declare",
	"dojo/_base/connect",
	"dojo/_base/event",
	"dojo/_base/sniff",
	"dojo/query",
	"./util",
	"dojo/_base/html"
], function(array, lang, declare, connect, event, has, query, util, html){

// focus management
return declare("dojox.grid._FocusManager", null, {
	// summary:
	//		Controls grid cell focus. Owned by grid and used internally for focusing.
	//		Note: grid cell actually receives keyboard input only when cell is being edited.
	constructor: function(inGrid){
		this.grid = inGrid;
		this.cell = null;
		this.rowIndex = -1;
		this._connects = [];
		this._headerConnects = [];
		this.headerMenu = this.grid.headerMenu;
		this._connects.push(connect.connect(this.grid.domNode, "onfocus", this, "doFocus"));
		this._connects.push(connect.connect(this.grid.domNode, "onblur", this, "doBlur"));
		this._connects.push(connect.connect(this.grid.domNode, "mousedown", this, "_mouseDown"));
		this._connects.push(connect.connect(this.grid.domNode, "mouseup", this, "_mouseUp"));
		this._connects.push(connect.connect(this.grid.domNode, "oncontextmenu", this, "doContextMenu"));
		this._connects.push(connect.connect(this.grid.lastFocusNode, "onfocus", this, "doLastNodeFocus"));
		this._connects.push(connect.connect(this.grid.lastFocusNode, "onblur", this, "doLastNodeBlur"));
		this._connects.push(connect.connect(this.grid,"_onFetchComplete", this, "_delayedCellFocus"));
		this._connects.push(connect.connect(this.grid,"postrender", this, "_delayedHeaderFocus"));
	},
	destroy: function(){
		array.forEach(this._connects, connect.disconnect);
		array.forEach(this._headerConnects, connect.disconnect);
		delete this.grid;
		delete this.cell;
	},
	_colHeadNode: null,
	_colHeadFocusIdx: null,
	_contextMenuBindNode: null,
	tabbingOut: false,
	focusClass: "dojoxGridCellFocus",
	focusView: null,
	initFocusView: function(){
		this.focusView = this.grid.views.getFirstScrollingView() || this.focusView || this.grid.views.views[0];
		this._initColumnHeaders();
	},
	isFocusCell: function(inCell, inRowIndex){
		// summary:
		//		states if the given cell is focused
		// inCell: object
		//		grid cell object
		// inRowIndex: int
		//		grid row index
		// returns:
		//		true of the given grid cell is focused
		return (this.cell == inCell) && (this.rowIndex == inRowIndex);
	},
	isLastFocusCell: function(){
		if(this.cell){
			return (this.rowIndex == this.grid.rowCount-1) && (this.cell.index == this.grid.layout.cellCount-1);
		}
		return false;
	},
	isFirstFocusCell: function(){
		if(this.cell){
			return (this.rowIndex === 0) && (this.cell.index === 0);
		}
		return false;
	},
	isNoFocusCell: function(){
		return (this.rowIndex < 0) || !this.cell;
	},
	isNavHeader: function(){
		// summary:
		//		states whether currently navigating among column headers.
		// returns:
		//		true if focus is on a column header; false otherwise.
		return (!!this._colHeadNode);
	},
	getHeaderIndex: function(){
		// summary:
		//		if one of the column headers currently has focus, return its index.
		// returns:
		//		index of the focused column header, or -1 if none have focus.
		if(this._colHeadNode){
			return array.indexOf(this._findHeaderCells(), this._colHeadNode);
		}else{
			return -1;
		}
	},
	_focusifyCellNode: function(inBork){
		var n = this.cell && this.cell.getNode(this.rowIndex);
		if(n){
			html.toggleClass(n, this.focusClass, inBork);
			if(inBork){
				var sl = this.scrollIntoView();
				try{
					if(has("webkit") || !this.grid.edit.isEditing()){
						util.fire(n, "focus");
						if(sl){ this.cell.view.scrollboxNode.scrollLeft = sl; }
					}
				}catch(e){}
			}
		}
	},
	_delayedCellFocus: function(){
		if(this.isNavHeader()||!this.grid.focused){
				return;
		}
		var n = this.cell && this.cell.getNode(this.rowIndex);
		if(n){
			try{
				if(!this.grid.edit.isEditing()){
					html.toggleClass(n, this.focusClass, true);
					if(this._colHeadNode){
						this.blurHeader();
					}
					util.fire(n, "focus");
				}
			}
			catch(e){}
		}
	},
	_delayedHeaderFocus: function(){
		if(this.isNavHeader()){
			this.focusHeader();
			//this.grid.domNode.focus();
		}
	},
	_initColumnHeaders: function(){
		array.forEach(this._headerConnects, connect.disconnect);
		this._headerConnects = [];
		var headers = this._findHeaderCells();
		for(var i = 0; i < headers.length; i++){
			this._headerConnects.push(connect.connect(headers[i], "onfocus", this, "doColHeaderFocus"));
			this._headerConnects.push(connect.connect(headers[i], "onblur", this, "doColHeaderBlur"));
		}
	},
	_findHeaderCells: function(){
		// This should be a one liner:
		//	query("th[tabindex=-1]", this.grid.viewsHeaderNode);
		// But there is a bug in query() for IE -- see trac #7037.
		var allHeads = query("th", this.grid.viewsHeaderNode);
		var headers = [];
		for (var i = 0; i < allHeads.length; i++){
			var aHead = allHeads[i];
			var hasTabIdx = html.hasAttr(aHead, "tabIndex");
			var tabindex = html.attr(aHead, "tabIndex");
			if (hasTabIdx && tabindex < 0) {
				headers.push(aHead);
			}
		}
		return headers;
	},
	_setActiveColHeader: function(/*Node*/colHeaderNode, /*Integer*/colFocusIdx, /*Integer*/ prevColFocusIdx){
		//console.log("setActiveColHeader() - colHeaderNode:colFocusIdx:prevColFocusIdx = " + colHeaderNode + ":" + colFocusIdx + ":" + prevColFocusIdx);
		this.grid.domNode.setAttribute("aria-activedescendant",colHeaderNode.id);
		if (prevColFocusIdx != null && prevColFocusIdx >= 0 && prevColFocusIdx != colFocusIdx){
			html.toggleClass(this._findHeaderCells()[prevColFocusIdx],this.focusClass,false);
		}
		html.toggleClass(colHeaderNode,this.focusClass, true);
		this._colHeadNode = colHeaderNode;
		this._colHeadFocusIdx = colFocusIdx;
		this._scrollHeader(this._colHeadFocusIdx);
	},
	scrollIntoView: function(){
		var info = (this.cell ? this._scrollInfo(this.cell) : null);
		if(!info || !info.s){
			return null;
		}
		var rt = this.grid.scroller.findScrollTop(this.rowIndex);
		// place cell within horizontal view
		if(info.n && info.sr){
			if(info.n.offsetLeft + info.n.offsetWidth > info.sr.l + info.sr.w){
				info.s.scrollLeft = info.n.offsetLeft + info.n.offsetWidth - info.sr.w;
			}else if(info.n.offsetLeft < info.sr.l){
				info.s.scrollLeft = info.n.offsetLeft;
			}
		}
		// place cell within vertical view
		if(info.r && info.sr){
			if(rt + info.r.offsetHeight > info.sr.t + info.sr.h){
				this.grid.setScrollTop(rt + info.r.offsetHeight - info.sr.h);
			}else if(rt < info.sr.t){
				this.grid.setScrollTop(rt);
			}
		}

		return info.s.scrollLeft;
	},
	_scrollInfo: function(cell, domNode){
		if(cell){
			var cl = cell,
				sbn = cl.view.scrollboxNode,
				sbnr = {
					w: sbn.clientWidth,
					l: sbn.scrollLeft,
					t: sbn.scrollTop,
					h: sbn.clientHeight
				},
				rn = cl.view.getRowNode(this.rowIndex);
			return {
				c: cl,
				s: sbn,
				sr: sbnr,
				n: (domNode ? domNode : cell.getNode(this.rowIndex)),
				r: rn
			};
		}
		return null;
	},
	_scrollHeader: function(currentIdx){
		var info = null;
		if(this._colHeadNode){
			var cell = this.grid.getCell(currentIdx);
			if(!cell){ return; }
			info = this._scrollInfo(cell, cell.getNode(0));
		}
		if(info && info.s && info.sr && info.n){
			// scroll horizontally as needed.
			var scroll = info.sr.l + info.sr.w;
			if(info.n.offsetLeft + info.n.offsetWidth > scroll){
				info.s.scrollLeft = info.n.offsetLeft + info.n.offsetWidth - info.sr.w;
			}else if(info.n.offsetLeft < info.sr.l){
				info.s.scrollLeft = info.n.offsetLeft;
			}else if(has('ie') <= 7 && cell && cell.view.headerNode){
				// Trac 7158: scroll dojoxGridHeader for IE7 and lower
				cell.view.headerNode.scrollLeft = info.s.scrollLeft;
			}
		}
	},
	_isHeaderHidden: function(){
		// summary:
		//		determine if the grid headers are hidden
		//		relies on documented technique of setting .dojoxGridHeader { display:none; }
		// returns: Boolean
		//		true if headers are hidden
		//		false if headers are not hidden
		
		var curView = this.focusView;
		if (!curView){
			// find one so we can determine if headers are hidden
			// there is no focusView after adding items to empty grid (test_data_grid_empty.html)
			for (var i = 0, cView; (cView = this.grid.views.views[i]); i++) {
				if(cView.headerNode ){
					curView=cView;
					break;
				}
			}
		}
		return (curView && html.getComputedStyle(curView.headerNode).display == "none");
	},
	colSizeAdjust: function (e, colIdx, delta){ // adjust the column specified by colIdx by the specified delta px
		var headers = this._findHeaderCells();
		var view = this.focusView;
		if(!view || !view.header.tableMap.map){
			for(var i = 0, cView; (cView = this.grid.views.views[i]); i++){
				// find first view with a tableMap in order to work with empty grid
				if(cView.header.tableMap.map){
					view=cView;
					break;
				}
			}
		}
		var curHeader = headers[colIdx];
		if (!view || (colIdx == headers.length-1 && colIdx === 0)){
			return; // can't adjust single col. grid
		}
		view.content.baseDecorateEvent(e);
		// need to adjust event with header cell info since focus is no longer on header cell
		e.cellNode = curHeader; //this.findCellTarget(e.target, e.rowNode);
		e.cellIndex = view.content.getCellNodeIndex(e.cellNode);
		e.cell = (e.cellIndex >= 0 ? this.grid.getCell(e.cellIndex) : null);
		if (view.header.canResize(e)){
			var deltaObj = {
				l: delta
			};
			var drag = view.header.colResizeSetup(e,false);
			view.header.doResizeColumn(drag, null, deltaObj);
			view.update();
		}
	},
	styleRow: function(inRow){
		return;
	},
	setFocusIndex: function(inRowIndex, inCellIndex){
		// summary:
		//		focuses the given grid cell
		// inRowIndex: int
		//		grid row index
		// inCellIndex: int
		//		grid cell index
		this.setFocusCell(this.grid.getCell(inCellIndex), inRowIndex);
	},
	setFocusCell: function(inCell, inRowIndex){
		// summary:
		//		focuses the given grid cell
		// inCell: object
		//		grid cell object
		// inRowIndex: int
		//		grid row index
		if(inCell && !this.isFocusCell(inCell, inRowIndex)){
			this.tabbingOut = false;
			if (this._colHeadNode){
				this.blurHeader();
			}
			this._colHeadNode = this._colHeadFocusIdx = null;
			this.focusGridView();
			this._focusifyCellNode(false);
			this.cell = inCell;
			this.rowIndex = inRowIndex;
			this._focusifyCellNode(true);
		}
		// even if this cell isFocusCell, the document focus may need to be rejiggered
		// call opera on delay to prevent keypress from altering focus
		if(has('opera')){
			setTimeout(lang.hitch(this.grid, 'onCellFocus', this.cell, this.rowIndex), 1);
		}else{
			this.grid.onCellFocus(this.cell, this.rowIndex);
		}
	},
	next: function(){
		// summary:
		//	focus next grid cell
		if(this.cell){
			var row=this.rowIndex, col=this.cell.index+1, cc=this.grid.layout.cellCount-1, rc=this.grid.rowCount-1;
			if(col > cc){
				col = 0;
				row++;
			}
			if(row > rc){
				col = cc;
				row = rc;
			}
			if(this.grid.edit.isEditing()){ //when editing, only navigate to editable cells
				var nextCell = this.grid.getCell(col);
				if (!this.isLastFocusCell() && (!nextCell.editable ||
					this.grid.canEdit && !this.grid.canEdit(nextCell, row))){
					this.cell=nextCell;
					this.rowIndex=row;
					this.next();
					return;
				}
			}
			this.setFocusIndex(row, col);
		}
	},
	previous: function(){
		// summary:
		//	focus previous grid cell
		if(this.cell){
			var row=(this.rowIndex || 0), col=(this.cell.index || 0) - 1;
			if(col < 0){
				col = this.grid.layout.cellCount-1;
				row--;
			}
			if(row < 0){
				row = 0;
				col = 0;
			}
			if(this.grid.edit.isEditing()){ //when editing, only navigate to editable cells
				var prevCell = this.grid.getCell(col);
				if (!this.isFirstFocusCell() && !prevCell.editable){
					this.cell=prevCell;
					this.rowIndex=row;
					this.previous();
					return;
				}
			}
			this.setFocusIndex(row, col);
		}
	},
	move: function(inRowDelta, inColDelta) {
		// summary:
		//		focus grid cell or  simulate focus to column header based on position relative to current focus
		// inRowDelta: int
		//		vertical distance from current focus
		// inColDelta: int
		//		horizontal distance from current focus

		var colDir = inColDelta < 0 ? -1 : 1;
		// Handle column headers.
		if(this.isNavHeader()){
			var headers = this._findHeaderCells();
			var savedIdx = currentIdx = array.indexOf(headers, this._colHeadNode);
			currentIdx += inColDelta;
			while(currentIdx >=0 && currentIdx < headers.length && headers[currentIdx].style.display == "none"){
				// skip over hidden column headers
				currentIdx += colDir;
			}
			if((currentIdx >= 0) && (currentIdx < headers.length)){
				this._setActiveColHeader(headers[currentIdx],currentIdx, savedIdx);
			}
		}else{
			if(this.cell){
				// Handle grid proper.
				var sc = this.grid.scroller,
					r = this.rowIndex,
					rc = this.grid.rowCount-1,
					row = Math.min(rc, Math.max(0, r+inRowDelta));
				if(inRowDelta){
					if(inRowDelta>0){
						if(row > sc.getLastPageRow(sc.page)){
							//need to load additional data, let scroller do that
							this.grid.setScrollTop(this.grid.scrollTop+sc.findScrollTop(row)-sc.findScrollTop(r));
						}
					}else if(inRowDelta<0){
						if(row <= sc.getPageRow(sc.page)){
							//need to load additional data, let scroller do that
							this.grid.setScrollTop(this.grid.scrollTop-sc.findScrollTop(r)-sc.findScrollTop(row));
						}
					}
				}
				var cc = this.grid.layout.cellCount-1,
				i = this.cell.index,
				col = Math.min(cc, Math.max(0, i+inColDelta));
				var cell = this.grid.getCell(col);
				while(col>=0 && col < cc && cell && cell.hidden === true){
					// skip hidden cells
					col += colDir;
					cell = this.grid.getCell(col);
				}
				if (!cell || cell.hidden === true){
					// don't change col if would move to hidden
					col = i;
				}
				//skip hidden row|cell
				var n = cell.getNode(row);
				if(!n && inRowDelta){
					if((row + inRowDelta) >= 0 && (row + inRowDelta) <= rc){
						this.move(inRowDelta > 0 ? ++inRowDelta : --inRowDelta, inColDelta);
					}
					return;
				}else if((!n || html.style(n, "display") === "none") && inColDelta){
					if((col + inColDelta) >= 0 && (col + inColDelta) <= cc){
						this.move(inRowDelta, inColDelta > 0 ? ++inColDelta : --inColDelta);
					}
					return;
				}
				this.setFocusIndex(row, col);
				if(inRowDelta){
					this.grid.updateRow(r);
				}
			}
		}
	},
	previousKey: function(e){
		if(this.grid.edit.isEditing()){
			event.stop(e);
			this.previous();
		}else if(!this.isNavHeader() && !this._isHeaderHidden()) {
			this.grid.domNode.focus(); // will call doFocus and set focus into header.
			event.stop(e);
		}else{
			this.tabOut(this.grid.domNode);
			if (this._colHeadFocusIdx != null) { // clear grid header focus
				html.toggleClass(this._findHeaderCells()[this._colHeadFocusIdx], this.focusClass, false);
				this._colHeadFocusIdx = null;
			}
			this._focusifyCellNode(false);
		}
	},
	nextKey: function(e) {
		var isEmpty = (this.grid.rowCount === 0);
		if(e.target === this.grid.domNode && this._colHeadFocusIdx == null){
			this.focusHeader();
			event.stop(e);
		}else if(this.isNavHeader()){
			// if tabbing from col header, then go to grid proper.
			this.blurHeader();
			if(!this.findAndFocusGridCell()){
				this.tabOut(this.grid.lastFocusNode);
			}
			this._colHeadNode = this._colHeadFocusIdx= null;
		}else if(this.grid.edit.isEditing()){
			event.stop(e);
			this.next();
		}else{
			this.tabOut(this.grid.lastFocusNode);
		}
	},
	tabOut: function(inFocusNode){
		this.tabbingOut = true;
		inFocusNode.focus();
	},
	focusGridView: function(){
		util.fire(this.focusView, "focus");
	},
	focusGrid: function(inSkipFocusCell){
		this.focusGridView();
		this._focusifyCellNode(true);
	},
	findAndFocusGridCell: function(){
		// summary:
		//		find the first focusable grid cell
		// returns: Boolean
		//		true if focus was set to a cell
		//		false if no cell found to set focus onto
		
		var didFocus = true;
		var isEmpty = (this.grid.rowCount === 0); // If grid is empty this.grid.rowCount == 0
		if (this.isNoFocusCell() && !isEmpty){
			var cellIdx = 0;
			var cell = this.grid.getCell(cellIdx);
			if (cell.hidden) {
				// if first cell isn't visible, use _colHeadFocusIdx
				// could also use a while loop to find first visible cell - not sure that is worth it
				cellIdx = this.isNavHeader() ? this._colHeadFocusIdx : 0;
			}
			this.setFocusIndex(0, cellIdx);
		}
		else if (this.cell && !isEmpty){
			if (this.focusView && !this.focusView.rowNodes[this.rowIndex]){
				// if rowNode for current index is undefined (likely as a result of a sort and because of #7304)
				// scroll to that row
				this.grid.scrollToRow(this.rowIndex);
			}
			this.focusGrid();
		}else {
			didFocus = false;
		}
		this._colHeadNode = this._colHeadFocusIdx= null;
		return didFocus;
	},
	focusHeader: function(){
		var headerNodes = this._findHeaderCells();
		var saveColHeadFocusIdx = this._colHeadFocusIdx;
		if (this._isHeaderHidden()){
			// grid header is hidden, focus a cell
			this.findAndFocusGridCell();
		}
		else if (!this._colHeadFocusIdx) {
			if (this.isNoFocusCell()) {
				this._colHeadFocusIdx = 0;
			}
			else {
				this._colHeadFocusIdx = this.cell.index;
			}
		}
		this._colHeadNode = headerNodes[this._colHeadFocusIdx];
		while(this._colHeadNode && this._colHeadFocusIdx >=0 && this._colHeadFocusIdx < headerNodes.length &&
				this._colHeadNode.style.display == "none"){
			// skip over hidden column headers
			this._colHeadFocusIdx++;
			this._colHeadNode = headerNodes[this._colHeadFocusIdx];
		}
		if(this._colHeadNode && this._colHeadNode.style.display != "none"){
			// Column header cells know longer receive actual focus.  So, for keyboard invocation of
			// contextMenu to work, the contextMenu must be bound to the grid.domNode rather than the viewsHeaderNode.
			// unbind the contextmenu from the viewsHeaderNode and to the grid when header cells are active.  Reset
			// the binding back to the viewsHeaderNode when header cells are no longer acive (in blurHeader) #10483
			if (this.headerMenu && this._contextMenuBindNode != this.grid.domNode){
				this.headerMenu.unBindDomNode(this.grid.viewsHeaderNode);
				this.headerMenu.bindDomNode(this.grid.domNode);
				this._contextMenuBindNode = this.grid.domNode;
			}
			this._setActiveColHeader(this._colHeadNode, this._colHeadFocusIdx, saveColHeadFocusIdx);
			this._scrollHeader(this._colHeadFocusIdx);
			this._focusifyCellNode(false);
		}else {
			// all col head nodes are hidden - focus the grid
			this.findAndFocusGridCell();
		}
	},
	blurHeader: function(){
		html.removeClass(this._colHeadNode, this.focusClass);
		html.removeAttr(this.grid.domNode,"aria-activedescendant");
		// reset contextMenu onto viewsHeaderNode so right mouse on header will invoke (see focusHeader)
		if (this.headerMenu && this._contextMenuBindNode == this.grid.domNode) {
			var viewsHeader = this.grid.viewsHeaderNode;
			this.headerMenu.unBindDomNode(this.grid.domNode);
			this.headerMenu.bindDomNode(viewsHeader);
			this._contextMenuBindNode = viewsHeader;
		}
	},
	doFocus: function(e){
		// trap focus only for grid dom node
		if(e && e.target != e.currentTarget){
			event.stop(e);
			return;
		}
		// don't change focus if clicking on scroller bar
		if(this._clickFocus){
			return;
		}
		// do not focus for scrolling if grid is about to blur
		if(!this.tabbingOut){
			this.focusHeader();
		}
		this.tabbingOut = false;
		event.stop(e);
	},
	doBlur: function(e){
		event.stop(e);	// FF2
	},
	doContextMenu: function(e){
	//stop contextMenu event if no header Menu to prevent default/browser contextMenu
		if (!this.headerMenu){
			event.stop(e);
		}
	},
	doLastNodeFocus: function(e){
		if (this.tabbingOut){
			this._focusifyCellNode(false);
		}else if(this.grid.rowCount >0){
			if (this.isNoFocusCell()){
				this.setFocusIndex(0,0);
			}
			this._focusifyCellNode(true);
		}else {
			this.focusHeader();
		}
		this.tabbingOut = false;
		event.stop(e);	 // FF2
	},
	doLastNodeBlur: function(e){
		event.stop(e);	 // FF2
	},
	doColHeaderFocus: function(e){
		this._setActiveColHeader(e.target,html.attr(e.target, "idx"),this._colHeadFocusIdx);
		this._scrollHeader(this.getHeaderIndex());
		event.stop(e);
	},
	doColHeaderBlur: function(e){
		html.toggleClass(e.target, this.focusClass, false);
	},
	_mouseDown: function(e){
		// a flag indicating grid is being focused by clicking
		this._clickFocus = dojo.some(this.grid.views.views, function(v){
			return v.scrollboxNode === e.target;
		});
	},
	_mouseUp: function(e){
		this._clickFocus = false;
	}
});
});
},
'dojox/grid/_EditManager':function(){
define([
	"dojo/_base/lang",
	"dojo/_base/array",
	"dojo/_base/declare",
	"dojo/_base/connect",
	"dojo/_base/sniff",
	"./util"
], function(lang, array, declare, connect, has, util){

return declare("dojox.grid._EditManager", null, {
	// summary:
	//		Controls grid cell editing process. Owned by grid and used internally for editing.
	constructor: function(inGrid){
		// inGrid: dojox.Grid
		//		The dojox.Grid this editor should be attached to
		this.grid = inGrid;
		this.connections = !has('ie') ? [] : [connect.connect(document.body, "onfocus", lang.hitch(this, "_boomerangFocus"))];
		this.connections.push(connect.connect(this.grid, 'onBlur', this, 'apply'));
		this.connections.push(connect.connect(this.grid, 'prerender', this, '_onPreRender'));
	},
	
	info: {},

	destroy: function(){
		array.forEach(this.connections, connect.disconnect);
	},

	cellFocus: function(inCell, inRowIndex){
		// summary:
		//		Invoke editing when cell is focused
		// inCell: cell object
		//		Grid cell object
		// inRowIndex: Integer
		//		Grid row index
		if(this.grid.singleClickEdit || this.isEditRow(inRowIndex)){
			// if same row or quick editing, edit
			this.setEditCell(inCell, inRowIndex);
		}else{
			// otherwise, apply any pending row edits
			this.apply();
		}
		// if dynamic or static editing...
		if(this.isEditing() || (inCell && inCell.editable && inCell.alwaysEditing)){
			// let the editor focus itself as needed
			this._focusEditor(inCell, inRowIndex);
		}
	},

	rowClick: function(e){
		if(this.isEditing() && !this.isEditRow(e.rowIndex)){
			this.apply();
		}
	},

	styleRow: function(inRow){
		if(inRow.index == this.info.rowIndex){
			inRow.customClasses += ' dojoxGridRowEditing';
		}
	},

	dispatchEvent: function(e){
		var c = e.cell, ed = (c && c["editable"]) ? c : 0;
		return ed && ed.dispatchEvent(e.dispatch, e);
	},

	// Editing
	isEditing: function(){
		// summary:
		//		Indicates editing state of the grid.
		// returns: Boolean
		//	 	True if grid is actively editing
		return this.info.rowIndex !== undefined;
	},

	isEditCell: function(inRowIndex, inCellIndex){
		// summary:
		//		Indicates if the given cell is being edited.
		// inRowIndex: Integer
		//		Grid row index
		// inCellIndex: Integer
		//		Grid cell index
		// returns: Boolean
		//	 	True if given cell is being edited
		return (this.info.rowIndex === inRowIndex) && (this.info.cell.index == inCellIndex);
	},

	isEditRow: function(inRowIndex){
		// summary:
		//		Indicates if the given row is being edited.
		// inRowIndex: Integer
		//		Grid row index
		// returns: Boolean
		//	 	True if given row is being edited
		return this.info.rowIndex === inRowIndex;
	},

	setEditCell: function(inCell, inRowIndex){
		// summary:
		//		Set the given cell to be edited
		// inRowIndex: Integer
		//		Grid row index
		// inCell: Object
		//		Grid cell object
		if(!this.isEditCell(inRowIndex, inCell.index) && this.grid.canEdit && this.grid.canEdit(inCell, inRowIndex)){
			this.start(inCell, inRowIndex, this.isEditRow(inRowIndex) || inCell.editable);
		}
	},

	_focusEditor: function(inCell, inRowIndex){
		util.fire(inCell, "focus", [inRowIndex]);
	},

	focusEditor: function(){
		if(this.isEditing()){
			this._focusEditor(this.info.cell, this.info.rowIndex);
		}
	},

	// implement fix for focus boomerang effect on IE
	_boomerangWindow: 500,
	_shouldCatchBoomerang: function(){
		return this._catchBoomerang > new Date().getTime();
	},
	_boomerangFocus: function(){
		//console.log("_boomerangFocus");
		if(this._shouldCatchBoomerang()){
			// make sure we don't utterly lose focus
			this.grid.focus.focusGrid();
			// let the editor focus itself as needed
			this.focusEditor();
			// only catch once
			this._catchBoomerang = 0;
		}
	},
	_doCatchBoomerang: function(){
		// give ourselves a few ms to boomerang IE focus effects
		if(has('ie')){this._catchBoomerang = new Date().getTime() + this._boomerangWindow;}
	},
	// end boomerang fix API

	start: function(inCell, inRowIndex, inEditing){
		if(!this._isValidInput()){
			return;
		}
		this.grid.beginUpdate();
		this.editorApply();
		if(this.isEditing() && !this.isEditRow(inRowIndex)){
			this.applyRowEdit();
			this.grid.updateRow(inRowIndex);
		}
		if(inEditing){
			this.info = { cell: inCell, rowIndex: inRowIndex };
			this.grid.doStartEdit(inCell, inRowIndex);
			this.grid.updateRow(inRowIndex);
		}else{
			this.info = {};
		}
		this.grid.endUpdate();
		// make sure we don't utterly lose focus
		this.grid.focus.focusGrid();
		// let the editor focus itself as needed
		this._focusEditor(inCell, inRowIndex);
		// give ourselves a few ms to boomerang IE focus effects
		this._doCatchBoomerang();
	},

	_editorDo: function(inMethod){
		var c = this.info.cell;
		//c && c.editor && c.editor[inMethod](c, this.info.rowIndex);
		if(c && c.editable){
			c[inMethod](this.info.rowIndex);
		}
	},

	editorApply: function(){
		this._editorDo("apply");
	},

	editorCancel: function(){
		this._editorDo("cancel");
	},

	applyCellEdit: function(inValue, inCell, inRowIndex){
		if(this.grid.canEdit(inCell, inRowIndex)){
			this.grid.doApplyCellEdit(inValue, inRowIndex, inCell.field);
		}
	},

	applyRowEdit: function(){
		this.grid.doApplyEdit(this.info.rowIndex, this.info.cell.field);
	},

	apply: function(){
		// summary:
		//		Apply a grid edit
		if(this.isEditing() && this._isValidInput()){
			this.grid.beginUpdate();
			this.editorApply();
			this.applyRowEdit();
			this.info = {};
			this.grid.endUpdate();
			this.grid.focus.focusGrid();
			this._doCatchBoomerang();
		}
	},

	cancel: function(){
		// summary:
		//		Cancel a grid edit
		if(this.isEditing()){
			this.grid.beginUpdate();
			this.editorCancel();
			this.info = {};
			this.grid.endUpdate();
			this.grid.focus.focusGrid();
			this._doCatchBoomerang();
		}
	},

	save: function(inRowIndex, inView){
		// summary:
		//		Save the grid editing state
		// inRowIndex: Integer
		//		Grid row index
		// inView: Object
		//		Grid view
		var c = this.info.cell;
		if(this.isEditRow(inRowIndex) && (!inView || c.view==inView) && c.editable){
			c.save(c, this.info.rowIndex);
		}
	},

	restore: function(inView, inRowIndex){
		// summary:
		//		Restores the grid editing state
		// inRowIndex: Integer
		//		Grid row index
		// inView: Object
		//		Grid view
		var c = this.info.cell;
		if(this.isEditRow(inRowIndex) && c.view == inView && c.editable){
			c.restore(this.info.rowIndex);
		}
	},
	
	_isValidInput: function(){
		var w = (this.info.cell || {}).widget;		
		if(!w || !w.isValid){
			//no validation needed
			return true;
		}		
		w.focused = true;
		return w.isValid(true);
	},
	
	_onPreRender: function(){
		if(this.isEditing()){
			//cache the current editing value before render
			this.info.value = this.info.cell.getValue();
		}
	}
});
});
},
'dojox/grid/Selection':function(){
define([
	"dojo/_base/declare",
	"dojo/_base/array",
	"dojo/_base/lang",
	"dojo/dom-attr"
], function(declare, array, lang, domAttr){

return declare("dojox.grid.Selection", null, {
	// summary:
	//		Manages row selection for grid. Owned by grid and used internally
	//		for selection. Override to implement custom selection.

	constructor: function(inGrid){
		this.grid = inGrid;
		this.selected = [];

		this.setMode(inGrid.selectionMode);
	},

	mode: 'extended',

	selected: null,
	updating: 0,
	selectedIndex: -1,
	rangeStartIndex: -1,

	setMode: function(mode){
		if(this.selected.length){
			this.deselectAll();
		}
		if(mode != 'extended' && mode != 'multiple' && mode != 'single' && mode != 'none'){
			this.mode = 'extended';
		}else{
			this.mode = mode;
		}
	},

	onCanSelect: function(inIndex){
		return this.grid.onCanSelect(inIndex);
	},

	onCanDeselect: function(inIndex){
		return this.grid.onCanDeselect(inIndex);
	},

	onSelected: function(inIndex){
	},

	onDeselected: function(inIndex){
	},

	//onSetSelected: function(inIndex, inSelect) { };
	onChanging: function(){
	},

	onChanged: function(){
	},

	isSelected: function(inIndex){
		if(this.mode == 'none'){
			return false;
		}
		return this.selected[inIndex];
	},

	getFirstSelected: function(){
		if(!this.selected.length||this.mode == 'none'){ return -1; }
		for(var i=0, l=this.selected.length; i<l; i++){
			if(this.selected[i]){
				return i;
			}
		}
		return -1;
	},

	getNextSelected: function(inPrev){
		if(this.mode == 'none'){ return -1; }
		for(var i=inPrev+1, l=this.selected.length; i<l; i++){
			if(this.selected[i]){
				return i;
			}
		}
		return -1;
	},

	getSelected: function(){
		var result = [];
		for(var i=0, l=this.selected.length; i<l; i++){
			if(this.selected[i]){
				result.push(i);
			}
		}
		return result;
	},

	getSelectedCount: function(){
		var c = 0;
		for(var i=0; i<this.selected.length; i++){
			if(this.selected[i]){
				c++;
			}
		}
		return c;
	},

	_beginUpdate: function(){
		if(this.updating === 0){
			this.onChanging();
		}
		this.updating++;
	},

	_endUpdate: function(){
		this.updating--;
		if(this.updating === 0){
			this.onChanged();
		}
	},

	select: function(inIndex){
		if(this.mode == 'none'){ return; }
		if(this.mode != 'multiple'){
			this.deselectAll(inIndex);
			this.addToSelection(inIndex);
		}else{
			this.toggleSelect(inIndex);
		}
	},

	addToSelection: function(inIndex){
		if(this.mode == 'none'){ return; }
		if(lang.isArray(inIndex)){
			array.forEach(inIndex, this.addToSelection, this);
			return;
		}
		inIndex = Number(inIndex);
		if(this.selected[inIndex]){
			this.selectedIndex = inIndex;
		}else{
			if(this.onCanSelect(inIndex) !== false){
				this.selectedIndex = inIndex;
				var rowNode = this.grid.getRowNode(inIndex);
				if(rowNode){
					domAttr.set(rowNode, "aria-selected", "true");
				}
				this._beginUpdate();
				this.selected[inIndex] = true;
				//this.grid.onSelected(inIndex);
				this.onSelected(inIndex);
				//this.onSetSelected(inIndex, true);
				this._endUpdate();
			}
		}
	},

	deselect: function(inIndex){
		if(this.mode == 'none'){ return; }
		if(lang.isArray(inIndex)){
			array.forEach(inIndex, this.deselect, this);
			return;
		}
		inIndex = Number(inIndex);
		if(this.selectedIndex == inIndex){
			this.selectedIndex = -1;
		}
		if(this.selected[inIndex]){
			if(this.onCanDeselect(inIndex) === false){
				return;
			}
			var rowNode = this.grid.getRowNode(inIndex);
			if(rowNode){
				domAttr.set(rowNode, "aria-selected", "false");
			}
			this._beginUpdate();
			delete this.selected[inIndex];
			//this.grid.onDeselected(inIndex);
			this.onDeselected(inIndex);
			//this.onSetSelected(inIndex, false);
			this._endUpdate();
		}
	},

	setSelected: function(inIndex, inSelect){
		this[(inSelect ? 'addToSelection' : 'deselect')](inIndex);
	},

	toggleSelect: function(inIndex){
		if(lang.isArray(inIndex)){
			array.forEach(inIndex, this.toggleSelect, this);
			return;
		}
		this.setSelected(inIndex, !this.selected[inIndex]);
	},

	_range: function(inFrom, inTo, func){
		var s = (inFrom >= 0 ? inFrom : inTo), e = inTo;
		if(s > e){
			e = s;
			s = inTo;
		}
		for(var i=s; i<=e; i++){
			func(i);
		}
	},

	selectRange: function(inFrom, inTo){
		this._range(inFrom, inTo, lang.hitch(this, "addToSelection"));
	},

	deselectRange: function(inFrom, inTo){
		this._range(inFrom, inTo, lang.hitch(this, "deselect"));
	},

	insert: function(inIndex){
		this.selected.splice(inIndex, 0, false);
		if(this.selectedIndex >= inIndex){
			this.selectedIndex++;
		}
	},

	remove: function(inIndex){
		this.selected.splice(inIndex, 1);
		if(this.selectedIndex >= inIndex){
			this.selectedIndex--;
		}
	},

	deselectAll: function(inExcept){
		for(var i in this.selected){
			if((i!=inExcept)&&(this.selected[i]===true)){
				this.deselect(i);
			}
		}
	},

	clickSelect: function(inIndex, inCtrlKey, inShiftKey){
		if(this.mode == 'none'){ return; }
		this._beginUpdate();
		if(this.mode != 'extended'){
			this.select(inIndex);
		}else{
			if(!inShiftKey || this.rangeStartIndex < 0){
				this.rangeStartIndex = inIndex;
			}
			if(!inCtrlKey){
				this.deselectAll(inIndex);
			}
			if(inShiftKey){
				this.selectRange(this.rangeStartIndex, inIndex);
			}else if(inCtrlKey){
				this.toggleSelect(inIndex);
			}else{
				this.addToSelection(inIndex);
			}
		}
		this._endUpdate();
	},

	clickSelectEvent: function(e){
		this.clickSelect(e.rowIndex, dojo.isCopyKey(e), e.shiftKey);
	},

	clear: function(){
		this._beginUpdate();
		this.deselectAll();
		this._endUpdate();
	}
});
});
},
'dijit/CheckedMenuItem':function(){
define([
	"dojo/_base/declare", // declare
	"dojo/dom-class", // domClass.toggle
	"./MenuItem",
	"dojo/text!./templates/CheckedMenuItem.html",
	"./hccss"
], function(declare, domClass, MenuItem, template){

	// module:
	//		dijit/CheckedMenuItem

	return declare("dijit.CheckedMenuItem", MenuItem, {
		// summary:
		//		A checkbox-like menu item for toggling on and off

		// Use both base classes so we get styles like dijitMenuItemDisabled
		baseClass: "dijitMenuItem dijitCheckedMenuItem",

		templateString: template,

		// checked: Boolean
		//		Our checked state
		checked: false,
		_setCheckedAttr: function(/*Boolean*/ checked){
			this.domNode.setAttribute("aria-checked", checked ? "true" : "false");
			this._set("checked", checked);	// triggers CSS update via _CssStateMixin
		},

		iconClass: "",	// override dijitNoIcon

		role: "menuitemcheckbox",

		// checkedChar: String
		//		Character (or string) used in place of checkbox icon when display in high contrast mode
		checkedChar: "&#10003;",

		onChange: function(/*Boolean*/ /*===== checked =====*/){
			// summary:
			//		User defined function to handle check/uncheck events
			// tags:
			//		callback
		},

		_onClick: function(evt){
			// summary:
			//		Clicking this item just toggles its state
			// tags:
			//		private
			if(!this.disabled){
				this.set("checked", !this.checked);
				this.onChange(this.checked);
			}
			this.onClick(evt);
		}
	});
});

},
'dijit/MenuItem':function(){
define([
	"dojo/_base/declare", // declare
	"dojo/dom", // dom.setSelectable
	"dojo/dom-attr", // domAttr.set
	"dojo/dom-class", // domClass.toggle
	"dojo/_base/kernel", // kernel.deprecated
	"dojo/sniff", // has("ie")
	"dojo/_base/lang", // lang.hitch
	"./_Widget",
	"./_TemplatedMixin",
	"./_Contained",
	"./_CssStateMixin",
	"dojo/text!./templates/MenuItem.html"
], function(declare, dom, domAttr, domClass, kernel, has, lang,
			_Widget, _TemplatedMixin, _Contained, _CssStateMixin, template){

	// module:
	//		dijit/MenuItem

	var MenuItem = declare("dijit.MenuItem" + (has("dojo-bidi") ? "_NoBidi" : ""),
		[_Widget, _TemplatedMixin, _Contained, _CssStateMixin], {
		// summary:
		//		A line item in a Menu Widget

		// Make 3 columns
		// icon, label, and expand arrow (BiDi-dependent) indicating sub-menu
		templateString: template,

		baseClass: "dijitMenuItem",

		// label: String
		//		Menu text as HTML
		label: "",
		_setLabelAttr: function(val){
			this._set("label", val);
			var shortcutKey = "";
			var text;
			var ndx = val.search(/{\S}/);
			if(ndx >= 0){
				shortcutKey = val.charAt(ndx + 1);
				var prefix = val.substr(0, ndx);
				var suffix = val.substr(ndx + 3);
				text = prefix + shortcutKey + suffix;
				val = prefix + '<span class="dijitMenuItemShortcutKey">' + shortcutKey + '</span>' + suffix;
			}else{
				text = val;
			}
			this.domNode.setAttribute("aria-label", text + " " + this.accelKey);
			this.containerNode.innerHTML = val;
			this._set('shortcutKey', shortcutKey);
		},

		/*=====
		// shortcutKey: [readonly] String
		//		Single character (underlined when the parent Menu is focused) used to navigate directly to this widget,
		//		also known as [a mnemonic](http://en.wikipedia.org/wiki/Mnemonics_(keyboard%29).
		//		This is denoted in the label by surrounding the single character with {}.
		//		For example, if label="{F}ile", then shortcutKey="F".
		shortcutKey: "",
		=====*/

		// iconClass: String
		//		Class to apply to DOMNode to make it display an icon.
		iconClass: "dijitNoIcon",
		_setIconClassAttr: { node: "iconNode", type: "class" },

		// accelKey: String
		//		Text for the accelerator (shortcut) key combination, a control, alt, etc. modified keystroke meant to
		//		execute the menu item regardless of where the focus is on the page.
		//
		//		Note that although Menu can display accelerator keys, there is no infrastructure to actually catch and
		//		execute those accelerators.
		accelKey: "",

		// disabled: Boolean
		//		If true, the menu item is disabled.
		//		If false, the menu item is enabled.
		disabled: false,

		_fillContent: function(/*DomNode*/ source){
			// If button label is specified as srcNodeRef.innerHTML rather than
			// this.params.label, handle it here.
			if(source && !("label" in this.params)){
				this._set('label', source.innerHTML);
			}
		},

		buildRendering: function(){
			this.inherited(arguments);
			var label = this.id + "_text";
			domAttr.set(this.containerNode, "id", label); // only needed for backward compat
			if(this.accelKeyNode){
				domAttr.set(this.accelKeyNode, "id", this.id + "_accel"); // only needed for backward compat
			}
			dom.setSelectable(this.domNode, false);
		},

		onClick: function(/*Event*/){
			// summary:
			//		User defined function to handle clicks
			// tags:
			//		callback
		},

		focus: function(){
			// summary:
			//		Focus on this MenuItem
			try{
				if(has("ie") == 8){
					// needed for IE8 which won't scroll TR tags into view on focus yet calling scrollIntoView creates flicker (#10275)
					this.containerNode.focus();
				}
				this.focusNode.focus();
			}catch(e){
				// this throws on IE (at least) in some scenarios
			}
		},

		_setSelected: function(selected){
			// summary:
			//		Indicate that this node is the currently selected one
			// tags:
			//		private

			domClass.toggle(this.domNode, "dijitMenuItemSelected", selected);
		},

		setLabel: function(/*String*/ content){
			// summary:
			//		Deprecated.   Use set('label', ...) instead.
			// tags:
			//		deprecated
			kernel.deprecated("dijit.MenuItem.setLabel() is deprecated.  Use set('label', ...) instead.", "", "2.0");
			this.set("label", content);
		},

		setDisabled: function(/*Boolean*/ disabled){
			// summary:
			//		Deprecated.   Use set('disabled', bool) instead.
			// tags:
			//		deprecated
			kernel.deprecated("dijit.Menu.setDisabled() is deprecated.  Use set('disabled', bool) instead.", "", "2.0");
			this.set('disabled', disabled);
		},

		_setDisabledAttr: function(/*Boolean*/ value){
			// summary:
			//		Hook for attr('disabled', ...) to work.
			//		Enable or disable this menu item.

			this.focusNode.setAttribute('aria-disabled', value ? 'true' : 'false');
			this._set("disabled", value);
		},

		_setAccelKeyAttr: function(/*String*/ value){
			// summary:
			//		Hook for attr('accelKey', ...) to work.
			//		Set accelKey on this menu item.

			if(this.accelKeyNode){
				this.accelKeyNode.style.display = value ? "" : "none";
				this.accelKeyNode.innerHTML = value;
				//have to use colSpan to make it work in IE
				domAttr.set(this.containerNode, 'colSpan', value ? "1" : "2");
			}
			this._set("accelKey", value);
		}
	});

	if(has("dojo-bidi")){
		MenuItem = declare("dijit.MenuItem", MenuItem, {
			_setLabelAttr: function(val){
				this.inherited(arguments);
				if(this.textDir === "auto"){
					this.applyTextDir(this.textDirNode);
				}
			}
		});
	}

	return MenuItem;
});

},
'dijit/_Contained':function(){
define([
	"dojo/_base/declare", // declare
	"./registry"	// registry.getEnclosingWidget(), registry.byNode()
], function(declare, registry){

	// module:
	//		dijit/_Contained

	return declare("dijit._Contained", null, {
		// summary:
		//		Mixin for widgets that are children of a container widget
		// example:
		//	|	// make a basic custom widget that knows about its parents
		//	|	declare("my.customClass",[dijit._WidgetBase, dijit._Contained],{});

		_getSibling: function(/*String*/ which){
			// summary:
			//		Returns next or previous sibling
			// which:
			//		Either "next" or "previous"
			// tags:
			//		private
			var p = this.getParent();
			return (p && p._getSiblingOfChild && p._getSiblingOfChild(this, which == "previous" ? -1 : 1)) || null;	// dijit/_WidgetBase
		},

		getPreviousSibling: function(){
			// summary:
			//		Returns null if this is the first child of the parent,
			//		otherwise returns the next element sibling to the "left".

			return this._getSibling("previous"); // dijit/_WidgetBase
		},

		getNextSibling: function(){
			// summary:
			//		Returns null if this is the last child of the parent,
			//		otherwise returns the next element sibling to the "right".

			return this._getSibling("next"); // dijit/_WidgetBase
		},

		getIndexInParent: function(){
			// summary:
			//		Returns the index of this widget within its container parent.
			//		It returns -1 if the parent does not exist, or if the parent
			//		is not a dijit/_Container

			var p = this.getParent();
			if(!p || !p.getIndexOfChild){
				return -1; // int
			}
			return p.getIndexOfChild(this); // int
		}
	});
});

},
'dijit/_CssStateMixin':function(){
define([
	"dojo/_base/array", // array.forEach array.map
	"dojo/_base/declare", // declare
	"dojo/dom", // dom.isDescendant()
	"dojo/dom-class", // domClass.toggle
	"dojo/has",
	"dojo/_base/lang", // lang.hitch
	"dojo/on",
	"dojo/domReady",
	"dojo/touch",
	"dojo/_base/window", // win.body
	"./a11yclick",
	"./registry"
], function(array, declare, dom, domClass, has, lang, on, domReady, touch, win, a11yclick, registry){

	// module:
	//		dijit/_CssStateMixin

	var CssStateMixin = declare("dijit._CssStateMixin", [], {
		// summary:
		//		Mixin for widgets to set CSS classes on the widget DOM nodes depending on hover/mouse press/focus
		//		state changes, and also higher-level state changes such becoming disabled or selected.
		//
		// description:
		//		By mixing this class into your widget, and setting the this.baseClass attribute, it will automatically
		//		maintain CSS classes on the widget root node (this.domNode) depending on hover,
		//		active, focus, etc. state.   Ex: with a baseClass of dijitButton, it will apply the classes
		//		dijitButtonHovered and dijitButtonActive, as the user moves the mouse over the widget and clicks it.
		//
		//		It also sets CSS like dijitButtonDisabled based on widget semantic state.
		//
		//		By setting the cssStateNodes attribute, a widget can also track events on subnodes (like buttons
		//		within the widget).

		/*=====
		 // cssStateNodes: [protected] Object
		 //		Subclasses may define a cssStateNodes property that lists sub-nodes within the widget that
		 //		need CSS classes applied on mouse hover/press and focus.
		 //
		 //		Each entry in this optional hash is a an attach-point name (like "upArrowButton") mapped to a CSS class name
		 //		(like "dijitUpArrowButton"). Example:
		 //	|		{
		 //	|			"upArrowButton": "dijitUpArrowButton",
		 //	|			"downArrowButton": "dijitDownArrowButton"
		 //	|		}
		 //		The above will set the CSS class dijitUpArrowButton to the this.upArrowButton DOMNode when it
		 //		is hovered, etc.
		 cssStateNodes: {},
		 =====*/

		// hovering: [readonly] Boolean
		//		True if cursor is over this widget
		hovering: false,

		// active: [readonly] Boolean
		//		True if mouse was pressed while over this widget, and hasn't been released yet
		active: false,

		_applyAttributes: function(){
			// This code would typically be in postCreate(), but putting in _applyAttributes() for
			// performance: so the class changes happen before DOM is inserted into the document.
			// Change back to postCreate() in 2.0.  See #11635.

			this.inherited(arguments);

			// Monitoring changes to disabled, readonly, etc. state, and update CSS class of root node
			array.forEach(["disabled", "readOnly", "checked", "selected", "focused", "state", "hovering", "active", "_opened"], function(attr){
				this.watch(attr, lang.hitch(this, "_setStateClass"));
			}, this);

			// Track hover and active mouse events on widget root node, plus possibly on subnodes
			for(var ap in this.cssStateNodes || {}){
				this._trackMouseState(this[ap], this.cssStateNodes[ap]);
			}
			this._trackMouseState(this.domNode, this.baseClass);

			// Set state initially; there's probably no hover/active/focus state but widget might be
			// disabled/readonly/checked/selected so we want to set CSS classes for those conditions.
			this._setStateClass();
		},

		_cssMouseEvent: function(/*Event*/ event){
			// summary:
			//		Handler for CSS event on this.domNode. Sets hovering and active properties depending on mouse state,
			//		which triggers _setStateClass() to set appropriate CSS classes for this.domNode.

			if(!this.disabled){
				switch(event.type){
					case "mouseover":
					case "MSPointerOver":
					case "pointerover":
						this._set("hovering", true);
						this._set("active", this._mouseDown);
						break;
					case "mouseout":
					case "MSPointerOut":
					case "pointerout":
						this._set("hovering", false);
						this._set("active", false);
						break;
					case "mousedown":
					case "touchstart":
					case "MSPointerDown":
					case "pointerdown":
					case "keydown":
						this._set("active", true);
						break;
					case "mouseup":
					case "dojotouchend":
					case "MSPointerUp":
					case "pointerup":
					case "keyup":
						this._set("active", false);
						break;
				}
			}
		},

		_setStateClass: function(){
			// summary:
			//		Update the visual state of the widget by setting the css classes on this.domNode
			//		(or this.stateNode if defined) by combining this.baseClass with
			//		various suffixes that represent the current widget state(s).
			//
			// description:
			//		In the case where a widget has multiple
			//		states, it sets the class based on all possible
			//		combinations.  For example, an invalid form widget that is being hovered
			//		will be "dijitInput dijitInputInvalid dijitInputHover dijitInputInvalidHover".
			//
			//		The widget may have one or more of the following states, determined
			//		by this.state, this.checked, this.valid, and this.selected:
			//
			//		- Error - ValidationTextBox sets this.state to "Error" if the current input value is invalid
			//		- Incomplete - ValidationTextBox sets this.state to "Incomplete" if the current input value is not finished yet
			//		- Checked - ex: a checkmark or a ToggleButton in a checked state, will have this.checked==true
			//		- Selected - ex: currently selected tab will have this.selected==true
			//
			//		In addition, it may have one or more of the following states,
			//		based on this.disabled and flags set in _onMouse (this.active, this.hovering) and from focus manager (this.focused):
			//
			//		- Disabled	- if the widget is disabled
			//		- Active		- if the mouse (or space/enter key?) is being pressed down
			//		- Focused		- if the widget has focus
			//		- Hover		- if the mouse is over the widget

			// Compute new set of classes
			var newStateClasses = this.baseClass.split(" ");

			function multiply(modifier){
				newStateClasses = newStateClasses.concat(array.map(newStateClasses, function(c){
					return c + modifier;
				}), "dijit" + modifier);
			}

			if(!this.isLeftToRight()){
				// For RTL mode we need to set an addition class like dijitTextBoxRtl.
				multiply("Rtl");
			}

			var checkedState = this.checked == "mixed" ? "Mixed" : (this.checked ? "Checked" : "");
			if(this.checked){
				multiply(checkedState);
			}
			if(this.state){
				multiply(this.state);
			}
			if(this.selected){
				multiply("Selected");
			}
			if(this._opened){
				multiply("Opened");
			}

			if(this.disabled){
				multiply("Disabled");
			}else if(this.readOnly){
				multiply("ReadOnly");
			}else{
				if(this.active){
					multiply("Active");
				}else if(this.hovering){
					multiply("Hover");
				}
			}

			if(this.focused){
				multiply("Focused");
			}

			// Remove old state classes and add new ones.
			// For performance concerns we only write into domNode.className once.
			var tn = this.stateNode || this.domNode,
				classHash = {};	// set of all classes (state and otherwise) for node

			array.forEach(tn.className.split(" "), function(c){
				classHash[c] = true;
			});

			if("_stateClasses" in this){
				array.forEach(this._stateClasses, function(c){
					delete classHash[c];
				});
			}

			array.forEach(newStateClasses, function(c){
				classHash[c] = true;
			});

			var newClasses = [];
			for(var c in classHash){
				newClasses.push(c);
			}
			tn.className = newClasses.join(" ");

			this._stateClasses = newStateClasses;
		},

		_subnodeCssMouseEvent: function(node, clazz, evt){
			// summary:
			//		Handler for hover/active mouse event on widget's subnode
			if(this.disabled || this.readOnly){
				return;
			}

			function hover(isHovering){
				domClass.toggle(node, clazz + "Hover", isHovering);
			}

			function active(isActive){
				domClass.toggle(node, clazz + "Active", isActive);
			}

			function focused(isFocused){
				domClass.toggle(node, clazz + "Focused", isFocused);
			}

			switch(evt.type){
				case "mouseover":
				case "MSPointerOver":
				case "pointerover":
					hover(true);
					break;
				case "mouseout":
				case "MSPointerOut":
				case "pointerout":
					hover(false);
					active(false);
					break;
				case "mousedown":
				case "touchstart":
				case "MSPointerDown":
				case "pointerdown":
				case "keydown":
					active(true);
					break;
				case "mouseup":
				case "MSPointerUp":
				case "pointerup":
				case "dojotouchend":
				case "keyup":
					active(false);
					break;
				case "focus":
				case "focusin":
					focused(true);
					break;
				case "blur":
				case "focusout":
					focused(false);
					break;
			}
		},

		_trackMouseState: function(/*DomNode*/ node, /*String*/ clazz){
			// summary:
			//		Track mouse/focus events on specified node and set CSS class on that node to indicate
			//		current state.   Usually not called directly, but via cssStateNodes attribute.
			// description:
			//		Given class=foo, will set the following CSS class on the node
			//
			//		- fooActive: if the user is currently pressing down the mouse button while over the node
			//		- fooHover: if the user is hovering the mouse over the node, but not pressing down a button
			//		- fooFocus: if the node is focused
			//
			//		Note that it won't set any classes if the widget is disabled.
			// node: DomNode
			//		Should be a sub-node of the widget, not the top node (this.domNode), since the top node
			//		is handled specially and automatically just by mixing in this class.
			// clazz: String
			//		CSS class name (ex: dijitSliderUpArrow)

			// Flag for listener code below to call this._cssMouseEvent() or this._subnodeCssMouseEvent()
			// when node is hovered/active
			node._cssState = clazz;
		}
	});

	domReady(function(){
		// Document level listener to catch hover etc. events on widget root nodes and subnodes.
		// Note that when the mouse is moved quickly, a single onmouseenter event could signal that multiple widgets
		// have been hovered or unhovered (try test_Accordion.html)

		function pointerHandler(evt, target, relatedTarget){
			// Handler for mouseover, mouseout, a11yclick.press and a11click.release events

			// Poor man's event propagation.  Don't propagate event to ancestors of evt.relatedTarget,
			// to avoid processing mouseout events moving from a widget's domNode to a descendant node;
			// such events shouldn't be interpreted as a mouseleave on the widget.
			if(relatedTarget && dom.isDescendant(relatedTarget, target)){
				return;
			}

			for(var node = target; node && node != relatedTarget; node = node.parentNode){
				// Process any nodes with _cssState property.   They are generally widget root nodes,
				// but could also be sub-nodes within a widget
				if(node._cssState){
					var widget = registry.getEnclosingWidget(node);
					if(widget){
						if(node == widget.domNode){
							// event on the widget's root node
							widget._cssMouseEvent(evt);
						}else{
							// event on widget's sub-node
							widget._subnodeCssMouseEvent(node, node._cssState, evt);
						}
					}
				}
			}
		}

		var body = win.body(), activeNode;

		// Handle pointer related events (i.e. mouse or touch)
		on(body, touch.over, function(evt){
			// Using touch.over rather than mouseover mainly to ignore phantom mouse events on iOS.
			pointerHandler(evt, evt.target, evt.relatedTarget);
		});
		on(body, touch.out, function(evt){
			// Using touch.out rather than mouseout mainly to ignore phantom mouse events on iOS.
			pointerHandler(evt, evt.target, evt.relatedTarget);
		});
		on(body, a11yclick.press, function(evt){
			// Save the a11yclick.press target to reference when the a11yclick.release comes.
			activeNode = evt.target;
			pointerHandler(evt, activeNode)
		});
		on(body, a11yclick.release, function(evt){
			// The release event could come on a separate node than the press event, if for example user slid finger.
			// Reference activeNode to reset the state of the node that got state set in the a11yclick.press handler.
			pointerHandler(evt, activeNode);
			activeNode = null;
		});

		// Track focus events on widget sub-nodes that have been registered via _trackMouseState().
		// However, don't track focus events on the widget root nodes, because focus is tracked via the
		// focus manager (and it's not really tracking focus, but rather tracking that focus is on one of the widget's
		// nodes or a subwidget's node or a popup node, etc.)
		// Remove for 2.0 (if focus CSS needed, just use :focus pseudo-selector).
		on(body, "focusin, focusout", function(evt){
			var node = evt.target;
			if(node._cssState && !node.getAttribute("widgetId")){
				var widget = registry.getEnclosingWidget(node);
				if(widget){
					widget._subnodeCssMouseEvent(node, node._cssState, evt);
				}
			}
		});
	});

	return CssStateMixin;
});

},
'dojox/grid/DataSelection':function(){
define([
	"dojo/_base/declare",
	"./_SelectionPreserver",
	"./Selection"
], function(declare, _SelectionPreserver, Selection){
	
return declare("dojox.grid.DataSelection", Selection, {
	constructor: function(grid){
		if(grid.keepSelection){
			this.preserver = new _SelectionPreserver(this);
		}
	},
	
	destroy: function(){
		if(this.preserver){
			this.preserver.destroy();
		}
	},
	
	getFirstSelected: function(){
		var idx = Selection.prototype.getFirstSelected.call(this);

		if(idx == -1){ return null; }
		return this.grid.getItem(idx);
	},

	getNextSelected: function(inPrev){
		var old_idx = this.grid.getItemIndex(inPrev);
		var idx = Selection.prototype.getNextSelected.call(this, old_idx);

		if(idx == -1){ return null; }
		return this.grid.getItem(idx);
	},

	getSelected: function(){
		var result = [];
		for(var i=0, l=this.selected.length; i<l; i++){
			if(this.selected[i]){
				result.push(this.grid.getItem(i));
			}
		}
		return result;
	},

	addToSelection: function(inItemOrIndex){
		if(this.mode == 'none'){ return; }
		var idx = null;
		if(typeof inItemOrIndex == "number" || typeof inItemOrIndex == "string"){
			idx = inItemOrIndex;
		}else{
			idx = this.grid.getItemIndex(inItemOrIndex);
		}
		Selection.prototype.addToSelection.call(this, idx);
	},

	deselect: function(inItemOrIndex){
		if(this.mode == 'none'){ return; }
		var idx = null;
		if(typeof inItemOrIndex == "number" || typeof inItemOrIndex == "string"){
			idx = inItemOrIndex;
		}else{
			idx = this.grid.getItemIndex(inItemOrIndex);
		}
		Selection.prototype.deselect.call(this, idx);
	},

	deselectAll: function(inItemOrIndex){
		var idx = null;
		if(inItemOrIndex || typeof inItemOrIndex == "number"){
			if(typeof inItemOrIndex == "number" || typeof inItemOrIndex == "string"){
				idx = inItemOrIndex;
			}else{
				idx = this.grid.getItemIndex(inItemOrIndex);
			}
			Selection.prototype.deselectAll.call(this, idx);
		}else{
			this.inherited(arguments);
		}
	}
});
});
},
'dojox/grid/_SelectionPreserver':function(){
define([
	"dojo/_base/declare",
	"dojo/_base/connect",
	"dojo/_base/lang",
	"dojo/_base/array"
], function(declare, connect, lang, array){

return declare("dojox.grid._SelectionPreserver", null, {
	// summary:
	//		Preserve selections across various user actions.
	//
	// description:
	//		When this feature is turned on, Grid will try to preserve selections across actions, e.g. sorting, filtering etc.
	//
	//		Precondition - Identifier(id) is required for store since id is the only way for differentiating row items.
	//		Known issue - The preserved selections might be inaccurate if some unloaded rows are previously selected by range(e.g.SHIFT + click)
	//
	// example:
	// |	//To turn on this - please set 'keepSelection' attribute to true
	// |	<div dojoType="dojox.grid.DataGrid" keepSelection = true .../>
	// |	<div dojoType="dojox.grid.TreeGrid" keepSelection = true .../>
	// |	<div dojoType="dojox.grid.LazyTreeGrid" keepSelection = true .../>
	
	constructor: function(selection){
		this.selection = selection;
		var grid = this.grid = selection.grid;
		this.reset();
		this._connects = [
			connect.connect(grid, '_setStore', this, 'reset'),
			connect.connect(grid, '_addItem', this, '_reSelectById'),
			connect.connect(selection, 'onSelected', lang.hitch(this, '_selectById', true)),
			connect.connect(selection, 'onDeselected', lang.hitch(this, '_selectById', false)),
			connect.connect(selection, 'deselectAll', this, 'reset')
		];
	},
	destroy: function(){
		this.reset();
		array.forEach(this._connects, connect.disconnect);
		delete this._connects;
	},
	reset: function(){
		this._selectedById = {};
	},
	_reSelectById: function(item, index){
		// summary:
		//		When some rows is fetched, determine whether it should be selected.
		if(item && this.grid._hasIdentity){
			this.selection.selected[index] = this._selectedById[this.grid.store.getIdentity(item)];
		}
	},
	_selectById: function(toSelect, inItemOrIndex){
		// summary:
		//		Record selected rows by ID.
		if(this.selection.mode == 'none' || !this.grid._hasIdentity){ return; }
		var item = inItemOrIndex, g = this.grid;
		if(typeof inItemOrIndex == "number" || typeof inItemOrIndex == "string"){
			var entry = g._by_idx[inItemOrIndex];
			item = entry && entry.item;
		}
		if(item){
			this._selectedById[g.store.getIdentity(item)] = !!toSelect;
		}
		return item;
	}
});
});
},
'url:dojox/grid/resources/View.html':"<div class=\"dojoxGridView\" role=\"presentation\">\n\t<div class=\"dojoxGridHeader\" dojoAttachPoint=\"headerNode\" role=\"presentation\">\n\t\t<div dojoAttachPoint=\"headerNodeContainer\" style=\"width:9000em\" role=\"presentation\">\n\t\t\t<div dojoAttachPoint=\"headerContentNode\" role=\"row\"></div>\n\t\t</div>\n\t</div>\n\t<input type=\"checkbox\" class=\"dojoxGridHiddenFocus\" dojoAttachPoint=\"hiddenFocusNode\" aria-hidden=\"true\" />\n\t<input type=\"checkbox\" class=\"dojoxGridHiddenFocus\" aria-hidden=\"true\" />\n\t<div class=\"dojoxGridScrollbox\" dojoAttachPoint=\"scrollboxNode\" role=\"presentation\">\n\t\t<div class=\"dojoxGridContent\" dojoAttachPoint=\"contentNode\" hidefocus=\"hidefocus\" role=\"presentation\"></div>\n\t</div>\n</div>\n",
'url:dijit/templates/MenuItem.html':"<tr class=\"dijitReset\" data-dojo-attach-point=\"focusNode\" role=\"menuitem\" tabIndex=\"-1\">\n\t<td class=\"dijitReset dijitMenuItemIconCell\" role=\"presentation\">\n\t\t<span role=\"presentation\" class=\"dijitInline dijitIcon dijitMenuItemIcon\" data-dojo-attach-point=\"iconNode\"></span>\n\t</td>\n\t<td class=\"dijitReset dijitMenuItemLabel\" colspan=\"2\" data-dojo-attach-point=\"containerNode,textDirNode\"\n\t\trole=\"presentation\"></td>\n\t<td class=\"dijitReset dijitMenuItemAccelKey\" style=\"display: none\" data-dojo-attach-point=\"accelKeyNode\"></td>\n\t<td class=\"dijitReset dijitMenuArrowCell\" role=\"presentation\">\n\t\t<span data-dojo-attach-point=\"arrowWrapper\" style=\"visibility: hidden\">\n\t\t\t<span class=\"dijitInline dijitIcon dijitMenuExpand\"></span>\n\t\t\t<span class=\"dijitMenuExpandA11y\">+</span>\n\t\t</span>\n\t</td>\n</tr>\n",
'url:dijit/templates/CheckedMenuItem.html':"<tr class=\"dijitReset\" data-dojo-attach-point=\"focusNode\" role=\"${role}\" tabIndex=\"-1\" aria-checked=\"${checked}\">\n\t<td class=\"dijitReset dijitMenuItemIconCell\" role=\"presentation\">\n\t\t<span class=\"dijitInline dijitIcon dijitMenuItemIcon dijitCheckedMenuItemIcon\" data-dojo-attach-point=\"iconNode\"></span>\n\t\t<span class=\"dijitMenuItemIconChar dijitCheckedMenuItemIconChar\">${!checkedChar}</span>\n\t</td>\n\t<td class=\"dijitReset dijitMenuItemLabel\" colspan=\"2\" data-dojo-attach-point=\"containerNode,labelNode,textDirNode\"></td>\n\t<td class=\"dijitReset dijitMenuItemAccelKey\" style=\"display: none\" data-dojo-attach-point=\"accelKeyNode\"></td>\n\t<td class=\"dijitReset dijitMenuArrowCell\" role=\"presentation\">&#160;</td>\n</tr>\n",
'url:dojox/grid/resources/_Grid.html':"<div hidefocus=\"hidefocus\" role=\"grid\" dojoAttachEvent=\"onmouseout:_mouseOut\">\n\t<div class=\"dojoxGridMasterHeader\" dojoAttachPoint=\"viewsHeaderNode\" role=\"presentation\"></div>\n\t<div class=\"dojoxGridMasterView\" dojoAttachPoint=\"viewsNode\" role=\"presentation\"></div>\n\t<div class=\"dojoxGridMasterMessages\" style=\"display: none;\" dojoAttachPoint=\"messagesNode\"></div>\n\t<span dojoAttachPoint=\"lastFocusNode\" tabindex=\"0\"></span>\n</div>\n",
'*now':function(r){r(['dojo/i18n!*preload*dojox/grid/nls/DataGrid*["ar","ca","cs","da","de","el","en-gb","en-us","es-es","fi-fi","fr-fr","he-il","hu","it-it","ja-jp","ko-kr","nl-nl","nb","pl","pt-br","pt-pt","ru","sk","sl","sv","th","tr","zh-tw","zh-cn","ROOT"]']);}
}});
define("dojox/grid/DataGrid", [
	"../main",
	"dojo/_base/array",
	"dojo/_base/lang",
	"dojo/_base/json",
	"dojo/_base/sniff",
	"dojo/_base/declare",
	"./_Grid",
	"./DataSelection",
	"dojo/_base/html",
	"dojo/has",
	"dojo/has!dojo-bidi?./bidi/_BidiMixin"
], function(dojox, array, lang, json, has, declare, _Grid, DataSelection, html){

/*=====
declare("dojox.grid.__DataCellDef", dojox.grid.__CellDef, {
	constructor: function(){
		// field: String?
		//		The attribute to read from the dojo.data item for the row.
		// fields: String[]?
		//		An array of fields to grab the values of and pass as an array to the grid
		// get: Function?
		//		function(rowIndex, item?){} rowIndex is of type Integer, item is of type
		//		Object.  This function will be called when a cell requests data.  Returns
		//		the unformatted data for the cell.
	}
});
=====*/

/*=====
declare("dojox.grid.__DataViewDef", dojox.grid.__ViewDef, {
	constructor: function(){
		// cells: dojox.grid.__DataCellDef[]|Array[dojox.grid.__DataCellDef[]]?
		//		The structure of the cells within this grid.
		// defaultCell: dojox.grid.__DataCellDef?
		//		A cell definition with default values for all cells in this view.  If
		//		a property is defined in a cell definition in the "cells" array and
		//		this property, the cell definition's property will override this
		//		property's property.
	}
});
=====*/

var DataGrid = declare("dojox.grid.DataGrid", _Grid, {
	store: null,
	query: null,
	queryOptions: null,
	fetchText: '...',
	sortFields: null,
	
	// updateDelay: int
	//		Time, in milliseconds, to delay updates automatically so that multiple
	//		calls to onSet/onNew/onDelete don't keep rerendering the grid.  Set
	//		to 0 to immediately cause updates.  A higher value will result in
	//		better performance at the expense of responsiveness of the grid.
	updateDelay: 1,

/*=====
	// structure: dojox.grid.__DataViewDef|dojox.grid.__DataViewDef[]|dojox.grid.__DataCellDef[]|Array[dojox.grid.__DataCellDef[]]
	//		View layout definition.
	structure: '',
=====*/

	// You can specify items instead of a query, if you like.  They do not need
	// to be loaded - but the must be items in the store
	items: null,
	
	_store_connects: null,
	_by_idty: null,
	_by_idx: null,
	_cache: null,
	_pages: null,
	_pending_requests: null,
	_bop: -1,
	_eop: -1,
	_requests: 0,
	rowCount: 0,

	_isLoaded: false,
	_isLoading: false,
	
	// keepSelection: Boolean
	//		Whether keep selection after sort, filter etc.
	keepSelection: false,	
	
	postCreate: function(){
		this._pages = [];
		this._store_connects = [];
		this._by_idty = {};
		this._by_idx = [];
		this._cache = [];
		this._pending_requests = {};

		this._setStore(this.store);
		this.inherited(arguments);
	},
	
	destroy: function(){
		this.selection.destroy();
		this.inherited(arguments);
	},

	createSelection: function(){
		this.selection = new DataSelection(this);
	},

	get: function(inRowIndex, inItem){
		// summary:
		//		Default data getter.
		// description:
		//		Provides data to display in a grid cell. Called in grid cell context.
		//		So this.cell.index is the column index.
		// inRowIndex: Integer
		//		Row for which to provide data
		// returns:
		//		Data to display for a given grid cell.
		
		if(inItem && this.field == "_item" && !this.fields){
			return inItem;
		}else if(inItem && this.fields){
			var ret = [];
			var s = this.grid.store;
			array.forEach(this.fields, function(f){
				ret = ret.concat(s.getValues(inItem, f));
			});
			return ret;
		}else if(!inItem && typeof inRowIndex === "string"){
			return this.inherited(arguments);
		}
		return (!inItem ? this.defaultValue : (!this.field ? this.value : (this.field == "_item" ? inItem : this.grid.store.getValue(inItem, this.field))));
	},

	_checkUpdateStatus: function(){
		if(this.updateDelay > 0){
			var iStarted = false;
			if(this._endUpdateDelay){
				clearTimeout(this._endUpdateDelay);
				delete this._endUpdateDelay;
				iStarted = true;
			}
			if(!this.updating){
				this.beginUpdate();
				iStarted = true;
			}
			if(iStarted){
				var _this = this;
				this._endUpdateDelay = setTimeout(function(){
					delete _this._endUpdateDelay;
					_this.endUpdate();
				}, this.updateDelay);
			}
		}
	},
	
	_onSet: function(item, attribute, oldValue, newValue){
		this._checkUpdateStatus();
		var idx = this.getItemIndex(item);
		if(idx>-1){
			this.updateRow(idx);
		}
	},
	
	_createItem: function(item, index){
		var idty = this._hasIdentity ? this.store.getIdentity(item) : json.toJson(this.query) + ":idx:" + index + ":sort:" + json.toJson(this.getSortProps());
		var o = this._by_idty[idty] = { idty: idty, item: item };
		return o;
	},

	_addItem: function(item, index, noUpdate){
		this._by_idx[index] = this._createItem(item, index);
		if(!noUpdate){
			this.updateRow(index);
		}
	},

	_onNew: function(item, parentInfo){
		this._checkUpdateStatus();
		var rowCount = this.get('rowCount');
		this._addingItem = true;
		this.updateRowCount(rowCount+1);
		this._addingItem = false;
		this._addItem(item, rowCount);
		this.showMessage();
	},

	_onDelete: function(item){
		this._checkUpdateStatus();
		var idx = this._getItemIndex(item, true);

		if(idx >= 0){
			// When a row is deleted, all rest rows are shifted down,
			// and migrate from page to page. If some page is not
			// loaded yet empty rows can migrate to initialized pages
			// without refreshing. It causes empty rows in some pages, see:
			// http://bugs.dojotoolkit.org/ticket/6818
			// this code fix this problem by reseting loaded page info
			this._pages = [];
			this._bop = -1;
			this._eop = -1;

			var o = this._by_idx[idx];
			this._by_idx.splice(idx, 1);
			delete this._by_idty[o.idty];
			this.updateRowCount(this.get('rowCount')-1);
			if(this.get('rowCount') === 0){
				this.showMessage(this.noDataMessage);
			}
		}
		if(this.selection.isSelected(idx)){
			this.selection.deselect(idx);
			this.selection.selected.splice(idx, 1);
		}
	},

	_onRevert: function(){
		this._refresh();
	},

	setStore: function(store, query, queryOptions){
		if(this._requestsPending(0)){
			return;
		}
		this._setQuery(query, queryOptions);
		this._setStore(store);
		this._refresh(true);
	},
	
	setQuery: function(query, queryOptions){
		if(this._requestsPending(0)){
			return;
		}
		this._setQuery(query, queryOptions);
		this._refresh(true);
	},
	
	setItems: function(items){
		this.items = items;
		this._setStore(this.store);
		this._refresh(true);
	},
	
	_setQuery: function(query, queryOptions){
		this.query = query;
		this.queryOptions = queryOptions || this.queryOptions;
	},

	_setStore: function(store){
		if(this.store && this._store_connects){
			array.forEach(this._store_connects, this.disconnect, this);
		}
		this.store = store;

		if(this.store){
			var f = this.store.getFeatures();
			var h = [];

			this._canEdit = !!f["dojo.data.api.Write"] && !!f["dojo.data.api.Identity"];
			this._hasIdentity = !!f["dojo.data.api.Identity"];

			if(!!f["dojo.data.api.Notification"] && !this.items){
				h.push(this.connect(this.store, "onSet", "_onSet"));
				h.push(this.connect(this.store, "onNew", "_onNew"));
				h.push(this.connect(this.store, "onDelete", "_onDelete"));
			}
			if(this._canEdit){
				h.push(this.connect(this.store, "revert", "_onRevert"));
			}

			this._store_connects = h;
		}
	},

	_onFetchBegin: function(size, req){
		if(!this.scroller){ return; }
		if(this.rowCount != size){
			if(req.isRender){
				this.scroller.init(size, this.keepRows, this.rowsPerPage);
				this.rowCount = size;
				this._setAutoHeightAttr(this.autoHeight, true);
				this._skipRowRenormalize = true;
				this.prerender();
				this._skipRowRenormalize = false;
			}else{
				this.updateRowCount(size);
			}
		}
		if(!size){
			this.views.render();
			this._resize();
			this.showMessage(this.noDataMessage);
			this.focus.initFocusView();
		}else{
			this.showMessage();
		}
	},

	_onFetchComplete: function(items, req){
		if(!this.scroller){ return; }
		if(items && items.length > 0){
			//console.log(items);
			array.forEach(items, function(item, idx){
				this._addItem(item, req.start+idx, true);
			}, this);
			this.updateRows(req.start, items.length);
			if(req.isRender){
				this.setScrollTop(0);
				this.postrender();
			}else if(this._lastScrollTop){
				this.setScrollTop(this._lastScrollTop);
			}
			if(has("ie")){
				html.setSelectable(this.domNode, this.selectable);
			}	
		}
		delete this._lastScrollTop;
		if(!this._isLoaded){
			this._isLoading = false;
			this._isLoaded = true;
		}
		this._pending_requests[req.start] = false;
	},

	_onFetchError: function(err, req){
		console.log(err);
		delete this._lastScrollTop;
		if(!this._isLoaded){
			this._isLoading = false;
			this._isLoaded = true;
			this.showMessage(this.errorMessage);
		}
		this._pending_requests[req.start] = false;
		this.onFetchError(err, req);
	},

	onFetchError: function(err, req){
	},

	_fetch: function(start, isRender){
		start = start || 0;
		if(this.store && !this._pending_requests[start]){
			if(!this._isLoaded && !this._isLoading){
				this._isLoading = true;
				this.showMessage(this.loadingMessage);
			}
			this._pending_requests[start] = true;
			//console.log("fetch: ", start);
			try{
				if(this.items){
					var items = this.items;
					var store = this.store;
					this.rowsPerPage = items.length;
					var req = {
						start: start,
						count: this.rowsPerPage,
						isRender: isRender
					};
					this._onFetchBegin(items.length, req);
					
					// Load them if we need to
					var waitCount = 0;
					array.forEach(items, function(i){
						if(!store.isItemLoaded(i)){ waitCount++; }
					});
					if(waitCount === 0){
						this._onFetchComplete(items, req);
					}else{
						var onItem = function(item){
							waitCount--;
							if(waitCount === 0){
								this._onFetchComplete(items, req);
							}
						};
						array.forEach(items, function(i){
							if(!store.isItemLoaded(i)){
								store.loadItem({item: i, onItem: onItem, scope: this});
							}
						}, this);
					}
				}else{
					this.store.fetch({
						start: start,
						count: this.rowsPerPage,
						query: this.query,
						sort: this.getSortProps(),
						queryOptions: this.queryOptions,
						isRender: isRender,
						onBegin: lang.hitch(this, "_onFetchBegin"),
						onComplete: lang.hitch(this, "_onFetchComplete"),
						onError: lang.hitch(this, "_onFetchError")
					});
				}
			}catch(e){
				this._onFetchError(e, {start: start, count: this.rowsPerPage});
			}
		}
	},

	_clearData: function(){
		this.updateRowCount(0);
		this._by_idty = {};
		this._by_idx = [];
		this._pages = [];
		this._bop = this._eop = -1;
		this._isLoaded = false;
		this._isLoading = false;
	},

	getItem: function(idx){
		var data = this._by_idx[idx];
		if(!data||(data&&!data.item)){
			this._preparePage(idx);
			return null;
		}
		return data.item;
	},

	getItemIndex: function(item){
		return this._getItemIndex(item, false);
	},
	
	_getItemIndex: function(item, isDeleted){
		if(!isDeleted && !this.store.isItem(item)){
			return -1;
		}

		var idty = this._hasIdentity ? this.store.getIdentity(item) : null;

		for(var i=0, l=this._by_idx.length; i<l; i++){
			var d = this._by_idx[i];
			if(d && ((idty && d.idty == idty) || (d.item === item))){
				return i;
			}
		}
		return -1;
	},

	filter: function(query, reRender){
		this.query = query;
		if(reRender){
			this._clearData();
		}
		this._fetch();
	},

	_getItemAttr: function(idx, attr){
		var item = this.getItem(idx);
		return (!item ? this.fetchText : this.store.getValue(item, attr));
	},

	// rendering
	_render: function(){
		if(this.domNode.parentNode){
			this.scroller.init(this.get('rowCount'), this.keepRows, this.rowsPerPage);
			this.prerender();
			this._fetch(0, true);
		}
	},

	// paging
	_requestsPending: function(inRowIndex){
		return this._pending_requests[inRowIndex];
	},

	_rowToPage: function(inRowIndex){
		return (this.rowsPerPage ? Math.floor(inRowIndex / this.rowsPerPage) : inRowIndex);
	},

	_pageToRow: function(inPageIndex){
		return (this.rowsPerPage ? this.rowsPerPage * inPageIndex : inPageIndex);
	},

	_preparePage: function(inRowIndex){
		if((inRowIndex < this._bop || inRowIndex >= this._eop) && !this._addingItem){
			var pageIndex = this._rowToPage(inRowIndex);
			this._needPage(pageIndex);
			this._bop = pageIndex * this.rowsPerPage;
			this._eop = this._bop + (this.rowsPerPage || this.get('rowCount'));
		}
	},

	_needPage: function(inPageIndex){
		if(!this._pages[inPageIndex]){
			this._pages[inPageIndex] = true;
			this._requestPage(inPageIndex);
		}
	},

	_requestPage: function(inPageIndex){
		var row = this._pageToRow(inPageIndex);
		var count = Math.min(this.rowsPerPage, this.get('rowCount') - row);
		if(count > 0){
			this._requests++;
			if(!this._requestsPending(row)){
				setTimeout(lang.hitch(this, "_fetch", row, false), 1);
				//this.requestRows(row, count);
			}
		}
	},

	getCellName: function(inCell){
		return inCell.field;
		//console.log(inCell);
	},

	_refresh: function(isRender){
		this._clearData();
		this._fetch(0, isRender);
	},

	sort: function(){
		this.edit.apply();
		this._lastScrollTop = this.scrollTop;
		this._refresh();
	},

	canSort: function(){
		return (!this._isLoading);
	},

	getSortProps: function(){
		var c = this.getCell(this.getSortIndex());
		if(!c){
			if(this.sortFields){
				return this.sortFields;
			}
			return null;
		}else{
			var desc = c["sortDesc"];
			var si = !(this.sortInfo>0);
			if(typeof desc == "undefined"){
				desc = si;
			}else{
				desc = si ? !desc : desc;
			}
			return [{ attribute: c.field, descending: desc }];
		}
	},

	styleRowState: function(inRow){
		// summary:
		//		Perform row styling
		if(this.store && this.store.getState){
			var states=this.store.getState(inRow.index), c='';
			for(var i=0, ss=["inflight", "error", "inserting"], s; s=ss[i]; i++){
				if(states[s]){
					c = ' dojoxGridRow-' + s;
					break;
				}
			}
			inRow.customClasses += c;
		}
	},

	onStyleRow: function(inRow){
		this.styleRowState(inRow);
		this.inherited(arguments);
	},

	// editing
	canEdit: function(inCell, inRowIndex){
		return this._canEdit;
	},

	_copyAttr: function(idx, attr){
		var row = {};
		var backstop = {};
		var src = this.getItem(idx);
		return this.store.getValue(src, attr);
	},

	doStartEdit: function(inCell, inRowIndex){
		if(!this._cache[inRowIndex]){
			this._cache[inRowIndex] = this._copyAttr(inRowIndex, inCell.field);
		}
		this.onStartEdit(inCell, inRowIndex);
	},

	doApplyCellEdit: function(inValue, inRowIndex, inAttrName){
		this.store.fetchItemByIdentity({
			identity: this._by_idx[inRowIndex].idty,
			onItem: lang.hitch(this, function(item){
				var oldValue = this.store.getValue(item, inAttrName);
				if(typeof oldValue == 'number'){
					inValue = isNaN(inValue) ? inValue : parseFloat(inValue);
				}else if(typeof oldValue == 'boolean'){
					inValue = inValue == 'true' ? true : inValue == 'false' ? false : inValue;
				}else if(oldValue instanceof Date){
					var asDate = new Date(inValue);
					inValue = isNaN(asDate.getTime()) ? inValue : asDate;
				}
				this.store.setValue(item, inAttrName, inValue);
				this.onApplyCellEdit(inValue, inRowIndex, inAttrName);
			})
		});
	},

	doCancelEdit: function(inRowIndex){
		var cache = this._cache[inRowIndex];
		if(cache){
			this.updateRow(inRowIndex);
			delete this._cache[inRowIndex];
		}
		this.onCancelEdit.apply(this, arguments);
	},

	doApplyEdit: function(inRowIndex, inDataAttr){
		var cache = this._cache[inRowIndex];
		/*if(cache){
			var data = this.getItem(inRowIndex);
			if(this.store.getValue(data, inDataAttr) != cache){
				this.update(cache, data, inRowIndex);
			}
			delete this._cache[inRowIndex];
		}*/
		this.onApplyEdit(inRowIndex);
	},

	removeSelectedRows: function(){
		// summary:
		//		Remove the selected rows from the grid.
		if(this._canEdit){
			this.edit.apply();
			var fx = lang.hitch(this, function(items){
				if(items.length){
					array.forEach(items, this.store.deleteItem, this.store);
					this.selection.clear();
				}
			});
			if(this.allItemsSelected){
				this.store.fetch({
							query: this.query,
							queryOptions: this.queryOptions,
							onComplete: fx});
			}else{
				fx(this.selection.getSelected());
			}
		}
	}
});

DataGrid.cell_markupFactory = function(cellFunc, node, cellDef){
	var field = lang.trim(html.attr(node, "field")||"");
	if(field){
		cellDef.field = field;
	}
	cellDef.field = cellDef.field||cellDef.name;
	var fields = lang.trim(html.attr(node, "fields")||"");
	if(fields){
		cellDef.fields = fields.split(",");
	}
	if(cellFunc){
		cellFunc(node, cellDef);
	}
};

DataGrid.markupFactory = function(props, node, ctor, cellFunc){
	return _Grid.markupFactory(props, node, ctor,
					lang.partial(DataGrid.cell_markupFactory, cellFunc));
};

return DataGrid;

});
