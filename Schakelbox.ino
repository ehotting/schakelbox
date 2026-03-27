/* V5.0. last edit: 25/03/2026
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////// Frederik Akallouch - QIRION /////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/////////////////// Max Weel - The Experience Office //////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

  Arduino Mega 2560
  3 velden (51, 53 en 54):
    - RS A-D (2-pos, C-NC bekabeld)
    - VS prim & VS sec (3-pos, C-NC bekabeld)
    - LED's per veld: storing-indicatie (knipperen/aan/uit)
  Een centrale buzzer (actief HIGH)
  -----------------------------------------
  Uitbreiding:
  - Koppelveld_VS_prim -> schakelt foutcontrole uit voor ALLE RS A & B
  - Koppelveld_VS_sec  -> schakelt foutcontrole uit voor ALLE RS C & D

  Bugfixes V5.0:
  1. Alarm bij koppelveld uit terwijl beide RS in hetzelfde veld aan staan
     (edge-detect + continue buzzer zolang situatie voortduurt)
  2. Rail-LED logica: power flow model dat de hele keten volgt
     (50kV rail -> RS A/B -> VS prim -> VS sec -> RS C/D -> 10kV rail)
     inclusief beide koppelvelden
  3. Storing-LED: knippert bij onopgeloste storing, continu aan bij opgelost,
     uit bij inactieve knop. Gebruikt sense/pair checks ipv VS-stand.
  4. btnVeld[2] geinitialiseerd (was undefined, geen knop voor trafo 3)

  NB: RS-vrijgave (allow_rs* functies) was al correct per veld.
  Als de global buzzer onterecht afgaat bij testen, check Serial Monitor
  of het een GLOBAL alarm is (alle VS uit door ontbrekende hardware).
*/

#include <Arduino.h>

//  INSTELLINGEN
#define DEBUG true                //true = krijg elk interval een debug update
#define STATUS_INTERVAL_MS 3000   //interval van de debug update in ms

#define BUZZER_PIN 25             //uitgaande pin van de piezo buzzer
#define BUZZER_LATCH_MS 1200      //duur van buzzer na schakelfout in ms

#define BLINK_INTERVAL 500        //interval van storingsleds in ms

#define SWAP_VS_I_O 0             //?
#define ENFORCE_KOPPEL_OFF_RULE 0 //?

//  DEFINIER PINS

//  NIEUW: SPANNING-DETECT (ANALOOG) [MW: meet spanning op de ground]
// Let op: Arduino Mega ADC meet 0..5V (standaard). Gebruik spanningsdeler bij hogere spanningen!
#define VOLT_SENSE_PIN A3  // nieuwe poort waar spanning op binnenkomt
#define ADC_REF_MV 5000    // 5000mV bij 5V referentie (standaard)
#define VTHRESH_MV 4000    // drempel: 2000mV

// KOPPELVELD PINS
#define KOPPEL_PRIM_PIN 39  // INPUT_PULLUP, LOW = actief
#define KOPPEL_SEC_PIN 40   // INPUT_PULLUP, LOW = actief

//  2 DRUKKNOPPEN (NO vergrendelend) [MW: om de storing actief/inactief te maken]
// NO -> pin, COM -> GND  => INPUT_PULLUP, LOW = ingedrukt/actief
#define BTN_VELD51_PIN 51
#define BTN_VELD53_PIN 53

//  2 -> 7 VERBINDINGSCHECK (vervangt oude lussen)
// 2 bronnen (OUTPUT LOW) die je op breadboard gebruikt als "GND-bron" voor de verbindingen
#define SRC1_PIN 44
#define SRC2_PIN 45

// 7 ontvangers (INPUT_PULLUP): LOW = aangesloten, HIGH = niet aangesloten
#define NUM_SENSE 7
const uint8_t SENSE_PINS[NUM_SENSE] = { 5, 6, 7, 11, 46, 47, 41 };

// SENSE-PAIR (42 pinouts) storing 2
// Per pair: TX = OUTPUT LOW, RX = INPUT_PULLUP. RX LOW = OK, RX HIGH = FOUT
#define NUM_PAIRS 1
const uint8_t PAIR_TX_PINS[NUM_PAIRS] = { 42 };
const uint8_t PAIR_RX_PINS[NUM_PAIRS] = { 43 };

//  NIEUW: 4 LEDS = 2 SETS IN SERIE (2 pins)
#define LED_SET_C_PIN 50  // set 1 (2 leds in serie) = Rail 1 indicator
#define LED_SET_D_PIN 14  // set 2 (2 leds in serie) = Rail 2 indicator
// Pin HIGH = LED-set AAN

//  TYPES
enum RS_State : uint8_t { RS_OFF = 0,
                          RS_ON = 1 };

struct Veld {
  uint8_t rsA, rsB, rsC, rsD;
  uint8_t vsPrim, vsSec;
  uint8_t ledPrim, ledSec;
  const char* naam;
};

//  HELPERS
static inline RS_State readRS(uint8_t pin) {
  return (digitalRead(pin) == LOW) ? RS_ON : RS_OFF;  // NC naar GND => LOW = IN
}

static inline bool VS_is_I(uint8_t pinNC) {
  bool ncActive = (digitalRead(pinNC) == LOW);
#if SWAP_VS_I_O
  return !ncActive;
#else
  return ncActive;
#endif
}

static inline const char* rsTxt(RS_State s) {
  return (s == RS_ON) ? "IN" : "UIT";
}
static inline const char* vsTxt(bool I) {
  return I ? "IN(I)" : "UIT(O)";
}

// Buzzer latch + foutmelding
static unsigned long buzzerUntil = 0;

static void triggerFault(const char* veldNaam, const char* device, const char* reason) {
  unsigned long now = millis();
  buzzerUntil = now + BUZZER_LATCH_MS;

  if (DEBUG) {
    Serial.print(F("!!! FOUT  "));
    Serial.print(now);
    Serial.print(F("ms | "));
    Serial.print(veldNaam);
    Serial.print(F(" | "));
    Serial.print(device);
    Serial.print(F(" | "));
    Serial.println(reason);
  }
}

// PINOUT PER VELD
// VELD 51
#define RS1_A 2
#define RS1_B 3
#define RS1_C 12
#define RS1_D 13
#define VS1_PRIM 8
#define VS1_SEC 4
#define LED1_PRIM 10
#define LED1_SEC 9

// VELD 53
#define RS2_A 22
#define RS2_B 23
#define RS2_C 24
#define RS2_D 26
#define VS2_PRIM 28
#define VS2_SEC 27
#define LED2_PRIM 29
#define LED2_SEC 30

// VELD 54
#define RS3_A 31
#define RS3_B 32
#define RS3_C 33
#define RS3_D 34
#define VS3_PRIM 36
#define VS3_SEC 35
#define LED3_PRIM 37
#define LED3_SEC 38

//  Array van velden
Veld velden[3] = {
  { RS1_A, RS1_B, RS1_C, RS1_D, VS1_PRIM, VS1_SEC, LED1_PRIM, LED1_SEC, "Veld 51" },
  { RS2_A, RS2_B, RS2_C, RS2_D, VS2_PRIM, VS2_SEC, LED2_PRIM, LED2_SEC, "Veld 53" },
  { RS3_A, RS3_B, RS3_C, RS3_D, VS3_PRIM, VS3_SEC, LED3_PRIM, LED3_SEC, "Veld 54" }
};

//  VRIJGAVE-REGELS (schema's)
// PRIM (A/B) - 50kV zijde
static inline bool allow_rsA_on(bool vsPrimI, RS_State rsB, bool koppelPrim) {
  bool vsUit = !vsPrimI;
  return (vsUit && rsB == RS_OFF) || (koppelPrim && rsB == RS_ON);
}
static inline bool allow_rsB_on(bool vsPrimI, RS_State rsA, bool koppelPrim) {
  bool vsUit = !vsPrimI;
  return (vsUit && rsA == RS_OFF) || (koppelPrim && rsA == RS_ON);
}
static inline bool allow_rsA_off(bool vsPrimI, RS_State rsB, bool koppelPrim) {
  bool vsUit = !vsPrimI;
  return (vsUit) || (koppelPrim && rsB == RS_ON);
}
static inline bool allow_rsB_off(bool vsPrimI, RS_State rsA, bool koppelPrim) {
  bool vsUit = !vsPrimI;
  return (vsUit) || (koppelPrim && rsA == RS_ON);
}

// SEC (C/D) - 10kV zijde
static inline bool allow_rsC_on(bool vsSecI, RS_State rsD, bool koppelSec) {
  bool vsUit = !vsSecI;
  return (vsUit && rsD == RS_OFF) || (koppelSec && rsD == RS_ON);
}
static inline bool allow_rsD_on(bool vsSecI, RS_State rsC, bool koppelSec) {
  bool vsUit = !vsSecI;
  return (vsUit && rsC == RS_OFF) || (koppelSec && rsC == RS_ON);
}
static inline bool allow_rsC_off(bool vsSecI, RS_State rsD, bool koppelSec) {
  bool vsUit = !vsSecI;
  return (vsUit) || (koppelSec && rsD == RS_ON);
}
static inline bool allow_rsD_off(bool vsSecI, RS_State rsC, bool koppelSec) {
  bool vsUit = !vsSecI;
  return (vsUit) || (koppelSec && rsC == RS_ON);
}

//  SETUP
void setup() {
  if (DEBUG) {
    Serial.begin(9600);
    delay(200);
    Serial.println(F("-"));
    Serial.println(F("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"));
    Serial.println(F("Start: schema-logica + koppelveld + knoppen (51/52/53) + 1->7 verbindingcheck + pairs + 4 extra leds + VOLT_SENSE."));
  }

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(KOPPEL_PRIM_PIN, INPUT_PULLUP);
  pinMode(KOPPEL_SEC_PIN, INPUT_PULLUP);

  // 2 storingsknoppen
  pinMode(BTN_VELD51_PIN, INPUT_PULLUP);
  pinMode(BTN_VELD53_PIN, INPUT_PULLUP);

  // 2 bronnen: altijd LOW (als "GND-bron")
  pinMode(SRC1_PIN, OUTPUT);
  digitalWrite(SRC1_PIN, LOW);
  pinMode(SRC2_PIN, OUTPUT);
  digitalWrite(SRC2_PIN, LOW);

  // 7 ontvangers
  for (uint8_t i = 0; i < NUM_SENSE; i++) {
    pinMode(SENSE_PINS[i], INPUT_PULLUP);
  }

  // pairs
  for (uint8_t i = 0; i < NUM_PAIRS; i++) {
    pinMode(PAIR_TX_PINS[i], OUTPUT);
    digitalWrite(PAIR_TX_PINS[i], LOW);
    pinMode(PAIR_RX_PINS[i], INPUT_PULLUP);
  }

  // NIEUW: 2 LED-sets (4 leds totaal)
  pinMode(LED_SET_C_PIN, OUTPUT);
  pinMode(LED_SET_D_PIN, OUTPUT);
  digitalWrite(LED_SET_C_PIN, HIGH);  // normaal AAN
  digitalWrite(LED_SET_D_PIN, HIGH);  // normaal AAN

  // NIEUW: voltage sense pin (optioneel, analoog werkt ook zonder pinMode)
  pinMode(VOLT_SENSE_PIN, INPUT);

  // veld pinmodes
  for (auto& v : velden) {
    pinMode(v.rsA, INPUT_PULLUP);
    pinMode(v.rsB, INPUT_PULLUP);
    pinMode(v.rsC, INPUT_PULLUP);
    pinMode(v.rsD, INPUT_PULLUP);

    pinMode(v.vsPrim, INPUT_PULLUP);
    pinMode(v.vsSec, INPUT_PULLUP);

    pinMode(v.ledPrim, OUTPUT);
    pinMode(v.ledSec, OUTPUT);
    digitalWrite(v.ledPrim, LOW);
    digitalWrite(v.ledSec, LOW);
  }
}

//  LOOP
void loop() {
  unsigned long now = millis();

  //  SPANNING-DETECT
  int adc = analogRead(VOLT_SENSE_PIN);                       // 0..1023
  unsigned long mv = (unsigned long)adc * ADC_REF_MV / 1023;  // naar mV
  bool voltHigh = (mv >= VTHRESH_MV);

  static bool prevVoltHigh = false;
  if (!prevVoltHigh && voltHigh) {
    triggerFault("SYSTEEM", "VOLT_SENSE", "Spanning >= 200mV gedetecteerd -> buzzer AAN");
  }
  prevVoltHigh = voltHigh;

  if (voltHigh) {
    buzzerUntil = now + BUZZER_LATCH_MS;
  }

  // blink patroon voor storing LEDs
  static unsigned long lastBlink = 0;
  static bool blinkState = false;
  if (now - lastBlink >= BLINK_INTERVAL) {
    blinkState = !blinkState;
    lastBlink = now;
  }

  // koppelveld status
  bool koppelPrimActief = (digitalRead(KOPPEL_PRIM_PIN) == LOW);
  bool koppelSecActief = (digitalRead(KOPPEL_SEC_PIN) == LOW);

  // knoppen status (LOW = actief)
  bool btnVeld[3];
  btnVeld[0] = (digitalRead(BTN_VELD51_PIN) == LOW);
  btnVeld[1] = (digitalRead(BTN_VELD53_PIN) == LOW);
  btnVeld[2] = false;  // FIX: geen knop voor trafo 3, was niet geinitialiseerd

  // ===== VERBINDINGSCHECK (vroegtijdig lezen voor storing-LED logica) =====
  // 7 sense-ontvangers (storing 1)
  static bool prevSenseOk[NUM_SENSE] = { false, false, false, false, false, false, false };
  bool senseOk[NUM_SENSE];
  bool allSenseOk = true;

  for (uint8_t i = 0; i < NUM_SENSE; i++) {
    bool ok = (digitalRead(SENSE_PINS[i]) == LOW);
    senseOk[i] = ok;
    if (!ok) allSenseOk = false;

    if (ok != prevSenseOk[i]) {
      if (DEBUG) {
        Serial.print(F("SENSE "));
        Serial.print(i + 1);
        Serial.print(F(" (pin "));
        Serial.print(SENSE_PINS[i]);
        Serial.print(F(") = "));
        Serial.println(ok ? F("AANGESLOTEN") : F("NIET AANGESLOTEN"));
      }
      prevSenseOk[i] = ok;
    }
  }

  static bool prevAllSenseOk = true;
  if (allSenseOk != prevAllSenseOk) {
    if (DEBUG) {
      if (!allSenseOk) Serial.println(F("!!! VERBINDING FOUT: minstens 1 van de 7 ontvangers is HIGH (geen aansluiting)."));
      else Serial.println(F("OK: alle 7 ontvangers zijn LOW (aangesloten)."));
    }
    prevAllSenseOk = allSenseOk;
  }

  // pairs check (storing 2)
  static bool prevPairOk[NUM_PAIRS] = { false };
  bool pairOk[NUM_PAIRS];
  bool allPairsOk = true;

  for (uint8_t i = 0; i < NUM_PAIRS; i++) {
    bool ok = (digitalRead(PAIR_RX_PINS[i]) == LOW);
    pairOk[i] = ok;
    if (!ok) allPairsOk = false;

    if (ok != prevPairOk[i]) {
      if (DEBUG) {
        Serial.print(F("PAIR "));
        Serial.print(i + 1);
        Serial.print(F(" (TX "));
        Serial.print(PAIR_TX_PINS[i]);
        Serial.print(F(" -> RX "));
        Serial.print(PAIR_RX_PINS[i]);
        Serial.print(F(") = "));
        Serial.println(ok ? F("OK (ontvangt)") : F("FOUT (geen ontvangst)"));
      }
      prevPairOk[i] = ok;
    }
  }

  static bool prevAllPairsOk = true;
  if (allPairsOk != prevAllPairsOk) {
    if (DEBUG) {
      if (!allPairsOk) Serial.println(F("!!! PAIR FOUT: minstens 1 pair ontvangt geen signaal (RX HIGH)."));
      else Serial.println(F("OK: alle pairs ontvangen signaal (RX LOW)."));
    }
    prevAllPairsOk = allPairsOk;
  }

  // ===== HUIDIGE TOESTAND LEZEN =====
  RS_State rsA[3], rsB[3], rsC[3], rsD[3];
  bool vsPrimI[3], vsSecI[3];

  for (int i = 0; i < 3; i++) {
    Veld& v = velden[i];
    rsA[i] = readRS(v.rsA);
    rsB[i] = readRS(v.rsB);
    rsC[i] = readRS(v.rsC);
    rsD[i] = readRS(v.rsD);
    vsPrimI[i] = VS_is_I(v.vsPrim);
    vsSecI[i] = VS_is_I(v.vsSec);
  }

  // ===== FIX BUG 4: STORING-LED LOGICA =====
  // Knop uit          -> LED uit
  // Knop aan + niet opgelost -> LED knippert
  // Knop aan + opgelost      -> LED continu aan
  //
  // Storing 1 (trafo 1, index 0): opgelost = allSenseOk
  // Storing 2 (trafo 2, index 1): opgelost = allPairsOk
  // Trafo 3 (index 2): geen storing-knop
  for (int i = 0; i < 3; i++) {
    Veld& v = velden[i];

    bool primLed = false;
    bool secLed = false;

    if (btnVeld[i]) {
      bool storingOpgelost = false;
      if (i == 0) storingOpgelost = allSenseOk;
      else if (i == 1) storingOpgelost = allPairsOk;

      if (storingOpgelost) {
        primLed = true;   // continu aan
        secLed = true;
      } else {
        primLed = blinkState;  // knipperen
        secLed = blinkState;
      }
    }

    digitalWrite(v.ledPrim, primLed ? HIGH : LOW);
    digitalWrite(v.ledSec, secLed ? HIGH : LOW);
  }

  // ===== FIX BUG 2: KOPPELVELD ALARM =====
  // Als koppelveld uitschakelt terwijl beide RS (A+B of C+D) in een veld aan staan -> alarm
  static bool prevKoppelPrimActief = false;
  static bool prevKoppelSecActief = false;

  if (prevKoppelPrimActief && !koppelPrimActief) {
    for (int i = 0; i < 3; i++) {
      if (rsA[i] == RS_ON && rsB[i] == RS_ON) {
        triggerFault(velden[i].naam, "KOPPEL-PRIM", "Koppelveld prim UIT terwijl RS-A en RS-B beide IN staan!");
      }
    }
  }
  if (prevKoppelSecActief && !koppelSecActief) {
    for (int i = 0; i < 3; i++) {
      if (rsC[i] == RS_ON && rsD[i] == RS_ON) {
        triggerFault(velden[i].naam, "KOPPEL-SEC", "Koppelveld sec UIT terwijl RS-C en RS-D beide IN staan!");
      }
    }
  }

  // Continue check: koppelveld uit en beide RS aan = blijvend alarm
  if (!koppelPrimActief) {
    for (int i = 0; i < 3; i++) {
      if (rsA[i] == RS_ON && rsB[i] == RS_ON) {
        buzzerUntil = now + BUZZER_LATCH_MS;
      }
    }
  }
  if (!koppelSecActief) {
    for (int i = 0; i < 3; i++) {
      if (rsC[i] == RS_ON && rsD[i] == RS_ON) {
        buzzerUntil = now + BUZZER_LATCH_MS;
      }
    }
  }

  prevKoppelPrimActief = koppelPrimActief;
  prevKoppelSecActief = koppelSecActief;

  // EXTRA BUZZER-REGELS
  bool allVsPrimOut = (!vsPrimI[0] && !vsPrimI[1] && !vsPrimI[2]);
  bool allVsSecOut = (!vsSecI[0] && !vsSecI[1] && !vsSecI[2]);
  bool allVsPrimAndSecOut = (allVsPrimOut && allVsSecOut);

  bool allRsCDOut =
    (rsC[0] == RS_OFF && rsD[0] == RS_OFF) && (rsC[1] == RS_OFF && rsD[1] == RS_OFF) && (rsC[2] == RS_OFF && rsD[2] == RS_OFF);

  bool allRsABOut =
    (rsA[0] == RS_OFF && rsB[0] == RS_OFF) && (rsA[1] == RS_OFF && rsB[1] == RS_OFF) && (rsA[2] == RS_OFF && rsB[2] == RS_OFF);

  bool globalBuzzerCond = allVsSecOut || allVsPrimOut || allVsPrimAndSecOut || allRsCDOut || allRsABOut || voltHigh;

  static bool prevAllVsPrimOut = false;
  static bool prevAllVsSecOut = false;
  static bool prevAllVsBothOut = false;
  static bool prevAllRsCDOut = false;
  static bool prevAllRsABOut = false;

  if (!prevAllVsPrimOut && allVsPrimOut) triggerFault("SYSTEEM", "GLOBAL", "ALLE VS_PRIM = UIT(O) -> buzzer AAN");
  if (!prevAllVsSecOut && allVsSecOut) triggerFault("SYSTEEM", "GLOBAL", "ALLE VS_SEC  = UIT(O) -> buzzer AAN");
  if (!prevAllVsBothOut && allVsPrimAndSecOut) triggerFault("SYSTEEM", "GLOBAL", "ALLE VS_PRIM en VS_SEC = UIT(O) -> buzzer AAN");
  if (!prevAllRsCDOut && allRsCDOut) triggerFault("SYSTEEM", "GLOBAL", "ALLE RS_C en RS_D = UIT -> buzzer AAN");
  if (!prevAllRsABOut && allRsABOut) triggerFault("SYSTEEM", "GLOBAL", "ALLE RS_A en RS_B = UIT -> buzzer AAN");

  prevAllVsPrimOut = allVsPrimOut;
  prevAllVsSecOut = allVsSecOut;
  prevAllVsBothOut = allVsPrimAndSecOut;
  prevAllRsCDOut = allRsCDOut;
  prevAllRsABOut = allRsABOut;

  if (globalBuzzerCond) {
    buzzerUntil = now + BUZZER_LATCH_MS;
  }

  // ===== FIX BUG 3: RAIL-LED LOGICA MET POWER FLOW MODEL =====
  // Bepaal welke 50kV rails spanning hebben
  // Rail D (50kV) = altijd spanning (12V van Arduino)
  // Rail C (50kV) = alleen spanning als Koppelveld Prim (V55) actief is
  bool railCPowered = koppelPrimActief;
  bool railDPowered = true;

  // Bepaal per trafo of deze de 10kV-rail kan voeden
  bool rail1Fed = false;  // 10kV Rail 1 (via RS_C)
  bool rail2Fed = false;  // 10kV Rail 2 (via RS_D)

  for (int i = 0; i < 3; i++) {
    // Heeft deze trafo 50kV spanning?
    bool has50kV = false;
    if (rsA[i] == RS_ON && railCPowered) has50kV = true;
    if (rsB[i] == RS_ON && railDPowered) has50kV = true;

    // Kan de trafo 10kV voeden? (beide VS'en moeten IN staan)
    bool canFeed10kV = has50kV && vsPrimI[i] && vsSecI[i];

    if (canFeed10kV && rsC[i] == RS_ON) rail1Fed = true;
    if (canFeed10kV && rsD[i] == RS_ON) rail2Fed = true;
  }

  // Als Koppelveld Sec (V16) actief is, zijn Rail 1 en Rail 2 verbonden
  if (koppelSecActief && (rail1Fed || rail2Fed)) {
    rail1Fed = true;
    rail2Fed = true;
  }

  digitalWrite(LED_SET_C_PIN, rail1Fed ? HIGH : LOW);
  digitalWrite(LED_SET_D_PIN, rail2Fed ? HIGH : LOW);

  // Debug logging voor rail-LEDs
  static bool prevRail1Fed = true;
  static bool prevRail2Fed = true;

  // Rail spanning verloren -> buzzer
  if (prevRail1Fed && !rail1Fed) {
    triggerFault("SYSTEEM", "RAIL-1", "10kV Rail 1 heeft geen spanning meer!");
  }
  if (prevRail2Fed && !rail2Fed) {
    triggerFault("SYSTEEM", "RAIL-2", "10kV Rail 2 heeft geen spanning meer!");
  }

  if (rail1Fed != prevRail1Fed) {
    if (DEBUG) {
      Serial.print(F("RAIL-1 LED (pin "));
      Serial.print(LED_SET_C_PIN);
      Serial.print(F(") = "));
      Serial.println(rail1Fed ? F("AAN (spanning)") : F("UIT (geen spanning)"));
    }
    prevRail1Fed = rail1Fed;
  }

  if (rail2Fed != prevRail2Fed) {
    if (DEBUG) {
      Serial.print(F("RAIL-2 LED (pin "));
      Serial.print(LED_SET_D_PIN);
      Serial.print(F(") = "));
      Serial.println(rail2Fed ? F("AAN (spanning)") : F("UIT (geen spanning)"));
    }
    prevRail2Fed = rail2Fed;
  }

  //  EDGE DETECT RS-fouten (bestaand)
  static RS_State prevA[3] = { RS_OFF, RS_OFF, RS_OFF };
  static RS_State prevB[3] = { RS_OFF, RS_OFF, RS_OFF };
  static RS_State prevC[3] = { RS_OFF, RS_OFF, RS_OFF };
  static RS_State prevD[3] = { RS_OFF, RS_OFF, RS_OFF };

  for (int i = 0; i < 3; i++) {
    Veld& v = velden[i];

    if (prevA[i] == RS_OFF && rsA[i] == RS_ON) {
      if (!allow_rsA_on(vsPrimI[i], rsB[i], koppelPrimActief)) triggerFault(v.naam, "RS-A", "IN niet toegestaan (schema Inschakelen RS-A).");
    }
    if (prevA[i] == RS_ON && rsA[i] == RS_OFF) {
      if (!allow_rsA_off(vsPrimI[i], rsB[i], koppelPrimActief)) triggerFault(v.naam, "RS-A", "UIT niet toegestaan (schema Uitschakelen RS-A).");
    }

    if (prevB[i] == RS_OFF && rsB[i] == RS_ON) {
      if (!allow_rsB_on(vsPrimI[i], rsA[i], koppelPrimActief)) triggerFault(v.naam, "RS-B", "IN niet toegestaan (schema Inschakelen RS-B).");
    }
    if (prevB[i] == RS_ON && rsB[i] == RS_OFF) {
      if (!allow_rsB_off(vsPrimI[i], rsA[i], koppelPrimActief)) triggerFault(v.naam, "RS-B", "UIT niet toegestaan (schema Uitschakelen RS-B).");
    }

    if (prevC[i] == RS_OFF && rsC[i] == RS_ON) {
      if (!allow_rsC_on(vsSecI[i], rsD[i], koppelSecActief)) triggerFault(v.naam, "RS-C", "IN niet toegestaan (schema Inschakelen RS-C).");
    }
    if (prevC[i] == RS_ON && rsC[i] == RS_OFF) {
      if (!allow_rsC_off(vsSecI[i], rsD[i], koppelSecActief)) triggerFault(v.naam, "RS-C", "UIT niet toegestaan (schema Uitschakelen RS-C).");
    }

    if (prevD[i] == RS_OFF && rsD[i] == RS_ON) {
      if (!allow_rsD_on(vsSecI[i], rsC[i], koppelSecActief)) triggerFault(v.naam, "RS-D", "IN niet toegestaan (schema Inschakelen RS-D).");
    }
    if (prevD[i] == RS_ON && rsD[i] == RS_OFF) {
      if (!allow_rsD_off(vsSecI[i], rsC[i], koppelSecActief)) triggerFault(v.naam, "RS-D", "UIT niet toegestaan (schema Uitschakelen RS-D).");
    }
  }

  for (int i = 0; i < 3; i++) {
    prevA[i] = rsA[i];
    prevB[i] = rsB[i];
    prevC[i] = rsC[i];
    prevD[i] = rsD[i];
  }

  // BUZZER LATCH
  bool buzzerOn = ((long)(buzzerUntil - now) > 0);
  digitalWrite(BUZZER_PIN, buzzerOn ? HIGH : LOW);

  //  PERIODIEKE STATUS
  static unsigned long lastStatus = 0;
  if (DEBUG && ((millis() - lastStatus) > STATUS_INTERVAL_MS)) {
    Serial.println(F("KOPPELVELD PRIMAIR = "));
    Serial.print(koppelPrimActief ? "IN" : "UIT");
    Serial.print(F(" | KOPPELVELD SECUNDAIR = "));
    Serial.println(koppelSecActief ? "IN" : "UIT");

    Serial.print(F("VOLT_SENSE (A3) = "));
    Serial.print(mv);
    Serial.print(F("mV | boven drempel="));
    Serial.println(voltHigh ? F("JA") : F("NEE"));

    Serial.print(F("RAIL LEDS: Rail1="));
    Serial.print(rail1Fed ? "AAN" : "UIT");
    Serial.print(F(" | Rail2="));
    Serial.println(rail2Fed ? "AAN" : "UIT");

    Serial.print(F(" || Storing trafo 1 = "));
    Serial.print(btnVeld[0] ? "AAN" : "UIT");
    if (btnVeld[0]) { Serial.print(allSenseOk ? F(" (opgelost)") : F(" (NIET opgelost)")); }
    Serial.print(F(" | Storing trafo 2 = "));
    Serial.print(btnVeld[1] ? "AAN" : "UIT");
    if (btnVeld[1]) { Serial.print(allPairsOk ? F(" (opgelost)") : F(" (NIET opgelost)")); }
    Serial.println();

    for (int i = 0; i < 3; i++) {
      Serial.print(velden[i].naam);
      Serial.print(F(" -> RS-A= "));
      Serial.print(rsTxt(rsA[i]));
      Serial.print(F(" | RS-B= "));
      Serial.print(rsTxt(rsB[i]));
      Serial.print(F(" | VS-PRIM= "));
      Serial.print(vsTxt(vsPrimI[i]));
      Serial.print(F(" || VS-SEC= "));
      Serial.print(vsTxt(vsSecI[i]));
      Serial.print(F(" | RS-C= "));
      Serial.print(rsTxt(rsC[i]));
      Serial.print(F(" | RS-D= "));
      Serial.println(rsTxt(rsD[i]));
    }

    Serial.print(F("GLOBAL: alle VS primair Out = "));
    Serial.print(allVsPrimOut ? "JA" : "NEE");
    Serial.print(F(" | alle VS secundair Out = "));
    Serial.print(allVsSecOut ? "JA" : "NEE");
    Serial.print(F(" | all RS A+B Out = "));
    Serial.print(allRsABOut ? "JA" : "NEE");
    Serial.print(F(" | all RS C+D Out = "));
    Serial.println(allRsCDOut ? "JA" : "NEE");

    Serial.print(F("Storing 1 verbindingen compleet? = "));
    Serial.println(allSenseOk ? F("JA") : F("NEE"));

    Serial.println(F("SENSE STATUS (1..7):"));
    for (uint8_t i = 0; i < NUM_SENSE; i++) {
      Serial.print(F(" - SENSE "));
      Serial.print(i + 1);
      Serial.print(F(" (pin "));
      Serial.print(SENSE_PINS[i]);
      Serial.print(F(") = "));
      Serial.println(senseOk[i] ? F("AANGESLOTEN") : F("NIET AANGESLOTEN"));
    }

    Serial.println(F("PAIR STATUS (1..2):"));
    for (uint8_t i = 0; i < NUM_PAIRS; i++) {
      Serial.print(F(" - PAIR "));
      Serial.print(i + 1);
      Serial.print(F(" (TX "));
      Serial.print(PAIR_TX_PINS[i]);
      Serial.print(F(" -> RX "));
      Serial.print(PAIR_RX_PINS[i]);
      Serial.print(F(") = "));
      Serial.println(pairOk[i] ? F("OK") : F("FOUT"));
    }

    Serial.print(F("BUZZER = "));
    Serial.println(buzzerOn ? F("AAN") : F("UIT"));

    Serial.println(F("====================================================================================="));

    lastStatus = millis();
  }

  delay(5);
}
