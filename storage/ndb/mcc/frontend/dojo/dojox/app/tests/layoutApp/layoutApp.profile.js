var profile = {
	resourceTags:{
		declarative: function(filename){
	 		return /\.html?$/.test(filename); // tags any .html or .htm files as declarative
	 	},
		amd: function(filename){
			return /\.js$/.test(filename);
		},
		copyOnly: function(filename, mid){
			return mid == "layoutApp/build.profile";
		}
	}
};
