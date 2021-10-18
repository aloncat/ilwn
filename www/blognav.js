// Blog: main page

let currentYear = 0;
let currentMonth = 0;
let currentLanguage = "en";

function toggleYear(year, lang) {
	for (let i = 2019; i <= 2021; ++i) {
		let yearTag = document.getElementById("nav-y-" + i);

		//yearTag.onclick = function() { toggleYear(i); };
		yearTag.title = i + ((currentLanguage === "ru") ? " год" : " year");

		yearTag.style.removeProperty("background-color");
		yearTag.style.removeProperty("color");
		yearTag.style.removeProperty("cursor");
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

		monthTag.style.removeProperty("background-color");
		monthTag.style.removeProperty("color");
		monthTag.style.removeProperty("cursor");
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

function initNavigation() {
	const lang = document.documentElement.lang;
	if (lang === "en" || lang === "ru")
		currentLanguage = lang;

	toggleYear(2021);
	toggleMonth(9);
}

// Blog: post pages

function setBlogPostLink(elementId, url) {
	let navElement = document.getElementById(elementId);

	if (navElement) {
		navElement.classList.add("postlink");
		navElement.onclick = function() {
			window.location.href = url;
		};
	}
}

function setPrevBlogPostUrl(url) {
	if (url) {
		setBlogPostLink("link-prev-top", url);
		setBlogPostLink("link-prev-bottom", url);
	}
}

function setNextBlogPostUrl(url) {
	if (url) {
		setBlogPostLink("link-next-top", url);
		setBlogPostLink("link-next-bottom", url);
	}
}
