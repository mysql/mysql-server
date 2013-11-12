//>>built
define("dojox/grid/enhanced/plugins/filter/FilterDefDialog",["dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojo/_base/lang","dojo/_base/event","dojo/_base/html","dojo/_base/sniff","dojo/cache","dojo/keys","dojo/string","dojo/window","dojo/date/locale","./FilterBuilder","../Dialog","dijit/form/ComboBox","dijit/form/TextBox","dijit/form/NumberTextBox","dijit/form/DateTextBox","dijit/form/TimeTextBox","dijit/form/Button","dijit/layout/AccordionContainer","dijit/layout/ContentPane","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/focus","dojox/html/metrics","dijit/a11y","dijit/Tooltip","dijit/form/Select","dijit/form/RadioButton","dojox/html/ellipsis","../../../cells/dijit"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b,_1c){
var _1d={relSelect:60,accordionTitle:70,removeCBoxBtn:-1,colSelect:90,condSelect:95,valueBox:10,addCBoxBtn:20,filterBtn:30,clearBtn:40,cancelBtn:50};
var _1e=_1("dojox.grid.enhanced.plugins.filter.AccordionContainer",_15,{nls:null,addChild:function(_1f,_20){
var _21=arguments[0]=_1f._pane=new _16({content:_1f});
this.inherited(arguments);
this._modifyChild(_21);
},removeChild:function(_22){
var _23=_22,_24=false;
if(_22._pane){
_24=true;
_23=arguments[0]=_22._pane;
}
this.inherited(arguments);
if(_24){
this._hackHeight(false,this._titleHeight);
var _25=this.getChildren();
if(_25.length===1){
_6.style(_25[0]._removeCBoxBtn.domNode,"display","none");
}
}
_23.destroyRecursive();
},selectChild:function(_26){
if(_26._pane){
arguments[0]=_26._pane;
}
this.inherited(arguments);
},resize:function(){
this.inherited(arguments);
_2.forEach(this.getChildren(),this._setupTitleDom);
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(parseInt(_7("ie"),10)==7){
_2.some(this._connects,function(_27){
if(_27[0][1]=="onresize"){
this.disconnect(_27);
return true;
}
},this);
}
_2.forEach(this.getChildren(),function(_28){
this._modifyChild(_28,true);
},this);
},_onKeyPress:function(e,_29){
if(this.disabled||e.altKey||!(_29||e.ctrlKey)){
return;
}
var k=_9,c=e.charOrCode,ltr=_6._isBodyLtr(),_2a=null;
if((_29&&c==k.UP_ARROW)||(e.ctrlKey&&c==k.PAGE_UP)){
_2a=false;
}else{
if((_29&&c==k.DOWN_ARROW)||(e.ctrlKey&&(c==k.PAGE_DOWN||c==k.TAB))){
_2a=true;
}else{
if(c==(ltr?k.LEFT_ARROW:k.RIGHT_ARROW)){
_2a=this._focusOnRemoveBtn?null:false;
this._focusOnRemoveBtn=!this._focusOnRemoveBtn;
}else{
if(c==(ltr?k.RIGHT_ARROW:k.LEFT_ARROW)){
_2a=this._focusOnRemoveBtn?true:null;
this._focusOnRemoveBtn=!this._focusOnRemoveBtn;
}else{
return;
}
}
}
}
if(_2a!==null){
this._adjacent(_2a)._buttonWidget._onTitleClick();
}
_5.stop(e);
_b.scrollIntoView(this.selectedChildWidget._buttonWidget.domNode.parentNode);
if(_7("ie")){
this.selectedChildWidget._removeCBoxBtn.focusNode.setAttribute("tabIndex",this._focusOnRemoveBtn?_1d.accordionTitle:-1);
}
_1a.focus(this.selectedChildWidget[this._focusOnRemoveBtn?"_removeCBoxBtn":"_buttonWidget"].focusNode);
},_modifyChild:function(_2b,_2c){
if(!_2b||!this._started){
return;
}
_6.style(_2b.domNode,"overflow","hidden");
_2b._buttonWidget.connect(_2b._buttonWidget,"_setSelectedAttr",function(){
this.focusNode.setAttribute("tabIndex",this.selected?_1d.accordionTitle:"-1");
});
var _2d=this;
_2b._buttonWidget.connect(_2b._buttonWidget.domNode,"onclick",function(){
_2d._focusOnRemoveBtn=false;
});
(_2b._removeCBoxBtn=new _14({label:this.nls.removeRuleButton,showLabel:false,iconClass:"dojoxGridFCBoxRemoveCBoxBtnIcon",tabIndex:_1d.removeCBoxBtn,onClick:_4.hitch(_2b.content,"onRemove"),onKeyPress:function(e){
_2d._onKeyPress(e,_2b._buttonWidget.contentWidget);
}})).placeAt(_2b._buttonWidget.domNode);
var i,_2e=this.getChildren();
if(_2e.length===1){
_2b._buttonWidget.set("selected",true);
_6.style(_2b._removeCBoxBtn.domNode,"display","none");
}else{
for(i=0;i<_2e.length;++i){
if(_2e[i]._removeCBoxBtn){
_6.style(_2e[i]._removeCBoxBtn.domNode,"display","");
}
}
}
this._setupTitleDom(_2b);
if(!this._titleHeight){
for(i=0;i<_2e.length;++i){
if(_2e[i]!=this.selectedChildWidget){
this._titleHeight=_6.marginBox(_2e[i]._buttonWidget.domNode.parentNode).h;
break;
}
}
}
if(!_2c){
this._hackHeight(true,this._titleHeight);
}
},_hackHeight:function(_2f,_30){
var _31=this.getChildren(),dn=this.domNode,h=_6.style(dn,"height");
if(!_2f){
dn.style.height=(h-_30)+"px";
}else{
if(_31.length>1){
dn.style.height=(h+_30)+"px";
}else{
return;
}
}
this.resize();
},_setupTitleDom:function(_32){
var w=_6.contentBox(_32._buttonWidget.titleNode).w;
if(_7("ie")<8){
w-=8;
}
_6.style(_32._buttonWidget.titleTextNode,"width",w+"px");
}});
var _33=_1("dojox.grid.enhanced.plugins.filter.FilterDefPane",[_17,_18,_19],{templateString:_8("dojox.grid","enhanced/templates/FilterDefPane.html"),widgetsInTemplate:true,dlg:null,postMixInProperties:function(){
this.plugin=this.dlg.plugin;
var nls=this.plugin.nls;
this._addRuleBtnLabel=nls.addRuleButton;
this._cancelBtnLabel=nls.cancelButton;
this._clearBtnLabel=nls.clearButton;
this._filterBtnLabel=nls.filterButton;
this._relAll=nls.relationAll;
this._relAny=nls.relationAny;
this._relMsgFront=nls.relationMsgFront;
this._relMsgTail=nls.relationMsgTail;
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onkeypress","_onKey");
(this.cboxContainer=new _1e({nls:this.plugin.nls})).placeAt(this.criteriaPane);
this._relSelect.set("tabIndex",_1d.relSelect);
this._addCBoxBtn.set("tabIndex",_1d.addCBoxBtn);
this._cancelBtn.set("tabIndex",_1d.cancelBtn);
this._clearFilterBtn.set("tabIndex",_1d.clearBtn);
this._filterBtn.set("tabIndex",_1d.filterBtn);
var nls=this.plugin.nls;
this._relSelect.domNode.setAttribute("aria-label",nls.waiRelAll);
this._addCBoxBtn.domNode.setAttribute("aria-label",nls.waiAddRuleButton);
this._cancelBtn.domNode.setAttribute("aria-label",nls.waiCancelButton);
this._clearFilterBtn.domNode.setAttribute("aria-label",nls.waiClearButton);
this._filterBtn.domNode.setAttribute("aria-label",nls.waiFilterButton);
this._relSelect.set("value",this.dlg._relOpCls==="logicall"?"0":"1");
},uninitialize:function(){
this.cboxContainer.destroyRecursive();
this.plugin=null;
this.dlg=null;
},_onRelSelectChange:function(val){
this.dlg._relOpCls=val=="0"?"logicall":"logicany";
this._relSelect.domNode.setAttribute("aria-label",this.plugin.nls[val=="0"?"waiRelAll":"waiRelAny"]);
},_onAddCBox:function(){
this.dlg.addCriteriaBoxes(1);
},_onCancel:function(){
this.dlg.onCancel();
},_onClearFilter:function(){
this.dlg.onClearFilter();
},_onFilter:function(){
this.dlg.onFilter();
},_onKey:function(e){
if(e.keyCode==_9.ENTER){
this.dlg.onFilter();
}
}});
var _34=_1("dojox.grid.enhanced.plugins.filter.CriteriaBox",[_17,_18,_19],{templateString:_8("dojox.grid","enhanced/templates/CriteriaBox.html"),widgetsInTemplate:true,dlg:null,postMixInProperties:function(){
this.plugin=this.dlg.plugin;
this._curValueBox=null;
var nls=this.plugin.nls;
this._colSelectLabel=nls.columnSelectLabel;
this._condSelectLabel=nls.conditionSelectLabel;
this._valueBoxLabel=nls.valueBoxLabel;
this._anyColumnOption=nls.anyColumnOption;
},postCreate:function(){
var dlg=this.dlg,g=this.plugin.grid;
this._colSelect.set("tabIndex",_1d.colSelect);
this._colOptions=this._getColumnOptions();
this._colSelect.addOption([{label:this.plugin.nls.anyColumnOption,value:"anycolumn",selected:dlg.curColIdx<0},{value:""}].concat(this._colOptions));
this._condSelect.set("tabIndex",_1d.condSelect);
this._condSelect.addOption(this._getUsableConditions(dlg.getColumnType(dlg.curColIdx)));
this._showSelectOrLabel(this._condSelect,this._condSelectAlt);
this.connect(g.layout,"moveColumn","onMoveColumn");
},_getColumnOptions:function(){
var _35=this.dlg.curColIdx>=0?String(this.dlg.curColIdx):"anycolumn";
return _2.map(_2.filter(this.plugin.grid.layout.cells,function(_36){
return !(_36.filterable===false||_36.hidden);
}),function(_37){
return {label:_37.name||_37.field,value:String(_37.index),selected:_35==String(_37.index)};
});
},onMoveColumn:function(){
var tmp=this._onChangeColumn;
this._onChangeColumn=function(){
};
var _38=this._colSelect.get("selectedOptions");
this._colSelect.removeOption(this._colOptions);
this._colOptions=this._getColumnOptions();
this._colSelect.addOption(this._colOptions);
var i=0;
for(;i<this._colOptions.length;++i){
if(this._colOptions[i].label==_38.label){
break;
}
}
if(i<this._colOptions.length){
this._colSelect.set("value",this._colOptions[i].value);
}
var _39=this;
setTimeout(function(){
_39._onChangeColumn=tmp;
},0);
},onRemove:function(){
this.dlg.removeCriteriaBoxes(this);
},uninitialize:function(){
if(this._curValueBox){
this._curValueBox.destroyRecursive();
this._curValueBox=null;
}
this.plugin=null;
this.dlg=null;
},_showSelectOrLabel:function(sel,alt){
var _3a=sel.getOptions();
if(_3a.length==1){
alt.innerHTML=_3a[0].label;
_6.style(sel.domNode,"display","none");
_6.style(alt,"display","");
}else{
_6.style(sel.domNode,"display","");
_6.style(alt,"display","none");
}
},_onChangeColumn:function(val){
this._checkValidCriteria();
var _3b=this.dlg.getColumnType(val);
this._setConditionsByType(_3b);
this._setValueBoxByType(_3b);
this._updateValueBox();
},_onChangeCondition:function(val){
this._checkValidCriteria();
var f=(val=="range");
if(f^this._isRange){
this._isRange=f;
this._setValueBoxByType(this.dlg.getColumnType(this._colSelect.get("value")));
}
this._updateValueBox();
},_updateValueBox:function(_3c){
this._curValueBox.set("disabled",this._condSelect.get("value")=="isempty");
},_checkValidCriteria:function(){
setTimeout(_4.hitch(this,function(){
this.updateRuleTitle();
this.dlg._updatePane();
}),0);
},_createValueBox:function(cls,arg){
var _3d=_4.hitch(arg.cbox,"_checkValidCriteria");
return new cls(_4.mixin(arg,{tabIndex:_1d.valueBox,onKeyPress:_3d,onChange:_3d,"class":"dojoxGridFCBoxValueBox"}));
},_createRangeBox:function(cls,arg){
var _3e=_4.hitch(arg.cbox,"_checkValidCriteria");
_4.mixin(arg,{tabIndex:_1d.valueBox,onKeyPress:_3e,onChange:_3e});
var div=_6.create("div",{"class":"dojoxGridFCBoxValueBox"}),_3f=new cls(arg),txt=_6.create("span",{"class":"dojoxGridFCBoxRangeValueTxt","innerHTML":this.plugin.nls.rangeTo}),end=new cls(arg);
_6.addClass(_3f.domNode,"dojoxGridFCBoxStartValue");
_6.addClass(end.domNode,"dojoxGridFCBoxEndValue");
div.appendChild(_3f.domNode);
div.appendChild(txt);
div.appendChild(end.domNode);
div.domNode=div;
div.set=function(_40,_41){
if(_4.isObject(_41)){
_3f.set("value",_41.start);
end.set("value",_41.end);
}
};
div.get=function(){
var s=_3f.get("value"),e=end.get("value");
return s&&e?{start:s,end:e}:"";
};
return div;
},changeCurrentColumn:function(_42){
var _43=this.dlg.curColIdx;
this._colSelect.removeOption(this._colOptions);
this._colOptions=this._getColumnOptions();
this._colSelect.addOption(this._colOptions);
this._colSelect.set("value",_43>=0?String(_43):"anycolumn");
this.updateRuleTitle(true);
},curColumn:function(){
return this._colSelect.getOptions(this._colSelect.get("value")).label;
},curCondition:function(){
return this._condSelect.getOptions(this._condSelect.get("value")).label;
},curValue:function(){
var _44=this._condSelect.get("value");
if(_44=="isempty"){
return "";
}
return this._curValueBox?this._curValueBox.get("value"):"";
},save:function(){
if(this.isEmpty()){
return null;
}
var _45=this._colSelect.get("value"),_46=this.dlg.getColumnType(_45),_47=this.curValue(),_48=this._condSelect.get("value");
return {"column":_45,"condition":_48,"value":_47,"formattedVal":this.formatValue(_46,_48,_47),"type":_46,"colTxt":this.curColumn(),"condTxt":this.curCondition()};
},load:function(obj){
var tmp=[this._onChangeColumn,this._onChangeCondition];
this._onChangeColumn=this._onChangeCondition=function(){
};
if(obj.column){
this._colSelect.set("value",obj.column);
}
if(obj.condition){
this._condSelect.set("value",obj.condition);
}
if(obj.type){
this._setValueBoxByType(obj.type);
}else{
obj.type=this.dlg.getColumnType(this._colSelect.get("value"));
}
var _49=obj.value||"";
if(_49||(obj.type!="date"&&obj.type!="time")){
this._curValueBox.set("value",_49);
}
this._updateValueBox();
setTimeout(_4.hitch(this,function(){
this._onChangeColumn=tmp[0];
this._onChangeCondition=tmp[1];
}),0);
},getExpr:function(){
if(this.isEmpty()){
return null;
}
var _4a=this._colSelect.get("value");
return this.dlg.getExprForCriteria({"type":this.dlg.getColumnType(_4a),"column":_4a,"condition":this._condSelect.get("value"),"value":this.curValue()});
},isEmpty:function(){
var _4b=this._condSelect.get("value");
if(_4b=="isempty"){
return false;
}
var v=this.curValue();
return v===""||v===null||typeof v=="undefined"||(typeof v=="number"&&isNaN(v));
},updateRuleTitle:function(_4c){
var _4d=this._pane._buttonWidget.titleTextNode;
var _4e=["<div class='dojoxEllipsis'>"];
if(_4c||this.isEmpty()){
_4d.title=_a.substitute(this.plugin.nls.ruleTitleTemplate,[this._ruleIndex||1]);
_4e.push(_4d.title);
}else{
var _4f=this.dlg.getColumnType(this._colSelect.get("value"));
var _50=this.curColumn();
var _51=this.curCondition();
var _52=this.formatValue(_4f,this._condSelect.get("value"),this.curValue());
_4e.push(_50,"&nbsp;<span class='dojoxGridRuleTitleCondition'>",_51,"</span>&nbsp;",_52);
_4d.title=[_50," ",_51," ",_52].join("");
}
_4d.innerHTML=_4e.join("");
if(_7("mozilla")){
var tt=_6.create("div",{"style":"width: 100%; height: 100%; position: absolute; top: 0; left: 0; z-index: 9999;"},_4d);
tt.title=_4d.title;
}
},updateRuleIndex:function(_53){
if(this._ruleIndex!=_53){
this._ruleIndex=_53;
if(this.isEmpty()){
this.updateRuleTitle();
}
}
},setAriaInfo:function(idx){
var dss=_a.substitute,nls=this.plugin.nls;
this._colSelect.domNode.setAttribute("aria-label",dss(nls.waiColumnSelectTemplate,[idx]));
this._condSelect.domNode.setAttribute("aria-label",dss(nls.waiConditionSelectTemplate,[idx]));
this._pane._removeCBoxBtn.domNode.setAttribute("aria-label",dss(nls.waiRemoveRuleButtonTemplate,[idx]));
this._index=idx;
},_getUsableConditions:function(_54){
var _55=_4.clone(this.dlg._dataTypeMap[_54].conditions);
var _56=(this.plugin.args.disabledConditions||{})[_54];
var _57=parseInt(this._colSelect.get("value"),10);
var _58=isNaN(_57)?(this.plugin.args.disabledConditions||{})["anycolumn"]:this.plugin.grid.layout.cells[_57].disabledConditions;
if(!_4.isArray(_56)){
_56=[];
}
if(!_4.isArray(_58)){
_58=[];
}
var arr=_56.concat(_58);
if(arr.length){
var _59={};
_2.forEach(arr,function(c){
if(_4.isString(c)){
_59[c.toLowerCase()]=true;
}
});
return _2.filter(_55,function(_5a){
return !(_5a.value in _59);
});
}
return _55;
},_setConditionsByType:function(_5b){
var _5c=this._condSelect;
_5c.removeOption(_5c.options);
_5c.addOption(this._getUsableConditions(_5b));
this._showSelectOrLabel(this._condSelect,this._condSelectAlt);
},_setValueBoxByType:function(_5d){
if(this._curValueBox){
this.valueNode.removeChild(this._curValueBox.domNode);
try{
this._curValueBox.destroyRecursive();
}
catch(e){
}
delete this._curValueBox;
}
var _5e=this.dlg._dataTypeMap[_5d].valueBoxCls[this._getValueBoxClsInfo(this._colSelect.get("value"),_5d)],_5f=this._getValueBoxArgByType(_5d);
this._curValueBox=this[this._isRange?"_createRangeBox":"_createValueBox"](_5e,_5f);
this.valueNode.appendChild(this._curValueBox.domNode);
this._curValueBox.domNode.setAttribute("aria-label",_a.substitute(this.plugin.nls.waiValueBoxTemplate,[this._index]));
this.dlg.onRendered(this);
},_getValueBoxArgByType:function(_60){
var g=this.plugin.grid,_61=g.layout.cells[parseInt(this._colSelect.get("value"),10)],res={cbox:this};
if(_60=="string"){
if(_61&&(_61.suggestion||_61.autoComplete)){
_6.mixin(res,{store:g.store,searchAttr:_61.field||_61.name,fetchProperties:{sort:[{"attribute":_61.field||_61.name}],query:g.query,queryOptions:g.queryOptions}});
}
}else{
if(_60=="boolean"){
_6.mixin(res,this.dlg.builder.defaultArgs["boolean"]);
}
}
if(_61&&_61.dataTypeArgs){
_6.mixin(res,_61.dataTypeArgs);
}
return res;
},formatValue:function(_62,_63,v){
if(_63=="isempty"){
return "";
}
if(_62=="date"||_62=="time"){
var opt={selector:_62},fmt=_c.format;
if(_63=="range"){
return _a.substitute(this.plugin.nls.rangeTemplate,[fmt(v.start,opt),fmt(v.end,opt)]);
}
return fmt(v,opt);
}else{
if(_62=="boolean"){
return v?this._curValueBox._lblTrue:this._curValueBox._lblFalse;
}
}
return v;
},_getValueBoxClsInfo:function(_64,_65){
var _66=this.plugin.grid.layout.cells[parseInt(_64,10)];
if(_65=="string"){
return (_66&&(_66.suggestion||_66.autoComplete))?"ac":"dft";
}
return "dft";
}});
var _67=_1("dojox.grid.enhanced.plugins.filter.UniqueComboBox",_f,{_openResultList:function(_68){
var _69={},s=this.store,_6a=this.searchAttr;
arguments[0]=_2.filter(_68,function(_6b){
var key=s.getValue(_6b,_6a),_6c=_69[key];
_69[key]=true;
return !_6c;
});
this.inherited(arguments);
},_onKey:function(evt){
if(evt.charOrCode===_9.ENTER&&this._opened){
_5.stop(evt);
}
this.inherited(arguments);
}});
var _6d=_1("dojox.grid.enhanced.plugins.filter.BooleanValueBox",[_17,_18,_19],{templateString:_8("dojox.grid","enhanced/templates/FilterBoolValueBox.html"),widgetsInTemplate:true,constructor:function(_6e){
var nls=_6e.cbox.plugin.nls;
this._baseId=_6e.cbox.id;
this._lblTrue=_6e.trueLabel||nls.trueLabel||"true";
this._lblFalse=_6e.falseLabel||nls.falseLabel||"false";
this.args=_6e;
},postCreate:function(){
this.onChange();
},onChange:function(){
},get:function(_6f){
return this.rbTrue.get("checked");
},set:function(_70,v){
this.inherited(arguments);
if(_70=="value"){
this.rbTrue.set("checked",!!v);
this.rbFalse.set("checked",!v);
}
}});
var _71=_1("dojox.grid.enhanced.plugins.filter.FilterDefDialog",null,{curColIdx:-1,_relOpCls:"logicall",_savedCriterias:null,plugin:null,constructor:function(_72){
var _73=this.plugin=_72.plugin;
this.builder=new _d();
this._setupData();
this._cboxes=[];
this.defaultType=_73.args.defaultType||"string";
(this.filterDefPane=new _33({"dlg":this})).startup();
(this._defPane=new _e({"refNode":this.plugin.grid.domNode,"title":_73.nls.filterDefDialogTitle,"class":"dojoxGridFDTitlePane","iconClass":"dojoxGridFDPaneIcon","content":this.filterDefPane})).startup();
this._defPane.connect(_73.grid.layer("filter"),"filterDef",_4.hitch(this,"_onSetFilter"));
_73.grid.setFilter=_4.hitch(this,"setFilter");
_73.grid.getFilter=_4.hitch(this,"getFilter");
_73.grid.getFilterRelation=_4.hitch(this,function(){
return this._relOpCls;
});
_73.connect(_73.grid.layout,"moveColumn",_4.hitch(this,"onMoveColumn"));
},onMoveColumn:function(_74,_75,_76,_77,_78){
if(this._savedCriterias&&_76!=_77){
if(_78){
--_77;
}
var min=_76<_77?_76:_77;
var max=_76<_77?_77:_76;
var dir=_77>min?1:-1;
_2.forEach(this._savedCriterias,function(sc){
var idx=parseInt(sc.column,10);
if(!isNaN(idx)&&idx>=min&&idx<=max){
sc.column=String(idx==_76?idx+(max-min)*dir:idx-dir);
}
});
}
},destroy:function(){
this._defPane.destroyRecursive();
this._defPane=null;
this.filterDefPane=null;
this.builder=null;
this._dataTypeMap=null;
this._cboxes=null;
var g=this.plugin.grid;
g.setFilter=null;
g.getFilter=null;
g.getFilterRelation=null;
this.plugin=null;
},_setupData:function(){
var nls=this.plugin.nls;
this._dataTypeMap={"number":{valueBoxCls:{dft:_11},conditions:[{label:nls.conditionEqual,value:"equalto",selected:true},{label:nls.conditionNotEqual,value:"notequalto"},{label:nls.conditionLess,value:"lessthan"},{label:nls.conditionLessEqual,value:"lessthanorequalto"},{label:nls.conditionLarger,value:"largerthan"},{label:nls.conditionLargerEqual,value:"largerthanorequalto"},{label:nls.conditionIsEmpty,value:"isempty"}]},"string":{valueBoxCls:{dft:_10,ac:_67},conditions:[{label:nls.conditionContains,value:"contains",selected:true},{label:nls.conditionIs,value:"equalto"},{label:nls.conditionStartsWith,value:"startswith"},{label:nls.conditionEndWith,value:"endswith"},{label:nls.conditionNotContain,value:"notcontains"},{label:nls.conditionIsNot,value:"notequalto"},{label:nls.conditionNotStartWith,value:"notstartswith"},{label:nls.conditionNotEndWith,value:"notendswith"},{label:nls.conditionIsEmpty,value:"isempty"}]},"date":{valueBoxCls:{dft:_12},conditions:[{label:nls.conditionIs,value:"equalto",selected:true},{label:nls.conditionBefore,value:"lessthan"},{label:nls.conditionAfter,value:"largerthan"},{label:nls.conditionRange,value:"range"},{label:nls.conditionIsEmpty,value:"isempty"}]},"time":{valueBoxCls:{dft:_13},conditions:[{label:nls.conditionIs,value:"equalto",selected:true},{label:nls.conditionBefore,value:"lessthan"},{label:nls.conditionAfter,value:"largerthan"},{label:nls.conditionRange,value:"range"},{label:nls.conditionIsEmpty,value:"isempty"}]},"boolean":{valueBoxCls:{dft:_6d},conditions:[{label:nls.conditionIs,value:"equalto",selected:true},{label:nls.conditionIsEmpty,value:"isempty"}]}};
},setFilter:function(_79,_7a){
_79=_79||[];
if(!_4.isArray(_79)){
_79=[_79];
}
var _7b=function(){
if(_79.length){
this._savedCriterias=_2.map(_79,function(_7c){
var _7d=_7c.type||this.defaultType;
return {"type":_7d,"column":String(_7c.column),"condition":_7c.condition,"value":_7c.value,"colTxt":this.getColumnLabelByValue(String(_7c.column)),"condTxt":this.getConditionLabelByValue(_7d,_7c.condition),"formattedVal":_7c.formattedVal||_7c.value};
},this);
this._criteriasChanged=true;
if(_7a==="logicall"||_7a==="logicany"){
this._relOpCls=_7a;
}
var _7e=_2.map(_79,this.getExprForCriteria,this);
_7e=this.builder.buildExpression(_7e.length==1?_7e[0]:{"op":this._relOpCls,"data":_7e});
this.plugin.grid.layer("filter").filterDef(_7e);
this.plugin.filterBar.toggleClearFilterBtn(false);
}
this._closeDlgAndUpdateGrid();
};
if(this._savedCriterias){
this._clearWithoutRefresh=true;
var _7f=_3.connect(this,"clearFilter",this,function(){
_3.disconnect(_7f);
this._clearWithoutRefresh=false;
_7b.apply(this);
});
this.onClearFilter();
}else{
_7b.apply(this);
}
},getFilter:function(){
return _4.clone(this._savedCriterias)||[];
},getColumnLabelByValue:function(v){
var nls=this.plugin.nls;
if(v.toLowerCase()=="anycolumn"){
return nls["anyColumnOption"];
}else{
var _80=this.plugin.grid.layout.cells[parseInt(v,10)];
return _80?(_80.name||_80.field):"";
}
},getConditionLabelByValue:function(_81,c){
var _82=this._dataTypeMap[_81].conditions;
for(var i=_82.length-1;i>=0;--i){
var _83=_82[i];
if(_83.value==c.toLowerCase()){
return _83.label;
}
}
return "";
},addCriteriaBoxes:function(cnt){
if(typeof cnt!="number"||cnt<=0){
return;
}
var cbs=this._cboxes,cc=this.filterDefPane.cboxContainer,_84=this.plugin.args.ruleCount,len=cbs.length,_85;
if(_84>0&&len+cnt>_84){
cnt=_84-len;
}
for(;cnt>0;--cnt){
_85=new _34({dlg:this});
cbs.push(_85);
cc.addChild(_85);
}
cc.startup();
this._updatePane();
this._updateCBoxTitles();
cc.selectChild(cbs[cbs.length-1]);
this.filterDefPane.criteriaPane.scrollTop=1000000;
if(cbs.length===4){
if(_7("ie")<=6&&!this.__alreadyResizedForIE6){
var _86=_6.position(cc.domNode);
_86.w-=_1b.getScrollbar().w;
cc.resize(_86);
this.__alreadyResizedForIE6=true;
}else{
cc.resize();
}
}
},removeCriteriaBoxes:function(cnt,_87){
var cbs=this._cboxes,cc=this.filterDefPane.cboxContainer,len=cbs.length,_88=len-cnt,end=len-1,_89,_8a=_2.indexOf(cbs,cc.selectedChildWidget.content);
if(_4.isArray(cnt)){
var i,_8b=cnt;
_8b.sort();
cnt=_8b.length;
for(i=len-1;i>=0&&_2.indexOf(_8b,i)>=0;--i){
}
if(i>=0){
if(i!=_8a){
cc.selectChild(cbs[i]);
}
for(i=cnt-1;i>=0;--i){
if(_8b[i]>=0&&_8b[i]<len){
cc.removeChild(cbs[_8b[i]]);
cbs.splice(_8b[i],1);
}
}
}
_88=cbs.length;
}else{
if(_87===true){
if(cnt>=0&&cnt<len){
_88=end=cnt;
cnt=1;
}else{
return;
}
}else{
if(cnt instanceof _34){
_89=cnt;
cnt=1;
_88=end=_2.indexOf(cbs,_89);
}else{
if(typeof cnt!="number"||cnt<=0){
return;
}else{
if(cnt>=len){
cnt=end;
_88=1;
}
}
}
}
if(end<_88){
return;
}
if(_8a>=_88&&_8a<=end){
cc.selectChild(cbs[_88?_88-1:end+1]);
}
for(;end>=_88;--end){
cc.removeChild(cbs[end]);
}
cbs.splice(_88,cnt);
}
this._updatePane();
this._updateCBoxTitles();
if(cbs.length===3){
cc.resize();
}
},getCriteria:function(idx){
if(typeof idx!="number"){
return this._savedCriterias?this._savedCriterias.length:0;
}
if(this._savedCriterias&&this._savedCriterias[idx]){
return _4.mixin({relation:this._relOpCls=="logicall"?this.plugin.nls.and:this.plugin.nls.or},this._savedCriterias[idx]);
}
return null;
},getExprForCriteria:function(_8c){
if(_8c.column=="anycolumn"){
var _8d=_2.filter(this.plugin.grid.layout.cells,function(_8e){
return !(_8e.filterable===false||_8e.hidden);
});
return {"op":"logicany","data":_2.map(_8d,function(_8f){
return this.getExprForColumn(_8c.value,_8f.index,_8c.type,_8c.condition);
},this)};
}else{
return this.getExprForColumn(_8c.value,_8c.column,_8c.type,_8c.condition);
}
},getExprForColumn:function(_90,_91,_92,_93){
_91=parseInt(_91,10);
var _94=this.plugin.grid.layout.cells[_91],_95=_94.field||_94.name,obj={"datatype":_92||this.getColumnType(_91),"args":_94.dataTypeArgs,"isColumn":true},_96=[_4.mixin({"data":this.plugin.args.isServerSide?_95:_94},obj)];
obj.isColumn=false;
if(_93=="range"){
_96.push(_4.mixin({"data":_90.start},obj),_4.mixin({"data":_90.end},obj));
}else{
if(_93!="isempty"){
_96.push(_4.mixin({"data":_90},obj));
}
}
return {"op":_93,"data":_96};
},getColumnType:function(_97){
var _98=this.plugin.grid.layout.cells[parseInt(_97,10)];
if(!_98||!_98.datatype){
return this.defaultType;
}
var _99=String(_98.datatype).toLowerCase();
return this._dataTypeMap[_99]?_99:this.defaultType;
},clearFilter:function(_9a){
if(!this._savedCriterias){
return;
}
this._savedCriterias=null;
this.plugin.grid.layer("filter").filterDef(null);
try{
this.plugin.filterBar.toggleClearFilterBtn(true);
this.filterDefPane._clearFilterBtn.set("disabled",true);
this.removeCriteriaBoxes(this._cboxes.length-1);
this._cboxes[0].load({});
}
catch(e){
}
if(_9a){
this.closeDialog();
}else{
this._closeDlgAndUpdateGrid();
}
},showDialog:function(_9b){
this._defPane.show();
this.plugin.filterStatusTip.closeDialog();
this._prepareDialog(_9b);
},closeDialog:function(){
if(this._defPane.open){
this._defPane.hide();
}
},onFilter:function(e){
if(this.canFilter()){
this._defineFilter();
this._closeDlgAndUpdateGrid();
this.plugin.filterBar.toggleClearFilterBtn(false);
}
},onClearFilter:function(e){
if(this._savedCriterias){
if(this._savedCriterias.length>=this.plugin.ruleCountToConfirmClearFilter){
this.plugin.clearFilterDialog.show();
}else{
this.clearFilter(this._clearWithoutRefresh);
}
}
},onCancel:function(e){
var sc=this._savedCriterias;
var cbs=this._cboxes;
if(sc){
this.addCriteriaBoxes(sc.length-cbs.length);
this.removeCriteriaBoxes(cbs.length-sc.length);
_2.forEach(sc,function(c,i){
cbs[i].load(c);
});
}else{
this.removeCriteriaBoxes(cbs.length-1);
cbs[0].load({});
}
this.closeDialog();
},onRendered:function(_9c){
if(!_7("ff")){
var _9d=_1c._getTabNavigable(_6.byId(_9c.domNode));
_1a.focus(_9d.lowest||_9d.first);
}else{
var dp=this._defPane;
dp._getFocusItems(dp.domNode);
_1a.focus(dp._firstFocusItem);
}
},_onSetFilter:function(_9e){
if(_9e===null&&this._savedCriterias){
this.clearFilter();
}
},_prepareDialog:function(_9f){
var sc=this._savedCriterias,cbs=this._cboxes,i,_a0;
this.curColIdx=_9f;
if(!sc){
if(cbs.length===0){
this.addCriteriaBoxes(1);
}else{
for(i=0;(_a0=cbs[i]);++i){
_a0.changeCurrentColumn();
}
}
}else{
if(this._criteriasChanged){
this.filterDefPane._relSelect.set("value",this._relOpCls==="logicall"?"0":"1");
this._criteriasChanged=false;
var _a1=sc.length>cbs.length?sc.length-cbs.length:0;
this.addCriteriaBoxes(_a1);
this.removeCriteriaBoxes(cbs.length-sc.length);
this.filterDefPane._clearFilterBtn.set("disabled",false);
for(i=0;i<cbs.length-_a1;++i){
cbs[i].load(sc[i]);
}
if(_a1>0){
var _a2=[],_a3=_3.connect(this,"onRendered",function(_a4){
var i=_2.indexOf(cbs,_a4);
if(!_a2[i]){
_a2[i]=true;
if(--_a1===0){
_3.disconnect(_a3);
}
_a4.load(sc[i]);
}
});
}
}
}
this.filterDefPane.cboxContainer.resize();
},_defineFilter:function(){
var cbs=this._cboxes,_a5=function(_a6){
return _2.filter(_2.map(cbs,function(_a7){
return _a7[_a6]();
}),function(_a8){
return !!_a8;
});
},_a9=_a5("getExpr");
this._savedCriterias=_a5("save");
_a9=_a9.length==1?_a9[0]:{"op":this._relOpCls,"data":_a9};
_a9=this.builder.buildExpression(_a9);
this.plugin.grid.layer("filter").filterDef(_a9);
this.filterDefPane._clearFilterBtn.set("disabled",false);
},_updateCBoxTitles:function(){
for(var cbs=this._cboxes,i=cbs.length;i>0;--i){
cbs[i-1].updateRuleIndex(i);
cbs[i-1].setAriaInfo(i);
}
},_updatePane:function(){
var cbs=this._cboxes,_aa=this.filterDefPane;
_aa._addCBoxBtn.set("disabled",cbs.length==this.plugin.args.ruleCount);
_aa._filterBtn.set("disabled",!this.canFilter());
},canFilter:function(){
return _2.filter(this._cboxes,function(_ab){
return !_ab.isEmpty();
}).length>0;
},_closeDlgAndUpdateGrid:function(){
this.closeDialog();
var g=this.plugin.grid;
g.showMessage(g.loadingMessage);
setTimeout(_4.hitch(g,g._refresh),this._defPane.duration+10);
}});
return _71;
});
