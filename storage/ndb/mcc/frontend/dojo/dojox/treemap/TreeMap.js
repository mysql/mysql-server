//>>built
define("dojox/treemap/TreeMap",["dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dojo/_base/event","dojo/_base/Color","dojo/touch","dojo/when","dojo/on","dojo/query","dojo/dom-construct","dojo/dom-geometry","dojo/dom-class","dojo/dom-style","./_utils","dijit/_WidgetBase","dojox/widget/_Invalidating","dojox/widget/Selection","dojo/_base/sniff","dojo/uacss"],function(_1,_2,_3,_4,_5,_6,_7,on,_8,_9,_a,_b,_c,_d,_e,_f,_10,has){
return _3("dojox.treemap.TreeMap",[_e,_f,_10],{baseClass:"dojoxTreeMap",store:null,query:{},itemToRenderer:null,_dataChanged:false,rootItem:null,_rootItemChanged:false,tooltipAttr:"",areaAttr:"",_areaChanged:false,labelAttr:"label",labelThreshold:NaN,colorAttr:"",colorModel:null,_coloringChanged:false,groupAttrs:[],groupFuncs:null,_groupFuncs:null,_groupingChanged:false,constructor:function(){
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
this.connect(this.domNode,"mouseover",this._onMouseOver);
this.connect(this.domNode,"mouseout",this._onMouseOut);
this.connect(this.domNode,_6.release,this._onMouseUp);
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
var _14=this.rootItem;
if(_14!=null){
if(this._isLeaf(_14)){
_14=this._getRenderer(_14).parentItem;
}
}
var box=_a.getMarginBox(this.domNode);
if(_14!=null){
this._buildRenderer(this.domNode,null,_14,{x:box.l,y:box.t,w:box.w,h:box.h},0,_12);
}else{
this._buildChildrenRenderers(this.domNode,_14?_14:{__treeRoot:true,children:this._items},0,_12,box);
}
},_setRootItemAttr:function(_15){
this._rootItemChanged=true;
this._set("rootItem",_15);
},_setStoreAttr:function(_16){
var r;
if(_16!=null){
var _17=_16.query(this.query);
if(_17.observe){
_17.observe(_2.hitch(this,this._updateItem),true);
}
r=_7(_17,_2.hitch(this,this._initItems));
}else{
r=this._initItems([]);
}
this._set("store",_16);
return r;
},_initItems:function(_18){
this._dataChanged=true;
this._data=_18;
this.invalidateRendering();
return _18;
},_updateItem:function(_19,_1a,_1b){
if(_1a!=-1){
if(_1b!=_1a){
this._data.splice(_1a,1);
}else{
this._data[_1b]=_19;
}
}else{
if(_1b!=-1){
this._data.splice(_1b,0,_19);
}
}
this._dataChanged=true;
this.invalidateRendering();
},_setGroupAttrsAttr:function(_1c){
this._groupingChanged=true;
if(this.groupFuncs==null){
if(_1c!=null){
this._groupFuncs=_1.map(_1c,function(_1d){
return function(_1e){
return _1e[_1d];
};
});
}else{
this._groupFuncs=null;
}
}
this._set("groupAttrs",_1c);
},_setGroupFuncsAttr:function(_1f){
this._groupingChanged=true;
this._set("groupFuncs",this._groupFuncs=_1f);
if(_1f==null&&this.groupAttrs!=null){
this._groupFuncs=_1.map(this.groupAttrs,function(_20){
return function(_21){
return _21[_20];
};
});
}
},_setAreaAttrAttr:function(_22){
this._areaChanged=true;
this._set("areaAttr",_22);
},areaFunc:function(_23,_24){
return (this.areaAttr&&this.areaAttr.length>0)?parseFloat(_23[this.areaAttr]):1;
},_setAreaFuncAttr:function(_25){
this._areaChanged=true;
this._set("areaFunc",_25);
},labelFunc:function(_26,_27){
var _28=(this.labelAttr&&this.labelAttr.length>0)?_26[this.labelAttr]:null;
return _28?_28.toString():null;
},tooltipFunc:function(_29,_2a){
var _2b=(this.tooltipAttr&&this.tooltipAttr.length>0)?_29[this.tooltipAttr]:null;
return _2b?_2b.toString():null;
},_setColorModelAttr:function(_2c){
this._coloringChanged=true;
this._set("colorModel",_2c);
},_setColorAttrAttr:function(_2d){
this._coloringChanged=true;
this._set("colorAttr",_2d);
},colorFunc:function(_2e,_2f){
var _30=(this.colorAttr&&this.colorAttr.length>0)?_2e[this.colorAttr]:0;
if(_30==null){
_30=0;
}
return parseFloat(_30);
},_setColorFuncAttr:function(_31){
this._coloringChanged=true;
this._set("colorFunc",_31);
},createRenderer:function(_32,_33,_34){
var div=_9.create("div");
if(_34!="header"){
_c.set(div,"overflow","hidden");
_c.set(div,"position","absolute");
}
return div;
},styleRenderer:function(_35,_36,_37,_38){
switch(_38){
case "leaf":
_c.set(_35,"background",this.getColorForItem(_36).toHex());
case "header":
var _39=this.getLabelForItem(_36);
if(_39&&(isNaN(this.labelThreshold)||_37<this.labelThreshold)){
_35.innerHTML=_39;
}else{
_9.empty(_35);
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
},_removeAreaForGroup:function(_3a){
var _3b;
if(_3a!=null){
if(_3a.__treeValue){
delete _3a.__treeValue;
_3b=_3a.children;
}else{
return;
}
}else{
_3b=this._items;
}
if(_3b){
for(var i=0;i<_3b.length;++i){
this._removeAreaForGroup(_3b[i]);
}
}
},_getAreaForItem:function(_3c){
var _3d=this.areaFunc(_3c,this.store);
return isNaN(_3d)?0:_3d;
},_computeAreaForItem:function(_3e){
var _3f;
if(_3e.__treeID){
_3f=_3e.__treeValue;
if(!_3f){
_3f=0;
var _40=_3e.children;
for(var i=0;i<_40.length;++i){
_3f+=this._computeAreaForItem(_40[i]);
}
_3e.__treeValue=_3f;
}
}else{
_3f=this._getAreaForItem(_3e);
}
return _3f;
},getColorForItem:function(_41){
var _42=this.colorFunc(_41,this.store);
if(this.colorModel!=null){
return this.colorModel.getColor(_42);
}else{
return new _5(_42);
}
},getLabelForItem:function(_43){
return _43.__treeName?_43.__treeName:this.labelFunc(_43,this.store);
},_buildChildrenRenderers:function(_44,_45,_46,_47,_48,_49){
var _4a=_45.children;
var box=_a.getMarginBox(_44);
var _4b=_d.solve(_4a,box.w,box.h,_2.hitch(this,this._computeAreaForItem),!this.isLeftToRight());
var _4c=_4b.rectangles;
if(_48){
_4c=_1.map(_4c,function(_4d){
_4d.x+=_48.l;
_4d.y+=_48.t;
return _4d;
});
}
var _4e;
for(var j=0;j<_4a.length;++j){
_4e=_4c[j];
this._buildRenderer(_44,_45,_4a[j],_4e,_46,_47,_49);
}
},_isLeaf:function(_4f){
return !_4f.children;
},_isRoot:function(_50){
return _50.__treeRoot;
},_getRenderer:function(_51,_52,_53){
if(_52){
for(var i=0;i<_53.children.length;++i){
if(_53.children[i].item==_51){
return _53.children[i];
}
}
}
return this.itemToRenderer[this.getIdentity(_51)];
},_buildRenderer:function(_54,_55,_56,_57,_58,_59,_5a){
var _5b=this._isLeaf(_56);
var _5c=!_59?this._getRenderer(_56,_5a,_54):null;
_5c=_5b?this._updateLeafRenderer(_5c,_56,_58):this._updateGroupRenderer(_5c,_56,_58);
if(_59){
_5c.level=_58;
_5c.item=_56;
_5c.parentItem=_55;
this.itemToRenderer[this.getIdentity(_56)]=_5c;
this.updateRenderers(_56);
}
var x=Math.floor(_57.x);
var y=Math.floor(_57.y);
var w=Math.floor(_57.x+_57.w+1e-11)-x;
var h=Math.floor(_57.y+_57.h+1e-11)-y;
if(_59){
_9.place(_5c,_54);
}
_a.setMarginBox(_5c,{l:x,t:y,w:w,h:h});
if(!_5b){
var box=_a.getContentBox(_5c);
this._layoutGroupContent(_5c,box.w,box.h,_58+1,_59,_5a);
}
this.onRendererUpdated({renderer:_5c,item:_56,kind:_5b?"leaf":"group",level:_58});
},_layoutGroupContent:function(_5d,_5e,_5f,_60,_61,_62){
var _63=_8(".dojoxTreeMapHeader",_5d)[0];
var _64=_8(".dojoxTreeMapGroupContent",_5d)[0];
if(_63==null||_64==null){
return;
}
var box=_a.getMarginBox(_63);
if(box.h>_5f){
box.h=_5f;
_c.set(_64,"display","none");
}else{
_c.set(_64,"display","block");
_a.setMarginBox(_64,{l:0,t:box.h,w:_5e,h:(_5f-box.h)});
this._buildChildrenRenderers(_64,_5d.item,_60,_61,null,_62);
}
_a.setMarginBox(_63,{l:0,t:0,w:_5e,h:box.h});
},_updateGroupRenderer:function(_65,_66,_67){
var _68=_65==null;
if(_65==null){
_65=this.createRenderer("div",_67,"group");
_b.add(_65,"dojoxTreeMapGroup");
}
this.styleRenderer(_65,_66,_67,"group");
var _69=_8(".dojoxTreeMapHeader",_65)[0];
_69=this._updateHeaderRenderer(_69,_66,_67);
if(_68){
_9.place(_69,_65);
}
var _6a=_8(".dojoxTreeMapGroupContent",_65)[0];
_6a=this._updateGroupContentRenderer(_6a,_66,_67);
if(_68){
_9.place(_6a,_65);
}
return _65;
},_updateHeaderRenderer:function(_6b,_6c,_6d){
if(_6b==null){
_6b=this.createRenderer(_6c,_6d,"header");
_b.add(_6b,"dojoxTreeMapHeader");
_b.add(_6b,"dojoxTreeMapHeader_"+_6d);
}
this.styleRenderer(_6b,_6c,_6d,"header");
return _6b;
},_updateLeafRenderer:function(_6e,_6f,_70){
if(_6e==null){
_6e=this.createRenderer(_6f,_70,"leaf");
_b.add(_6e,"dojoxTreeMapLeaf");
_b.add(_6e,"dojoxTreeMapLeaf_"+_70);
}
this.styleRenderer(_6e,_6f,_70,"leaf");
var _71=this.tooltipFunc(_6f,this.store);
if(_71){
_6e.title=_71;
}
return _6e;
},_updateGroupContentRenderer:function(_72,_73,_74){
if(_72==null){
_72=this.createRenderer(_73,_74,"content");
_b.add(_72,"dojoxTreeMapGroupContent");
_b.add(_72,"dojoxTreeMapGroupContent_"+_74);
}
this.styleRenderer(_72,_73,_74,"content");
return _72;
},_getRendererFromTarget:function(_75){
var _76=_75;
while(_76!=this.domNode&&!_76.item){
_76=_76.parentNode;
}
return _76;
},_onMouseOver:function(e){
var _77=this._getRendererFromTarget(e.target);
if(_77.item){
var _78=_77.item;
this._hoveredItem=_78;
this.updateRenderers(_78);
this.onItemRollOver({renderer:_77,item:_78,triggerEvent:e});
}
},_onMouseOut:function(e){
var _79=this._getRendererFromTarget(e.target);
if(_79.item){
var _7a=_79.item;
this._hoveredItem=null;
this.updateRenderers(_7a);
this.onItemRollOut({renderer:_79,item:_7a,triggerEvent:e});
}
},_onMouseUp:function(e){
var _7b=this._getRendererFromTarget(e.target);
if(_7b.item){
this.selectFromEvent(e,_7b.item,e.currentTarget,true);
}
},onRendererUpdated:function(){
},onItemRollOver:function(){
},onItemRollOut:function(){
},updateRenderers:function(_7c){
if(!_7c){
return;
}
if(!_2.isArray(_7c)){
_7c=[_7c];
}
for(var i=0;i<_7c.length;i++){
var _7d=_7c[i];
var _7e=this._getRenderer(_7d);
if(!_7e){
continue;
}
var _7f=this.isItemSelected(_7d);
var ie=has("ie");
var div;
if(_7f){
_b.add(_7e,"dojoxTreeMapSelected");
if(ie&&(has("quirks")||ie<9)){
div=_7e.previousSibling;
var _80=_c.get(_7e);
if(!div||!_b.contains(div,"dojoxTreeMapIEHack")){
div=this.createRenderer(_7d,-10,"group");
_b.add(div,"dojoxTreeMapIEHack");
_b.add(div,"dojoxTreeMapSelected");
_c.set(div,{position:"absolute",overflow:"hidden"});
_9.place(div,_7e,"before");
}
var _81=2*parseInt(_c.get(div,"border-width"));
if(this._isLeaf(_7d)){
_81-=1;
}else{
_81+=1;
}
if(_80["left"]!="auto"){
_c.set(div,{left:(parseInt(_80["left"])+1)+"px",top:(parseInt(_80["top"])+1)+"px",width:(parseInt(_80["width"])-_81)+"px",height:(parseInt(_80["height"])-_81)+"px"});
}
}
}else{
if(ie&&(has("quirks")||ie<9)){
div=_7e.previousSibling;
if(div&&_b.contains(div,"dojoxTreeMapIEHack")){
div.parentNode.removeChild(div);
}
}
_b.remove(_7e,"dojoxTreeMapSelected");
}
if(this._hoveredItem==_7d){
_b.add(_7e,"dojoxTreeMapHovered");
}else{
_b.remove(_7e,"dojoxTreeMapHovered");
}
if(_7f||this._hoveredItem==_7d){
_c.set(_7e,"zIndex",20);
}else{
_c.set(_7e,"zIndex",(has("ie")<=7)?0:"auto");
}
}
}});
});
