//>>built
define("dojox/data/XmlStore",["dojo/_base/lang","dojo/_base/declare","dojo/_base/xhr","dojo/data/util/simpleFetch","dojo/_base/query","dojo/_base/array","dojo/_base/window","dojo/data/util/filter","dojox/xml/parser","dojox/data/XmlItem"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var _b=_2("dojox.data.XmlStore",null,{constructor:function(_c){
if(_c){
this.url=_c.url;
this.rootItem=(_c.rootItem||_c.rootitem||this.rootItem);
this.keyAttribute=(_c.keyAttribute||_c.keyattribute||this.keyAttribute);
this._attributeMap=(_c.attributeMap||_c.attributemap);
this.label=_c.label||this.label;
this.sendQuery=(_c.sendQuery||_c.sendquery||this.sendQuery);
if("urlPreventCache" in _c){
this.urlPreventCache=_c.urlPreventCache?true:false;
}
}
this._newItems=[];
this._deletedItems=[];
this._modifiedItems=[];
},url:"",rootItem:"",keyAttribute:"",label:"",sendQuery:false,attributeMap:null,urlPreventCache:true,getValue:function(_d,_e,_f){
var _10=_d.element;
var i;
var _11;
if(_e==="tagName"){
return _10.nodeName;
}else{
if(_e==="childNodes"){
for(i=0;i<_10.childNodes.length;i++){
_11=_10.childNodes[i];
if(_11.nodeType===1){
return this._getItem(_11);
}
}
return _f;
}else{
if(_e==="text()"){
for(i=0;i<_10.childNodes.length;i++){
_11=_10.childNodes[i];
if(_11.nodeType===3||_11.nodeType===4){
return _11.nodeValue;
}
}
return _f;
}else{
_e=this._getAttribute(_10.nodeName,_e);
if(_e.charAt(0)==="@"){
var _12=_e.substring(1);
var _13=_10.getAttribute(_12);
return (_13)?_13:_f;
}else{
for(i=0;i<_10.childNodes.length;i++){
_11=_10.childNodes[i];
if(_11.nodeType===1&&_11.nodeName===_e){
return this._getItem(_11);
}
}
return _f;
}
}
}
}
},getValues:function(_14,_15){
var _16=_14.element;
var _17=[];
var i;
var _18;
if(_15==="tagName"){
return [_16.nodeName];
}else{
if(_15==="childNodes"){
for(i=0;i<_16.childNodes.length;i++){
_18=_16.childNodes[i];
if(_18.nodeType===1){
_17.push(this._getItem(_18));
}
}
return _17;
}else{
if(_15==="text()"){
var ec=_16.childNodes;
for(i=0;i<ec.length;i++){
_18=ec[i];
if(_18.nodeType===3||_18.nodeType===4){
_17.push(_18.nodeValue);
}
}
return _17;
}else{
_15=this._getAttribute(_16.nodeName,_15);
if(_15.charAt(0)==="@"){
var _19=_15.substring(1);
var _1a=_16.getAttribute(_19);
return (_1a!==undefined)?[_1a]:[];
}else{
for(i=0;i<_16.childNodes.length;i++){
_18=_16.childNodes[i];
if(_18.nodeType===1&&_18.nodeName===_15){
_17.push(this._getItem(_18));
}
}
return _17;
}
}
}
}
},getAttributes:function(_1b){
var _1c=_1b.element;
var _1d=[];
var i;
_1d.push("tagName");
if(_1c.childNodes.length>0){
var _1e={};
var _1f=true;
var _20=false;
for(i=0;i<_1c.childNodes.length;i++){
var _21=_1c.childNodes[i];
if(_21.nodeType===1){
var _22=_21.nodeName;
if(!_1e[_22]){
_1d.push(_22);
_1e[_22]=_22;
}
_1f=true;
}else{
if(_21.nodeType===3){
_20=true;
}
}
}
if(_1f){
_1d.push("childNodes");
}
if(_20){
_1d.push("text()");
}
}
for(i=0;i<_1c.attributes.length;i++){
_1d.push("@"+_1c.attributes[i].nodeName);
}
if(this._attributeMap){
for(var key in this._attributeMap){
i=key.indexOf(".");
if(i>0){
var _23=key.substring(0,i);
if(_23===_1c.nodeName){
_1d.push(key.substring(i+1));
}
}else{
_1d.push(key);
}
}
}
return _1d;
},hasAttribute:function(_24,_25){
return (this.getValue(_24,_25)!==undefined);
},containsValue:function(_26,_27,_28){
var _29=this.getValues(_26,_27);
for(var i=0;i<_29.length;i++){
if((typeof _28==="string")){
if(_29[i].toString&&_29[i].toString()===_28){
return true;
}
}else{
if(_29[i]===_28){
return true;
}
}
}
return false;
},isItem:function(_2a){
if(_2a&&_2a.element&&_2a.store&&_2a.store===this){
return true;
}
return false;
},isItemLoaded:function(_2b){
return this.isItem(_2b);
},loadItem:function(_2c){
},getFeatures:function(){
var _2d={"dojo.data.api.Read":true,"dojo.data.api.Write":true};
if(!this.sendQuery||this.keyAttribute!==""){
_2d["dojo.data.api.Identity"]=true;
}
return _2d;
},getLabel:function(_2e){
if((this.label!=="")&&this.isItem(_2e)){
var _2f=this.getValue(_2e,this.label);
if(_2f){
return _2f.toString();
}
}
return undefined;
},getLabelAttributes:function(_30){
if(this.label!==""){
return [this.label];
}
return null;
},_fetchItems:function(_31,_32,_33){
var url=this._getFetchUrl(_31);
if(!url){
_33(new Error("No URL specified."),_31);
return;
}
var _34=(!this.sendQuery?_31:{});
var _35=this;
var _36={url:url,handleAs:"xml",preventCache:_35.urlPreventCache};
var _37=_3.get(_36);
_37.addCallback(function(_38){
var _39=_35._getItems(_38,_34);
if(_39&&_39.length>0){
_32(_39,_31);
}else{
_32([],_31);
}
});
_37.addErrback(function(_3a){
_33(_3a,_31);
});
},_getFetchUrl:function(_3b){
if(!this.sendQuery){
return this.url;
}
var _3c=_3b.query;
if(!_3c){
return this.url;
}
if(_1.isString(_3c)){
return this.url+_3c;
}
var _3d="";
for(var _3e in _3c){
var _3f=_3c[_3e];
if(_3f){
if(_3d){
_3d+="&";
}
_3d+=(_3e+"="+_3f);
}
}
if(!_3d){
return this.url;
}
var _40=this.url;
if(_40.indexOf("?")<0){
_40+="?";
}else{
_40+="&";
}
return _40+_3d;
},_getItems:function(_41,_42){
var _43=null;
if(_42){
_43=_42.query;
}
var _44=[];
var _45=null;
if(this.rootItem!==""){
_45=_5(this.rootItem,_41);
}else{
_45=_41.documentElement.childNodes;
}
var _46=_42.queryOptions?_42.queryOptions.deep:false;
if(_46){
_45=this._flattenNodes(_45);
}
for(var i=0;i<_45.length;i++){
var _47=_45[i];
if(_47.nodeType!=1){
continue;
}
var _48=this._getItem(_47);
if(_43){
var _49=_42.queryOptions?_42.queryOptions.ignoreCase:false;
var _4a;
var _4b=false;
var j;
var _4c=true;
var _4d={};
for(var key in _43){
_4a=_43[key];
if(typeof _4a==="string"){
_4d[key]=_8.patternToRegExp(_4a,_49);
}else{
if(_4a){
_4d[key]=_4a;
}
}
}
for(var _4e in _43){
_4c=false;
var _4f=this.getValues(_48,_4e);
for(j=0;j<_4f.length;j++){
_4a=_4f[j];
if(_4a){
var _50=_43[_4e];
if((typeof _4a)==="string"&&(_4d[_4e])){
if((_4a.match(_4d[_4e]))!==null){
_4b=true;
}else{
_4b=false;
}
}else{
if((typeof _4a)==="object"){
if(_4a.toString&&(_4d[_4e])){
var _51=_4a.toString();
if((_51.match(_4d[_4e]))!==null){
_4b=true;
}else{
_4b=false;
}
}else{
if(_50==="*"||_50===_4a){
_4b=true;
}else{
_4b=false;
}
}
}
}
}
if(_4b){
break;
}
}
if(!_4b){
break;
}
}
if(_4c||_4b){
_44.push(_48);
}
}else{
_44.push(_48);
}
}
_6.forEach(_44,function(_52){
if(_52.element.parentNode){
_52.element.parentNode.removeChild(_52.element);
}
},this);
return _44;
},_flattenNodes:function(_53){
var _54=[];
if(_53){
var i;
for(i=0;i<_53.length;i++){
var _55=_53[i];
_54.push(_55);
if(_55.childNodes&&_55.childNodes.length>0){
_54=_54.concat(this._flattenNodes(_55.childNodes));
}
}
}
return _54;
},close:function(_56){
},newItem:function(_57,_58){
_57=(_57||{});
var _59=_57.tagName;
if(!_59){
_59=this.rootItem;
if(_59===""){
return null;
}
}
var _5a=this._getDocument();
var _5b=_5a.createElement(_59);
for(var _5c in _57){
var _5d;
if(_5c==="tagName"){
continue;
}else{
if(_5c==="text()"){
_5d=_5a.createTextNode(_57[_5c]);
_5b.appendChild(_5d);
}else{
_5c=this._getAttribute(_59,_5c);
if(_5c.charAt(0)==="@"){
var _5e=_5c.substring(1);
_5b.setAttribute(_5e,_57[_5c]);
}else{
var _5f=_5a.createElement(_5c);
_5d=_5a.createTextNode(_57[_5c]);
_5f.appendChild(_5d);
_5b.appendChild(_5f);
}
}
}
}
var _60=this._getItem(_5b);
this._newItems.push(_60);
var _61=null;
if(_58&&_58.parent&&_58.attribute){
_61={item:_58.parent,attribute:_58.attribute,oldValue:undefined};
var _62=this.getValues(_58.parent,_58.attribute);
if(_62&&_62.length>0){
var _63=_62.slice(0,_62.length);
if(_62.length===1){
_61.oldValue=_62[0];
}else{
_61.oldValue=_62.slice(0,_62.length);
}
_63.push(_60);
this.setValues(_58.parent,_58.attribute,_63);
_61.newValue=this.getValues(_58.parent,_58.attribute);
}else{
this.setValue(_58.parent,_58.attribute,_60);
_61.newValue=_60;
}
}
return _60;
},deleteItem:function(_64){
var _65=_64.element;
if(_65.parentNode){
this._backupItem(_64);
_65.parentNode.removeChild(_65);
return true;
}
this._forgetItem(_64);
this._deletedItems.push(_64);
return true;
},setValue:function(_66,_67,_68){
if(_67==="tagName"){
return false;
}
this._backupItem(_66);
var _69=_66.element;
var _6a;
var _6b;
if(_67==="childNodes"){
_6a=_68.element;
_69.appendChild(_6a);
}else{
if(_67==="text()"){
while(_69.firstChild){
_69.removeChild(_69.firstChild);
}
_6b=this._getDocument(_69).createTextNode(_68);
_69.appendChild(_6b);
}else{
_67=this._getAttribute(_69.nodeName,_67);
if(_67.charAt(0)==="@"){
var _6c=_67.substring(1);
_69.setAttribute(_6c,_68);
}else{
for(var i=0;i<_69.childNodes.length;i++){
var _6d=_69.childNodes[i];
if(_6d.nodeType===1&&_6d.nodeName===_67){
_6a=_6d;
break;
}
}
var _6e=this._getDocument(_69);
if(_6a){
while(_6a.firstChild){
_6a.removeChild(_6a.firstChild);
}
}else{
_6a=_6e.createElement(_67);
_69.appendChild(_6a);
}
_6b=_6e.createTextNode(_68);
_6a.appendChild(_6b);
}
}
}
return true;
},setValues:function(_6f,_70,_71){
if(_70==="tagName"){
return false;
}
this._backupItem(_6f);
var _72=_6f.element;
var i;
var _73;
var _74;
if(_70==="childNodes"){
while(_72.firstChild){
_72.removeChild(_72.firstChild);
}
for(i=0;i<_71.length;i++){
_73=_71[i].element;
_72.appendChild(_73);
}
}else{
if(_70==="text()"){
while(_72.firstChild){
_72.removeChild(_72.firstChild);
}
var _75="";
for(i=0;i<_71.length;i++){
_75+=_71[i];
}
_74=this._getDocument(_72).createTextNode(_75);
_72.appendChild(_74);
}else{
_70=this._getAttribute(_72.nodeName,_70);
if(_70.charAt(0)==="@"){
var _76=_70.substring(1);
_72.setAttribute(_76,_71[0]);
}else{
for(i=_72.childNodes.length-1;i>=0;i--){
var _77=_72.childNodes[i];
if(_77.nodeType===1&&_77.nodeName===_70){
_72.removeChild(_77);
}
}
var _78=this._getDocument(_72);
for(i=0;i<_71.length;i++){
_73=_78.createElement(_70);
_74=_78.createTextNode(_71[i]);
_73.appendChild(_74);
_72.appendChild(_73);
}
}
}
}
return true;
},unsetAttribute:function(_79,_7a){
if(_7a==="tagName"){
return false;
}
this._backupItem(_79);
var _7b=_79.element;
if(_7a==="childNodes"||_7a==="text()"){
while(_7b.firstChild){
_7b.removeChild(_7b.firstChild);
}
}else{
_7a=this._getAttribute(_7b.nodeName,_7a);
if(_7a.charAt(0)==="@"){
var _7c=_7a.substring(1);
_7b.removeAttribute(_7c);
}else{
for(var i=_7b.childNodes.length-1;i>=0;i--){
var _7d=_7b.childNodes[i];
if(_7d.nodeType===1&&_7d.nodeName===_7a){
_7b.removeChild(_7d);
}
}
}
}
return true;
},save:function(_7e){
if(!_7e){
_7e={};
}
var i;
for(i=0;i<this._modifiedItems.length;i++){
this._saveItem(this._modifiedItems[i],_7e,"PUT");
}
for(i=0;i<this._newItems.length;i++){
var _7f=this._newItems[i];
if(_7f.element.parentNode){
this._newItems.splice(i,1);
i--;
continue;
}
this._saveItem(this._newItems[i],_7e,"POST");
}
for(i=0;i<this._deletedItems.length;i++){
this._saveItem(this._deletedItems[i],_7e,"DELETE");
}
},revert:function(){
this._newItems=[];
this._restoreItems(this._deletedItems);
this._deletedItems=[];
this._restoreItems(this._modifiedItems);
this._modifiedItems=[];
return true;
},isDirty:function(_80){
if(_80){
var _81=this._getRootElement(_80.element);
return (this._getItemIndex(this._newItems,_81)>=0||this._getItemIndex(this._deletedItems,_81)>=0||this._getItemIndex(this._modifiedItems,_81)>=0);
}else{
return (this._newItems.length>0||this._deletedItems.length>0||this._modifiedItems.length>0);
}
},_saveItem:function(_82,_83,_84){
var url;
var _85;
if(_84==="PUT"){
url=this._getPutUrl(_82);
}else{
if(_84==="DELETE"){
url=this._getDeleteUrl(_82);
}else{
url=this._getPostUrl(_82);
}
}
if(!url){
if(_83.onError){
_85=_83.scope||_7.global;
_83.onError.call(_85,new Error("No URL for saving content: "+this._getPostContent(_82)));
}
return;
}
var _86={url:url,method:(_84||"POST"),contentType:"text/xml",handleAs:"xml"};
var _87;
if(_84==="PUT"){
_86.putData=this._getPutContent(_82);
_87=_3.put(_86);
}else{
if(_84==="DELETE"){
_87=_3.del(_86);
}else{
_86.postData=this._getPostContent(_82);
_87=_3.post(_86);
}
}
_85=(_83.scope||_7.global);
var _88=this;
_87.addCallback(function(_89){
_88._forgetItem(_82);
if(_83.onComplete){
_83.onComplete.call(_85);
}
});
_87.addErrback(function(_8a){
if(_83.onError){
_83.onError.call(_85,_8a);
}
});
},_getPostUrl:function(_8b){
return this.url;
},_getPutUrl:function(_8c){
return this.url;
},_getDeleteUrl:function(_8d){
var url=this.url;
if(_8d&&this.keyAttribute!==""){
var _8e=this.getValue(_8d,this.keyAttribute);
if(_8e){
var key=this.keyAttribute.charAt(0)==="@"?this.keyAttribute.substring(1):this.keyAttribute;
url+=url.indexOf("?")<0?"?":"&";
url+=key+"="+_8e;
}
}
return url;
},_getPostContent:function(_8f){
return "<?xml version='1.0'?>"+_9.innerXML(_8f.element);
},_getPutContent:function(_90){
return "<?xml version='1.0'?>"+_9.innerXML(_90.element);
},_getAttribute:function(_91,_92){
if(this._attributeMap){
var key=_91+"."+_92;
var _93=this._attributeMap[key];
if(_93){
_92=_93;
}else{
_93=this._attributeMap[_92];
if(_93){
_92=_93;
}
}
}
return _92;
},_getItem:function(_94){
try{
var q=null;
if(this.keyAttribute===""){
q=this._getXPath(_94);
}
return new _a(_94,this,q);
}
catch(e){
}
return null;
},_getItemIndex:function(_95,_96){
for(var i=0;i<_95.length;i++){
if(_95[i].element===_96){
return i;
}
}
return -1;
},_backupItem:function(_97){
var _98=this._getRootElement(_97.element);
if(this._getItemIndex(this._newItems,_98)>=0||this._getItemIndex(this._modifiedItems,_98)>=0){
return;
}
if(_98!=_97.element){
_97=this._getItem(_98);
}
_97._backup=_98.cloneNode(true);
this._modifiedItems.push(_97);
},_restoreItems:function(_99){
_6.forEach(_99,function(_9a){
if(_9a._backup){
_9a.element=_9a._backup;
_9a._backup=null;
}
},this);
},_forgetItem:function(_9b){
var _9c=_9b.element;
var _9d=this._getItemIndex(this._newItems,_9c);
if(_9d>=0){
this._newItems.splice(_9d,1);
}
_9d=this._getItemIndex(this._deletedItems,_9c);
if(_9d>=0){
this._deletedItems.splice(_9d,1);
}
_9d=this._getItemIndex(this._modifiedItems,_9c);
if(_9d>=0){
this._modifiedItems.splice(_9d,1);
}
},_getDocument:function(_9e){
if(_9e){
return _9e.ownerDocument;
}else{
if(!this._document){
return _9.parse();
}
}
return null;
},_getRootElement:function(_9f){
while(_9f.parentNode){
_9f=_9f.parentNode;
}
return _9f;
},_getXPath:function(_a0){
var _a1=null;
if(!this.sendQuery){
var _a2=_a0;
_a1="";
while(_a2&&_a2!=_a0.ownerDocument){
var pos=0;
var _a3=_a2;
var _a4=_a2.nodeName;
while(_a3){
_a3=_a3.previousSibling;
if(_a3&&_a3.nodeName===_a4){
pos++;
}
}
var _a5="/"+_a4+"["+pos+"]";
if(_a1){
_a1=_a5+_a1;
}else{
_a1=_a5;
}
_a2=_a2.parentNode;
}
}
return _a1;
},getIdentity:function(_a6){
if(!this.isItem(_a6)){
throw new Error("dojox.data.XmlStore: Object supplied to getIdentity is not an item");
}else{
var id=null;
if(this.sendQuery&&this.keyAttribute!==""){
id=this.getValue(_a6,this.keyAttribute).toString();
}else{
if(!this.serverQuery){
if(this.keyAttribute!==""){
id=this.getValue(_a6,this.keyAttribute).toString();
}else{
id=_a6.q;
}
}
}
return id;
}
},getIdentityAttributes:function(_a7){
if(!this.isItem(_a7)){
throw new Error("dojox.data.XmlStore: Object supplied to getIdentity is not an item");
}else{
if(this.keyAttribute!==""){
return [this.keyAttribute];
}else{
return null;
}
}
},fetchItemByIdentity:function(_a8){
var _a9=null;
var _aa=null;
var _ab=this;
var url=null;
var _ac=null;
var _ad=null;
if(!_ab.sendQuery){
_a9=function(_ae){
if(_ae){
if(_ab.keyAttribute!==""){
var _af={};
_af.query={};
_af.query[_ab.keyAttribute]=_a8.identity;
_af.queryOptions={deep:true};
var _b0=_ab._getItems(_ae,_af);
_aa=_a8.scope||_7.global;
if(_b0.length===1){
if(_a8.onItem){
_a8.onItem.call(_aa,_b0[0]);
}
}else{
if(_b0.length===0){
if(_a8.onItem){
_a8.onItem.call(_aa,null);
}
}else{
if(_a8.onError){
_a8.onError.call(_aa,new Error("Items array size for identity lookup greater than 1, invalid keyAttribute."));
}
}
}
}else{
var _b1=_a8.identity.split("/");
var i;
var _b2=_ae;
for(i=0;i<_b1.length;i++){
if(_b1[i]&&_b1[i]!==""){
var _b3=_b1[i];
_b3=_b3.substring(0,_b3.length-1);
var _b4=_b3.split("[");
var tag=_b4[0];
var _b5=parseInt(_b4[1],10);
var pos=0;
if(_b2){
var _b6=_b2.childNodes;
if(_b6){
var j;
var _b7=null;
for(j=0;j<_b6.length;j++){
var _b8=_b6[j];
if(_b8.nodeName===tag){
if(pos<_b5){
pos++;
}else{
_b7=_b8;
break;
}
}
}
if(_b7){
_b2=_b7;
}else{
_b2=null;
}
}else{
_b2=null;
}
}else{
break;
}
}
}
var _b9=null;
if(_b2){
_b9=_ab._getItem(_b2);
if(_b9.element.parentNode){
_b9.element.parentNode.removeChild(_b9.element);
}
}
if(_a8.onItem){
_aa=_a8.scope||_7.global;
_a8.onItem.call(_aa,_b9);
}
}
}
};
url=this._getFetchUrl(null);
_ac={url:url,handleAs:"xml",preventCache:_ab.urlPreventCache};
_ad=_3.get(_ac);
_ad.addCallback(_a9);
if(_a8.onError){
_ad.addErrback(function(_ba){
var s=_a8.scope||_7.global;
_a8.onError.call(s,_ba);
});
}
}else{
if(_ab.keyAttribute!==""){
var _bb={query:{}};
_bb.query[_ab.keyAttribute]=_a8.identity;
url=this._getFetchUrl(_bb);
_a9=function(_bc){
var _bd=null;
if(_bc){
var _be=_ab._getItems(_bc,{});
if(_be.length===1){
_bd=_be[0];
}else{
if(_a8.onError){
var _bf=_a8.scope||_7.global;
_a8.onError.call(_bf,new Error("More than one item was returned from the server for the denoted identity"));
}
}
}
if(_a8.onItem){
_bf=_a8.scope||_7.global;
_a8.onItem.call(_bf,_bd);
}
};
_ac={url:url,handleAs:"xml",preventCache:_ab.urlPreventCache};
_ad=_3.get(_ac);
_ad.addCallback(_a9);
if(_a8.onError){
_ad.addErrback(function(_c0){
var s=_a8.scope||_7.global;
_a8.onError.call(s,_c0);
});
}
}else{
if(_a8.onError){
var s=_a8.scope||_7.global;
_a8.onError.call(s,new Error("XmlStore is not told that the server to provides identity support.  No keyAttribute specified."));
}
}
}
}});
_1.extend(_b,_4);
return _b;
});
