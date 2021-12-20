'use strict';

exports.handler = (event, context, callback) => {
	let request = event.Records[0].cf.request;

	if (request.uri === "/ru" ||
		request.uri === "/alsn" || request.uri === "/ru/alsn" ||
		request.uri === "/blog" || request.uri === "/ru/blog" ||
		request.uri === "/mdpn" || request.uri === "/ru/mdpn" ||
		request.uri === "/mdpn/pal" || request.uri === "/ru/mdpn/pal" ||
		request.uri === "/p196" || request.uri === "/ru/p196")
	{
		request.uri += "/";
	}

	// Replace any '/' at the end of a URI with default index
	request.uri = request.uri.replace(/\/$/, "/index.html");

	return callback(null, request);
};
