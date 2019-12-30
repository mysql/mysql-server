//>>built
define("dojox/widget/SortList",["dojo","dijit","dojox","dojo/require!dijit/layout/_LayoutWidget,dijit/_Templated"],function(_1,_2,_3){
_1.provide("dojox.widget.SortList");
_1.experimental("dojox.widget.SortList");
_1.require("dijit.layout._LayoutWidget");
_1.require("dijit._Templated");
_1.declare("dojox.widget.SortList",[_2.layout._LayoutWidget,_2._Templated],{title:"",heading:"",descending:true,selected:null,sortable:true,store:"",key:"name",baseClass:"dojoxSortList",templateString:_1.cache("dojox.widget","SortList/SortList.html","<div class=\"sortList\" id=\"${id}\">\n\t\t<div class=\"sortListTitle\" dojoAttachPoint=\"titleNode\">\n\t\t<div class=\"dijitInline sortListIcon\">&thinsp;</div>\n\t\t<span dojoAttachPoint=\"focusNode\">${title}</span>\n\t\t</div>\n\t\t<div class=\"sortListBodyWrapper\" dojoAttachEvent=\"onmouseover: _set, onmouseout: _unset, onclick:_handleClick\" dojoAttachPoint=\"bodyWrapper\">\n\t\t<ul dojoAttachPoint=\"containerNode\" class=\"sortListBody\"></ul>\n\t</div>\n</div>"),_addItem:function(_4){
_1.create("li",{innerHTML:this.store.getValue(_4,this.key).replace(/</g,"&lt;")},this.containerNode);
},postCreate:function(){
if(this.store){
this.store=_1.getObject(this.store);
var _5={onItem:_1.hitch(this,"_addItem"),onComplete:_1.hitch(this,"onSort")};
this.store.fetch(_5);
}else{
this.onSort();
}
this.inherited(arguments);
},startup:function(){
this.inherited(arguments);
if(this.heading){
this.setTitle(this.heading);
this.title=this.heading;
}
setTimeout(_1.hitch(this,"resize"),5);
if(this.sortable){
this.connect(this.titleNode,"onclick","onSort");
}
},resize:function(){
this.inherited(arguments);
var _6=((this._contentBox.h)-(_1.style(this.titleNode,"height")))-10;
this.bodyWrapper.style.height=Math.abs(_6)+"px";
},onSort:function(e){
var _7=_1.query("li",this.domNode);
if(this.sortable){
this.descending=!this.descending;
_1.addClass(this.titleNode,((this.descending)?"sortListDesc":"sortListAsc"));
_1.removeClass(this.titleNode,((this.descending)?"sortListAsc":"sortListDesc"));
_7.sort(this._sorter);
if(this.descending){
_7.reverse();
}
}
var i=0;
_1.forEach(_7,function(_8){
_1[(i++)%2===0?"addClass":"removeClass"](_8,"sortListItemOdd");
this.containerNode.appendChild(_8);
},this);
},_set:function(e){
if(e.target!==this.bodyWrapper){
_1.addClass(e.target,"sortListItemHover");
}
},_unset:function(e){
_1.removeClass(e.target,"sortListItemHover");
},_handleClick:function(e){
_1.toggleClass(e.target,"sortListItemSelected");
e.target.focus();
this._updateValues(e.target.innerHTML);
},_updateValues:function(){
this._selected=_1.query("li.sortListItemSelected",this.containerNode);
this.selected=[];
_1.forEach(this._selected,function(_9){
this.selected.push(_9.innerHTML);
},this);
this.onChanged(arguments);
},_sorter:function(a,b){
var _a=a.innerHTML;
var _b=b.innerHTML;
if(_a>_b){
return 1;
}
if(_a<_b){
return -1;
}
return 0;
},setTitle:function(_c){
this.focusNode.innerHTML=this.title=_c;
},onChanged:function(){
}});
});
