//>>built
require({cache:{"url:dojox/grid/enhanced/templates/FilterDefPane.html":"<div class=\"dojoxGridFDPane\">\n\t<div class=\"dojoxGridFDPaneRelation\">${_relMsgFront}\n\t<span class=\"dojoxGridFDPaneModes\" dojoAttachPoint=\"criteriaModeNode\">\n\t\t<select dojoAttachPoint=\"_relSelect\" dojoType=\"dijit.form.Select\" dojoAttachEvent=\"onChange: _onRelSelectChange\">\n\t\t\t<option value=\"0\">${_relAll}</option>\n\t\t\t<option value=\"1\">${_relAny}</option>\n\t\t</select>\n\t</span>\n\t${_relMsgTail}\n\t</div>\n\t<div dojoAttachPoint=\"criteriaPane\" class=\"dojoxGridFDPaneRulePane\"></div>\n\t<div dojoAttachPoint=\"_addCBoxBtn\" dojoType=\"dijit.form.Button\" \n\t\tclass=\"dojoxGridFDPaneAddCBoxBtn\" iconClass=\"dojoxGridFDPaneAddCBoxBtnIcon\"\n\t\tdojoAttachEvent=\"onClick:_onAddCBox\" label=\"${_addRuleBtnLabel}\" showLabel=\"false\">\n\t</div>\n\t<div class=\"dojoxGridFDPaneBtns\" dojoAttachPoint=\"buttonsPane\">\n\t\t<span dojoAttachPoint=\"_cancelBtn\" dojoType=\"dijit.form.Button\" \n\t\t\tdojoAttachEvent=\"onClick:_onCancel\" label=\"${_cancelBtnLabel}\">\n\t\t</span>\n\t\t<span dojoAttachPoint=\"_clearFilterBtn\" dojoType=\"dijit.form.Button\" \n\t\t\tdojoAttachEvent=\"onClick:_onClearFilter\" label=\"${_clearBtnLabel}\" disabled=\"true\">\n\t\t</span>\n\t\t<span dojoAttachPoint=\"_filterBtn\" dojoType=\"dijit.form.Button\" \n\t\t\tdojoAttachEvent=\"onClick:_onFilter\" label=\"${_filterBtnLabel}\" disabled=\"true\">\n\t\t</span>\n\t</div>\n</div>\n","url:dojox/grid/enhanced/templates/CriteriaBox.html":"<div class=\"dojoxGridFCBox\">\n\t<div class=\"dojoxGridFCBoxSelCol\" dojoAttachPoint=\"selColNode\">\n\t\t<span class=\"dojoxGridFCBoxField\">${_colSelectLabel}</span>\n\t\t<select dojoAttachPoint=\"_colSelect\" dojoType=\"dijit.form.Select\" \n\t\t\tclass=\"dojoxGridFCBoxColSelect\"\n\t\t\tdojoAttachEvent=\"onChange:_onChangeColumn\">\n\t\t</select>\n\t</div>\n\t<div class=\"dojoxGridFCBoxCondition\" dojoAttachPoint=\"condNode\">\n\t\t<span class=\"dojoxGridFCBoxField\">${_condSelectLabel}</span>\n\t\t<select dojoAttachPoint=\"_condSelect\" dojoType=\"dijit.form.Select\" \n\t\t\tclass=\"dojoxGridFCBoxCondSelect\"\n\t\t\tdojoAttachEvent=\"onChange:_onChangeCondition\">\n\t\t</select>\n\t\t<div class=\"dojoxGridFCBoxCondSelectAlt\" dojoAttachPoint=\"_condSelectAlt\" style=\"display:none;\"></div>\n\t</div>\n\t<div class=\"dojoxGridFCBoxValue\" dojoAttachPoint=\"valueNode\">\n\t\t<span class=\"dojoxGridFCBoxField\">${_valueBoxLabel}</span>\n\t</div>\n</div>\n","url:dojox/grid/enhanced/templates/FilterBoolValueBox.html":"<div class=\"dojoxGridBoolValueBox\">\n\t<div class=\"dojoxGridTrueBox\">\n\t\t<input dojoType=\"dijit.form.RadioButton\" type='radio' name='a1' id='${_baseId}_rbTrue' checked=\"true\" \n\t\t\tdojoAttachPoint=\"rbTrue\" dojoAttachEvent=\"onChange: onChange\"/>\n\t\t<div class=\"dojoxGridTrueLabel\" for='${_baseId}_rbTrue'>${_lblTrue}</div>\n\t</div>\n\t<div class=\"dojoxGridFalseBox\">\n\t\t<input dojoType=\"dijit.form.RadioButton\" dojoAttachPoint=\"rbFalse\" type='radio' name='a1' id='${_baseId}_rbFalse'/>\n\t\t<div class=\"dojoxGridTrueLabel\" for='${_baseId}_rbFalse'>${_lblFalse}</div>\n\t</div>\n</div>\n"}});
define("dojox/grid/enhanced/plugins/filter/FilterDefDialog",["dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojo/_base/lang","dojo/_base/event","dojo/_base/html","dojo/_base/sniff","dojo/keys","dojo/on","dojo/string","dojo/window","dojo/date/locale","./FilterBuilder","../Dialog","dijit/form/ComboBox","dijit/form/TextBox","dijit/form/NumberTextBox","dijit/form/DateTextBox","dijit/form/TimeTextBox","dijit/form/Button","dijit/layout/AccordionContainer","dijit/layout/ContentPane","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/focus","dojox/html/metrics","dijit/a11y","dojo/text!../../templates/FilterDefPane.html","dojo/text!../../templates/CriteriaBox.html","dojo/text!../../templates/FilterBoolValueBox.html","dijit/Tooltip","dijit/form/Select","dijit/form/RadioButton","dojox/html/ellipsis","../../../cells/dijit"],function(_1,_2,_3,_4,_5,_6,_7,_8,on,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b,_1c,_1d,_1e){
var _1f={relSelect:60,accordionTitle:70,removeCBoxBtn:-1,colSelect:90,condSelect:95,valueBox:10,addCBoxBtn:20,filterBtn:30,clearBtn:40,cancelBtn:50};
var _20=_1("dojox.grid.enhanced.plugins.filter.AccordionContainer",_14,{nls:null,addChild:function(_21,_22){
var _23=arguments[0]=_21._pane=new _15({content:_21});
this.inherited(arguments);
this._modifyChild(_23);
},removeChild:function(_24){
var _25=_24,_26=false;
if(_24._pane){
_26=true;
_25=arguments[0]=_24._pane;
}
this.inherited(arguments);
if(_26){
this._hackHeight(false,this._titleHeight);
var _27=this.getChildren();
if(_27.length===1){
_6.style(_27[0]._removeCBoxBtn.domNode,"display","none");
}
}
_25.destroyRecursive();
},selectChild:function(_28){
if(_28._pane){
arguments[0]=_28._pane;
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
_2.some(this._connects,function(_29){
if((_29[0]||{})[1]=="onresize"){
this.disconnect(_29);
return true;
}
},this);
}
_2.forEach(this.getChildren(),function(_2a){
this._modifyChild(_2a,true);
},this);
},_onKeyPress:function(e,_2b){
if(this.disabled||e.altKey||!(_2b||e.ctrlKey)){
return;
}
var k=_8,c=e.charOrCode,ltr=_6._isBodyLtr(),_2c=null;
if((_2b&&c==k.UP_ARROW)||(e.ctrlKey&&c==k.PAGE_UP)){
_2c=false;
}else{
if((_2b&&c==k.DOWN_ARROW)||(e.ctrlKey&&(c==k.PAGE_DOWN||c==k.TAB))){
_2c=true;
}else{
if(c==(ltr?k.LEFT_ARROW:k.RIGHT_ARROW)){
_2c=this._focusOnRemoveBtn?null:false;
this._focusOnRemoveBtn=!this._focusOnRemoveBtn;
}else{
if(c==(ltr?k.RIGHT_ARROW:k.LEFT_ARROW)){
_2c=this._focusOnRemoveBtn?true:null;
this._focusOnRemoveBtn=!this._focusOnRemoveBtn;
}else{
return;
}
}
}
}
if(_2c!==null){
this._adjacent(_2c)._buttonWidget._onTitleClick();
}
_5.stop(e);
_a.scrollIntoView(this.selectedChildWidget._buttonWidget.domNode.parentNode);
if(_7("ie")){
this.selectedChildWidget._removeCBoxBtn.focusNode.setAttribute("tabIndex",this._focusOnRemoveBtn?_1f.accordionTitle:-1);
}
_19.focus(this.selectedChildWidget[this._focusOnRemoveBtn?"_removeCBoxBtn":"_buttonWidget"].focusNode);
},_modifyChild:function(_2d,_2e){
if(!_2d||!this._started){
return;
}
_6.style(_2d.domNode,"overflow","hidden");
_2d._buttonWidget.connect(_2d._buttonWidget,"_setSelectedAttr",function(){
this.focusNode.setAttribute("tabIndex",this.selected?_1f.accordionTitle:"-1");
});
var _2f=this;
_2d._buttonWidget.connect(_2d._buttonWidget.domNode,"onclick",function(){
_2f._focusOnRemoveBtn=false;
});
(_2d._removeCBoxBtn=new _13({label:this.nls.removeRuleButton,showLabel:false,iconClass:"dojoxGridFCBoxRemoveCBoxBtnIcon",tabIndex:_1f.removeCBoxBtn,onClick:_4.hitch(_2d.content,"onRemove"),onKeyPress:function(e){
_2f._onKeyPress(e,_2d._buttonWidget.contentWidget);
}})).placeAt(_2d._buttonWidget.domNode);
var i,_30=this.getChildren();
if(_30.length===1){
_2d._buttonWidget.set("selected",true);
_6.style(_2d._removeCBoxBtn.domNode,"display","none");
}else{
for(i=0;i<_30.length;++i){
if(_30[i]._removeCBoxBtn){
_6.style(_30[i]._removeCBoxBtn.domNode,"display","");
}
}
}
this._setupTitleDom(_2d);
if(!this._titleHeight){
for(i=0;i<_30.length;++i){
if(_30[i]!=this.selectedChildWidget){
this._titleHeight=_6.marginBox(_30[i]._buttonWidget.domNode.parentNode).h;
break;
}
}
}
if(!_2e){
this._hackHeight(true,this._titleHeight);
}
},_hackHeight:function(_31,_32){
var _33=this.getChildren(),dn=this.domNode,h=_6.style(dn,"height");
if(!_31){
dn.style.height=(h-_32)+"px";
}else{
if(_33.length>1){
dn.style.height=(h+_32)+"px";
}else{
return;
}
}
this.resize();
},_setupTitleDom:function(_34){
var w=_6.contentBox(_34._buttonWidget.titleNode).w;
if(_7("ie")<8){
w-=8;
}
_6.style(_34._buttonWidget.titleTextNode,"width",w+"px");
}});
var _35=_1("dojox.grid.enhanced.plugins.filter.FilterDefPane",[_16,_17,_18],{templateString:_1c,widgetsInTemplate:true,dlg:null,postMixInProperties:function(){
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
on(this.domNode,"keydown",_4.hitch(this,"_onKey"));
(this.cboxContainer=new _20({nls:this.plugin.nls})).placeAt(this.criteriaPane);
this._relSelect.set("tabIndex",_1f.relSelect);
this._addCBoxBtn.set("tabIndex",_1f.addCBoxBtn);
this._cancelBtn.set("tabIndex",_1f.cancelBtn);
this._clearFilterBtn.set("tabIndex",_1f.clearBtn);
this._filterBtn.set("tabIndex",_1f.filterBtn);
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
if(e.keyCode==_8.ENTER){
this.dlg.onFilter();
}
}});
var _36=_1("dojox.grid.enhanced.plugins.filter.CriteriaBox",[_16,_17,_18],{templateString:_1d,widgetsInTemplate:true,dlg:null,postMixInProperties:function(){
this.plugin=this.dlg.plugin;
this._curValueBox=null;
var nls=this.plugin.nls;
this._colSelectLabel=nls.columnSelectLabel;
this._condSelectLabel=nls.conditionSelectLabel;
this._valueBoxLabel=nls.valueBoxLabel;
this._anyColumnOption=nls.anyColumnOption;
},postCreate:function(){
var dlg=this.dlg,g=this.plugin.grid;
this._colSelect.set("tabIndex",_1f.colSelect);
this._colOptions=this._getColumnOptions();
this._colSelect.addOption([{label:this.plugin.nls.anyColumnOption,value:"anycolumn",selected:dlg.curColIdx<0},{value:""}].concat(this._colOptions));
this._condSelect.set("tabIndex",_1f.condSelect);
this._condSelect.addOption(this._getUsableConditions(dlg.getColumnType(dlg.curColIdx)));
this._showSelectOrLabel(this._condSelect,this._condSelectAlt);
this.connect(g.layout,"moveColumn","onMoveColumn");
var _37=this;
setTimeout(function(){
var _38=dlg.getColumnType(dlg.curColIdx);
_37._setValueBoxByType(_38);
},0);
},_getColumnOptions:function(){
var _39=this.dlg.curColIdx>=0?String(this.dlg.curColIdx):"anycolumn";
return _2.map(_2.filter(this.plugin.grid.layout.cells,function(_3a){
return !(_3a.filterable===false||_3a.hidden);
}),function(_3b){
return {label:_3b.name||_3b.field,value:String(_3b.index),selected:_39==String(_3b.index)};
});
},onMoveColumn:function(){
var tmp=this._onChangeColumn;
this._onChangeColumn=function(){
};
var _3c=this._colSelect.get("selectedOptions");
this._colSelect.removeOption(this._colOptions);
this._colOptions=this._getColumnOptions();
this._colSelect.addOption(this._colOptions);
var i=0;
for(;i<this._colOptions.length;++i){
if(this._colOptions[i].label==_3c.label){
break;
}
}
if(i<this._colOptions.length){
this._colSelect.set("value",this._colOptions[i].value);
}
var _3d=this;
setTimeout(function(){
_3d._onChangeColumn=tmp;
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
var _3e=sel.getOptions();
if(_3e.length==1){
alt.innerHTML=_3e[0].label;
_6.style(sel.domNode,"display","none");
_6.style(alt,"display","");
}else{
_6.style(sel.domNode,"display","");
_6.style(alt,"display","none");
}
},_onChangeColumn:function(val){
this._checkValidCriteria();
var _3f=this.dlg.getColumnType(val);
this._setConditionsByType(_3f);
this._setValueBoxByType(_3f);
this._updateValueBox();
},_onChangeCondition:function(val){
this._checkValidCriteria();
var f=(val=="range");
if(f^this._isRange){
this._isRange=f;
this._setValueBoxByType(this.dlg.getColumnType(this._colSelect.get("value")));
}
this._updateValueBox();
},_updateValueBox:function(_40){
this._curValueBox.set("disabled",this._condSelect.get("value")=="isempty");
},_checkValidCriteria:function(){
setTimeout(_4.hitch(this,function(){
this.updateRuleTitle();
this.dlg._updatePane();
}),0);
},_createValueBox:function(cls,arg){
var _41=_4.hitch(arg.cbox,"_checkValidCriteria");
return new cls(_4.mixin(arg,{tabIndex:_1f.valueBox,onKeyPress:_41,onChange:_41,"class":"dojoxGridFCBoxValueBox"}));
},_createRangeBox:function(cls,arg){
var _42=_4.hitch(arg.cbox,"_checkValidCriteria");
_4.mixin(arg,{tabIndex:_1f.valueBox,onKeyPress:_42,onChange:_42});
var div=_6.create("div",{"class":"dojoxGridFCBoxValueBox"}),_43=new cls(arg),txt=_6.create("span",{"class":"dojoxGridFCBoxRangeValueTxt","innerHTML":this.plugin.nls.rangeTo}),end=new cls(arg);
_6.addClass(_43.domNode,"dojoxGridFCBoxStartValue");
_6.addClass(end.domNode,"dojoxGridFCBoxEndValue");
div.appendChild(_43.domNode);
div.appendChild(txt);
div.appendChild(end.domNode);
div.domNode=div;
div.set=function(_44,_45){
if(_4.isObject(_45)){
_43.set("value",_45.start);
end.set("value",_45.end);
}
};
div.get=function(){
var s=_43.get("value"),e=end.get("value");
return s&&e?{start:s,end:e}:"";
};
return div;
},changeCurrentColumn:function(_46){
var _47=this.dlg.curColIdx;
this._colSelect.removeOption(this._colOptions);
this._colOptions=this._getColumnOptions();
this._colSelect.addOption(this._colOptions);
this._colSelect.set("value",_47>=0?String(_47):"anycolumn");
this.updateRuleTitle(true);
},curColumn:function(){
return this._colSelect.getOptions(this._colSelect.get("value")).label;
},curCondition:function(){
return this._condSelect.getOptions(this._condSelect.get("value")).label;
},curValue:function(){
var _48=this._condSelect.get("value");
if(_48=="isempty"){
return "";
}
return this._curValueBox?this._curValueBox.get("value"):"";
},save:function(){
if(this.isEmpty()){
return null;
}
var _49=this._colSelect.get("value"),_4a=this.dlg.getColumnType(_49),_4b=this.curValue(),_4c=this._condSelect.get("value");
return {"column":_49,"condition":_4c,"value":_4b,"formattedVal":this.formatValue(_4a,_4c,_4b),"type":_4a,"colTxt":this.curColumn(),"condTxt":this.curCondition()};
},load:function(obj){
var tmp=[this._onChangeColumn,this._onChangeCondition];
this._onChangeColumn=this._onChangeCondition=function(){
};
if(obj.column){
this._colSelect.set("value",obj.column);
}
if(obj.type){
this._setConditionsByType(obj.type);
this._setValueBoxByType(obj.type);
}else{
obj.type=this.dlg.getColumnType(this._colSelect.get("value"));
}
if(obj.condition){
this._condSelect.set("value",obj.condition);
}
var _4d=obj.value||"";
if(_4d||(obj.type!="date"&&obj.type!="time")){
this._curValueBox.set("value",_4d);
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
var _4e=this._colSelect.get("value");
return this.dlg.getExprForCriteria({"type":this.dlg.getColumnType(_4e),"column":_4e,"condition":this._condSelect.get("value"),"value":this.curValue()});
},isEmpty:function(){
var _4f=this._condSelect.get("value");
if(_4f=="isempty"){
return false;
}
var v=this.curValue();
return v===""||v===null||typeof v=="undefined"||(typeof v=="number"&&isNaN(v));
},updateRuleTitle:function(_50){
var _51=this._pane._buttonWidget.titleTextNode;
var _52=["<div class='dojoxEllipsis'>"];
if(_50||this.isEmpty()){
_51.title=_9.substitute(this.plugin.nls.ruleTitleTemplate,[this._ruleIndex||1]);
_52.push(_51.title);
}else{
var _53=this.dlg.getColumnType(this._colSelect.get("value"));
var _54=this.curColumn();
var _55=this.curCondition();
var _56=this.formatValue(_53,this._condSelect.get("value"),this.curValue());
_52.push(_54,"&nbsp;<span class='dojoxGridRuleTitleCondition'>",_55,"</span>&nbsp;",_56);
_51.title=[_54," ",_55," ",_56].join("");
}
_51.innerHTML=_52.join("");
if(_7("mozilla")){
var tt=_6.create("div",{"style":"width: 100%; height: 100%; position: absolute; top: 0; left: 0; z-index: 9999;"},_51);
tt.title=_51.title;
}
},updateRuleIndex:function(_57){
if(this._ruleIndex!=_57){
this._ruleIndex=_57;
if(this.isEmpty()){
this.updateRuleTitle();
}
}
},setAriaInfo:function(idx){
var dss=_9.substitute,nls=this.plugin.nls;
this._colSelect.domNode.setAttribute("aria-label",dss(nls.waiColumnSelectTemplate,[idx]));
this._condSelect.domNode.setAttribute("aria-label",dss(nls.waiConditionSelectTemplate,[idx]));
this._pane._removeCBoxBtn.domNode.setAttribute("aria-label",dss(nls.waiRemoveRuleButtonTemplate,[idx]));
this._index=idx;
},_getUsableConditions:function(_58){
var _59=_4.clone(this.dlg._dataTypeMap[_58].conditions);
var _5a=(this.plugin.args.disabledConditions||{})[_58];
var _5b=parseInt(this._colSelect.get("value"),10);
var _5c=isNaN(_5b)?(this.plugin.args.disabledConditions||{})["anycolumn"]:this.plugin.grid.layout.cells[_5b].disabledConditions;
if(!_4.isArray(_5a)){
_5a=[];
}
if(!_4.isArray(_5c)){
_5c=[];
}
var arr=_5a.concat(_5c);
if(arr.length){
var _5d={};
_2.forEach(arr,function(c){
if(_4.isString(c)){
_5d[c.toLowerCase()]=true;
}
});
return _2.filter(_59,function(_5e){
return !(_5e.value in _5d);
});
}
return _59;
},_setConditionsByType:function(_5f){
var _60=this._condSelect;
_60.removeOption(_60.options);
_60.addOption(this._getUsableConditions(_5f));
this._showSelectOrLabel(this._condSelect,this._condSelectAlt);
},_setValueBoxByType:function(_61){
if(this._curValueBox){
this.valueNode.removeChild(this._curValueBox.domNode);
try{
this._curValueBox.destroyRecursive();
}
catch(e){
}
delete this._curValueBox;
}
var _62=this.dlg._dataTypeMap[_61].valueBoxCls[this._getValueBoxClsInfo(this._colSelect.get("value"),_61)],_63=this._getValueBoxArgByType(_61);
this._curValueBox=this[this._isRange?"_createRangeBox":"_createValueBox"](_62,_63);
this.valueNode.appendChild(this._curValueBox.domNode);
this._curValueBox.focusNode.setAttribute("aria-label",_9.substitute(this.plugin.nls.waiValueBoxTemplate,[this._index]));
this.dlg.onRendered(this);
},_getValueBoxArgByType:function(_64){
var g=this.plugin.grid,_65=g.layout.cells[parseInt(this._colSelect.get("value"),10)],res={cbox:this};
if(_64=="string"){
if(_65&&(_65.suggestion||_65.autoComplete)){
_4.mixin(res,{store:g.store,searchAttr:_65.field||_65.name,query:g.query||{},fetchProperties:{sort:[{"attribute":_65.field||_65.name}],queryOptions:_4.mixin({ignoreCase:true,deep:true},g.queryOptions||{})}});
}
}else{
if(_64=="boolean"){
_4.mixin(res,this.dlg.builder.defaultArgs["boolean"]);
}
}
if(_65&&_65.dataTypeArgs){
_4.mixin(res,_65.dataTypeArgs);
}
return res;
},formatValue:function(_66,_67,v){
if(_67=="isempty"){
return "";
}
if(_66=="date"||_66=="time"){
var opt={selector:_66},fmt=_b.format;
if(_67=="range"){
return _9.substitute(this.plugin.nls.rangeTemplate,[fmt(v.start,opt),fmt(v.end,opt)]);
}
return fmt(v,opt);
}else{
if(_66=="boolean"){
return v?this._curValueBox._lblTrue:this._curValueBox._lblFalse;
}
}
return v;
},_getValueBoxClsInfo:function(_68,_69){
var _6a=this.plugin.grid.layout.cells[parseInt(_68,10)];
if(_69=="string"){
return (_6a&&(_6a.suggestion||_6a.autoComplete))?"ac":"dft";
}
return "dft";
}});
var _6b=_1("dojox.grid.enhanced.plugins.filter.UniqueComboBox",_e,{_openResultList:function(_6c){
var _6d={},s=this.store,_6e=this.searchAttr;
arguments[0]=_2.filter(_6c,function(_6f){
var key=s.getValue(_6f,_6e),_70=_6d[key];
_6d[key]=true;
return !_70;
});
this.inherited(arguments);
},_onKey:function(evt){
if(evt.charOrCode===_8.ENTER&&this._opened){
_5.stop(evt);
}
this.inherited(arguments);
}});
var _71=_1("dojox.grid.enhanced.plugins.filter.BooleanValueBox",[_16,_17,_18],{templateString:_1e,widgetsInTemplate:true,constructor:function(_72){
var nls=_72.cbox.plugin.nls;
this._baseId=_72.cbox.id;
this._lblTrue=_72.trueLabel||nls.trueLabel||"true";
this._lblFalse=_72.falseLabel||nls.falseLabel||"false";
this.args=_72;
},postCreate:function(){
this.onChange();
},onChange:function(){
},get:function(_73){
return this.rbTrue.get("checked");
},set:function(_74,v){
this.inherited(arguments);
if(_74=="value"){
this.rbTrue.set("checked",!!v);
this.rbFalse.set("checked",!v);
}
}});
var _75=_1("dojox.grid.enhanced.plugins.filter.FilterDefDialog",null,{curColIdx:-1,_relOpCls:"logicall",_savedCriterias:null,plugin:null,constructor:function(_76){
var _77=this.plugin=_76.plugin;
this.builder=new _c();
this._setupData();
this._cboxes=[];
this.defaultType=_77.args.defaultType||"string";
(this.filterDefPane=new _35({"dlg":this})).startup();
(this._defPane=new _d({"refNode":this.plugin.grid.domNode,"title":_77.nls.filterDefDialogTitle,"class":"dojoxGridFDTitlePane","iconClass":"dojoxGridFDPaneIcon","content":this.filterDefPane})).startup();
this._defPane.connect(_77.grid.layer("filter"),"filterDef",_4.hitch(this,"_onSetFilter"));
_77.grid.setFilter=_4.hitch(this,"setFilter");
_77.grid.getFilter=_4.hitch(this,"getFilter");
_77.grid.getFilterRelation=_4.hitch(this,function(){
return this._relOpCls;
});
_77.connect(_77.grid.layout,"moveColumn",_4.hitch(this,"onMoveColumn"));
},onMoveColumn:function(_78,_79,_7a,_7b,_7c){
if(this._savedCriterias&&_7a!=_7b){
if(_7c){
--_7b;
}
var min=_7a<_7b?_7a:_7b;
var max=_7a<_7b?_7b:_7a;
var dir=_7b>min?1:-1;
_2.forEach(this._savedCriterias,function(sc){
var idx=parseInt(sc.column,10);
if(!isNaN(idx)&&idx>=min&&idx<=max){
sc.column=String(idx==_7a?idx+(max-min)*dir:idx-dir);
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
this._dataTypeMap={"number":{valueBoxCls:{dft:_10},conditions:[{label:nls.conditionEqual,value:"equalto",selected:true},{label:nls.conditionNotEqual,value:"notequalto"},{label:nls.conditionLess,value:"lessthan"},{label:nls.conditionLessEqual,value:"lessthanorequalto"},{label:nls.conditionLarger,value:"largerthan"},{label:nls.conditionLargerEqual,value:"largerthanorequalto"},{label:nls.conditionIsEmpty,value:"isempty"}]},"string":{valueBoxCls:{dft:_f,ac:_6b},conditions:[{label:nls.conditionContains,value:"contains",selected:true},{label:nls.conditionIs,value:"equalto"},{label:nls.conditionStartsWith,value:"startswith"},{label:nls.conditionEndWith,value:"endswith"},{label:nls.conditionNotContain,value:"notcontains"},{label:nls.conditionIsNot,value:"notequalto"},{label:nls.conditionNotStartWith,value:"notstartswith"},{label:nls.conditionNotEndWith,value:"notendswith"},{label:nls.conditionIsEmpty,value:"isempty"}]},"date":{valueBoxCls:{dft:_11},conditions:[{label:nls.conditionIs,value:"equalto",selected:true},{label:nls.conditionBefore,value:"lessthan"},{label:nls.conditionAfter,value:"largerthan"},{label:nls.conditionRange,value:"range"},{label:nls.conditionIsEmpty,value:"isempty"}]},"time":{valueBoxCls:{dft:_12},conditions:[{label:nls.conditionIs,value:"equalto",selected:true},{label:nls.conditionBefore,value:"lessthan"},{label:nls.conditionAfter,value:"largerthan"},{label:nls.conditionRange,value:"range"},{label:nls.conditionIsEmpty,value:"isempty"}]},"boolean":{valueBoxCls:{dft:_71},conditions:[{label:nls.conditionIs,value:"equalto",selected:true},{label:nls.conditionIsEmpty,value:"isempty"}]}};
},setFilter:function(_7d,_7e){
_7d=_7d||[];
if(!_4.isArray(_7d)){
_7d=[_7d];
}
var _7f=function(){
if(_7d.length){
this._savedCriterias=_2.map(_7d,function(_80){
var _81=_80.type||this.defaultType;
return {"type":_81,"column":String(_80.column),"condition":_80.condition,"value":_80.value,"colTxt":this.getColumnLabelByValue(String(_80.column)),"condTxt":this.getConditionLabelByValue(_81,_80.condition),"formattedVal":_80.formattedVal||_80.value};
},this);
this._criteriasChanged=true;
if(_7e==="logicall"||_7e==="logicany"){
this._relOpCls=_7e;
}
var _82=_2.map(_7d,this.getExprForCriteria,this);
_82=this.builder.buildExpression(_82.length==1?_82[0]:{"op":this._relOpCls,"data":_82});
this.plugin.grid.layer("filter").filterDef(_82);
this.plugin.filterBar.toggleClearFilterBtn(false);
}
this._closeDlgAndUpdateGrid();
};
if(this._savedCriterias){
this._clearWithoutRefresh=true;
var _83=_3.connect(this,"clearFilter",this,function(){
_3.disconnect(_83);
this._clearWithoutRefresh=false;
_7f.apply(this);
});
this.onClearFilter();
}else{
_7f.apply(this);
}
},getFilter:function(){
return _4.clone(this._savedCriterias)||[];
},getColumnLabelByValue:function(v){
var nls=this.plugin.nls;
if(v.toLowerCase()=="anycolumn"){
return nls["anyColumnOption"];
}else{
var _84=this.plugin.grid.layout.cells[parseInt(v,10)];
return _84?(_84.name||_84.field):"";
}
},getConditionLabelByValue:function(_85,c){
var _86=this._dataTypeMap[_85].conditions;
for(var i=_86.length-1;i>=0;--i){
var _87=_86[i];
if(_87.value==c.toLowerCase()){
return _87.label;
}
}
return "";
},addCriteriaBoxes:function(cnt){
if(typeof cnt!="number"||cnt<=0){
return;
}
var cbs=this._cboxes,cc=this.filterDefPane.cboxContainer,_88=this.plugin.args.ruleCount,len=cbs.length,_89;
if(_88>0&&len+cnt>_88){
cnt=_88-len;
}
for(;cnt>0;--cnt){
_89=new _36({dlg:this});
cbs.push(_89);
cc.addChild(_89);
}
cc.startup();
this._updatePane();
this._updateCBoxTitles();
cc.selectChild(cbs[cbs.length-1]);
this.filterDefPane.criteriaPane.scrollTop=1000000;
if(cbs.length===4){
if(_7("ie")<=6&&!this.__alreadyResizedForIE6){
var _8a=_6.position(cc.domNode);
_8a.w-=_1a.getScrollbar().w;
cc.resize(_8a);
this.__alreadyResizedForIE6=true;
}else{
cc.resize();
}
}
},removeCriteriaBoxes:function(cnt,_8b){
var cbs=this._cboxes,cc=this.filterDefPane.cboxContainer,len=cbs.length,_8c=len-cnt,end=len-1,_8d,_8e=_2.indexOf(cbs,cc.selectedChildWidget.content);
if(_4.isArray(cnt)){
var i,_8f=cnt;
_8f.sort();
cnt=_8f.length;
for(i=len-1;i>=0&&_2.indexOf(_8f,i)>=0;--i){
}
if(i>=0){
if(i!=_8e){
cc.selectChild(cbs[i]);
}
for(i=cnt-1;i>=0;--i){
if(_8f[i]>=0&&_8f[i]<len){
cc.removeChild(cbs[_8f[i]]);
cbs.splice(_8f[i],1);
}
}
}
_8c=cbs.length;
}else{
if(_8b===true){
if(cnt>=0&&cnt<len){
_8c=end=cnt;
cnt=1;
}else{
return;
}
}else{
if(cnt instanceof _36){
_8d=cnt;
cnt=1;
_8c=end=_2.indexOf(cbs,_8d);
}else{
if(typeof cnt!="number"||cnt<=0){
return;
}else{
if(cnt>=len){
cnt=end;
_8c=1;
}
}
}
}
if(end<_8c){
return;
}
if(_8e>=_8c&&_8e<=end){
cc.selectChild(cbs[_8c?_8c-1:end+1]);
}
for(;end>=_8c;--end){
cc.removeChild(cbs[end]);
}
cbs.splice(_8c,cnt);
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
},getExprForCriteria:function(_90){
if(_90.column=="anycolumn"){
var _91=_2.filter(this.plugin.grid.layout.cells,function(_92){
return !(_92.filterable===false||_92.hidden);
});
return {"op":"logicany","data":_2.map(_91,function(_93){
return this.getExprForColumn(_90.value,_93.index,_90.type,_90.condition);
},this)};
}else{
return this.getExprForColumn(_90.value,_90.column,_90.type,_90.condition);
}
},getExprForColumn:function(_94,_95,_96,_97){
_95=parseInt(_95,10);
var _98=this.plugin.grid.layout.cells[_95],_99=_98.field||_98.name,obj={"datatype":_96||this.getColumnType(_95),"args":_98.dataTypeArgs,"isColumn":true},_9a=[_4.mixin({"data":this.plugin.args.isServerSide?_99:_98},obj)];
obj.isColumn=false;
if(_97=="range"){
_9a.push(_4.mixin({"data":_94.start},obj),_4.mixin({"data":_94.end},obj));
}else{
if(_97!="isempty"){
_9a.push(_4.mixin({"data":_94},obj));
}
}
return {"op":_97,"data":_9a};
},getColumnType:function(_9b){
var _9c=this.plugin.grid.layout.cells[parseInt(_9b,10)];
if(!_9c||!_9c.datatype){
return this.defaultType;
}
var _9d=String(_9c.datatype).toLowerCase();
return this._dataTypeMap[_9d]?_9d:this.defaultType;
},clearFilter:function(_9e){
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
if(_9e){
this.closeDialog();
}else{
this._closeDlgAndUpdateGrid();
}
},showDialog:function(_9f){
this._defPane.show();
this.plugin.filterStatusTip.closeDialog();
this._prepareDialog(_9f);
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
},onRendered:function(_a0){
if(!_7("ff")){
var _a1=_1b._getTabNavigable(_6.byId(_a0.domNode));
_19.focus(_a1.lowest||_a1.first);
}else{
var dp=this._defPane;
dp._getFocusItems(dp.domNode);
_19.focus(dp._firstFocusItem);
}
},_onSetFilter:function(_a2){
if(_a2===null&&this._savedCriterias){
this.clearFilter();
}
},_prepareDialog:function(_a3){
var sc=this._savedCriterias,cbs=this._cboxes,i,_a4;
this.curColIdx=_a3;
if(!sc){
if(cbs.length===0){
this.addCriteriaBoxes(1);
}else{
for(i=0;(_a4=cbs[i]);++i){
_a4.changeCurrentColumn();
}
}
}else{
if(this._criteriasChanged){
this.filterDefPane._relSelect.set("value",this._relOpCls==="logicall"?"0":"1");
this._criteriasChanged=false;
var _a5=sc.length>cbs.length?sc.length-cbs.length:0;
this.addCriteriaBoxes(_a5);
this.removeCriteriaBoxes(cbs.length-sc.length);
this.filterDefPane._clearFilterBtn.set("disabled",false);
for(i=0;i<cbs.length-_a5;++i){
cbs[i].load(sc[i]);
}
if(_a5>0){
var _a6=[],_a7=_3.connect(this,"onRendered",function(_a8){
var i=_2.indexOf(cbs,_a8);
if(!_a6[i]){
_a6[i]=true;
if(--_a5===0){
_3.disconnect(_a7);
}
_a8.load(sc[i]);
}
});
}
}
}
this.filterDefPane.cboxContainer.resize();
},_defineFilter:function(){
var cbs=this._cboxes,_a9=function(_aa){
return _2.filter(_2.map(cbs,function(_ab){
return _ab[_aa]();
}),function(_ac){
return !!_ac;
});
},_ad=_a9("getExpr");
this._savedCriterias=_a9("save");
_ad=_ad.length==1?_ad[0]:{"op":this._relOpCls,"data":_ad};
_ad=this.builder.buildExpression(_ad);
this.plugin.grid.layer("filter").filterDef(_ad);
this.filterDefPane._clearFilterBtn.set("disabled",false);
},_updateCBoxTitles:function(){
for(var cbs=this._cboxes,i=cbs.length;i>0;--i){
cbs[i-1].updateRuleIndex(i);
cbs[i-1].setAriaInfo(i);
}
},_updatePane:function(){
var cbs=this._cboxes,_ae=this.filterDefPane;
_ae._addCBoxBtn.set("disabled",cbs.length==this.plugin.args.ruleCount);
_ae._filterBtn.set("disabled",!this.canFilter());
},canFilter:function(){
return _2.filter(this._cboxes,function(_af){
return !_af.isEmpty();
}).length>0;
},_closeDlgAndUpdateGrid:function(){
this.closeDialog();
var g=this.plugin.grid;
g.showMessage(g.loadingMessage);
setTimeout(_4.hitch(g,g._refresh),this._defPane.duration+10);
}});
return _75;
});
