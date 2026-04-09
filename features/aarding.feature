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
