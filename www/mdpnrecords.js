console.log("Starting calculation...");

const defaultWideLength = 55;
const wideLengths = [];
wideLengths[23] = 50;
wideLengths[24] = 50;
wideLengths[25] = 48;

const wideLenInitFlags = [];
for (let step = 34; step <= 289; ++step)
	initPalindrome(step);

console.log("Calculation finished");

function initPalindrome(step) {
	let palElement = document.getElementById("pal-" + step);
	let resElement = document.getElementById("respal-" + step);

	if (palElement && resElement) {
		const numText = palElement.textContent;
		const num = numText.replace(/[, ']/g, "").trim();

		if (isCorrectNumber(num)) {
			const wideLength = wideLengths[num.length] || defaultWideLength;

			if (!wideLenInitFlags[num.length]) {
				wideLenInitFlags[num.length] = true;
				const lenElement = document.getElementById("wideLen-" + num.length);
				if (lenElement) {
					lenElement.innerHTML = getWideLengthText(wideLength);
				}
			}

			const info = raaTillPalindrome(num, 500);
			if (info.isPalindrome) {
				let shortSpan, wideSpan;
				if (info.result.length <= 23) {
					shortSpan = '<span class="pal-short">' + info.result + '</span>';
				} else {
					shortSpan = '<span class="pal-short pal-active" ' +
					'onclick="toggleFullPalindrome(' + step + ', true)">' +
					info.result.substr(0, 20) + '...</span>';
				}

				if (info.result.length <= wideLength + 3) {
					wideSpan = '<span class="pal-wide">' + info.result + '</span>';
				} else {
					wideSpan = '<span class="pal-wide pal-active" ' +
					'onclick="toggleFullPalindrome(' + step + ', true)">' +
					info.result.substr(0, wideLength) + '...</span>';
				}

				resElement.innerHTML = '<div id="' + step + '-norm">' +
					shortSpan + wideSpan + '</div><div id="' + step +
					'-full" class="pal-active" style="display: none" ' +
					'onclick="toggleFullPalindrome(' + step + ', false)">' +
					'<span class="breakable">' + info.result + '</span></div>';
			}
		}
	}
}

function getWideLengthText(value) {
	if (document.documentElement.lang === "ru") {
		return "перв" + getCaseEnding(value, "ая", "ые") + " " +
			value + "&nbsp;цифр" + getCaseEnding(value, "а", "ы", "");
	}
	return "first " + value + " digits";
}

function getCaseEnding(value, one, twofour, other) {
	if (other === undefined)
		other = twofour;

	return ((value % 10) && (value % 10 < 5) && ((value % 100 < 11) || (value % 100 > 19))) ?
		(value % 10 === 1) ? one : twofour : other;
}

function isCorrectNumber(value) {
	let hasDigits = false;
	const s = value.trim();
	for (let i = 0; i < s.length; ++i) {
		if (s[i] < "0" || s[i] > "9")
			return false;
		hasDigits = true;
	}
	return hasDigits;
}

function raaTillPalindrome(value, maxSteps) {
	let result = {
		iterationCount: 0,
		isPalindrome: false,
		result: ""
	};

	let current = "" + value;
	while (result.iterationCount < maxSteps) {
		current = reverseAndAdd(current);
		++result.iterationCount;

		if (isPalindrome(current)) {
			result.isPalindrome = true;
			result.result = current;
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

function toggleFullPalindrome(step, show) {
	if (!window.getSelection().toString()) {
		const norm = document.getElementById(step + "-norm");
		const full = document.getElementById(step + "-full");

		if (norm && full) {
			norm.style.display = show ? "none" : "inline-block";
			full.style.display = show ? "inline-block" : "none";
		}
	}
}
