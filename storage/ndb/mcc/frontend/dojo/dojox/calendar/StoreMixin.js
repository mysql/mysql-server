define(["dojo/_base/declare", "dojo/_base/array", "dojo/_base/html", "dojo/_base/lang", "dojo/dom-class",
	"dojo/Stateful", "dojo/when"],
	function(declare, arr, html, lang, domClass, Stateful, when){

	return declare("dojox.calendar.StoreMixin", Stateful, {

		// summary:
		//		This mixin contains the store management.

		// store: dojo.store.Store
		//		The store that contains the events to display.
		store: null,

		// query: Object
		//		A query that can be passed to when querying the store.
		query: {},

		// queryOptions: dojo/store/api/Store.QueryOptions?
		//		Options to be applied when querying the store.
		queryOptions: null,

		// startTimeAttr: String
		//		The attribute of the store item that contains the start time of
		//		the events represented by this item.	Default is "startTime".
		startTimeAttr: "startTime",

		// endTimeAttr: String
		//		The attribute of the store item that contains the end time of
		//		the events represented by this item.	Default is "endTime".
		endTimeAttr: "endTime",

		// summaryAttr: String
		//		The attribute of the store item that contains the summary of
		//		the events represented by this item.	Default is "summary".
		summaryAttr: "summary",

		// allDayAttr: String
		//		The attribute of the store item that contains the all day state of
		//		the events represented by this item.	Default is "allDay".
		allDayAttr: "allDay",

		// subColumnAttr: String
		//		The attribute of the store item that contains the sub column name of
		//		the events represented by this item.	Default is "calendar".
		subColumnAttr: "calendar",

		// cssClassFunc: Function
		//		Optional function that returns a css class name to apply to item renderers that are displaying the specified item in parameter.
		cssClassFunc: null,

		// decodeDate: Function?
		//		An optional function to transform store date into Date objects.	Default is null.
		decodeDate: null,

		// encodeDate: Function?
		//		An optional function to transform Date objects into store date.	Default is null.
		encodeDate: null,

		// displayedItemsInvalidated: Boolean
		//		Whether the data items displayed must be recomputed, usually after the displayed
		//		time range has changed.
		// tags:
		//		protected
		displayedItemsInvalidated: false,

		itemToRenderItem: function(item, store){
			// summary:
			//		Creates the render item based on the dojo.store item. It must be of the form:
			//	|	{
			//  |		id: Object,
			//	|		startTime: Date,
			//	|		endTime: Date,
			//	|		summary: String
			//	|	}
			//		By default it is building an object using the store id, the summaryAttr,
			//		startTimeAttr and endTimeAttr properties as well as decodeDate property if not null.
			//		Other fields or way to query fields can be used if needed.
			// item: Object
			//		The store item.
			// store: dojo.store.api.Store
			//		The store.
			// returns: Object
			if(this.owner){
				return this.owner.itemToRenderItem(item, store);
			}
			return {
				id: store.getIdentity(item),
				summary: item[this.summaryAttr],
				startTime: (this.decodeDate && this.decodeDate(item[this.startTimeAttr])) || this.newDate(item[this.startTimeAttr], this.dateClassObj),
				endTime: (this.decodeDate && this.decodeDate(item[this.endTimeAttr])) || this.newDate(item[this.endTimeAttr], this.dateClassObj),
				allDay: item[this.allDayAttr] != null ? item[this.allDayAttr] : false,
				subColumn: item[this.subColumnAttr],
				cssClass: this.cssClassFunc ? this.cssClassFunc(item) : null
			};
		},

		renderItemToItem: function(/*Object*/ renderItem, /*dojo.store.api.Store*/ store){
			// summary:
			//		Create a store item based on the render item. It must be of the form:
			//	|	{
			//	|		id: Object
			//	|		startTime: Date,
			//	|		endTime: Date,
			//	|		summary: String
			//	|	}
			//		By default it is building an object using the summaryAttr, startTimeAttr and endTimeAttr properties
			//		and encodeDate property if not null. If the encodeDate property is null a Date object will be set in the start and end time.
			//		When using a JsonRest store, for example, it is recommended to transfer dates using the ISO format (see dojo.date.stamp).
			//		In that case, provide a custom function to the encodeDate property that is using the date ISO encoding provided by Dojo.
			// renderItem: Object
			//		The render item.
			// store: dojo.store.api.Store
			//		The store.
			// returns:Object
			if(this.owner){
				return this.owner.renderItemToItem(renderItem, store);
			}
			var item = {};
			item[store.idProperty] = renderItem.id;
			item[this.summaryAttr] = renderItem.summary;
			item[this.startTimeAttr] = (this.encodeDate && this.encodeDate(renderItem.startTime)) || renderItem.startTime;
			item[this.endTimeAttr] = (this.encodeDate && this.encodeDate(renderItem.endTime)) || renderItem.endTime;
			if(renderItem.subColumn){
				item[this.subColumnAttr] = renderItem.subColumn;
			}
			return this.getItemStoreState(renderItem) === "unstored" ? item : lang.mixin(renderItem._item, item);
		},

		_computeVisibleItems: function(renderData){
			// summary:
			//		Computes the data items that are in the displayed interval.
			// renderData: Object
			//		The renderData that contains the start and end time of the displayed interval.
			// tags:
			//		protected

			if(this.owner){
				return this.owner._computeVisibleItems(renderData);
			}
			renderData.items = this.storeManager._computeVisibleItems(renderData);
		},

		_initItems: function(items){
			// tags:
			//		private
			this.set("items", items);
			return items;
		},

		_refreshItemsRendering: function(renderData){
		},

		_setStoreAttr: function(value){
			this.store = value;
			return this.storeManager.set("store", value);
		},

		_getItemStoreStateObj: function(/*Object*/item){
			// tags
			//		private
			return this.storeManager._getItemStoreStateObj(item);
		},

		getItemStoreState: function(item){
			//	summary:
			//		Returns the creation state of an item.
			//		This state is changing during the interactive creation of an item.
			//		Valid values are:
			//		- "unstored": The event is being interactively created. It is not in the store yet.
			//		- "storing": The creation gesture has ended, the event is being added to the store.
			//		- "stored": The event is not in the two previous states, and is assumed to be in the store
			//		(not checking because of performance reasons, use store API for testing existence in store).
			// item: Object
			//		The item.
			// returns: String

			return this.storeManager.getItemStoreState(item);
		},

		_cleanItemStoreState: function(id){
			this.storeManager._cleanItemStoreState(id);
		},

		_setItemStoreState: function(/*Object*/item, /*String*/state){
			// tags
			//		private
			this.storeManager._setItemStoreState(item, state);
		}

	});

});
