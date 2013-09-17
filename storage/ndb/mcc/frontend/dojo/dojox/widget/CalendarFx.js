//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/widget/FisheyeLite"],function(_1,_2,_3){
_2.provide("dojox.widget.CalendarFx");
_2.require("dojox.widget.FisheyeLite");
_2.declare("dojox.widget._FisheyeFX",null,{addFx:function(_4,_5){
_2.query(_4,_5).forEach(function(_6){
new _3.widget.FisheyeLite({properties:{fontSize:1.1}},_6);
});
}});
_2.declare("dojox.widget.CalendarFisheye",[_3.widget.Calendar,_3.widget._FisheyeFX],{});
});
