define(["dojo/_base/lang"], function(lang){

// module:
//		dojox/app/utils/hash

var hashUtil = {
	// summary:
	//		This module contains the hash

		getParams: function(/*String*/ hash){
			// summary:
			//		get the params from the hash
			//
			// hash: String
			//		the url hash
			//
			// returns:
			//		the params object
			//
			var params;
			if(hash && hash.length){
				// fixed handle view specific params
				
				while(hash.indexOf("(") > 0){ 
					var index = hash.indexOf("(");
					var endindex = hash.indexOf(")");
					var viewPart = hash.substring(index,endindex+1);
					if(!params){ params = {}; }
					params = hashUtil.getParamObj(params, viewPart);
					// next need to remove the viewPart from the hash, and look for the next one
					var viewName = viewPart.substring(1,viewPart.indexOf("&"));
					hash = hash.replace(viewPart, viewName);
				}	
				// after all of the viewParts need to get the other params	

				for(var parts = hash.split("&"), x = 0; x < parts.length; x++){
					var tp = parts[x].split("="), name = tp[0], value = encodeURIComponent(tp[1] || "");
					if(name && value){
						if(!params){ params = {}; }
						params[name] = value;
					}
				}
			}
			return params; // Object
		},

		getParamObj: function(/*Object*/ params, /*String*/ viewPart){
			// summary:
			//		called to handle a view specific params object
			// params: Object
			//		the view specific params object
			// viewPart: String
			//		the part of the view with the params for the view
			//
			// returns:
	 		//		the params object for the view
			//
			var viewparams;
			var viewName = viewPart.substring(1,viewPart.indexOf("&"));
			var hash = viewPart.substring(viewPart.indexOf("&"), viewPart.length-1);
				for(var parts = hash.split("&"), x = 0; x < parts.length; x++){
					var tp = parts[x].split("="), name = tp[0], value = encodeURIComponent(tp[1] || "");
					if(name && value){
						if(!viewparams){ viewparams = {}; }
						viewparams[name] = value;
					}
				}
			params[viewName] = 	viewparams;
			return params; // Object
		},

		buildWithParams: function(/*String*/ hash, /*Object*/ params){
			// summary:
			//		build up the url hash adding the params
			// hash: String
			//		the url hash
			// params: Object
			//		the params object
			//
			// returns:
	 		//		the params object
			//
			if(hash.charAt(0) !== "#"){
				hash = "#"+hash;
			}
			for(var item in params){
				var value = params[item];
				// add a check to see if the params includes a view name if so setup the hash like (viewName&item=value);
				if(lang.isObject(value)){
					hash = hashUtil.addViewParams(hash, item, value);
				}else{
					if(item && value != null){
						hash = hash+"&"+item+"="+params[item];
					}
				}
			}
			return hash; // String
		},

		addViewParams: function(/*String*/ hash, /*String*/ view, /*Object*/ params){
			// summary:
			//		add the view specific params to the hash for example (view1&param1=value1)
			// hash: String
			//		the url hash
			// view: String
			//		the view name
			// params: Object
			//		the params for this view
			//
			// returns:
			//		the hash string
			//
			if(hash.charAt(0) !== "#"){
				hash = "#"+hash;
			}
			var index = hash.indexOf(view);
			if(index > 0){ // found the view?
				if((hash.charAt(index-1) == "#" || hash.charAt(index-1) == "+") && // assume it is the view? or could check the char after for + or & or -
					(hash.charAt(index+view.length) == "&" || hash.charAt(index+view.length) == "+" || hash.charAt(index+view.length) == "-")){
					// found the view at this index.
					var oldView = hash.substring(index-1,index+view.length+1);
					var paramString = hashUtil.getParamString(params);
					var newView = hash.charAt(index-1) + "(" + view + paramString + ")" + hash.charAt(index+view.length);
					hash = hash.replace(oldView, newView);
				}
			}
			
			return hash; // String
		},

		getParamString: function(/*Object*/ params){
			// summary:
			//		return the param string
			// params: Object
			//		the params object
			//
			// returns:
			//		the params string
			//
			var paramStr = "";
			for(var item in params){
				var value = params[item];
				if(item && value != null){
					paramStr = paramStr+"&"+item+"="+params[item];
				}
			}
			return paramStr; // String
		},

		getTarget: function(/*String*/ hash, /*String?*/ defaultView){
			// summary:
			//		return the target string
			// hash: String
			//		the hash string
			// defaultView: String
			//		the optional defaultView string
			//
			// returns:
			//		the target string
			//
			if(!defaultView){ defaultView = ""}
			while(hash.indexOf("(") > 0){ 
				var index = hash.indexOf("(");
				var endindex = hash.indexOf(")");
				var viewPart = hash.substring(index,endindex+1);
				var viewName = viewPart.substring(1,viewPart.indexOf("&"));
				hash = hash.replace(viewPart, viewName);
			}	
			
			return (((hash && hash.charAt(0) == "#") ? hash.substr(1) : hash) || defaultView).split('&')[0];	// String
		}
};

return hashUtil;

});