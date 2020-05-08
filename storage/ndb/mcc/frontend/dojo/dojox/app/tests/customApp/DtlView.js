define(["dojo/_base/declare", "dojox/app/View", "dojox/dtl/_Templated"],
	function(declare, View, _Templated){
		return declare([_Templated, View], {
			_dijitTemplateCompat: true
		});
	}
);