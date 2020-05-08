define([
	"../buildControl",
	"../fileUtils",
	"../fs",
	"dojo/_base/lang",
	"dojo/json"
], function(bc, fileUtils, fs, lang, json){
	var
		computingLayers
			// the set of layers being computed; use this to detect circular layer dependencies
			= {},

		computeLayerContents = function(
			layerModule,
			include,
			exclude
		){
			// add property layerSet (a set of mid) to layerModule that...
			//
			//	 * includes the layerModule itself
			//	 * includes dependency tree of layerModule
			//	 * includes all modules in layerInclude and their dependency trees
			//	 * excludes all modules in layerExclude and their dependency trees
			//
			// note: layerSet is built exactly as given above, so included modules that are later excluded
			// are *not* in result layerSet

			if(computingLayers[layerModule.mid]){
				bc.log("amdCircularDependency", ["module", layerModule.mid]);
				return {};
			}
			computingLayers[layerModule.mid] = 1;

			var
				includeSet = {},
				visited,
				includePhase,
				traverse = function(module){
					var mid = module.mid;

					if(visited[mid]){
						return;
					}
					visited[mid] = 1;
					if(includePhase){
						includeSet[mid] = module;
					}else{
						delete includeSet[mid];
					}
					if(module!==layerModule && module.layer){
						var layerModuleSet = module.moduleSet || computeLayerContents(module, module.layer.include, module.layer.exclude);
						for(var p in layerModuleSet){
							if(includePhase){
								includeSet[p] = layerModuleSet[p];
							}else{
								delete includeSet[p];
							}
						}
					}else{
						for(var deps = module.deps, i = 0; deps && i<deps.length; traverse(deps[i++])){
						}
					}
				};

			visited = {};
			includePhase = true;
			traverse(layerModule);
			include.forEach(function(mid){
				var module = bc.amdResources[bc.getSrcModuleInfo(mid, layerModule).mid];
				if(!module){
					bc.log("amdMissingLayerIncludeModule", ["missing", mid, "layer", layerModule.mid]);
				}else{
					traverse(module);
				}
			});

			visited = {};
			includePhase = false;
			exclude.forEach(function(mid){
				var module = bc.amdResources[bc.getSrcModuleInfo(mid, layerModule).mid];
				if(!module){
					bc.log("amdMissingLayerExcludeModule", ["missing", mid, "layer", layerModule.mid]);
				}else{
					traverse(module);
				}
			});

			layerModule.moduleSet = includeSet;
			delete computingLayers[layerModule.mid];

			// return a copy so that clients can not mutate layerModule.moduleSet
			var result = {};
			for(var p in includeSet){
				result[p] = includeSet[p];
			}
			return result;
		},

		insertAbsMid = function(
			text,
			resource
		){
			return (!resource.mid || resource.tag.hasAbsMid || !bc.insertAbsMids) ?
				text : text.replace(/(define\s*\(\s*)(.*)/, "$1\"" + resource.mid + "\", $2");
		},

		pushString = function(
			strings,
			pair
		){
			strings[pair[0]] = pair[1];
		},

		appendStringsToCache = function(
			strings,
			cache
		) {
			for(var p in strings){
				cache.push("'" + p + "':" + strings[p])
			}
		},

		getPreloadL10nRootPath = function(
			dest
		){
			var match = dest.match(/(.+)\/([^\/]+)$/);
			return match[1] + "/nls/" + match[2];
		},

		flattenRootBundle = function(resource){
			if(resource.flattenedBundles){
				return;
			}
			resource.flattenedBundles = {};
			var scheduledToFlatten = {};
			bc.localeList.forEach(function(locale){
				scheduledToFlatten[locale] = 1;
			});
			bc.localeList.forEach(function(locale){
					var accumulator = lang.mixin({}, resource.bundleValue.root);
					if(!bc.localeList.discreteLocales[locale]){
						console.log(resource.mid, locale);
					}else{
						bc.localeList.discreteLocales[locale].forEach(function(discreteLocale){
							var localizedBundle = resource.localizedSet[discreteLocale];
							if(localizedBundle && localizedBundle.bundleValue){
								lang.mixin(accumulator, localizedBundle.bundleValue);
							}
						});

						var set = {};
						if(locale === "ROOT"){
							for(var p in resource.localizedSet) set[p] = 1;
						}else{
							for(var p in resource.localizedSet) if(!scheduledToFlatten[p] && p.indexOf(locale) == 0 && p.length > locale.length){
								// p is a more-specific version of locale and it is not scheduled to be flattened itself
								set[p] = 1;
							}
						}
						accumulator._localized = set;
					}
					resource.flattenedBundles[locale] = accumulator;
				}
			)
			;
		},

		getFlattenedBundles = function(
			resource,
			rootBundles
		){
			rootBundles.forEach(flattenRootBundle);

			var newline = bc.newline,
				rootPath = getPreloadL10nRootPath(resource.dest.match(/(.+)(\.js)$/)[1]),
				mid, cache;
			bc.localeList.forEach(function(locale){
				cache = [];
				rootBundles.forEach(function(rootResource){
					cache.push("'" + rootResource.prefix + rootResource.bundle + "':" + json.stringify(rootResource.flattenedBundles[locale]) + newline);
				});
				mid = getPreloadL10nRootPath(resource.mid) + "_" + locale;
				var flattenedResource = {
					src:"*synthetic*",
					dest:rootPath + "_" + locale + ".js",
					pid:resource.pid,
					mid:mid,
					pack:resource.pack,
					deps:[],
					tag:{flattenedNlsBundle:1},
					encoding:'utf8',
					text:"define(" + (bc.insertAbsMids ? "'" + mid + "',{" : "{") + newline + cache.join("," + newline) + "});",
					getText:function(){ return this.text; }
				};
				if(bc.insertAbsMids){
					flattenedResource.tag.hasAbsMid = 1;
				}
				bc.start(flattenedResource);
			});
		},

		getLayerText = function(
			resource,
			resourceText // ===undefined (normal case) => AMD define() the resource at the end of the layer
						 // ===false => put the resource in the cache
						 // otherwise write the value of resourceText at the end of the layer (so far only used in the writeDojo transform)
		){
			var newline = bc.newline,
				rootBundles = [],
				strings = {},
				cache = [],
				layer = resource.layer,
				moduleSet = computeLayerContents(resource, layer.include, layer.exclude),
				includeLocales = "includeLocales" in layer ? layer.includeLocales : bc.includeLocales;			
			for(var p in moduleSet){
				// always put modules!=resource in the cache; put resource in the cache if it's a boot layer and an explicit resourceText wasn't given
				if(p!=resource.mid || resourceText===false){
					var module = moduleSet[p];
					if(module.localizedSet && bc.localeList){
						// this is a root NLS bundle and the profile is building flattened layer bundles;
						// therefore, add this bundle to the set to be flattened, but don't write the root bundle
						// to the cache since the loader will explicitly load the flattened bundle
						rootBundles.push(module);
						if(includeLocales){
							// include the ROOT always
							cache.push("'" + p + "':function(){" + newline + module.getText() + newline + "}");
							// now include each locale in the layer
							includeLocales.forEach(function(locale){
								var parts = locale.split("-");
								for(var i = parts.length; i > 0; i--){
									var localizedSet = module.localizedSet[parts.slice(0, i).join("-")];
									// see if the localized set is there
									if(localizedSet){
										// put the bundle in the cache
										cache.push("'" + localizedSet.mid + "':function(){" + newline + localizedSet.getText() + newline + "}");
									}
								}
							});
						}
					}else if(module.internStrings){
						pushString(strings, module.internStrings());
					}else if(module.getText){
						cache.push("'" + p + "':function(){" + newline + module.getText() + newline + "}");
					}else{
						bc.log("amdMissingLayerModuleText", ["module", module.mid, "layer", resource.mid]);
					}
				}
			}
			appendStringsToCache(strings, cache);

			// compute the flattened layer bundles (if any)
			if(rootBundles.length){
				getFlattenedBundles(resource, rootBundles);
				// push an *now into the cache that causes the flattened layer bundles to be loaded immediately
				cache.push("'*now':function(r){r(['dojo/i18n!*preload*" + getPreloadL10nRootPath(resource.mid) + "*" + 
					json.stringify(bc.localeList.filter(function(locale){
						return !includeLocales || (includeLocales.indexOf(locale) == -1 && locale != "ROOT");
					})) + "']);}" + newline);
			}

			// construct the cache text
			if(cache.length && resource.layer.noref){
				cache.push("'*noref':1");
			}
			return	(cache.length ? "require({cache:{" + newline + cache.join("," + newline) + "}});" + newline : "") +
				(resourceText===undefined ?	 insertAbsMid(resource.getText(), resource) : (resourceText==false ? "" : resourceText)) +
				(resource.layer.postscript ? resource.layer.postscript : "");
		},

		getStrings = function(
			resource
		){
			var strings = {},
				cache = [],
				newline = bc.newline;
			resource.deps && resource.deps.forEach(function(dep){
				if(dep.internStrings){
					pushString(strings, dep.internStrings());
				}
			});
			appendStringsToCache(strings, cache);
			return cache.length ? "require({cache:{" + newline + cache.join("," + newline) + "}});" + newline : "";
		},

		getDestFilename = function(
			resource
		){
			if((resource.layer && bc.layerOptimize) || (!resource.layer && bc.optimize)){
				return resource.dest + ".uncompressed.js";
			}
			return resource.dest;
		},

		processNlsBundle = function(
			resource
		){
			var newline = bc.newline, text, p;

			if(resource.localizedSet && resource.bundleValue){
				// do a check that all localizations mentioned in the root actually exist
				var missing = [];
				for(p in resource.bundleValue){
					if(p!="root" && resource.bundleValue[p] && !resource.localizedSet[p]){
						missing.push("'" + p + "'");
					}
				}
				if(missing.length){
					missing.sort();
					bc.log("missingL10n", "Root: " + resource.mid + "; missing bundles: " + missing.join(",") + ".");
				}
			}
			if(resource.bundleType=="legacy"){
				if(resource.bundleValue){
					if(resource.localizedSet){
						// this is the root bundle; augment with all available localizations
						for(p in resource.localizedSet){
							resource.bundleValue[p] = 1;
						}
					}
					text = json.stringify(resource.bundleValue);
				}else{
					text = "// ERROR: builder was unable to evaluate source bundle; therefore, this empty conversion was written" + newline + "{}";
				}
				return "define(" + (bc.insertAbsMids ? "'" + resource.mid + "'," : "") + newline + text  + newline + ");";
			}else{
				return insertAbsMid(resource.getText(), resource);
			}
		},

		write = function(
			resource,
			callback
		){
			if(resource.layer && (resource.layer.boot || resource.layer.discard)){
				// resource.layer.boot layers are written by the writeDojo transform
				return 0;
			}

			var copyright;
			if(resource.pack){
				copyright = resource.pack.copyrightNonlayers && (resource.pack.copyright || bc.copyright);
			}else{
				copyright = bc.copyrightNonlayers &&  bc.copyright;
			}
			if(!copyright){
				copyright = "";
			}

			var text;
			if(resource.tag.nls){
				text = processNlsBundle(resource);
			}else if(resource.layer){
				// don't insertAbsMid or internStrings since that's done in getLayerText
				text= getLayerText(resource);
				if(resource.layer.compat=="1.6"){
					text += "require(" + json.stringify(resource.layer.include) + ");" + bc.newline;
				}
				copyright = resource.layer.copyright || "";
			}else{
				text = insertAbsMid(resource.getText(), resource);
				if(bc.internStrings){
					text = getStrings(resource) + text;
				}
			}

			// remember the uncompressed text for the optimizer
			resource.uncompressedText = text;

			var destFilename = getDestFilename(resource);
			fileUtils.ensureDirectoryByFilename(destFilename);
			fs.writeFile(destFilename, bc.newlineFilter(text, resource, "writeAmd"), resource.encoding, function(err){
				callback(resource, err);
			});
			return callback;
		};

		write.getLayerText = getLayerText;
		write.getDestFilename = getDestFilename;
		write.computeLayerContents = computeLayerContents;

		return write;
});

