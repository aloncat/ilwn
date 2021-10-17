function enableInput() {
	loadingText.style.display = "none";
	checkButton.style.display = null;

	numberInput.oninput = onNumberInput;
	numberInput.onkeydown = function(e) {
		if (e.keyCode === 13)
			onCheckButtonDown();
	};

	checkButton.onclick = onCheckButtonDown;

	const numFromUrl = window.location.hash.slice(1);
	numberInput.value = decodeURIComponent(numFromUrl);
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
	if (numberInput.value) {
		let number = isCorrectNumber(numberInput.value) &&
			getNumberString(numberInput.value);

		contentBlock.innerHTML = null;
		resultsBlock.style.display = null;

		if (number && number !== "0") {
			errorText.style.display = "none";
			const language = document.documentElement.lang;
			contentBlock.innerHTML = getPageContent(language, number);
		} else {
			errorText.style.display = null;
		}
	}
}

function isCorrectNumber(value) {
	let hasDigits = false;
	const s = value.trim();
	for (let i = 0; i < s.length; ++i) {
		if (s[i] >= "0" && s[i] <= "9")
			hasDigits = true;
		else if (!(", \'".includes(s[i])))
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
	let result = "";

	const canonical = getLowestKin(number);
	//const info = raaTillPalindrome(number, 650);

	result += '<p class="text" style="word-wrap: break-word; text-align: left">';
	result += 'Проверяемое число: <span style="color: #03a">' + separateWithCommas(number) + '</span>.</p>';

	result += '<p class="text" style="word-wrap: break-word; text-align: left">';
	result += 'Длина числа: ' + number.length + '.<br>';
	result += 'Палиндром: ' + (isPalindrome(number) ? "да" : "нет") + '.<br>';
	result += 'Каноническое: ' + ((number === canonical) ? "да" : "нет") + '.</p>';

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

function separateWithCommas(value, separator = ",") {
	let result = [];
	const size = value.length;
	for (let p = 0, l = size % 3; p < size;) {
		result.push(p ? String(separator) : "");
		for (let i = (p || !l) ? 3 : l; i; --i)
			result.push(value[p++]);
	}
	return result.join("");
}

function raaTillPalindrome(number, maxSteps) {
	let result = {
		iterationCount: 0,
		isPalindrome: false,
		steps: [ "" + number ]
	};

	let current = "" + number;
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
