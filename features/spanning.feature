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

  Scenario: Koppelveld sec verbindt Rail 1 en Rail 2
    Gegeven een lege schakelbox
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    En koppelveld secundair is dicht
    Dan heeft Rail 1 spanning
    En heeft Rail 2 spanning

  Scenario: Meerdere trafos voeden dezelfde rail
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    En trafo 2 RS-C is dicht
    En trafo 2 VS-prim is dicht
    En trafo 2 VS-sec is dicht
    En trafo 2 RS-1 is dicht
    Dan heeft Rail 1 spanning

  Scenario: Trafo 3 in standby - VS open blokkeert spanning naar trafoSec
    Gegeven een lege schakelbox
    En trafo 3 RS-D is dicht
    En trafo 3 VS-prim is open
    En trafo 3 VS-sec is open
    En trafo 3 RS-2 is dicht
    Dan heeft trafoPrim van trafo 3 spanning
    En heeft trafoSec van trafo 3 geen spanning
    En heeft Rail 2 geen spanning

  Scenario: Alle RS van een rail loskoppelen geeft geen spanning
    Gegeven de startpositie
    En trafo 1 RS-2 is open
    Dan heeft Rail 2 geen spanning

  Scenario: Rail houdt spanning als tweede trafo het overneemt
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    En trafo 2 RS-C is dicht
    En trafo 2 VS-prim is dicht
    En trafo 2 VS-sec is dicht
    En trafo 2 RS-1 is dicht
    En trafo 1 RS-1 is open
    Dan heeft Rail 1 spanning

  Scenario: Trafo via RS-C zonder koppelveld krijgt geen 50kV spanning
    Gegeven een lege schakelbox
    En trafo 1 RS-C is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    Dan heeft Rail C geen spanning
    En heeft trafoPrim van trafo 1 geen spanning
    En heeft Rail 1 geen spanning
