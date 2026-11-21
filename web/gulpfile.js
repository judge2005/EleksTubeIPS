const fs = require('fs');
const { src, dest, series } = require('gulp');
const htmlmin = require('gulp-htmlmin');
const cleancss = require('gulp-clean-css');
const uglify = require('gulp-uglify');
const gzip = require('gulp-gzip');
const inline = require('gulp-inline');
const UglifyJS = require('uglify-js');

// The whole web UI, built into the firmware: see board_build.embed_files in platformio.ini and settingsPages in
// main.cpp. Not in the filesystem image, data, which only has the clock faces, weather icons and slide shows.
const buildFolder = 'build';
const srcFolder = 'src';

function clean(done) {
    // Builds before 2.1.16 put them in data, so a filesystem image built here could have old copies
    for (const folder of [buildFolder, '../data']) {
        if (fs.existsSync(folder)) {
            for (const file of fs.readdirSync(folder)) {
                if (file.endsWith('.gz')) {
                    fs.rmSync(folder + '/' + file);
                }
            }
        }
    }
    fs.mkdirSync(buildFolder, { recursive: true });
    done();
}

// Each page with its scripts and styles in it: app.html has the widgets and their look, and the settings pages
// their own scripts. html-minifier's own copy of UglifyJS is too old for them, so they go through ours.
function pages() {
    return src(srcFolder + '/*.html')
        .pipe(inline({
            base: srcFolder,
            js: uglify,
            css: [cleancss],
            disabledTypes: ['svg', 'img']
        }))
        .pipe(htmlmin({
            collapseWhitespace: true,
            removeComments: true,
            minifyCSS: true,
            minifyJS: (code, attribute) => attribute ? code : (UglifyJS.minify(code).code || code)
        }))
        .pipe(gzip())
        .pipe(dest(buildFolder));
}

// Gzipped like everything else here, though the PNGs barely shrink, so the firmware serves them all the same way
function icons() {
    // Not as text, which is gulp's default and mangles the PNGs
    return src([srcFolder + '/favicon.svg', srcFolder + '/favicon.png', srcFolder + '/apple-touch-icon.png'], { encoding: false })
        .pipe(gzip())
        .pipe(dest(buildFolder));
}

exports.clean = clean;
exports.default = series(clean, pages, icons);
