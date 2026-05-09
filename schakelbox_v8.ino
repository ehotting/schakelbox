/* Schakelbox V8 - Power Flow Model
 * Laatste wijziging: 09/05/2026
 *
 * Simuleert stroomvoorziening door een onderstation met 3 trafovelden.
 * Spanning wordt gepropageerd door gesloten schakelaars (bidirectioneel).
 *
 * Topologie per veld:
 *   50kV Rail C --[RS-C]--+                    +--[RS-1]-- 10kV Rail 1
 *                         |                    |
 *                     trafoPrim --[VS]-- trafoSec
 *                         |                    |
 *   50kV Rail D --[RS-D]--+                    +--[RS-2]-- 10kV Rail 2
 *
 * Bron: Rail D heeft altijd spanning. Rail C alleen via Koppelveld Prim.
 *
 * Foutsoorten:
 *   1. SCHAKELFOUT: RS geschakeld onder belasting, of spanning op geaard punt
 *   2. UITVAL: 10kV rail verliest spanning (klanten zonder stroom)
 *   3. ONJUISTE RAILKOPPELING: beide RS >60s tegelijk dicht
 *   4. VERKEERDE VOLGORDE: VS in verkeerde volgorde geschakeld
 *   5. FOUTE PIN: verkeerde aansluiting bij storing-puzzel
 *
 * Aarding: per trafo 3 spanningspunten + 1 aardepunt.
 *   Monteur prikt bananenstekker van spanningspunt naar aardepunt.
 *   Fout als spanning op geaard punt staat.
 *
 * Hardware: Arduino Mega 2560 R3
 *
 * Pin layout:
 *   Rechts:  D2-D13 outputs (LEDs), D14-D15 koppelvelden,
 *            D16-D24 spanningspunten, D6-D7 buzzers
 *   Onder:   D26-D43 schakelaars (3x6), D45-D47 aardepunten
 *   Links:   A0 VSENSE, A1 storing1 fout, A2-A7 storing1 goed,
 *            A8 storing2 TX, A9 storing2 RX, D48-D49 storingsknoppen
 */

#include <Arduino.h>

// =================================================================
//  CONFIGURATIE
// =================================================================

#define DEBUG            true
#define STATUS_INTERVAL  3000
#define DEBOUNCE_MS      25      // input moet zo lang stabiel zijn voor accept
#define VOLT_SENSE_ENABLED  false  // A0 voltage-sense check; floating pin → ruis
#define VOLT_SENSE_SAMPLES  10     // aantal opeenvolgende samples >threshold voor trigger
#define BLINK_INTERVAL        500
#define BLINK_FAST_INTERVAL   200   // LED 'verloren' state — sneller knipperen

// =================================================================
//  BUZZER PATTERNS
// =================================================================
// Eén buzzer (D6, passieve piezo via tone()). Patronen zijn een rij
// {freq, ms} met sentinel {0,0}. freq=0 binnen patroon = stilte ms lang.
// Event-patronen krijgen per FoutKind een max duur (maxMsFor) en eventueel
// een loop-flag (repeatFor). Ongoing-patronen herhalen continu zolang state
// actief is; bij meerdere ongoing fouten speelt de hoogste prio (geen multiplex).

struct Note { uint16_t freq; uint16_t ms; };
struct Player {
  const Note* pattern;
  uint8_t idx;
  unsigned long noteStart;
  bool noteActive;
};

enum StepStatus { STEP_RUNNING, STEP_NOTE_DONE, STEP_END };

enum FoutKind {
  F_NONE = 0,
  F_UITVAL,            // T1: rail spanningsloos — perm
  F_AARDPUNT_KORT,     // T2: spanning op geaard punt — perm
  F_KOPPEL_FOUT,       // T3: RS dicht zonder koppelveld — 8s event
  F_VLAMBOOG,          // T4: RS schakelen onder belasting — 5s event
  F_VERLOREN,          // T5: storing 1 verloren — perm
  F_AARDING,           // T6: werken zonder aarding — 5s event
  F_VOLGORDE,          // T7: VS verkeerde volgorde — 3s event
  F_RAILKOPPELING      // beide RS dicht >60s — 3 blips, eenmalig
};

const Note PAT_UITVAL[]          = { {180, 500}, {0, 250}, {180, 500}, {0, 0} };                       // T1
const Note PAT_AARDPUNT_KORT[]   = { {250, 800}, {0, 0} };                                              // T2
const Note PAT_KOPPEL_FOUT[]     = { {250, 250}, {0, 50}, {350, 250}, {0, 50}, {0, 0} };                // T3
// Sad trombone (Price-is-Right losing horn): chromatische daling F4-E4-Eb4-D4,
// drie korte noten + 4e lang aangehouden = "ta-da-da-daaaa".
const Note PAT_VLAMBOOG[]        = { {349, 200}, {0, 60}, {330, 200}, {0, 60}, {311, 200}, {0, 60}, {294, 900}, {0, 0} };  // T4
const Note PAT_VERLOREN[]        = { {400, 200}, {0, 200}, {0, 0} };                                    // T5
const Note PAT_AARDING[]         = { {200, 150}, {0, 100}, {200, 150}, {0, 100}, {200, 150}, {0, 0} };  // T6
const Note PAT_VOLGORDE[]        = { {200, 200}, {0, 50}, {400, 200}, {0, 50}, {0, 0} };                // T7 (200/400 alt)
const Note PAT_RAILKOPPELING[]   = { {400, 100}, {0, 100}, {400, 100}, {0, 100}, {400, 100}, {0, 0} };  // 3 blips 400Hz

const Note* patternFor(FoutKind k) {
  switch (k) {
    case F_UITVAL:         return PAT_UITVAL;
    case F_AARDPUNT_KORT:  return PAT_AARDPUNT_KORT;
    case F_KOPPEL_FOUT:    return PAT_KOPPEL_FOUT;
    case F_VLAMBOOG:       return PAT_VLAMBOOG;
    case F_VERLOREN:       return PAT_VERLOREN;
    case F_AARDING:        return PAT_AARDING;
    case F_VOLGORDE:       return PAT_VOLGORDE;
    case F_RAILKOPPELING:  return PAT_RAILKOPPELING;
    default:               return nullptr;
  }
}

// Hoeveel ms speelt het kort-patroon maximaal? Wordt in updateBuzzer gecapped.
unsigned long maxMsFor(FoutKind k) {
  switch (k) {
    case F_KOPPEL_FOUT:    return 8000;  // T3
    case F_VLAMBOOG:       return 5000;  // T4
    case F_AARDING:        return 5000;  // T6
    case F_VOLGORDE:       return 3000;  // T7
    case F_RAILKOPPELING:  return 700;   // 3 blips, geen herhaling
    default:               return 3000;
  }
}

// Speelt het patroon door (loop bij sentinel) of stopt het na één doorgang?
bool repeatFor(FoutKind k) {
  switch (k) {
    case F_RAILKOPPELING:  return false;  // exact 3 blips
    default:               return true;   // loop tot maxMs hit
  }
}

// =================================================================
//  PIN DEFINITIES
// =================================================================

// --- OUTPUTS (rechter reep) ---
#define LED_RAIL_C       2       // Rail C spanning LED
#define LED_RAIL_D       3       // Rail D spanning LED
#define LED_RAIL_1       4       // Rail 1 + Duinpadweg LED (parallel)
#define LED_RAIL_2       5       // Rail 2 + Frederiklaan LED (parallel)

// Aangenomen rail-belasting (Ampere). Detectie blijft binair (stroom ja/nee);
// constants gebruikt voor documentatie en eventuele uitbreidingen.
#define LOAD_RAIL_10_1   200     // Duinpadweg
#define LOAD_RAIL_10_2   500     // Frederiklaan
#define BUZZER_KORT_PIN  6       // Buzzer (D6, passieve piezo) — alle event- en ongoing-patronen
#define LED_STOR_T1_P    8       // Storing LED trafo 1, VS-prim
#define LED_STOR_T1_S    9       // Storing LED trafo 1, VS-sec
#define LED_STOR_T2_P    10      // Storing LED trafo 2, VS-prim
#define LED_STOR_T2_S    11      // Storing LED trafo 2, VS-sec
#define LED_STOR_T3_P    12      // Storing LED trafo 3, VS-prim
#define LED_STOR_T3_S    13      // Storing LED trafo 3, VS-sec

// --- INPUTS koppelvelden (rechter reep) ---
#define KOPPEL_PRIM_PIN  14      // INPUT_PULLUP, LOW = dicht
#define KOPPEL_SEC_PIN   15

// --- INPUTS spanningspunten / aarding (rechter reep D16-D24) ---
// Per trafo 3 punten: boven VS-P, midden, onder VS-S
// INPUT_PULLUP: LOW = geaard (bananenstekker naar aardepunt)
const uint8_t SPAN_PUNT_PINS[3][3] = {
  { 16, 17, 18 },  // T1: boven, midden, onder
  { 19, 20, 21 },  // T2
  { 22, 23, 24 }   // T3
};

// --- INPUTS schakelaars (onderste reep D26-D43) ---
// Per trafo 6 pins: RS-C, RS-D, VS-prim, VS-sec, RS-1, RS-2
struct VeldPins {
  uint8_t rsC, rsD, vsPrim, vsSec, rs1, rs2;
  uint8_t ledPrim, ledSec;
  const char* naam;
};

const VeldPins VELD[3] = {
  { 26, 27, 28, 29, 30, 31, LED_STOR_T1_P, LED_STOR_T1_S, "Veld 51" },
  { 32, 33, 34, 35, 36, 37, LED_STOR_T2_P, LED_STOR_T2_S, "Veld 53" },
  { 38, 39, 40, 41, 42, 43, LED_STOR_T3_P, LED_STOR_T3_S, "Veld 54" }
};

// --- OUTPUTS aardepunten (onderste reep D45-D47, OUTPUT LOW) ---
const uint8_t AARD_PUNT_PINS[3] = { 45, 46, 47 };

// --- INPUTS storingsknoppen (linker reep) ---
#define BTN_STORING1_PIN 48      // T1 stuk
#define BTN_STORING2_PIN 49      // VS-prim T2 stuk

// --- STORING 1 puzzel: A1=fout, A2-A7=goed (linker reep) ---
#define SENSE_FOUT_PIN   A1      // INPUT_PULLUP, LOW = fout aangesloten
#define NUM_SENSE_GOED   6
const uint8_t SENSE_GOED_PINS[NUM_SENSE_GOED] = { A2, A3, A4, A5, A6, A7 };

// --- STORING 2 pair: A8=TX(GND), A9=RX (linker reep) ---
#define PAIR_TX_PIN      A8      // OUTPUT LOW (GND bron)
#define PAIR_RX_PIN      A9      // INPUT_PULLUP, LOW = verbonden

// --- ANALOOG ---
#define VOLT_SENSE_PIN   A0
#define ADC_REF_MV       5000
#define VTHRESH_MV       4000

// --- TIMERS ---
#define KOPPELING_MAX_MS 60000UL

// =================================================================
//  NETWERK TOPOLOGIE
// =================================================================

enum Node : uint8_t {
  RAIL_50C, RAIL_50D,
  TRAFO_PRIM_0, TRAFO_PRIM_1, TRAFO_PRIM_2,
  TRAFO_SEC_0, TRAFO_SEC_1, TRAFO_SEC_2,
  RAIL_10_1, RAIL_10_2,
  NUM_NODES
};

// RS indices (intern)
#define RS_C_IDX 0
#define RS_D_IDX 1
#define RS_1_IDX 2
#define RS_2_IDX 3

const char* RS_NAAM[] = { "RS-C", "RS-D", "RS-1", "RS-2" };
const char* VELD_NAAM[] = { "Veld 51", "Veld 53", "Veld 54" };

// =================================================================
//  TOESTAND
// =================================================================

bool rs[3][4];                   // [veld][RS_C, RS_D, RS_1, RS_2]
bool vs[3][2];                   // [veld][prim, sec]
bool koppelPrim, koppelSec;
bool prevRs[3][4];
bool prevVs[3][2];

bool spanning[NUM_NODES];
bool prevSpanning[NUM_NODES];

// Railkoppeling timers
unsigned long kopStart_50[3], kopStart_10[3];
bool kopAlarm_50[3], kopAlarm_10[3];

// Storing
bool btnStoring[2];
bool allSenseGoedOk;
bool senseFoutAangesloten;
bool pairVerbonden;
bool puzzel1Verloren = false;    // foute pin A1 aangesloten tijdens storing 1

// Aarding
bool geaard[3][3];               // [veld][boven, midden, onder]
bool prevAardFout[3][3];         // edge detection

// Buzzer state machines (Player struct gedefinieerd in BUZZER PATTERNS sectie)
Player kortPlayer = { nullptr, 0, 0, false };
Player permPlayer = { nullptr, 0, 0, false };
unsigned long kortStart = 0;
unsigned long kortMaxMs = 3000;
bool kortRepeat = true;
FoutKind permCurrentKind = F_NONE;

// Blink
bool blinkState = false;
unsigned long lastBlink = 0;
bool fastBlinkState = false;
unsigned long lastFastBlink = 0;

// Edge detection (was static in functies, nu global voor consistentie)
bool prevVoltHoog = false;
bool prevSenseGoed[NUM_SENSE_GOED] = {};
bool kopFout_50[3], kopFout_10[3];

// =================================================================
//  DEBOUNCE
// =================================================================
//
// Filtert mains-hum en hand-pickup op INPUT_PULLUP lijnen. Een input
// moet DEBOUNCE_MS stabiel zijn voordat de nieuwe waarde geaccepteerd
// wordt. Vervangt overal digitalRead() voor digitale input-pins.

struct DebounceState {
  uint8_t stable;
  uint8_t candidate;
  unsigned long lastChange;
};

static DebounceState dbState[NUM_DIGITAL_PINS];

void debounceInit(uint8_t pin) {
  uint8_t v = digitalRead(pin);
  dbState[pin].stable = v;
  dbState[pin].candidate = v;
  dbState[pin].lastChange = 0;
}

uint8_t debouncedRead(uint8_t pin) {
  DebounceState& s = dbState[pin];
  uint8_t raw = digitalRead(pin);
  unsigned long now = millis();
  if (raw != s.candidate) {
    s.candidate = raw;
    s.lastChange = now;
  } else if (raw != s.stable && (now - s.lastChange) >= DEBOUNCE_MS) {
    s.stable = raw;
  }
  return s.stable;
}

// =================================================================
//  SPANNING PROPAGATIE
// =================================================================

static inline bool propageer(bool* s, uint8_t a, uint8_t b) {
  if (s[a] && !s[b]) { s[b] = true; return true; }
  if (s[b] && !s[a]) { s[a] = true; return true; }
  return false;
}

void berekenSpanning(int skipVeld = -1, int skipRs = -1) {
  memset(spanning, false, sizeof(spanning));
  spanning[RAIL_50D] = true;

  bool gewijzigd = true;
  while (gewijzigd) {
    gewijzigd = false;

    if (koppelPrim)
      gewijzigd |= propageer(spanning, RAIL_50C, RAIL_50D);

    for (int v = 0; v < 3; v++) {
      uint8_t prim = TRAFO_PRIM_0 + v;
      uint8_t sec  = TRAFO_SEC_0 + v;

      if (rs[v][RS_C_IDX] && !(v == skipVeld && skipRs == RS_C_IDX))
        gewijzigd |= propageer(spanning, RAIL_50C, prim);
      if (rs[v][RS_D_IDX] && !(v == skipVeld && skipRs == RS_D_IDX))
        gewijzigd |= propageer(spanning, RAIL_50D, prim);
      if (vs[v][0] && vs[v][1])
        gewijzigd |= propageer(spanning, prim, sec);
      if (rs[v][RS_1_IDX] && !(v == skipVeld && skipRs == RS_1_IDX))
        gewijzigd |= propageer(spanning, sec, RAIL_10_1);
      if (rs[v][RS_2_IDX] && !(v == skipVeld && skipRs == RS_2_IDX))
        gewijzigd |= propageer(spanning, sec, RAIL_10_2);
    }

    if (koppelSec)
      gewijzigd |= propageer(spanning, RAIL_10_1, RAIL_10_2);
  }
}

// Spanning op een aardpunt
bool aardPuntSpanning(int v, int punt) {
  if (punt == 0) return spanning[TRAFO_PRIM_0 + v];
  if (punt == 2) return spanning[TRAFO_SEC_0 + v];
  // punt 1 = midden
  return (vs[v][0] && spanning[TRAFO_PRIM_0 + v])
      || (vs[v][1] && spanning[TRAFO_SEC_0 + v]);
}

// =================================================================
//  FOUTDETECTIE
// =================================================================

void meldFout(FoutKind kind, const char* veldNaam, const char* bericht) {
  unsigned long now = millis();
  // F_NONE = alleen loggen (state-flags zorgen voor eventueel ongoing alarm)
  if (kind != F_NONE) {
    kortPlayer.pattern = patternFor(kind);
    kortPlayer.idx = 0;
    kortPlayer.noteActive = false;
    kortStart = now;
    kortMaxMs = maxMsFor(kind);
    kortRepeat = repeatFor(kind);
    permPlayer.noteActive = false;
  }

  if (!DEBUG) return;
  Serial.print(F("!!! FOUT "));
  Serial.print((int)kind);
  Serial.print(F(" | "));
  Serial.print(now);
  Serial.print(F("ms | "));
  Serial.print(veldNaam);
  Serial.print(F(" | "));
  Serial.println(bericht);
}

// Soort 1: SCHAKELFOUT
void checkSchakelfout(int veld, int rsIdx) {
  uint8_t nodeA, nodeB;
  switch (rsIdx) {
    case RS_C_IDX: nodeA = RAIL_50C;            nodeB = TRAFO_PRIM_0 + veld; break;
    case RS_D_IDX: nodeA = RAIL_50D;            nodeB = TRAFO_PRIM_0 + veld; break;
    case RS_1_IDX: nodeA = TRAFO_SEC_0 + veld;  nodeB = RAIL_10_1;           break;
    case RS_2_IDX: nodeA = TRAFO_SEC_0 + veld;  nodeB = RAIL_10_2;           break;
  }

  bool backup[NUM_NODES];
  memcpy(backup, spanning, sizeof(spanning));
  berekenSpanning(veld, rsIdx);

  bool spanA = spanning[nodeA];
  bool spanB = spanning[nodeB];
  memcpy(spanning, backup, sizeof(spanning));

  // Vlamboog alleen als de RS een potentiaalverschil overbrugt: in de sim
  // met deze RS open is één kant dood en de andere onder spanning. Geen
  // verschil → bypass aanwezig (bv. via koppelveld of parallelle trafo) →
  // schakelen is veilig.
  if (spanA != spanB) {
    bool vsDicht = (rsIdx <= RS_D_IDX) ? vs[veld][0] : vs[veld][1];
    if (vsDicht) {
      meldFout(F_VLAMBOOG, VELD[veld].naam, "VLAMBOOG: RS geschakeld onder belasting!");
    }
  }
}

// Soort 2: UITVAL
void checkUitval() {
  // F_NONE: alleen log; ongoing T1 wordt opgepikt via collectOngoing zolang
  // de rail spanningloos is.
  if (prevSpanning[RAIL_10_1] && !spanning[RAIL_10_1])
    meldFout(F_NONE, "SYSTEEM", "UITVAL: 10kV Rail 1 geen spanning!");
  if (prevSpanning[RAIL_10_2] && !spanning[RAIL_10_2])
    meldFout(F_NONE, "SYSTEEM", "UITVAL: 10kV Rail 2 geen spanning!");
}

// Soort 1/3: RAILKOPPELING
// Beide RS dicht zonder koppelveld = direct fout 1
// Beide RS dicht met koppelveld = max 60s (fout 3)
void checkRailkoppeling(unsigned long now) {
  for (int v = 0; v < 3; v++) {
    bool beide50 = rs[v][RS_C_IDX] && rs[v][RS_D_IDX];
    bool beide10 = rs[v][RS_1_IDX] && rs[v][RS_2_IDX];

    // 50kV: zonder koppelveld = direct fout 1
    if (beide50 && !koppelPrim) {
      if (!kopFout_50[v]) {
        meldFout(F_KOPPEL_FOUT, VELD[v].naam, "KOPPELVELD: RS-C+D dicht zonder koppelveld prim!");  // T3
        kopFout_50[v] = true;
      }
    } else { kopFout_50[v] = false; }

    // 10kV: zonder koppelveld = direct fout 1
    if (beide10 && !koppelSec) {
      if (!kopFout_10[v]) {
        meldFout(F_KOPPEL_FOUT, VELD[v].naam, "KOPPELVELD: RS-1+2 dicht zonder koppelveld sec!");  // T3
        kopFout_10[v] = true;
      }
    } else { kopFout_10[v] = false; }

    // Met koppelveld: 60s timer
    if (beide50 && koppelPrim) {
      if (kopStart_50[v] == 0) { kopStart_50[v] = now; kopAlarm_50[v] = false; }
      if (!kopAlarm_50[v] && ((long)(now - kopStart_50[v]) >= (long)KOPPELING_MAX_MS)) {
        meldFout(F_RAILKOPPELING, VELD[v].naam, "RAILKOPPELING: RS-C + RS-D >60s!");
        kopAlarm_50[v] = true;
      }
    } else { kopStart_50[v] = 0; kopAlarm_50[v] = false; }

    if (beide10 && koppelSec) {
      if (kopStart_10[v] == 0) { kopStart_10[v] = now; kopAlarm_10[v] = false; }
      if (!kopAlarm_10[v] && ((long)(now - kopStart_10[v]) >= (long)KOPPELING_MAX_MS)) {
        meldFout(F_RAILKOPPELING, VELD[v].naam, "RAILKOPPELING: RS-1 + RS-2 >60s!");
        kopAlarm_10[v] = true;
      }
    } else { kopStart_10[v] = 0; kopAlarm_10[v] = false; }
  }
}

// Soort 1 (aarding): spanning op geaard punt
void checkAardingFout() {
  for (int v = 0; v < 3; v++) {
    for (int p = 0; p < 3; p++) {
      bool fout = geaard[v][p] && aardPuntSpanning(v, p);
      if (fout && !prevAardFout[v][p]) {
        // F_NONE: ongoing T2 wordt door collectOngoing opgepikt zolang de
        // kortsluiting bestaat; geen losse event-toon nodig.
        meldFout(F_NONE, VELD[v].naam, "AARDPUNT: Spanning op geaard punt!");
      }
      prevAardFout[v][p] = fout;
    }
  }
}

// =================================================================
//  INPUTS LEZEN
// =================================================================

void leesSchakelaars() {
  for (int v = 0; v < 3; v++) {
    rs[v][RS_C_IDX] = (debouncedRead(VELD[v].rsC)    == LOW);
    rs[v][RS_D_IDX] = (debouncedRead(VELD[v].rsD)    == LOW);
    rs[v][RS_1_IDX] = (debouncedRead(VELD[v].rs1)    == LOW);
    rs[v][RS_2_IDX] = (debouncedRead(VELD[v].rs2)    == LOW);
    vs[v][0]        = (debouncedRead(VELD[v].vsPrim)  == LOW);
    vs[v][1]        = (debouncedRead(VELD[v].vsSec)   == LOW);
  }
  koppelPrim = (debouncedRead(KOPPEL_PRIM_PIN) == LOW);
  koppelSec  = (debouncedRead(KOPPEL_SEC_PIN)  == LOW);

  // Storingsknoppen
  btnStoring[0] = (debouncedRead(BTN_STORING1_PIN) == LOW);
  btnStoring[1] = (debouncedRead(BTN_STORING2_PIN) == LOW);

  // Scenario 2: VS-prim T2 stuk — forceer IN als storing actief en niet opgelost
  if (btnStoring[1] && !pairVerbonden) {
    vs[1][0] = true;  // VS-prim T2 blijft IN ongeacht schakelaar
  }
}

// Is trafo geaard? (minstens 1 punt)
bool isTrafoGeaard(int v) {
  for (int p = 0; p < 3; p++)
    if (geaard[v][p]) return true;
  return false;
}

void leesStoringPuzzel() {
  // Storing 1: A2-A7 goede pins, A1 foute pin
  bool prevAllOk = allSenseGoedOk;
  allSenseGoedOk = true;
  for (uint8_t i = 0; i < NUM_SENSE_GOED; i++) {
    bool ok = (debouncedRead(SENSE_GOED_PINS[i]) == LOW);
    // Edge: pin wordt aangesloten zonder aarding → fout 1
    if (ok && !prevSenseGoed[i] && btnStoring[0] && !isTrafoGeaard(0)) {
      meldFout(F_AARDING, "STORING 1", "Werken zonder aarding! Dodelijk gevaar!");
    }
    prevSenseGoed[i] = ok;
    if (!ok) allSenseGoedOk = false;
  }

  bool wasFoutAangesloten = senseFoutAangesloten;
  senseFoutAangesloten = (debouncedRead(SENSE_FOUT_PIN) == LOW);
  if (senseFoutAangesloten && !wasFoutAangesloten) {
    if (btnStoring[0] && !isTrafoGeaard(0))
      meldFout(F_AARDING, "STORING 1", "Werken zonder aarding! Dodelijk gevaar!");
    // F_NONE: state-flag puzzel1Verloren triggert ongoing T5 (verloren-pulse)
    // direct, dus geen losse event-toon.
    meldFout(F_NONE, "STORING 1", "Foute pin A1 aangesloten!");
    if (btnStoring[0]) puzzel1Verloren = true;
  }
  // Reset 'verloren' bij toggle storing-1 uit
  if (!btnStoring[0]) puzzel1Verloren = false;

  // Storing 2: A8→A9 pair
  bool wasPairVerbonden = pairVerbonden;
  pairVerbonden = (debouncedRead(PAIR_RX_PIN) == LOW);
  if (pairVerbonden && !wasPairVerbonden && btnStoring[1] && !isTrafoGeaard(1)) {
    meldFout(F_AARDING, "STORING 2", "Werken zonder aarding! Dodelijk gevaar!");
  }
}

void leesAarding() {
  for (int v = 0; v < 3; v++) {
    for (int p = 0; p < 3; p++) {
      geaard[v][p] = (debouncedRead(SPAN_PUNT_PINS[v][p]) == LOW);
    }
  }
}

// =================================================================
//  EASTER EGG
// =================================================================
// Trigger: beide koppelvelden 3× gelijktijdig (binnen 200 ms) toggleren
// binnen 5 seconden. Per ongeluk vrijwel niet te raken in normaal gebruik.
// Effect: 4 snelle audio-sweeps + Cylon-style LED chase, daarna alles weer
// normaal.

#define EE_SYNC_WINDOW_MS  200
#define EE_RESET_MS        5000
#define EE_COOLDOWN_MS     10000
#define EGG_SWEEP_MS       250
#define EGG_NUM_SWEEPS     4
#define EGG_FREQ_LO        200
#define EGG_FREQ_HI        2000

bool prevKoppelPrim = false;
bool prevKoppelSec  = false;
unsigned long lastPrimEdgeMs = 0;
unsigned long lastSecEdgeMs  = 0;
bool eeArmedPrim = false;
bool eeArmedSec  = false;
uint8_t eeSyncCount = 0;
unsigned long eeFirstSyncMs = 0;
unsigned long eeLastTriggerMs = 0;

bool eggActive = false;
unsigned long eggStart = 0;

const uint8_t EGG_LEDS[] = {
  LED_RAIL_C, LED_RAIL_D,
  LED_STOR_T1_P, LED_STOR_T1_S,
  LED_STOR_T2_P, LED_STOR_T2_S,
  LED_STOR_T3_P, LED_STOR_T3_S,
  LED_RAIL_1, LED_RAIL_2
};
const uint8_t EGG_NUM_LEDS = sizeof(EGG_LEDS);

void checkEasterEgg(unsigned long now) {
  bool primEdge = (koppelPrim != prevKoppelPrim);
  bool secEdge  = (koppelSec  != prevKoppelSec);
  prevKoppelPrim = koppelPrim;
  prevKoppelSec  = koppelSec;

  if (primEdge) { lastPrimEdgeMs = now; eeArmedPrim = true; }
  if (secEdge)  { lastSecEdgeMs  = now; eeArmedSec  = true; }

  // Sync-toggle = beide koppelvelden binnen window edged sinds laatste reset.
  if (eeArmedPrim && eeArmedSec) {
    unsigned long delta = (lastPrimEdgeMs > lastSecEdgeMs)
                            ? (lastPrimEdgeMs - lastSecEdgeMs)
                            : (lastSecEdgeMs - lastPrimEdgeMs);
    if (delta <= EE_SYNC_WINDOW_MS) {
      if (eeSyncCount > 0 && (now - eeFirstSyncMs) > EE_RESET_MS) {
        eeSyncCount = 0;
      }
      if (eeSyncCount == 0) eeFirstSyncMs = now;
      eeSyncCount++;
      eeArmedPrim = false;
      eeArmedSec  = false;
      if (eeSyncCount >= 3 && (now - eeLastTriggerMs) > EE_COOLDOWN_MS) {
        eggActive = true;
        eggStart = now;
        eeLastTriggerMs = now;
        eeSyncCount = 0;
        if (DEBUG) Serial.println(F("*** EASTER EGG ***"));
      }
    }
  }

  // Counter resetten als 5 s verstreken zonder de derde sync-toggle.
  if (eeSyncCount > 0 && (now - eeFirstSyncMs) > EE_RESET_MS) {
    eeSyncCount = 0;
    eeArmedPrim = false;
    eeArmedSec  = false;
  }
}

// Speelt af, returnt true zolang de egg loopt (caller skipt dan normale outputs).
bool playEgg(unsigned long now) {
  if (!eggActive) return false;
  unsigned long elapsed = now - eggStart;
  unsigned long total = (unsigned long)EGG_SWEEP_MS * EGG_NUM_SWEEPS;
  if (elapsed >= total) {
    eggActive = false;
    noTone(BUZZER_KORT_PIN);
    for (uint8_t i = 0; i < EGG_NUM_LEDS; i++) digitalWrite(EGG_LEDS[i], LOW);
    return false;
  }

  uint8_t sweepIdx = elapsed / EGG_SWEEP_MS;
  unsigned long sweepElapsed = elapsed - (unsigned long)sweepIdx * EGG_SWEEP_MS;
  // 0..1000 schaal om floats te vermijden
  unsigned long t1000 = sweepElapsed * 1000UL / EGG_SWEEP_MS;

  // Frequentie sweep low → high
  uint16_t freq = EGG_FREQ_LO + (uint16_t)(((unsigned long)(EGG_FREQ_HI - EGG_FREQ_LO) * t1000) / 1000UL);
  tone(BUZZER_KORT_PIN, freq);

  // LED chase, afwisselend L→R en R→L per sweep
  uint8_t pos = (uint8_t)(t1000 * EGG_NUM_LEDS / 1000UL);
  if (pos >= EGG_NUM_LEDS) pos = EGG_NUM_LEDS - 1;
  uint8_t activeLed = (sweepIdx % 2 == 0) ? pos : (EGG_NUM_LEDS - 1 - pos);
  for (uint8_t i = 0; i < EGG_NUM_LEDS; i++) {
    digitalWrite(EGG_LEDS[i], (i == activeLed) ? HIGH : LOW);
  }
  return true;
}

// =================================================================
//  OUTPUTS
// =================================================================

void updateRailLeds() {
  digitalWrite(LED_RAIL_C, spanning[RAIL_50C] ? HIGH : LOW);
  digitalWrite(LED_RAIL_D, spanning[RAIL_50D] ? HIGH : LOW);
  digitalWrite(LED_RAIL_1, spanning[RAIL_10_1] ? HIGH : LOW);
  digitalWrite(LED_RAIL_2, spanning[RAIL_10_2] ? HIGH : LOW);
}

#define STORING1_TRAFO 0  // Storing 1 = trafo 1 (Veld 51)
#define STORING2_TRAFO 1  // Storing 2 = trafo 2 (Veld 53)

void updateStoringLeds(unsigned long now) {
  if (now - lastBlink >= BLINK_INTERVAL) {
    blinkState = !blinkState;
    lastBlink = now;
  }
  if (now - lastFastBlink >= BLINK_FAST_INTERVAL) {
    fastBlinkState = !fastBlinkState;
    lastFastBlink = now;
  }

  for (int v = 0; v < 3; v++) {
    bool ledP = false, ledS = false;

    if (btnStoring[0] && v == STORING1_TRAFO) {
      bool led;
      if (puzzel1Verloren)      led = fastBlinkState;
      else if (allSenseGoedOk)  led = true;
      else                      led = blinkState;
      ledP = ledS = led;
    }
    if (btnStoring[1] && v == STORING2_TRAFO) {
      ledP = pairVerbonden ? true : blinkState;
    }

    digitalWrite(VELD[v].ledPrim, ledP ? HIGH : LOW);
    digitalWrite(VELD[v].ledSec,  ledS ? HIGH : LOW);
  }
}

// Verzamel actieve ongoing-fouten in volgorde van prioriteit (hoogste eerst).
// Alleen T1 (uitval), T2 (aardpunt-kortsluiting) en T5 (verloren) zijn ongoing.
// T3 (koppelveld) en T4 (vlamboog) zijn event-only (kort-pattern).
uint8_t collectOngoing(FoutKind* out, uint8_t maxN) {
  uint8_t n = 0;
  bool anyAardpunt = false;
  for (int v = 0; v < 3; v++) {
    for (int p = 0; p < 3; p++) {
      if (geaard[v][p] && aardPuntSpanning(v, p)) anyAardpunt = true;
    }
  }
  if (anyAardpunt && n < maxN) out[n++] = F_AARDPUNT_KORT;        // T2 (hoogste prio)
  if ((!spanning[RAIL_10_1] || !spanning[RAIL_10_2]) && n < maxN) out[n++] = F_UITVAL; // T1
  if (puzzel1Verloren && n < maxN) out[n++] = F_VERLOREN;         // T5 (laagste prio)
  return n;
}

// stepPlayer returnt STEP_RUNNING (noot speelt nog), STEP_NOTE_DONE (huidige
// noot net afgelopen — caller bepaalt of we doorgaan of rotateren) of STEP_END
// (pattern op sentinel).
StepStatus stepPlayer(Player& p, unsigned long now) {
  if (p.pattern == nullptr) return STEP_END;
  Note n = p.pattern[p.idx];
  if (n.freq == 0 && n.ms == 0) return STEP_END;
  if (!p.noteActive) {
    if (n.freq > 0) tone(BUZZER_KORT_PIN, n.freq);
    else            noTone(BUZZER_KORT_PIN);
    p.noteStart = now;
    p.noteActive = true;
    return STEP_RUNNING;
  }
  if (now - p.noteStart >= n.ms) return STEP_NOTE_DONE;
  return STEP_RUNNING;
}

void updateBuzzer(unsigned long now) {
  // 1. Event-patroon (kort) heeft prioriteit. Stopt bij timeout of bij sentinel
  //    (afhankelijk van kortRepeat: loop tot timeout, of speel exact 1×).
  if (kortPlayer.pattern != nullptr) {
    bool timeout = (now - kortStart >= kortMaxMs);
    if (timeout) {
      kortPlayer.pattern = nullptr;
      kortPlayer.noteActive = false;
      noTone(BUZZER_KORT_PIN);
      permPlayer.noteActive = false;
    } else {
      StepStatus s = stepPlayer(kortPlayer, now);
      if (s == STEP_NOTE_DONE) {
        kortPlayer.idx++;
        kortPlayer.noteActive = false;
        return;
      }
      if (s == STEP_END) {
        if (kortRepeat) {
          // Loop: opnieuw vanaf begin
          kortPlayer.idx = 0;
          kortPlayer.noteActive = false;
          return;
        }
        // Eénmalig: stoppen
        kortPlayer.pattern = nullptr;
        noTone(BUZZER_KORT_PIN);
        permPlayer.noteActive = false;
        // val door naar perm
      } else {
        return; // STEP_RUNNING
      }
    }
  }

  // 2. Ongoing-patroon op perm: hoogste-prio fout speelt zijn patroon in lus
  //    tot de fout opgelost is. Lager-prio fouten zijn dan niet hoorbaar.
  FoutKind ongoing[6];
  uint8_t nOngoing = collectOngoing(ongoing, 6);
  if (nOngoing == 0) {
    if (permPlayer.pattern != nullptr) {
      permPlayer.pattern = nullptr;
      noTone(BUZZER_KORT_PIN);
    }
    permCurrentKind = F_NONE;
    return;
  }

  FoutKind topPrio = ongoing[0];
  if (permPlayer.pattern == nullptr || permCurrentKind != topPrio) {
    permCurrentKind = topPrio;
    permPlayer.pattern = patternFor(topPrio);
    permPlayer.idx = 0;
    permPlayer.noteActive = false;
  }

  StepStatus s = stepPlayer(permPlayer, now);
  if (s == STEP_NOTE_DONE) {
    permPlayer.idx++;
    permPlayer.noteActive = false;
  } else if (s == STEP_END) {
    permPlayer.idx = 0;
    permPlayer.noteActive = false;
    noTone(BUZZER_KORT_PIN);
  }
}

// =================================================================
//  DEBUG
// =================================================================

void printStatus(unsigned long now) {
  static unsigned long lastPrint = 0;
  if (!DEBUG || (now - lastPrint < STATUS_INTERVAL)) return;
  lastPrint = now;

  Serial.println(F("===================================================="));

  // Spanning
  Serial.print(F("SPANNING: C="));
  Serial.print(spanning[RAIL_50C] ? F("JA") : F("--"));
  Serial.print(F(" D="));
  Serial.print(spanning[RAIL_50D] ? F("JA") : F("--"));
  Serial.print(F(" R1="));
  Serial.print(spanning[RAIL_10_1] ? F("JA") : F("--"));
  Serial.print(F(" R2="));
  Serial.println(spanning[RAIL_10_2] ? F("JA") : F("--"));

  // Koppelvelden
  Serial.print(F("Koppel: prim="));
  Serial.print(koppelPrim ? F("DICHT") : F("OPEN"));
  Serial.print(F(" sec="));
  Serial.println(koppelSec ? F("DICHT") : F("OPEN"));

  // Per veld
  for (int v = 0; v < 3; v++) {
    Serial.print(VELD[v].naam);
    Serial.print(F(": RS-C="));
    Serial.print(rs[v][0] ? F("D") : F("O"));
    Serial.print(F(" RS-D="));
    Serial.print(rs[v][1] ? F("D") : F("O"));
    Serial.print(F(" VS-p="));
    Serial.print(vs[v][0] ? F("I") : F("U"));
    Serial.print(F(" VS-s="));
    Serial.print(vs[v][1] ? F("I") : F("U"));
    Serial.print(F(" RS-1="));
    Serial.print(rs[v][2] ? F("D") : F("O"));
    Serial.print(F(" RS-2="));
    Serial.println(rs[v][3] ? F("D") : F("O"));
  }

  // Aarding
  for (int v = 0; v < 3; v++) {
    for (int p = 0; p < 3; p++) {
      if (geaard[v][p]) {
        Serial.print(VELD[v].naam);
        Serial.print(F(" aard punt "));
        Serial.print(p + 1);
        Serial.print(F(": "));
        Serial.println(aardPuntSpanning(v, p) ? F("SPANNING!") : F("veilig"));
      }
    }
  }

  // Storingen
  Serial.print(F("Storing 1: btn="));
  Serial.print(btnStoring[0] ? F("AAN") : F("UIT"));
  if (btnStoring[0]) {
    if (puzzel1Verloren) Serial.print(F(" VERLOREN"));
    else Serial.print(allSenseGoedOk ? F(" (opgelost)") : F(" (NIET opgelost)"));
    if (senseFoutAangesloten) Serial.print(F(" FOUT-PIN!"));
    // Per-pin diagnose: welke A2-A7 zijn nog niet doorverbonden?
    if (!allSenseGoedOk) {
      Serial.print(F(" pending="));
      const char names[NUM_SENSE_GOED][3] = {"A2","A3","A4","A5","A6","A7"};
      for (uint8_t i = 0; i < NUM_SENSE_GOED; i++) {
        if (debouncedRead(SENSE_GOED_PINS[i]) != LOW) {
          Serial.print(names[i]); Serial.print(' ');
        }
      }
    }
  }
  Serial.print(F(" | Storing 2: btn="));
  Serial.print(btnStoring[1] ? F("AAN") : F("UIT"));
  if (btnStoring[1]) {
    Serial.print(pairVerbonden ? F(" (opgelost)") : F(" (NIET opgelost)"));
  }
  Serial.println();

  // Buzzer
  Serial.print(F("Buzzer: "));
  if (kortPlayer.pattern != nullptr) {
    Serial.println(F("KORT (event)"));
  } else if (permPlayer.pattern != nullptr) {
    Serial.print(F("PERM kind="));
    Serial.println((int)permCurrentKind);
  } else {
    Serial.println(F("stil"));
  }

  Serial.println(F("===================================================="));
}

// =================================================================
//  SETUP
// =================================================================

void setup() {
  if (DEBUG) {
    Serial.begin(9600);
    delay(200);
    Serial.println(F("Schakelbox V8 - Power Flow Model"));
    Serial.println(F("==================================="));
  }

  // Output LEDs (rechter reep)
  const uint8_t outPins[] = {
    LED_RAIL_C, LED_RAIL_D, LED_RAIL_1, LED_RAIL_2,
    BUZZER_KORT_PIN,
    LED_STOR_T1_P, LED_STOR_T1_S,
    LED_STOR_T2_P, LED_STOR_T2_S,
    LED_STOR_T3_P, LED_STOR_T3_S
  };
  for (uint8_t p : outPins) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }

  // D7 was permanente buzzer — niet meer gebruikt; expliciet LOW houden
  // zodat de bedrading niet random klikt door een floating MOSFET-gate.
  pinMode(7, OUTPUT);
  digitalWrite(7, LOW);

  // Koppelvelden (rechter reep)
  pinMode(KOPPEL_PRIM_PIN, INPUT_PULLUP);
  pinMode(KOPPEL_SEC_PIN,  INPUT_PULLUP);

  // Spanningspunten (rechter reep D16-D24)
  for (int v = 0; v < 3; v++)
    for (int p = 0; p < 3; p++)
      pinMode(SPAN_PUNT_PINS[v][p], INPUT_PULLUP);

  // Schakelaars (onderste reep D26-D43)
  for (int v = 0; v < 3; v++) {
    uint8_t pins[] = { VELD[v].rsC, VELD[v].rsD, VELD[v].vsPrim,
                       VELD[v].vsSec, VELD[v].rs1, VELD[v].rs2 };
    for (uint8_t p : pins) pinMode(p, INPUT_PULLUP);
  }

  // Aardepunten (onderste reep D45-D47, OUTPUT LOW = GND bron)
  for (uint8_t p : AARD_PUNT_PINS) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }

  // Storingsknoppen (linker reep)
  pinMode(BTN_STORING1_PIN, INPUT_PULLUP);
  pinMode(BTN_STORING2_PIN, INPUT_PULLUP);

  // Storing 1 puzzel
  pinMode(SENSE_FOUT_PIN, INPUT_PULLUP);
  for (uint8_t p : SENSE_GOED_PINS) pinMode(p, INPUT_PULLUP);

  // Storing 2 pair
  pinMode(PAIR_TX_PIN, OUTPUT);
  digitalWrite(PAIR_TX_PIN, LOW);
  pinMode(PAIR_RX_PIN, INPUT_PULLUP);

  // A0 = common-GND voor storing-1 puzzel (banaanstekker label "A0").
  // VOLT_SENSE feature is uitgezet; pin hergebruikt als OUTPUT LOW,
  // analoog aan A8 voor storing-2 (PAIR_TX_PIN).
  pinMode(VOLT_SENSE_PIN, OUTPUT);
  digitalWrite(VOLT_SENSE_PIN, LOW);

  // Debounce state initialiseren op de actuele idle-waarden
  // (anders detecteert debouncedRead alle pins als "veranderend" bij boot)
  uint8_t debouncePins[] = {
    KOPPEL_PRIM_PIN, KOPPEL_SEC_PIN,
    BTN_STORING1_PIN, BTN_STORING2_PIN,
    SENSE_FOUT_PIN, PAIR_RX_PIN,
    A2, A3, A4, A5, A6, A7
  };
  for (uint8_t p : debouncePins) debounceInit(p);
  for (int v = 0; v < 3; v++) {
    debounceInit(VELD[v].rsC);    debounceInit(VELD[v].rsD);
    debounceInit(VELD[v].vsPrim); debounceInit(VELD[v].vsSec);
    debounceInit(VELD[v].rs1);    debounceInit(VELD[v].rs2);
    for (int p = 0; p < 3; p++) debounceInit(SPAN_PUNT_PINS[v][p]);
  }

  // T8 boot tone: drie tonen oplopend (laag-mid-hoog) — synchroon, kort.
  tone(BUZZER_KORT_PIN, 200); delay(200);
  tone(BUZZER_KORT_PIN, 300); delay(200);
  tone(BUZZER_KORT_PIN, 400); delay(300);
  noTone(BUZZER_KORT_PIN);

  // Initieel lezen
  leesSchakelaars();
  leesStoringPuzzel();
  leesAarding();
  berekenSpanning();
  memcpy(prevSpanning, spanning, sizeof(spanning));
  memcpy(prevRs, rs, sizeof(rs));
  memcpy(prevVs, vs, sizeof(vs));
  memset(prevAardFout, false, sizeof(prevAardFout));
  memset(kopStart_50, 0, sizeof(kopStart_50));
  memset(kopStart_10, 0, sizeof(kopStart_10));
  memset(kopAlarm_50, false, sizeof(kopAlarm_50));
  memset(kopAlarm_10, false, sizeof(kopAlarm_10));
}

// =================================================================
//  LOOP
// =================================================================

void loop() {
  unsigned long now = millis();

  // --- 1. Inputs lezen ---
  leesSchakelaars();
  leesStoringPuzzel();
  leesAarding();

  // --- 2. Spanning detectie (analoog, optioneel) ---
  // Vereist VOLT_SENSE_SAMPLES opeenvolgende reads boven threshold; voorkomt
  // valse triggers door mains-hum pickup op een floating pin.
  if (VOLT_SENSE_ENABLED) {
    static uint8_t voltHoogCount = 0;
    int adc = analogRead(VOLT_SENSE_PIN);
    unsigned long mv = (unsigned long)adc * ADC_REF_MV / 1023;
    bool sample = (mv >= VTHRESH_MV);
    if (sample) {
      if (voltHoogCount < VOLT_SENSE_SAMPLES) voltHoogCount++;
    } else {
      voltHoogCount = 0;
    }
    bool voltHoog = (voltHoogCount >= VOLT_SENSE_SAMPLES);
    if (!prevVoltHoog && voltHoog)
      meldFout(F_NONE, "SYSTEEM", "VOLT_SENSE: spanning op ground!");
    prevVoltHoog = voltHoog;
  }

  // --- 3. Schakelfout detectie (soort 1) ---
  for (int v = 0; v < 3; v++) {
    for (int r = 0; r < 4; r++) {
      if (rs[v][r] != prevRs[v][r]) {
        checkSchakelfout(v, r);
        if (DEBUG) {
          Serial.print(VELD[v].naam);
          Serial.print(F(" "));
          Serial.print(RS_NAAM[r]);
          Serial.print(F(" -> "));
          Serial.println(rs[v][r] ? F("DICHT") : F("OPEN"));
        }
      }
    }
  }

  // --- 4. Spanning propagatie ---
  memcpy(prevSpanning, spanning, sizeof(spanning));
  berekenSpanning();

  // --- 5. Uitval (soort 2) ---
  checkUitval();

  // --- 6. Railkoppeling (soort 3) ---
  checkRailkoppeling(now);

  // --- 7. Verkeerde volgorde (soort 4) ---
  for (int v = 0; v < 3; v++) {
    if (prevVs[v][0] && !vs[v][0] && vs[v][1])
      meldFout(F_VOLGORDE, VELD[v].naam, "Verkeerde volgorde: VS-prim UIT terwijl VS-sec IN.");
    if (!prevVs[v][1] && vs[v][1] && !vs[v][0])
      meldFout(F_VOLGORDE, VELD[v].naam, "Verkeerde volgorde: VS-sec IN terwijl VS-prim UIT.");
  }

  // --- 8. Aarding fout (soort 1) ---
  checkAardingFout();

  // --- 9. Easter egg trigger detectie ---
  checkEasterEgg(now);

  // --- 10. Outputs (egg neemt over zolang 'ie speelt) ---
  if (!playEgg(now)) {
    updateRailLeds();
    updateStoringLeds(now);
    updateBuzzer(now);
  }

  // --- 11. Debug ---
  printStatus(now);

  // --- 12. Opslaan vorige toestand ---
  memcpy(prevRs, rs, sizeof(rs));
  memcpy(prevVs, vs, sizeof(vs));

  delay(5);
}
