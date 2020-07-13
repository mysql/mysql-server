//>>built
define("dojox/treemap/TreeMap",["dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dojo/_base/event","dojo/_base/Color","dojo/touch","dojo/when","dojo/on","dojo/query","dojo/dom-construct","dojo/dom-geometry","dojo/dom-class","dojo/dom-style","./_utils","dijit/_WidgetBase","dojox/widget/_Invalidating","dojox/widget/Selection","dojo/_base/sniff","dojo/uacss"],function(_1,_2,_3,_4,_5,_6,_7,on,_8,_9,_a,_b,_c,_d,_e,_f,_10,has){
return _3("dojox.treemap.TreeMap",[_e,_f,_10],{baseClass:"dojoxTreeMap",store:null,query:{},queryOptions:null,itemToRenderer:null,_dataChanged:false,rootItem:null,_rootItemChanged:false,tooltipAttr:"",areaAttr:"",_areaChanged:false,labelAttr:"label",labelThreshold:NaN,colorAttr:"",colorModel:null,_coloringChanged:false,groupAttrs:[],groupFuncs:null,_groupFuncs:null,_groupingChanged:false,constructor:function(){
this.itemToRenderer={};
this.invalidatingProperties=["colorModel","groupAttrs","groupFuncs","areaAttr","areaFunc","labelAttr","labelFunc","labelThreshold","tooltipAttr","tooltipFunc","colorAttr","colorFunc","rootItem"];
},getIdentity:function(_11){
return _11.__treeID?_11.__treeID:this.store.getIdentity(_11);
},resize:function(box){
if(box){
_a.setMarginBox(this.domNode,box);
this.invalidateRendering();
}
},postCreate:function(){
this.inherited(arguments);
this.own(on(this.domNode,"mouseover",_2.hitch(this,this._onMouseOver)));
this.own(on(this.domNode,"mouseout",_2.hitch(this,this._onMouseOut)));
this.own(on(this.domNode,_6.release,_2.hitch(this,this._onMouseUp)));
this.domNode.setAttribute("role","presentation");
this.domNode.setAttribute("aria-label","treemap");
},buildRendering:function(){
this.inherited(arguments);
this.refreshRendering();
},refreshRendering:function(){
var _12=false;
if(this._dataChanged){
this._dataChanged=false;
this._groupingChanged=true;
this._coloringChanged=true;
}
if(this._groupingChanged){
this._groupingChanged=false;
this._set("rootItem",null);
this._updateTreeMapHierarchy();
_12=true;
}
if(this._rootItemChanged){
this._rootItemChanged=false;
_12=true;
}
if(this._coloringChanged){
this._coloringChanged=false;
if(this.colorModel!=null&&this._data!=null&&this.colorModel.initialize){
this.colorModel.initialize(this._data,_2.hitch(this,function(_13){
return this.colorFunc(_13,this.store);
}));
}
}
if(this._areaChanged){
this._areaChanged=false;
this._removeAreaForGroup();
}
if(this.domNode==undefined||this._items==null){
return;
}
if(_12){
_9.empty(this.domNode);
}
var _14=this.rootItem,_15;
if(_14!=null){
var _16=this._getRenderer(_14);
if(_16){
if(this._isLeaf(_14)){
_14=_16.parentItem;
}
_15=_16.parentItem;
}
}
var box=_a.getMarginBox(this.domNode);
if(_14!=null){
this._buildRenderer(this.domNode,_15,_14,{x:box.l,y:box.t,w:box.w,h:box.h},0,_12);
}else{
this._buildChildrenRenderers(this.domNode,_14?_14:{__treeRoot:true,children:this._items},0,_12,box);
}
},_setRootItemAttr:function(_17){
this._rootItemChanged=true;
this._set("rootItem",_17);
},_setStoreAttr:function(_18){
var r;
if(this._observeHandler){
this._observeHandler.remove();
this._observeHandler=null;
}
if(_18!=null){
var _19=_18.query(this.query,this.queryOptions);
if(_19.observe){
this._observeHandler=_19.observe(_2.hitch(this,this._updateItem),true);
}
r=_7(_19,_2.hitch(this,this._initItems));
}else{
r=this._initItems([]);
}
this._set("store",_18);
return r;
},_initItems:function(_1a){
this._dataChanged=true;
this._data=_1a;
this.invalidateRendering();
return _1a;
},_updateItem:function(_1b,_1c,_1d){
if(_1c!=-1){
if(_1d!=_1c){
this._data.splice(_1c,1);
}else{
this._data[_1d]=_1b;
}
}else{
if(_1d!=-1){
this._data.splice(_1d,0,_1b);
}
}
this._dataChanged=true;
this.invalidateRendering();
},_setGroupAttrsAttr:function(_1e){
this._groupingChanged=true;
if(this.groupFuncs==null){
if(_1e!=null){
this._groupFuncs=_1.map(_1e,function(_1f){
return function(_20){
return _20[_1f];
};
});
}else{
this._groupFuncs=null;
}
}
this._set("groupAttrs",_1e);
},_setGroupFuncsAttr:function(_21){
this._groupingChanged=true;
this._set("groupFuncs",this._groupFuncs=_21);
if(_21==null&&this.groupAttrs!=null){
this._groupFuncs=_1.map(this.groupAttrs,function(_22){
return function(_23){
return _23[_22];
};
});
}
},_setAreaAttrAttr:function(_24){
this._areaChanged=true;
this._set("areaAttr",_24);
},areaFunc:function(_25,_26){
return (this.areaAttr&&this.areaAttr.length>0)?parseFloat(_25[this.areaAttr]):1;
},_setAreaFuncAttr:function(_27){
this._areaChanged=true;
this._set("areaFunc",_27);
},labelFunc:function(_28,_29){
var _2a=(this.labelAttr&&this.labelAttr.length>0)?_28[this.labelAttr]:null;
return _2a?_2a.toString():null;
},tooltipFunc:function(_2b,_2c){
var _2d=(this.tooltipAttr&&this.tooltipAttr.length>0)?_2b[this.tooltipAttr]:null;
return _2d?_2d.toString():null;
},_setColorModelAttr:function(_2e){
this._coloringChanged=true;
this._set("colorModel",_2e);
},_setColorAttrAttr:function(_2f){
this._coloringChanged=true;
this._set("colorAttr",_2f);
},colorFunc:function(_30,_31){
var _32=(this.colorAttr&&this.colorAttr.length>0)?_30[this.colorAttr]:0;
if(_32==null){
_32=0;
}
return parseFloat(_32);
},_setColorFuncAttr:function(_33){
this._coloringChanged=true;
this._set("colorFunc",_33);
},createRenderer:function(_34,_35,_36){
var div=_9.create("div");
if(_36!="header"){
_c.set(div,"overflow","hidden");
_c.set(div,"position","absolute");
}
return div;
},styleRenderer:function(_37,_38,_39,_3a){
switch(_3a){
case "leaf":
_c.set(_37,"background",this.getColorForItem(_38).toHex());
case "header":
var _3b=this.getLabelForItem(_38);
if(_3b&&(isNaN(this.labelThreshold)||_39<this.labelThreshold)){
_37.innerHTML=_3b;
}else{
_9.empty(_37);
}
break;
default:
}
},_updateTreeMapHierarchy:function(){
if(this._data==null){
return;
}
if(this._groupFuncs!=null&&this._groupFuncs.length>0){
this._items=_d.group(this._data,this._groupFuncs,_2.hitch(this,this._getAreaForItem)).children;
}else{
this._items=this._data;
}
},_removeAreaForGroup:function(_3c){
var _3d;
if(_3c!=null){
if(_3c.__treeValue){
delete _3c.__treeValue;
_3d=_3c.children;
}else{
return;
}
}else{
_3d=this._items;
}
if(_3d){
for(var i=0;i<_3d.length;++i){
this._removeAreaForGroup(_3d[i]);
}
}
},_getAreaForItem:function(_3e){
var _3f=this.areaFunc(_3e,this.store);
return isNaN(_3f)?0:_3f;
},_computeAreaForItem:function(_40){
var _41;
if(_40.__treeID){
_41=_40.__treeValue;
if(!_41){
_41=0;
var _42=_40.children;
for(var i=0;i<_42.length;++i){
_41+=this._computeAreaForItem(_42[i]);
}
_40.__treeValue=_41;
}
}else{
_41=this._getAreaForItem(_40);
}
return _41;
},getColorForItem:function(_43){
var _44=this.colorFunc(_43,this.store);
if(this.colorModel!=null){
return this.colorModel.getColor(_44);
}else{
return new _5(_44);
}
},getLabelForItem:function(_45){
return _45.__treeName?_45.__treeName:this.labelFunc(_45,this.store);
},_buildChildrenRenderers:function(_46,_47,_48,_49,_4a,_4b){
var _4c=_47.children;
var box=_a.getMarginBox(_46);
var _4d=_d.solve(_4c,box.w,box.h,_2.hitch(this,this._computeAreaForItem),!this.isLeftToRight());
var _4e=_4d.rectangles;
if(_4a){
_4e=_1.map(_4e,function(_4f){
_4f.x+=_4a.l;
_4f.y+=_4a.t;
return _4f;
});
}
var _50;
for(var j=0;j<_4c.length;++j){
_50=_4e[j];
this._buildRenderer(_46,_47,_4c[j],_50,_48,_49,_4b);
}
},_isLeaf:function(_51){
return !_51.children;
},_isRoot:function(_52){
return _52.__treeRoot;
},_getRenderer:function(_53,_54,_55){
if(_54){
for(var i=0;i<_55.children.length;++i){
if(_55.children[i].item==_53){
return _55.children[i];
}
}
}
return this.itemToRenderer[this.getIdentity(_53)];
},_buildRenderer:function(_56,_57,_58,_59,_5a,_5b,_5c){
var _5d=this._isLeaf(_58);
var _5e=!_5b?this._getRenderer(_58,_5c,_56):null;
_5e=_5d?this._updateLeafRenderer(_5e,_58,_5a):this._updateGroupRenderer(_5e,_58,_5a);
if(_5b){
_5e.level=_5a;
_5e.item=_58;
_5e.parentItem=_57;
this.itemToRenderer[this.getIdentity(_58)]=_5e;
this.updateRenderers(_58);
}
var x=Math.floor(_59.x);
var y=Math.floor(_59.y);
var w=Math.floor(_59.x+_59.w+1e-11)-x;
var h=Math.floor(_59.y+_59.h+1e-11)-y;
if(_5b){
_9.place(_5e,_56);
}
_a.setMarginBox(_5e,{l:x,t:y,w:w,h:h});
if(!_5d){
var box=_a.getContentBox(_5e);
this._layoutGroupContent(_5e,box.w,box.h,_5a+1,_5b,_5c);
}
this.onRendererUpdated({renderer:_5e,item:_58,kind:_5d?"leaf":"group",level:_5a});
},_layoutGroupContent:function(_5f,_60,_61,_62,_63,_64){
var _65=_8(".dojoxTreeMapHeader",_5f)[0];
var _66=_8(".dojoxTreeMapGroupContent",_5f)[0];
if(_65==null||_66==null){
return;
}
var box=_a.getMarginBox(_65);
if(box.h>_61){
box.h=_61;
_c.set(_66,"display","none");
}else{
_c.set(_66,"display","block");
_a.setMarginBox(_66,{l:0,t:box.h,w:_60,h:(_61-box.h)});
this._buildChildrenRenderers(_66,_5f.item,_62,_63,null,_64);
}
_a.setMarginBox(_65,{l:0,t:0,w:_60,h:box.h});
},_updateGroupRenderer:function(_67,_68,_69){
var _6a=_67==null;
if(_67==null){
_67=this.createRenderer("div",_69,"group");
_b.add(_67,"dojoxTreeMapGroup");
}
this.styleRenderer(_67,_68,_69,"group");
var _6b=_8(".dojoxTreeMapHeader",_67)[0];
_6b=this._updateHeaderRenderer(_6b,_68,_69);
if(_6a){
_9.place(_6b,_67);
}
var _6c=_8(".dojoxTreeMapGroupContent",_67)[0];
_6c=this._updateGroupContentRenderer(_6c,_68,_69);
if(_6a){
_9.place(_6c,_67);
}
return _67;
},_updateHeaderRenderer:function(_6d,_6e,_6f){
if(_6d==null){
_6d=this.createRenderer(_6e,_6f,"header");
_b.add(_6d,"dojoxTreeMapHeader");
_b.add(_6d,"dojoxTreeMapHeader_"+_6f);
}
this.styleRenderer(_6d,_6e,_6f,"header");
return _6d;
},_updateLeafRenderer:function(_70,_71,_72){
if(_70==null){
_70=this.createRenderer(_71,_72,"leaf");
_b.add(_70,"dojoxTreeMapLeaf");
_b.add(_70,"dojoxTreeMapLeaf_"+_72);
}
this.styleRenderer(_70,_71,_72,"leaf");
var _73=this.tooltipFunc(_71,this.store);
if(_73){
_70.title=_73;
}
return _70;
},_updateGroupContentRenderer:function(_74,_75,_76){
if(_74==null){
_74=this.createRenderer(_75,_76,"content");
_b.add(_74,"dojoxTreeMapGroupContent");
_b.add(_74,"dojoxTreeMapGroupContent_"+_76);
}
this.styleRenderer(_74,_75,_76,"content");
return _74;
},_getRendererFromTarget:function(_77){
var _78=_77;
while(_78!=this.domNode&&!_78.item){
_78=_78.parentNode;
}
return _78;
},_onMouseOver:function(e){
var _79=this._getRendererFromTarget(e.target);
if(_79.item){
var _7a=_79.item;
this._hoveredItem=_7a;
this.updateRenderers(_7a);
this.onItemRollOver({renderer:_79,item:_7a,triggerEvent:e});
}
},_onMouseOut:function(e){
var _7b=this._getRendererFromTarget(e.target);
if(_7b.item){
var _7c=_7b.item;
this._hoveredItem=null;
this.updateRenderers(_7c);
this.onItemRollOut({renderer:_7b,item:_7c,triggerEvent:e});
}
},_onMouseUp:function(e){
var _7d=this._getRendererFromTarget(e.target);
if(_7d.item){
this.selectFromEvent(e,_7d.item,_7d,true);
}
},onRendererUpdated:function(){
},onItemRollOver:function(){
},onItemRollOut:function(){
},updateRenderers:function(_7e){
if(!_7e){
return;
}
if(!_2.isArray(_7e)){
_7e=[_7e];
}
for(var i=0;i<_7e.length;i++){
var _7f=_7e[i];
var _80=this._getRenderer(_7f);
if(!_80){
continue;
}
var _81=this.isItemSelected(_7f);
var ie=has("ie");
var div;
if(_81){
_b.add(_80,"dojoxTreeMapSelected");
if(ie&&(has("quirks")||ie<9)){
div=_80.previousSibling;
var _82=_c.get(_80);
if(!div||!_b.contains(div,"dojoxTreeMapIEHack")){
div=this.createRenderer(_7f,-10,"group");
_b.add(div,"dojoxTreeMapIEHack");
_b.add(div,"dojoxTreeMapSelected");
_c.set(div,{position:"absolute",overflow:"hidden"});
_9.place(div,_80,"before");
}
var _83=2*parseInt(_c.get(div,"border-width"));
if(this._isLeaf(_7f)){
_83-=1;
}else{
_83+=1;
}
if(_82["left"]!="auto"){
_c.set(div,{left:(parseInt(_82["left"])+1)+"px",top:(parseInt(_82["top"])+1)+"px",width:(parseInt(_82["width"])-_83)+"px",height:(parseInt(_82["height"])-_83)+"px"});
}
}
}else{
if(ie&&(has("quirks")||ie<9)){
div=_80.previousSibling;
if(div&&_b.contains(div,"dojoxTreeMapIEHack")){
div.parentNode.removeChild(div);
}
}
_b.remove(_80,"dojoxTreeMapSelected");
}
if(this._hoveredItem==_7f){
_b.add(_80,"dojoxTreeMapHovered");
}else{
_b.remove(_80,"dojoxTreeMapHovered");
}
if(_81||this._hoveredItem==_7f){
_c.set(_80,"zIndex",20);
}else{
_c.set(_80,"zIndex",(has("ie")<=7)?0:"auto");
}
}
}});
});
