(function() {
	const navBarId = "topNavigationBar";
	const offsetId = "topOffsetElement";

	let urlPrefix = "";
	let blogTitle = "Blog";

	if (document.documentElement.lang === "ru") {
		//urlPrefix = "ru/";
		blogTitle = "Блог";
	}

	document.write(
		'<div id="' + offsetId + '"></div>' +
		'<table id="' + navBarId + '" class="navbar">' +
		  '<tr>' +
			'<td class="logo">' +
			  '<a class="nodecor" href="https://dmaslov.me">Dmitry Maslov</a>' +
			'</td>' +
			'<td class="divider"></td>' +
			'<td style="padding: 0">' +
			  '<div class="flexitems">' +
				'<div class="flexitem home">' +
				  '<a class="nodecor" href="https://dmaslov.me">' +
					'<div class="navitem svg">' +
					  '<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 18 547.6 503.7" style="position: relative; left: 0; top: 1px; width: 25px; height: 23px">' +
						'<path d="M540.76,254.788L294.506,38.216 c-11.475-10.098-30.064-10.098-41.386,0L6.943,254.788 c-11.475,10.098-8.415,18.284,6.885,18.284 h75.964 v221.773 c0,12.087,9.945,22.108,22.108,22.108 h92.947V371.067 c0-12.087,9.945-22.108,22.109-22.108 h93.865 c12.239,0,22.108,9.792,22.108,22.108 v145.886 h92.947 c12.24,0,22.108-9.945,22.108-22.108 v-221.85 h75.965 C549.021,272.995,552.081,264.886,540.76,254.788z" />' +
					  '</svg>' +
					'</div>' +
				  '</a>' +
				'</div>' +
				'<div class="flexitem">' +
				  '<a class="nodecor" href="/' + urlPrefix + 'blog/">' +
					'<div class="navitem">' + blogTitle + '</div>' +
				  '</a>' +
				'</div>' +
				'<div class="flexitem">' +
				  //'<a class="nodecor" href="/' + urlPrefix + 'alsn/">' +
					'<div class="navitem" style="background: inherit; color: #888">ALSN</div>' +
				  //'</a>' +
				'</div>' +
				'<div class="flexitem">' +
				  '<a class="nodecor" href="/' + urlPrefix + 'mdpn/">' +
					'<div class="navitem">MDPN</div>' +
				  '</a>' +
				'</div>' +
				'<div class="flexitem">' +
				  '<a class="nodecor" href="/' + urlPrefix + 'p196/">' +
					'<div class="navitem">P196</div>' +
				  '</a>' +
				'</div>' +
			  '</div>' +
			'</td>' +
		  '</tr>' +
		'</table>'
	);

	var resizeTimeout;
	function resizeThrottler() {
		if (!resizeTimeout) {
			resizeTimeout = setTimeout(function() {
				resizeTimeout = null;
				resizeHandler();
			}, 80);
		}
	}

	function resizeHandler() {
		const bodyStyle = window.getComputedStyle(document.body);
		const margin = parseInt(bodyStyle.marginTop, 10);

		const navBar = document.getElementById(navBarId);
		const height = navBar ? navBar.offsetHeight : 40;

		if (height >= margin) {
			let offsetDiv = document.getElementById(offsetId);
			offsetDiv.style.minHeight = (height - margin) + "px";
			offsetDiv.style.marginBottom = margin + "px";
		}

		const padding = (height + 8) + "px";
		document.documentElement.style.scrollPaddingTop = padding;
	}

	resizeHandler();
	window.addEventListener("resize", resizeThrottler, false);
})();

function insertFooter(lang, url) {
	let languageCode = "EN";
	let languageTitle = "English (EN)";

	if (lang === "ru") {
		languageCode = "RU";
		languageTitle = "Русский (RU)";
	}

	let urlPrefix = "";
	let mapText = "Site map";
	let contactsText = "Contacts";

	if (document.documentElement.lang === "ru") {
		//urlPrefix = "ru/";
		mapText = "Карта сайта";
		contactsText = "Контакты";
	}

	document.write(
		'<div class="column">' +
		  '<hr class="footer">' +
		  '<table class="nospacing" style="width: 100%">' +
			'<tr style="vertical-align: top">' +
			  '<td class="footer">' +
				'<div style="margin-right: 25px">' +
				  'Copyright &copy;&nbsp;2019-2021 Dmitry&nbsp;Maslov' +
				'</div>' +
			  '</td>' +
			  '<td class="footer" style="text-align: right">' +
				'<div style="display: inline-block">' +
				  '<a href="/' + urlPrefix + 'map.html">' + mapText + '</a>' +
				'</div>' +
				'<div style="display: inline-block">' +
				  '<span class="dot">&middot;</span>' +
				  '<a href="/' + urlPrefix + 'contacts.html">' + contactsText + '</a>' +
				'</div>' +
				'<div style="display: inline-block">' +
				  '<span class="dot">&middot;</span>' +
				  '<a href="' + url + '" title="' + languageTitle + '" style="color: inherit">' +
					'<b>' + languageCode + '</b>' +
				  '</a>' +
				'</div>' +
			  '</td>' +
			'</tr>' +
		  '</table>' +
		'</div>'
	);
}
