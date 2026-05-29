#!/usr/bin/env node
import { writeFile } from "node:fs/promises";
import vm from "node:vm";

function parseArgs(argv) {
  const args = {
    output: "",
    parts: [],
  };

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--output") {
      args.output = argv[++i] ?? "";
    } else if (arg.startsWith("--")) {
      throw new Error(`Unknown option ${arg}`);
    } else {
      args.parts.push(arg);
    }
  }

  if (!args.parts.length) {
    throw new Error("Usage: check-jlcpcb-parts.mjs [--output file.json] C123 C456 ...");
  }

  return args;
}

async function lookupPart(code) {
  const url = `https://jlcpcb.com/partdetail/${encodeURIComponent(code)}`;
  const response = await fetch(url, {
    headers: {
      "user-agent": "Mozilla/5.0 (compatible; vera-module-lcsc-check)",
      "accept-language": "en-US,en;q=0.9",
      "cache-control": "no-cache",
    },
  });

  if (!response.ok) {
    return {
      code,
      found: false,
      available: false,
      error: `HTTP ${response.status}`,
      sourceUrl: url,
    };
  }

  const html = await response.text();
  const match = html.match(/<script>window\.__NUXT__=(.*?)<\/script>/s);
  if (!match) {
    return {
      code,
      found: false,
      available: false,
      error: "Unable to locate JLCPCB parts data in page",
      sourceUrl: url,
    };
  }

  const sandbox = { window: {}, Set, Map, Array };
  vm.createContext(sandbox);
  vm.runInContext(`window.__NUXT__=${match[1]}`, sandbox, { timeout: 5000 });

  const exact = sandbox.window.__NUXT__?.data?.[0]?.componentInfo;

  if (!exact || String(exact.componentCode ?? "").toUpperCase() !== code.toUpperCase()) {
    return {
      code,
      found: false,
      available: false,
      error: "Exact LCSC/JLCPCB code not found",
      sourceUrl: url,
    };
  }

  const stock = Number(exact.stockCount ?? 0);
  return {
    code,
    found: true,
    available: stock > 0,
    stock,
    type: exact.componentLibraryType ?? "",
    preferred: Boolean(exact.preferredComponentFlag),
    manufacturer: exact.componentBrandEn ?? "",
    mpn: exact.componentModelEn ?? "",
    package: exact.componentSpecificationEn ?? exact.encapsulationNumber ?? "",
    description: exact.describe ?? exact.componentName ?? "",
    sourceUrl: url,
  };
}

const args = parseArgs(process.argv.slice(2));
const uniqueParts = [...new Set(args.parts.map((part) => part.trim()).filter(Boolean))].sort();
const results = {};

for (const code of uniqueParts) {
  results[code] = await lookupPart(code);
  const result = results[code];
  const status = result.available ? "available" : result.found ? "no-stock" : "not-found";
  console.error(`${code}: ${status}${result.stock !== undefined ? ` stock=${result.stock}` : ""}`);
}

const output = {
  checkedAt: new Date().toISOString(),
  source: "https://jlcpcb.com/partdetail/{code}",
  parts: results,
};

const text = JSON.stringify(output, null, 2) + "\n";
if (args.output) {
  await writeFile(args.output, text, "utf8");
} else {
  process.stdout.write(text);
}
