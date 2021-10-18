var currentNumber;

(function () {
	loadingText.style.display = "none";
	checkButton.style.removeProperty("display");

	numberInput.oninput = onNumberInput;
	numberInput.onkeydown = function(e) {
		if (e.keyCode === 13)
			onCheckButtonDown();
	};

	checkButton.onclick = onCheckButtonDown;

	const numFromUrl = window.location.hash.slice(1);
	setNewNumber(decodeURIComponent(numFromUrl));
})();

function onNumberInput() {
	const isCorrect = isCorrectNumber(numberInput.value);

	if (!numberInput.value.length || isCorrect) {
		numberInput.classList.remove("errorcolor");
		if (currentNumber && isCorrect && getNumberString(numberInput.value) === currentNumber)
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

		if (!number || number !== currentNumber) {
			currentNumber = null;
			contentBlock.innerHTML = "";
			resultsBlock.style.removeProperty("display");

			if (number && number !== "0") {
				errorText.style.display = "none";

				const language = document.documentElement.lang;
				const pageContent = getPageContent(language, number);
				contentBlock.innerHTML = pageContent.htmlData;
				pageContent.onContentReadyCb();

				currentNumber = number;
				onNumberInput();
			} else {
				errorText.style.removeProperty("display");
			}
		}
	}
}

function setNewNumber(number) {
	numberInput.value = number;
	onNumberInput();

	if (isCorrectNumber(number)) {
		document.activeElement.blur();
		onCheckButtonDown();
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

function getPageContent(language, number) {
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

	const scrollableId = "stepsBlock";
	let result = getBasicContent(language, data) +
		'<div class="column divider">&bull; &bull; &bull;</div>' +
		getStepDetails(data, scrollableId) +
		getFooterContent(language, data);

	return {
		htmlData: result,
		onContentReadyCb: function() {
			let scrollable = document.getElementById(scrollableId);
			scrollable.scrollLeft = scrollable.scrollWidth;
		}
	};
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
	let result = "";

	result += '<p class="text" style="word-wrap: break-word; text-align: left">';
	result += 'Проверяемое число: <span style="color: #03a">' + separateWithCommas(data.number) + '</span>.</p>';

	result += '<p class="text" style="word-wrap: break-word; text-align: left">';
	result += 'Длина числа: ' + data.number.length + '.<br>';
	result += 'Палиндром: ' + (isPalindrome(data.number) ? "да" : "нет") + '.<br>';
	result += 'Каноническое: ' + ((data.number === data.canonical) ? "да" : "нет") + '.</p>';

	return result;
}

function getStepDetails(data, scrollableId) {
	let result = "";

	result += '<div id="' + scrollableId + '"></div>';

	return result;
}

function getFooterContent(language, data) {
	let topText = "To the top";

	if (language === "ru") {
		topText = "Наверх";
	}

	return '' +
		'<div style="text-align: center">' +
		  '<a href="#">' + topText + '</a>' +
		'</div>';
}

function getLinkToThisPage(number) {
	const url = "https://dmaslov.me" + window.location.pathname;
	return number ? url + "#" + number : url;
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
