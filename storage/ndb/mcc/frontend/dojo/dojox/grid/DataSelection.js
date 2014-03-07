//>>built
define("dojox/grid/DataSelection",["dojo/_base/declare","./_SelectionPreserver","./Selection"],function(_1,_2,_3){
return _1("dojox.grid.DataSelection",_3,{constructor:function(_4){
if(_4.keepSelection){
this.preserver=new _2(this);
}
},destroy:function(){
if(this.preserver){
this.preserver.destroy();
}
},getFirstSelected:function(){
var _5=_3.prototype.getFirstSelected.call(this);
if(_5==-1){
return null;
}
return this.grid.getItem(_5);
},getNextSelected:function(_6){
var _7=this.grid.getItemIndex(_6);
var _8=_3.prototype.getNextSelected.call(this,_7);
if(_8==-1){
return null;
}
return this.grid.getItem(_8);
},getSelected:function(){
var _9=[];
for(var i=0,l=this.selected.length;i<l;i++){
if(this.selected[i]){
_9.push(this.grid.getItem(i));
}
}
return _9;
},addToSelection:function(_a){
if(this.mode=="none"){
return;
}
var _b=null;
if(typeof _a=="number"||typeof _a=="string"){
_b=_a;
}else{
_b=this.grid.getItemIndex(_a);
}
_3.prototype.addToSelection.call(this,_b);
},deselect:function(_c){
if(this.mode=="none"){
return;
}
var _d=null;
if(typeof _c=="number"||typeof _c=="string"){
_d=_c;
}else{
_d=this.grid.getItemIndex(_c);
}
_3.prototype.deselect.call(this,_d);
},deselectAll:function(_e){
var _f=null;
if(_e||typeof _e=="number"){
if(typeof _e=="number"||typeof _e=="string"){
_f=_e;
}else{
_f=this.grid.getItemIndex(_e);
}
_3.prototype.deselectAll.call(this,_f);
}else{
this.inherited(arguments);
}
}});
});
