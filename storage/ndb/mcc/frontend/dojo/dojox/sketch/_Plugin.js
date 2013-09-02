//>>built
define("dojox/sketch/_Plugin",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/connect","dijit/form/ToggleButton"],function(_1){
_1.getObject("sketch",true,dojox);
_1.declare("dojox.sketch._Plugin",null,{constructor:function(_2){
if(_2){
_1.mixin(this,_2);
}
this._connects=[];
},figure:null,iconClassPrefix:"dojoxSketchIcon",itemGroup:"toolsGroup",button:null,queryCommand:null,shape:"",useDefaultCommand:true,buttonClass:dijit.form.ToggleButton,_initButton:function(){
if(this.shape.length){
var _3=this.iconClassPrefix+" "+this.iconClassPrefix+this.shape.charAt(0).toUpperCase()+this.shape.substr(1);
if(!this.button){
var _4={label:this.shape,showLabel:false,iconClass:_3,dropDown:this.dropDown,tabIndex:"-1"};
this.button=new this.buttonClass(_4);
this.connect(this.button,"onClick","activate");
}
}
},attr:function(_5,_6){
return this.button.attr(_5,_6);
},onActivate:function(){
},activate:function(e){
this.onActivate();
this.figure.setTool(this);
this.attr("checked",true);
},onMouseDown:function(e){
},onMouseMove:function(e){
},onMouseUp:function(e){
},destroy:function(f){
_1.forEach(this._connects,_1.disconnect);
},connect:function(o,f,tf){
this._connects.push(_1.connect(o,f,this,tf));
},setFigure:function(_7){
this.figure=_7;
},setToolbar:function(_8){
this._initButton();
if(this.button){
_8.addChild(this.button);
}
if(this.itemGroup){
_8.addGroupItem(this,this.itemGroup);
}
}});
return dojox.sketch._Plugin;
});
