import fs from "node:fs";
import http from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { WebSocket, WebSocketServer } from "ws";
import {open, getMimeType} from "./util.js";

// ===== Constants ============================================================
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const hostname = "127.0.0.1"; ///< servers will be hosted on localhost
const FILE_PORT = 3000;       ///< port number of the file server
const WS_PORT = 1234;         ///< port number of the websocket reload server

// ===== Global Variables =====================================================
// Track the number of servers initialized to signal opening the web browser
let serversInit = 0;

/**
 * Serves the target application
 * @type {Server}
*/
let fileServer = null;

/**
 * Handles sockets broadcasting page reloads on target file change
 * @type {WebServerServer}
*/
let reloadServer = null;

/**
 * Current web socket connections
 * @type {Set<WebSocket>}
 */
let sockets = new Set();

// ===== Main =================================================================
/**
 * Program entry point
 * @param {string} targetFile - target file to open/watch
 */
export function startServers(targetFile)
{
    if (!path.isAbsolute(targetFile))
    {
        targetFile = path.join(process.cwd(), targetFile);
    }

    const root = path.dirname(targetFile);

    startFileServer(root, targetFile);
    startReloadServer();

    fs.watch(targetFile, {},
        (eventType, eventFile) => {
            if (eventType === "change" && reloadServer !== null)
                reloadServer.emit("reload");
        });

    process.on("SIGTERM", () => shutdown());
    process.on("SIGINT", () => shutdown());

    return 0;
}

// ===== Helper functions =====================================================

function openBrowser()
{
    open("http://localhost:" + FILE_PORT);
}


// ----- Setup ----------------------------------------------------------------

function startReloadServer()
{
    closeReloadServer(); // close if already open

    reloadServer = new WebSocketServer({
        port: WS_PORT,
    });

    reloadServer.on("listening", () => {
        if (++serversInit >= 2)
            openBrowser();
    });

    reloadServer.on("connection", ( /** @type {WebSocket} */ socket, req) => {
        sockets.add(socket);
        const ip = req.socket.remoteAddress;
        console.log("Socket opened from remote address: " + ip);

        socket.on("close", () => {
            sockets.delete(socket);
            console.log("Socket closed from remote address: " + ip);
        });
    });

    reloadServer.on("reload", () => {
        reloadServer.clients.forEach(client => {
            if (client.readyState === WebSocket.OPEN)
                client.send("RELOAD");
        });
    });
}

function startFileServer(root, mainFile)
{
    closeFileServer();

    fileServer = http.createServer((req, res) => {
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
                res.setHeader("Content-Type", getMimeType(pathname));
                res.statusCode = 200;
                res.end(data);
            }
        });
    });

    fileServer.listen(FILE_PORT, hostname);

    if (++serversInit >= 2)
        openBrowser();
    console.log("Server running at: http://" + hostname + ":" + FILE_PORT);
}

// ----- Clean up -------------------------------------------------------------

function closeReloadServer()
{
    if (reloadServer === null) return;

    // Clean up sockets
    for (const socket of sockets)
        socket.close();
    sockets.clear();

    // Clean up server
    reloadServer.close();
    reloadServer = null;
    --serversInit;
}

function closeFileServer()
{
    if (fileServer === null) return;

    fileServer.close();
    fileServer = null;
    --serversInit;
}

function shutdown()
{
    console.log("Shutting down");

    closeReloadServer();
    closeFileServer();
    process.exit(0);
}
