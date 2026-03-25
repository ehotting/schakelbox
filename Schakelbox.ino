/* V4.1. last edit: 06/03/2026
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
    - RS A–D (2-pos, C–NC bekabeld)
    - VS prim & VS sec (3-pos, C–NC bekabeld)
    - LED’s per VS → knipperen bij I-stand
  Eén centrale buzzer (actief HIGH)
  ─────────────────────────────────────────────
  Uitbreiding:
  - Koppelveld_VS_prim → schakelt foutcontrole uit voor ALLE RS A & B (nieuw: C en D)
  - Koppelveld_VS_sec  → schakelt foutcontrole uit voor ALLE RS C & D (nieuw: 1 en 2)
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
#define LED_SET_C_PIN 50  // set 1 (2 leds in serie)
#define LED_SET_D_PIN 14  // set 2 (2 leds in serie)
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

//  VRIJGAVE-REGELS (schema’s)
// PRIM (A/B)oud > (C/D)nieuw
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

// SEC (C/D)oud > (1/2)nieuw
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

  // blink patroon voor VS LEDs
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

  // huidige toestand lezen
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

    // LEDs knipperen alleen als knop van dat veld actief is én VS = IN
    bool primBlink = (btnVeld[i] && vsPrimI[i] && blinkState);
    bool secBlink = (btnVeld[i] && vsSecI[i] && blinkState);

    digitalWrite(v.ledPrim, primBlink ? HIGH : LOW);
    digitalWrite(v.ledSec, secBlink ? HIGH : LOW);
  }

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
  if (!prevAllVsBothOut && allVsPrimAndSecOut) triggerFault("SYSTEEM", "GLOBAL", "ALLE VS_PRIM én VS_SEC = UIT(O) -> buzzer AAN");
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

  //  NIEUW: 4 LEDS (2 sets) LOGICA + SERIAL [MW: voorwaarden kloppen niet! Hier moet worden gekeken of een 10kV-rail wel of geen spanning heeft]
  bool allRsCOut = (rsC[0] == RS_OFF && rsC[1] == RS_OFF && rsC[2] == RS_OFF);
  bool allRsDOut = (rsD[0] == RS_OFF && rsD[1] == RS_OFF && rsD[2] == RS_OFF);

  bool ledSetC_on = true;
  bool ledSetD_on = true;

  bool ledsAllOffCond = (allVsPrimOut || allVsSecOut || allRsCDOut || allRsABOut);

  if (ledsAllOffCond) {
    ledSetC_on = false;
    ledSetD_on = false;
  } else {
    if (!koppelSecActief) {
      if (allRsCOut && !allRsDOut) ledSetC_on = false;
      if (allRsDOut && !allRsCOut) ledSetD_on = false;
    }
  }

  digitalWrite(LED_SET_C_PIN, ledSetC_on ? HIGH : LOW);
  digitalWrite(LED_SET_D_PIN, ledSetD_on ? HIGH : LOW);

  static bool prevLedSetC = true;
  static bool prevLedSetD = true;
  static bool prevLedsAllOff = false;

  if (ledsAllOffCond != prevLedsAllOff) {
    if (DEBUG) {
      if (ledsAllOffCond) {
        Serial.print(F("LED-SETS: ALLE 4 UIT | reden: "));
        bool first = true;
        if (allVsPrimOut) {
          Serial.print(first ? F("ALL VS_PRIM OUT") : F(", ALL VS_PRIM OUT"));
          first = false;
        }
        if (allVsSecOut) {
          Serial.print(first ? F("ALL VS_SEC OUT") : F(", ALL VS_SEC OUT"));
          first = false;
        }
        if (allRsABOut) {
          Serial.print(first ? F("ALL RS_A+B OUT") : F(", ALL RS_A+B OUT"));
          first = false;
        }
        if (allRsCDOut) {
          Serial.print(first ? F("ALL RS_C+D OUT") : F(", ALL RS_C+D OUT"));
          first = false;
        }
        Serial.println();
      } else {
        Serial.println(F("LED-SETS: niet meer in 'alles-uit' conditie (terug naar normale logica)."));
      }
    }
    prevLedsAllOff = ledsAllOffCond;
  }

  if (ledSetC_on != prevLedSetC) {
    if (DEBUG) {
      Serial.print(F("LED-SET C (pin "));
      Serial.print(LED_SET_C_PIN);
      Serial.print(F(") = "));
      Serial.print(ledSetC_on ? F("AAN") : F("UIT"));
      if (!ledSetC_on && !ledsAllOffCond && !koppelSecActief && allRsCOut && !allRsDOut) {
        Serial.print(F(" | reden: koppelSec=UIT en ALLEEN RS_C UIT"));
      }
      Serial.println();
    }
    prevLedSetC = ledSetC_on;
  }

  if (ledSetD_on != prevLedSetD) {
    if (DEBUG) {
      Serial.print(F("LED-SET D (pin "));
      Serial.print(LED_SET_D_PIN);
      Serial.print(F(") = "));
      Serial.print(ledSetD_on ? F("AAN") : F("UIT"));
      if (!ledSetD_on && !ledsAllOffCond && !koppelSecActief && allRsDOut && !allRsCOut) {
        Serial.print(F(" | reden: koppelSec=UIT en ALLEEN RS_D UIT"));
      }
      Serial.println();
    }
    prevLedSetD = ledSetD_on;
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

  //  2->7 VERBINDINGSCHECK (SERIAL + per verandering)
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

  // pairs check
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

    Serial.print(F("EXTRA LEDS: SET_C="));
    Serial.print(prevLedSetC ? "AAN" : "UIT");
    Serial.print(F(" | SET_D="));
    Serial.print(prevLedSetD ? "AAN" : "UIT");
    Serial.print(F(" | allOffCond="));
    Serial.println(prevLedsAllOff ? "JA" : "NEE");

    Serial.print(F(" || Secundaire storing V1 = "));
    Serial.print(btnVeld[0] ? "IN" : "UIT");
    Serial.print(F(" | Secundaire storing V2 = "));
    Serial.print(btnVeld[1] ? "IN" : "UIT");
    Serial.print(F(" | Secundaire storing V3 = "));
    Serial.println(btnVeld[2] ? "IN" : "UIT");

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

    Serial.print(F("Zijn alle aansluitingen van de eerste storing compleet?  = "));
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