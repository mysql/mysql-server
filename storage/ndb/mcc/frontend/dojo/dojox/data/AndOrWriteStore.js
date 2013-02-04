//>>built
define("dojox/data/AndOrWriteStore",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/json","dojo/date/stamp","dojo/_base/window","./AndOrReadStore"],function(_1,_2,_3,_4,_5,_6,_7){
return _1("dojox.data.AndOrWriteStore",_7,{constructor:function(_8){
this._features["dojo.data.api.Write"]=true;
this._features["dojo.data.api.Notification"]=true;
this._pending={_newItems:{},_modifiedItems:{},_deletedItems:{}};
if(!this._datatypeMap["Date"].serialize){
this._datatypeMap["Date"].serialize=function(_9){
return _5.toISOString(_9,{zulu:true});
};
}
if(_8&&(_8.referenceIntegrity===false)){
this.referenceIntegrity=false;
}
this._saveInProgress=false;
},referenceIntegrity:true,_assert:function(_a){
if(!_a){
throw new Error("assertion failed in ItemFileWriteStore");
}
},_getIdentifierAttribute:function(){
var _b=this.getFeatures()["dojo.data.api.Identity"];
return _b;
},newItem:function(_c,_d){
this._assert(!this._saveInProgress);
if(!this._loadFinished){
this._forceLoad();
}
if(typeof _c!="object"&&typeof _c!="undefined"){
throw new Error("newItem() was passed something other than an object");
}
var _e=null;
var _f=this._getIdentifierAttribute();
if(_f===Number){
_e=this._arrayOfAllItems.length;
}else{
_e=_c[_f];
if(typeof _e==="undefined"){
throw new Error("newItem() was not passed an identity for the new item");
}
if(_2.isArray(_e)){
throw new Error("newItem() was not passed an single-valued identity");
}
}
if(this._itemsByIdentity){
this._assert(typeof this._itemsByIdentity[_e]==="undefined");
}
this._assert(typeof this._pending._newItems[_e]==="undefined");
this._assert(typeof this._pending._deletedItems[_e]==="undefined");
var _10={};
_10[this._storeRefPropName]=this;
_10[this._itemNumPropName]=this._arrayOfAllItems.length;
if(this._itemsByIdentity){
this._itemsByIdentity[_e]=_10;
_10[_f]=[_e];
}
this._arrayOfAllItems.push(_10);
var _11=null;
if(_d&&_d.parent&&_d.attribute){
_11={item:_d.parent,attribute:_d.attribute,oldValue:undefined};
var _12=this.getValues(_d.parent,_d.attribute);
if(_12&&_12.length>0){
var _13=_12.slice(0,_12.length);
if(_12.length===1){
_11.oldValue=_12[0];
}else{
_11.oldValue=_12.slice(0,_12.length);
}
_13.push(_10);
this._setValueOrValues(_d.parent,_d.attribute,_13,false);
_11.newValue=this.getValues(_d.parent,_d.attribute);
}else{
this._setValueOrValues(_d.parent,_d.attribute,_10,false);
_11.newValue=_10;
}
}else{
_10[this._rootItemPropName]=true;
this._arrayOfTopLevelItems.push(_10);
}
this._pending._newItems[_e]=_10;
for(var key in _c){
if(key===this._storeRefPropName||key===this._itemNumPropName){
throw new Error("encountered bug in ItemFileWriteStore.newItem");
}
var _14=_c[key];
if(!_2.isArray(_14)){
_14=[_14];
}
_10[key]=_14;
if(this.referenceIntegrity){
for(var i=0;i<_14.length;i++){
var val=_14[i];
if(this.isItem(val)){
this._addReferenceToMap(val,_10,key);
}
}
}
}
this.onNew(_10,_11);
return _10;
},_removeArrayElement:function(_15,_16){
var _17=_3.indexOf(_15,_16);
if(_17!=-1){
_15.splice(_17,1);
return true;
}
return false;
},deleteItem:function(_18){
this._assert(!this._saveInProgress);
this._assertIsItem(_18);
var _19=_18[this._itemNumPropName];
var _1a=this.getIdentity(_18);
if(this.referenceIntegrity){
var _1b=this.getAttributes(_18);
if(_18[this._reverseRefMap]){
_18["backup_"+this._reverseRefMap]=_2.clone(_18[this._reverseRefMap]);
}
_3.forEach(_1b,function(_1c){
_3.forEach(this.getValues(_18,_1c),function(_1d){
if(this.isItem(_1d)){
if(!_18["backupRefs_"+this._reverseRefMap]){
_18["backupRefs_"+this._reverseRefMap]=[];
}
_18["backupRefs_"+this._reverseRefMap].push({id:this.getIdentity(_1d),attr:_1c});
this._removeReferenceFromMap(_1d,_18,_1c);
}
},this);
},this);
var _1e=_18[this._reverseRefMap];
if(_1e){
for(var _1f in _1e){
var _20=null;
if(this._itemsByIdentity){
_20=this._itemsByIdentity[_1f];
}else{
_20=this._arrayOfAllItems[_1f];
}
if(_20){
for(var _21 in _1e[_1f]){
var _22=this.getValues(_20,_21)||[];
var _23=_3.filter(_22,function(_24){
return !(this.isItem(_24)&&this.getIdentity(_24)==_1a);
},this);
this._removeReferenceFromMap(_18,_20,_21);
if(_23.length<_22.length){
this._setValueOrValues(_20,_21,_23);
}
}
}
}
}
}
this._arrayOfAllItems[_19]=null;
_18[this._storeRefPropName]=null;
if(this._itemsByIdentity){
delete this._itemsByIdentity[_1a];
}
this._pending._deletedItems[_1a]=_18;
if(_18[this._rootItemPropName]){
this._removeArrayElement(this._arrayOfTopLevelItems,_18);
}
this.onDelete(_18);
return true;
},setValue:function(_25,_26,_27){
return this._setValueOrValues(_25,_26,_27,true);
},setValues:function(_28,_29,_2a){
return this._setValueOrValues(_28,_29,_2a,true);
},unsetAttribute:function(_2b,_2c){
return this._setValueOrValues(_2b,_2c,[],true);
},_setValueOrValues:function(_2d,_2e,_2f,_30){
this._assert(!this._saveInProgress);
this._assertIsItem(_2d);
this._assert(_2.isString(_2e));
this._assert(typeof _2f!=="undefined");
var _31=this._getIdentifierAttribute();
if(_2e==_31){
throw new Error("ItemFileWriteStore does not have support for changing the value of an item's identifier.");
}
var _32=this._getValueOrValues(_2d,_2e);
var _33=this.getIdentity(_2d);
if(!this._pending._modifiedItems[_33]){
var _34={};
for(var key in _2d){
if((key===this._storeRefPropName)||(key===this._itemNumPropName)||(key===this._rootItemPropName)){
_34[key]=_2d[key];
}else{
if(key===this._reverseRefMap){
_34[key]=_2.clone(_2d[key]);
}else{
_34[key]=_2d[key].slice(0,_2d[key].length);
}
}
}
this._pending._modifiedItems[_33]=_34;
}
var _35=false;
if(_2.isArray(_2f)&&_2f.length===0){
_35=delete _2d[_2e];
_2f=undefined;
if(this.referenceIntegrity&&_32){
var _36=_32;
if(!_2.isArray(_36)){
_36=[_36];
}
for(var i=0;i<_36.length;i++){
var _37=_36[i];
if(this.isItem(_37)){
this._removeReferenceFromMap(_37,_2d,_2e);
}
}
}
}else{
var _38;
if(_2.isArray(_2f)){
var _39=_2f;
_38=_2f.slice(0,_2f.length);
}else{
_38=[_2f];
}
if(this.referenceIntegrity){
if(_32){
var _36=_32;
if(!_2.isArray(_36)){
_36=[_36];
}
var map={};
_3.forEach(_36,function(_3a){
if(this.isItem(_3a)){
var id=this.getIdentity(_3a);
map[id.toString()]=true;
}
},this);
_3.forEach(_38,function(_3b){
if(this.isItem(_3b)){
var id=this.getIdentity(_3b);
if(map[id.toString()]){
delete map[id.toString()];
}else{
this._addReferenceToMap(_3b,_2d,_2e);
}
}
},this);
for(var rId in map){
var _3c;
if(this._itemsByIdentity){
_3c=this._itemsByIdentity[rId];
}else{
_3c=this._arrayOfAllItems[rId];
}
this._removeReferenceFromMap(_3c,_2d,_2e);
}
}else{
for(var i=0;i<_38.length;i++){
var _37=_38[i];
if(this.isItem(_37)){
this._addReferenceToMap(_37,_2d,_2e);
}
}
}
}
_2d[_2e]=_38;
_35=true;
}
if(_30){
this.onSet(_2d,_2e,_32,_2f);
}
return _35;
},_addReferenceToMap:function(_3d,_3e,_3f){
var _40=this.getIdentity(_3e);
var _41=_3d[this._reverseRefMap];
if(!_41){
_41=_3d[this._reverseRefMap]={};
}
var _42=_41[_40];
if(!_42){
_42=_41[_40]={};
}
_42[_3f]=true;
},_removeReferenceFromMap:function(_43,_44,_45){
var _46=this.getIdentity(_44);
var _47=_43[this._reverseRefMap];
var _48;
if(_47){
for(_48 in _47){
if(_48==_46){
delete _47[_48][_45];
if(this._isEmpty(_47[_48])){
delete _47[_48];
}
}
}
if(this._isEmpty(_47)){
delete _43[this._reverseRefMap];
}
}
},_dumpReferenceMap:function(){
var i;
for(i=0;i<this._arrayOfAllItems.length;i++){
var _49=this._arrayOfAllItems[i];
if(_49&&_49[this._reverseRefMap]){
}
}
},_getValueOrValues:function(_4a,_4b){
var _4c=undefined;
if(this.hasAttribute(_4a,_4b)){
var _4d=this.getValues(_4a,_4b);
if(_4d.length==1){
_4c=_4d[0];
}else{
_4c=_4d;
}
}
return _4c;
},_flatten:function(_4e){
if(this.isItem(_4e)){
var _4f=_4e;
var _50=this.getIdentity(_4f);
var _51={_reference:_50};
return _51;
}else{
if(typeof _4e==="object"){
for(var _52 in this._datatypeMap){
var _53=this._datatypeMap[_52];
if(_2.isObject(_53)&&!_2.isFunction(_53)){
if(_4e instanceof _53.type){
if(!_53.serialize){
throw new Error("ItemFileWriteStore:  No serializer defined for type mapping: ["+_52+"]");
}
return {_type:_52,_value:_53.serialize(_4e)};
}
}else{
if(_4e instanceof _53){
return {_type:_52,_value:_4e.toString()};
}
}
}
}
return _4e;
}
},_getNewFileContentString:function(){
var _54={};
var _55=this._getIdentifierAttribute();
if(_55!==Number){
_54.identifier=_55;
}
if(this._labelAttr){
_54.label=this._labelAttr;
}
_54.items=[];
for(var i=0;i<this._arrayOfAllItems.length;++i){
var _56=this._arrayOfAllItems[i];
if(_56!==null){
var _57={};
for(var key in _56){
if(key!==this._storeRefPropName&&key!==this._itemNumPropName&&key!==this._reverseRefMap&&key!==this._rootItemPropName){
var _58=key;
var _59=this.getValues(_56,_58);
if(_59.length==1){
_57[_58]=this._flatten(_59[0]);
}else{
var _5a=[];
for(var j=0;j<_59.length;++j){
_5a.push(this._flatten(_59[j]));
_57[_58]=_5a;
}
}
}
}
_54.items.push(_57);
}
}
var _5b=true;
return _4.toJson(_54,_5b);
},_isEmpty:function(_5c){
var _5d=true;
if(_2.isObject(_5c)){
var i;
for(i in _5c){
_5d=false;
break;
}
}else{
if(_2.isArray(_5c)){
if(_5c.length>0){
_5d=false;
}
}
}
return _5d;
},save:function(_5e){
this._assert(!this._saveInProgress);
this._saveInProgress=true;
var _5f=this;
var _60=function(){
_5f._pending={_newItems:{},_modifiedItems:{},_deletedItems:{}};
_5f._saveInProgress=false;
if(_5e&&_5e.onComplete){
var _61=_5e.scope||_6.global;
_5e.onComplete.call(_61);
}
};
var _62=function(){
_5f._saveInProgress=false;
if(_5e&&_5e.onError){
var _63=_5e.scope||_6.global;
_5e.onError.call(_63);
}
};
if(this._saveEverything){
var _64=this._getNewFileContentString();
this._saveEverything(_60,_62,_64);
}
if(this._saveCustom){
this._saveCustom(_60,_62);
}
if(!this._saveEverything&&!this._saveCustom){
_60();
}
},revert:function(){
this._assert(!this._saveInProgress);
var _65;
for(_65 in this._pending._modifiedItems){
var _66=this._pending._modifiedItems[_65];
var _67=null;
if(this._itemsByIdentity){
_67=this._itemsByIdentity[_65];
}else{
_67=this._arrayOfAllItems[_65];
}
_66[this._storeRefPropName]=this;
for(key in _67){
delete _67[key];
}
_2.mixin(_67,_66);
}
var _68;
for(_65 in this._pending._deletedItems){
_68=this._pending._deletedItems[_65];
_68[this._storeRefPropName]=this;
var _69=_68[this._itemNumPropName];
if(_68["backup_"+this._reverseRefMap]){
_68[this._reverseRefMap]=_68["backup_"+this._reverseRefMap];
delete _68["backup_"+this._reverseRefMap];
}
this._arrayOfAllItems[_69]=_68;
if(this._itemsByIdentity){
this._itemsByIdentity[_65]=_68;
}
if(_68[this._rootItemPropName]){
this._arrayOfTopLevelItems.push(_68);
}
}
for(_65 in this._pending._deletedItems){
_68=this._pending._deletedItems[_65];
if(_68["backupRefs_"+this._reverseRefMap]){
_3.forEach(_68["backupRefs_"+this._reverseRefMap],function(_6a){
var _6b;
if(this._itemsByIdentity){
_6b=this._itemsByIdentity[_6a.id];
}else{
_6b=this._arrayOfAllItems[_6a.id];
}
this._addReferenceToMap(_6b,_68,_6a.attr);
},this);
delete _68["backupRefs_"+this._reverseRefMap];
}
}
for(_65 in this._pending._newItems){
var _6c=this._pending._newItems[_65];
_6c[this._storeRefPropName]=null;
this._arrayOfAllItems[_6c[this._itemNumPropName]]=null;
if(_6c[this._rootItemPropName]){
this._removeArrayElement(this._arrayOfTopLevelItems,_6c);
}
if(this._itemsByIdentity){
delete this._itemsByIdentity[_65];
}
}
this._pending={_newItems:{},_modifiedItems:{},_deletedItems:{}};
return true;
},isDirty:function(_6d){
if(_6d){
var _6e=this.getIdentity(_6d);
return new Boolean(this._pending._newItems[_6e]||this._pending._modifiedItems[_6e]||this._pending._deletedItems[_6e]).valueOf();
}else{
if(!this._isEmpty(this._pending._newItems)||!this._isEmpty(this._pending._modifiedItems)||!this._isEmpty(this._pending._deletedItems)){
return true;
}
return false;
}
},onSet:function(_6f,_70,_71,_72){
},onNew:function(_73,_74){
},onDelete:function(_75){
},close:function(_76){
if(this.clearOnClose){
if(!this.isDirty()){
this.inherited(arguments);
}else{
throw new Error("dojox.data.AndOrWriteStore: There are unsaved changes present in the store.  Please save or revert the changes before invoking close.");
}
}
}});
});
