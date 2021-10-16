function enableInput() {
	loadingText.style.display = "none";
	checkButton.style.display = null;

	numberInput.oninput = onNumberInput;
	numberInput.onkeydown = function(e) {
		if (e.keyCode === 13)
			onCheckButtonDown();
	}

	checkButton.onclick = onCheckButtonDown;

	const numFromUrl = window.location.hash;
	if (numFromUrl.length && numFromUrl !== "#")
		numberInput.value = numFromUrl.slice(1);
	onNumberInput();

	if (isCorrectNumber(numberInput.value))
		onCheckButtonDown();
}

function onNumberInput() {
	const isCorrect = isCorrectNumber(numberInput.value);

	const useNormalColor = !numberInput.value.length || isCorrect;
	numberInput.style.color = useNormalColor ? "inherit" : "#e00";

	checkButton.disabled = !isCorrect;
}

function onCheckButtonDown() {
	if (numberInput.value.length) {
		let number = null;
		if (isCorrectNumber(numberInput.value)) {
			number = getNumberString(numberInput.value);
		}

		if (number && number !== "0") {
			resultsBlock.style.display = null;
			errorText.style.display = "none";
			contentBlock.innerHTML = null;

			const language = document.documentElement.lang;
			contentBlock.innerHTML = getPageContent(language, number);
		} else {
			resultsBlock.style.display = null;
			errorText.style.display = null;
			contentBlock.innerHTML = null;
		}
	}
}

function isCorrectNumber(value) {
	let isCorrect = true;
	let hasDigits = false;

	if (value.length) {
		const s = value.trim();
		for (let i = 0; i < s.length; ++i) {
			if (s[i] >= '0' && s[i] <= '9')
				hasDigits = true;
			else if (!(", \'".includes(s[i]))) {
				isCorrect = false;
				break;
			}
		}
	}

	return hasDigits && isCorrect;
}

function getNumberString(value) {
	let result = "0";
	for (let i = 0; i < value.length; ++i) {
		if (value[i] >= '0' && value[i] <= '9') {
			if (result !== "0")
				result += value[i];
			else
				result = value[i];
		}
	}

	return result;
}

function getPageContent(language, number) {
	const canonical = getCanonicalForm(number);

	let result = String();
	result += '<p class="text" style="word-wrap: break-word; text-align: left">';
	result += 'Проверяемое число: <span style="color: #03a">' + separateWithCommas(number) + '</span>.</p>';

	result += '<p class="text" style="word-wrap: break-word; text-align: left">';
	result += 'Длина числа: ' + number.length + '.<br>';
	result += 'Палиндром: ' + (isPalindrome(number) ? "да" : "нет") + '.<br>';
	result += 'Каноническое: ' + ((number === canonical) ? "да" : "нет") + '.</p>';

	return result;
}

function getCanonicalForm(number) {
	let digits = number.split("");
	for (let i = 0, j = digits.length - 1; i < j; ++i, --j) {
		while ((digits[i] > '1' || (digits[i] === '1' && i)) && digits[j] < '9') {
			--digits[i];
			++digits[j];
		}
	}

	return digits.join("");
}

function separateWithCommas(value, separator = ",") {
	let result = String();
	const size = value.length;
	for (let p = 0, l = size % 3; p < size;) {
		if (p && separator)
			result += separator;
		for (let i = (p || !l) ? 3 : l; i; --i)
			result += value[p++];
	}

	return result;
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
