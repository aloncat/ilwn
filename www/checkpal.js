// Results for the last tested number
var currentData = null;

// Global constants
const MIN_SHOWN_STEPS = 30;
const MAX_SHOWN_STEPS = 350;
const FX_STAY_TIMEOUT = 200; // Milliseconds
const FX_FADE_TIMEOUT = "1.0s"; // Duration (CSS)
const ONCOPY_FX_COLOR = "#0c0"; // Color (CSS)
const URL_PREFIX = "https://dmaslov.me";

(function () {
	loadingText.style.display = "none";
	checkButton.style.removeProperty("display");

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
	const data = {};
	data.number = number;
	data.canonical = getLowestKin(number);
	data.highestKin = getHighestKin(number);
	data.totalKinCount = getKinCount(number);

	data.highestKnownStep = 289;
	const stepLimit = (number.length < 20) ? 350 : 650;

	const info = raaTillPalindrome(number, stepLimit);
	data.result = info.steps[info.iterationCount];
	data.iterationCount = info.iterationCount;
	data.isPalindrome = info.isPalindrome;
	data.steps = info.steps;

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
	if (number.length > 33) {
		// For numbers with more than 33 digits, it's possible to exceed (2^53 - 1) max safe integer
		// the JavaScript Number type can hold. A 34-digit number can have up to 9*10^16 kin numbers,
		// so to prevent going above "safe" value, we will return "udefined" for all such numbers
		return;
	}

	let count = 1;
	for (let i = 0, j = number.length - 1; i < j; ++i, --j) {
		let down = Math.min(number[i] - !i, 9 - number[j]);
		let up = Math.min(9 - number[i], number[j]);
		count *= 1 + down + up;
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

	if (data.isPalindrome && data.iterationCount > data.highestKnownStep) {
		result += '<p id="newRecord">Похоже, что Вы обнаружили <b>новый мировой рекорд</b>! ' +
			'Наиболее отложенный палиндром, известный на данный момент, разрешается за <b>' +
			data.highestKnownStep + '</b> шаг' + getCaseEnding(data.highestKnownStep, "", "а", "ов") +
			', тогда как Ваше число &mdash; за <b>' + data.iterationCount + '</b>! Пожалуйста, ' +
			'<a href="/contacts.html" target="_blank">свяжитесь со мной</a> и сообщите о Вашей находке!</p>';
	}

	result += '<p class="text">' + 'Указанное ' +
		data.number.length + '-значное число является';

	if (data.isPalindrome) {
		result += ' <b>отложенным палиндромом</b>, разрешающимся за <b>' + data.iterationCount +
			'</b> операц' + getCaseEnding(data.iterationCount, "ию", "ии", "ий") +
			' Перевернуть-И-Сложить. Длина его результирующего ' + 'палиндрома составляет <b>' +
			data.result.length + '</b> знак' + getCaseEnding(data.result.length, "", "а", "ов") + '.';
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
		result += 'Указанное число записано <b><span class="errorcolor">не</span> в канонической форме</b>. ' +
			'То есть существует меньшее число, которое можно получить из указанного путём изменения ' +
			'симметричных пар его цифр при условии сохранения их сумм, такое, что результаты одной ' +
			'операции Перевернуть-И-Сложить для обоих чисел будут совпадать.';
	}

	if (data.totalKinCount <= 1) {
		result += ' Для этого числа не существует других родственных чисел.';
	} else {
		const count = data.totalKinCount;
		const e = getCaseEnding(count, "ое", "ых");
		result += ' Для указанного числа существу' + getCaseEnding(count, "ет", "ют") + ' <b>' +
			separateWithCommas(count) + '</b> различн' + e + ' родственн' + e + ' чис' +
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
