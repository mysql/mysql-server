//>>built
define("dojox/widget/PortletDialogSettings",["dojo/_base/declare","dojo/_base/window","dojo/dom-style","./PortletSettings","dijit/Dialog"],function(_1,_2,_3,_4,_5){
return _1("dojox.widget.PortletDialogSettings",[_4],{dimensions:null,constructor:function(_6,_7){
this.dimensions=_6.dimensions||[300,100];
},toggle:function(){
if(!this.dialog){
this.dialog=new _5({title:this.title});
_2.body().appendChild(this.dialog.domNode);
this.dialog.containerNode.appendChild(this.domNode);
_3.set(this.dialog.domNode,{"width":this.dimensions[0]+"px","height":this.dimensions[1]+"px"});
_3.set(this.domNode,"display","");
}
if(this.dialog.open){
this.dialog.hide();
}else{
this.dialog.show(this.domNode);
}
}});
});
