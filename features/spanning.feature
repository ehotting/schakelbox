# language: nl
Functionaliteit: Spanning propagatie
  Spanning wordt gepropageerd van Rail D door gesloten schakelaars.

  Scenario: Rail D heeft altijd spanning
    Gegeven een lege schakelbox
    Dan heeft Rail D spanning
    En heeft Rail C geen spanning

  Scenario: Koppelveld primair verbindt Rail C en D
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    Dan heeft Rail C spanning
    En heeft Rail D spanning

  Scenario: Trafo voedt 10kV rail via gesloten pad
    Gegeven een lege schakelbox
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-2 is dicht
    Dan heeft Rail 2 spanning

  Scenario: VS open blokkeert spanning
    Gegeven een lege schakelbox
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is open
    En trafo 1 RS-2 is dicht
    Dan heeft Rail 2 geen spanning

  Scenario: Backfeed via 10kV zijde
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En trafo 2 RS-C is dicht
    En trafo 2 VS-prim is dicht
    En trafo 2 VS-sec is dicht
    En trafo 2 RS-1 is dicht
    En trafo 1 RS-2 is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En koppelveld secundair is dicht
    Dan heeft trafoPrim van trafo 1 spanning

  Scenario: Startpositie heeft spanning op beide 10kV rails
    Gegeven de startpositie
    Dan heeft Rail 1 spanning
    En heeft Rail 2 spanning
