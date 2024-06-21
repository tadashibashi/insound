const socket = new WebSocket("ws://localhost:1234");


socket.addEventListener("open", () => {
    console.log("Connected to dev server.");
});
socket.addEventListener("message", event => {
    if (event.data === "RELOAD")
    {
        window.location.reload(true);
    }
});
socket.addEventListener("close", event => {
    console.error("Reload server socket disconnected. Try restarting the server and refreshing the page.");
});
socket.addEventListener("error", error => {
    console.error("WebSocket error:", error);
});
window.addEventListener("beforeunload", ev => {
    socket.close();
});
