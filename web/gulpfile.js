const fs = require('fs');
const { src, dest, series } = require('gulp');
const htmlmin = require('gulp-htmlmin');
const cleancss = require('gulp-clean-css');
const uglify = require('gulp-uglify');
const gzip = require('gulp-gzip');
const del = require('del');
const inline = require('gulp-inline');
const inlineImages = require('gulp-css-inline-images');
const favicon = require('gulp-base64-favicon');

const dataFolder = '../data';
const srcFolder = 'src';

function clean() {
    return del([ dataFolder + '/*.gz'], {force: true});
}

function buildfs_inline() {
    return src(srcFolder + '/*.html')
        // .pipe(favicon())
        .pipe(inline({
            base: srcFolder,
            js: uglify,
            css: [cleancss],
            disabledTypes: ['svg', 'img']
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

exports.clean = clean;
exports.default = series(clean, buildfs_inline);