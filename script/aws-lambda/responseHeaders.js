﻿'use strict';

exports.handler = (event, context, callback) => {
	const requestUri = event.Records[0].cf.request.uri;
	const response = event.Records[0].cf.response;
	let headers = response.headers;

	headers["strict-transport-security"] = [{
		key: "Strict-Transport-Security",
		value: "max-age=63072000"
	}];

	if (requestUri.slice(-5) === ".html") {
		headers["content-security-policy"] = [{
			key: "Content-Security-Policy",
			value: "default-src 'none'" +
				"; connect-src https://www.google-analytics.com" +
				"; img-src 'self' www.googletagmanager.com www.google-analytics.com" +
				"; script-src 'self' https://www.googletagmanager.com https://www.google-analytics.com https://ssl.google-analytics.com https://gist.github.com 'unsafe-inline'" +
				"; style-src 'self' https://fonts.googleapis.com https://github.githubassets.com 'unsafe-inline'" +
				"; font-src 'self' https://fonts.gstatic.com" +
				"; frame-src https:"
		}];

		headers["referrer-policy"] = [{
			key: "Referrer-Policy",
			value: "no-referrer, strict-origin-when-cross-origin"
		}];

		headers["x-content-type-options"] = [{
			key: "X-Content-Type-Options",
			value: "nosniff"
		}];
		headers["x-frame-options"] = [{
			key: "X-Frame-Options",
			value: "DENY"
		}];
		headers["x-xss-protection"] = [{
			key: "X-XSS-Protection",
			value: "1; mode=block"
		}];
	}

	headers["cache-control"] = [{
		key: "Cache-Control",
		value: "public, max-age=3600"
	}];

	return callback(null, response);
};