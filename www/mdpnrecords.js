(function() {
	console.log("Starting calculation...");
	for (let step = 1; step <= 289; ++step)
		initPalindrome(step);

	console.log("Calculation finished");
})();

function initPalindrome(step) {
	let palElement = document.getElementById("pal-" + step);
	let resElement = document.getElementById("respal-" + step);

	if (palElement && resElement) {
		const num = palElement.innerText.replaceAll(",", "");
		if (isCorrectNumber(num)) {
			const info = raaTillPalindrome(num.trim(), 500);
			if (info.isPalindrome) {
				let shortSpan, wideSpan;
				if (info.result.length <= 20) {
					shortSpan = '<span class="pal-short">' + info.result + '</span>';
				} else {
					shortSpan = '<span class="pal-short pal-active" ' +
					'onclick="toggleFullPalindrome(' + step + ', true)">' +
					info.result.substr(0, 20) + '...</span>';
				}

				if (info.result.length <= 50) {
					wideSpan = '<span class="pal-wide">' + info.result + '</span>';
				} else {
					wideSpan = '<span class="pal-wide pal-active" ' +
					'onclick="toggleFullPalindrome(' + step + ', true)">' +
					info.result.substr(0, 50) + '...</span>';
				}

				resElement.innerHTML = '<div id="' + step + '-norm" ' +
					'style="display: none">' + shortSpan + wideSpan + '</div>' +
					'<div id="' + step + '-full" class="pal-active" style="display: none" ' +
					'onclick="toggleFullPalindrome(' + step + ', false)">' +
					'<span class="breakable">' + info.result + '</span></div>';

				document.getElementById(step + "-norm").style.display = "inline-block";
			}
		}
	}
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
	const norm = document.getElementById(step + "-norm");
	const full = document.getElementById(step + "-full");
	if (norm && full) {
		if (show) {
			norm.style.display = "none";
			let element = document.getElementById("respal-" + step);
			if (element) {
				const cellStyle = window.getComputedStyle(element);
				const w = parseInt(cellStyle.width, 10);
				full.style.width = w + "px";
			}
			full.style.display = "inline-block";
		} else {
			full.style.display = "none";
			norm.style.display = "inline-block";
		}
	}
}
