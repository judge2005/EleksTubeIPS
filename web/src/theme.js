// The theme picked with the header's button, kept in the browser. Without one, theme.css follows the
// browser's light or dark setting. The build inlines this into app.html and every settings page, so a
// clock whose app.html is older than its firmware gets it too, and the new icon. It's a class on <html> rather than
// data-theme, which jQuery Mobile would take for one of its own themes.
(function () {
	var theme = null;
	try {
		theme = localStorage.getItem('theme');
	} catch (e) {
	}

	if (theme == 'light' || theme == 'dark') {
		document.documentElement.classList.remove('light', 'dark');
		document.documentElement.classList.add(theme);
	}

	// An older app.html has a picture of a mermaid for its icon, in the page itself
	var icon = document.querySelector('link[rel~="icon"][href^="data:"]');
	if (icon) {
		icon.type = 'image/svg+xml';
		icon.href = '/favicon.svg';
	}
})();

function toggleTheme() {
	var root = document.documentElement;
	var dark = root.classList.contains('dark') || (!root.classList.contains('light') && matchMedia('(prefers-color-scheme: dark)').matches);
	var theme = dark ? 'light' : 'dark';

	root.classList.remove('light', 'dark');
	root.classList.add(theme);
	try {
		localStorage.setItem('theme', theme);
	} catch (e) {
	}
}
