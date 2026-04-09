const { Given, When, Then, Before } = require('@cucumber/cucumber');
const assert = require('assert');
const logic = require('../../schakelbox_logic');

let state, prevSpanning, spanning, prevVs, fouten;

function herbereken() {
  spanning = logic.berekenSpanning(state);
}

Before(function () {
  state = logic.createState();
  prevSpanning = new Array(logic.NUM_NODES).fill(false);
  prevVs = [[false,false],[false,false],[false,false]];
  spanning = logic.berekenSpanning(state);
  fouten = [];
});

// === GEGEVEN ===

Given('een lege schakelbox', function () {
  state = logic.createState();
  spanning = logic.berekenSpanning(state);
  prevSpanning = [...spanning];
  prevVs = state.vs.map(v => [...v]);
});

Given('de startpositie', function () {
  state = logic.createStartScenario();
  spanning = logic.berekenSpanning(state);
  prevSpanning = [...spanning];
  prevVs = state.vs.map(v => [...v]);
});

Given('koppelveld primair is dicht', function () {
  state.koppelPrim = true;
  herbereken();
});

Given('koppelveld secundair is dicht', function () {
  state.koppelSec = true;
  herbereken();
});

const trafoIdx = { '1': 0, '2': 1, '3': 2 };
const rsMap = { 'RS-C': 0, 'RS-D': 1, 'RS-1': 2, 'RS-2': 3 };

Given('trafo {int} {word} is dicht', function (t, schakelaar) {
  const v = t - 1;
  if (schakelaar.startsWith('RS-')) {
    state.rs[v][rsMap[schakelaar]] = true;
  } else if (schakelaar === 'VS-prim') {
    state.vs[v][0] = true;
  } else if (schakelaar === 'VS-sec') {
    state.vs[v][1] = true;
  }
  herbereken();
  fouten.push(...logic.checkRailkoppeling(state, spanning));
});

Given('trafo {int} {word} is open', function (t, schakelaar) {
  const v = t - 1;
  if (schakelaar.startsWith('RS-')) {
    state.rs[v][rsMap[schakelaar]] = false;
  } else if (schakelaar === 'VS-prim') {
    state.vs[v][0] = false;
  } else if (schakelaar === 'VS-sec') {
    state.vs[v][1] = false;
  }
  herbereken();
});

Given('trafo {int} punt {int} is geaard', function (t, punt) {
  state.aard[t - 1][punt - 1] = true;
});

// === ALS ===

When('ik trafo {int} RS-C sluit', function (t) { schakelRS(t-1, 0, true); });
When('ik trafo {int} RS-D sluit', function (t) { schakelRS(t-1, 1, true); });
When('ik trafo {int} RS-1 sluit', function (t) { schakelRS(t-1, 2, true); });
When('ik trafo {int} RS-2 sluit', function (t) { schakelRS(t-1, 3, true); });
When('ik trafo {int} RS-C open', function (t) { schakelRS(t-1, 0, false); });
When('ik trafo {int} RS-D open', function (t) { schakelRS(t-1, 1, false); });
When('ik trafo {int} RS-1 open', function (t) { schakelRS(t-1, 2, false); });
When('ik trafo {int} RS-2 open', function (t) { schakelRS(t-1, 3, false); });

function schakelRS(v, r, dicht) {
  prevSpanning = [...spanning];
  prevVs = state.vs.map(vs => [...vs]);
  state.rs[v][r] = dicht;
  const fout = logic.checkSchakelfout(state, v, r);
  if (fout) fouten.push(fout);
  herbereken();
  fouten.push(...logic.checkUitval(prevSpanning, spanning));
  fouten.push(...logic.checkRailkoppeling(state, spanning));
}

When('ik trafo {int} VS-prim sluit', function (t) { schakelVS(t-1, 0, true); });
When('ik trafo {int} VS-sec sluit', function (t) { schakelVS(t-1, 1, true); });
When('ik trafo {int} VS-prim open', function (t) { schakelVS(t-1, 0, false); });
When('ik trafo {int} VS-sec open', function (t) { schakelVS(t-1, 1, false); });

function schakelVS(v, idx, dicht) {
  prevVs = state.vs.map(vs => [...vs]);
  state.vs[v][idx] = dicht;
  fouten.push(...logic.checkVolgorde(prevVs, state.vs));
  herbereken();
}

When('ik koppelveld primair open', function () {
  prevSpanning = [...spanning];
  state.koppelPrim = false;
  herbereken();
  fouten.push(...logic.checkRailkoppeling(state, spanning));
});

When('ik koppelveld primair sluit', function () {
  state.koppelPrim = true;
  herbereken();
});

When('ik trafo {int} punt {int} aard', function (t, punt) {
  state.aard[t-1][punt-1] = true;
  herbereken();
  fouten.push(...logic.checkAardingFout(state, spanning));
});

// === DAN ===

Then('heeft Rail C spanning', function () {
  herbereken();
  assert.strictEqual(spanning[logic.RAIL_50C], true, 'Rail C zou spanning moeten hebben');
});

Then('heeft Rail C geen spanning', function () {
  herbereken();
  assert.strictEqual(spanning[logic.RAIL_50C], false, 'Rail C zou geen spanning moeten hebben');
});

Then('heeft Rail D spanning', function () {
  herbereken();
  assert.strictEqual(spanning[logic.RAIL_50D], true, 'Rail D zou spanning moeten hebben');
});

Then('heeft Rail 1 spanning', function () {
  herbereken();
  assert.strictEqual(spanning[logic.RAIL_10_1], true, 'Rail 1 zou spanning moeten hebben');
});

Then('heeft Rail 1 geen spanning', function () {
  herbereken();
  assert.strictEqual(spanning[logic.RAIL_10_1], false);
});

Then('heeft Rail 2 spanning', function () {
  herbereken();
  assert.strictEqual(spanning[logic.RAIL_10_2], true, 'Rail 2 zou spanning moeten hebben');
});

Then('heeft Rail 2 geen spanning', function () {
  herbereken();
  assert.strictEqual(spanning[logic.RAIL_10_2], false);
});

Then('heeft trafoPrim van trafo {int} spanning', function (t) {
  herbereken();
  assert.strictEqual(spanning[logic.TRAFO_PRIM_0 + t - 1], true);
});

Then('heeft het middenpunt van trafo {int} spanning', function (t) {
  herbereken();
  const v = t - 1;
  assert.strictEqual(logic.aardPuntSpanning(state, spanning, v, 1), true);
});

Then('volgt een schakelfout', function () {
  const sf = fouten.filter(f => f.soort === 1);
  assert.ok(sf.length > 0, `Verwachte schakelfout maar geen gevonden. Fouten: ${JSON.stringify(fouten)}`);
});

Then('volgt geen schakelfout', function () {
  const sf = fouten.filter(f => f.soort === 1);
  assert.strictEqual(sf.length, 0, `Onverwachte schakelfout: ${JSON.stringify(sf)}`);
});

Then('volgt een uitval op Rail {int}', function (rail) {
  const uv = fouten.filter(f => f.soort === 2 && f.bericht.includes(`Rail ${rail}`));
  assert.ok(uv.length > 0, `Verwachte uitval Rail ${rail} maar niet gevonden. Fouten: ${JSON.stringify(fouten)}`);
});

Then('volgt geen uitval', function () {
  const uv = fouten.filter(f => f.soort === 2);
  assert.strictEqual(uv.length, 0, `Onverwachte uitval: ${JSON.stringify(uv)}`);
});

Then('volgt een railkoppeling-fout', function () {
  const rk = fouten.filter(f => f.soort === 1 && f.bericht.includes('ZONDER koppelveld'));
  assert.ok(rk.length > 0, `Verwachte railkoppeling-fout maar niet gevonden. Fouten: ${JSON.stringify(fouten)}`);
});

Then('volgt geen railkoppeling-fout', function () {
  const rk = fouten.filter(f => f.soort === 1 && f.bericht.includes('ZONDER koppelveld'));
  assert.strictEqual(rk.length, 0, `Onverwachte railkoppeling-fout: ${JSON.stringify(rk)}`);
});

Then('volgt een volgorde-fout', function () {
  const vf = fouten.filter(f => f.soort === 4);
  assert.ok(vf.length > 0, `Verwachte volgorde-fout maar niet gevonden. Fouten: ${JSON.stringify(fouten)}`);
});

Then('volgt geen volgorde-fout', function () {
  const vf = fouten.filter(f => f.soort === 4);
  assert.strictEqual(vf.length, 0, `Onverwachte volgorde-fout: ${JSON.stringify(vf)}`);
});

Then('staat er spanning op geaard punt {int} van trafo {int}', function (punt, t) {
  herbereken();
  assert.ok(logic.aardPuntSpanning(state, spanning, t-1, punt-1), `Punt ${punt} van trafo ${t} zou spanning moeten hebben`);
});
