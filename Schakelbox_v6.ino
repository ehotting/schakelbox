/* Schakelbox V6.0 - Power Flow Model
 * Laatste wijziging: 04/04/2026
 *
 * Simuleert stroomvoorziening door een onderstation met 3 trafovelden.
 * Centraal staat het power-flow model: spanning wordt gepropageerd
 * door gesloten schakelaars om te bepalen welke knooppunten spanning hebben.
 *
 * Topologie per veld:
 *   50kV Rail C --[RS-A]--+                    +--[RS-C]-- 10kV Rail 1
 *                         |                    |
 *                     trafoPrim --[VS]-- trafoSec
 *                         |                    |
 *   50kV Rail D --[RS-B]--+                    +--[RS-D]-- 10kV Rail 2
 *
 *   Koppelveld prim: verbindt 50kV Rail C en D
 *   Koppelveld sec:  verbindt 10kV Rail 1 en 2
 *
 * Bron: Rail D heeft altijd spanning. Rail C alleen via Koppelveld Prim.
 *
 * Foutsoorten:
 *   1. SCHAKELFOUT: railscheider geschakeld onder belasting (dodelijk!)
 *      Een scheider mag nooit belasting schakelen. VS'en mogen dat wel
 *      (40ms schakeltijd + isolator). Scheiders zijn traag -> vlamboog.
 *      Uitzondering: "omgaan van rail" via koppelveld (zelfde potentiaal).
 *   2. UITVAL: 10kV rail verliest spanning (klanten zonder stroom)
 *   3. ONJUISTE RAILKOPPELING: beide RS (A+B of C+D) in een veld
 *      langer dan 60s tegelijk dicht = onbeveiligd koppelveld
 *   4. VERKEERDE VOLGORDE: VS'en in verkeerde volgorde geschakeld
 *      Uitschakelen: eerst VS-sec UIT, dan VS-prim UIT
 *      Inschakelen: eerst VS-prim IN, dan VS-sec IN
 *
 * Hardware: Arduino Mega 2560 R3
 */

#include <Arduino.h>

// =================================================================
//  CONFIGURATIE
// =================================================================

#define DEBUG            true
#define STATUS_INTERVAL  3000    // debug print interval (ms)
#define BUZZER_PIN       25
#define BUZZER_DUUR      1200    // buzzer aan na fout (ms)
#define BLINK_INTERVAL   500     // storing LED knippersnelheid (ms)

// Spanning detectie (analoog, meet spanning op ground)
#define VOLT_SENSE_PIN   A3
#define ADC_REF_MV       5000
#define VTHRESH_MV       4000

// Koppelvelden (INPUT_PULLUP, LOW = actief/dicht)
#define KOPPEL_PRIM_PIN  39
#define KOPPEL_SEC_PIN   40

// Storingsknoppen (INPUT_PULLUP, LOW = ingedrukt)
#define BTN_STORING1_PIN 51     // trafo 1
#define BTN_STORING2_PIN 53     // trafo 2
// Trafo 3 heeft geen storingsknop

// Verbindingscheck bronnen (OUTPUT LOW als GND-bron)
#define SRC1_PIN         44
#define SRC2_PIN         45

// Verbindingscheck ontvangers: storing 1 (INPUT_PULLUP, LOW = OK)
#define NUM_SENSE 7
const uint8_t SENSE_PINS[NUM_SENSE] = { 5, 6, 7, 11, 46, 47, 41 };

// Verbindingscheck pairs: storing 2 (TX=OUTPUT LOW, RX=INPUT_PULLUP)
#define NUM_PAIRS 1
const uint8_t PAIR_TX_PINS[NUM_PAIRS] = { 42 };
const uint8_t PAIR_RX_PINS[NUM_PAIRS] = { 43 };

// Rail LEDs (HIGH = aan)
#define LED_RAIL1_PIN    50     // 10kV Rail 1 indicator
#define LED_RAIL2_PIN    14     // 10kV Rail 2 indicator

// Onjuiste railkoppeling: max 60 seconden
#define KOPPELING_MAX_MS 60000UL

// =================================================================
//  NETWERK TOPOLOGIE
// =================================================================

// Knooppunten in het elektrisch netwerk
enum Node : uint8_t {
  RAIL_50C,         // 50kV rail C
  RAIL_50D,         // 50kV rail D (altijd spanning)
  TRAFO_PRIM_0,     // primaire zijde trafo veld 51
  TRAFO_PRIM_1,     // primaire zijde trafo veld 53
  TRAFO_PRIM_2,     // primaire zijde trafo veld 54
  TRAFO_SEC_0,      // secundaire zijde trafo veld 51
  TRAFO_SEC_1,      // secundaire zijde trafo veld 53
  TRAFO_SEC_2,      // secundaire zijde trafo veld 54
  RAIL_10_1,        // 10kV rail 1 (klanten)
  RAIL_10_2,        // 10kV rail 2 (klanten)
  NUM_NODES
};

const char* nodeNaam(uint8_t n) {
  switch (n) {
    case RAIL_50C:     return "50kV-C";
    case RAIL_50D:     return "50kV-D";
    case TRAFO_PRIM_0: return "Prim-51";
    case TRAFO_PRIM_1: return "Prim-53";
    case TRAFO_PRIM_2: return "Prim-54";
    case TRAFO_SEC_0:  return "Sec-51";
    case TRAFO_SEC_1:  return "Sec-53";
    case TRAFO_SEC_2:  return "Sec-54";
    case RAIL_10_1:    return "10kV-1";
    case RAIL_10_2:    return "10kV-2";
    default:           return "?";
  }
}

// Pin-layout per trafoveld
struct VeldPins {
  uint8_t rsA, rsB, rsC, rsD;
  uint8_t vsPrim, vsSec;
  uint8_t ledPrim, ledSec;
  const char* naam;
};

const VeldPins VELD[3] = {
  {  2,  3, 12, 13,  8,  4, 10,  9, "Veld 51" },
  { 22, 23, 24, 26, 28, 27, 29, 30, "Veld 53" },
  { 31, 32, 33, 34, 36, 35, 37, 38, "Veld 54" }
};

// RS-indices
#define RS_A 0
#define RS_B 1
#define RS_C 2
#define RS_D 3

const char* RS_NAAM[] = { "RS-A", "RS-B", "RS-C", "RS-D" };

// =================================================================
//  TOESTAND
// =================================================================

// Schakelstanden (true = dicht/IN)
bool rs[3][4];              // [veld][RS_A..RS_D]
bool vs[3][2];              // [veld][0=prim, 1=sec]
bool koppelPrim, koppelSec;

// Vorige toestand (edge-detectie)
bool prevRs[3][4];

// Spanning per knooppunt
bool spanning[NUM_NODES];
bool prevSpanning[NUM_NODES];

// Railkoppeling timers (0 = niet actief)
unsigned long koppelingStart_AB[3];
unsigned long koppelingStart_CD[3];
bool koppelingAlarm_AB[3];  // al geflagged deze cyclus?
bool koppelingAlarm_CD[3];

// Storing checks
bool senseOk[NUM_SENSE];
bool pairOk[NUM_PAIRS];
bool allSenseOk;
bool allPairsOk;

// Buzzer
unsigned long buzzerTot = 0;

// Blink
bool blinkState = false;
unsigned long lastBlink = 0;

// =================================================================
//  SPANNING PROPAGATIE
// =================================================================

// Propageer spanning bidirectioneel door een gesloten verbinding
static inline bool propageer(bool* s, uint8_t a, uint8_t b) {
  if (s[a] && !s[b])      { s[b] = true; return true; }
  else if (s[b] && !s[a]) { s[a] = true; return true; }
  return false;
}

// Bereken welke knooppunten spanning hebben.
// skipVeld/skipRs: optioneel een RS overslaan (voor foutdetectie).
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

      if (rs[v][RS_A] && !(v == skipVeld && skipRs == RS_A))
        gewijzigd |= propageer(spanning, RAIL_50C, prim);

      if (rs[v][RS_B] && !(v == skipVeld && skipRs == RS_B))
        gewijzigd |= propageer(spanning, RAIL_50D, prim);

      if (vs[v][0] && vs[v][1])
        gewijzigd |= propageer(spanning, prim, sec);

      if (rs[v][RS_C] && !(v == skipVeld && skipRs == RS_C))
        gewijzigd |= propageer(spanning, sec, RAIL_10_1);

      if (rs[v][RS_D] && !(v == skipVeld && skipRs == RS_D))
        gewijzigd |= propageer(spanning, sec, RAIL_10_2);
    }

    if (koppelSec)
      gewijzigd |= propageer(spanning, RAIL_10_1, RAIL_10_2);
  }
}

// =================================================================
//  FOUTDETECTIE
// =================================================================

void meldFout(uint8_t soort, const char* veldNaam, const char* bericht) {
  buzzerTot = millis() + BUZZER_DUUR;
  if (!DEBUG) return;
  Serial.print(F("!!! FOUT SOORT "));
  Serial.print(soort);
  Serial.print(F(" | "));
  Serial.print(millis());
  Serial.print(F("ms | "));
  Serial.print(veldNaam);
  Serial.print(F(" | "));
  Serial.println(bericht);
}

// Nodes aan weerszijden van een RS
void rsNodes(int veld, int rsIdx, uint8_t &nodeA, uint8_t &nodeB) {
  switch (rsIdx) {
    case RS_A: nodeA = RAIL_50C;           nodeB = TRAFO_PRIM_0 + veld; break;
    case RS_B: nodeA = RAIL_50D;           nodeB = TRAFO_PRIM_0 + veld; break;
    case RS_C: nodeA = TRAFO_SEC_0 + veld; nodeB = RAIL_10_1;           break;
    case RS_D: nodeA = TRAFO_SEC_0 + veld; nodeB = RAIL_10_2;           break;
  }
}

// Soort 1: SCHAKELFOUT
// Wanneer een RS wisselt: bereken spanning ZONDER die RS.
// Als er potentiaalverschil is (een kant spanning, andere niet)
// EN de VS in het circuit is dicht → stroom loopt → SCHAKELFOUT.
// Als beide kanten spanning hebben → zelfde potentiaal → veilig (omgaan van rail).
void checkSchakelfout(int veld, int rsIdx) {
  uint8_t nodeA, nodeB;
  rsNodes(veld, rsIdx, nodeA, nodeB);

  // Bewaar huidige spanning, bereken zonder deze RS
  bool backup[NUM_NODES];
  memcpy(backup, spanning, sizeof(spanning));
  berekenSpanning(veld, rsIdx);

  bool spanA = spanning[nodeA];
  bool spanB = spanning[nodeB];

  memcpy(spanning, backup, sizeof(spanning));

  // Beide kanten gelijk → veilig
  if (spanA == spanB) return;

  // Potentiaalverschil aanwezig. Is de VS dicht?
  bool vsDicht = (rsIdx <= RS_B) ? vs[veld][0] : vs[veld][1];

  if (vsDicht) {
    char msg[90];
    snprintf(msg, sizeof(msg),
      "SCHAKELFOUT: %s geschakeld onder belasting! (%s=%s, %s=%s)",
      RS_NAAM[rsIdx],
      nodeNaam(nodeA), spanA ? "spanning" : "geen",
      nodeNaam(nodeB), spanB ? "spanning" : "geen");
    meldFout(1, VELD[veld].naam, msg);
  }
}

// Soort 2: UITVAL
// 10kV rail verliest spanning → klanten zonder stroom
void checkUitval() {
  if (prevSpanning[RAIL_10_1] && !spanning[RAIL_10_1])
    meldFout(2, "SYSTEEM", "UITVAL: 10kV Rail 1 heeft geen spanning meer!");
  if (prevSpanning[RAIL_10_2] && !spanning[RAIL_10_2])
    meldFout(2, "SYSTEEM", "UITVAL: 10kV Rail 2 heeft geen spanning meer!");
}

// Soort 3: ONJUISTE RAILKOPPELING
// Beide RS in een veld (A+B of C+D) langer dan 60s tegelijk dicht
// = onbeveiligd koppelveld tussen de twee rails
void checkRailkoppeling(unsigned long now) {
  for (int v = 0; v < 3; v++) {
    // Primair: RS-A + RS-B
    if (rs[v][RS_A] && rs[v][RS_B]) {
      if (koppelingStart_AB[v] == 0) {
        koppelingStart_AB[v] = now;
        koppelingAlarm_AB[v] = false;
      }
      if (!koppelingAlarm_AB[v] && (now - koppelingStart_AB[v] >= KOPPELING_MAX_MS)) {
        meldFout(3, VELD[v].naam, "ONJUISTE RAILKOPPELING: RS-A + RS-B >60s beide dicht!");
        koppelingAlarm_AB[v] = true;
      }
      // Blijf buzzer activeren zolang de situatie voortduurt
      if (koppelingAlarm_AB[v])
        buzzerTot = now + BUZZER_DUUR;
    } else {
      koppelingStart_AB[v] = 0;
      koppelingAlarm_AB[v] = false;
    }

    // Secundair: RS-C + RS-D
    if (rs[v][RS_C] && rs[v][RS_D]) {
      if (koppelingStart_CD[v] == 0) {
        koppelingStart_CD[v] = now;
        koppelingAlarm_CD[v] = false;
      }
      if (!koppelingAlarm_CD[v] && (now - koppelingStart_CD[v] >= KOPPELING_MAX_MS)) {
        meldFout(3, VELD[v].naam, "ONJUISTE RAILKOPPELING: RS-C + RS-D >60s beide dicht!");
        koppelingAlarm_CD[v] = true;
      }
      if (koppelingAlarm_CD[v])
        buzzerTot = now + BUZZER_DUUR;
    } else {
      koppelingStart_CD[v] = 0;
      koppelingAlarm_CD[v] = false;
    }
  }
}

// =================================================================
//  INPUTS LEZEN
// =================================================================

void leesSchakelaars() {
  for (int v = 0; v < 3; v++) {
    uint8_t rsPins[] = { VELD[v].rsA, VELD[v].rsB, VELD[v].rsC, VELD[v].rsD };
    for (int r = 0; r < 4; r++)
      rs[v][r] = (digitalRead(rsPins[r]) == LOW);  // NC: LOW = dicht

    vs[v][0] = (digitalRead(VELD[v].vsPrim) == LOW);
    vs[v][1] = (digitalRead(VELD[v].vsSec)  == LOW);
  }
  koppelPrim = (digitalRead(KOPPEL_PRIM_PIN) == LOW);
  koppelSec  = (digitalRead(KOPPEL_SEC_PIN)  == LOW);
}

void leesVerbindingscheck() {
  allSenseOk = true;
  for (uint8_t i = 0; i < NUM_SENSE; i++) {
    senseOk[i] = (digitalRead(SENSE_PINS[i]) == LOW);
    if (!senseOk[i]) allSenseOk = false;
  }

  allPairsOk = true;
  for (uint8_t i = 0; i < NUM_PAIRS; i++) {
    pairOk[i] = (digitalRead(PAIR_RX_PINS[i]) == LOW);
    if (!pairOk[i]) allPairsOk = false;
  }
}

// =================================================================
//  OUTPUTS
// =================================================================

void updateRailLeds() {
  digitalWrite(LED_RAIL1_PIN, spanning[RAIL_10_1] ? HIGH : LOW);
  digitalWrite(LED_RAIL2_PIN, spanning[RAIL_10_2] ? HIGH : LOW);
}

void updateStoringLeds(unsigned long now) {
  // Blink timer
  if (now - lastBlink >= BLINK_INTERVAL) {
    blinkState = !blinkState;
    lastBlink = now;
  }

  // Storingsknoppen: trafo 1 en 2 (trafo 3 heeft geen knop)
  bool btnStoring[3];
  btnStoring[0] = (digitalRead(BTN_STORING1_PIN) == LOW);
  btnStoring[1] = (digitalRead(BTN_STORING2_PIN) == LOW);
  btnStoring[2] = false;

  for (int v = 0; v < 3; v++) {
    bool led = false;

    if (btnStoring[v]) {
      bool opgelost = false;
      if (v == 0) opgelost = allSenseOk;
      if (v == 1) opgelost = allPairsOk;

      led = opgelost ? true : blinkState;  // aan of knipperen
    }

    digitalWrite(VELD[v].ledPrim, led ? HIGH : LOW);
    digitalWrite(VELD[v].ledSec,  led ? HIGH : LOW);
  }
}

void updateBuzzer(unsigned long now) {
  bool aan = ((long)(buzzerTot - now) > 0);
  digitalWrite(BUZZER_PIN, aan ? HIGH : LOW);
}

// =================================================================
//  DEBUG
// =================================================================

void printStatus(unsigned long now) {
  static unsigned long lastPrint = 0;
  if (!DEBUG || (now - lastPrint < STATUS_INTERVAL)) return;
  lastPrint = now;

  Serial.println(F("===================================================="));

  // Spanning per knooppunt
  Serial.print(F("SPANNING: "));
  for (int n = 0; n < NUM_NODES; n++) {
    Serial.print(nodeNaam(n));
    Serial.print(F("="));
    Serial.print(spanning[n] ? F("JA") : F("--"));
    if (n < NUM_NODES - 1) Serial.print(F(" | "));
  }
  Serial.println();

  // Koppelvelden
  Serial.print(F("Koppelveld prim="));
  Serial.print(koppelPrim ? F("DICHT") : F("OPEN"));
  Serial.print(F(" | sec="));
  Serial.println(koppelSec ? F("DICHT") : F("OPEN"));

  // Per veld
  for (int v = 0; v < 3; v++) {
    Serial.print(VELD[v].naam);
    Serial.print(F(": "));
    for (int r = 0; r < 4; r++) {
      Serial.print(RS_NAAM[r]);
      Serial.print(F("="));
      Serial.print(rs[v][r] ? F("DICHT") : F("OPEN"));
      Serial.print(F(" "));
    }
    Serial.print(F("VS-p="));
    Serial.print(vs[v][0] ? F("IN") : F("UIT"));
    Serial.print(F(" VS-s="));
    Serial.println(vs[v][1] ? F("IN") : F("UIT"));
  }

  // Railkoppeling timers
  for (int v = 0; v < 3; v++) {
    if (koppelingStart_AB[v] > 0) {
      Serial.print(VELD[v].naam);
      Serial.print(F(" RS-A+B dicht: "));
      Serial.print((now - koppelingStart_AB[v]) / 1000);
      Serial.println(F("s"));
    }
    if (koppelingStart_CD[v] > 0) {
      Serial.print(VELD[v].naam);
      Serial.print(F(" RS-C+D dicht: "));
      Serial.print((now - koppelingStart_CD[v]) / 1000);
      Serial.println(F("s"));
    }
  }

  // Spanning detectie
  int adc = analogRead(VOLT_SENSE_PIN);
  unsigned long mv = (unsigned long)adc * ADC_REF_MV / 1023;
  Serial.print(F("VOLT_SENSE: "));
  Serial.print(mv);
  Serial.print(F("mV (drempel="));
  Serial.print(VTHRESH_MV);
  Serial.println(F("mV)"));

  // Storing status
  Serial.print(F("Storing 1 (sense): "));
  Serial.print(allSenseOk ? F("OK") : F("FOUT"));
  Serial.print(F(" | Storing 2 (pairs): "));
  Serial.println(allPairsOk ? F("OK") : F("FOUT"));

  // Buzzer
  bool buzzerAan = ((long)(buzzerTot - now) > 0);
  Serial.print(F("BUZZER: "));
  Serial.println(buzzerAan ? F("AAN") : F("UIT"));

  Serial.println(F("===================================================="));
}

// =================================================================
//  SETUP
// =================================================================

void setup() {
  if (DEBUG) {
    Serial.begin(9600);
    delay(200);
    Serial.println(F("Schakelbox V6.0 - Power Flow Model"));
    Serial.println(F("==================================="));
  }

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Koppelvelden
  pinMode(KOPPEL_PRIM_PIN, INPUT_PULLUP);
  pinMode(KOPPEL_SEC_PIN,  INPUT_PULLUP);

  // Storingsknoppen
  pinMode(BTN_STORING1_PIN, INPUT_PULLUP);
  pinMode(BTN_STORING2_PIN, INPUT_PULLUP);

  // Verbindingscheck bronnen
  pinMode(SRC1_PIN, OUTPUT); digitalWrite(SRC1_PIN, LOW);
  pinMode(SRC2_PIN, OUTPUT); digitalWrite(SRC2_PIN, LOW);

  // Verbindingscheck ontvangers
  for (uint8_t i = 0; i < NUM_SENSE; i++)
    pinMode(SENSE_PINS[i], INPUT_PULLUP);

  // Verbindingscheck pairs
  for (uint8_t i = 0; i < NUM_PAIRS; i++) {
    pinMode(PAIR_TX_PINS[i], OUTPUT);
    digitalWrite(PAIR_TX_PINS[i], LOW);
    pinMode(PAIR_RX_PINS[i], INPUT_PULLUP);
  }

  // Rail LEDs
  pinMode(LED_RAIL1_PIN, OUTPUT); digitalWrite(LED_RAIL1_PIN, HIGH);
  pinMode(LED_RAIL2_PIN, OUTPUT); digitalWrite(LED_RAIL2_PIN, HIGH);

  // Spanning detectie
  pinMode(VOLT_SENSE_PIN, INPUT);

  // Veld pins
  for (int v = 0; v < 3; v++) {
    uint8_t inputs[] = { VELD[v].rsA, VELD[v].rsB, VELD[v].rsC, VELD[v].rsD,
                         VELD[v].vsPrim, VELD[v].vsSec };
    for (int p = 0; p < 6; p++)
      pinMode(inputs[p], INPUT_PULLUP);

    pinMode(VELD[v].ledPrim, OUTPUT); digitalWrite(VELD[v].ledPrim, LOW);
    pinMode(VELD[v].ledSec,  OUTPUT); digitalWrite(VELD[v].ledSec,  LOW);
  }

  // Initieel: lees schakelaars en bereken spanning
  leesSchakelaars();
  berekenSpanning();
  memcpy(prevSpanning, spanning, sizeof(spanning));
  memcpy(prevRs, rs, sizeof(rs));

  // Timers
  memset(koppelingStart_AB, 0, sizeof(koppelingStart_AB));
  memset(koppelingStart_CD, 0, sizeof(koppelingStart_CD));
  memset(koppelingAlarm_AB, 0, sizeof(koppelingAlarm_AB));
  memset(koppelingAlarm_CD, 0, sizeof(koppelingAlarm_CD));
}

// =================================================================
//  LOOP
// =================================================================

void loop() {
  unsigned long now = millis();

  // --- 1. Inputs lezen ---
  leesSchakelaars();
  leesVerbindingscheck();

  // --- 2. Spanning detectie (analoog) ---
  int adc = analogRead(VOLT_SENSE_PIN);
  unsigned long mv = (unsigned long)adc * ADC_REF_MV / 1023;
  bool voltHoog = (mv >= VTHRESH_MV);

  static bool prevVoltHoog = false;
  if (!prevVoltHoog && voltHoog)
    meldFout(1, "SYSTEEM", "VOLT_SENSE: spanning gedetecteerd op ground!");
  if (voltHoog)
    buzzerTot = now + BUZZER_DUUR;
  prevVoltHoog = voltHoog;

  // --- 3. Schakelfout detectie (soort 1) ---
  // Check elke RS die van toestand is veranderd
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

  // --- 5. Uitval detectie (soort 2) ---
  checkUitval();

  // --- 6. Railkoppeling detectie (soort 3) ---
  checkRailkoppeling(now);

  // --- 7. Verkeerde schakelvolgorde detectie (soort 4) ---
  // Uitschakelen: eerst VS-sec UIT, dan VS-prim UIT
  // Inschakelen: eerst VS-prim IN, dan VS-sec IN
  static bool prevVs[3][2];
  for (int v = 0; v < 3; v++) {
    // VS-prim gaat UIT terwijl VS-sec nog IN
    if (prevVs[v][0] && !vs[v][0] && vs[v][1]) {
      meldFout(4, VELD[v].naam, "Verkeerde volgorde: VS-prim UIT terwijl VS-sec nog IN. Eerst VS-sec UIT.");
    }
    // VS-sec gaat IN terwijl VS-prim nog UIT
    if (!prevVs[v][1] && vs[v][1] && !vs[v][0]) {
      meldFout(4, VELD[v].naam, "Verkeerde volgorde: VS-sec IN terwijl VS-prim nog UIT. Eerst VS-prim IN.");
    }
    prevVs[v][0] = vs[v][0];
    prevVs[v][1] = vs[v][1];
  }

  // --- 8. Outputs ---
  updateRailLeds();
  updateStoringLeds(now);
  updateBuzzer(now);

  // --- 9. Debug ---
  printStatus(now);

  // --- 10. Opslaan voor volgende cyclus ---
  memcpy(prevRs, rs, sizeof(rs));

  delay(5);
}
