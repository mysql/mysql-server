/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Target",["./Source"],function(_1){
return dojo.declare("dojo.dnd.Target",_1,{constructor:function(_2,_3){
this.isSource=false;
dojo.removeClass(this.node,"dojoDndSource");
}});
});
