# Schakelbox

Trainingssimulator voor het schakelen in een 50/10kV onderstation met 3 trafovelden. Draait op een Arduino Mega 2560 R3 met fysieke schakelaars, 12V signaal-LEDs en buzzers.

## Bestanden

- `schakelbox_v7.ino` - Arduino sketch
- `schakelbox_simulator_v7.html` - browser-simulator met live Arduino pinout

## Hoe het werkt

Spanning wordt gesimuleerd vanuit de 50kV rails, door scheiders en vermogensschakelaars naar de 10kV rails. Het power-flow model propageert bidirectioneel: stroom kan ook via de 10kV-kant terugkomen als daar een pad is. Op basis van de schakelstanden detecteert de software 5 soorten fouten.

### Fouten

1. Schakelfout: railscheider geschakeld onder belasting, of spanning op geaard punt
2. Uitval: 10kV rail verliest spanning, klanten zonder stroom
3. Onjuiste railkoppeling: twee railscheiders langer dan 60s dicht zonder koppelveld
4. Verkeerde volgorde: vermogensschakelaars in verkeerde volgorde geschakeld
5. Foute pin: verkeerde aansluiting bij storing-puzzel

### Spelscenario's

Storing 1: trafo 1 stuk. Monteur herstelt 6 verbindingen (A2-A7). A1 is een foute pin.
Storing 2: VS-prim trafo 2 stuk. Monteur verbindt A8 met A9. VS schakelt pas weer na oplossing.

Bij beide moet de trafo eerst geaard worden, anders volgt een schakelfout.

## Hardware

- Arduino Mega 2560 R3
- 18 toggle-schakelaars (railscheiders + vermogensschakelaars)
- 2 koppelveld-schakelaars
- 10x 12V signaal-LED via N-channel MOSFET
- 2 buzzers (kort + permanent)
- 2 storingsknoppen
- 9 spanningspunten + 3 aardepunten (bananenstekkers)
- 7 sense-aansluitingen + 1 pair-aansluiting voor de storingspuzzels

## Upload naar Arduino

```
arduino-cli compile --fqbn arduino:avr:mega schakelbox_v7.ino
arduino-cli upload -p /dev/cu.usbmodem101 --fqbn arduino:avr:mega schakelbox_v7.ino
```

## Pin layout

### Rechter reep (outputs + koppelvelden + spanningspunten)

| Pin | Functie |
|-----|---------|
| D2 | Rail C LED |
| D3 | Rail D LED |
| D4 | Rail 1 + Duinpad LED |
| D5 | Rail 2 + Fre de Rik LED |
| D6 | Buzzer kort |
| D7 | Buzzer permanent |
| D8-D9 | Storing LED T1 (prim, sec) |
| D10-D11 | Storing LED T2 (prim, sec) |
| D12-D13 | Storing LED T3 (prim, sec) |
| D14 | Koppelveld primair |
| D15 | Koppelveld secundair |
| D16-D18 | Spanningspunten T1 (boven, midden, onder) |
| D19-D21 | Spanningspunten T2 |
| D22-D24 | Spanningspunten T3 |

### Onderste reep (schakelaars + aardepunten)

| Pin | Functie |
|-----|---------|
| D26-D27 | T1 RS-C, RS-D |
| D28-D29 | T1 VS-prim, VS-sec |
| D30-D31 | T1 RS-1, RS-2 |
| D32-D37 | T2 (zelfde volgorde) |
| D38-D43 | T3 (zelfde volgorde) |
| D45-D47 | Aardepunten T1, T2, T3 (OUTPUT LOW) |

### Linker reep (storingen + analog)

| Pin | Functie |
|-----|---------|
| A0 | Voltage sense (optioneel) |
| A1 | Storing 1: foute pin |
| A2-A7 | Storing 1: goede pins |
| A8 | Storing 2: TX (OUTPUT LOW) |
| A9 | Storing 2: RX |
| D48 | Storingsknop T1 |
| D49 | Storingsknop T2 |

Schakelaars: INPUT_PULLUP, naar GND. LEDs: via N-channel MOSFET naar 12V.

## Tests

BDD tests in Gherkin, in het Nederlands. 63 scenario's voor spanning, schakelfout, uitval, railkoppeling, volgorde, aarding en randgevallen.

```
npm install
npm test
```

De pre-commit hook draait de tests automatisch bij elke commit.

## Simulator

Open `schakelbox_simulator_v7.html` in een browser. Links de schakelbox, rechts het Arduino pinout. Pins lichten op als je schakelt.

## Licentie

MIT
