//>>built
define("dijit/_Contained",["dojo/_base/declare","./registry"],function(_1,_2){
return _1("dijit._Contained",null,{_getSibling:function(_3){
var p=this.getParent();
return (p&&p._getSiblingOfChild&&p._getSiblingOfChild(this,_3=="previous"?-1:1))||null;
},getPreviousSibling:function(){
return this._getSibling("previous");
},getNextSibling:function(){
return this._getSibling("next");
},getIndexInParent:function(){
var p=this.getParent();
if(!p||!p.getIndexOfChild){
return -1;
}
return p.getIndexOfChild(this);
}});
});
