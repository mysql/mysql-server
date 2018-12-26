//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/layout/_LayoutWidget,dijit/_Templated"],function(_1,_2,_3){
_2.provide("dojox.widget.SortList");
_2.experimental("dojox.widget.SortList");
_2.require("dijit.layout._LayoutWidget");
_2.require("dijit._Templated");
_2.declare("dojox.widget.SortList",[_1.layout._LayoutWidget,_1._Templated],{title:"",heading:"",descending:true,selected:null,sortable:true,store:"",key:"name",baseClass:"dojoxSortList",templateString:_2.cache("dojox.widget","SortList/SortList.html","<div class=\"sortList\" id=\"${id}\">\n\t\t<div class=\"sortListTitle\" dojoAttachPoint=\"titleNode\">\n\t\t<div class=\"dijitInline sortListIcon\">&thinsp;</div>\n\t\t<span dojoAttachPoint=\"focusNode\">${title}</span>\n\t\t</div>\n\t\t<div class=\"sortListBodyWrapper\" dojoAttachEvent=\"onmouseover: _set, onmouseout: _unset, onclick:_handleClick\" dojoAttachPoint=\"bodyWrapper\">\n\t\t<ul dojoAttachPoint=\"containerNode\" class=\"sortListBody\"></ul>\n\t</div>\n</div>"),_addItem:function(_4){
_2.create("li",{innerHTML:this.store.getValue(_4,this.key).replace(/</g,"&lt;")},this.containerNode);
},postCreate:function(){
if(this.store){
this.store=_2.getObject(this.store);
var _5={onItem:_2.hitch(this,"_addItem"),onComplete:_2.hitch(this,"onSort")};
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
setTimeout(_2.hitch(this,"resize"),5);
if(this.sortable){
this.connect(this.titleNode,"onclick","onSort");
}
},resize:function(){
this.inherited(arguments);
var _6=((this._contentBox.h)-(_2.style(this.titleNode,"height")))-10;
this.bodyWrapper.style.height=Math.abs(_6)+"px";
},onSort:function(e){
var _7=_2.query("li",this.domNode);
if(this.sortable){
this.descending=!this.descending;
_2.addClass(this.titleNode,((this.descending)?"sortListDesc":"sortListAsc"));
_2.removeClass(this.titleNode,((this.descending)?"sortListAsc":"sortListDesc"));
_7.sort(this._sorter);
if(this.descending){
_7.reverse();
}
}
var i=0;
_2.forEach(_7,function(_8){
_2[(i++)%2===0?"addClass":"removeClass"](_8,"sortListItemOdd");
this.containerNode.appendChild(_8);
},this);
},_set:function(e){
if(e.target!==this.bodyWrapper){
_2.addClass(e.target,"sortListItemHover");
}
},_unset:function(e){
_2.removeClass(e.target,"sortListItemHover");
},_handleClick:function(e){
_2.toggleClass(e.target,"sortListItemSelected");
e.target.focus();
this._updateValues(e.target.innerHTML);
},_updateValues:function(){
this._selected=_2.query("li.sortListItemSelected",this.containerNode);
this.selected=[];
_2.forEach(this._selected,function(_9){
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
