// Runs core's canonical config lint over config/CameraUnlock.ini, the committed file, and over
// every distinct CameraUnlock.ini the differential test migrated (the folder it names as the
// argument).
//
// A migrated file writes a value, not default, on a row where the player's imported value
// differs from what Defaults.ini gives. The lint reports that as a committed file's problem, so
// it is the one problem a migrated file may have.
//
// The table marks CycleTrackingModeKey PerGame(), because Arx binds G to drinking a mana potion
// and the cycle chord is Ctrl+Shift+J. The lint is run with that row as this repo's per_game,
// which core's data/config-format.json has to record for conformance to pass.
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { lintCanonicalConfig } from "../../cameraunlock-core/scripts/check-canonical-config.mjs";

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const migratedDir = process.argv[2];
if (!migratedDir) throw new Error("usage: node lint-migrated.mjs <folder of migrated files>");

const options = { dialect: "native", perGame: ["CycleTrackingModeKey"] };
const HOLDS_A_VALUE = /\b(holds a value|hold values), and data\/config-format\.json per_game /;
const failures = [];

const committed = path.join(repo, "config", "CameraUnlock.ini");
for (const problem of lintCanonicalConfig(fs.readFileSync(committed), options)) {
  failures.push(`config/CameraUnlock.ini: ${problem}`);
}

const files = fs.readdirSync(migratedDir).filter((f) => f.endsWith(".ini"));
if (files.length === 0) throw new Error(`${migratedDir} holds no migrated files`);
let withValues = 0;
for (const file of files) {
  let holdsAValue = false;
  for (const problem of lintCanonicalConfig(fs.readFileSync(path.join(migratedDir, file)), options)) {
    if (HOLDS_A_VALUE.test(problem)) holdsAValue = true;
    else failures.push(`${file}: ${problem}`);
  }
  if (holdsAValue) withValues++;
}

if (failures.length > 0) {
  for (const f of failures) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log(
  `canonical config lint: the committed file and ${files.length} migrated files pass (${withValues} set a row to a value, as an import does where the player's value is not Defaults.ini's)`,
);
