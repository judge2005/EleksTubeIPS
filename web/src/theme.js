// The theme picked with the header's button, kept in the browser. Without one, theme.css follows the
// browser's light or dark setting. The build inlines this into app.html and every settings page, so a
// clock whose app.html is older than its firmware gets it too, and the new icon. It's a class on <html> rather than
// data-theme, which jQuery Mobile would take for one of its own themes.

function getCurrentTheme(root) {
	root = root || (typeof document !== 'undefined' ? document.documentElement : null);
	if (!root) {
		return 'auto';
	}
	if (root.classList.contains('light')) {
		return 'light';
	}
	if (root.classList.contains('dark')) {
		return 'dark';
	}
	if (root.classList.contains('auto')) {
		return 'auto';
	}
	return 'auto';
}

function getNextTheme(theme) {
	if (theme === 'light') {
		return 'dark';
	}
	if (theme === 'dark') {
		return 'auto';
	}
	return 'light';
}

function getThemeDisplayName(theme) {
	if (theme === 'light') {
		return 'light';
	}
	if (theme === 'dark') {
		return 'dark';
	}
	return 'system';
}

function getThemeIcon(theme) {
	if (theme === 'light') {
		return 'url("data:image/svg+xml,<svg xmlns=\'http://www.w3.org/2000/svg\' viewBox=\'0 0 24 24\' fill=\'none\' stroke=\'black\' stroke-width=\'2\' stroke-linecap=\'round\'><circle cx=\'12\' cy=\'12\' r=\'4\'/><path d=\'M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4\'/></svg>")';
	}
	if (theme === 'dark') {
		return 'url("data:image/svg+xml,<svg xmlns=\'http://www.w3.org/2000/svg\' viewBox=\'0 0 24 24\' fill=\'none\' stroke=\'black\' stroke-width=\'2\' stroke-linejoin=\'round\'><path d=\'M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z\'/></svg>")';
	}
	return 'url("data:image/svg+xml,<svg xmlns=\'http://www.w3.org/2000/svg\' viewBox=\'0 0 24 24\' fill=\'none\' stroke=\'black\' stroke-width=\'2\'><circle cx=\'12\' cy=\'12\' r=\'9\'/><path d=\'M12 3a9 9 0 0 0 0 18Z\' fill=\'black\' stroke=\'none\'/></svg>")';
}

function updateThemeButtonIcons(root) {
	if (typeof document === 'undefined') {
		return;
	}

	var currentTheme = getCurrentTheme(root);
	var buttons = document.querySelectorAll('.ui-icon-theme');
	for (var i = 0; i < buttons.length; i++) {
		var button = buttons[i];
		button.style.setProperty('--theme-icon', getThemeIcon(currentTheme));
		button.setAttribute('title', 'Theme: currently ' + getThemeDisplayName(currentTheme) + ', click to switch');
		button.setAttribute('aria-label', 'Theme: currently ' + getThemeDisplayName(currentTheme) + ', click to switch');
	}
}

function applyTheme(theme, root, storage) {
	root = root || (typeof document !== 'undefined' ? document.documentElement : null);
	storage = storage || (typeof localStorage !== 'undefined' ? localStorage : null);
	if (!root || !root.classList) {
		return theme;
	}

	root.classList.remove('light', 'dark', 'auto');
	if (theme === 'light' || theme === 'dark' || theme === 'auto') {
		root.classList.add(theme);
	}

	if (storage && typeof storage.setItem === 'function') {
		try {
			storage.setItem('theme', theme);
		} catch (e) {
		}
	}

	if (typeof document !== 'undefined') {
		updateThemeButtonIcons(root);
	}

	return theme;
}

(function () {
	if (typeof document === 'undefined') {
		return;
	}

	var root = document.documentElement;
	var theme = null;
	try {
		theme = localStorage.getItem('theme');
	} catch (e) {
	}

	if (theme !== 'light' && theme !== 'dark' && theme !== 'auto') {
		theme = 'auto';
	}
	applyTheme(theme, root);

	// An older app.html has a picture of a mermaid for its icon, in the page itself
	var icon = document.querySelector('link[rel~="icon"][href^="data:"]');
	if (icon) {
		icon.type = 'image/svg+xml';
		icon.href = '/favicon.svg';
	}
})();

function toggleTheme() {
	var root = document.documentElement;
	var theme = getCurrentTheme(root);
	applyTheme(getNextTheme(theme), root);
}

if (typeof module !== 'undefined') {
	module.exports = {
		getCurrentTheme: getCurrentTheme,
		getNextTheme: getNextTheme,
		getThemeDisplayName: getThemeDisplayName,
		getThemeIcon: getThemeIcon,
		applyTheme: applyTheme
	};
}
