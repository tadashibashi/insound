const fs               = require("node:fs");
const path             = require("node:path");
const fileServer       = require("./file-server.js");
const reloadServer     = require("./reload-server.js");
const util             = require("./util.js");

// ===== Constants ============================================================
const HOSTNAME = "127.0.0.1"; ///< servers will be hosted on localhost
const FILE_PORT = 3000;       ///< port number of the file server
const WS_PORT = 1234;         ///< port number of the websocket reload server

// ===== Global Variables =====================================================
/**
 * @type {number}
 * Tracks the number of servers initialized to signal opening the web browser
 */
let serversInit = 0;

/**
 * Program entry point
 * @param {string[]} args command line arguments (0 is node, 1 is this filename)
 */
function main(args)
{
    // Parse args
    if (args.length < 3)
    {
        console.log("Please provide the name of a file to serve");
        return -1;
    }

    let filename = args[2];
    if (!fs.existsSync(filename))
    {
        console.log("Requested file \"" + filename + "\" does not exist");
        return -2;
    }

    startServers(filename);
    return 0;
}

main(process.argv);

// ===== Helpers =================================================================
/**
 * Passed to server open functions. When each server inits, it keeps track how
 * many have finished opening before running web browser with target file.
 */
function serverInitCallback() {
    if (++serversInit >= 2)
        openBrowser();
}

/**
 * @param {string} targetFile - target file to open/watch
 */
function startServers(targetFile)
{
    // Make path to target file absolute
    if (!path.isAbsolute(targetFile))
    {
        targetFile = path.join(process.cwd(), targetFile);
    }

    // Use target file directory as root
    const root = path.dirname(targetFile);

    try {
        fileServer.open(HOSTNAME, FILE_PORT, root, targetFile, serverInitCallback);
        reloadServer.open(HOSTNAME, WS_PORT, targetFile, serverInitCallback);
    }
    catch(err)
    {
        shutdown();
        throw err;
    }

    process.on("SIGTERM", () => {
        shutdown();
        process.exit(0);
    });

    process.on("SIGINT", () => {
        shutdown();
        process.exit(0);
    });

    return 0;
}

function shutdown()
{
    console.log("Shutting down");

    reloadServer.close();
    fileServer.close();
    serversInit = 0;
}

/** Open the file browser at the root of the file server */
function openBrowser()
{
    util.openURL("http://localhost:" + FILE_PORT);
}
