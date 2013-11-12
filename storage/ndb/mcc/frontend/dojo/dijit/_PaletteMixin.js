//>>built
define("dijit/_PaletteMixin",["dojo/_base/declare","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/_base/event","dojo/keys","dojo/_base/lang","./_CssStateMixin","./focus","./typematic"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _1("dijit._PaletteMixin",[_8],{defaultTimeout:500,timeoutChangeRate:0.9,value:"",_selectedCell:-1,tabIndex:"0",cellClass:"dijitPaletteCell",dyeClass:"",summary:"",_setSummaryAttr:"paletteTableNode",_dyeFactory:function(_b){
var _c=_7.getObject(this.dyeClass);
return new _c(_b);
},_preparePalette:function(_d,_e){
this._cells=[];
var _f=this._blankGif;
this.connect(this.gridNode,"ondijitclick","_onCellClick");
for(var row=0;row<_d.length;row++){
var _10=_4.create("tr",{tabIndex:"-1"},this.gridNode);
for(var col=0;col<_d[row].length;col++){
var _11=_d[row][col];
if(_11){
var _12=this._dyeFactory(_11,row,col);
var _13=_4.create("td",{"class":this.cellClass,tabIndex:"-1",title:_e[_11],role:"gridcell"});
_12.fillCell(_13,_f);
_4.place(_13,_10);
_13.index=this._cells.length;
this._cells.push({node:_13,dye:_12});
}
}
}
this._xDim=_d[0].length;
this._yDim=_d.length;
var _14={UP_ARROW:-this._xDim,DOWN_ARROW:this._xDim,RIGHT_ARROW:this.isLeftToRight()?1:-1,LEFT_ARROW:this.isLeftToRight()?-1:1};
for(var key in _14){
this._connects.push(_a.addKeyListener(this.domNode,{charOrCode:_6[key],ctrlKey:false,altKey:false,shiftKey:false},this,function(){
var _15=_14[key];
return function(_16){
this._navigateByKey(_15,_16);
};
}(),this.timeoutChangeRate,this.defaultTimeout));
}
},postCreate:function(){
this.inherited(arguments);
this._setCurrent(this._cells[0].node);
},focus:function(){
_9.focus(this._currentFocus);
},_onCellClick:function(evt){
var _17=evt.target;
while(_17.tagName!="TD"){
if(!_17.parentNode||_17==this.gridNode){
return;
}
_17=_17.parentNode;
}
var _18=this._getDye(_17).getValue();
this._setCurrent(_17);
_9.focus(_17);
this._setValueAttr(_18,true);
_5.stop(evt);
},_setCurrent:function(_19){
if("_currentFocus" in this){
_2.set(this._currentFocus,"tabIndex","-1");
}
this._currentFocus=_19;
if(_19){
_2.set(_19,"tabIndex",this.tabIndex);
}
},_setValueAttr:function(_1a,_1b){
if(this._selectedCell>=0){
_3.remove(this._cells[this._selectedCell].node,this.cellClass+"Selected");
}
this._selectedCell=-1;
if(_1a){
for(var i=0;i<this._cells.length;i++){
if(_1a==this._cells[i].dye.getValue()){
this._selectedCell=i;
_3.add(this._cells[i].node,this.cellClass+"Selected");
break;
}
}
}
this._set("value",this._selectedCell>=0?_1a:null);
if(_1b||_1b===undefined){
this.onChange(_1a);
}
},onChange:function(){
},_navigateByKey:function(_1c,_1d){
if(_1d==-1){
return;
}
var _1e=this._currentFocus.index+_1c;
if(_1e<this._cells.length&&_1e>-1){
var _1f=this._cells[_1e].node;
this._setCurrent(_1f);
setTimeout(_7.hitch(dijit,"focus",_1f),0);
}
},_getDye:function(_20){
return this._cells[_20.index].dye;
}});
});
