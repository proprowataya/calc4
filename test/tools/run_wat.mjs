#!/usr/bin/env node
/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

/*****
 * run_wat.mjs
 *
 * A small helper used by calc4-codegen-test:
 *   - reads a WAT file
 *   - compiles it to WASM using "wat2wasm" (from WABT)
 *   - instantiates with imports (getchar/putchar/mem_get/mem_set)
 *   - calls exported "main"
 *   - writes: <stdout bytes><result>\n   to stdout
 *
 * Notes:
 *   - getchar/putchar are byte-oriented to match the C++ implementation.
 *   - mem_get/mem_set may be either i32 or i64.
 *
 * Exit codes:
 *   0: success
 *   2: missing dependency ("wat2wasm" not installed / not found)
 *   other: failure
 *****/

import fs from "node:fs/promises";
import path from "node:path";
import { spawnSync } from "node:child_process";

function getArg(name) {
  const i = process.argv.indexOf(name);
  if (i < 0) return null;
  if (i + 1 >= process.argv.length) return null;
  return process.argv[i + 1];
}

const watPath = getArg("--wat");
const stdinPath = getArg("--stdin");
const wat2wasmPath = getArg("--wat2wasm") || process.env.WAT2WASM || "wat2wasm";

if (!watPath) {
  console.error(
    "Usage: node run_wat.mjs --wat <file> [--stdin <file>] [--wat2wasm <path>]"
  );
  process.exit(1);
}

function compileWatToWasmFile(wat2wasmExe, watFile) {
  // Use a side-by-side .wasm file under the same directory as the input .wat.
  // calc4-codegen-test already uses a unique temporary directory, so collisions are unlikely.
  const wasmFile =
    path.join(
      path.dirname(watFile),
      path.basename(watFile, path.extname(watFile)) + ".wasm"
    );

  const r = spawnSync(wat2wasmExe, [watFile, "-o", wasmFile], {
    encoding: "utf8",
  });

  if (r.error) {
    if (r.error && r.error.code === "ENOENT") {
      console.error(
        "calc4-codegen-test: missing dependency: cannot execute '" +
        wat2wasmExe +
        "'.\n" +
        "Install WABT (WebAssembly Binary Toolkit) and ensure 'wat2wasm' is in PATH.\n" +
        "You can also pass --wat2wasm <path> or set the WAT2WASM environment variable.\n"
      );
      process.exit(2);
    }

    console.error(
      "Failed to execute '" + wat2wasmExe + "': " + String(r.error)
    );
    process.exit(1);
  }

  if (r.status !== 0) {
    const out = (r.stdout || "") + (r.stderr || "");
    console.error(
      "wat2wasm failed (exit code " +
      String(r.status) +
      ").\nCommand: " +
      wat2wasmExe +
      " " +
      watFile +
      " -o " +
      wasmFile +
      "\n" +
      out
    );
    process.exit(1);
  }

  return wasmFile;
}

try {
  const stdinBytes = stdinPath ? await fs.readFile(stdinPath) : new Uint8Array();

  const wasmPath = compileWatToWasmFile(wat2wasmPath, watPath);
  const bytes = await fs.readFile(wasmPath);

  // stdin/stdout and sparse fallback memory
  let inPos = 0;
  const outBytes = [];
  const sparseI32 = new Map();
  const sparseI64 = new Map();

  const getchar = () => {
    if (inPos >= stdinBytes.length) return -1;
    return stdinBytes[inPos++] & 0xff;
  };

  const putchar = (c) => {
    // c is i32 from WASM; keep the low byte.
    outBytes.push(c & 0xff);
  };

  const importsI64 = {
    env: {
      getchar,
      putchar,
      mem_get: (idx) => {
        const v = sparseI64.get(idx);
        return v === undefined ? 0n : v;
      },
      mem_set: (idx, value) => {
        if (value === 0n) sparseI64.delete(idx);
        else sparseI64.set(idx, value);
      },
    },
  };

  const importsI32 = {
    env: {
      getchar,
      putchar,
      mem_get: (idx) => {
        idx |= 0;
        const v = sparseI32.get(idx);
        return v === undefined ? 0 : v;
      },
      mem_set: (idx, value) => {
        idx |= 0;
        value |= 0;
        if (value === 0) sparseI32.delete(idx);
        else sparseI32.set(idx, value);
      },
    },
  };

  const module = new WebAssembly.Module(bytes);

  let instance;
  try {
    instance = new WebAssembly.Instance(module, importsI64);
  } catch (e64) {
    try {
      instance = new WebAssembly.Instance(module, importsI32);
    } catch (e32) {
      throw new Error(
        "Failed to instantiate module with either i64 or i32 imports.\n" +
        "i64 attempt: " + String(e64) + "\n" +
        "i32 attempt: " + String(e32)
      );
    }
  }

  const main = instance.exports.main;
  if (typeof main !== "function") {
    throw new Error("Exported function 'main' not found.");
  }

  const ret = main();
  const retStr = typeof ret === "bigint" ? ret.toString() : String(ret);

  process.stdout.write(Buffer.from(outBytes));
  process.stdout.write(retStr + "\n");
} catch (e) {
  console.error(e && e.stack ? e.stack : String(e));
  process.exit(1);
}
