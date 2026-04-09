# language: nl
Functionaliteit: Aarding
  Spanningspunten moeten geaard worden voor het veilig is om te werken.
  Aarden onder spanning veroorzaakt een vlamboog.

  Scenario: Aarden zonder spanning is veilig
    Gegeven de startpositie
    En trafo 1 RS-D is open
    En trafo 1 RS-C is open
    Als ik trafo 1 punt 1 aard
    Dan volgt geen schakelfout

  Scenario: Aarden onder spanning is een schakelfout
    Gegeven de startpositie
    Als ik trafo 1 punt 1 aard
    Dan volgt een schakelfout

  Scenario: Spanning op geaard punt is een schakelfout
    Gegeven de startpositie
    En trafo 1 RS-D is open
    En trafo 1 RS-C is open
    En trafo 1 punt 1 is geaard
    Als ik trafo 1 RS-D sluit
    Dan heeft trafoPrim van trafo 1 spanning
    En staat er spanning op geaard punt 1 van trafo 1

  Scenario: Middenpunt heeft spanning als VS-prim dicht
    Gegeven de startpositie
    Dan heeft het middenpunt van trafo 1 spanning

  Scenario: Aarden punt 2 onder spanning via VS-sec (trafoSec)
    Gegeven de startpositie
    Als ik trafo 1 punt 3 aard
    Dan volgt een schakelfout

  Scenario: Aarden punt 2 zonder spanning is veilig
    Gegeven een lege schakelbox
    Als ik trafo 1 punt 2 aard
    Dan volgt geen schakelfout

  Scenario: Middenpunt aarden wanneer alleen VS-sec dicht (backfeed van 10kV)
    Gegeven een lege schakelbox
    En trafo 2 RS-D is dicht
    En trafo 2 VS-prim is dicht
    En trafo 2 VS-sec is dicht
    En trafo 2 RS-1 is dicht
    En trafo 1 RS-1 is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 VS-prim is open
    En koppelveld secundair is dicht
    Als ik trafo 1 punt 2 aard
    Dan volgt een schakelfout

  Scenario: Meerdere punten tegelijk geaard zonder spanning
    Gegeven een lege schakelbox
    En trafo 1 punt 1 is geaard
    En trafo 1 punt 2 is geaard
    En trafo 1 punt 3 is geaard
    Als ik trafo 2 punt 1 aard
    Dan volgt geen schakelfout

  Scenario: Aarden wanneer geen spanning altijd veilig
    Gegeven een lege schakelbox
    Als ik trafo 1 punt 1 aard
    Dan volgt geen schakelfout
    Als ik trafo 2 punt 2 aard
    Dan volgt geen schakelfout
    Als ik trafo 3 punt 3 aard
    Dan volgt geen schakelfout

  Scenario: Aarden trafoSec punt 3 zonder spanning is veilig
    Gegeven de startpositie
    En trafo 1 RS-2 is open
    En trafo 1 VS-sec is open
    Als ik trafo 1 punt 3 aard
    Dan volgt geen schakelfout
