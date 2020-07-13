define("dojox/app/utils/simpleModel", ["dojo/_base/lang", "dojo/Deferred", "dojo/when"],
function(lang, Deferred, when){
	return function(/*Object*/config, /*Object*/params, /*String*/item){
		// summary:
		//		simpleModel is called for each simple model, to create the simple model from the DataStore 
		//		based upon the store and query params.
		//		It will also load models and return the either the loadedModels or a promise.
		// description:
		//		Called for each model with a modelLoader of "dojox/app/utils/simpleModel", it will
		//		create the model based upon the store and query params set for the model in the config.
		// config: Object
		//		The models section of the config for this view or for the app.
		// params: Object
		//		The params set into the config for this model.
		// item: String
		//		The String with the name of this model
		// returns: model 
		//		 The model, of the store and query params specified in the config for this model.
		var loadedModels = {};
		var loadSimpleModelDeferred = new Deferred();

		var fixupQuery = function(query){
			var ops = {};
			for(var item in query){ // need this to handle query params without errors
				if(item.charAt(0) !== "_"){
					ops[item] = query[item];
				}
			}
			return(ops);
		};

		var options, dataStoreCtor;
		if(params.store){
			if(!params.store.params){
				throw new Error("Invalid store for model ["+item+"]");
			}else if((params.store.params.data || params.store.params.store)){
				options = {
					"store": params.store.store,
					"query": params.query ? fixupQuery(params.query) : params.store.query ? fixupQuery(params.store.query) : {}
				};
			}else if(params.store.params.url){
				try{
					dataStoreCtor = require("dojo/store/DataStore");
				}catch(e){
					throw new Error("dojo/store/DataStore must be listed in the dependencies");
				}
				options = {
					"store": new dataStoreCtor({
						store: params.store.store
					}),
					"query": params.query ? fixupQuery(params.query) : params.store.query ? fixupQuery(params.store.query) : {}
				};
			} else if(params.store.store){
				//	if query is not set on the model params, it may be set on the store
				options = {
					"store": params.store.store,
					"query": params.query ? fixupQuery(params.query) : params.store.query ? fixupQuery(params.store.query) : {}
				};
			}
		}else if(params.datastore){
			try{
				dataStoreCtor = require("dojo/store/DataStore");
			}catch(e){
				throw new Error("When using datastore the dojo/store/DataStore module must be listed in the dependencies");
			}
			options = {
				"store": new dataStoreCtor({
					store: params.datastore.store
				}),
				"query": fixupQuery(params.query)
			};
		}else if(params.data){
			if(params.data && lang.isString(params.data)){
				params.data = lang.getObject(params.data);
			}
			options = {"data": params.data, query: {}};
		} else{
			console.warn("simpleModel: Missing parameters.");
		}
		var createSimplePromise;
		try{
			if(options.store){
				createSimplePromise = options.store.query();
			}else{
				createSimplePromise = options.data;
			}
		}catch(ex){
			loadSimpleModelDeferred.reject("load simple model error.");
			return loadSimpleModelDeferred.promise;
		}
		if(createSimplePromise.then){
			when(createSimplePromise, lang.hitch(this, function(newModel){
				// now the loadedModels[item].models is set.
				//console.log("in simpleModel promise path, loadedModels = ", loadedModels);
				loadedModels = newModel;
				loadSimpleModelDeferred.resolve(loadedModels);
				return loadedModels;
			}), function(){
				loadModelLoaderDeferred.reject("load model error.")
			});
		}else{ // query did not return a promise, so use newModel
			loadedModels = createSimplePromise;
			//console.log("in simpleModel else path, loadedModels = ",loadedModels);
			loadSimpleModelDeferred.resolve(loadedModels);
			return loadedModels;
		}
			
		return loadSimpleModelDeferred;
	}
});
