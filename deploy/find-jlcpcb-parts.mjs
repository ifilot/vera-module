#!/usr/bin/env node
import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import vm from "node:vm";

const BOARDS = ["fork-r0", "fork-r1"];
const ROOT = "manufacturing/jlcpcb";
const CACHE_DIR = ".cache/jlcpcb-parts";
const LOOKUP_APPROVED = process.env.ALLOW_JLCPCB_LOOKUP === "1";

const FOOTPRINT_PACKAGES = [
  [/0603|1608/i, "0603"],
  [/0805|2012/i, "0805"],
  [/TSSOP-14/i, "TSSOP-14"],
  [/TSSOP-16/i, "TSSOP-16"],
  [/TSSOP-24/i, "TSSOP-24"],
  [/SOIC-8/i, "SOIC-8"],
  [/SOT-23-5/i, "SOT-23-5"],
  [/QFN-48/i, "QFN-48"],
  [/5\.0x3\.2|5032|SG8002/i, "SMD5032-4P"],
  [/SRN4018|4018/i, "4018"],
  [/CONN9_MSD|micro.?SD|SD_TE/i, "microSD"],
  [/R_Array_Convex_4x0603/i, "0603x4"],
];

function csvEscape(value) {
  const text = String(value ?? "");
  return /[",\n\r]/.test(text) ? `"${text.replaceAll('"', '""')}"` : text;
}

function parseCsv(text) {
  const rows = [];
  let row = [];
  let field = "";
  let quoted = false;
  for (let i = 0; i < text.length; i += 1) {
    const ch = text[i];
    if (quoted) {
      if (ch === '"' && text[i + 1] === '"') {
        field += '"';
        i += 1;
      } else if (ch === '"') {
        quoted = false;
      } else {
        field += ch;
      }
      continue;
    }
    if (ch === '"') {
      quoted = true;
    } else if (ch === ",") {
      row.push(field);
      field = "";
    } else if (ch === "\n") {
      row.push(field);
      rows.push(row);
      row = [];
      field = "";
    } else if (ch !== "\r") {
      field += ch;
    }
  }
  if (field || row.length) {
    row.push(field);
    rows.push(row);
  }

  const [header, ...data] = rows.filter((item) => item.length && item.some(Boolean));
  return data.map((values) =>
    Object.fromEntries(header.map((name, index) => [name, values[index] ?? ""])),
  );
}

function writeCsv(rows, fields) {
  return [
    fields.join(","),
    ...rows.map((row) => fields.map((field) => csvEscape(row[field])).join(",")),
  ].join("\n") + "\n";
}

function packageFromFootprint(footprint) {
  for (const [pattern, pkg] of FOOTPRINT_PACKAGES) {
    if (pattern.test(footprint)) return pkg;
  }
  return "";
}

function referencePrefix(designator) {
  return (designator.match(/^[A-Za-z]+/)?.[0] ?? "").toUpperCase();
}

function normalizedValue(comment, prefix) {
  const raw = comment.trim();
  if (prefix === "R") {
    if (/^0$/.test(raw)) return "0R";
    if (/^\d+$/.test(raw)) return `${raw}R`;
    return raw.replace(/ohm/i, "R");
  }
  return raw.replace(/µ/g, "u");
}

function queryFor(row) {
  const prefix = referencePrefix(row.Designator);
  const pkg = packageFromFootprint(row.Footprint);
  const value = normalizedValue(row.Comment, prefix);
  if (prefix === "R") return `${value} ${pkg} resistor 1%`;
  if (prefix === "C") return `${value} ${pkg} capacitor`;
  if (prefix === "L") return `${row.Comment} ${pkg} inductor`;
  if (row["LCSC Part #"]) return row["LCSC Part #"];
  return `${row.Comment} ${pkg}`.trim();
}

function containsValue(part, row) {
  const prefix = referencePrefix(row.Designator);
  const value = normalizedValue(row.Comment, prefix).toLowerCase();
  const haystack = [
    part.componentModelEn,
    part.componentName,
    part.componentSpecificationEn,
    part.describe,
  ].join(" ").toLowerCase();

  if (prefix === "R") {
    const alternatives = new Set([value, value.replace("r", "Ω")]);
    if (value.endsWith("r")) alternatives.add(value.slice(0, -1) + "Ω");
    if (value === "0r") alternatives.add("0Ω");
    return [...alternatives].some((needle) => haystack.includes(needle.toLowerCase()));
  }
  if (prefix === "C") {
    const alternatives = new Set([value, value.replace("uf", "uF"), value.replace("nf", "nF")]);
    if (value === "1u") alternatives.add("1uF");
    return [...alternatives].some((needle) => haystack.includes(needle.toLowerCase()));
  }
  return haystack.includes(row.Comment.toLowerCase().replaceAll(",", " "));
}

function packageMatches(part, row) {
  const pkg = packageFromFootprint(row.Footprint);
  if (!pkg) return false;
  const haystack = [
    part.componentSpecificationEn,
    part.describe,
    part.encapsulationNumber,
    part.componentModelEn,
  ].join(" ").toLowerCase();
  if (pkg === "microSD") return /micro.?sd|tf|card socket|conn9|msd/i.test(haystack);
  if (pkg === "0603x4") return /0603|1608|4.*resistor|array/i.test(haystack);
  return haystack.includes(pkg.toLowerCase());
}

function firstPrice(part) {
  const prices = part.buyComponentPrices?.length ? part.buyComponentPrices : part.componentPrices;
  return Number(prices?.[0]?.productPrice ?? part.initialPrice ?? 999);
}

function scorePart(part, row) {
  let score = 0;
  const prefix = referencePrefix(row.Designator);
  const pkgMatch = packageMatches(part, row);
  const valueMatch = containsValue(part, row);
  const text = [part.componentModelEn, part.componentName, part.describe].join(" ").toLowerCase();

  if (pkgMatch) score += 35;
  if (valueMatch) score += 35;
  if (part.componentLibraryType === "base") score += 16;
  if (part.preferredComponentFlag) score += 6;
  if ((part.stockCount ?? 0) > 1000) score += 5;
  if ((part.stockCount ?? 0) > 10000) score += 3;
  if (firstPrice(part) < 0.01) score += 4;
  if (firstPrice(part) < 0.005) score += 3;

  if (prefix === "R" && /resistor/.test(text)) score += 7;
  if (prefix === "C" && /capacitor|mlcc|x5r|x7r|c0g|np0/.test(text)) score += 7;
  if (prefix === "L" && /inductor/.test(text)) score += 7;
  if (prefix === "U" || prefix === "X" || prefix === "RN" || prefix === "J") {
    if (text.includes(row.Comment.toLowerCase().replaceAll(",", " "))) score += 35;
  }

  return score;
}

async function fetchParts(query) {
  if (!LOOKUP_APPROVED) {
    throw new Error(
      "Live JLCPCB lookup is disabled. Set ALLOW_JLCPCB_LOOKUP=1 to allow BOM-derived search terms to be sent to jlcpcb.com.",
    );
  }

  await mkdir(CACHE_DIR, { recursive: true });
  const cacheName = query.replace(/[^A-Za-z0-9._-]+/g, "_").slice(0, 120) + ".html";
  const cachePath = path.join(CACHE_DIR, cacheName);
  let html;
  try {
    html = await readFile(cachePath, "utf8");
  } catch {
    const url = `https://jlcpcb.com/parts/componentsearch?isSearch=true&searchTxt=${encodeURIComponent(query)}`;
    const response = await fetch(url, {
      headers: {
        "user-agent": "Mozilla/5.0 (compatible; vera-module-bom-sourcing)",
        "accept-language": "en-US,en;q=0.9",
      },
    });
    if (!response.ok) throw new Error(`JLCPCB search failed ${response.status} for ${query}`);
    html = await response.text();
    await writeFile(cachePath, html);
  }

  const match = html.match(/<script>window\.__NUXT__=(.*?)<\/script>/s);
  if (!match) return [];
  const sandbox = { window: {}, Set, Map, Array };
  vm.createContext(sandbox);
  vm.runInContext(`window.__NUXT__=${match[1]}`, sandbox, { timeout: 5000 });
  const tabs = sandbox.window.__NUXT__?.data?.[0]?.presaleTypeTabs ?? [];
  const stockTab = tabs.find((tab) => /stock/i.test(tab.label)) ?? tabs[0];
  return stockTab?.tableInfo?.tableList ?? [];
}

function candidateRow(board, row, part, rank, score, selected, reason, query) {
  return {
    Board: board,
    Comment: row.Comment,
    Designator: row.Designator,
    Footprint: row.Footprint,
    Query: query,
    Rank: rank,
    Score: score,
    Selected: selected ? "yes" : "",
    Reason: reason,
    "LCSC Part #": part?.componentCode ?? "",
    Type: part?.componentLibraryType ?? "",
    Stock: part?.stockCount ?? "",
    Price: part ? firstPrice(part).toFixed(6) : "",
    Manufacturer: part?.componentBrandEn ?? "",
    MPN: part?.componentModelEn ?? "",
    Package: part?.componentSpecificationEn ?? "",
    Description: part?.describe ?? "",
  };
}

async function processBoard(board) {
  const bomPath = path.join(ROOT, board, "bom.csv");
  const rows = parseCsv(await readFile(bomPath, "utf8"));
  const candidateRows = [];
  const autofilledRows = [];

  for (const row of rows) {
    if (row["LCSC Part #"]) {
      autofilledRows.push(row);
      candidateRows.push(candidateRow(board, row, { componentCode: row["LCSC Part #"] }, 1, 100, true, "already-set", row["LCSC Part #"]));
      continue;
    }

    const query = queryFor(row);
    const parts = await fetchParts(query);
    const scored = parts
      .map((part) => ({ part, score: scorePart(part, row) }))
      .filter(({ score }) => score > 25)
      .sort((a, b) => {
        if (b.score !== a.score) return b.score - a.score;
        if ((b.part.componentLibraryType === "base") !== (a.part.componentLibraryType === "base")) {
          return b.part.componentLibraryType === "base" ? 1 : -1;
        }
        return firstPrice(a.part) - firstPrice(b.part);
      });

    const selected = scored[0];
    const highConfidence = Boolean(selected && selected.score >= 78 && packageMatches(selected.part, row) && containsValue(selected.part, row));
    autofilledRows.push({ ...row, "LCSC Part #": highConfidence ? selected.part.componentCode : row["LCSC Part #"] });

    if (!scored.length) {
      candidateRows.push(candidateRow(board, row, null, "", 0, false, "no-good-candidate", query));
      continue;
    }

    scored.slice(0, 5).forEach(({ part, score }, index) => {
      candidateRows.push(candidateRow(
        board,
        row,
        part,
        index + 1,
        score,
        highConfidence && index === 0,
        highConfidence && index === 0 ? "auto-selected" : "review",
        query,
      ));
    });
  }

  await writeFile(path.join(ROOT, board, "bom_autofilled.csv"), writeCsv(autofilledRows, ["Comment", "Designator", "Footprint", "LCSC Part #"]));
  await writeFile(
    path.join(ROOT, board, "part_candidates.csv"),
    writeCsv(candidateRows, ["Board", "Comment", "Designator", "Footprint", "Query", "Rank", "Score", "Selected", "Reason", "LCSC Part #", "Type", "Stock", "Price", "Manufacturer", "MPN", "Package", "Description"]),
  );
  console.log(`${board}: selected ${autofilledRows.filter((row) => row["LCSC Part #"]).length}/${autofilledRows.length}`);
}

for (const board of BOARDS) {
  await processBoard(board);
}
