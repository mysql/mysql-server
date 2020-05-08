define([
	'exports',
	'intern/dojo/_base/lang',
	'intern/dojo/_base/array',
	'intern/dojo/Deferred',
	'intern/dojo/errors/CancelError',
	'intern/dojo/promise/Promise',
	'intern/dojo/promise/all'
], function (exports, lang, array, Deferred, CancelError, DojoPromise, all) {
	exports.Promise = function Promise(executor, returnDfd) {
		var canceler;
		var dfd = new Deferred(function (reason) {
			if (canceler) {
				return canceler(reason);
			}
			else {
				return new CancelError('canceled');
			}
		});

		try {
			executor(
				lang.hitch(dfd, 'resolve'),
				lang.hitch(dfd, 'reject'),
				lang.hitch(dfd, 'progress'),
				function (_canceler) {
					canceler = _canceler;
				}
			);
		}
		catch (e) {
			dfd.reject(e);
		}

		return returnDfd ? dfd : dfd.promise;
	};

	exports.Promise.reject = function (error) {
		var dfd = new Deferred();
		dfd.reject(error);
		return dfd.promise;
	};

	exports.Promise.resolve = function (value) {
		if (value instanceof DojoPromise) {
			return value;
		}
		if (value instanceof Deferred) {
			return value.promise;
		}

		var dfd = new Deferred();
		if (value && typeof value.then === 'function') {
			value.then(
				lang.hitch(dfd, 'resolve'),
				lang.hitch(dfd, 'reject'),
				lang.hitch(dfd, 'progress')
			);
		}
		else {
			dfd.resolve(value);
		}
		return dfd.promise;
	};

	exports.map = function (values, callback) {
		return all(
			array.map(values, function (value, index) {
				return exports.Promise.resolve(value).then(function (value) {
					return callback(value, index);
				});
			})
		);
	};

	exports.delay = function (value, milliseconds) {
		var promise = exports.Promise.resolve(value);
		return new exports.Promise(function (resolve, reject, progress) {
			setTimeout(function () {
				promise.then(resolve, reject, progress);
			}, milliseconds);
		});
	};

	exports.call = function (func) {
		var args = Array.prototype.slice.call(arguments, 1);
		return exports.Promise(function (resolve, reject) {
			args.push(function (err, value) {
				if (err) {
					reject(err);
				}
				else {
					resolve(value);
				}
			});
			func.apply(this, args);
		});
	};
});
