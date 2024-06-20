# Dev server for Insound Engine

Serves as a replacement for `emrun` which is somewhat limited in developer experience with CMake.

Features:
- Serves the target file by visiting "/"
- Enables SharedArrayBuffer and Wasm Web Workers by serving the proper headers
- Hot reloads target Emscripten projects when the .html file changes (may change this to the .js file of the same name)

Requires Node.js or Bun.

## Running the Server

```shell
npm install
node server.js <path/to/target.html>
```

The target html file should have its necessary .js, .wasm, .data, etc. files stored relative to its own directory.
Now, by visiting http://localhost:3000, the target file should be properly served and hot-reloaded when changed.
Note: do not use this in a production environment, as it is only intended for development.
