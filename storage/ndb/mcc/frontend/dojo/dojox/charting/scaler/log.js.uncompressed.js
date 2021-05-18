define("dojox/charting/scaler/log", ["dojo/_base/declare", "dojo/_base/lang", "dojo/_base/array", "./linear", "./common"],
    function(declare, lang, arr, linear, common){
        var log = {}, getLabel = common.getNumericLabel;

        function makeLabel(value, kwArgs){
            var precision = 0;
            if(value < 0.6){  // covers 1/2, 1/3, 1/4, and so on
                var m = value.toString().match(/\.(\d+)/);
                if(m && m[1]){
                    precision = -m[1].length;
                }
            }
            return getLabel(value, precision, kwArgs);
        }

        return lang.mixin(log, linear, {
            base: 10,

            setBase: function (/*Number*/ base) {
                this.base = Math.round(base || 10);
            },

            buildScaler: function(/*Number*/ min, /*Number*/ max, /*Number*/ span, /*Object*/ kwArgs){
                var logBase = Math.log(this.base), from, to;
                // update bounds
                if("min" in kwArgs){ min = kwArgs.min; }
                if("max" in kwArgs){ max = kwArgs.max; }
                // transform bounds
                min = Math.log(min) / logBase;
                max = Math.log(max) / logBase;
                var fMin = Math.floor(min), cMax = Math.ceil(max);
                min = fMin < min ? fMin : min - 1;
                max = max < cMax ? cMax : max + 1;

                // continue normal processing
                if(kwArgs.includeZero){
                    if(min  > 0){ min  = 0; }
                    if(max  < 0){ max  = 0; }
                    if(from > 0){ from = 0; }
                    if(to   > 0){ to   = 0; }
                }

                var linArgs = {
                    min: min,
                    max: max,
                    fixUpper: kwArgs.fixUpper,
                    fixLower: kwArgs.fixLower,
                    natural:  kwArgs.natural,
                    minorTicks:    false,
                    minorLabels:   false,
                    majorTickStep: 1
                };

                // Process from/to
                if("from" in kwArgs){
                    linArgs.from  = Math.log(kwArgs.from) / logBase;
                }
                if("to" in kwArgs){
                    linArgs.to = Math.log(kwArgs.to) / logBase;
                }

                var result = linear.buildScaler.call(linear, min, max, span, linArgs);
                // transform scaler back
                result.scaler = log;
                result.bounds.lower = Math.exp(result.bounds.lower * logBase);
                result.bounds.upper = Math.exp(result.bounds.upper * logBase);
                result.bounds.from  = Math.exp(result.bounds.from  * logBase);
                result.bounds.to    = Math.exp(result.bounds.to    * logBase);
                return result;
            },
            buildTicks: function(scaler, kwArgs){
                var base = this.base, logBase = Math.log(this.base);

                // transform scaler to log
                var oldBounds = lang.mixin({}, scaler.bounds);
                scaler.bounds.lower = Math.log(scaler.bounds.lower) / logBase;
                scaler.bounds.upper = Math.log(scaler.bounds.upper) / logBase;
                scaler.bounds.from  = Math.log(scaler.bounds.from)  / logBase;
                scaler.bounds.to    = Math.log(scaler.bounds.to)    / logBase;

                var newKwArgs = lang.mixin({}, kwArgs);
                newKwArgs.minorTicks = newKwArgs.minorLabels = false;
                newKwArgs.majorTickStep = 1;

                var result = linear.buildTicks.call(linear, scaler, newKwArgs);

                // revert scaler back
                lang.mixin(scaler.bounds, oldBounds);

                if(!result){
                    return result;
                }

                // transform ticks back
                function transformTick(tick){
                    tick.value = Math.exp(tick.value * logBase);
                    if(tick.value >= 1){
                        tick.value = Math.round(tick.value);
                    }
                    if(base === 10){
                        tick.value = +tick.value.toPrecision(1);
                    }
                    if(kwArgs.minorLabels){
                        tick.label = makeLabel(tick.value, kwArgs);
                    }
                }

                arr.forEach(result.major, transformTick);

                result.minor = [];
                if(kwArgs.minorTicks && this.base === 10){
                    var from = scaler.bounds.from, to = scaler.bounds.to,
                        push = function(value){
                            if(from <= value && value <= to){
                                if(kwArgs.minorLabels){
                                    result.minor.push({value: value,
                                        label: makeLabel(value, kwArgs)});
                                }else{
                                    result.minor.push({value: value});
                                }
                            }
                        };
                    if(result.major.length){
                        push(+(result.major[0].value / 5).toPrecision(1));
                        push(+(result.major[0].value / 2).toPrecision(1));
                    }
                    arr.forEach(result.major, function(tick, i){
                        push(+(tick.value * 2).toPrecision(1));
                        push(+(tick.value * 5).toPrecision(1));
                    });
                }

                result.micro = [];

                return result;
            },
            getTransformerFromModel: function(/*Object*/ scaler){
                var logBase = Math.log(this.base),
                    offset = Math.log(scaler.bounds.from) / logBase,
                    scale = scaler.bounds.scale;
                return function(x){ return (Math.log(x) / logBase - offset) * scale; };	// Function
            },
            getTransformerFromPlot: function(/*Object*/ scaler){
                var base = this.base,
                    offset = scaler.bounds.from,
                    scale = scaler.bounds.scale;
                return function(x){ return Math.pow(base, (x/scale))*offset; };	// Function
            }
        });
    }
);
