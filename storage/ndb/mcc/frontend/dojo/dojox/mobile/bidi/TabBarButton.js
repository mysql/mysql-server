//>>built
define("dojox/mobile/bidi/TabBarButton",["dojo/_base/declare","./common","dojo/dom-class"],function(_1,_2,_3){
return _1(null,{_setBadgeAttr:function(_4){
this.inherited(arguments);
this.badgeObj.setTextDir(this.textDir);
},_setIcon:function(_5,n){
this.inherited(arguments);
if(this.iconDivNode&&!this.isLeftToRight()){
_3.remove(this.iconDivNode,"mblTabBarButtonIconArea");
_3.add(this.iconDivNode,"mblTabBarButtonIconAreaRtl");
}
}});
});
