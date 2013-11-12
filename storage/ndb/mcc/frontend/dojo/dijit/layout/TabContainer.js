//>>built
define("dijit/layout/TabContainer",["dojo/_base/lang","dojo/_base/declare","./_TabContainerBase","./TabController","./ScrollingTabController"],function(_1,_2,_3,_4,_5){
return _2("dijit.layout.TabContainer",_3,{useMenu:true,useSlider:true,controllerWidget:"",_makeController:function(_6){
var _7=this.baseClass+"-tabs"+(this.doLayout?"":" dijitTabNoLayout"),_4=_1.getObject(this.controllerWidget);
return new _4({id:this.id+"_tablist",dir:this.dir,lang:this.lang,textDir:this.textDir,tabPosition:this.tabPosition,doLayout:this.doLayout,containerId:this.id,"class":_7,nested:this.nested,useMenu:this.useMenu,useSlider:this.useSlider,tabStripClass:this.tabStrip?this.baseClass+(this.tabStrip?"":"No")+"Strip":null},_6);
},postMixInProperties:function(){
this.inherited(arguments);
if(!this.controllerWidget){
this.controllerWidget=(this.tabPosition=="top"||this.tabPosition=="bottom")&&!this.nested?"dijit.layout.ScrollingTabController":"dijit.layout.TabController";
}
}});
});
