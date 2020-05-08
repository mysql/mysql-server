define([
"dojo/_base/array",
"dojo/_base/declare",
"dojo/_base/event",
"dojo/_base/lang",
"dojo/dom-geometry",
"dojo/dom-style",
"./MatrixView",
"dojo/text!./templates/ColumnViewSecondarySheet.html"],

function(arr,
		declare,
		event,
		lang,
		domGeometry,
		domStyle,
		MatrixView,
		template){

	return declare("dojox.calendar.ColumnViewSecondarySheet", MatrixView, {

		// summary:
		//		This class defines a matrix view designed to be embedded in a column view,
		//		usually to display long or all day events on one row.

		templateString: template,

		rowCount: 1,

		cellPaddingTop: 4,

		roundToDay: false,

		_defaultHeight: -1,

		layoutDuringResize: true,

		buildRendering: function(){
			this.inherited(arguments);
			this._hScrollNodes = [this.gridTable, this.itemContainerTable];
		},

		_configureHScrollDomNodes: function(styleWidth){
			arr.forEach(this._hScrollNodes, function(elt){
				domStyle.set(elt, "width", styleWidth);
			}, this);
		},

		_defaultItemToRendererKindFunc: function(item){
			// tags:
			//		private
			return item.allDay ? "horizontal" : null;
		},

		_formatGridCellLabel: function(){return null;},

		_formatRowHeaderLabel: function(){return null;},


		// events redispatch
		__fixEvt:function(e){
			e.sheet = "secondary";
			e.source = this;
			return e;
		},

		_dispatchCalendarEvt: function(e, name){
			e = this.inherited(arguments);
			if(this.owner.owner){ // the calendar
				this.owner.owner[name](e);
			}
		},

		_layoutExpandRenderers: function(index, hasHiddenItems, hiddenItems){
			if(!this.expandRenderer || this._expandedRowCol == -1){
				return;
			}
			var h = domGeometry.getMarginBox(this.domNode).h;
			if(this._defaultHeight == -1 ||  // not set
				this._defaultHeight === 0){  // initialized at 0, must be reset
				this._defaultHeight = h;
			}

			if(this._defaultHeight != h && h >= this._getExpandedHeight() ||
				this._expandedRowCol !== undefined && this._expandedRowCol !== -1){
				var col = this._expandedRowCol;
				if(col >= this.renderData.columnCount){
					col = 0;
				}
				this._layoutExpandRendererImpl(0, col, null, true);
			}else{
				this.inherited(arguments);
			}
		},

		expandRendererClickHandler: function(e, renderer){
			// summary:
			//		Default action when an expand renderer is clicked.
			//		This method will expand the secondary sheet to show all the events.
			// e: Event
			//		The mouse event.
			// renderer: Object
			//		The renderer that was clicked.
			// tags:
			//		callback

			event.stop(e);

			var h = domGeometry.getMarginBox(this.domNode).h;
			var expandedH = this._getExpandedHeight();
			if(this._defaultHeight == h || h < expandedH){
				this._expandedRowCol = renderer.columnIndex;
				this.owner.resizeSecondarySheet(expandedH);
			}else{
				delete this._expandedRowCol;
				this.owner.resizeSecondarySheet(this._defaultHeight);
			}
		},


		_getExpandedHeight: function(){
			// tags:
			//		private

			return (this.naturalRowsHeight && this.naturalRowsHeight.length > 0 ? this.naturalRowsHeight[0] : 0) +
				this.expandRendererHeight + this.verticalGap + this.verticalGap;
		},

		_layoutRenderers: function(renderData){
			if(!this._domReady){
				return;
			}
			this.inherited(arguments);
			// make sure to show the expand/collapse renderer if no item is displayed but the row was expanded.
			if(!renderData.items || renderData.items.length === 0){
				this._layoutExpandRenderers(0, false, null);
			}
		}

	});
});
