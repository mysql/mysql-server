define([
	'../util'
], function (util) {
	function JSONP(callback, data) {
		return callback + '(' + JSON.stringify(data) + ');';
	}

	function checkString(variable, data) {
		return 'var ' + variable + ' = ' + JSON.stringify(data) + ';';
	}

	function scriptVar(variable) {
		return 'var ' + variable + ' = "loaded";';
	}

	return function (request) {
		var promise = new util.Promise(function (resolve) {
			var data = '';

			if (request.query.callback) {
				data = JSONP(request.query.callback, { animalType: 'mammal' });
			}
			else if (request.query.checkString) {
				data = checkString(request.query.checkString, [ 'Take out trash.', 'Do dishes.', 'Brush teeth.' ]);
			}
			else if (request.query.scriptVar) {
				data = scriptVar(request.query.scriptVar);
			}

			resolve({
				status: 200,
				headers: {
					'Content-Type': 'application/x-javascript'
				},
				body: [
					data
				]
			});
		});

		var milliseconds = request.query.delay;
		if (milliseconds) {
			milliseconds = parseInt(milliseconds, 10);
			promise = util.delay(promise, milliseconds);
		}

		return promise;
	};
});
