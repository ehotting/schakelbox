/* Schakelbox V7.0 - Power Flow Model
 * Laatste wijziging: 10/04/2026
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
#define BUZZER_KORT_DUUR 1200    // korte buzzer standaard
// Per foutsoort: 1=schakelfout, 2=uitval, 3=railkoppeling, 4=volgorde(kort), 5=foute pin
const unsigned long BUZZER_DUUR_PER_TYPE[] = { 0, 1200, 1200, 1200, 600, 1200 };
#define BLINK_INTERVAL   500

// =================================================================
//  PIN DEFINITIES
// =================================================================

// --- OUTPUTS (rechter reep) ---
#define LED_RAIL_C       2       // Rail C spanning LED
#define LED_RAIL_D       3       // Rail D spanning LED
#define LED_RAIL_1       4       // Rail 1 + Duinpad LED (parallel)
#define LED_RAIL_2       5       // Rail 2 + Fre de Rik LED (parallel)
#define BUZZER_KORT_PIN  6       // Korte buzzer (3s bij elke fout)
#define BUZZER_PERM_PIN  7       // Permanente buzzer (zolang fout voortduurt)
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

// Aarding
bool geaard[3][3];               // [veld][boven, midden, onder]
bool prevAardFout[3][3];         // edge detection

// Buzzers
unsigned long buzzerKortTot = 0;
bool buzzerPermAan = false;

// Blink
bool blinkState = false;
unsigned long lastBlink = 0;

// Edge detection (was static in functies, nu global voor consistentie)
bool prevVoltHoog = false;
bool prevSenseGoed[NUM_SENSE_GOED] = {};
bool kopFout_50[3], kopFout_10[3];

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

void meldFout(uint8_t soort, const char* veldNaam, const char* bericht) {
  unsigned long now = millis();
  buzzerKortTot = now + ((soort >= 1 && soort <= 5) ? BUZZER_DUUR_PER_TYPE[soort] : BUZZER_KORT_DUUR);

  if (!DEBUG) return;
  Serial.print(F("!!! FOUT SOORT "));
  Serial.print(soort);
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

  if (spanA == spanB) return;

  bool vsDicht = (rsIdx <= RS_D_IDX) ? vs[veld][0] : vs[veld][1];
  if (vsDicht) {
    meldFout(1, VELD[veld].naam, "SCHAKELFOUT: RS geschakeld onder belasting!");
  }
}

// Soort 2: UITVAL
void checkUitval() {
  if (prevSpanning[RAIL_10_1] && !spanning[RAIL_10_1])
    meldFout(2, "SYSTEEM", "UITVAL: 10kV Rail 1 geen spanning!");
  if (prevSpanning[RAIL_10_2] && !spanning[RAIL_10_2])
    meldFout(2, "SYSTEEM", "UITVAL: 10kV Rail 2 geen spanning!");
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
        meldFout(1, VELD[v].naam, "RS-C+D dicht ZONDER koppelveld prim! Vlamboog!");
        kopFout_50[v] = true;
      }
    } else { kopFout_50[v] = false; }

    // 10kV: zonder koppelveld = direct fout 1
    if (beide10 && !koppelSec) {
      if (!kopFout_10[v]) {
        meldFout(1, VELD[v].naam, "RS-1+2 dicht ZONDER koppelveld sec! Vlamboog!");
        kopFout_10[v] = true;
      }
    } else { kopFout_10[v] = false; }

    // Met koppelveld: 60s timer
    if (beide50 && koppelPrim) {
      if (kopStart_50[v] == 0) { kopStart_50[v] = now; kopAlarm_50[v] = false; }
      if (!kopAlarm_50[v] && ((long)(now - kopStart_50[v]) >= (long)KOPPELING_MAX_MS)) {
        meldFout(3, VELD[v].naam, "RAILKOPPELING: RS-C + RS-D >60s!");
        kopAlarm_50[v] = true;
      }
    } else { kopStart_50[v] = 0; kopAlarm_50[v] = false; }

    if (beide10 && koppelSec) {
      if (kopStart_10[v] == 0) { kopStart_10[v] = now; kopAlarm_10[v] = false; }
      if (!kopAlarm_10[v] && ((long)(now - kopStart_10[v]) >= (long)KOPPELING_MAX_MS)) {
        meldFout(3, VELD[v].naam, "RAILKOPPELING: RS-1 + RS-2 >60s!");
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
        meldFout(1, VELD[v].naam, "Spanning op geaard punt! Dodelijke vlamboog!");
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
    rs[v][RS_C_IDX] = (digitalRead(VELD[v].rsC)    == LOW);
    rs[v][RS_D_IDX] = (digitalRead(VELD[v].rsD)    == LOW);
    rs[v][RS_1_IDX] = (digitalRead(VELD[v].rs1)    == LOW);
    rs[v][RS_2_IDX] = (digitalRead(VELD[v].rs2)    == LOW);
    vs[v][0]        = (digitalRead(VELD[v].vsPrim)  == LOW);
    vs[v][1]        = (digitalRead(VELD[v].vsSec)   == LOW);
  }
  koppelPrim = (digitalRead(KOPPEL_PRIM_PIN) == LOW);
  koppelSec  = (digitalRead(KOPPEL_SEC_PIN)  == LOW);

  // Storingsknoppen
  btnStoring[0] = (digitalRead(BTN_STORING1_PIN) == LOW);
  btnStoring[1] = (digitalRead(BTN_STORING2_PIN) == LOW);

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
    bool ok = (digitalRead(SENSE_GOED_PINS[i]) == LOW);
    // Edge: pin wordt aangesloten zonder aarding → fout 1
    if (ok && !prevSenseGoed[i] && btnStoring[0] && !isTrafoGeaard(0)) {
      meldFout(1, "STORING 1", "Werken zonder aarding! Dodelijk gevaar!");
    }
    prevSenseGoed[i] = ok;
    if (!ok) allSenseGoedOk = false;
  }

  bool wasFoutAangesloten = senseFoutAangesloten;
  senseFoutAangesloten = (digitalRead(SENSE_FOUT_PIN) == LOW);
  if (senseFoutAangesloten && !wasFoutAangesloten) {
    if (btnStoring[0] && !isTrafoGeaard(0))
      meldFout(1, "STORING 1", "Werken zonder aarding! Dodelijk gevaar!");
    meldFout(5, "STORING 1", "Foute pin A1 aangesloten!");
  }

  // Storing 2: A8→A9 pair
  bool wasPairVerbonden = pairVerbonden;
  pairVerbonden = (digitalRead(PAIR_RX_PIN) == LOW);
  if (pairVerbonden && !wasPairVerbonden && btnStoring[1] && !isTrafoGeaard(1)) {
    meldFout(1, "STORING 2", "Werken zonder aarding! Dodelijk gevaar!");
  }
}

void leesAarding() {
  for (int v = 0; v < 3; v++) {
    for (int p = 0; p < 3; p++) {
      geaard[v][p] = (digitalRead(SPAN_PUNT_PINS[v][p]) == LOW);
    }
  }
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

  for (int v = 0; v < 3; v++) {
    bool ledP = false, ledS = false;

    if (btnStoring[0] && v == STORING1_TRAFO) {
      bool led = allSenseGoedOk ? true : blinkState;
      ledP = ledS = led;
    }
    if (btnStoring[1] && v == STORING2_TRAFO) {
      ledP = pairVerbonden ? true : blinkState;
    }

    digitalWrite(VELD[v].ledPrim, ledP ? HIGH : LOW);
    digitalWrite(VELD[v].ledSec,  ledS ? HIGH : LOW);
  }
}

// Check of er een permanente foutconditie actief is
bool heeftPermanenteFout() {
  if (!spanning[RAIL_10_1] || !spanning[RAIL_10_2]) return true;
  for (int v = 0; v < 3; v++) {
    if (kopAlarm_50[v] || kopAlarm_10[v]) return true;
    if (kopFout_50[v] || kopFout_10[v]) return true;
    for (int p = 0; p < 3; p++)
      if (geaard[v][p] && aardPuntSpanning(v, p)) return true;
  }
  return false;
}

void updateBuzzers(unsigned long now) {
  digitalWrite(BUZZER_KORT_PIN, ((long)(buzzerKortTot - now) > 0) ? HIGH : LOW);
  buzzerPermAan = heeftPermanenteFout();
  digitalWrite(BUZZER_PERM_PIN, buzzerPermAan ? HIGH : LOW);
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
    Serial.print(allSenseGoedOk ? F(" (opgelost)") : F(" (NIET opgelost)"));
    if (senseFoutAangesloten) Serial.print(F(" FOUT-PIN!"));
  }
  Serial.print(F(" | Storing 2: btn="));
  Serial.print(btnStoring[1] ? F("AAN") : F("UIT"));
  if (btnStoring[1]) {
    Serial.print(pairVerbonden ? F(" (opgelost)") : F(" (NIET opgelost)"));
  }
  Serial.println();

  // Buzzers
  bool kortAan = ((long)(buzzerKortTot - now) > 0);
  Serial.print(F("Buzzer kort="));
  Serial.print(kortAan ? F("AAN") : F("UIT"));
  Serial.print(F(" perm="));
  Serial.println(buzzerPermAan ? F("AAN") : F("UIT"));

  Serial.println(F("===================================================="));
}

// =================================================================
//  SETUP
// =================================================================

void setup() {
  if (DEBUG) {
    Serial.begin(9600);
    delay(200);
    Serial.println(F("Schakelbox V7.0 - Power Flow Model"));
    Serial.println(F("==================================="));
  }

  // Output LEDs (rechter reep)
  const uint8_t outPins[] = {
    LED_RAIL_C, LED_RAIL_D, LED_RAIL_1, LED_RAIL_2,
    BUZZER_KORT_PIN, BUZZER_PERM_PIN,
    LED_STOR_T1_P, LED_STOR_T1_S,
    LED_STOR_T2_P, LED_STOR_T2_S,
    LED_STOR_T3_P, LED_STOR_T3_S
  };
  for (uint8_t p : outPins) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }

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

  // VSENSE
  pinMode(VOLT_SENSE_PIN, INPUT);

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
  int adc = analogRead(VOLT_SENSE_PIN);
  unsigned long mv = (unsigned long)adc * ADC_REF_MV / 1023;
  bool voltHoog = (mv >= VTHRESH_MV);
  if (!prevVoltHoog && voltHoog)
    meldFout(1, "SYSTEEM", "VOLT_SENSE: spanning op ground!");
  prevVoltHoog = voltHoog;

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
      meldFout(4, VELD[v].naam, "Verkeerde volgorde: VS-prim UIT terwijl VS-sec IN.");
    if (!prevVs[v][1] && vs[v][1] && !vs[v][0])
      meldFout(4, VELD[v].naam, "Verkeerde volgorde: VS-sec IN terwijl VS-prim UIT.");
  }

  // --- 8. Aarding fout (soort 1) ---
  checkAardingFout();

  // --- 9. Outputs ---
  updateRailLeds();
  updateStoringLeds(now);
  updateBuzzers(now);

  // --- 10. Debug ---
  printStatus(now);

  // --- 11. Opslaan vorige toestand ---
  memcpy(prevRs, rs, sizeof(rs));
  memcpy(prevVs, vs, sizeof(vs));

  delay(5);
}
