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

## Simulator

Open `schakelbox_simulator_v7.html` in een browser. Links de schakelbox, rechts het Arduino pinout. Pins lichten op als je schakelt.
