//>>built
require({cache:{"url:dojox/grid/enhanced/templates/FilterStatusPane.html":"<div class=\"dojoxGridFStatusTip\"\n\t><div class=\"dojoxGridFStatusTipHead\"\n\t\t><span class=\"dojoxGridFStatusTipTitle\" dojoAttachPoint=\"statusTitle\"></span\n\t\t><span class=\"dojoxGridFStatusTipRel\" dojoAttachPoint=\"statusRel\"></span\n\t></div\n\t><div class=\"dojoxGridFStatusTipDetail\" dojoAttachPoint=\"statusDetailNode\"\n\t></div\n></div>\n"}});
define("dojox/grid/enhanced/plugins/filter/FilterStatusTip",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/query","dojo/string","dojo/date/locale","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/TooltipDialog","dijit/form/Button","dijit/_base/popup","dojo/text!../../templates/FilterStatusPane.html","dojo/i18n!../../nls/Filter"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
var _e="",_f="",_10="",_11="",_12="dojoxGridFStatusTipOddRow",_13="dojoxGridFStatusTipHandle",_14="dojoxGridFStatusTipCondition",_15="dojoxGridFStatusTipDelRuleBtnIcon",_16="</tbody></table>";
var _17=_1("dojox.grid.enhanced.plugins.filter.FilterStatusPane",[_7,_8],{templateString:_d});
return _1("dojox.grid.enhanced.plugins.filter.FilterStatusTip",null,{constructor:function(_18){
var _19=this.plugin=_18.plugin;
this._statusHeader=["<table border='0' cellspacing='0' class='",_e,"'><thead><tr class='",_f,"'><th class='",_10,"'><div>",_19.nls["statusTipHeaderColumn"],"</div></th><th class='",_10," lastColumn'><div>",_19.nls["statusTipHeaderCondition"],"</div></th></tr></thead><tbody>"].join("");
this._removedCriterias=[];
this._rules=[];
this.statusPane=new _17();
this._dlg=new _a({"class":"dijitTooltipBelow dojoxGridFStatusTipDialog",content:this.statusPane,autofocus:false});
this._dlg.connect(this._dlg.domNode,"onmouseleave",_3.hitch(this,this.closeDialog));
this._dlg.connect(this._dlg.domNode,"click",_3.hitch(this,this._modifyFilter));
},destroy:function(){
this._dlg.destroyRecursive();
},showDialog:function(_1a,_1b,_1c){
this._pos={x:_1a,y:_1b};
_c.close(this._dlg);
this._removedCriterias=[];
this._rules=[];
this._updateStatus(_1c);
_c.open({popup:this._dlg,parent:this.plugin.filterBar,onCancel:function(){
},x:_1a-12,y:_1b-3});
},closeDialog:function(){
_c.close(this._dlg);
if(this._removedCriterias.length){
this.plugin.filterDefDialog.removeCriteriaBoxes(this._removedCriterias);
this._removedCriterias=[];
this.plugin.filterDefDialog.onFilter();
}
},_updateStatus:function(_1d){
var res,p=this.plugin,nls=p.nls,sp=this.statusPane,fdg=p.filterDefDialog;
if(fdg.getCriteria()===0){
sp.statusTitle.innerHTML=nls["statusTipTitleNoFilter"];
sp.statusRel.innerHTML="";
var _1e=p.grid.layout.cells[_1d];
var _1f=_1e?"'"+(_1e.name||_1e.field)+"'":nls["anycolumn"];
res=_5.substitute(nls["statusTipMsg"],[_1f]);
}else{
sp.statusTitle.innerHTML=nls["statusTipTitleHasFilter"];
sp.statusRel.innerHTML=fdg._relOpCls=="logicall"?nls["statusTipRelAll"]:nls["statusTipRelAny"];
this._rules=[];
var i=0,c=fdg.getCriteria(i++);
while(c){
c.index=i-1;
this._rules.push(c);
c=fdg.getCriteria(i++);
}
res=this._createStatusDetail();
}
sp.statusDetailNode.innerHTML=res;
this._addButtonForRules();
},_createStatusDetail:function(){
return this._statusHeader+_2.map(this._rules,function(_20,i){
return this._getCriteriaStr(_20,i);
},this).join("")+_16;
},_addButtonForRules:function(){
if(this._rules.length>1){
_4("."+_13,this.statusPane.statusDetailNode).forEach(_3.hitch(this,function(nd,idx){
(new _b({label:this.plugin.nls["removeRuleButton"],showLabel:false,iconClass:_15,onClick:_3.hitch(this,function(e){
e.stopPropagation();
this._removedCriterias.push(this._rules[idx].index);
this._rules.splice(idx,1);
this.statusPane.statusDetailNode.innerHTML=this._createStatusDetail();
this._addButtonForRules();
})})).placeAt(nd,"last");
}));
}
},_getCriteriaStr:function(c,_21){
var res=["<tr class='",_11," ",(_21%2?_12:""),"'><td class='",_10,"'>",c.colTxt,"</td><td class='",_10,"'><div class='",_13,"'><span class='",_14,"'>",c.condTxt,"&nbsp;</span>",c.formattedVal,"</div></td></tr>"];
return res.join("");
},_modifyFilter:function(){
this.closeDialog();
var p=this.plugin;
p.filterDefDialog.showDialog(p.filterBar.getColumnIdx(this._pos.x));
}});
});
