// wrapped by build app
define("dojox/widget/gauge/_Gauge", ["dojo","dijit","dojox","dojo/require!dojox/gauges/_Gauge"], function(dojo,dijit,dojox){
dojo.provide("dojox.widget.gauge._Gauge");
dojo.require("dojox.gauges._Gauge");

dojox.widget.gauge._Gauge = dojox.gauges._Gauge;
dojox.widget.gauge.Range = dojox.gauges.Range;
dojox.widget.gauge._indicator = dojox.gauges._indicator;

});
