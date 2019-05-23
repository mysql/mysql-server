//>>built
define("dijit/PopupMenuItem",["dojo/_base/declare","dojo/dom-style","dojo/query","./registry","./MenuItem","./hccss"],function(_1,_2,_3,_4,_5){
return _1("dijit.PopupMenuItem",_5,{_fillContent:function(){
if(this.srcNodeRef){
var _6=_3("*",this.srcNodeRef);
this.inherited(arguments,[_6[0]]);
this.dropDownContainer=this.srcNodeRef;
}
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(!this.popup){
var _7=_3("[widgetId]",this.dropDownContainer)[0];
this.popup=_4.byNode(_7);
}
this.ownerDocumentBody.appendChild(this.popup.domNode);
this.popup.startup();
this.popup.domNode.style.display="none";
if(this.arrowWrapper){
_2.set(this.arrowWrapper,"visibility","");
}
this.focusNode.setAttribute("aria-haspopup","true");
},destroyDescendants:function(_8){
if(this.popup){
if(!this.popup._destroyed){
this.popup.destroyRecursive(_8);
}
delete this.popup;
}
this.inherited(arguments);
}});
});
