//>>built
require({cache:{"url:dijit/templates/TreeNode.html":"<div class=\"dijitTreeNode\" role=\"presentation\"\n\t><div data-dojo-attach-point=\"rowNode\" class=\"dijitTreeRow dijitInline\" role=\"presentation\"\n\t\t><div data-dojo-attach-point=\"indentNode\" class=\"dijitInline\"></div\n\t\t><img src=\"${_blankGif}\" alt=\"\" data-dojo-attach-point=\"expandoNode\" class=\"dijitTreeExpando\" role=\"presentation\"\n\t\t/><span data-dojo-attach-point=\"expandoNodeText\" class=\"dijitExpandoText\" role=\"presentation\"\n\t\t></span\n\t\t><span data-dojo-attach-point=\"contentNode\"\n\t\t\tclass=\"dijitTreeContent\" role=\"presentation\">\n\t\t\t<img src=\"${_blankGif}\" alt=\"\" data-dojo-attach-point=\"iconNode\" class=\"dijitIcon dijitTreeIcon\" role=\"presentation\"\n\t\t\t/><span data-dojo-attach-point=\"labelNode\" class=\"dijitTreeLabel\" role=\"treeitem\"\n\t\t\t\t\ttabindex=\"-1\" aria-selected=\"false\" id=\"${id}_label\"></span>\n\t\t</span\n\t></div>\n\t<div data-dojo-attach-point=\"containerNode\" class=\"dijitTreeContainer\" role=\"presentation\"\n\t\t style=\"display: none;\" aria-labelledby=\"${id}_label\"></div>\n</div>\n","url:dijit/templates/Tree.html":"<div class=\"dijitTree dijitTreeContainer\" role=\"tree\">\n\t<div class=\"dijitInline dijitTreeIndent\" style=\"position: absolute; top: -9999px\" data-dojo-attach-point=\"indentDetector\"></div>\n</div>\n"}});
define("dijit/Tree",["dojo/_base/array","dojo/_base/connect","dojo/cookie","dojo/_base/declare","dojo/Deferred","dojo/DeferredList","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/errors/create","dojo/fx","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/on","dojo/topic","dojo/touch","dojo/when","./focus","./registry","./_base/manager","./_Widget","./_TemplatedMixin","./_Container","./_Contained","./_CssStateMixin","dojo/text!./templates/TreeNode.html","dojo/text!./templates/Tree.html","./tree/TreeStoreModel","./tree/ForestStoreModel","./tree/_dndSelector"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,on,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b,_1c,_1d,_1e,_1f,_20){
_5=_4(_5,{addCallback:function(_21){
this.then(_21);
},addErrback:function(_22){
this.then(null,_22);
}});
var _23=_4("dijit._TreeNode",[_17,_18,_19,_1a,_1b],{item:null,isTreeNode:true,label:"",_setLabelAttr:{node:"labelNode",type:"innerText"},isExpandable:null,isExpanded:false,state:"UNCHECKED",templateString:_1c,baseClass:"dijitTreeNode",cssStateNodes:{rowNode:"dijitTreeRow"},_setTooltipAttr:{node:"rowNode",type:"attribute",attribute:"title"},buildRendering:function(){
this.inherited(arguments);
this._setExpando();
this._updateItemClasses(this.item);
if(this.isExpandable){
this.labelNode.setAttribute("aria-expanded",this.isExpanded);
}
this.setSelected(false);
},_setIndentAttr:function(_24){
var _25=(Math.max(_24,0)*this.tree._nodePixelIndent)+"px";
_a.set(this.domNode,"backgroundPosition",_25+" 0px");
_a.set(this.indentNode,this.isLeftToRight()?"paddingLeft":"paddingRight",_25);
_1.forEach(this.getChildren(),function(_26){
_26.set("indent",_24+1);
});
this._set("indent",_24);
},markProcessing:function(){
this.state="LOADING";
this._setExpando(true);
},unmarkProcessing:function(){
this._setExpando(false);
},_updateItemClasses:function(_27){
var _28=this.tree,_29=_28.model;
if(_28._v10Compat&&_27===_29.root){
_27=null;
}
this._applyClassAndStyle(_27,"icon","Icon");
this._applyClassAndStyle(_27,"label","Label");
this._applyClassAndStyle(_27,"row","Row");
this.tree._startPaint(true);
},_applyClassAndStyle:function(_2a,_2b,_2c){
var _2d="_"+_2b+"Class";
var _2e=_2b+"Node";
var _2f=this[_2d];
this[_2d]=this.tree["get"+_2c+"Class"](_2a,this.isExpanded);
_8.replace(this[_2e],this[_2d]||"",_2f||"");
_a.set(this[_2e],this.tree["get"+_2c+"Style"](_2a,this.isExpanded)||{});
},_updateLayout:function(){
var _30=this.getParent();
if(!_30||!_30.rowNode||_30.rowNode.style.display=="none"){
_8.add(this.domNode,"dijitTreeIsRoot");
}else{
_8.toggle(this.domNode,"dijitTreeIsLast",!this.getNextSibling());
}
},_setExpando:function(_31){
var _32=["dijitTreeExpandoLoading","dijitTreeExpandoOpened","dijitTreeExpandoClosed","dijitTreeExpandoLeaf"],_33=["*","-","+","*"],idx=_31?0:(this.isExpandable?(this.isExpanded?1:2):3);
_8.replace(this.expandoNode,_32[idx],_32);
this.expandoNodeText.innerHTML=_33[idx];
},expand:function(){
if(this._expandDeferred){
return this._expandDeferred;
}
if(this._collapseDeferred){
this._collapseDeferred.cancel();
delete this._collapseDeferred;
}
this.isExpanded=true;
this.labelNode.setAttribute("aria-expanded","true");
if(this.tree.showRoot||this!==this.tree.rootNode){
this.containerNode.setAttribute("role","group");
}
_8.add(this.contentNode,"dijitTreeContentExpanded");
this._setExpando();
this._updateItemClasses(this.item);
if(this==this.tree.rootNode&&this.tree.showRoot){
this.tree.domNode.setAttribute("aria-expanded","true");
}
var def,_34=_d.wipeIn({node:this.containerNode,duration:_16.defaultDuration,onEnd:function(){
def.resolve(true);
}});
def=(this._expandDeferred=new _5(function(){
_34.stop();
}));
_34.play();
return def;
},collapse:function(){
if(this._collapseDeferred){
return this._collapseDeferred;
}
if(this._expandDeferred){
this._expandDeferred.cancel();
delete this._expandDeferred;
}
this.isExpanded=false;
this.labelNode.setAttribute("aria-expanded","false");
if(this==this.tree.rootNode&&this.tree.showRoot){
this.tree.domNode.setAttribute("aria-expanded","false");
}
_8.remove(this.contentNode,"dijitTreeContentExpanded");
this._setExpando();
this._updateItemClasses(this.item);
var def,_35=_d.wipeOut({node:this.containerNode,duration:_16.defaultDuration,onEnd:function(){
def.resolve(true);
}});
def=(this._collapseDeferred=new _5(function(){
_35.stop();
}));
_35.play();
return def;
},indent:0,setChildItems:function(_36){
var _37=this.tree,_38=_37.model,_39=[];
var _3a=this.getChildren();
_1.forEach(_3a,function(_3b){
_19.prototype.removeChild.call(this,_3b);
},this);
this.defer(function(){
_1.forEach(_3a,function(_3c){
if(!_3c._destroyed&&!_3c.getParent()){
_37.dndController.removeTreeNode(_3c);
var id=_38.getIdentity(_3c.item),ary=_37._itemNodesMap[id];
if(ary.length==1){
delete _37._itemNodesMap[id];
}else{
var _3d=_1.indexOf(ary,_3c);
if(_3d!=-1){
ary.splice(_3d,1);
}
}
_3c.destroyRecursive();
}
});
});
this.state="LOADED";
if(_36&&_36.length>0){
this.isExpandable=true;
_1.forEach(_36,function(_3e){
var id=_38.getIdentity(_3e),_3f=_37._itemNodesMap[id],_40;
if(_3f){
for(var i=0;i<_3f.length;i++){
if(_3f[i]&&!_3f[i].getParent()){
_40=_3f[i];
_40.set("indent",this.indent+1);
break;
}
}
}
if(!_40){
_40=this.tree._createTreeNode({item:_3e,tree:_37,isExpandable:_38.mayHaveChildren(_3e),label:_37.getLabel(_3e),tooltip:_37.getTooltip(_3e),ownerDocument:_37.ownerDocument,dir:_37.dir,lang:_37.lang,textDir:_37.textDir,indent:this.indent+1});
if(_3f){
_3f.push(_40);
}else{
_37._itemNodesMap[id]=[_40];
}
}
this.addChild(_40);
if(this.tree.autoExpand||this.tree._state(_40)){
_39.push(_37._expandNode(_40));
}
},this);
_1.forEach(this.getChildren(),function(_41){
_41._updateLayout();
});
}else{
this.isExpandable=false;
}
if(this._setExpando){
this._setExpando(false);
}
this._updateItemClasses(this.item);
if(this==_37.rootNode){
var fc=this.tree.showRoot?this:this.getChildren()[0];
if(fc){
fc.setFocusable(true);
_37.lastFocused=fc;
}else{
_37.domNode.setAttribute("tabIndex","0");
}
}
var def=new _6(_39);
this.tree._startPaint(def);
return def;
},getTreePath:function(){
var _42=this;
var _43=[];
while(_42&&_42!==this.tree.rootNode){
_43.unshift(_42.item);
_42=_42.getParent();
}
_43.unshift(this.tree.rootNode.item);
return _43;
},getIdentity:function(){
return this.tree.model.getIdentity(this.item);
},removeChild:function(_44){
this.inherited(arguments);
var _45=this.getChildren();
if(_45.length==0){
this.isExpandable=false;
this.collapse();
}
_1.forEach(_45,function(_46){
_46._updateLayout();
});
},makeExpandable:function(){
this.isExpandable=true;
this._setExpando(false);
},setSelected:function(_47){
this.labelNode.setAttribute("aria-selected",_47?"true":"false");
_8.toggle(this.rowNode,"dijitTreeRowSelected",_47);
},setFocusable:function(_48){
this.labelNode.setAttribute("tabIndex",_48?"0":"-1");
},_setTextDirAttr:function(_49){
if(_49&&((this.textDir!=_49)||!this._created)){
this._set("textDir",_49);
this.applyTextDir(this.labelNode,this.labelNode.innerText||this.labelNode.textContent||"");
_1.forEach(this.getChildren(),function(_4a){
_4a.set("textDir",_49);
},this);
}
}});
var _4b=_4("dijit.Tree",[_17,_18],{store:null,model:null,query:null,label:"",showRoot:true,childrenAttr:["children"],paths:[],path:[],selectedItems:null,selectedItem:null,openOnClick:false,openOnDblClick:false,templateString:_1d,persist:true,autoExpand:false,dndController:_20,dndParams:["onDndDrop","itemCreator","onDndCancel","checkAcceptance","checkItemAcceptance","dragThreshold","betweenThreshold"],onDndDrop:null,itemCreator:null,onDndCancel:null,checkAcceptance:null,checkItemAcceptance:null,dragThreshold:5,betweenThreshold:0,_nodePixelIndent:19,_publish:function(_4c,_4d){
_11.publish(this.id,_10.mixin({tree:this,event:_4c},_4d||{}));
},postMixInProperties:function(){
this.tree=this;
if(this.autoExpand){
this.persist=false;
}
this._itemNodesMap={};
if(!this.cookieName&&this.id){
this.cookieName=this.id+"SaveStateCookie";
}
this.expandChildrenDeferred=new _5();
this.pendingCommandsDeferred=this.expandChildrenDeferred;
this.inherited(arguments);
},postCreate:function(){
this._initState();
var _4e=this;
this.own(on(this.domNode,on.selector(".dijitTreeNode",_12.enter),function(evt){
_4e._onNodeMouseEnter(_15.byNode(this),evt);
}),on(this.domNode,on.selector(".dijitTreeNode",_12.leave),function(evt){
_4e._onNodeMouseLeave(_15.byNode(this),evt);
}),on(this.domNode,on.selector(".dijitTreeRow","click"),function(evt){
_4e._onClick(_15.getEnclosingWidget(this),evt);
}),on(this.domNode,on.selector(".dijitTreeRow","dblclick"),function(evt){
_4e._onDblClick(_15.getEnclosingWidget(this),evt);
}),on(this.domNode,on.selector(".dijitTreeNode","keypress"),function(evt){
_4e._onKeyPress(_15.byNode(this),evt);
}),on(this.domNode,on.selector(".dijitTreeNode","keydown"),function(evt){
_4e._onKeyDown(_15.byNode(this),evt);
}),on(this.domNode,on.selector(".dijitTreeRow","focusin"),function(evt){
_4e._onNodeFocus(_15.getEnclosingWidget(this),evt);
}));
if(!this.model){
this._store2model();
}
this.connect(this.model,"onChange","_onItemChange");
this.connect(this.model,"onChildrenChange","_onItemChildrenChange");
this.connect(this.model,"onDelete","_onItemDelete");
this.inherited(arguments);
if(this.dndController){
if(_10.isString(this.dndController)){
this.dndController=_10.getObject(this.dndController);
}
var _4f={};
for(var i=0;i<this.dndParams.length;i++){
if(this[this.dndParams[i]]){
_4f[this.dndParams[i]]=this[this.dndParams[i]];
}
}
this.dndController=new this.dndController(this,_4f);
}
this._load();
if(!this.params.path&&!this.params.paths&&this.persist){
this.set("paths",this.dndController._getSavedPaths());
}
this.onLoadDeferred=this.pendingCommandsDeferred;
this.onLoadDeferred.then(_10.hitch(this,"onLoad"));
},_store2model:function(){
this._v10Compat=true;
_e.deprecated("Tree: from version 2.0, should specify a model object rather than a store/query");
var _50={id:this.id+"_ForestStoreModel",store:this.store,query:this.query,childrenAttrs:this.childrenAttr};
if(this.params.mayHaveChildren){
_50.mayHaveChildren=_10.hitch(this,"mayHaveChildren");
}
if(this.params.getItemChildren){
_50.getChildren=_10.hitch(this,function(_51,_52,_53){
this.getItemChildren((this._v10Compat&&_51===this.model.root)?null:_51,_52,_53);
});
}
this.model=new _1f(_50);
this.showRoot=Boolean(this.label);
},onLoad:function(){
},_load:function(){
this.model.getRoot(_10.hitch(this,function(_54){
var rn=(this.rootNode=this.tree._createTreeNode({item:_54,tree:this,isExpandable:true,label:this.label||this.getLabel(_54),textDir:this.textDir,indent:this.showRoot?0:-1}));
if(!this.showRoot){
rn.rowNode.style.display="none";
this.domNode.setAttribute("role","presentation");
this.domNode.removeAttribute("aria-expanded");
this.domNode.removeAttribute("aria-multiselectable");
if(this["aria-label"]){
rn.containerNode.setAttribute("aria-label",this["aria-label"]);
this.domNode.removeAttribute("aria-label");
}else{
if(this["aria-labelledby"]){
rn.containerNode.setAttribute("aria-labelledby",this["aria-labelledby"]);
this.domNode.removeAttribute("aria-labelledby");
}
}
rn.labelNode.setAttribute("role","presentation");
rn.containerNode.setAttribute("role","tree");
rn.containerNode.setAttribute("aria-expanded","true");
rn.containerNode.setAttribute("aria-multiselectable",!this.dndController.singular);
}else{
this.domNode.setAttribute("aria-multiselectable",!this.dndController.singular);
}
this.domNode.appendChild(rn.domNode);
var _55=this.model.getIdentity(_54);
if(this._itemNodesMap[_55]){
this._itemNodesMap[_55].push(rn);
}else{
this._itemNodesMap[_55]=[rn];
}
rn._updateLayout();
this._expandNode(rn).then(_10.hitch(this,function(){
this.expandChildrenDeferred.resolve(true);
}));
}),_10.hitch(this,function(err){
console.error(this,": error loading root: ",err);
}));
},getNodesByItem:function(_56){
if(!_56){
return [];
}
var _57=_10.isString(_56)?_56:this.model.getIdentity(_56);
return [].concat(this._itemNodesMap[_57]);
},_setSelectedItemAttr:function(_58){
this.set("selectedItems",[_58]);
},_setSelectedItemsAttr:function(_59){
var _5a=this;
return this.pendingCommandsDeferred=this.pendingCommandsDeferred.then(_10.hitch(this,function(){
var _5b=_1.map(_59,function(_5c){
return (!_5c||_10.isString(_5c))?_5c:_5a.model.getIdentity(_5c);
});
var _5d=[];
_1.forEach(_5b,function(id){
_5d=_5d.concat(_5a._itemNodesMap[id]||[]);
});
this.set("selectedNodes",_5d);
}));
},_setPathAttr:function(_5e){
if(_5e.length){
return this.set("paths",[_5e]);
}else{
return this.set("paths",[]);
}
},_setPathsAttr:function(_5f){
var _60=this;
return this.pendingCommandsDeferred=this.pendingCommandsDeferred.then(function(){
return new _6(_1.map(_5f,function(_61){
var d=new _5();
_61=_1.map(_61,function(_62){
return _10.isString(_62)?_62:_60.model.getIdentity(_62);
});
if(_61.length){
_63(_61,[_60.rootNode],d);
}else{
d.reject(new _4b.PathError("Empty path"));
}
return d;
}));
}).then(_64);
function _63(_65,_66,def){
var _67=_65.shift();
var _68=_1.filter(_66,function(_69){
return _69.getIdentity()==_67;
})[0];
if(!!_68){
if(_65.length){
_60._expandNode(_68).then(function(){
_63(_65,_68.getChildren(),def);
});
}else{
def.resolve(_68);
}
}else{
def.reject(new _4b.PathError("Could not expand path at "+_67));
}
};
function _64(_6a){
_60.set("selectedNodes",_1.map(_1.filter(_6a,function(x){
return x[0];
}),function(x){
return x[1];
}));
};
},_setSelectedNodeAttr:function(_6b){
this.set("selectedNodes",[_6b]);
},_setSelectedNodesAttr:function(_6c){
this.dndController.setSelection(_6c);
},expandAll:function(){
var _6d=this;
function _6e(_6f){
var def=new dojo.Deferred();
_6d._expandNode(_6f).then(function(){
var _70=_1.filter(_6f.getChildren()||[],function(_71){
return _71.isExpandable;
}),_72=_1.map(_70,_6e);
new dojo.DeferredList(_72).then(function(){
def.resolve(true);
});
});
return def;
};
return _6e(this.rootNode);
},collapseAll:function(){
var _73=this;
function _74(_75){
var def=new dojo.Deferred();
def.label="collapseAllDeferred";
var _76=_1.filter(_75.getChildren()||[],function(_77){
return _77.isExpandable;
}),_78=_1.map(_76,_74);
new dojo.DeferredList(_78).then(function(){
if(!_75.isExpanded||(_75==_73.rootNode&&!_73.showRoot)){
def.resolve(true);
}else{
_73._collapseNode(_75).then(function(){
def.resolve(true);
});
}
});
return def;
};
return _74(this.rootNode);
},mayHaveChildren:function(){
},getItemChildren:function(){
},getLabel:function(_79){
return this.model.getLabel(_79);
},getIconClass:function(_7a,_7b){
return (!_7a||this.model.mayHaveChildren(_7a))?(_7b?"dijitFolderOpened":"dijitFolderClosed"):"dijitLeaf";
},getLabelClass:function(){
},getRowClass:function(){
},getIconStyle:function(){
},getLabelStyle:function(){
},getRowStyle:function(){
},getTooltip:function(){
return "";
},_onKeyPress:function(_7c,e){
if(e.charCode<=32){
return;
}
if(!e.altKey&&!e.ctrlKey&&!e.shiftKey&&!e.metaKey){
var c=String.fromCharCode(e.charCode);
this._onLetterKeyNav({node:_7c,key:c.toLowerCase()});
_b.stop(e);
}
},_onKeyDown:function(_7d,e){
var key=e.keyCode;
var map=this._keyHandlerMap;
if(!map){
map={};
map[_f.ENTER]=map[_f.SPACE]=map[" "]="_onEnterKey";
map[this.isLeftToRight()?_f.LEFT_ARROW:_f.RIGHT_ARROW]="_onLeftArrow";
map[this.isLeftToRight()?_f.RIGHT_ARROW:_f.LEFT_ARROW]="_onRightArrow";
map[_f.UP_ARROW]="_onUpArrow";
map[_f.DOWN_ARROW]="_onDownArrow";
map[_f.HOME]="_onHomeKey";
map[_f.END]="_onEndKey";
this._keyHandlerMap=map;
}
if(this._keyHandlerMap[key]){
if(this._curSearch){
this._curSearch.timer.remove();
delete this._curSearch;
}
this[this._keyHandlerMap[key]]({node:_7d,item:_7d.item,evt:e});
_b.stop(e);
}
},_onEnterKey:function(_7e){
this._publish("execute",{item:_7e.item,node:_7e.node});
this.dndController.userSelect(_7e.node,_2.isCopyKey(_7e.evt),_7e.evt.shiftKey);
this.onClick(_7e.item,_7e.node,_7e.evt);
},_onDownArrow:function(_7f){
var _80=this._getNextNode(_7f.node);
if(_80&&_80.isTreeNode){
this.focusNode(_80);
}
},_onUpArrow:function(_81){
var _82=_81.node;
var _83=_82.getPreviousSibling();
if(_83){
_82=_83;
while(_82.isExpandable&&_82.isExpanded&&_82.hasChildren()){
var _84=_82.getChildren();
_82=_84[_84.length-1];
}
}else{
var _85=_82.getParent();
if(!(!this.showRoot&&_85===this.rootNode)){
_82=_85;
}
}
if(_82&&_82.isTreeNode){
this.focusNode(_82);
}
},_onRightArrow:function(_86){
var _87=_86.node;
if(_87.isExpandable&&!_87.isExpanded){
this._expandNode(_87);
}else{
if(_87.hasChildren()){
_87=_87.getChildren()[0];
if(_87&&_87.isTreeNode){
this.focusNode(_87);
}
}
}
},_onLeftArrow:function(_88){
var _89=_88.node;
if(_89.isExpandable&&_89.isExpanded){
this._collapseNode(_89);
}else{
var _8a=_89.getParent();
if(_8a&&_8a.isTreeNode&&!(!this.showRoot&&_8a===this.rootNode)){
this.focusNode(_8a);
}
}
},_onHomeKey:function(){
var _8b=this._getRootOrFirstNode();
if(_8b){
this.focusNode(_8b);
}
},_onEndKey:function(){
var _8c=this.rootNode;
while(_8c.isExpanded){
var c=_8c.getChildren();
_8c=c[c.length-1];
}
if(_8c&&_8c.isTreeNode){
this.focusNode(_8c);
}
},multiCharSearchDuration:250,_onLetterKeyNav:function(_8d){
var cs=this._curSearch;
if(cs){
cs.pattern=cs.pattern+_8d.key;
cs.timer.remove();
}else{
cs=this._curSearch={pattern:_8d.key,startNode:_8d.node};
}
cs.timer=this.defer(function(){
delete this._curSearch;
},this.multiCharSearchDuration);
var _8e=cs.startNode;
do{
_8e=this._getNextNode(_8e);
if(!_8e){
_8e=this._getRootOrFirstNode();
}
}while(_8e!==cs.startNode&&(_8e.label.toLowerCase().substr(0,cs.pattern.length)!=cs.pattern));
if(_8e&&_8e.isTreeNode){
if(_8e!==cs.startNode){
this.focusNode(_8e);
}
}
},isExpandoNode:function(_8f,_90){
return _7.isDescendant(_8f,_90.expandoNode)||_7.isDescendant(_8f,_90.expandoNodeText);
},_onClick:function(_91,e){
var _92=e.target,_93=this.isExpandoNode(_92,_91);
if((this.openOnClick&&_91.isExpandable)||_93){
if(_91.isExpandable){
this._onExpandoClick({node:_91});
}
}else{
this._publish("execute",{item:_91.item,node:_91,evt:e});
this.onClick(_91.item,_91,e);
this.focusNode(_91);
}
_b.stop(e);
},_onDblClick:function(_94,e){
var _95=e.target,_96=(_95==_94.expandoNode||_95==_94.expandoNodeText);
if((this.openOnDblClick&&_94.isExpandable)||_96){
if(_94.isExpandable){
this._onExpandoClick({node:_94});
}
}else{
this._publish("execute",{item:_94.item,node:_94,evt:e});
this.onDblClick(_94.item,_94,e);
this.focusNode(_94);
}
_b.stop(e);
},_onExpandoClick:function(_97){
var _98=_97.node;
this.focusNode(_98);
if(_98.isExpanded){
this._collapseNode(_98);
}else{
this._expandNode(_98);
}
},onClick:function(){
},onDblClick:function(){
},onOpen:function(){
},onClose:function(){
},_getNextNode:function(_99){
if(_99.isExpandable&&_99.isExpanded&&_99.hasChildren()){
return _99.getChildren()[0];
}else{
while(_99&&_99.isTreeNode){
var _9a=_99.getNextSibling();
if(_9a){
return _9a;
}
_99=_99.getParent();
}
return null;
}
},_getRootOrFirstNode:function(){
return this.showRoot?this.rootNode:this.rootNode.getChildren()[0];
},_collapseNode:function(_9b){
if(_9b._expandNodeDeferred){
delete _9b._expandNodeDeferred;
}
if(_9b.state=="LOADING"){
return;
}
if(_9b.isExpanded){
var ret=_9b.collapse();
this.onClose(_9b.item,_9b);
this._state(_9b,false);
this._startPaint(ret);
return ret;
}
},_expandNode:function(_9c){
var def=new _5();
if(_9c._expandNodeDeferred){
return _9c._expandNodeDeferred;
}
var _9d=this.model,_9e=_9c.item,_9f=this;
if(!_9c._loadDeferred){
_9c.markProcessing();
_9c._loadDeferred=new _5();
_9d.getChildren(_9e,function(_a0){
_9c.unmarkProcessing();
_9c.setChildItems(_a0).then(function(){
_9c._loadDeferred.resolve(_a0);
});
},function(err){
console.error(_9f,": error loading "+_9c.label+" children: ",err);
_9c._loadDeferred.reject(err);
});
}
_9c._loadDeferred.then(_10.hitch(this,function(){
_9c.expand().then(function(){
def.resolve(true);
});
this.onOpen(_9c.item,_9c);
this._state(_9c,true);
}));
this._startPaint(def);
return def;
},focusNode:function(_a1){
_14.focus(_a1.labelNode);
},_onNodeFocus:function(_a2){
if(_a2&&_a2!=this.lastFocused){
if(this.lastFocused&&!this.lastFocused._destroyed){
this.lastFocused.setFocusable(false);
}
_a2.setFocusable(true);
this.lastFocused=_a2;
}
},_onNodeMouseEnter:function(){
},_onNodeMouseLeave:function(){
},_onItemChange:function(_a3){
var _a4=this.model,_a5=_a4.getIdentity(_a3),_a6=this._itemNodesMap[_a5];
if(_a6){
var _a7=this.getLabel(_a3),_a8=this.getTooltip(_a3);
_1.forEach(_a6,function(_a9){
_a9.set({item:_a3,label:_a7,tooltip:_a8});
_a9._updateItemClasses(_a3);
});
}
},_onItemChildrenChange:function(_aa,_ab){
var _ac=this.model,_ad=_ac.getIdentity(_aa),_ae=this._itemNodesMap[_ad];
if(_ae){
_1.forEach(_ae,function(_af){
_af.setChildItems(_ab);
});
}
},_onItemDelete:function(_b0){
var _b1=this.model,_b2=_b1.getIdentity(_b0),_b3=this._itemNodesMap[_b2];
if(_b3){
_1.forEach(_b3,function(_b4){
this.dndController.removeTreeNode(_b4);
var _b5=_b4.getParent();
if(_b5){
_b5.removeChild(_b4);
}
_b4.destroyRecursive();
},this);
delete this._itemNodesMap[_b2];
}
},_initState:function(){
this._openedNodes={};
if(this.persist&&this.cookieName){
var _b6=_3(this.cookieName);
if(_b6){
_1.forEach(_b6.split(","),function(_b7){
this._openedNodes[_b7]=true;
},this);
}
}
},_state:function(_b8,_b9){
if(!this.persist){
return false;
}
var _ba=_1.map(_b8.getTreePath(),function(_bb){
return this.model.getIdentity(_bb);
},this).join("/");
if(arguments.length===1){
return this._openedNodes[_ba];
}else{
if(_b9){
this._openedNodes[_ba]=true;
}else{
delete this._openedNodes[_ba];
}
if(this.persist&&this.cookieName){
var ary=[];
for(var id in this._openedNodes){
ary.push(id);
}
_3(this.cookieName,ary.join(","),{expires:365});
}
}
},destroy:function(){
if(this._curSearch){
this._curSearch.timer.remove();
delete this._curSearch;
}
if(this.rootNode){
this.rootNode.destroyRecursive();
}
if(this.dndController&&!_10.isString(this.dndController)){
this.dndController.destroy();
}
this.rootNode=null;
this.inherited(arguments);
},destroyRecursive:function(){
this.destroy();
},resize:function(_bc){
if(_bc){
_9.setMarginBox(this.domNode,_bc);
}
this._nodePixelIndent=_9.position(this.tree.indentDetector).w||this._nodePixelIndent;
this.expandChildrenDeferred.then(_10.hitch(this,function(){
this.rootNode.set("indent",this.showRoot?0:-1);
this._adjustWidths();
}));
},_outstandingPaintOperations:0,_startPaint:function(p){
this._outstandingPaintOperations++;
if(this._adjustWidthsTimer){
this._adjustWidthsTimer.remove();
delete this._adjustWidthsTimer;
}
var oc=_10.hitch(this,function(){
this._outstandingPaintOperations--;
if(this._outstandingPaintOperations<=0&&!this._adjustWidthsTimer&&this._started){
this._adjustWidthsTimer=this.defer("_adjustWidths");
}
});
_13(p,oc,oc);
},_adjustWidths:function(){
if(this._adjustWidthsTimer){
this._adjustWidthsTimer.remove();
delete this._adjustWidthsTimer;
}
var _bd=0,_be=[];
function _bf(_c0){
var _c1=_c0.rowNode;
_c1.style.width="auto";
_bd=Math.max(_bd,_c1.clientWidth);
_be.push(_c1);
if(_c0.isExpanded){
_1.forEach(_c0.getChildren(),_bf);
}
};
_bf(this.rootNode);
_bd=Math.max(_bd,_9.getContentBox(this.domNode).w);
_1.forEach(_be,function(_c2){
_c2.style.width=_bd+"px";
});
},_createTreeNode:function(_c3){
return new _23(_c3);
},_setTextDirAttr:function(_c4){
if(_c4&&this.textDir!=_c4){
this._set("textDir",_c4);
this.rootNode.set("textDir",_c4);
}
}});
_4b.PathError=_c("TreePathError");
_4b._TreeNode=_23;
return _4b;
});
