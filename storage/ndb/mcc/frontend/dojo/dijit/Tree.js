//>>built
require({cache:{"url:dijit/templates/TreeNode.html":"<div class=\"dijitTreeNode\" role=\"presentation\"\n\t><div data-dojo-attach-point=\"rowNode\" class=\"dijitTreeRow\" role=\"presentation\"\n\t\t><span data-dojo-attach-point=\"expandoNode\" class=\"dijitInline dijitTreeExpando\" role=\"presentation\"></span\n\t\t><span data-dojo-attach-point=\"expandoNodeText\" class=\"dijitExpandoText\" role=\"presentation\"></span\n\t\t><span data-dojo-attach-point=\"contentNode\"\n\t\t\tclass=\"dijitTreeContent\" role=\"presentation\">\n\t\t\t<span role=\"presentation\" class=\"dijitInline dijitIcon dijitTreeIcon\" data-dojo-attach-point=\"iconNode\"></span\n\t\t\t><span data-dojo-attach-point=\"labelNode,focusNode\" class=\"dijitTreeLabel\" role=\"treeitem\"\n\t\t\t\t   tabindex=\"-1\" aria-selected=\"false\" id=\"${id}_label\"></span>\n\t\t</span\n\t></div>\n\t<div data-dojo-attach-point=\"containerNode\" class=\"dijitTreeNodeContainer\" role=\"presentation\"\n\t\t style=\"display: none;\" aria-labelledby=\"${id}_label\"></div>\n</div>\n","url:dijit/templates/Tree.html":"<div role=\"tree\">\n\t<div class=\"dijitInline dijitTreeIndent\" style=\"position: absolute; top: -9999px\" data-dojo-attach-point=\"indentDetector\"></div>\n\t<div class=\"dijitTreeExpando dijitTreeExpandoLoading\" data-dojo-attach-point=\"rootLoadingIndicator\"></div>\n\t<div data-dojo-attach-point=\"containerNode\" class=\"dijitTreeContainer\" role=\"presentation\">\n\t</div>\n</div>\n"}});
define("dijit/Tree",["dojo/_base/array","dojo/aspect","dojo/cookie","dojo/_base/declare","dojo/Deferred","dojo/promise/all","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/errors/create","dojo/fx","dojo/has","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/on","dojo/topic","dojo/touch","dojo/when","./a11yclick","./focus","./registry","./_base/manager","./_Widget","./_TemplatedMixin","./_Container","./_Contained","./_CssStateMixin","./_KeyNavMixin","dojo/text!./templates/TreeNode.html","dojo/text!./templates/Tree.html","./tree/TreeStoreModel","./tree/ForestStoreModel","./tree/_dndSelector","dojo/query!css2"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,on,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b,_1c,_1d,_1e,_1f,_20,_21,_22){
function _23(d){
return _10.delegate(d.promise||d,{addCallback:function(_24){
this.then(_24);
},addErrback:function(_25){
this.otherwise(_25);
}});
};
var _26=_4("dijit._TreeNode",[_18,_19,_1a,_1b,_1c],{item:null,isTreeNode:true,label:"",_setLabelAttr:function(val){
this.labelNode[this.labelType=="html"?"innerHTML":"innerText" in this.labelNode?"innerText":"textContent"]=val;
this._set("label",val);
if(_d("dojo-bidi")){
this.applyTextDir(this.labelNode);
}
},labelType:"text",isExpandable:null,isExpanded:false,state:"NotLoaded",templateString:_1e,baseClass:"dijitTreeNode",cssStateNodes:{rowNode:"dijitTreeRow"},_setTooltipAttr:{node:"rowNode",type:"attribute",attribute:"title"},buildRendering:function(){
this.inherited(arguments);
this._setExpando();
this._updateItemClasses(this.item);
if(this.isExpandable){
this.labelNode.setAttribute("aria-expanded",this.isExpanded);
}
this.setSelected(false);
},_setIndentAttr:function(_27){
var _28=(Math.max(_27,0)*this.tree._nodePixelIndent)+"px";
_a.set(this.domNode,"backgroundPosition",_28+" 0px");
_a.set(this.rowNode,this.isLeftToRight()?"paddingLeft":"paddingRight",_28);
_1.forEach(this.getChildren(),function(_29){
_29.set("indent",_27+1);
});
this._set("indent",_27);
},markProcessing:function(){
this.state="Loading";
this._setExpando(true);
},unmarkProcessing:function(){
this._setExpando(false);
},_updateItemClasses:function(_2a){
var _2b=this.tree,_2c=_2b.model;
if(_2b._v10Compat&&_2a===_2c.root){
_2a=null;
}
this._applyClassAndStyle(_2a,"icon","Icon");
this._applyClassAndStyle(_2a,"label","Label");
this._applyClassAndStyle(_2a,"row","Row");
this.tree._startPaint(true);
},_applyClassAndStyle:function(_2d,_2e,_2f){
var _30="_"+_2e+"Class";
var _31=_2e+"Node";
var _32=this[_30];
this[_30]=this.tree["get"+_2f+"Class"](_2d,this.isExpanded);
_8.replace(this[_31],this[_30]||"",_32||"");
_a.set(this[_31],this.tree["get"+_2f+"Style"](_2d,this.isExpanded)||{});
},_updateLayout:function(){
var _33=this.getParent(),_34=!_33||!_33.rowNode||_33.rowNode.style.display=="none";
_8.toggle(this.domNode,"dijitTreeIsRoot",_34);
_8.toggle(this.domNode,"dijitTreeIsLast",!_34&&!this.getNextSibling());
},_setExpando:function(_35){
var _36=["dijitTreeExpandoLoading","dijitTreeExpandoOpened","dijitTreeExpandoClosed","dijitTreeExpandoLeaf"],_37=["*","-","+","*"],idx=_35?0:(this.isExpandable?(this.isExpanded?1:2):3);
_8.replace(this.expandoNode,_36[idx],_36);
this.expandoNodeText.innerHTML=_37[idx];
},expand:function(){
if(this._expandDeferred){
return _23(this._expandDeferred);
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
var _38=_c.wipeIn({node:this.containerNode,duration:_17.defaultDuration});
var def=(this._expandDeferred=new _5(function(){
_38.stop();
}));
_2.after(_38,"onEnd",function(){
def.resolve(true);
},true);
_38.play();
return _23(def);
},collapse:function(){
if(this._collapseDeferred){
return _23(this._collapseDeferred);
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
var _39=_c.wipeOut({node:this.containerNode,duration:_17.defaultDuration});
var def=(this._collapseDeferred=new _5(function(){
_39.stop();
}));
_2.after(_39,"onEnd",function(){
def.resolve(true);
},true);
_39.play();
return _23(def);
},indent:0,setChildItems:function(_3a){
var _3b=this.tree,_3c=_3b.model,_3d=[];
var _3e=_3b.focusedChild;
var _3f=this.getChildren();
_1.forEach(_3f,function(_40){
_1a.prototype.removeChild.call(this,_40);
},this);
this.defer(function(){
_1.forEach(_3f,function(_41){
if(!_41._destroyed&&!_41.getParent()){
_3b.dndController.removeTreeNode(_41);
function _42(_43){
var id=_3c.getIdentity(_43.item),ary=_3b._itemNodesMap[id];
if(ary.length==1){
delete _3b._itemNodesMap[id];
}else{
var _44=_1.indexOf(ary,_43);
if(_44!=-1){
ary.splice(_44,1);
}
}
_1.forEach(_43.getChildren(),_42);
};
_42(_41);
if(_3b.persist){
var _45=_1.map(_41.getTreePath(),function(_46){
return _3b.model.getIdentity(_46);
}).join("/");
for(var _47 in _3b._openedNodes){
if(_47.substr(0,_45.length)==_45){
delete _3b._openedNodes[_47];
}
}
_3b._saveExpandedNodes();
}
if(_3b.lastFocusedChild&&!_7.isDescendant(_3b.lastFocusedChild.domNode,_3b.domNode)){
delete _3b.lastFocusedChild;
}
if(_3e&&!_7.isDescendant(_3e.domNode,_3b.domNode)){
_3b.focus();
}
_41.destroyRecursive();
}
});
});
this.state="Loaded";
if(_3a&&_3a.length>0){
this.isExpandable=true;
_1.forEach(_3a,function(_48){
var id=_3c.getIdentity(_48),_49=_3b._itemNodesMap[id],_4a;
if(_49){
for(var i=0;i<_49.length;i++){
if(_49[i]&&!_49[i].getParent()){
_4a=_49[i];
_4a.set("indent",this.indent+1);
break;
}
}
}
if(!_4a){
_4a=this.tree._createTreeNode({item:_48,tree:_3b,isExpandable:_3c.mayHaveChildren(_48),label:_3b.getLabel(_48),labelType:(_3b.model&&_3b.model.labelType)||"text",tooltip:_3b.getTooltip(_48),ownerDocument:_3b.ownerDocument,dir:_3b.dir,lang:_3b.lang,textDir:_3b.textDir,indent:this.indent+1});
if(_49){
_49.push(_4a);
}else{
_3b._itemNodesMap[id]=[_4a];
}
}
this.addChild(_4a);
if(this.tree.autoExpand||this.tree._state(_4a)){
_3d.push(_3b._expandNode(_4a));
}
},this);
_1.forEach(this.getChildren(),function(_4b){
_4b._updateLayout();
});
}else{
this.isExpandable=false;
}
if(this._setExpando){
this._setExpando(false);
}
this._updateItemClasses(this.item);
var def=_6(_3d);
this.tree._startPaint(def);
return _23(def);
},getTreePath:function(){
var _4c=this;
var _4d=[];
while(_4c&&_4c!==this.tree.rootNode){
_4d.unshift(_4c.item);
_4c=_4c.getParent();
}
_4d.unshift(this.tree.rootNode.item);
return _4d;
},getIdentity:function(){
return this.tree.model.getIdentity(this.item);
},removeChild:function(_4e){
this.inherited(arguments);
var _4f=this.getChildren();
if(_4f.length==0){
this.isExpandable=false;
this.collapse();
}
_1.forEach(_4f,function(_50){
_50._updateLayout();
});
},makeExpandable:function(){
this.isExpandable=true;
this._setExpando(false);
},setSelected:function(_51){
this.labelNode.setAttribute("aria-selected",_51?"true":"false");
_8.toggle(this.rowNode,"dijitTreeRowSelected",_51);
},focus:function(){
_15.focus(this.focusNode);
}});
if(_d("dojo-bidi")){
_26.extend({_setTextDirAttr:function(_52){
if(_52&&((this.textDir!=_52)||!this._created)){
this._set("textDir",_52);
this.applyTextDir(this.labelNode);
_1.forEach(this.getChildren(),function(_53){
_53.set("textDir",_52);
},this);
}
}});
}
var _54=_4("dijit.Tree",[_18,_1d,_19,_1c],{baseClass:"dijitTree",store:null,model:null,query:null,label:"",showRoot:true,childrenAttr:["children"],paths:[],path:[],selectedItems:null,selectedItem:null,openOnClick:false,openOnDblClick:false,templateString:_1f,persist:false,autoExpand:false,dndController:_22,dndParams:["onDndDrop","itemCreator","onDndCancel","checkAcceptance","checkItemAcceptance","dragThreshold","betweenThreshold"],onDndDrop:null,itemCreator:null,onDndCancel:null,checkAcceptance:null,checkItemAcceptance:null,dragThreshold:5,betweenThreshold:0,_nodePixelIndent:19,_publish:function(_55,_56){
_11.publish(this.id,_10.mixin({tree:this,event:_55},_56||{}));
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
this.pendingCommandsPromise=this.expandChildrenDeferred.promise;
this.inherited(arguments);
},postCreate:function(){
this._initState();
var _57=this;
this.own(on(this.containerNode,on.selector(".dijitTreeNode",_12.enter),function(evt){
_57._onNodeMouseEnter(_16.byNode(this),evt);
}),on(this.containerNode,on.selector(".dijitTreeNode",_12.leave),function(evt){
_57._onNodeMouseLeave(_16.byNode(this),evt);
}),on(this.containerNode,on.selector(".dijitTreeRow",_14.press),function(evt){
_57._onNodePress(_16.getEnclosingWidget(this),evt);
}),on(this.containerNode,on.selector(".dijitTreeRow",_14),function(evt){
_57._onClick(_16.getEnclosingWidget(this),evt);
}),on(this.containerNode,on.selector(".dijitTreeRow","dblclick"),function(evt){
_57._onDblClick(_16.getEnclosingWidget(this),evt);
}));
if(!this.model){
this._store2model();
}
this.own(_2.after(this.model,"onChange",_10.hitch(this,"_onItemChange"),true),_2.after(this.model,"onChildrenChange",_10.hitch(this,"_onItemChildrenChange"),true),_2.after(this.model,"onDelete",_10.hitch(this,"_onItemDelete"),true));
this.inherited(arguments);
if(this.dndController){
if(_10.isString(this.dndController)){
this.dndController=_10.getObject(this.dndController);
}
var _58={};
for(var i=0;i<this.dndParams.length;i++){
if(this[this.dndParams[i]]){
_58[this.dndParams[i]]=this[this.dndParams[i]];
}
}
this.dndController=new this.dndController(this,_58);
}
this._load();
this.onLoadDeferred=_23(this.pendingCommandsPromise);
this.onLoadDeferred.then(_10.hitch(this,"onLoad"));
},_store2model:function(){
this._v10Compat=true;
_e.deprecated("Tree: from version 2.0, should specify a model object rather than a store/query");
var _59={id:this.id+"_ForestStoreModel",store:this.store,query:this.query,childrenAttrs:this.childrenAttr};
if(this.params.mayHaveChildren){
_59.mayHaveChildren=_10.hitch(this,"mayHaveChildren");
}
if(this.params.getItemChildren){
_59.getChildren=_10.hitch(this,function(_5a,_5b,_5c){
this.getItemChildren((this._v10Compat&&_5a===this.model.root)?null:_5a,_5b,_5c);
});
}
this.model=new _21(_59);
this.showRoot=Boolean(this.label);
},onLoad:function(){
},_load:function(){
this.model.getRoot(_10.hitch(this,function(_5d){
var rn=(this.rootNode=this.tree._createTreeNode({item:_5d,tree:this,isExpandable:true,label:this.label||this.getLabel(_5d),labelType:this.model.labelType||"text",textDir:this.textDir,indent:this.showRoot?0:-1}));
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
rn.labelNode.removeAttribute("aria-selected");
rn.containerNode.setAttribute("role","tree");
rn.containerNode.setAttribute("aria-expanded","true");
rn.containerNode.setAttribute("aria-multiselectable",!this.dndController.singular);
}else{
this.domNode.setAttribute("aria-multiselectable",!this.dndController.singular);
this.rootLoadingIndicator.style.display="none";
}
this.containerNode.appendChild(rn.domNode);
var _5e=this.model.getIdentity(_5d);
if(this._itemNodesMap[_5e]){
this._itemNodesMap[_5e].push(rn);
}else{
this._itemNodesMap[_5e]=[rn];
}
rn._updateLayout();
this._expandNode(rn).then(_10.hitch(this,function(){
if(!this._destroyed){
this.rootLoadingIndicator.style.display="none";
this.expandChildrenDeferred.resolve(true);
}
}));
}),_10.hitch(this,function(err){
console.error(this,": error loading root: ",err);
}));
},getNodesByItem:function(_5f){
if(!_5f){
return [];
}
var _60=_10.isString(_5f)?_5f:this.model.getIdentity(_5f);
return [].concat(this._itemNodesMap[_60]);
},_setSelectedItemAttr:function(_61){
this.set("selectedItems",[_61]);
},_setSelectedItemsAttr:function(_62){
var _63=this;
return this.pendingCommandsPromise=this.pendingCommandsPromise.always(_10.hitch(this,function(){
var _64=_1.map(_62,function(_65){
return (!_65||_10.isString(_65))?_65:_63.model.getIdentity(_65);
});
var _66=[];
_1.forEach(_64,function(id){
_66=_66.concat(_63._itemNodesMap[id]||[]);
});
this.set("selectedNodes",_66);
}));
},_setPathAttr:function(_67){
if(_67.length){
return _23(this.set("paths",[_67]).then(function(_68){
return _68[0];
}));
}else{
return _23(this.set("paths",[]).then(function(_69){
return _69[0];
}));
}
},_setPathsAttr:function(_6a){
var _6b=this;
function _6c(_6d,_6e){
var _6f=_6d.shift();
var _70=_1.filter(_6e,function(_71){
return _71.getIdentity()==_6f;
})[0];
if(!!_70){
if(_6d.length){
return _6b._expandNode(_70).then(function(){
return _6c(_6d,_70.getChildren());
});
}else{
return _70;
}
}else{
throw new _54.PathError("Could not expand path at "+_6f);
}
};
return _23(this.pendingCommandsPromise=this.pendingCommandsPromise.always(function(){
return _6(_1.map(_6a,function(_72){
_72=_1.map(_72,function(_73){
return _73&&_10.isObject(_73)?_6b.model.getIdentity(_73):_73;
});
if(_72.length){
return _6c(_72,[_6b.rootNode]);
}else{
throw new _54.PathError("Empty path");
}
}));
}).then(function setNodes(_74){
_6b.set("selectedNodes",_74);
return _6b.paths;
}));
},_setSelectedNodeAttr:function(_75){
this.set("selectedNodes",[_75]);
},_setSelectedNodesAttr:function(_76){
this.dndController.setSelection(_76);
},expandAll:function(){
var _77=this;
function _78(_79){
return _77._expandNode(_79).then(function(){
var _7a=_1.filter(_79.getChildren()||[],function(_7b){
return _7b.isExpandable;
});
return _6(_1.map(_7a,_78));
});
};
return _23(_78(this.rootNode));
},collapseAll:function(){
var _7c=this;
function _7d(_7e){
var _7f=_1.filter(_7e.getChildren()||[],function(_80){
return _80.isExpandable;
}),_81=_6(_1.map(_7f,_7d));
if(!_7e.isExpanded||(_7e==_7c.rootNode&&!_7c.showRoot)){
return _81;
}else{
return _81.then(function(){
return _7c._collapseNode(_7e);
});
}
};
return _23(_7d(this.rootNode));
},mayHaveChildren:function(){
},getItemChildren:function(){
},getLabel:function(_82){
return this.model.getLabel(_82);
},getIconClass:function(_83,_84){
return (!_83||this.model.mayHaveChildren(_83))?(_84?"dijitFolderOpened":"dijitFolderClosed"):"dijitLeaf";
},getLabelClass:function(){
},getRowClass:function(){
},getIconStyle:function(){
},getLabelStyle:function(){
},getRowStyle:function(){
},getTooltip:function(){
return "";
},_onDownArrow:function(evt,_85){
var _86=this._getNext(_85);
if(_86&&_86.isTreeNode){
this.focusNode(_86);
}
},_onUpArrow:function(evt,_87){
var _88=_87.getPreviousSibling();
if(_88){
_87=_88;
while(_87.isExpandable&&_87.isExpanded&&_87.hasChildren()){
var _89=_87.getChildren();
_87=_89[_89.length-1];
}
}else{
var _8a=_87.getParent();
if(!(!this.showRoot&&_8a===this.rootNode)){
_87=_8a;
}
}
if(_87&&_87.isTreeNode){
this.focusNode(_87);
}
},_onRightArrow:function(evt,_8b){
if(_8b.isExpandable&&!_8b.isExpanded){
this._expandNode(_8b);
}else{
if(_8b.hasChildren()){
_8b=_8b.getChildren()[0];
if(_8b&&_8b.isTreeNode){
this.focusNode(_8b);
}
}
}
},_onLeftArrow:function(evt,_8c){
if(_8c.isExpandable&&_8c.isExpanded){
this._collapseNode(_8c);
}else{
var _8d=_8c.getParent();
if(_8d&&_8d.isTreeNode&&!(!this.showRoot&&_8d===this.rootNode)){
this.focusNode(_8d);
}
}
},focusLastChild:function(){
var _8e=this._getLast();
if(_8e&&_8e.isTreeNode){
this.focusNode(_8e);
}
},_getFirst:function(){
return this.showRoot?this.rootNode:this.rootNode.getChildren()[0];
},_getLast:function(){
var _8f=this.rootNode;
while(_8f.isExpanded){
var c=_8f.getChildren();
if(!c.length){
break;
}
_8f=c[c.length-1];
}
return _8f;
},_getNext:function(_90){
if(_90.isExpandable&&_90.isExpanded&&_90.hasChildren()){
return _90.getChildren()[0];
}else{
while(_90&&_90.isTreeNode){
var _91=_90.getNextSibling();
if(_91){
return _91;
}
_90=_90.getParent();
}
return null;
}
},childSelector:".dijitTreeRow",isExpandoNode:function(_92,_93){
return _7.isDescendant(_92,_93.expandoNode)||_7.isDescendant(_92,_93.expandoNodeText);
},_onNodePress:function(_94,e){
this.focusNode(_94);
},__click:function(_95,e,_96,_97){
var _98=e.target,_99=this.isExpandoNode(_98,_95);
if(_95.isExpandable&&(_96||_99)){
this._onExpandoClick({node:_95});
}else{
this._publish("execute",{item:_95.item,node:_95,evt:e});
this[_97](_95.item,_95,e);
this.focusNode(_95);
}
e.stopPropagation();
e.preventDefault();
},_onClick:function(_9a,e){
this.__click(_9a,e,this.openOnClick,"onClick");
},_onDblClick:function(_9b,e){
this.__click(_9b,e,this.openOnDblClick,"onDblClick");
},_onExpandoClick:function(_9c){
var _9d=_9c.node;
this.focusNode(_9d);
if(_9d.isExpanded){
this._collapseNode(_9d);
}else{
this._expandNode(_9d);
}
},onClick:function(){
},onDblClick:function(){
},onOpen:function(){
},onClose:function(){
},_getNextNode:function(_9e){
_e.deprecated(this.declaredClass+"::_getNextNode(node) is deprecated. Use _getNext(node) instead.","","2.0");
return this._getNext(_9e);
},_getRootOrFirstNode:function(){
_e.deprecated(this.declaredClass+"::_getRootOrFirstNode() is deprecated. Use _getFirst() instead.","","2.0");
return this._getFirst();
},_collapseNode:function(_9f){
if(_9f._expandNodeDeferred){
delete _9f._expandNodeDeferred;
}
if(_9f.state=="Loading"){
return;
}
if(_9f.isExpanded){
var ret=_9f.collapse();
this.onClose(_9f.item,_9f);
this._state(_9f,false);
this._startPaint(ret);
return ret;
}
},_expandNode:function(_a0){
if(_a0._expandNodeDeferred){
return _a0._expandNodeDeferred;
}
var _a1=this.model,_a2=_a0.item,_a3=this;
if(!_a0._loadDeferred){
_a0.markProcessing();
_a0._loadDeferred=new _5();
_a1.getChildren(_a2,function(_a4){
_a0.unmarkProcessing();
_a0.setChildItems(_a4).then(function(){
_a0._loadDeferred.resolve(_a4);
});
},function(err){
console.error(_a3,": error loading "+_a0.label+" children: ",err);
_a0._loadDeferred.reject(err);
});
}
var def=_a0._loadDeferred.then(_10.hitch(this,function(){
var _a5=_a0.expand();
this.onOpen(_a0.item,_a0);
this._state(_a0,true);
return _a5;
}));
this._startPaint(def);
return def;
},focusNode:function(_a6){
var tmp=[];
for(var _a7=this.domNode;_a7&&_a7.tagName&&_a7.tagName.toUpperCase()!=="IFRAME";_a7=_a7.parentNode){
tmp.push({domNode:_a7.contentWindow||_a7,scrollLeft:_a7.scrollLeft||0,scrollTop:_a7.scrollTop||0});
}
this.focusChild(_a6);
this.defer(function(){
for(var i=0,max=tmp.length;i<max;i++){
tmp[i].domNode.scrollLeft=tmp[i].scrollLeft;
tmp[i].domNode.scrollTop=tmp[i].scrollTop;
}
},0);
},_onNodeMouseEnter:function(){
},_onNodeMouseLeave:function(){
},_onItemChange:function(_a8){
var _a9=this.model,_aa=_a9.getIdentity(_a8),_ab=this._itemNodesMap[_aa];
if(_ab){
var _ac=this.getLabel(_a8),_ad=this.getTooltip(_a8);
_1.forEach(_ab,function(_ae){
_ae.set({item:_a8,label:_ac,tooltip:_ad});
_ae._updateItemClasses(_a8);
});
}
},_onItemChildrenChange:function(_af,_b0){
var _b1=this.model,_b2=_b1.getIdentity(_af),_b3=this._itemNodesMap[_b2];
if(_b3){
_1.forEach(_b3,function(_b4){
_b4.setChildItems(_b0);
});
}
},_onItemDelete:function(_b5){
var _b6=this.model,_b7=_b6.getIdentity(_b5),_b8=this._itemNodesMap[_b7];
if(_b8){
_1.forEach(_b8,function(_b9){
this.dndController.removeTreeNode(_b9);
var _ba=_b9.getParent();
if(_ba){
_ba.removeChild(_b9);
}
if(this.lastFocusedChild&&!_7.isDescendant(this.lastFocusedChild.domNode,this.domNode)){
delete this.lastFocusedChild;
}
if(this.focusedChild&&!_7.isDescendant(this.focusedChild.domNode,this.domNode)){
this.focus();
}
_b9.destroyRecursive();
},this);
delete this._itemNodesMap[_b7];
}
},_initState:function(){
this._openedNodes={};
if(this.persist&&this.cookieName){
var _bb=_3(this.cookieName);
if(_bb){
_1.forEach(_bb.split(","),function(_bc){
this._openedNodes[_bc]=true;
},this);
}
}
},_state:function(_bd,_be){
if(!this.persist){
return false;
}
var _bf=_1.map(_bd.getTreePath(),function(_c0){
return this.model.getIdentity(_c0);
},this).join("/");
if(arguments.length===1){
return this._openedNodes[_bf];
}else{
if(_be){
this._openedNodes[_bf]=true;
}else{
delete this._openedNodes[_bf];
}
this._saveExpandedNodes();
}
},_saveExpandedNodes:function(){
if(this.persist&&this.cookieName){
var ary=[];
for(var id in this._openedNodes){
ary.push(id);
}
_3(this.cookieName,ary.join(","),{expires:365});
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
},resize:function(_c1){
if(_c1){
_9.setMarginBox(this.domNode,_c1);
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
this.containerNode.style.width="auto";
this.containerNode.style.width=this.domNode.scrollWidth>this.domNode.offsetWidth?"auto":"100%";
},_createTreeNode:function(_c2){
return new _26(_c2);
},focus:function(){
if(this.lastFocusedChild){
this.focusNode(this.lastFocusedChild);
}else{
this.focusFirstChild();
}
}});
if(_d("dojo-bidi")){
_54.extend({_setTextDirAttr:function(_c3){
if(_c3&&this.textDir!=_c3){
this._set("textDir",_c3);
this.rootNode.set("textDir",_c3);
}
}});
}
_54.PathError=_b("TreePathError");
_54._TreeNode=_26;
return _54;
});
