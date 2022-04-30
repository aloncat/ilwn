(function() {
	var resizeTimeout;
	function resizeThrottler() {
		if (!resizeTimeout) {
			resizeTimeout = setTimeout(function() {
				resizeTimeout = null;
				resizeHandler();
			}, 80);
		}
	}

	const navBarId = "topNavigationBar";
	const offsetId = "topOffsetElement";

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

	insertNavigationBar(navBarId, offsetId);
	resizeHandler();

	window.addEventListener("resize", resizeThrottler);
	document.addEventListener("DOMContentLoaded", insertFooter);
})();

// All pages: navigation bar

function insertNavigationBar(navBarId, offsetId) {
	let urlPrefix = "";
	let blogTitle = "Blog";
	let logoTitle = "iLWN project (Dmitry Maslov)";

	if (document.documentElement.lang === "ru") {
		urlPrefix = "ru/";
		blogTitle = "Блог";
		logoTitle = "Проект iLWN (Дмитрий Маслов)";
	}

	const isLocal = window.location.hostname === "localhost";
	const section = getCurrentSection();

	document.write(
		'<div id="' + offsetId + '"></div>' +
		'<table id="' + navBarId + '" class="navbar">' +
		  '<tr>' +
			'<td class="logo" title="' + logoTitle + '">' +
			  '<a href="/' + urlPrefix + 'about.html">iLWN</a>' +
			'</td>' +
			'<td class="divider"></td>' +
			'<td style="padding: 0">' +
			  '<div class="flexitems">' +
				'<div class="flexitem home">' +
				  '<a class="nodecor" href="' + (isLocal ? '/' : 'https://dmaslov.me') + '">' +
					'<div class="navitem svg' + (section === "home" ? 'active' : '') + '">' +
					  '<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 18 547.6 503.7" style="position: relative; left: 0; top: 1px; width: 25px; height: 23px">' +
						'<path d="M540.76,254.788L294.506,38.216 c-11.475-10.098-30.064-10.098-41.386,0L6.943,254.788 c-11.475,10.098-8.415,18.284,6.885,18.284 h75.964 v221.773 c0,12.087,9.945,22.108,22.108,22.108 h92.947V371.067 c0-12.087,9.945-22.108,22.109-22.108 h93.865 c12.239,0,22.108,9.792,22.108,22.108 v145.886 h92.947 c12.24,0,22.108-9.945,22.108-22.108 v-221.85 h75.965 C549.021,272.995,552.081,264.886,540.76,254.788z" />' +
					  '</svg>' +
					'</div>' +
				  '</a>' +
				'</div>' +
				'<div class="flexitem">' +
				  '<a class="nodecor" href="/' + urlPrefix + 'blog/">' +
					'<div class="navitem' + (section === "blog" ? ' active' : '') + '">' + blogTitle + '</div>' +
				  '</a>' +
				'</div>' +
				'<div class="flexitem">' +
				  '<a class="nodecor" href="/' + /*urlPrefix +*/ 'alsn/">' +
					'<div class="navitem' + (section === "alsn" ? ' active' : '') + '">ALSN</div>' +
				  '</a>' +
				'</div>' +
				'<div class="flexitem">' +
				  '<a class="nodecor" href="/' + /*urlPrefix +*/ 'eslp/">' +
					'<div class="navitem' + (section === "eslp" ? ' active' : '') + '">ESLP</div>' +
				  '</a>' +
				'</div>' +
				'<div class="flexitem">' +
				  '<a class="nodecor" href="/' + /*urlPrefix +*/ 'mdpn/">' +
					'<div class="navitem' + (section === "mdpn" ? ' active' : '') + '">MDPN</div>' +
				  '</a>' +
				'</div>' +
				'<div class="flexitem">' +
				  '<a class="nodecor" href="/' + /*urlPrefix +*/ 'p196/">' +
					'<div class="navitem' + (section === "p196" ? ' active' : '') + '">P196</div>' +
				  '</a>' +
				'</div>' +
			  '</div>' +
			'</td>' +
		  '</tr>' +
		'</table>'
	);
}

function getCurrentSection() {
	const url = window.location.pathname;
	if (url === "/" || url === "/ru" || url === "/ru/")
		return "home";

	const section = url.match(/^(\/ru)?\/(\w*)\//);
	return section ? section[2] : "";
}

// All pages: footer

function insertFooter() {
	const footer = document.getElementById("pageFooter");

	if (!footer)
		return;

	const lang = footer.getAttribute("data-lang");
	const urlPath = footer.getAttribute("data-url-path");

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

	footer.className = "column";

	footer.innerHTML =
		'<hr class="footer">' +
		'<table class="nospacing" style="width: 100%">' +
		  '<tr style="vertical-align: top">' +
			'<td class="footer">' +
			  '<div style="margin-right: 25px">' +
				'Copyright &copy;&nbsp;2019-2022 Dmitry&nbsp;Maslov' +
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
				'<a href="' + urlPath + '" title="' + languageTitle + '" style="color: inherit">' +
				  '<b>' + languageCode + '</b>' +
				'</a>' +
			  '</div>' +
			'</td>' +
		  '</tr>' +
		'</table>';
}

// Blog: links to articles

function insertTopLinks() {
	let prevText = "Previous article";
	let nextText = "Next article";

	if (document.documentElement.lang === "ru") {
		prevText = "Предыдущая статья";
		nextText = "Следующая статья";
	}

	document.write(
		'<div class="column bloglinks-top">' +
		  '<div style="display: inline-block">' +
			'<span id="link-prev-top">' +
			  '&laquo;&nbsp;' + prevText +
			'</span>' +
		  '</div>' +
		  '<div style="float: right">' +
			'<span id="link-next-top">' +
			  nextText + '&nbsp;&raquo;' +
			'</span>' +
		  '</div>' +
		'</div>'
	);
}

function insertBottomLinks() {
	let prevText = "Previous article";
	let nextText = "Next article";
	let topText = "To the top";

	if (document.documentElement.lang === "ru") {
		prevText = "Предыдущая статья";
		nextText = "Следующая статья";
		topText = "Наверх";
	}

	document.write(
		'<div class="column bloglinks-bottom">' +
		  '<span id="link-prev-bottom">' +
			'&laquo;&nbsp;' + prevText +
		  '</span>' +
		  '<span class="divider">&middot;</span>' +
		  '<span>' +
			'<a href="#">' + topText + '</a>' +
		  '</span>' +
		  '<span class="divider">&middot;</span>' +
		  '<span id="link-next-bottom">' +
			nextText + '&nbsp;&raquo;' +
		  '</span>' +
		'</div>'
	);
}

function setupBlogLinks() {
	const prevUrl = GetBlogLinkUrl("prevLink");
	setBlogLink("link-prev-top", prevUrl);
	setBlogLink("link-prev-bottom", prevUrl);

	const nextUrl = GetBlogLinkUrl("nextLink");
	setBlogLink("link-next-top", nextUrl);
	setBlogLink("link-next-bottom", nextUrl);
}

function GetBlogLinkUrl(elementId) {
	const e = document.getElementById(elementId);
	return e && e.getAttribute("data-url-path");
}

function setBlogLink(elementId, url) {
	const navElement = document.getElementById(elementId);

	if (navElement && url) {
		navElement.classList.add("link");
		navElement.onclick = function() {
			window.location.href = url;
		};
	}
}
