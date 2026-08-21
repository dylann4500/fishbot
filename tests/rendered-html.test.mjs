import assert from "node:assert/strict";
import { access, readFile } from "node:fs/promises";
import test from "node:test";

async function render() {
  const workerUrl = new URL("../dist/server/index.js", import.meta.url);
  workerUrl.searchParams.set("test", `${process.pid}-${Date.now()}`);
  const { default: worker } = await import(workerUrl.href);
  return worker.fetch(
    new Request("http://fishlab.test/", { headers: { accept: "text/html", host: "fishlab.test" } }),
    { ASSETS: { fetch: async () => new Response("Not found", { status: 404 }) } },
    { waitUntil() {}, passThroughOnException() {} },
  );
}

test("server-renders the FishLab research workbench", async () => {
  const response = await render();
  assert.equal(response.status, 200);
  assert.match(response.headers.get("content-type") ?? "", /^text\/html\b/i);
  const html = await response.text();
  assert.match(html, /<title>FishLab — What does optimal Fish look like\?<\/title>/i);
  assert.match(html, /What does/);
  assert.match(html, /optimal/);
  assert.match(html, /Experiment configuration/);
  assert.match(html, /Bayesian detective/);
  assert.match(html, /FishBot v0\.3/);
  assert.match(html, /Turn-starvation specialist/);
  assert.match(html, /Canadian Fish/);
  assert.match(html, /https?:\/\/fishlab\.test\/og\.png/);
  assert.doesNotMatch(html, /codex-preview|Your site is taking shape|SkeletonPreview/);
});

test("ships the complete simulation and removes starter artifacts", async () => {
  const [engine, client, packageJson, methodology, socialCard] = await Promise.all([
    readFile(new URL("../lib/fish-engine.ts", import.meta.url), "utf8"),
    readFile(new URL("../components/fish-lab.tsx", import.meta.url), "utf8"),
    readFile(new URL("../package.json", import.meta.url), "utf8"),
    readFile(new URL("../docs/METHODOLOGY.md", import.meta.url), "utf8"),
    access(new URL("../public/og.png", import.meta.url)),
  ]);
  assert.match(engine, /export function simulateGame/);
  assert.match(engine, /export function runBatch/);
  assert.match(client, /Outliers and replay candidates/);
  assert.match(client, /Pivotal only/);
  assert.match(client, /What each computer actually does/);
  assert.match(methodology, /Monte Carlo CFR/);
  assert.doesNotMatch(packageJson, /react-loading-skeleton|site-creator-vinext-starter/);
  assert.equal(socialCard, undefined);
  await assert.rejects(access(new URL("../app/_sites-preview/SkeletonPreview.tsx", import.meta.url)));
});
