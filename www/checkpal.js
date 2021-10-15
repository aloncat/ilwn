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
	if (numFromUrl.length && numFromUrl != "#")
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

function getPageContent(language, number) {
	let result = "";

	// TODO
	result += '<p class="text">Проверяемое число: ';
	result += separateWithCommas(number);
	result += '.</p>';

	return result;
}

function separateWithCommas(value) {
	// TODO
	return value;
}

function isPalindrome(number) {
	// TODO
	return false;
}
