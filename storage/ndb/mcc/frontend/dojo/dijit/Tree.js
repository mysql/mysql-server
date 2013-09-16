//>>built
require({cache:{"url:dijit/templates/TreeNode.html":"<div class=\"dijitTreeNode\" role=\"presentation\"\n\t><div data-dojo-attach-point=\"rowNode\" class=\"dijitTreeRow\" role=\"presentation\" data-dojo-attach-event=\"onmouseenter:_onMouseEnter, onmouseleave:_onMouseLeave, onclick:_onClick, ondblclick:_onDblClick\"\n\t\t><img src=\"${_blankGif}\" alt=\"\" data-dojo-attach-point=\"expandoNode\" class=\"dijitTreeExpando\" role=\"presentation\"\n\t\t/><span data-dojo-attach-point=\"expandoNodeText\" class=\"dijitExpandoText\" role=\"presentation\"\n\t\t></span\n\t\t><span data-dojo-attach-point=\"contentNode\"\n\t\t\tclass=\"dijitTreeContent\" role=\"presentation\">\n\t\t\t<img src=\"${_blankGif}\" alt=\"\" data-dojo-attach-point=\"iconNode\" class=\"dijitIcon dijitTreeIcon\" role=\"presentation\"\n\t\t\t/><span data-dojo-attach-point=\"labelNode\" class=\"dijitTreeLabel\" role=\"treeitem\" tabindex=\"-1\" aria-selected=\"false\" data-dojo-attach-event=\"onfocus:_onLabelFocus\"></span>\n\t\t</span\n\t></div>\n\t<div data-dojo-attach-point=\"containerNode\" class=\"dijitTreeContainer\" role=\"presentation\" style=\"display: none;\"></div>\n</div>\n","url:dijit/templates/Tree.html":"<div class=\"dijitTree dijitTreeContainer\" role=\"tree\"\n\tdata-dojo-attach-event=\"onkeypress:_onKeyPress\">\n\t<div class=\"dijitInline dijitTreeIndent\" style=\"position: absolute; top: -9999px\" data-dojo-attach-point=\"indentDetector\"></div>\n</div>\n"}});
define("dijit/Tree",["dojo/_base/array","dojo/_base/connect","dojo/cookie","dojo/_base/declare","dojo/_base/Deferred","dojo/DeferredList","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/fx","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/topic","./focus","./registry","./_base/manager","./_Widget","./_TemplatedMixin","./_Container","./_Contained","./_CssStateMixin","dojo/text!./templates/TreeNode.html","dojo/text!./templates/Tree.html","./tree/TreeStoreModel","./tree/ForestStoreModel","./tree/_dndSelector"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b,_1c,_1d){
var _1e=_4("dijit._TreeNode",[_14,_15,_16,_17,_18],{item:null,isTreeNode:true,label:"",_setLabelAttr:{node:"labelNode",type:"innerText"},isExpandable:null,isExpanded:false,state:"UNCHECKED",templateString:_19,baseClass:"dijitTreeNode",cssStateNodes:{rowNode:"dijitTreeRow",labelNode:"dijitTreeLabel"},_setTooltipAttr:{node:"rowNode",type:"attribute",attribute:"title"},buildRendering:function(){
this.inherited(arguments);
this._setExpando();
this._updateItemClasses(this.item);
if(this.isExpandable){
this.labelNode.setAttribute("aria-expanded",this.isExpanded);
}
this.setSelected(false);
},_setIndentAttr:function(_1f){
var _20=(Math.max(_1f,0)*this.tree._nodePixelIndent)+"px";
_a.set(this.domNode,"backgroundPosition",_20+" 0px");
_a.set(this.rowNode,this.isLeftToRight()?"paddingLeft":"paddingRight",_20);
_1.forEach(this.getChildren(),function(_21){
_21.set("indent",_1f+1);
});
this._set("indent",_1f);
},markProcessing:function(){
this.state="LOADING";
this._setExpando(true);
},unmarkProcessing:function(){
this._setExpando(false);
},_updateItemClasses:function(_22){
var _23=this.tree,_24=_23.model;
if(_23._v10Compat&&_22===_24.root){
_22=null;
}
this._applyClassAndStyle(_22,"icon","Icon");
this._applyClassAndStyle(_22,"label","Label");
this._applyClassAndStyle(_22,"row","Row");
},_applyClassAndStyle:function(_25,_26,_27){
var _28="_"+_26+"Class";
var _29=_26+"Node";
var _2a=this[_28];
this[_28]=this.tree["get"+_27+"Class"](_25,this.isExpanded);
_8.replace(this[_29],this[_28]||"",_2a||"");
_a.set(this[_29],this.tree["get"+_27+"Style"](_25,this.isExpanded)||{});
},_updateLayout:function(){
var _2b=this.getParent();
if(!_2b||!_2b.rowNode||_2b.rowNode.style.display=="none"){
_8.add(this.domNode,"dijitTreeIsRoot");
}else{
_8.toggle(this.domNode,"dijitTreeIsLast",!this.getNextSibling());
}
},_setExpando:function(_2c){
var _2d=["dijitTreeExpandoLoading","dijitTreeExpandoOpened","dijitTreeExpandoClosed","dijitTreeExpandoLeaf"],_2e=["*","-","+","*"],idx=_2c?0:(this.isExpandable?(this.isExpanded?1:2):3);
_8.replace(this.expandoNode,_2d[idx],_2d);
this.expandoNodeText.innerHTML=_2e[idx];
},expand:function(){
if(this._expandDeferred){
return this._expandDeferred;
}
this._wipeOut&&this._wipeOut.stop();
this.isExpanded=true;
this.labelNode.setAttribute("aria-expanded","true");
if(this.tree.showRoot||this!==this.tree.rootNode){
this.containerNode.setAttribute("role","group");
}
_8.add(this.contentNode,"dijitTreeContentExpanded");
this._setExpando();
this._updateItemClasses(this.item);
if(this==this.tree.rootNode){
this.tree.domNode.setAttribute("aria-expanded","true");
}
var def,_2f=_c.wipeIn({node:this.containerNode,duration:_13.defaultDuration,onEnd:function(){
def.callback(true);
}});
def=(this._expandDeferred=new _5(function(){
_2f.stop();
}));
_2f.play();
return def;
},collapse:function(){
if(!this.isExpanded){
return;
}
if(this._expandDeferred){
this._expandDeferred.cancel();
delete this._expandDeferred;
}
this.isExpanded=false;
this.labelNode.setAttribute("aria-expanded","false");
if(this==this.tree.rootNode){
this.tree.domNode.setAttribute("aria-expanded","false");
}
_8.remove(this.contentNode,"dijitTreeContentExpanded");
this._setExpando();
this._updateItemClasses(this.item);
if(!this._wipeOut){
this._wipeOut=_c.wipeOut({node:this.containerNode,duration:_13.defaultDuration});
}
this._wipeOut.play();
},indent:0,setChildItems:function(_30){
var _31=this.tree,_32=_31.model,_33=[];
_1.forEach(this.getChildren(),function(_34){
_16.prototype.removeChild.call(this,_34);
},this);
this.state="LOADED";
if(_30&&_30.length>0){
this.isExpandable=true;
_1.forEach(_30,function(_35){
var id=_32.getIdentity(_35),_36=_31._itemNodesMap[id],_37;
if(_36){
for(var i=0;i<_36.length;i++){
if(_36[i]&&!_36[i].getParent()){
_37=_36[i];
_37.set("indent",this.indent+1);
break;
}
}
}
if(!_37){
_37=this.tree._createTreeNode({item:_35,tree:_31,isExpandable:_32.mayHaveChildren(_35),label:_31.getLabel(_35),tooltip:_31.getTooltip(_35),dir:_31.dir,lang:_31.lang,textDir:_31.textDir,indent:this.indent+1});
if(_36){
_36.push(_37);
}else{
_31._itemNodesMap[id]=[_37];
}
}
this.addChild(_37);
if(this.tree.autoExpand||this.tree._state(_37)){
_33.push(_31._expandNode(_37));
}
},this);
_1.forEach(this.getChildren(),function(_38){
_38._updateLayout();
});
}else{
this.isExpandable=false;
}
if(this._setExpando){
this._setExpando(false);
}
this._updateItemClasses(this.item);
if(this==_31.rootNode){
var fc=this.tree.showRoot?this:this.getChildren()[0];
if(fc){
fc.setFocusable(true);
_31.lastFocused=fc;
}else{
_31.domNode.setAttribute("tabIndex","0");
}
}
return new _6(_33);
},getTreePath:function(){
var _39=this;
var _3a=[];
while(_39&&_39!==this.tree.rootNode){
_3a.unshift(_39.item);
_39=_39.getParent();
}
_3a.unshift(this.tree.rootNode.item);
return _3a;
},getIdentity:function(){
return this.tree.model.getIdentity(this.item);
},removeChild:function(_3b){
this.inherited(arguments);
var _3c=this.getChildren();
if(_3c.length==0){
this.isExpandable=false;
this.collapse();
}
_1.forEach(_3c,function(_3d){
_3d._updateLayout();
});
},makeExpandable:function(){
this.isExpandable=true;
this._setExpando(false);
},_onLabelFocus:function(){
this.tree._onNodeFocus(this);
},setSelected:function(_3e){
this.labelNode.setAttribute("aria-selected",_3e);
_8.toggle(this.rowNode,"dijitTreeRowSelected",_3e);
},setFocusable:function(_3f){
this.labelNode.setAttribute("tabIndex",_3f?"0":"-1");
},_onClick:function(evt){
this.tree._onClick(this,evt);
},_onDblClick:function(evt){
this.tree._onDblClick(this,evt);
},_onMouseEnter:function(evt){
this.tree._onNodeMouseEnter(this,evt);
},_onMouseLeave:function(evt){
this.tree._onNodeMouseLeave(this,evt);
},_setTextDirAttr:function(_40){
if(_40&&((this.textDir!=_40)||!this._created)){
this._set("textDir",_40);
this.applyTextDir(this.labelNode,this.labelNode.innerText||this.labelNode.textContent||"");
_1.forEach(this.getChildren(),function(_41){
_41.set("textDir",_40);
},this);
}
}});
var _42=_4("dijit.Tree",[_14,_15],{store:null,model:null,query:null,label:"",showRoot:true,childrenAttr:["children"],paths:[],path:[],selectedItems:null,selectedItem:null,openOnClick:false,openOnDblClick:false,templateString:_1a,persist:true,autoExpand:false,dndController:_1d,dndParams:["onDndDrop","itemCreator","onDndCancel","checkAcceptance","checkItemAcceptance","dragThreshold","betweenThreshold"],onDndDrop:null,itemCreator:null,onDndCancel:null,checkAcceptance:null,checkItemAcceptance:null,dragThreshold:5,betweenThreshold:0,_nodePixelIndent:19,_publish:function(_43,_44){
_10.publish(this.id,_f.mixin({tree:this,event:_43},_44||{}));
},postMixInProperties:function(){
this.tree=this;
if(this.autoExpand){
this.persist=false;
}
this._itemNodesMap={};
if(!this.cookieName&&this.id){
this.cookieName=this.id+"SaveStateCookie";
}
this._loadDeferred=new _5();
this.inherited(arguments);
},postCreate:function(){
this._initState();
if(!this.model){
this._store2model();
}
this.connect(this.model,"onChange","_onItemChange");
this.connect(this.model,"onChildrenChange","_onItemChildrenChange");
this.connect(this.model,"onDelete","_onItemDelete");
this._load();
this.inherited(arguments);
if(this.dndController){
if(_f.isString(this.dndController)){
this.dndController=_f.getObject(this.dndController);
}
var _45={};
for(var i=0;i<this.dndParams.length;i++){
if(this[this.dndParams[i]]){
_45[this.dndParams[i]]=this[this.dndParams[i]];
}
}
this.dndController=new this.dndController(this,_45);
}
},_store2model:function(){
this._v10Compat=true;
_d.deprecated("Tree: from version 2.0, should specify a model object rather than a store/query");
var _46={id:this.id+"_ForestStoreModel",store:this.store,query:this.query,childrenAttrs:this.childrenAttr};
if(this.params.mayHaveChildren){
_46.mayHaveChildren=_f.hitch(this,"mayHaveChildren");
}
if(this.params.getItemChildren){
_46.getChildren=_f.hitch(this,function(_47,_48,_49){
this.getItemChildren((this._v10Compat&&_47===this.model.root)?null:_47,_48,_49);
});
}
this.model=new _1c(_46);
this.showRoot=Boolean(this.label);
},onLoad:function(){
},_load:function(){
this.model.getRoot(_f.hitch(this,function(_4a){
var rn=(this.rootNode=this.tree._createTreeNode({item:_4a,tree:this,isExpandable:true,label:this.label||this.getLabel(_4a),textDir:this.textDir,indent:this.showRoot?0:-1}));
if(!this.showRoot){
rn.rowNode.style.display="none";
this.domNode.setAttribute("role","presentation");
rn.labelNode.setAttribute("role","presentation");
rn.containerNode.setAttribute("role","tree");
}
this.domNode.appendChild(rn.domNode);
var _4b=this.model.getIdentity(_4a);
if(this._itemNodesMap[_4b]){
this._itemNodesMap[_4b].push(rn);
}else{
this._itemNodesMap[_4b]=[rn];
}
rn._updateLayout();
this._expandNode(rn).addCallback(_f.hitch(this,function(){
this._loadDeferred.callback(true);
this.onLoad();
}));
}),function(err){
console.error(this,": error loading root: ",err);
});
},getNodesByItem:function(_4c){
if(!_4c){
return [];
}
var _4d=_f.isString(_4c)?_4c:this.model.getIdentity(_4c);
return [].concat(this._itemNodesMap[_4d]);
},_setSelectedItemAttr:function(_4e){
this.set("selectedItems",[_4e]);
},_setSelectedItemsAttr:function(_4f){
var _50=this;
this._loadDeferred.addCallback(_f.hitch(this,function(){
var _51=_1.map(_4f,function(_52){
return (!_52||_f.isString(_52))?_52:_50.model.getIdentity(_52);
});
var _53=[];
_1.forEach(_51,function(id){
_53=_53.concat(_50._itemNodesMap[id]||[]);
});
this.set("selectedNodes",_53);
}));
},_setPathAttr:function(_54){
if(_54.length){
return this.set("paths",[_54]);
}else{
return this.set("paths",[]);
}
},_setPathsAttr:function(_55){
var _56=this;
return new _6(_1.map(_55,function(_57){
var d=new _5();
_57=_1.map(_57,function(_58){
return _f.isString(_58)?_58:_56.model.getIdentity(_58);
});
if(_57.length){
_56._loadDeferred.addCallback(function(){
_59(_57,[_56.rootNode],d);
});
}else{
d.errback("Empty path");
}
return d;
})).addCallback(_5a);
function _59(_5b,_5c,def){
var _5d=_5b.shift();
var _5e=_1.filter(_5c,function(_5f){
return _5f.getIdentity()==_5d;
})[0];
if(!!_5e){
if(_5b.length){
_56._expandNode(_5e).addCallback(function(){
_59(_5b,_5e.getChildren(),def);
});
}else{
def.callback(_5e);
}
}else{
def.errback("Could not expand path at "+_5d);
}
};
function _5a(_60){
_56.set("selectedNodes",_1.map(_1.filter(_60,function(x){
return x[0];
}),function(x){
return x[1];
}));
};
},_setSelectedNodeAttr:function(_61){
this.set("selectedNodes",[_61]);
},_setSelectedNodesAttr:function(_62){
this._loadDeferred.addCallback(_f.hitch(this,function(){
this.dndController.setSelection(_62);
}));
},mayHaveChildren:function(){
},getItemChildren:function(){
},getLabel:function(_63){
return this.model.getLabel(_63);
},getIconClass:function(_64,_65){
return (!_64||this.model.mayHaveChildren(_64))?(_65?"dijitFolderOpened":"dijitFolderClosed"):"dijitLeaf";
},getLabelClass:function(){
},getRowClass:function(){
},getIconStyle:function(){
},getLabelStyle:function(){
},getRowStyle:function(){
},getTooltip:function(){
return "";
},_onKeyPress:function(e){
if(e.altKey){
return;
}
var _66=_12.getEnclosingWidget(e.target);
if(!_66){
return;
}
var key=e.charOrCode;
if(typeof key=="string"&&key!=" "){
if(!e.altKey&&!e.ctrlKey&&!e.shiftKey&&!e.metaKey){
this._onLetterKeyNav({node:_66,key:key.toLowerCase()});
_b.stop(e);
}
}else{
if(this._curSearch){
clearTimeout(this._curSearch.timer);
delete this._curSearch;
}
var map=this._keyHandlerMap;
if(!map){
map={};
map[_e.ENTER]="_onEnterKey";
map[_e.SPACE]=map[" "]="_onEnterKey";
map[this.isLeftToRight()?_e.LEFT_ARROW:_e.RIGHT_ARROW]="_onLeftArrow";
map[this.isLeftToRight()?_e.RIGHT_ARROW:_e.LEFT_ARROW]="_onRightArrow";
map[_e.UP_ARROW]="_onUpArrow";
map[_e.DOWN_ARROW]="_onDownArrow";
map[_e.HOME]="_onHomeKey";
map[_e.END]="_onEndKey";
this._keyHandlerMap=map;
}
if(this._keyHandlerMap[key]){
this[this._keyHandlerMap[key]]({node:_66,item:_66.item,evt:e});
_b.stop(e);
}
}
},_onEnterKey:function(_67){
this._publish("execute",{item:_67.item,node:_67.node});
this.dndController.userSelect(_67.node,_2.isCopyKey(_67.evt),_67.evt.shiftKey);
this.onClick(_67.item,_67.node,_67.evt);
},_onDownArrow:function(_68){
var _69=this._getNextNode(_68.node);
if(_69&&_69.isTreeNode){
this.focusNode(_69);
}
},_onUpArrow:function(_6a){
var _6b=_6a.node;
var _6c=_6b.getPreviousSibling();
if(_6c){
_6b=_6c;
while(_6b.isExpandable&&_6b.isExpanded&&_6b.hasChildren()){
var _6d=_6b.getChildren();
_6b=_6d[_6d.length-1];
}
}else{
var _6e=_6b.getParent();
if(!(!this.showRoot&&_6e===this.rootNode)){
_6b=_6e;
}
}
if(_6b&&_6b.isTreeNode){
this.focusNode(_6b);
}
},_onRightArrow:function(_6f){
var _70=_6f.node;
if(_70.isExpandable&&!_70.isExpanded){
this._expandNode(_70);
}else{
if(_70.hasChildren()){
_70=_70.getChildren()[0];
if(_70&&_70.isTreeNode){
this.focusNode(_70);
}
}
}
},_onLeftArrow:function(_71){
var _72=_71.node;
if(_72.isExpandable&&_72.isExpanded){
this._collapseNode(_72);
}else{
var _73=_72.getParent();
if(_73&&_73.isTreeNode&&!(!this.showRoot&&_73===this.rootNode)){
this.focusNode(_73);
}
}
},_onHomeKey:function(){
var _74=this._getRootOrFirstNode();
if(_74){
this.focusNode(_74);
}
},_onEndKey:function(){
var _75=this.rootNode;
while(_75.isExpanded){
var c=_75.getChildren();
_75=c[c.length-1];
}
if(_75&&_75.isTreeNode){
this.focusNode(_75);
}
},multiCharSearchDuration:250,_onLetterKeyNav:function(_76){
var cs=this._curSearch;
if(cs){
cs.pattern=cs.pattern+_76.key;
clearTimeout(cs.timer);
}else{
cs=this._curSearch={pattern:_76.key,startNode:_76.node};
}
var _77=this;
cs.timer=setTimeout(function(){
delete _77._curSearch;
},this.multiCharSearchDuration);
var _78=cs.startNode;
do{
_78=this._getNextNode(_78);
if(!_78){
_78=this._getRootOrFirstNode();
}
}while(_78!==cs.startNode&&(_78.label.toLowerCase().substr(0,cs.pattern.length)!=cs.pattern));
if(_78&&_78.isTreeNode){
if(_78!==cs.startNode){
this.focusNode(_78);
}
}
},isExpandoNode:function(_79,_7a){
return _7.isDescendant(_79,_7a.expandoNode);
},_onClick:function(_7b,e){
var _7c=e.target,_7d=this.isExpandoNode(_7c,_7b);
if((this.openOnClick&&_7b.isExpandable)||_7d){
if(_7b.isExpandable){
this._onExpandoClick({node:_7b});
}
}else{
this._publish("execute",{item:_7b.item,node:_7b,evt:e});
this.onClick(_7b.item,_7b,e);
this.focusNode(_7b);
}
_b.stop(e);
},_onDblClick:function(_7e,e){
var _7f=e.target,_80=(_7f==_7e.expandoNode||_7f==_7e.expandoNodeText);
if((this.openOnDblClick&&_7e.isExpandable)||_80){
if(_7e.isExpandable){
this._onExpandoClick({node:_7e});
}
}else{
this._publish("execute",{item:_7e.item,node:_7e,evt:e});
this.onDblClick(_7e.item,_7e,e);
this.focusNode(_7e);
}
_b.stop(e);
},_onExpandoClick:function(_81){
var _82=_81.node;
this.focusNode(_82);
if(_82.isExpanded){
this._collapseNode(_82);
}else{
this._expandNode(_82);
}
},onClick:function(){
},onDblClick:function(){
},onOpen:function(){
},onClose:function(){
},_getNextNode:function(_83){
if(_83.isExpandable&&_83.isExpanded&&_83.hasChildren()){
return _83.getChildren()[0];
}else{
while(_83&&_83.isTreeNode){
var _84=_83.getNextSibling();
if(_84){
return _84;
}
_83=_83.getParent();
}
return null;
}
},_getRootOrFirstNode:function(){
return this.showRoot?this.rootNode:this.rootNode.getChildren()[0];
},_collapseNode:function(_85){
if(_85._expandNodeDeferred){
delete _85._expandNodeDeferred;
}
if(_85.isExpandable){
if(_85.state=="LOADING"){
return;
}
_85.collapse();
this.onClose(_85.item,_85);
this._state(_85,false);
}
},_expandNode:function(_86,_87){
if(_86._expandNodeDeferred&&!_87){
return _86._expandNodeDeferred;
}
var _88=this.model,_89=_86.item,_8a=this;
switch(_86.state){
case "UNCHECKED":
_86.markProcessing();
var def=(_86._expandNodeDeferred=new _5());
_88.getChildren(_89,function(_8b){
_86.unmarkProcessing();
var _8c=_86.setChildItems(_8b);
var ed=_8a._expandNode(_86,true);
_8c.addCallback(function(){
ed.addCallback(function(){
def.callback();
});
});
},function(err){
console.error(_8a,": error loading root children: ",err);
});
break;
default:
def=(_86._expandNodeDeferred=_86.expand());
this.onOpen(_86.item,_86);
this._state(_86,true);
}
return def;
},focusNode:function(_8d){
_11.focus(_8d.labelNode);
},_onNodeFocus:function(_8e){
if(_8e&&_8e!=this.lastFocused){
if(this.lastFocused&&!this.lastFocused._destroyed){
this.lastFocused.setFocusable(false);
}
_8e.setFocusable(true);
this.lastFocused=_8e;
}
},_onNodeMouseEnter:function(){
},_onNodeMouseLeave:function(){
},_onItemChange:function(_8f){
var _90=this.model,_91=_90.getIdentity(_8f),_92=this._itemNodesMap[_91];
if(_92){
var _93=this.getLabel(_8f),_94=this.getTooltip(_8f);
_1.forEach(_92,function(_95){
_95.set({item:_8f,label:_93,tooltip:_94});
_95._updateItemClasses(_8f);
});
}
},_onItemChildrenChange:function(_96,_97){
var _98=this.model,_99=_98.getIdentity(_96),_9a=this._itemNodesMap[_99];
if(_9a){
_1.forEach(_9a,function(_9b){
_9b.setChildItems(_97);
});
}
},_onItemDelete:function(_9c){
var _9d=this.model,_9e=_9d.getIdentity(_9c),_9f=this._itemNodesMap[_9e];
if(_9f){
_1.forEach(_9f,function(_a0){
this.dndController.removeTreeNode(_a0);
var _a1=_a0.getParent();
if(_a1){
_a1.removeChild(_a0);
}
_a0.destroyRecursive();
},this);
delete this._itemNodesMap[_9e];
}
},_initState:function(){
this._openedNodes={};
if(this.persist&&this.cookieName){
var _a2=_3(this.cookieName);
if(_a2){
_1.forEach(_a2.split(","),function(_a3){
this._openedNodes[_a3]=true;
},this);
}
}
},_state:function(_a4,_a5){
if(!this.persist){
return false;
}
var _a6=_1.map(_a4.getTreePath(),function(_a7){
return this.model.getIdentity(_a7);
},this).join("/");
if(arguments.length===1){
return this._openedNodes[_a6];
}else{
if(_a5){
this._openedNodes[_a6]=true;
}else{
delete this._openedNodes[_a6];
}
var ary=[];
for(var id in this._openedNodes){
ary.push(id);
}
_3(this.cookieName,ary.join(","),{expires:365});
}
},destroy:function(){
if(this._curSearch){
clearTimeout(this._curSearch.timer);
delete this._curSearch;
}
if(this.rootNode){
this.rootNode.destroyRecursive();
}
if(this.dndController&&!_f.isString(this.dndController)){
this.dndController.destroy();
}
this.rootNode=null;
this.inherited(arguments);
},destroyRecursive:function(){
this.destroy();
},resize:function(_a8){
if(_a8){
_9.setMarginBox(this.domNode,_a8);
}
this._nodePixelIndent=_9.position(this.tree.indentDetector).w;
if(this.tree.rootNode){
this.tree.rootNode.set("indent",this.showRoot?0:-1);
}
},_createTreeNode:function(_a9){
return new _1e(_a9);
},_setTextDirAttr:function(_aa){
if(_aa&&this.textDir!=_aa){
this._set("textDir",_aa);
this.rootNode.set("textDir",_aa);
}
}});
_42._TreeNode=_1e;
return _42;
});
