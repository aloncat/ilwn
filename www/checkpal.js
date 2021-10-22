// Results for the last tested number (object)
var currentData;

// All known smallest numbers (array). Is initialized
// when calculateData() is called for the first time
var knownNumbers;

// Global constants
const MIN_SHOWN_STEPS = 30;
const MAX_SHOWN_STEPS = 350;
const FX_STAY_TIMEOUT = 200; // Milliseconds
const FX_FADE_TIMEOUT = "1.0s"; // Duration (CSS)
const ONCOPY_FX_COLOR = "#0c0"; // Color (CSS)
const URL_PREFIX = "https://dmaslov.me";
const SCRIPT_VERSION = "2021.10.22.4";

(function () {
	loadingText.style.display = "none";
	checkButton.style.removeProperty("display");
	showVersion(SCRIPT_VERSION, "versionText");

	numberInput.oninput = onNumberInput;
	numberInput.onkeydown = function(e) {
		if (e.keyCode === 13)
			onCheckButtonDown();
	};

	checkButton.onclick = onCheckButtonDown;

	const params = window.location.search.slice(1);
	// Treat the very first parameter as a number to test
	const numFromUrl = params.slice(params.indexOf("=") + 1);
	numberInput.value = decodeURIComponent(numFromUrl).trim();

	onNumberInput();
	if (!onCheckButtonDown())
		numberInput.focus();
})();

function onNumberInput() {
	const isCorrect = isCorrectNumber(numberInput.value);

	if (!numberInput.value.length || isCorrect) {
		numberInput.classList.remove("errorcolor");
		if (currentData && isCorrect && getNumberString(numberInput.value) === currentData.number)
			numberInput.style.color = "#39e";
		else
			numberInput.style.removeProperty("color");
	} else {
		numberInput.style.removeProperty("color");
		numberInput.classList.add("errorcolor");
	}

	checkButton.disabled = !isCorrect;
}

function onCheckButtonDown() {
	if (numberInput.value) {
		let number = isCorrectNumber(numberInput.value) &&
			getNumberString(numberInput.value);

		if (!number || !currentData || number !== currentData.number) {
			currentData = null;
			resultsBlock.style.removeProperty("display");
			const contentsElement = document.getElementById("contents");

			if (number && number !== "0") {
				errorText.style.display = "none";

				currentData = calculateData(number);
				const language = document.documentElement.lang;
				const pageContent = getPageContent(language, currentData);

				contentsElement.innerHTML = pageContent.htmlData;
				pageContent.onContentReadyCb();
				onNumberInput();
				return true;
			} else {
				contentsElement.innerHTML = "";
				errorText.style.removeProperty("display");
			}
		} else {
			// If "Check" button or Enter key is pressed for the same number, scroll the
			// results to the right to make them visible when page is scrolled to the top
			let scrollable = document.getElementById(currentData.scrollableId);
			scrollable.scrollLeft = scrollable.scrollWidth;
		}
	}

	return false;
}

function showVersion(version, elementId) {
	const language = document.documentElement.lang;
	const verElement = document.getElementById(elementId);

	if (verElement && version) {
		verElement.innerText = ((language === "ru") ?
			"Версия скрипта: " : "Script version: ") +
			version;
	}
}

function isCorrectNumber(value) {
	let hasDigits = false;
	const s = value.trim();
	for (let i = 0; i < s.length; ++i) {
		if (s[i] >= "0" && s[i] <= "9")
			hasDigits = true;
		else if (", \'".indexOf(s[i]) === -1)
			return false;
	}
	return hasDigits;
}

function getNumberString(value) {
	let digits = [];
	for (let i = 0; i < value.length; ++i) {
		if ((value[i] > "0" && value[i] <= "9") || (value[i] === "0" && digits.length))
			digits.push(value[i]);
	}
	return digits.length ? digits.join("") : "0";
}

function calculateData(number) {
	console.log("Starting calculation for number: " +
		separateWithCommas(number));

	if (!knownNumbers) {
		knownNumbers = getKnownNumbers();
		console.log("Known smallest numbers loaded, highest step: " +
			(knownNumbers.length - 1));
	}

	const data = {};
	data.number = number;
	data.canonical = getLowestKin(number);
	data.highestKin = getHighestKin(number);
	data.totalKinCount = getKinCount(number);

	const stepLimit = (number.length < 20) ? 350 : 650;
	const info = raaTillPalindrome(number, stepLimit);
	data.result = info.steps[info.iterationCount];
	data.iterationCount = info.iterationCount;
	data.isPalindrome = info.isPalindrome;
	data.steps = info.steps;

	console.log("Calculation finished");
	return data;
}

function getPageContent(language, data) {
	const result = {};
	data.scrollableId = "stepsBlock";
	result.htmlData = getBasicContent(language, data) +
		getStepDetailsContent(language, data) +
		getFooterContent(language, data);

	result.onContentReadyCb = function() {
		let scrollable = document.getElementById(data.scrollableId);
		scrollable.scrollLeft = scrollable.scrollWidth;
	};

	return result;
}

function getLowestKin(number) {
	let digits = number.split("");
	for (let i = 0, j = digits.length - 1; i < j; ++i, --j) {
		const diff = Math.min(digits[i] - !i, 9 - digits[j]);
		digits[i] -= diff;
		digits[j] = +digits[j] + diff;
	}
	return digits.join("");
}

function getHighestKin(number) {
	let digits = number.split("");
	for (let i = 0, j = digits.length - 1; i < j; ++i, --j) {
		const diff = Math.min(9 - digits[i], digits[j]);
		digits[i] = +digits[i] + diff;
		digits[j] -= diff;
	}
	return digits.join("");
}

function getKinCount(number) {
	let count = 1;
	for (let i = 0, j = number.length - 1; i < j; ++i, --j) {
		let down = Math.min(number[i] - !i, 9 - number[j]);
		let up = Math.min(9 - number[i], number[j]);

		count *= 1 + down + up;
		if (!Number.isSafeInteger(count))
			return Infinity;
	}
	return count;
}

function raaTillPalindrome(value, maxSteps) {
	let result = {
		iterationCount: 0,
		isPalindrome: false,
		steps: [ "" + value ]
	};

	let current = "" + value;
	while (result.iterationCount < maxSteps) {
		current = reverseAndAdd(current);
		result.steps.push(current);
		++result.iterationCount;

		if (isPalindrome(current)) {
			result.isPalindrome = true;
			break;
		}
	}

	return result;
}

function reverseAndAdd(number) {
	let result = [], v = 0;
	const last = number.length - 1;
	for (let i = 0; i <= last; ++i) {
		v += +number[i] + +number[last - i];
		result.push("" + (v % 10));
		v = v > 9;
	}

	if (v) {
		result.push(1);
	}

	return result.reverse().join("");
}

function isPalindrome(number) {
	if (number) {
		for (let i = 0, j = number.length - 1; i < j; ++i, --j) {
			if (number[i] !== number[j])
				return false;
		}
		return true;
	}
	return false;
}

function getBasicContent(language, data) {
	let result = '<p class="text bignum">' +
		((language === "ru") ? 'Проверяемое число: ' : 'Tested number: ') +
		'<span class="specnum">' + separateWithCommas(data.number) + '</span>.</p>';

	let smallestKnown;
	let isNewSmallest = false;
	let isSmallestKnown = false, isRsn = false;
	const highestKnownStep = knownNumbers.length - 1;

	if (data.isPalindrome && data.iterationCount > 0) {
		smallestKnown = knownNumbers[data.iterationCount];
		isNewSmallest = !smallestKnown || data.number.length < smallestKnown.number.length ||
			(data.number.length === smallestKnown.number.length && data.number < smallestKnown.number);
		isSmallestKnown = isNewSmallest || data.number === smallestKnown.number;
		isRsn = isSmallestKnown && !isNewSmallest && smallestKnown.rsn;
	}

	if (data.isPalindrome && data.iterationCount > highestKnownStep) {
		result += '<p id="newNumber">Похоже, что Вы обнаружили <b>новый мировой рекорд</b>! ' +
			'Наиболее отложенный палиндром, известный на данный момент, разрешается за <b>' +
			highestKnownStep + '</b> итерац' + getCaseEnding(highestKnownStep, "ию", "ии", "ий") +
			', тогда как Ваше число &mdash; за <b>' + data.iterationCount + '</b>! Пожалуйста, ' +
			'<a href="/contacts.html" target="_blank">свяжитесь со мной</a> и сообщите о Вашей находке!</p>';
	}
	else if (isNewSmallest) {
		result += '<p id="newNumber">Похоже, что Вы обнаружили число, которое <b>меньше</b> ' +
			'известного на данный момент <i>наименьшего</i> отложенного палиндрома, разрешающегося за <b>' +
			data.iterationCount + '</b> итерац' + getCaseEnding(data.iterationCount, "ию", "ии", "ий") +
			'! Пожалуйста, <a href="/contacts.html" target="_blank">свяжитесь со мной</a> и ' +
			'сообщите о Вашей находке!</p>';
	}

	result += '<p class="text">' + 'Указанное ' +
		data.number.length + '-значное число является';

	if (data.isPalindrome) {
		if (isSmallestKnown) {
			result += isRsn ?
				' <span class="rsnum">достоверно наименьшим</span>' :
				' <span class="sknum">наименьшим известным</span>';
		}
		result += ' <b>отложенным палиндромом</b>, разрешающимся за <b>' + data.iterationCount +
			'</b> операц' + getCaseEnding(data.iterationCount, "ию", "ии", "ий") +
			' Перевернуть-И-Сложить. Длина его результирующего ' + 'палиндрома составляет <b>' +
			data.result.length + '</b> знак' + getCaseEnding(data.result.length, "", "а", "ов");

			let raaResult = data.result;
			if (data.result.length <= 20) {
				result += ': <span id="raaResult">' + raaResult + '</span> (';
			} else {
				raaResult = data.result.substr(0, 20);
				result += ' (первые 20 из них: <span id="raaResult">' + raaResult + '</span>, ';
			}
			result += '<a class="jsanchor" onclick="copyRaaResultToClipboard(\'' + raaResult +
				'\', \'raaResult\')">скопировать в буфер обмена</a>).';
	} else {
		result += ' <b>числом Лишрел</b>. Мы только что выполнили над ним ' + data.iterationCount +
			' операц' + getCaseEnding(data.iterationCount, "ию", "ии", "ий") + ' Перевернуть-И-Сложить, но, ' +
			'достигнув длины ' + data.result.length + ' знак' + getCaseEnding(data.result.length, "", "а", "ов") +
			', оно так и не стало палиндромом.';
		result += ' Этого количества операций вполне достаточно для того, чтобы утверждать, что ' +
			'указанное число действительно является числом Лишрел.';
	}

	if (isPalindrome(data.number)) {
		result += ' Кроме того, указанное число само является <b>палиндромом</b>.';
	}

	result += '</p><p class="text">';

	if (data.number === data.canonical) {
		result += 'Указанное число является <b>каноническим</b>, то есть наименьшим числом, которое ' +
			'можно получить путём изменения симметричных пар его цифр при условии сохранения их сумм.';
	} else {
		result += 'Указанное число записано <b><span class="errorcolor">не</span> в каноническом виде</b>. ' +
			'То есть существует меньшее число, которое можно получить из указанного путём изменения ' +
			'симметричных пар его цифр при условии сохранения их сумм, такое, что результаты одной ' +
			'операции Перевернуть-И-Сложить для обоих чисел будут совпадать.';
	}

	if (data.totalKinCount <= 1) {
		result += ' Для этого числа не существует других родственных чисел.';
	} else {
		const count = data.totalKinCount;
		const e = getCaseEnding(count, "ое", "ых");
		const amountText = (count === Infinity) ?
			'существует более 9&times;10<span class="tiny"><sup>15</sup></span>' : 'существу' +
			getCaseEnding(count, "ет", "ют") + ' <b>' + separateWithCommas(count) + '</b>';
		result += ' Для указанного числа ' + amountText + ' различн' + e + ' родственн' + e + ' чис' +
			getCaseEnding(count, "ло", "ла", "ел") + ', котор' + getCaseEnding(count, "ое", "ые") +
			' после всего одной операции Перевернуть-И-Сложить да' + getCaseEnding(count, "ёт", "ют") +
			' одинаковый результат.';
	}

	if (data.number !== data.canonical) {
		result += '</p><p class="text bignum">' +
			'Канонический вид числа: <a href="' + getLinkToThisPage(data.canonical, "") +
			'"><i>' + separateWithCommas(data.canonical) + '</i></a>.';
	}

	if (data.totalKinCount > 1) {
		result += '</p><p class="text bignum">' +
			'Наибольшее из всех родственных чисел: <span class="specnum">' +
			separateWithCommas(data.highestKin) + '</span>.';
	}

	if (smallestKnown && !isSmallestKnown && !isNewSmallest) {
		result += '</p><p class="text">' +
			'Указанный отложенный палиндром <b>не является наименьшим</b> известным ' +
			'числом, которое разрешается за ' + data.iterationCount + ' операц' +
			getCaseEnding(data.iterationCount, "ию", "ии", "ий") + '. Наименьшее ' +
			'известное число: <a href="' + getLinkToThisPage(smallestKnown.number, "") +
			'"><i>' + separateWithCommas(smallestKnown.number) + '</i></a>.';
	}

	if (data.isPalindrome && data.iterationCount > 1) {
		const firstStep = getLowestKin(data.steps[1]);
		result += '</p><p class="text bignum">' +
			'Результат 1 итерации (отложенный палиндром): ' +
			'<a href="' + getLinkToThisPage(firstStep, "") +
			'"><i>' + separateWithCommas(firstStep) + '</i></a>.';
	}

	result += '</p>';

	result += '<div class="small">Ссылка на эту страницу (<a class="jsanchor" ' +
		'onclick="copyPageLinkToClipboard()">скопировать в буфер обмена</a>):</div>' +
		'<div><input id="pageLink" class="small" type="text" readonly value="' +
		getLinkToThisPage(data.number) + '"></input></div>';

	return result;
}

function getStepDetailsContent(language, data) {
	// This property contains a string with HTML code for "show more" link. If there
	// should be no link (because we have shown all the iterations), it will be null
	data.showMoreAnchor = null;

	// Property data.maxStepsToShow is initially undefined. But it will be initialized
	// with the desired number of iterations to show, if user clicks "show more" link
	const maxStepsToShow = data.maxStepsToShow || MIN_SHOWN_STEPS;
	const stepsToShow = data.isPalindrome ? data.iterationCount :
		Math.min(maxStepsToShow, data.iterationCount);

	let result = '<div>';
	if (data.isPalindrome) {
		result += (language === "ru") ?
			'Ниже выведены все шаги до достижения указанным числом палиндрома.' :
			'Below are shown all the iterations until the number reaches a palindrome.';
	} else {
		result += (language === "ru") ?
			'Ниже выведен' + getCaseEnding(stepsToShow, "", "ы") + ' только перв' +
			getCaseEnding(stepsToShow, "ый", "ые") + ' <b>' + stepsToShow +
				'</b> шаг' + getCaseEnding(stepsToShow, "", "а", "ов") :
			'Below are shown only the first <b>' + stepsToShow + '</b> iterations';
		if (stepsToShow < data.iterationCount && stepsToShow < MAX_SHOWN_STEPS) {
			const moreStepsToShow = (stepsToShow < 100) ? 100 : MAX_SHOWN_STEPS;
			data.showMoreAnchor = '<a class="jsanchor" onclick="showMoreSteps(' + moreStepsToShow + ')">';
			result += ' (' + data.showMoreAnchor + ((language === "ru") ?
				'показать больше' : 'show more') + '</a>)';
		}
		result += '.';
	}
	result += '</div>';

	result += '<div id="' + data.scrollableId +'" class="scrollable">' +
		'<div class="palsteps">';

	for (let i = 0; i <= stepsToShow; ++i) {
		if (i < stepsToShow) {
			result += '<span class="stepmarker" style="position: relative; left: -5px; top: 8px">#' +
				(i + 1) + '</span>';
		}

		result += '&nbsp; ';
		const n = data.steps[i];
		const isLast = i === stepsToShow && data.isPalindrome;
		const lineStyle = isLast ? "solid #999" : "dotted #ccc";
		result += i ? '<span style="border-top: 2px ' + lineStyle + '">' : '<span>';
		result += (isLast ? '<b>' + n + '</b>' : n) + '</span><br>';

		if (i < stepsToShow) {
			const r = n.split("").reverse().join("");
			result += '<span style="color: #999; position: relative; left: 8px; top: -8px">+</span>' +
				'&nbsp;<span style="color: #c50">' + r + '</span><br>';
		}
	}

	result += '</div></div>';

	return result;
}

function getFooterContent(language, data) {
	let result = '<div class="footer"><a class="jsanchor" onclick="copyStepsToClipboard()">' +
		((language === "ru") ? 'Скопировать в буфер обмена' : 'Copy to clipboard') + '</a>';

	if (data.showMoreAnchor) {
		result += '<span class="divider">&middot;</span>' +
			'<span>' + data.showMoreAnchor + ((language === "ru") ?
			'Показать больше шагов' : 'Show more iterations') + '</a></span>';
	}

	result += '<span class="divider">&middot;</span><span><a href="#">' +
		((language === "ru") ? 'Наверх' : 'To the top') + '</a></span></div>';

	return result;
}

function separateWithCommas(value, separator) {
	if (separator === undefined)
		separator = ",";

	let result = [];
	const number = "" + value;
	const size = number.length;
	for (let p = 0, l = size % 3; p < size;) {
		result.push(p ? String(separator) : "");
		for (let i = (p || !l) ? 3 : l; i; --i)
			result.push(number[p++]);
	}
	return result.join("");
}

function getCaseEnding(value, one, twofour, other) {
	if (other === undefined)
		other = twofour;

	return ((value % 10) && (value % 10 < 5) && ((value % 100 < 11) || (value % 100 > 19))) ?
		(value % 10 === 1) ? one : twofour : other;
}

function getLinkToThisPage(number, host) {
	if (host === undefined)
		host = URL_PREFIX;

	const url = host + window.location.pathname;
	return number ? url + "?n=" + number : url;
}

function copyRaaResultToClipboard(textToCopy, elementId) {
	var hideTimeOut;
	const raaResult = document.getElementById(elementId);
	copyTextToClipboard(textToCopy, function() {
		if (raaResult) {
			raaResult.style.transition = "none";
			raaResult.style.color = ONCOPY_FX_COLOR;
			hideTimeout = setTimeout(function() {
				hideTimeOut = null;
				raaResult.style.transition = "color " + FX_FADE_TIMEOUT;
				raaResult.style.removeProperty("color");
			}, 2 * FX_STAY_TIMEOUT);
		}
	});
}

function copyPageLinkToClipboard() {
	const pageLink = document.getElementById("pageLink");
	const linkText = pageLink ? pageLink.value : getLinkToThisPage();

	var hideTimeOut;
	copyTextToClipboard(linkText, function() {
		if (pageLink) {
			pageLink.style.transition = "none";
			pageLink.style.borderColor = ONCOPY_FX_COLOR;
			hideTimeout = setTimeout(function() {
				hideTimeOut = null;
				pageLink.style.transition = "border " + FX_FADE_TIMEOUT;
				pageLink.style.removeProperty("border-color");
			}, FX_STAY_TIMEOUT);
		}
	});
}

function copyStepsToClipboard() {
	if (currentData) {
		const stepsToShow = currentData.isPalindrome ? currentData.iterationCount :
			Math.min(currentData.iterationCount, MAX_SHOWN_STEPS);

		let result = [];
		for (let i = 0; i <= stepsToShow; ++i) {
			const n = currentData.steps[i];
			if (i < stepsToShow) {
				const r = n.split("").reverse().join("");
				result.push('#' + (i + 1) + ':\n ' + n + '\n+' + r + '\n');
			} else {
				result.push((n.length === currentData.steps[i - 1].length) ?
					'===\n ' + n + '\n' : '===\n' + n + '\n');
			}
		}

		var hideTimeOut;
		copyTextToClipboard(result.join(""), function() {
			const stepsBlock = document.getElementById(currentData.scrollableId);
			if (stepsBlock) {
				stepsBlock.style.transition = "none";
				stepsBlock.style.borderColor = ONCOPY_FX_COLOR;
				hideTimeout = setTimeout(function() {
					hideTimeOut = null;
					stepsBlock.style.transition = "border " + FX_FADE_TIMEOUT;
					stepsBlock.style.removeProperty("border-color");
				}, FX_STAY_TIMEOUT);
			}
		});
	}
}

function copyTextToClipboard(textToCopy, successCb) {
	if (navigator.clipboard) {
		navigator.clipboard.writeText(textToCopy).then(successCb, function(error) {
			console.error('Could not copy text into clipboard:', error);
		});
	} else {
		// Fallback for IE mostly
		let textArea = document.createElement("textarea");

		textArea.style.position = "fixed";
		textArea.style.left = 0;
		textArea.style.top = 0;
		textArea.style.width = "2em";
		textArea.style.height = "2em";
		textArea.style.padding = 0;
		textArea.style.border = "none";
		textArea.style.outline = "none";
		textArea.style.boxShadow = "none";
		textArea.style.background = "transparent";
		textArea.style.color = "#fff0";

		textArea.value = textToCopy;
		document.body.appendChild(textArea);

		textArea.focus();
		textArea.select();
		const hasCopied = document.execCommand('copy')
		document.body.removeChild(textArea);

		if (hasCopied && successCb)
			successCb();
	}
}

function showMoreSteps(stepsToShow) {
	if (currentData) {
		currentData.maxStepsToShow = Math.min(currentData.iterationCount,
			Math.max(stepsToShow || 100, MIN_SHOWN_STEPS));

		const language = document.documentElement.lang;
		const pageContent = getPageContent(language, currentData);

		const contentsElement = document.getElementById("contents");
		contentsElement.innerHTML = pageContent.htmlData;
		pageContent.onContentReadyCb();
	}
}

function getKnownNumbers() {
	const p = [];
	function pal(number, rsn) {
		const result = {};
		result.number = "" + number;
		result.rsn = (rsn !== undefined) ? rsn : true;
		return result;
	}

	// 1 digit
	p[1] = pal("1");
	p[2] = pal("5");

	// 2 digits
	p[3] = pal("59");
	p[4] = pal("69");
	p[6] = pal("79");
	p[24] = pal("89");

	// 3 digits
	p[5] = pal("166");
	p[7] = pal("188");
	p[8] = pal("193");
	p[10] = pal("829");
	p[11] = pal("167");
	p[14] = pal("849");
	p[15] = pal("177");
	p[16] = pal("999");
	p[17] = pal("739");
	p[19] = pal("989");
	p[22] = pal("869");
	p[23] = pal("187");

	// 4 digits
	p[9] = pal("1397");
	p[12] = pal("2069");
	p[13] = pal("1797");
	p[18] = pal("1798");
	p[20] = pal("6999");
	p[21] = pal("1297");

	// 5 digits
	p[25] = pal("10797");
	p[26] = pal("10853");
	p[27] = pal("10921");
	p[28] = pal("10971");
	p[29] = pal("13297");
	p[30] = pal("10548");
	p[31] = pal("13293");
	p[32] = pal("17793");
	p[33] = pal("20889");
	p[37] = pal("80359");
	p[38] = pal("13697");
	p[39] = pal("10794");
	p[40] = pal("15891");
	p[47] = pal("70759");
	p[52] = pal("70269");
	p[53] = pal("10677");
	p[54] = pal("10833");
	p[55] = pal("10911");

	// 6 digits
	p[34] = pal("700269");
	p[35] = pal("106977");
	p[36] = pal("108933");
	p[45] = pal("600259");
	p[46] = pal("131996");
	p[50] = pal("600279");
	p[51] = pal("141996");
	p[57] = pal("600579");
	p[58] = pal("147996");
	p[59] = pal("178992");
	p[60] = pal("190890");
	p[63] = pal("600589");
	p[64] = pal("150296");

	// 7 digits
	p[41] = pal("1009227");
	p[42] = pal("1007619");
	p[43] = pal("1009246");
	p[44] = pal("1008628");
	p[48] = pal("1007377");
	p[49] = pal("1001699");
	p[56] = pal("1009150");
	p[61] = pal("1058921");
	p[62] = pal("1050995");
	p[65] = pal("1003569");
	p[66] = pal("1036974");
	p[67] = pal("1490991");
	p[68] = pal("3009179");
	p[69] = pal("1008595");
	p[70] = pal("1064912");
	p[75] = pal("1998999");
	p[77] = pal("7008429");
	p[78] = pal("1000689");
	p[79] = pal("1005744");
	p[80] = pal("1007601");
	p[82] = pal("7008899");
	p[96] = pal("9008299");

	// 8 digits
	p[71] = pal("10905963");
	p[72] = pal("10069785");
	p[73] = pal("10089342");
	p[74] = pal("11979990");
	p[76] = pal("10029372");
	p[81] = pal("10029826");
	p[83] = pal("16207990");
	p[94] = pal("90000589");
	p[95] = pal("10309988");

	// 9 digits
	p[84] = pal("100389898");
	p[85] = pal("100055896");
	p[86] = pal("110909992");
	p[87] = pal("160009490");
	p[88] = pal("800067199");
	p[89] = pal("151033997");
	p[90] = pal("100093573");
	p[91] = pal("103249931");
	p[92] = pal("107025910");
	p[93] = pal("180005498");
	p[97] = pal("100239862");
	p[98] = pal("140669390");

	// 10 digits
	p[99] = pal("1090001921");
	p[100] = pal("7007009909");
	p[101] = pal("1009049407");
	p[103] = pal("9000046899");
	p[104] = pal("1050027948");
	p[105] = pal("1304199693");
	p[108] = pal("5020089949");
	p[109] = pal("1005499526");

	// 11 digits
	p[102] = pal("10000505448");
	p[106] = pal("10000922347");
	p[107] = pal("10000696511");
	p[110] = pal("10701592943");
	p[111] = pal("10018999583");
	p[112] = pal("10000442119");
	p[113] = pal("10000761554");
	p[114] = pal("10084899970");
	p[115] = pal("10006198250");
	p[116] = pal("18060009890");
	p[117] = pal("11400245996");
	p[118] = pal("16002897892");
	p[119] = pal("18317699990");
	p[122] = pal("37000488999");
	p[123] = pal("10050289485");
	p[130] = pal("90000626389");
	p[131] = pal("10000853648");
	p[132] = pal("13003696093");
	p[133] = pal("10050859271");
	p[134] = pal("10287799930");
	p[135] = pal("10000973037");
	p[136] = pal("10600713933");
	p[137] = pal("10942399911");
	p[144] = pal("60000180709");
	p[145] = pal("11009599796");
	p[146] = pal("16000097392");
	p[147] = pal("10031199494");
	p[148] = pal("10306095991");
	p[149] = pal("10087799570");

	// 12 digits
	p[120] = pal("100900509906");
	p[121] = pal("100000055859");
	p[124] = pal("104000146950");
	p[129] = pal("180005998298");
	p[142] = pal("300000185539");
	p[143] = pal("100001987765");

	// 13 digits
	p[125] = pal("1000007614641");
	p[126] = pal("1000043902320");
	p[127] = pal("1000006653746");
	p[128] = pal("1000005469548");
	p[139] = pal("4000096953659");
	p[140] = pal("1332003929995");
	p[141] = pal("1000201995662");
	p[183] = pal("6000008476379");
	p[184] = pal("1200004031698");
	p[185] = pal("1631002019993");
	p[186] = pal("1000006412206");
	p[187] = pal("1090604591930");
	p[188] = pal("1600005969190");

	// 14 digits
	p[138] = pal("10090899969901");
	p[181] = pal("40000004480279");
	p[182] = pal("14104229999995");

	// 15 digits
	p[150] = pal("100000109584608");
	p[151] = pal("100000098743648");
	p[152] = pal("100004789906151");
	p[153] = pal("100079239995161");
	p[154] = pal("100389619999030");
	p[155] = pal("200000729975309");
	p[156] = pal("107045067996994");
	p[157] = pal("105420999199982");
	p[158] = pal("101000269830970");
	p[159] = pal("104000047066970");
	p[163] = pal("700000001839569");
	p[164] = pal("100000050469737");
	p[165] = pal("101000789812993");
	p[166] = pal("100907098999571");
	p[167] = pal("100017449991820");
	p[169] = pal("890000023937399");
	p[170] = pal("100009989989199");
	p[171] = pal("101507024989944");
	p[172] = pal("107405139999943");
	p[173] = pal("100057569996821");
	p[174] = pal("103500369729970");
	p[178] = pal("900000076152049");
	p[179] = pal("100000439071028");
	p[180] = pal("120000046510993");
	p[189] = pal("103000015331997");
	p[190] = pal("100617081999573");
	p[191] = pal("100009029910821");
	p[192] = pal("107000020928910");
	p[198] = pal("100000090745299");
	p[199] = pal("102000149322944");
	p[200] = pal("130000074931591");
	p[201] = pal("100120849299260");

	// 16 digits
	p[161] = pal("6000000039361479");
	p[162] = pal("1421000069679996");
	p[168] = pal("1000650998992311");
	p[175] = pal("1000002899436401");
	p[176] = pal("1000100396492200");
	p[177] = pal("1400000027672498");
	p[193] = pal("7090000039309919");
	p[194] = pal("1000050048994957");
	p[195] = pal("1003000024749923");
	p[196] = pal("1000803019495711");
	p[197] = pal("1030020097997900");

	// 17 digits
	p[160] = pal("10000000730931027");
	p[206] = pal("20005000862599819");
	p[207] = pal("11450360479969994");
	p[208] = pal("10009000275899569");
	p[209] = pal("10059430139999234");
	p[210] = pal("12179702595999991");
	p[229] = pal("79000000445783599");
	p[230] = pal("10000000767846797");
	p[231] = pal("10000000673402336");
	p[232] = pal("10000000525586206");
	p[233] = pal("10005000760994249");
	p[234] = pal("10030503899969524");
	p[235] = pal("12000009694736291");
	p[236] = pal("10442000392399960");

	// 18 digits
	p[202] = pal("195030047999791993");
	p[203] = pal("100000078999111766");
	p[204] = pal("100710000333399973");
	p[205] = pal("100000002973751552");
	p[211] = pal("100000277999334202");
	p[212] = pal("110300361999869090");
	p[213] = pal("300000000128545799");
	p[214] = pal("104300000514769945");
	p[215] = pal("100700000509609622");
	p[216] = pal("120906490499909290");
	p[218] = pal("900040000881499569");
	p[219] = pal("100072100489999238");
	p[220] = pal("121506542999979993");
	p[221] = pal("106096507979997951");
	p[222] = pal("100980800839699830");
	p[227] = pal("600000000606339049");
	p[228] = pal("170500000303619996");

	// 19 digits
	p[217] = pal("1000000038990407538");
	p[224] = pal("9000000000255353839");
	p[225] = pal("1000000005577676468");
	p[226] = pal("1060000000523124995");
	p[258] = pal("3000000022999288679");
	p[259] = pal("1000000079994144385");
	p[260] = pal("1003062289999939142");
	p[261] = pal("1186060307891929990");

	// 20 digits
	p[223] = pal("10000000039513841287");
	p[253] = pal("70000000000507277299");
	p[254] = pal("10200000000708183947");
	p[255] = pal("10022000904998799523");
	p[256] = pal("10000000039395795416");
	p[257] = pal("10200000000065287900");

	// 21 digits
	p[251] = pal("500000060001199990549");
	p[252] = pal("100000081000999940726");

	// 22 digits
	p[238] = pal("3000800040089968999539");
	p[239] = pal("1000950870044993999265");
	p[240] = pal("1000000040796912297132");
	p[249] = pal("1990000200024599902999");
	p[250] = pal("1000000630016999401994");

	// 23 digits
	p[237] = pal("10060095109893999779862", false);
	p[241] = pal("13200130000005299908990", false);
	p[245] = pal("30370000300182999249979", false);
	p[246] = pal("10000000000500075338778");
	p[247] = pal("10000000380589999534342");
	p[248] = pal("10500020000017719681951", false);
	p[286] = pal("10000006000090499474959");
	p[287] = pal("10000008700094994282974");
	p[288] = pal("12000700000025339936491", false);
	p[289] = pal("10036069400174999499950", false);

	// 24 digits
	p[242] = pal("500000092000432999874429", false);
	p[243] = pal("100005000200219594932216", false);
	p[244] = pal("111120050011997799699992", false);
	p[282] = pal("140589900069999589999999", false);
	p[283] = pal("100800000089999249989964", false);
	p[284] = pal("140000030099999099448991", false);
	p[285] = pal("105422501000994959999960", false);

	// 25 digits
	p[278] = pal("1070409080308997989896919", false);
	p[279] = pal("1090749490009463999992904", false);
	p[280] = pal("1102079702329999899979891", false);
	p[281] = pal("1000489846069959999995450", false);

	// All others (computed)
	p[262] = pal("1020010060205799999798929938933", false);
	p[263] = pal("1064200200047899999954799499911", false);
	p[264] = pal("200831800008099998950439999599", false);
	p[265] = pal("153000022030000994899998568994", false);
	p[266] = pal("59351407000006693175494999999", false);
	p[267] = pal("10000030300120379932979657696", false);
	p[268] = pal("7240048000304039790299923999", false);
	p[269] = pal("1072079001047019959099909957", false);
	p[270] = pal("1090000010370197994949805923", false);
	p[271] = pal("1000100303090053959499392911", false);
	p[272] = pal("1002025030392029995959949900", false);
	p[273] = pal("100000600000002993299831999", false);
	p[274] = pal("10021001020200979198799998", false);
	p[275] = pal("10560445100609935999949993", false);
	p[276] = pal("10800077530059719999627941", false);
	p[277] = pal("10041388761007399999999820", false);

	return p;
}
