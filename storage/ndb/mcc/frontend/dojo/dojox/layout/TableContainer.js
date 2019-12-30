//>>built
define("dojox/layout/TableContainer",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dojo/_base/array","dojo/dom-prop","dojo/dom-style","dijit/_WidgetBase","dijit/layout/_LayoutWidget"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
_1.experimental("dojox.layout.TableContainer");
var _b=_3("dojox.layout.TableContainer",_a,{cols:1,labelWidth:"100",showLabels:true,orientation:"horiz",spacing:1,customClass:"",postCreate:function(){
this.inherited(arguments);
this._children=[];
this.connect(this,"set",function(_c,_d){
if(_d&&(_c=="orientation"||_c=="customClass"||_c=="cols")){
this.layout();
}
});
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(this._initialized){
return;
}
var _e=this.getChildren();
if(_e.length<1){
return;
}
this._initialized=true;
_4.add(this.domNode,"dijitTableLayout");
_6.forEach(_e,function(_f){
if(!_f.started&&!_f._started){
_f.startup();
}
});
this.resize();
this.layout();
},resize:function(){
_6.forEach(this.getChildren(),function(_10){
if(typeof _10.resize=="function"){
_10.resize();
}
});
},layout:function(){
if(!this._initialized){
return;
}
var _11=this.getChildren();
var _12={};
var _13=this;
function _14(_15,_16,_17){
if(_13.customClass!=""){
var _18=_13.customClass+"-"+(_16||_15.tagName.toLowerCase());
_4.add(_15,_18);
if(arguments.length>2){
_4.add(_15,_18+"-"+_17);
}
}
};
_6.forEach(this._children,_2.hitch(this,function(_19){
_12[_19.id]=_19;
}));
_6.forEach(_11,_2.hitch(this,function(_1a,_1b){
if(!_12[_1a.id]){
this._children.push(_1a);
}
}));
var _1c=_5.create("table",{"width":"100%","class":"tableContainer-table tableContainer-table-"+this.orientation,"cellspacing":this.spacing},this.domNode);
var _1d=_5.create("tbody");
_1c.appendChild(_1d);
_14(_1c,"table",this.orientation);
var _1e=Math.floor(100/this.cols)+"%";
var _1f=_5.create("tr",{},_1d);
var _20=(!this.showLabels||this.orientation=="horiz")?_1f:_5.create("tr",{},_1d);
var _21=this.cols*(this.showLabels?2:1);
var _22=0;
_6.forEach(this._children,_2.hitch(this,function(_23,_24){
var _25=_23.colspan||1;
if(_25>1){
_25=this.showLabels?Math.min(_21-1,_25*2-1):Math.min(_21,_25);
}
if(_22+_25-1+(this.showLabels?1:0)>=_21){
_22=0;
_1f=_5.create("tr",{},_1d);
_20=this.orientation=="horiz"?_1f:_5.create("tr",{},_1d);
}
var _26;
if(this.showLabels){
_26=_5.create("td",{"class":"tableContainer-labelCell"},_1f);
if(_23.spanLabel){
_7.set(_26,this.orientation=="vert"?"rowspan":"colspan",2);
}else{
_14(_26,"labelCell");
var _27={"for":_23.get("id")};
var _28=_5.create("label",_27,_26);
if(Number(this.labelWidth)>-1||String(this.labelWidth).indexOf("%")>-1){
_8.set(_26,"width",String(this.labelWidth).indexOf("%")<0?this.labelWidth+"px":this.labelWidth);
}
_28.innerHTML=_23.get("label")||_23.get("title");
}
}
var _29;
if(_23.spanLabel&&_26){
_29=_26;
}else{
_29=_5.create("td",{"class":"tableContainer-valueCell"},_20);
}
if(_25>1){
_7.set(_29,"colspan",_25);
}
_14(_29,"valueCell",_24);
_29.appendChild(_23.domNode);
_22+=_25+(this.showLabels?1:0);
}));
if(this.table){
this.table.parentNode.removeChild(this.table);
}
_6.forEach(_11,function(_2a){
if(typeof _2a.layout=="function"){
_2a.layout();
}
});
this.table=_1c;
this.resize();
},destroyDescendants:function(_2b){
_6.forEach(this._children,function(_2c){
_2c.destroyRecursive(_2b);
});
},_setSpacingAttr:function(_2d){
this.spacing=_2d;
if(this.table){
this.table.cellspacing=Number(_2d);
}
}});
_b.ChildWidgetProperties={label:"",title:"",spanLabel:false,colspan:1};
_2.extend(_9,_b.ChildWidgetProperties);
return _b;
});
