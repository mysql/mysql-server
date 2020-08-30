//>>built
define("dojox/drawing/ui/dom/Toolbar",["dojo","../../util/common"],function(_1,_2){
_1.deprecated("dojox.drawing.ui.dom.Toolbar","It may not even make it to the 1.4 release.",1.4);
return _1.declare("dojox.drawing.ui.dom.Toolbar",[],{baseClass:"drawingToolbar",buttonClass:"drawingButton",iconClass:"icon",constructor:function(_3,_4){
_1.addOnLoad(this,function(){
this.domNode=_1.byId(_4);
_1.addClass(this.domNode,this.baseClass);
this.parse();
});
},createIcon:function(_5,_6){
var _7=_6&&_6.setup?_6.setup:{};
if(_7.iconClass){
var _8=_7.iconClass?_7.iconClass:"iconNone";
var _9=_7.tooltip?_7.tooltip:"Tool";
var _a=_1.create("div",{title:_9},_5);
_1.addClass(_a,this.iconClass);
_1.addClass(_a,_8);
_1.connect(_5,"mouseup",function(_b){
_1.stopEvent(_b);
_1.removeClass(_5,"active");
});
_1.connect(_5,"mouseover",function(_c){
_1.stopEvent(_c);
_1.addClass(_5,"hover");
});
_1.connect(_5,"mousedown",this,function(_d){
_1.stopEvent(_d);
_1.addClass(_5,"active");
});
_1.connect(_5,"mouseout",this,function(_e){
_1.stopEvent(_e);
_1.removeClass(_5,"hover");
});
}
},createTool:function(_f){
_f.innerHTML="";
var _10=_1.attr(_f,"tool");
this.toolNodes[_10]=_f;
_1.attr(_f,"tabIndex",1);
var _11=_1.getObject(_10);
this.createIcon(_f,_11);
this.drawing.registerTool(_10,_11);
_1.connect(_f,"mouseup",this,function(evt){
_1.stopEvent(evt);
_1.removeClass(_f,"active");
this.onClick(_10);
});
_1.connect(_f,"mouseover",function(evt){
_1.stopEvent(evt);
_1.addClass(_f,"hover");
});
_1.connect(_f,"mousedown",this,function(evt){
_1.stopEvent(evt);
_1.addClass(_f,"active");
});
_1.connect(_f,"mouseout",this,function(evt){
_1.stopEvent(evt);
_1.removeClass(_f,"hover");
});
},parse:function(){
var _12=_1.attr(this.domNode,"drawingId");
this.drawing=_2.byId(_12);
!this.drawing&&console.error("Drawing not found based on 'drawingId' in Toolbar. ");
this.toolNodes={};
var _13;
_1.forEach(this.domNode.childNodes,function(_14,i){
if(_14.nodeType!==1){
return;
}
_14.className=this.buttonClass;
var _15=_1.attr(_14,"tool");
var _16=_1.attr(_14,"action");
var _17=_1.attr(_14,"plugin");
if(_15){
if(i==0||_1.attr(_14,"selected")=="true"){
_13=_15;
}
this.createTool(_14);
}else{
if(_17){
var p={name:_17,options:{}},opt=_1.attr(_14,"options");
if(opt){
p.options=eval("("+opt+")");
}
p.options.node=_14;
_14.innerHTML="";
this.drawing.addPlugin(p);
this.createIcon(_14,_1.getObject(_1.attr(_14,"plugin")));
}
}
},this);
this.drawing.initPlugins();
_1.connect(this.drawing,"setTool",this,"onSetTool");
this.drawing.setTool(_13);
},onClick:function(_18){
this.drawing.setTool(_18);
},onSetTool:function(_19){
for(var n in this.toolNodes){
if(n==_19){
_1.addClass(this.toolNodes[_19],"selected");
this.toolNodes[_19].blur();
}else{
_1.removeClass(this.toolNodes[n],"selected");
}
}
}});
});
