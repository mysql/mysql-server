//>>built
define("dijit/PopupMenuItem",["dojo/_base/declare","dojo/dom-style","dojo/query","dojo/_base/window","./registry","./MenuItem","./hccss"],function(_1,_2,_3,_4,_5,_6){
return _1("dijit.PopupMenuItem",_6,{_fillContent:function(){
if(this.srcNodeRef){
var _7=_3("*",this.srcNodeRef);
this.inherited(arguments,[_7[0]]);
this.dropDownContainer=this.srcNodeRef;
}
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(!this.popup){
var _8=_3("[widgetId]",this.dropDownContainer)[0];
this.popup=_5.byNode(_8);
}
_4.body().appendChild(this.popup.domNode);
this.popup.startup();
this.popup.domNode.style.display="none";
if(this.arrowWrapper){
_2.set(this.arrowWrapper,"visibility","");
}
this.focusNode.setAttribute("aria-haspopup","true");
},destroyDescendants:function(_9){
if(this.popup){
if(!this.popup._destroyed){
this.popup.destroyRecursive(_9);
}
delete this.popup;
}
this.inherited(arguments);
}});
});
