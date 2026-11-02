const fs = require('fs');
const { src, dest, series } = require('gulp');
const htmlmin = require('gulp-htmlmin');
const cleancss = require('gulp-clean-css');
const uglify = require('gulp-uglify');
const gzip = require('gulp-gzip');
const inline = require('gulp-inline');
const inlineImages = require('gulp-css-inline-images');
const favicon = require('gulp-base64-favicon');

const dataFolder = '../data';
const srcFolder = 'src';

function clean(done) {
    for (const file of fs.readdirSync(dataFolder)) {
        if (file.endsWith('.gz')) {
            fs.rmSync(dataFolder + '/' + file);
        }
    }
    done();
}

function buildfs_inline() {
    return src(srcFolder + '/*.html')
        // .pipe(favicon())
        .pipe(inline({
            base: srcFolder,
            js: uglify,
            css: [cleancss],
            disabledTypes: ['svg', 'img'],
            ignore: ['/theme.css']   // The firmware serves it, see buildfs_theme()
        }))
    	.pipe(inlineImages({
            webRoot: srcFolder
    	}))
        .pipe(htmlmin({
            collapseWhitespace: true,
            removeComments: true,
            minifyCSS: true,
            minifyJS: true
        }))
        .pipe(gzip())
        .pipe(dest(dataFolder));
}

// Linked rather than inlined, and built into the firmware, so that the pages and app.html share it and a
// clock that's only had its firmware updated gets it too
function buildfs_theme() {
    return src(srcFolder + '/theme.css')
        .pipe(cleancss())
        .pipe(gzip())
        .pipe(dest(dataFolder));
}

exports.clean = clean;
// Built into the firmware like the stylesheet. Gzipped like everything else here, though the PNGs barely shrink,
// so the firmware serves them all the same way.
function buildfs_icons() {
    // Not as text, which is gulp's default and mangles the PNGs
    return src([srcFolder + '/favicon.svg', srcFolder + '/favicon.png', srcFolder + '/apple-touch-icon.png'], { encoding: false })
        .pipe(gzip())
        .pipe(dest(dataFolder));
}

exports.default = series(clean, buildfs_inline, buildfs_theme, buildfs_icons);