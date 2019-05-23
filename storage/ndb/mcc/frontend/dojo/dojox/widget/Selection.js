//>>built
define("dojox/widget/Selection",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/Stateful"],function(_1,_2,_3,_4){
return _1("dojox.widget.Selection",_4,{constructor:function(){
this.selectedItems=[];
},selectionMode:"single",_setSelectionModeAttr:function(_5){
if(_5!="none"&&_5!="single"&&_5!="multiple"){
_5="single";
}
if(_5!=this.selectionMode){
this.selectionMode=_5;
if(_5=="none"){
this.set("selectedItems",null);
}else{
if(_5=="single"){
this.set("selectedItem",this.selectedItem);
}
}
}
},selectedItem:null,_setSelectedItemAttr:function(_6){
if(this.selectedItem!=_6){
this._set("selectedItem",_6);
this.set("selectedItems",_6?[_6]:null);
}
},selectedItems:null,_setSelectedItemsAttr:function(_7){
var _8=this.selectedItems;
this.selectedItems=_7;
this.selectedItem=null;
if(_8!=null&&_8.length>0){
this.updateRenderers(_8,true);
}
if(this.selectedItems&&this.selectedItems.length>0){
this.selectedItem=this.selectedItems[0];
this.updateRenderers(this.selectedItems,true);
}
},_getSelectedItemsAttr:function(){
return this.selectedItems==null?[]:this.selectedItems.concat();
},isItemSelected:function(_9){
if(this.selectedItems==null||this.selectedItems.length==0){
return false;
}
return _2.some(this.selectedItems,_3.hitch(this,function(_a){
return this.getIdentity(_a)==this.getIdentity(_9);
}));
},getIdentity:function(_b){
},setItemSelected:function(_c,_d){
if(this.selectionMode=="none"||_c==null){
return;
}
var _e=this.get("selectedItems");
var _f=this.get("selectedItems");
if(this.selectionMode=="single"){
if(_d){
this.set("selectedItem",_c);
}else{
if(this.isItemSelected(_c)){
this.set("selectedItems",null);
}
}
}else{
if(_d){
if(this.isItemSelected(_c)){
return;
}
if(_e==null){
_e=[_c];
}else{
_e.unshift(_c);
}
this.set("selectedItems",_e);
}else{
var res=_2.filter(_e,function(_10){
return _10.id!=_c.id;
});
if(res==null||res.length==_e.length){
return;
}
this.set("selectedItems",res);
}
}
},selectFromEvent:function(e,_11,_12,_13){
if(this.selectionMode=="none"){
return false;
}
var _14;
var _15=this.get("selectedItem");
var _16=_11?this.isItemSelected(_11):false;
if(_11==null){
if(!e.ctrlKey&&this.selectedItem!=null){
this.set("selectedItem",null);
_14=true;
}
}else{
if(this.selectionMode=="multiple"){
if(e.ctrlKey){
this.setItemSelected(_11,!_16);
_14=true;
}else{
this.set("selectedItem",_11);
_14=true;
}
}else{
if(e.ctrlKey){
this.set("selectedItem",_16?null:_11);
_14=true;
}else{
if(!_16){
this.set("selectedItem",_11);
_14=true;
}
}
}
}
if(_13&&_14){
this.dispatchChange(_15,this.get("selectedItem"),_12,e);
}
return _14;
},dispatchChange:function(_17,_18,_19,_1a){
this.onChange({oldValue:_17,newValue:_18,renderer:_19,triggerEvent:_1a});
},onChange:function(){
}});
});
