const child_process  = require("node:child_process");
const os             = require("node:os");
const path           = require("node:path");

/**
 * Get platform-specific open command for opening default web browser via url
 * @returns {string} command name
 */
function getPlatformOpenCommand()
{
    const platform = os.platform();
    switch(platform)
    {
        case "linux": return "xdg-open";
        case "win32": return "start";
        case "darwin": return "open";
        case "cygwin": return "cygstart";
    }
}

// Cache the value
const openCommand = getPlatformOpenCommand();

/**
 * Open a url on desktop platforms.
 * @param {string} url - url to open. It may be a file url too.
 * @returns {void}
 */
module.exports.openURL = function(url)
{
    child_process.exec(`${openCommand} ${url}`);
}

/**
 * Get mime type from a file name.
 * @param {string} filename
 */
module.exports.getMimeType = function(filename)
{
    const extname = path.extname(filename).toLowerCase();
    switch(extname)
    {
        case ".html": return "text/html";
        case ".js": return "text/javascript";
        case ".css": return "text/css";
        case ".json": return "application/json";
        case ".wasm": return "application/wasm";
        case ".png": return "image/png";
        case ".jpg": return "image/jpg";
        case ".tif": case ".tiff": return "image/tiff";
        case ".txt": return "text/plain";
        case ".pdf": return "application/pdf";
        case ".woff": return "font/woff"
        case ".woff2": return "font/woff2";
        case ".ttf": return "font/ttf";
        case ".eot": return "font/eot";
        case ".otf": return "font/otf";
        case ".aac": return "audio/aac"
        case ".wav": return "audio/wav";
        case ".flac": return "audio/flac";
        case ".opsu": return "audio/opus";
        case ".ogg": return "audio/ogg"
        case ".mp3": return "audio/mpeg";
        case ".mp4": return "video/mp4";
        case ".weba": return "audio/webm";
        case ".webm": return "video/webm";
        case ".webp": return "image/webp";
        case ".mid": case ".midi": return "audio/midi";
        default: return "application/octet-stream";
    }
}
