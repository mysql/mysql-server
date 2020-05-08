define([
	"dojo/_base/array",
	"dojo/_base/lang",
	"dojo/has"
], function(array, lang, has){
	"use strict";

	has.add("object-is-api", lang.isFunction(Object.is));

	var arrayProto = Array.prototype,
		areSameValues = has("object-is-api") ? Object.is : function(lhs, rhs){
			return lhs === rhs && (lhs !== 0 || 1 / lhs === 1 / rhs) || lhs !== lhs && rhs !== rhs;
		};

	function watch(o, prop, callback){
		var hWatch;

		if(o && lang.isFunction(o.watch)){
			hWatch = o.watch(prop, function(name, old, current){
				if(!areSameValues(old, current)){
					callback(current, old);
				}
			});
		}else{
			console.log("Attempt to observe non-stateful " + o + " with " + prop + ". Observation not happening.");
		}

		return {
			remove: function(){
				if(hWatch){
					hWatch.remove();
				}
			}
		};
	}

	function getProps(list){
		return array.map(list, function(p){
			return p.each ?
				array.map(p.target, function(entry){
					return entry.get ? entry.get(p.targetProp) : entry[p.targetProp];
				}) :
				p.target.get ? p.target.get(p.targetProp) : p.target[p.targetProp];
		});
	}

	function removeHandles(handles) {
		for(var h = null; (h = handles.shift());){
			h.remove();
		}
	}

	return function(target, targetProp, compute /*===== , deps =====*/){
		// summary:
		//		Returns a pointer to a dojo/Stateful property that are computed with other dojo/Stateful properties.
		// target: dojo/Stateful
		//		dojo/Stateful where the property is in.
		// targetProp: String
		//		The property name.
		// compute: Function
		//		The function, which takes dependent dojo/Stateful property values as the arguments, and returns the computed value.
		// deps: dojox/mvc/at...
		//		The dojo/Stateful properties this computed property depends on.
		// returns:
		//		The handle to clean this up.
		// example:
		//		If stateful.first is "John" and stateful.last is "Doe", stateful.name becomes "John Doe".
		// |        computed(stateful, "name", function(first, last){
		// |            return first + " " + last;
		// |        }, at(stateful, "first"), at(stateful, "last"));
		// example:
		//		If names is an array of objects with name property, stateful.totalNameLength becomes the sum of length of name property of each array item.
		// |        computed(stateful, "totalNameLength", function(names){
		// |            var total = 0;
		// |            array.forEach(names, function(name){
		// |                total += name.length;
		// |            });
		// |            return total;
		// |        }, lang.mixin(at(names, "name"), {each: true}));

		function applyComputed(data){
			var result, hasResult;

			try{
				result = compute.apply(target, data);
				hasResult = true;
			}catch(e){
				console.error("Error during computed property callback: " + (e && e.stack || e));
			}

			if(hasResult){
				if(lang.isFunction(target.set)){
					target.set(targetProp, result);
				}else{
					target[targetProp] = result;
				}
			}
		}

		if(target == null){
			throw new Error("Computed property cannot be applied to null.");
		}
		if(targetProp === "*"){
			throw new Error("Wildcard property cannot be used for computed properties.");
		}

		var deps = arrayProto.slice.call(arguments, 3),
			hDep = array.map(deps, function(dep, index){
				function observeEntry(entry){
					return watch(entry, dep.targetProp, function(){
						applyComputed(getProps(deps));
					});
				}

				if(dep.targetProp === "*"){
					throw new Error("Wildcard property cannot be used for computed properties.");
				}else if(dep.each){
					var hArray,
						hEntry = array.map(dep.target, observeEntry);

					if(dep.target && lang.isFunction(dep.target.watchElements)){
						hArray = dep.target.watchElements(function(idx, removals, adds){
							removeHandles(arrayProto.splice.apply(hEntry, [idx, removals.length].concat(array.map(adds, observeEntry))));
							applyComputed(getProps(deps));
						});
					}else{
						console.log("Attempt to observe non-stateful-array " + dep.target + ". Observation not happening.");
					}

					return {
						remove: function(){
							if(hArray){
								hArray.remove();
							}
							removeHandles(hEntry);
						}
					};
				}else{
					return watch(dep.target, dep.targetProp, function(current){
						var list = [];
						arrayProto.push.apply(list, getProps(deps.slice(0, index)));
						list.push(current);
						arrayProto.push.apply(list, getProps(deps.slice(index + 1)));
						applyComputed(list);
					});
				}
			});

		applyComputed(getProps(deps));
		return {
			remove: function(){
				removeHandles(hDep);
			}
		};
	};
});
