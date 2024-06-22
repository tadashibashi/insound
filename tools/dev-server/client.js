const socket = new WebSocket("ws://localhost:1234");

socket.addEventListener("open", () => {
    console.log("Connected to dev server.");
});
socket.addEventListener("message", event => {
    if (event.data === "RELOAD")
    {
        // noinspection JSCheckFunctionSignatures
        window.location.reload(true); // on FireFox, passing `true` forces a reload that bypasses cache
    }
});
socket.addEventListener("close", () => {
    console.error("Reload server socket disconnected. Try restarting the server and refreshing the page.");
});
socket.addEventListener("error", error => {
    console.error("WebSocket error:", error);
});
window.addEventListener("beforeunload", () => {
    socket.close();
});
