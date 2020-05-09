define(["../buildControl", "require"], function(bc, require){
	var optimizers = {};

	function resolveComments(optimizer) {
		// This is for back-compat of comments and comments.keepLines,
		// after the refactor to separate optimizers placed this logic in shrinksafe.
		// TODO: remove @ 2.0 (along with shrinksafe entirely, perhaps)
		return /^comments/.test(optimizer) ? "shrinksafe." + optimizer : optimizer;
	}

	if(bc.optimize){
		bc.optimize = resolveComments(bc.optimize);
		require(["./optimizer/" + bc.optimize.split(".")[0]], function(optimizer){
			optimizers[bc.optimize] = optimizer;
		});
	}
	if(bc.layerOptimize){
		bc.layerOptimize = resolveComments(bc.layerOptimize);
		require(["./optimizer/" + bc.layerOptimize.split(".")[0]], function(optimizer){
			optimizers[bc.layerOptimize] = optimizer;
		});
	}

	return function(resource, callback){
		var copyright = resource.pack ? resource.pack.copyright : bc.copyright;
		if(bc.optimize && !resource.layer){
			return optimizers[bc.optimize](resource, resource.uncompressedText, copyright, bc.optimize, callback);
		}else if(bc.layerOptimize && resource.layer && !resource.layer.discard){
			return optimizers[bc.layerOptimize](resource, resource.uncompressedText, copyright, bc.layerOptimize, callback);
		}else{
			return 0;
		}
	};
});
