const crypto = require("node:crypto");
const fs     = require("node:fs");
const net    = require("node:net");

// ===== Module variables =====================================================

/**
 * @type {net.Server}
 */
let reloadServer = null;

/**
 * Current active WebSockets from `reloadServer`
 * @type {Set<net.Socket>}
 */
const sockets = new Set();

/**
 * Filepath to watch for reloads
 * @type {string}
 */
let targetFile = "";

// Cached reload command message
const ReloadMessage = encodeWebSocketTextFrame("RELOAD");

// Defers reload until there are no more file changes detected.
// Sometimes there will be rapid file changes
// when the compiler generates the new files. This helps make
// sure we receive the last "correct" version of the file before reloading.
const ReloadCoolDown = 250; // in milliseconds
let reloadTimeout = null;


// ===== Public API ===========================================================
/**
 * Open the reload server.
 * @param {string} hostname        address to host the server at
 * @param {number} port            port number of address
 * @param {string} fileToWatch     file to watch changes for, triggering reload
 * @param {()=>void} initCallback  fired right after the server is initialized
 */
function open(hostname, port, fileToWatch, initCallback = ()=>{})
{
    close();
    const server = net.createServer(setupSocket);

    server.on("close", () => {
        for (const socket of sockets) // clean up remaining live sockets
            socket.end();
        sockets.clear(); // in case any remaining
    });

    server.on("reload", () => {
        for (const socket of sockets)
        {
            if (socket.readyState === "open")
            {
                if (reloadTimeout != null)
                {
                    clearTimeout(reloadTimeout);
                }

                reloadTimeout = setTimeout(() => {
                    socket.write(ReloadMessage);
                    reloadTimeout = null;
                }, ReloadCoolDown);
            }
        }

        console.log("Reloaded source file");
    });

    server.on("error", err => {
        console.error("Websocket error:", err);
    });

    server.listen(port, hostname, () => {
        initCallback();
    });

    fs.watch(fileToWatch, {},
        (eventType, eventFile) => {
            if (eventType === "change" && reloadServer !== null)
            {
                reloadServer.emit("reload");
            }
        });

    targetFile = fileToWatch;
    reloadServer = server;
}

function close()
{
    if (reloadServer === null) return;

    if (targetFile !== "")
        fs.unwatchFile(targetFile);
    reloadServer.close();
    reloadServer = null;
}

module.exports = { open, close };


// ===== Helpers ==============================================================
/**
 * Set up socket connection
 * @param {net.Socket} socket
 */
function setupSocket(socket)
{
    const address = socket.address().address;

    socket.on("data", (data) => {
        const request = data.toString();
        if (request.startsWith("GET"))
        {
            // Extract the Sec-WebSocket-Key
            const keyMatch = request.match(/Sec-WebSocket-Key: (.+)/);
            if (!keyMatch)
            {
                // Missing key from client header
                socket.end();
                return;
            }

            const key = keyMatch[1].trim();

            // Create the Sec-WebSocket-Accept value
            const acceptKey = crypto.createHash("sha1")
                .update(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")
                .digest("base64");

            const headers = [
                'HTTP/1.1 101 Switching Protocols',
                'Upgrade: websocket',
                'Connection: Upgrade',
                `Sec-WebSocket-Accept: ${acceptKey}`,
            ];

            socket.write(headers.join("\r\n") + "\r\n\r\n");
        }
        else
        {
            const message = request.slice(request.indexOf("\r\n\r\n") + 4);
            console.log("Received message: " + message);
        }
    });

    socket.on("close", () => {
        console.log("Client disconnected from: " + address);
        sockets.delete(socket);
    });

    sockets.add(socket);
    console.log("Client connected from: " + address);
}

/**
 * Create a WebSocket text data frame to be sent to the client
 * @param {string} data
 */
function encodeWebSocketTextFrame(data)
{
    const nPayloadBytes = Buffer.byteLength(data);

    // Payload length
    // 0 - 125 bytes, just the byte length is needed
    // 126 - 65535, set the payload length byte to 126, followed by big endian short of 2 bytes specifying length
    // 65536 - NUMBER MAX length byte set to 127, followed by 8 bytes in big endian order representing the length.
    //     JS maxes out at 2^53-1, which is 4 bytes.
    const lengthByteCount = nPayloadBytes < 126 ? 0 : nPayloadBytes < 65536 ? 2 : 8;
    const payloadLength = lengthByteCount === 0 ? nPayloadBytes : lengthByteCount === 2 ? 126 : 127;

    const buffer = Buffer.alloc(2 + lengthByteCount + nPayloadBytes);
    buffer[0] = 0b10000001;
    buffer[1] = payloadLength;

    let headerOffset = 2;
    if (lengthByteCount === 2)
    {
        buffer.writeUInt16BE(nPayloadBytes, 2);
        headerOffset += 2;
    }
    else if (lengthByteCount === 8)
    {
        buffer.writeUInt32BE(0, 2);
        buffer.writeUInt32BE(nPayloadBytes, 6);
        headerOffset += 8;
    }

    buffer.write(data, headerOffset);
    return buffer;
}
