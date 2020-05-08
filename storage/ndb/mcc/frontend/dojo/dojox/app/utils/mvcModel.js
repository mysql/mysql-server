define(["dojo/_base/lang", "dojo/Deferred", "dojo/when", "dojox/mvc/getStateful"],
function(lang, Deferred, when, getStateful){
	return function(/*Object*/config, /*Object*/params, /*String*/item){
		// summary:
		//		mvcModel is called for each mvc model, to create the mvc model based upon the type and params.
		//		It will also load models and return the either the loadedModels or a promise.
		// description:
		//		Called for each model with a modelLoader of "dojox/app/utils/mvcModel", it will
		//		create the model based upon the type and the params set for the model in the config.
		// config: Object
		//		The models section of the config for this view or for the app.
		// params: Object
		//		The params set into the config for this model.
		// item: String
		//		The String with the name of this model
		// returns: model 
		//		The model, of the type specified in the config for this model.
		var loadedModels = {};
		var loadMvcModelDeferred = new Deferred();

		var fixupQuery = function(query){
			var ops = {};
			for(var item in query){ // need this to handle query params without errors
				if(item.charAt(0) !== "_"){
					ops[item] = query[item];
				}
			}
			return(ops);
		};

		var options;
		if(params.store){
			//	if query is not set on the model params, it may be set on the store
			options = {
				"store": params.store.store,
				"query": params.query ? fixupQuery(params.query) : params.store.query ? fixupQuery(params.store.query) : {}
			};
		}else if(params.datastore){
			try{
				var dataStoreCtor = require("dojo/store/DataStore");
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
				//get the object specified by string value of data property
				//cannot assign object literal or reference to data property
				//because json.ref will generate __parent to point to its parent
				//and will cause infinitive loop when creating StatefulModel.
				params.data = lang.getObject(params.data);
			}
			options = {"data": params.data, query: {}};
		}
		else{
			console.warn("mvcModel: Missing parameters.");
		}

		var type = config[item].type ? config[item].type : "dojox/mvc/EditStoreRefListController";
		// need to load the class to use for the model
		// modelLoader must be listed in the dependencies and has thus already been loaded so it _must_ be here
		// => no need for complex code here
		try{
			var modelCtor = require(type);
		}catch(e){
			throw new Error(type+" must be listed in the dependencies");
		}
		var newModel = new modelCtor(options);
		var createMvcPromise;
		try{
			if(newModel.queryStore){
				createMvcPromise = newModel.queryStore(options.query);
			}else{
				var modelProp = newModel._refSourceModelProp || newModel._refModelProp || "model";
				newModel.set(modelProp, getStateful(options.data));
				createMvcPromise = newModel;
			}
		}catch(ex){
			//console.warn("load mvc model error.", ex);
			loadMvcModelDeferred.reject("load mvc model error.");
			return loadMvcModelDeferred.promise;
		}
		when(createMvcPromise, lang.hitch(this, function(){
			// now the loadedModels[item].models is set.
			loadedModels = newModel;
			loadMvcModelDeferred.resolve(loadedModels);
			//this.app.log("in mvcModel promise path, loadedModels = ", loadedModels);
			return loadedModels;
		}), function(){
			loadMvcModelDeferred.reject("load model error.")
		});
		return loadMvcModelDeferred;
	}
});
