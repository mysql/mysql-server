require({cache:
	{
		'common/another':function(){
			define(['./anotherone'], function () {
				console.log('this is Common2/another in layer');
				results.push('Common2/another:cache');
			});
		},
		'common/anotherone':function(){
			define([], function () {
				console.log('this is Common2/anotherone in layer');
				results.push('Common2/anotherone:cache');
			});
		}

	}
});
define(['common/another', 'starmapModule/mappedB'], function() {
	console.log('this is App2/thing in layer');
	results.push('App2/thing:cache');
});