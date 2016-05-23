exports = module.exports = wmresponse;

function wmresponse() {
	this.header = new Object();
	this.header['ack'] = 1;
	this.data = new Object();

	Object.defineProperty(Object.prototype, "deepCopy", {
	configurable: true,
	enumerable: false,
	value: function(destination, source) {
			for (var property in source) {
				if (typeof source[property] === "object" &&
					source[property] !== null ) {
						destination[property] = destination[property] || {};
						arguments.callee(destination[property], source[property]);
					} else {
						destination[property] = source[property];
					}
			}
			return destination;
		}
	});

	Object.defineProperty(Object.prototype, "populateData", {
	configurable: true,
	enumerable: false,
	value: function(destination, source) {
			for (var property in destination) {
				if (typeof destination[property] === "object" &&
				    property === "data" ) {
						destination[property] = source;
					}
			}
			return destination;
		}
	});

	this.send = function(res, httpCode) {
		res.writeHead(httpCode);
		res.write(JSON.stringify(this));
		res.end();
	};
}

