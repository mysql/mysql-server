//>>built
define("dojox/grid/TreeSelection",["../main","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/dom-attr","dojo/query","./DataSelection"],function(_1,_2,_3,_4,_5,_6,_7){
return _2("dojox.grid.TreeSelection",_7,{setMode:function(_8){
this.selected={};
this.sorted_sel=[];
this.sorted_ltos={};
this.sorted_stol={};
_7.prototype.setMode.call(this,_8);
},addToSelection:function(_9){
if(this.mode=="none"){
return;
}
var _a=null;
if(typeof _9=="number"||typeof _9=="string"){
_a=_9;
}else{
_a=this.grid.getItemIndex(_9);
}
if(this.selected[_a]){
this.selectedIndex=_a;
}else{
if(this.onCanSelect(_a)!==false){
this.selectedIndex=_a;
var _b=_6("tr[dojoxTreeGridPath='"+_a+"']",this.grid.domNode);
if(_b.length){
_5.set(_b[0],"aria-selected","true");
}
this._beginUpdate();
this.selected[_a]=true;
this._insertSortedSelection(_a);
this.onSelected(_a);
this._endUpdate();
}
}
},deselect:function(_c){
if(this.mode=="none"){
return;
}
var _d=null;
if(typeof _c=="number"||typeof _c=="string"){
_d=_c;
}else{
_d=this.grid.getItemIndex(_c);
}
if(this.selectedIndex==_d){
this.selectedIndex=-1;
}
if(this.selected[_d]){
if(this.onCanDeselect(_d)===false){
return;
}
var _e=_6("tr[dojoxTreeGridPath='"+_d+"']",this.grid.domNode);
if(_e.length){
_5.set(_e[0],"aria-selected","false");
}
this._beginUpdate();
delete this.selected[_d];
this._removeSortedSelection(_d);
this.onDeselected(_d);
this._endUpdate();
}
},getSelected:function(){
var _f=[];
for(var i in this.selected){
if(this.selected[i]){
_f.push(this.grid.getItem(i));
}
}
return _f;
},getSelectedCount:function(){
var c=0;
for(var i in this.selected){
if(this.selected[i]){
c++;
}
}
return c;
},_bsearch:function(v){
var o=this.sorted_sel;
var h=o.length-1,l=0,m;
while(l<=h){
var cmp=this._comparePaths(o[m=(l+h)>>1],v);
if(cmp<0){
l=m+1;
continue;
}
if(cmp>0){
h=m-1;
continue;
}
return m;
}
return cmp<0?m-cmp:m;
},_comparePaths:function(a,b){
for(var i=0,l=(a.length<b.length?a.length:b.length);i<l;i++){
if(a[i]<b[i]){
return -1;
}
if(a[i]>b[i]){
return 1;
}
}
if(a.length<b.length){
return -1;
}
if(a.length>b.length){
return 1;
}
return 0;
},_insertSortedSelection:function(_10){
_10=String(_10);
var s=this.sorted_sel;
var sl=this.sorted_ltos;
var ss=this.sorted_stol;
var _11=_10.split("/");
_11=_3.map(_11,function(_12){
return parseInt(_12,10);
});
sl[_11]=_10;
ss[_10]=_11;
if(s.length===0){
s.push(_11);
return;
}
if(s.length==1){
var cmp=this._comparePaths(s[0],_11);
if(cmp==1){
s.unshift(_11);
}else{
s.push(_11);
}
return;
}
var idx=this._bsearch(_11);
this.sorted_sel.splice(idx,0,_11);
},_removeSortedSelection:function(_13){
_13=String(_13);
var s=this.sorted_sel;
var sl=this.sorted_ltos;
var ss=this.sorted_stol;
if(s.length===0){
return;
}
var _14=ss[_13];
if(!_14){
return;
}
var idx=this._bsearch(_14);
if(idx>-1){
delete sl[_14];
delete ss[_13];
s.splice(idx,1);
}
},getFirstSelected:function(){
if(!this.sorted_sel.length||this.mode=="none"){
return -1;
}
var _15=this.sorted_sel[0];
if(!_15){
return -1;
}
_15=this.sorted_ltos[_15];
if(!_15){
return -1;
}
return _15;
},getNextSelected:function(_16){
if(!this.sorted_sel.length||this.mode=="none"){
return -1;
}
_16=String(_16);
var _17=this.sorted_stol[_16];
if(!_17){
return -1;
}
var idx=this._bsearch(_17);
var _18=this.sorted_sel[idx+1];
if(!_18){
return -1;
}
return this.sorted_ltos[_18];
},_range:function(_19,_1a,_1b){
if(!_4.isString(_19)&&_19<0){
_19=_1a;
}
var _1c=this.grid.layout.cells,_1d=this.grid.store,_1e=this.grid;
_19=new _1.grid.TreePath(String(_19),_1e);
_1a=new _1.grid.TreePath(String(_1a),_1e);
if(_19.compare(_1a)>0){
var tmp=_19;
_19=_1a;
_1a=tmp;
}
var _1f=_19._str,_20=_1a._str;
_1b(_1f);
var p=_19;
while((p=p.next())){
if(p._str==_20){
break;
}
_1b(p._str);
}
_1b(_20);
}});
});
