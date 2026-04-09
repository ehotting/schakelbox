# language: nl
Functionaliteit: Randgevallen
  Edge cases die niet in andere scenario's vallen.

  Scenario: Alle schakelaars open - geen fouten
    Gegeven een lege schakelbox
    Dan heeft Rail D spanning
    En heeft Rail C geen spanning
    En heeft Rail 1 geen spanning
    En heeft Rail 2 geen spanning
    En volgt geen schakelfout
    En volgt geen uitval
    En volgt geen railkoppeling-fout

  Scenario: Alle schakelaars dicht geeft railkoppeling-fouten
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En koppelveld secundair is dicht
    En trafo 1 RS-C is dicht
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    En trafo 1 RS-2 is dicht
    En trafo 2 RS-C is dicht
    En trafo 2 RS-D is dicht
    En trafo 2 VS-prim is dicht
    En trafo 2 VS-sec is dicht
    En trafo 2 RS-1 is dicht
    En trafo 2 RS-2 is dicht
    En trafo 3 RS-C is dicht
    En trafo 3 RS-D is dicht
    En trafo 3 VS-prim is dicht
    En trafo 3 VS-sec is dicht
    En trafo 3 RS-1 is dicht
    En trafo 3 RS-2 is dicht
    Dan heeft Rail D spanning
    En heeft Rail C spanning
    En heeft Rail 1 spanning
    En heeft Rail 2 spanning

  Scenario: Dezelfde schakelaar snel open en dicht
    Gegeven de startpositie
    Als ik trafo 1 RS-D open
    En ik trafo 1 RS-D sluit
    Dan heeft Rail 2 spanning

  Scenario: Koppelveld sec dicht zonder voeding op een rail doet niets
    Gegeven een lege schakelbox
    En koppelveld secundair is dicht
    Dan heeft Rail 1 geen spanning
    En heeft Rail 2 geen spanning

  Scenario: Trafo 3 volledig operationeel via Rail C
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En trafo 3 RS-C is dicht
    En trafo 3 VS-prim is dicht
    En trafo 3 VS-sec is dicht
    En trafo 3 RS-1 is dicht
    Dan heeft Rail 1 spanning

  Scenario: Drie trafos voeden tegelijk dezelfde rail
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
    En trafo 3 RS-D is dicht
    En trafo 3 VS-prim is dicht
    En trafo 3 VS-sec is dicht
    En trafo 3 RS-1 is dicht
    Dan heeft Rail 1 spanning
