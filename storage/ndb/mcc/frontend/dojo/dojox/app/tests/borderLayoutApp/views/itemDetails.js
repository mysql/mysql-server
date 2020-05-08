define([],
function(){

	var listStore = null;	//list data
	var currentItem = null;

	return {
		// show an item detail
		setDetailsContext: function(index){
			// only set the cursor if it is different and valid
			if(parseInt(index) < listStore.data.length){
				currentItem = listStore.data[index];
				this.First.set("value",currentItem.First);
				this.Last.set("value",currentItem.Last);
				this.Email.set("value",currentItem.Email);
				this.Tel.set("value",currentItem.Tel);
			}
		},

		// list view init
		init: function(){
			listStore = this.loadedStores.listStore;
		},

		beforeActivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//
			// if this.params["cursor"] is set use it to set the selected Details Context
			if(this.params["cursor"] || this.params["cursor"] == 0){
				this.setDetailsContext(this.params["cursor"]);
			}
		},

		beforeDeactivate: function(){
			// summary:
			//		view life cycle beforeDeactivate()
			//
			// put any updates back to the store
			currentItem.label = this.First.get("value") + " " + this.Last.get("value");
			currentItem.First = this.First.get("value");
			currentItem.Last = this.Last.get("value");
			currentItem.Email = this.Email.get("value");
			currentItem.Tel = this.Tel.get("value");
			listStore.put(currentItem);
		}
	}
});
