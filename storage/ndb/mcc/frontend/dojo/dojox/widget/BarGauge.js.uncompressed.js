// wrapped by build app
define("dojox/widget/BarGauge", ["dojo","dijit","dojox","dojo/require!dojox/widget/gauge/_Gauge,dojox/gauges/BarGauge"], function(dojo,dijit,dojox){
// backward compatibility for dojox.widget.BarGauge
dojo.provide("dojox.widget.BarGauge");
dojo.require("dojox.widget.gauge._Gauge");
dojo.require("dojox.gauges.BarGauge");
dojox.widget.BarGauge = dojox.gauges.BarGauge;
dojox.widget.gauge.BarLineIndicator = dojox.gauges.BarLineIndicator;

});
