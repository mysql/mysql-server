/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

/*
	This is an optimized version of Dojo, built for deployment and not for
	development. To get sources and documentation, please visit:

		http://dojotoolkit.org
*/

//>>built
define("dojox/mobile/compat",["dojo/_base/lang","dojo/_base/sniff"],function(_1,_2){var dm=_1.getObject("dojox.mobile",true);if(!_2("webkit")){var s="dojox/mobile/_compat";require([s]);}return dm;});