/* Blog main page */

var currentYear = 0;
var currentMonth = 0;
var currentLanguage = "en";

function toggleYear(year, lang) {
	if (lang === "en" || lang === "ru")
		currentLanguage = lang;

	for (let i = 2019; i <= 2021; ++i) {
		let yearTag = document.getElementById("nav-y-" + i);

		//yearTag.onclick = function() { toggleYear(i); };
		yearTag.title = i + ((currentLanguage == "ru") ? " год" : " year");

		yearTag.style.backgroundColor = null;
		yearTag.style.color = null;
		yearTag.style.cursor = null;
	}

	if (year >= 2019 && year <= 2021) {
		currentYear = year;
		let yearTag = document.getElementById("nav-y-" + year);

		yearTag.style.backgroundColor = "#fc6";
		yearTag.style.color = "inherit";
		yearTag.style.cursor = "pointer";
	}
}

function toggleMonth(month, lang) {
	if (lang === "en" || lang === "ru")
		currentLanguage = lang;

	let monthNamesEn = ["", "January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"];
	let monthNamesRu = ["", "Январь", "Февраль", "Март", "Апрель", "Май", "Июнь",
		"Июль", "Август", "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"];
	let monthNames = (currentLanguage === "ru") ?
		monthNamesRu : monthNamesEn;

	for (let i = 1; i <= 12; ++i) {
		let monthTag = document.getElementById("nav-m-" + i);

		//monthTag.onclick = function() { toggleMonth(i); };
		monthTag.textContent = (i < 10) ? "0" + i : i;
		monthTag.title = monthNames[i];

		monthTag.style.backgroundColor = null;
		monthTag.style.color = null;
		monthTag.style.cursor = null;
	}

	if (month >= 1 && month <= 12) {
		currentMonth = month;
		let monthTag = document.getElementById("nav-m-" + month);

		monthTag.textContent = monthNames[month];

		monthTag.style.backgroundColor = "#fc6";
		monthTag.style.color = "inherit";
		monthTag.style.cursor = "pointer";
	}
}

function initNavigation(lang) {
	toggleYear(2021, lang);
	toggleMonth(9, lang);
}

/* Post pages */

function initPostLink(elementId, url) {
	let navElement = document.getElementById(elementId);

	navElement.classList.add("postlink");
	navElement.onclick = function() {
		window.location.href = url;
	};
}

function initPrevPostLinks(url) {
	if (url && url != "") {
		initPostLink("link-prev-top", url);
		initPostLink("link-prev-bottom", url);
	}
}

function initNextPostLinks(url) {
	if (url && url != "") {
		initPostLink("link-next-top", url);
		initPostLink("link-next-bottom", url);
	}
}
