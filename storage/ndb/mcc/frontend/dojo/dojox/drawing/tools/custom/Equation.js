//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/tools/TextBlock"],function(_1,_2,_3){
_2.provide("dojox.drawing.tools.custom.Equation");
_2.require("dojox.drawing.tools.TextBlock");
_3.drawing.tools.custom.Equation=_3.drawing.util.oo.declare(_3.drawing.tools.TextBlock,function(_4){
},{customType:"equation"});
_3.drawing.tools.custom.Equation.setup={name:"dojox.drawing.tools.custom.Equation",tooltip:"Equation Tool",iconClass:"iconEq"};
_3.drawing.register(_3.drawing.tools.custom.Equation.setup,"tool");
});
