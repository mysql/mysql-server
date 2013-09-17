//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/Tree,dijit/Dialog,dijit/Menu,dijit/form/ValidationTextBox,dijit/form/Textarea,dijit/form/Button,dijit/form/RadioButton,dijit/form/FilteringSelect"],function(_1,_2,_3){
_2.provide("dojox.data.ItemExplorer");
_2.require("dijit.Tree");
_2.require("dijit.Dialog");
_2.require("dijit.Menu");
_2.require("dijit.form.ValidationTextBox");
_2.require("dijit.form.Textarea");
_2.require("dijit.form.Button");
_2.require("dijit.form.RadioButton");
_2.require("dijit.form.FilteringSelect");
(function(){
var _4=function(_5,_6,_7){
var _8=_5.getValues(_6,_7);
if(_8.length<2){
_8=_5.getValue(_6,_7);
}
return _8;
};
_2.declare("dojox.data.ItemExplorer",_1.Tree,{useSelect:false,refSelectSearchAttr:null,constructor:function(_9){
_2.mixin(this,_9);
var _a=this;
var _b={};
var _c=(this.rootModelNode={value:_b,id:"root"});
this._modelNodeIdMap={};
this._modelNodePropMap={};
var _d=1;
this.model={getRoot:function(_e){
_e(_c);
},mayHaveChildren:function(_f){
return _f.value&&typeof _f.value=="object"&&!(_f.value instanceof Date);
},getChildren:function(_10,_11,_12){
var _13,_14,_15=_10.value;
var _16=[];
if(_15==_b){
_11([]);
return;
}
var _17=_a.store&&_a.store.isItem(_15,true);
if(_17&&!_a.store.isItemLoaded(_15)){
_a.store.loadItem({item:_15,onItem:function(_18){
_15=_18;
_19();
}});
}else{
_19();
}
function _19(){
if(_17){
_13=_a.store.getAttributes(_15);
_14=_15;
}else{
if(_15&&typeof _15=="object"){
_14=_10.value;
_13=[];
for(var i in _15){
if(_15.hasOwnProperty(i)&&i!="__id"&&i!="__clientId"){
_13.push(i);
}
}
}
}
if(_13){
for(var key,k=0;key=_13[k++];){
_16.push({property:key,value:_17?_4(_a.store,_15,key):_15[key],parent:_14});
}
_16.push({addNew:true,parent:_14,parentNode:_10});
}
_11(_16);
};
},getIdentity:function(_1a){
if(!_1a.id){
if(_1a.addNew){
_1a.property="--addNew";
}
_1a.id=_d++;
if(_a.store){
if(_a.store.isItem(_1a.value)){
var _1b=_a.store.getIdentity(_1a.value);
(_a._modelNodeIdMap[_1b]=_a._modelNodeIdMap[_1b]||[]).push(_1a);
}
if(_1a.parent){
_1b=_a.store.getIdentity(_1a.parent)+"."+_1a.property;
(_a._modelNodePropMap[_1b]=_a._modelNodePropMap[_1b]||[]).push(_1a);
}
}
}
return _1a.id;
},getLabel:function(_1c){
return _1c===_c?"Object Properties":_1c.addNew?(_1c.parent instanceof Array?"Add new value":"Add new property"):_1c.property+": "+(_1c.value instanceof Array?"("+_1c.value.length+" elements)":_1c.value);
},onChildrenChange:function(_1d){
},onChange:function(_1e){
}};
},postCreate:function(){
this.inherited(arguments);
_2.connect(this,"onClick",function(_1f,_20){
this.lastFocused=_20;
if(_1f.addNew){
this._addProperty();
}else{
this._editProperty();
}
});
var _21=new _1.Menu({targetNodeIds:[this.rootNode.domNode],id:"contextMenu"});
_2.connect(_21,"_openMyself",this,function(e){
var _22=_1.getEnclosingWidget(e.target);
if(_22){
var _23=_22.item;
if(this.store.isItem(_23.value,true)&&!_23.parent){
_2.forEach(_21.getChildren(),function(_24){
_24.attr("disabled",(_24.label!="Add"));
});
this.lastFocused=_22;
}else{
if(_23.value&&typeof _23.value=="object"&&!(_23.value instanceof Date)){
_2.forEach(_21.getChildren(),function(_25){
_25.attr("disabled",(_25.label!="Add")&&(_25.label!="Delete"));
});
this.lastFocused=_22;
}else{
if(_23.property&&_2.indexOf(this.store.getIdentityAttributes(),_23.property)>=0){
this.focusNode(_22);
alert("Cannot modify an Identifier node.");
}else{
if(_23.addNew){
this.focusNode(_22);
}else{
_2.forEach(_21.getChildren(),function(_26){
_26.attr("disabled",(_26.label!="Edit")&&(_26.label!="Delete"));
});
this.lastFocused=_22;
}
}
}
}
}
});
_21.addChild(new _1.MenuItem({label:"Add",onClick:_2.hitch(this,"_addProperty")}));
_21.addChild(new _1.MenuItem({label:"Edit",onClick:_2.hitch(this,"_editProperty")}));
_21.addChild(new _1.MenuItem({label:"Delete",onClick:_2.hitch(this,"_destroyProperty")}));
_21.startup();
},store:null,setStore:function(_27){
this.store=_27;
var _28=this;
if(this._editDialog){
this._editDialog.destroyRecursive();
delete this._editDialog;
}
_2.connect(_27,"onSet",function(_29,_2a,_2b,_2c){
var _2d,i,_2e=_28.store.getIdentity(_29);
_2d=_28._modelNodeIdMap[_2e];
if(_2d&&(_2b===undefined||_2c===undefined||_2b instanceof Array||_2c instanceof Array||typeof _2b=="object"||typeof _2c=="object")){
for(i=0;i<_2d.length;i++){
(function(_2f){
_28.model.getChildren(_2f,function(_30){
_28.model.onChildrenChange(_2f,_30);
});
})(_2d[i]);
}
}
_2d=_28._modelNodePropMap[_2e+"."+_2a];
if(_2d){
for(i=0;i<_2d.length;i++){
_2d[i].value=_2c;
_28.model.onChange(_2d[i]);
}
}
});
this.rootNode.setChildItems([]);
},setItem:function(_31){
(this._modelNodeIdMap={})[this.store.getIdentity(_31)]=[this.rootModelNode];
this._modelNodePropMap={};
this.rootModelNode.value=_31;
var _32=this;
this.model.getChildren(this.rootModelNode,function(_33){
_32.rootNode.setChildItems(_33);
});
},refreshItem:function(){
this.setItem(this.rootModelNode.value);
},_createEditDialog:function(){
this._editDialog=new _1.Dialog({title:"Edit Property",execute:_2.hitch(this,"_updateItem"),preload:true});
this._editDialog.placeAt(_2.body());
this._editDialog.startup();
var _34=_2.doc.createElement("div");
var _35=_2.doc.createElement("label");
_2.attr(_35,"for","property");
_2.style(_35,"fontWeight","bold");
_2.attr(_35,"innerHTML","Property:");
_34.appendChild(_35);
var _36=new _1.form.ValidationTextBox({name:"property",value:"",required:true,disabled:true}).placeAt(_34);
_34.appendChild(_2.doc.createElement("br"));
_34.appendChild(_2.doc.createElement("br"));
var _37=new _1.form.RadioButton({name:"itemType",value:"value",onClick:_2.hitch(this,function(){
this._enableFields("value");
})}).placeAt(_34);
var _38=_2.doc.createElement("label");
_2.attr(_38,"for","value");
_2.attr(_38,"innerHTML","Value (JSON):");
_34.appendChild(_38);
var _39=_2.doc.createElement("div");
_2.addClass(_39,"value");
var _3a=new _1.form.Textarea({name:"jsonVal"}).placeAt(_39);
_34.appendChild(_39);
var _3b=new _1.form.RadioButton({name:"itemType",value:"reference",onClick:_2.hitch(this,function(){
this._enableFields("reference");
})}).placeAt(_34);
var _3c=_2.doc.createElement("label");
_2.attr(_3c,"for","_reference");
_2.attr(_3c,"innerHTML","Reference (ID):");
_34.appendChild(_3c);
_34.appendChild(_2.doc.createElement("br"));
var _3d=_2.doc.createElement("div");
_2.addClass(_3d,"reference");
if(this.useSelect){
var _3e=new _1.form.FilteringSelect({name:"_reference",store:this.store,searchAttr:this.refSelectSearchAttr||this.store.getIdentityAttributes()[0],required:false,value:null,pageSize:10}).placeAt(_3d);
}else{
var _3f=new _1.form.ValidationTextBox({name:"_reference",value:"",promptMessage:"Enter the ID of the item to reference",isValid:_2.hitch(this,function(_40){
return true;
})}).placeAt(_3d);
}
_34.appendChild(_3d);
_34.appendChild(_2.doc.createElement("br"));
_34.appendChild(_2.doc.createElement("br"));
var _41=document.createElement("div");
_41.setAttribute("dir","rtl");
var _42=new _1.form.Button({type:"reset",label:"Cancel"}).placeAt(_41);
_42.onClick=_2.hitch(this._editDialog,"onCancel");
var _43=new _1.form.Button({type:"submit",label:"OK"}).placeAt(_41);
_34.appendChild(_41);
this._editDialog.attr("content",_34);
},_enableFields:function(_44){
switch(_44){
case "reference":
_2.query(".value [widgetId]",this._editDialog.containerNode).forEach(function(_45){
_1.getEnclosingWidget(_45).attr("disabled",true);
});
_2.query(".reference [widgetId]",this._editDialog.containerNode).forEach(function(_46){
_1.getEnclosingWidget(_46).attr("disabled",false);
});
break;
case "value":
_2.query(".value [widgetId]",this._editDialog.containerNode).forEach(function(_47){
_1.getEnclosingWidget(_47).attr("disabled",false);
});
_2.query(".reference [widgetId]",this._editDialog.containerNode).forEach(function(_48){
_1.getEnclosingWidget(_48).attr("disabled",true);
});
break;
}
},_updateItem:function(_49){
var _4a,_4b,val,_4c,_4d=this._editDialog.attr("title")=="Edit Property";
var _4e=this._editDialog;
var _4f=this.store;
function _50(){
try{
var _51,_52=[];
var _53=_49.property;
if(_4d){
while(!_4f.isItem(_4b.parent,true)){
_4a=_4a.getParent();
_52.push(_4b.property);
_4b=_4a.item;
}
if(_52.length==0){
_4f.setValue(_4b.parent,_4b.property,val);
}else{
_4c=_4(_4f,_4b.parent,_4b.property);
if(_4c instanceof Array){
_4c=_4c.concat();
}
_51=_4c;
while(_52.length>1){
_51=_51[_52.pop()];
}
_51[_52]=val;
_4f.setValue(_4b.parent,_4b.property,_4c);
}
}else{
if(_4f.isItem(_54,true)){
if(!_4f.isItemLoaded(_54)){
_4f.loadItem({item:_54,onItem:function(_55){
if(_55 instanceof Array){
_53=_55.length;
}
_4f.setValue(_55,_53,val);
}});
}else{
if(_54 instanceof Array){
_53=_54.length;
}
_4f.setValue(_54,_53,val);
}
}else{
if(_4b.value instanceof Array){
_52.push(_4b.value.length);
}else{
_52.push(_49.property);
}
while(!_4f.isItem(_4b.parent,true)){
_4a=_4a.getParent();
_52.push(_4b.property);
_4b=_4a.item;
}
_4c=_4(_4f,_4b.parent,_4b.property);
_51=_4c;
while(_52.length>1){
_51=_51[_52.pop()];
}
_51[_52]=val;
_4f.setValue(_4b.parent,_4b.property,_4c);
}
}
}
catch(e){
alert(e);
}
};
if(_4e.validate()){
_4a=this.lastFocused;
_4b=_4a.item;
var _54=_4b.value;
if(_4b.addNew){
_54=_4a.item.parent;
_4a=_4a.getParent();
_4b=_4a.item;
}
val=null;
switch(_49.itemType){
case "reference":
this.store.fetchItemByIdentity({identity:_49._reference,onItem:function(_56){
val=_56;
_50();
},onError:function(){
alert("The id could not be found");
}});
break;
case "value":
var _57=_49.jsonVal;
val=_2.fromJson(_57);
if(typeof val=="function"){
val.toString=function(){
return _57;
};
}
_50();
break;
}
}else{
_4e.show();
}
},_editProperty:function(){
var _58=_2.mixin({},this.lastFocused.item);
if(!this._editDialog){
this._createEditDialog();
}else{
this._editDialog.reset();
}
if(_2.indexOf(this.store.getIdentityAttributes(),_58.property)>=0){
alert("Cannot Edit an Identifier!");
}else{
this._editDialog.attr("title","Edit Property");
_1.getEnclosingWidget(_2.query("input",this._editDialog.containerNode)[0]).attr("disabled",true);
if(this.store.isItem(_58.value,true)){
if(_58.parent){
_58.itemType="reference";
this._enableFields(_58.itemType);
_58._reference=this.store.getIdentity(_58.value);
this._editDialog.attr("value",_58);
this._editDialog.show();
}
}else{
if(_58.value&&typeof _58.value=="object"&&!(_58.value instanceof Date)){
}else{
_58.itemType="value";
this._enableFields(_58.itemType);
_58.jsonVal=typeof _58.value=="function"?_58.value.toString():_58.value instanceof Date?"new Date(\""+_58.value+"\")":_2.toJson(_58.value);
this._editDialog.attr("value",_58);
this._editDialog.show();
}
}
}
},_destroyProperty:function(){
var _59=this.lastFocused;
var _5a=_59.item;
var _5b=[];
while(!this.store.isItem(_5a.parent,true)||_5a.parent instanceof Array){
_59=_59.getParent();
_5b.push(_5a.property);
_5a=_59.item;
}
if(_2.indexOf(this.store.getIdentityAttributes(),_5a.property)>=0){
alert("Cannot Delete an Identifier!");
}else{
try{
if(_5b.length>0){
var _5c,_5d=_4(this.store,_5a.parent,_5a.property);
_5c=_5d;
while(_5b.length>1){
_5c=_5c[_5b.pop()];
}
if(_2.isArray(_5c)){
_5c.splice(_5b,1);
}else{
delete _5c[_5b];
}
this.store.setValue(_5a.parent,_5a.property,_5d);
}else{
this.store.unsetAttribute(_5a.parent,_5a.property);
}
}
catch(e){
alert(e);
}
}
},_addProperty:function(){
var _5e=this.lastFocused.item;
var _5f=_5e.value;
var _60=_2.hitch(this,function(){
var _61=null;
if(!this._editDialog){
this._createEditDialog();
}else{
this._editDialog.reset();
}
if(_5f instanceof Array){
_61=_5f.length;
_1.getEnclosingWidget(_2.query("input",this._editDialog.containerNode)[0]).attr("disabled",true);
}else{
_1.getEnclosingWidget(_2.query("input",this._editDialog.containerNode)[0]).attr("disabled",false);
}
this._editDialog.attr("title","Add Property");
this._enableFields("value");
this._editDialog.attr("value",{itemType:"value",property:_61});
this._editDialog.show();
});
if(_5e.addNew){
_5e=this.lastFocused.getParent().item;
_5f=this.lastFocused.item.parent;
}
if(_5e.property&&_2.indexOf(this.store.getIdentityAttributes(),_5e.property)>=0){
alert("Cannot add properties to an ID node!");
}else{
if(this.store.isItem(_5f,true)&&!this.store.isItemLoaded(_5f)){
this.store.loadItem({item:_5f,onItem:function(_62){
_5f=_62;
_60();
}});
}else{
_60();
}
}
}});
})();
});
