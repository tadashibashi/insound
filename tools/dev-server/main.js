import fs from "fs";
import { startServers } from "./server.js";

/**
 * Program entry point
 * @param {string[]} args command line arguments (0 is node, 1 is this filename)
 */
function main(args)
{
    if (args.length < 3)
    {
        console.log("Please provide the name of a file to serve");
        return -1;
    }

    console.log("args:", args);

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