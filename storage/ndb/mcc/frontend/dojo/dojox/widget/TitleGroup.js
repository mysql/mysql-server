//>>built
define("dojox/widget/TitleGroup",["dojo","dijit/registry","dijit/_Widget","dijit/TitlePane"],function(_1,_2,_3,_4){
var tp=_4.prototype,_5=function(){
var _6=this._dxfindParent&&this._dxfindParent();
_6&&_6.selectChild(this);
};
tp._dxfindParent=function(){
var n=this.domNode.parentNode;
if(n){
n=_2.getEnclosingWidget(n);
return n&&n instanceof dojox.widget.TitleGroup&&n;
}
return n;
};
_1.connect(tp,"_onTitleClick",_5);
_1.connect(tp,"_onTitleKey",function(e){
if(!(e&&e.type&&e.type=="keypress"&&e.charOrCode==_1.keys.TAB)){
_5.apply(this,arguments);
}
});
return _1.declare("dojox.widget.TitleGroup",dijit._Widget,{"class":"dojoxTitleGroup",addChild:function(_7,_8){
return _7.placeAt(this.domNode,_8);
},removeChild:function(_9){
this.domNode.removeChild(_9.domNode);
return _9;
},selectChild:function(_a){
_a&&_1.query("> .dijitTitlePane",this.domNode).forEach(function(n){
var tp=_2.byNode(n);
tp&&tp!==_a&&tp.open&&tp.toggle();
});
return _a;
}});
});
