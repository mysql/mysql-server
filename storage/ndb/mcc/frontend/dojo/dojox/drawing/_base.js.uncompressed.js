//>>built
// wrapped by build app
define("dojox/drawing/_base", ["dijit","dojo","dojox","dojo/require!dojox/drawing/manager/_registry,dojox/gfx,dojox/drawing/Drawing,dojox/drawing/util/oo,dojox/drawing/util/common,dojox/drawing/util/typeset,dojox/drawing/defaults,dojox/drawing/manager/Canvas,dojox/drawing/manager/Undo,dojox/drawing/manager/keys,dojox/drawing/manager/Mouse,dojox/drawing/manager/Stencil,dojox/drawing/manager/StencilUI,dojox/drawing/manager/Anchors,dojox/drawing/stencil/_Base,dojox/drawing/stencil/Line,dojox/drawing/stencil/Rect,dojox/drawing/stencil/Ellipse,dojox/drawing/stencil/Path,dojox/drawing/stencil/Text,dojox/drawing/stencil/Image,dojox/drawing/annotations/Label,dojox/drawing/annotations/Angle,dojox/drawing/annotations/Arrow,dojox/drawing/annotations/BoxShadow"], function(dijit,dojo,dojox){
dojo.provide("dojox.drawing._base");
dojo.experimental("dojox.drawing");

dojo.require("dojox.drawing.manager._registry");
dojo.require("dojox.gfx");
dojo.require("dojox.drawing.Drawing");
dojo.require("dojox.drawing.util.oo");
dojo.require("dojox.drawing.util.common");
dojo.require("dojox.drawing.util.typeset");
dojo.require("dojox.drawing.defaults");
dojo.require("dojox.drawing.manager.Canvas");

// interactive managers
dojo.require("dojox.drawing.manager.Undo");
dojo.require("dojox.drawing.manager.keys");
dojo.require("dojox.drawing.manager.Mouse");
dojo.require("dojox.drawing.manager.Stencil");
dojo.require("dojox.drawing.manager.StencilUI"); // plugin? or as a require? good here? in toolbar?
dojo.require("dojox.drawing.manager.Anchors");

// standard stencils
dojo.require("dojox.drawing.stencil._Base");
dojo.require("dojox.drawing.stencil.Line");
dojo.require("dojox.drawing.stencil.Rect");
dojo.require("dojox.drawing.stencil.Ellipse");
dojo.require("dojox.drawing.stencil.Path");
dojo.require("dojox.drawing.stencil.Text");
dojo.require("dojox.drawing.stencil.Image");

// annotations are built within stencil/_Base.js
// would like to optionally include them, but for
// now it's mandatory.
dojo.require("dojox.drawing.annotations.Label");
dojo.require("dojox.drawing.annotations.Angle");
dojo.require("dojox.drawing.annotations.Arrow");
dojo.require("dojox.drawing.annotations.BoxShadow");
});
