//>>built
define("dojox/mobile/bidi/SwapView",["dojo/_base/declare"],function(_1){
return _1(null,{_callParentFunction:false,nextView:function(_2){
if(this.isLeftToRight()||this._callParentFunction){
this._callParentFunction=false;
return this.inherited(arguments);
}else{
this._callParentFunction=true;
return this.previousView(_2);
}
},previousView:function(_3){
if(this.isLeftToRight()||this._callParentFunction){
this._callParentFunction=false;
return this.inherited(arguments);
}else{
this._callParentFunction=true;
return this.nextView(_3);
}
}});
});
