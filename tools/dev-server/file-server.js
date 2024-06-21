const fs = require("node:fs");
const http = require("node:http");
const path = require("node:path");
const util = require("./util.js");


// ===== Global Variables =====================================================
/**
 * Serves the target application
 * @type {http.Server}
*/
let server = null;


// ===== Public API ===========================================================
/**
 * @param {string} hostname  address to host the server with
 * @param {number} port      port number of address
 * @param {string} root      root path to serve files from; must be a directory.
 * @param {string} mainFile  main file to serve when the root "http:localhost:<port>" is visited
 */
function open(hostname, port, root, mainFile, initCallback = ()=>{})
{
    close();

    server = http.createServer((req, res) => {
        if (req.method !== "GET") // Reject non-GET method requests
        {
            req.statusCode = 405;
            res.end("Method not allowed.");
        }

        // Get clean path name (strip any query string)
        const url = new URL(req.url, "http://" + req.headers.host);
        let pathname = url.pathname;
        switch(pathname)
        {
            case "/client.js":
                pathname = path.join(__dirname, "client.js");
                break;
            case "/":
                pathname = mainFile;
                break;
            default:
                pathname = path.join(root, pathname);
                break;
        }

        fs.readFile(pathname, (err, data) => {
            if (err)
            {
                res.statusCode = 404;
                res.end("File could not be found.");
            }
            else
            {
                if (pathname.endsWith(".html")) // inject our client code
                {
                    data = data.toString().replace("</body>", `<script src="client.js"></script></body>`);
                }

                res.setHeader("Cross-Origin-Embedder-Policy", "require-corp");
                res.setHeader("Cross-Origin-Opener-Policy", "same-origin");
                res.setHeader("Content-Type", util.getMimeType(pathname));
                res.statusCode = 200;
                res.end(data);
            }
        });
    });

    server.listen(port, hostname, () => {
        initCallback();
    });

    console.log("Server running at: http://" + hostname + ":" + port);
}

/**
 * Close the file server. Safe to call, even if already closed.
 */
function close()
{
    if (server === null) return;

    server.close();
    server = null;
}

module.exports = { open, close };
