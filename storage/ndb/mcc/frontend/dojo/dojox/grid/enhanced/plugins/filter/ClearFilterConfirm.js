//>>built
define("dojox/grid/enhanced/plugins/filter/ClearFilterConfirm",["dojo/_base/declare","dojo/cache","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin"],function(_1,_2,_3,_4,_5){
return _1("dojox.grid.enhanced.plugins.filter.ClearFilterConfirm",[_3,_4,_5],{templateString:_2("dojox.grid","enhanced/templates/ClearFilterConfirmPane.html"),widgetsInTemplate:true,plugin:null,postMixInProperties:function(){
var _6=this.plugin.nls;
this._clearBtnLabel=_6["clearButton"];
this._cancelBtnLabel=_6["cancelButton"];
this._clearFilterMsg=_6["clearFilterMsg"];
},postCreate:function(){
this.inherited(arguments);
this.cancelBtn.domNode.setAttribute("aria-label",this.plugin.nls["waiCancelButton"]);
this.clearBtn.domNode.setAttribute("aria-label",this.plugin.nls["waiClearButton"]);
},uninitialize:function(){
this.plugin=null;
},_onCancel:function(){
this.plugin.clearFilterDialog.hide();
},_onClear:function(){
this.plugin.clearFilterDialog.hide();
this.plugin.filterDefDialog.clearFilter(this.plugin.filterDefDialog._clearWithoutRefresh);
}});
});
