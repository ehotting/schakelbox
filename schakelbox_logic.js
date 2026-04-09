// Schakelbox V7 - Kernlogica (geëxtraheerd voor testing)
// Identiek aan de logica in schakelbox_simulator_v7.html en schakelbox_v7.ino

const RAIL_50C = 0, RAIL_50D = 1;
const TRAFO_PRIM_0 = 2, TRAFO_PRIM_1 = 3, TRAFO_PRIM_2 = 4;
const TRAFO_SEC_0 = 5, TRAFO_SEC_1 = 6, TRAFO_SEC_2 = 7;
const RAIL_10_1 = 8, RAIL_10_2 = 9;
const NUM_NODES = 10;

const NODE_NAMEN = [
  '50kV-C', '50kV-D',
  'Prim-T1', 'Prim-T2', 'Prim-T3',
  'Sec-T1', 'Sec-T2', 'Sec-T3',
  '10kV-1', '10kV-2'
];

const VELD_NAMEN = ['Veld 51', 'Veld 53', 'Veld 54'];
const RS_LABELS = ['RS C', 'RS D', 'RS 1', 'RS 2'];

function createState() {
  return {
    rs: [[false,false,false,false],[false,false,false,false],[false,false,false,false]],
    vs: [[false,false],[false,false],[false,false]],
    koppelPrim: false,
    koppelSec: false,
    aard: [[false,false,false],[false,false,false],[false,false,false]],
    fouten: [],  // verzamelt fouten per cyclus
  };
}

function createStartScenario() {
  return {
    rs: [[false,true,false,true],[true,false,true,false],[false,true,false,true]],
    vs: [[true,true],[true,true],[false,false]],
    koppelPrim: true,
    koppelSec: false,
    aard: [[false,false,false],[false,false,false],[false,false,false]],
    fouten: [],
  };
}

// Spanning propagatie (bidirectioneel)
function propageer(s, a, b) {
  if (s[a] && !s[b]) { s[b] = true; return true; }
  if (s[b] && !s[a]) { s[a] = true; return true; }
  return false;
}

function berekenSpanning(state, skipVeld, skipRs) {
  const s = new Array(NUM_NODES).fill(false);
  s[RAIL_50D] = true;

  let gewijzigd = true;
  while (gewijzigd) {
    gewijzigd = false;

    if (state.koppelPrim)
      gewijzigd |= propageer(s, RAIL_50C, RAIL_50D);

    for (let v = 0; v < 3; v++) {
      const prim = TRAFO_PRIM_0 + v;
      const sec = TRAFO_SEC_0 + v;

      if (state.rs[v][0] && !(v === skipVeld && skipRs === 0))
        gewijzigd |= propageer(s, RAIL_50C, prim);
      if (state.rs[v][1] && !(v === skipVeld && skipRs === 1))
        gewijzigd |= propageer(s, RAIL_50D, prim);
      if (state.vs[v][0] && state.vs[v][1])
        gewijzigd |= propageer(s, prim, sec);
      if (state.rs[v][2] && !(v === skipVeld && skipRs === 2))
        gewijzigd |= propageer(s, sec, RAIL_10_1);
      if (state.rs[v][3] && !(v === skipVeld && skipRs === 3))
        gewijzigd |= propageer(s, sec, RAIL_10_2);
    }

    if (state.koppelSec)
      gewijzigd |= propageer(s, RAIL_10_1, RAIL_10_2);
  }
  return s;
}

// Spanning op een aardpunt
function aardPuntSpanning(state, spanning, v, punt) {
  if (punt === 0) return spanning[TRAFO_PRIM_0 + v];
  if (punt === 2) return spanning[TRAFO_SEC_0 + v];
  return (state.vs[v][0] && spanning[TRAFO_PRIM_0 + v])
      || (state.vs[v][1] && spanning[TRAFO_SEC_0 + v]);
}

// RS nodes aan weerszijden
function rsNodes(v, r) {
  return [
    [RAIL_50C, TRAFO_PRIM_0 + v],
    [RAIL_50D, TRAFO_PRIM_0 + v],
    [TRAFO_SEC_0 + v, RAIL_10_1],
    [TRAFO_SEC_0 + v, RAIL_10_2]
  ][r];
}

// Schakelfout check (soort 1): RS wisselt, check potentiaalverschil + VS
function checkSchakelfout(state, veld, rsIdx) {
  const [a, b] = rsNodes(veld, rsIdx);
  const s = berekenSpanning(state, veld, rsIdx);
  if (s[a] === s[b]) return null;
  const vsDicht = (rsIdx <= 1) ? state.vs[veld][0] : state.vs[veld][1];
  if (vsDicht) {
    return { soort: 1, veld: VELD_NAMEN[veld], bericht: `${RS_LABELS[rsIdx]} geschakeld onder belasting!` };
  }
  return null;
}

// Uitval check (soort 2): rail verliest spanning
function checkUitval(prevSpanning, spanning) {
  const fouten = [];
  if (prevSpanning[RAIL_10_1] && !spanning[RAIL_10_1])
    fouten.push({ soort: 2, veld: 'SYSTEEM', bericht: '10kV Rail 1 geen spanning!' });
  if (prevSpanning[RAIL_10_2] && !spanning[RAIL_10_2])
    fouten.push({ soort: 2, veld: 'SYSTEEM', bericht: '10kV Rail 2 geen spanning!' });
  return fouten;
}

// Railkoppeling check (soort 1 of 3)
function checkRailkoppeling(state, spanning) {
  const fouten = [];
  for (let v = 0; v < 3; v++) {
    // 50kV: beide RS dicht zonder koppelveld = fout 1
    if (state.rs[v][0] && state.rs[v][1] && !state.koppelPrim) {
      fouten.push({ soort: 1, veld: VELD_NAMEN[v], bericht: 'RS C+D dicht ZONDER koppelveld prim!' });
    }
    // 10kV: beide RS dicht zonder koppelveld = fout 1
    if (state.rs[v][2] && state.rs[v][3] && !state.koppelSec) {
      fouten.push({ soort: 1, veld: VELD_NAMEN[v], bericht: 'RS 1+2 dicht ZONDER koppelveld sec!' });
    }
  }
  return fouten;
}

// Verkeerde volgorde check (soort 4)
function checkVolgorde(prevVs, vs) {
  const fouten = [];
  for (let v = 0; v < 3; v++) {
    if (prevVs[v][0] && !vs[v][0] && vs[v][1])
      fouten.push({ soort: 4, veld: VELD_NAMEN[v], bericht: 'VS-prim UIT terwijl VS-sec IN.' });
    if (!prevVs[v][1] && vs[v][1] && !vs[v][0])
      fouten.push({ soort: 4, veld: VELD_NAMEN[v], bericht: 'VS-sec IN terwijl VS-prim UIT.' });
  }
  return fouten;
}

// Aarding fout check (soort 1): spanning op geaard punt
function checkAardingFout(state, spanning) {
  const fouten = [];
  for (let v = 0; v < 3; v++) {
    for (let p = 0; p < 3; p++) {
      if (state.aard[v][p] && aardPuntSpanning(state, spanning, v, p)) {
        fouten.push({ soort: 1, veld: VELD_NAMEN[v], bericht: `Spanning op geaard punt ${p + 1}!` });
      }
    }
  }
  return fouten;
}

module.exports = {
  RAIL_50C, RAIL_50D, TRAFO_PRIM_0, TRAFO_SEC_0, RAIL_10_1, RAIL_10_2, NUM_NODES,
  NODE_NAMEN, VELD_NAMEN, RS_LABELS,
  createState, createStartScenario,
  berekenSpanning, aardPuntSpanning, rsNodes,
  checkSchakelfout, checkUitval, checkRailkoppeling, checkVolgorde, checkAardingFout,
};
