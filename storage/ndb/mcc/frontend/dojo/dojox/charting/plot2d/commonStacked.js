define([
	"dojo/_base/lang",
	"dojox/lang/functional",
	"./common"
], function(lang, df, common){
	
	var commonStacked = lang.getObject("dojox.charting.plot2d.commonStacked", true);
	return lang.mixin(commonStacked, {
		collectStats: function(series, isNullValue){
			var stats = lang.delegate(common.defaultStats);
			for(var i = 0; i < series.length; ++i){
				var run = series[i];
				for(var j = 0; j < run.data.length; j++){
					var x, y;
					if(run.data[j] !== null){
						if(typeof run.data[j] == "number" || !run.data[j].hasOwnProperty("x")){
							y = commonStacked.getIndexValue(series, i, j, isNullValue)[0];
							x = j+1;
						}else{
							x = run.data[j].x;
							if(x !== null){
								y = commonStacked.getValue(series, i, x, isNullValue)[0];
								y = y != null && y.y ? y.y:null; 
							}
						}
						stats.hmin = Math.min(stats.hmin, x);
						stats.hmax = Math.max(stats.hmax, x);
						stats.vmin = Math.min(stats.vmin, y);
						stats.vmax = Math.max(stats.vmax, y);
					}
				}
			}
			return stats;
		},
		
		rearrangeValues: function(values, transform, baseline){
			// collect references to non-empty sets
			var sets = df.filter(values, "x"), n = sets.length;
			if(!n){
				// nothing to do at all
				return values;
			}

			// stack values
			var previousSet = {};
			for(var i = 0; i < n; ++i){
				var extractedSet = sets[i];
				for(var j = extractedSet.min, k = extractedSet.max; j < k; ++j){
					extractedSet[j] = (extractedSet[j] || 0) + (previousSet[j] || 0);
				}
				previousSet = extractedSet;
			}

			// transform to pixels
			for(i = 0; i < n; ++i){
				extractedSet = sets[i];
				for(j = extractedSet.min, k = extractedSet.max; j < k; ++j){
					extractedSet[j] = this.isNullValue(extractedSet[j]) ? 0 :
						transform(extractedSet[j]) - baseline;
				}
			}

			// correct the minimal width
			if(this.opt.minWidth){
				var minWidth = this.opt.minWidth;

				// unstack
				for(i = n - 1; i; --i){
					extractedSet = sets[i];
					previousSet  = sets[i - 1];
					for(j = extractedSet.min, k = extractedSet.max; j < k; ++j){
						extractedSet[j] = extractedSet[j] - previousSet[j];
					}
				}

				// now let's go over all values and correct them, if needed
				var min = extractedSet.min, max = extractedSet.max;
				for(var j = min; j < max; ++j){

					// find a total length of stack
					var sum = 0, counter = 0;
					for(i = 0; i < n; ++i){
						var value = sets[i][j];
						if(value > 0){
							sum += value;
							++counter;
						}
					}
					if(sum <= counter * minWidth){
						// the corner case: all values are very small
						for(i = 0; i < n; ++i){
							value = sets[i][j];
							if(value > 0){
								sets[i][j] = minWidth;
							}
						}
						continue;	// next stack
					}

					// distributing overflow up
					var overflow = 0;
					for(i = 0; i < n; ++i){
						extractedSet = sets[i];
						value = extractedSet[j];
						if(value > 0){
							if(value < minWidth){
								overflow += minWidth - value;
								extractedSet[j] = minWidth;
							}else if(overflow > 0){
								// calculate available space
								var available = extractedSet[j] - minWidth;
								if(available >= overflow){
									extractedSet[j] -= overflow;
									overflow = 0;
								}else if(available > 0){
									extractedSet[j] = minWidth;
									overflow -= available;
								}
							}
						}
					}

					// distributing overflow down, if any
					if(overflow > 0){
						for(i = n - 1; i >= 0; --i){
							extractedSet = sets[i];
							value = extractedSet[j];
							if(value > 0){
								// calculate available space
								available = extractedSet[j] - minWidth;
								if(available >= overflow){
									extractedSet[j] -= overflow;
									break;
								}else if(available > 0){
									extractedSet[j] = minWidth;
									overflow -= available;
								}
							}
						}
					}
				}

				// stack
				for(i = 1; i < n; ++i){
					extractedSet = sets[i];
					previousSet  = sets[i - 1];
					for(j = extractedSet.min, k = extractedSet.max; j < k; ++j){
						extractedSet[j] = extractedSet[j] + previousSet[j];
					}
				}
			}

			return values;
		},
		
		getIndexValue: function(series, i, index, isNullValue){
			var value = 0, v, j, pvalue;
			for(j = 0; j <= i; ++j){
				if(series[j].hidden){
					continue;
				}
				pvalue = value;
				v = series[j].data[index];
				if(!isNullValue(v)){
					if(isNaN(v)){ v = v.y || 0; }
					value += v;
				}
			}
			return [value , pvalue];
		},
		
		getValue: function(series, i, x, isNullValue){
			var value = null, j, z, v, pvalue;
			for(j = 0; j <= i; ++j){
				if(series[j].hidden){
					continue;
				}
				for(z = 0; z < series[j].data.length; z++){
					pvalue = value;
					v = series[j].data[z];
					if(!isNullValue(v)){
						if(v.x == x){
							if(!value){
								value = {x: x};
							}
							if(v.y != null){
								if(value.y == null){
									value.y = 0;
								}
								value.y += v.y;
							}
							break;
						}else if(v.x > x){break;}
					}
				}
			}
			return [value, pvalue];
		}
	});
});
