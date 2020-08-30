//>>built
define("dojox/widget/Selection",["dojo/_base/declare","dojo/_base/array","dojo/sniff","dojo/_base/lang","dojo/Stateful"],function(_1,_2,_3,_4,_5){
return _1("dojox.widget.Selection",_5,{constructor:function(){
this.selectedItems=[];
},selectionMode:"single",_setSelectionModeAttr:function(_6){
if(_6!="none"&&_6!="single"&&_6!="multiple"){
_6="single";
}
if(_6!=this.selectionMode){
this.selectionMode=_6;
if(_6=="none"){
this.set("selectedItems",null);
}else{
if(_6=="single"){
this.set("selectedItem",this.selectedItem);
}
}
}
},selectedItem:null,_setSelectedItemAttr:function(_7){
if(this.selectedItem!=_7){
this._set("selectedItem",_7);
this.set("selectedItems",_7?[_7]:null);
}
},selectedItems:null,_setSelectedItemsAttr:function(_8){
var _9=this.selectedItems;
this.selectedItems=_8;
this.selectedItem=null;
if(_9!=null&&_9.length>0){
this.updateRenderers(_9,true);
}
if(this.selectedItems&&this.selectedItems.length>0){
this.selectedItem=this.selectedItems[0];
this.updateRenderers(this.selectedItems,true);
}
},_getSelectedItemsAttr:function(){
return this.selectedItems==null?[]:this.selectedItems.concat();
},isItemSelected:function(_a){
if(this.selectedItems==null||this.selectedItems.length==0){
return false;
}
return _2.some(this.selectedItems,_4.hitch(this,function(_b){
return this.getIdentity(_b)==this.getIdentity(_a);
}));
},getIdentity:function(_c){
},setItemSelected:function(_d,_e){
if(this.selectionMode=="none"||_d==null){
return;
}
var _f=this.get("selectedItems");
if(this.selectionMode=="single"){
if(_e){
this.set("selectedItem",_d);
}else{
if(this.isItemSelected(_d)){
this.set("selectedItems",null);
}
}
}else{
if(_e){
if(this.isItemSelected(_d)){
return;
}
if(_f==null){
_f=[_d];
}else{
_f.unshift(_d);
}
this.set("selectedItems",_f);
}else{
var res=_2.filter(_f,function(_10){
return _10.id!=_d.id;
});
if(res==null||res.length==_f.length){
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
var _17=_3("mac")?e.metaKey:e.ctrlKey;
if(_11==null){
if(!_17&&this.selectedItem!=null){
this.set("selectedItem",null);
_14=true;
}
}else{
if(this.selectionMode=="multiple"){
if(_17){
this.setItemSelected(_11,!_16);
_14=true;
}else{
this.set("selectedItem",_11);
_14=true;
}
}else{
if(_17){
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
},dispatchChange:function(_18,_19,_1a,_1b){
this.onChange({oldValue:_18,newValue:_19,renderer:_1a,triggerEvent:_1b});
},onChange:function(){
}});
});
