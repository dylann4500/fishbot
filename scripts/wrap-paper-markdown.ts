import { readFileSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";

const source = resolve(process.argv[2] ?? "paper/fishbot_v03.tex");
const output = resolve(process.argv[3] ?? "paper/FISHBOT_V03_OVERLEAF.md");
const latex = readFileSync(source, "utf8").trimEnd();
const fence = "```";
writeFileSync(output, [
  "# FishBot v0.3 paper - Overleaf-ready LaTeX",
  "",
  "Copy the complete fenced block into `main.tex` in a blank Overleaf project. It is self-contained and uses only standard TeX Live packages.",
  "",
  `${fence}latex`,
  latex,
  fence,
  "",
].join("\n"));
