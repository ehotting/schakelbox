# language: nl
Functionaliteit: Schakelfout detectie
  Een railscheider mag niet geschakeld worden onder belasting.
  Uitzondering: omgaan van rail via koppelveld (zelfde potentiaal).

  Scenario: RS openen onder belasting is een schakelfout
    Gegeven de startpositie
    Als ik trafo 1 RS-D open
    Dan volgt een schakelfout

  Scenario: RS sluiten onder belasting is een schakelfout
    Gegeven de startpositie
    Als ik koppelveld primair open
    En ik trafo 1 RS-C sluit
    Dan volgt een schakelfout

  Scenario: Omgaan van rail via koppelveld is veilig
    Gegeven de startpositie
    Als ik trafo 1 RS-C sluit
    Dan volgt geen schakelfout

  Scenario: RS schakelen met VS open is veilig
    Gegeven een lege schakelbox
    En trafo 1 VS-prim is open
    Als ik trafo 1 RS-D sluit
    Dan volgt geen schakelfout

  Scenario: RS openen op 10kV zijde onder belasting
    Gegeven de startpositie
    Als ik trafo 1 RS-2 open
    Dan volgt een schakelfout

  Scenario: RS-1 sluiten onder belasting op 10kV zijde
    Gegeven de startpositie
    En trafo 1 RS-1 is open
    Als ik trafo 1 RS-1 sluit
    Dan volgt een schakelfout

  Scenario: RS-2 sluiten op 10kV zijde wanneer trafo al stroom levert via RS-1
    Gegeven een lege schakelbox
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    Als ik trafo 1 RS-2 sluit
    Dan volgt een schakelfout

  Scenario: RS-C sluiten zonder koppelveld terwijl andere trafo backfeed geeft
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En trafo 2 RS-D is dicht
    En trafo 2 VS-prim is dicht
    En trafo 2 VS-sec is dicht
    En trafo 2 RS-1 is dicht
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    Als ik koppelveld primair open
    En ik trafo 1 RS-C sluit
    Dan volgt een schakelfout

  Scenario: Laatste RS openen van rail met VS dicht geeft schakelfout
    Gegeven de startpositie
    Als ik trafo 1 RS-D open
    Dan volgt een schakelfout

  Scenario: RS-D sluiten op trafo 2 onder belasting zonder koppelveld
    Gegeven de startpositie
    Als ik koppelveld primair open
    En ik trafo 2 RS-D sluit
    Dan volgt een schakelfout

  Scenario: RS schakelen met VS open op 10kV zijde is veilig
    Gegeven een lege schakelbox
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is open
    Als ik trafo 1 RS-1 sluit
    Dan volgt geen schakelfout

  Scenario: RS-1 openen terwijl RS-2 dicht en koppelveld sec dicht is veilig
    # Koppelveld vereffent spanning over beide rails: stroom kan vloeiend
    # omleiden via RS-2+koppelveld als RS-1 wordt geopend.
    Gegeven een lege schakelbox
    En koppelveld secundair is dicht
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    En trafo 1 RS-2 is dicht
    Als ik trafo 1 RS-1 open
    Dan volgt geen schakelfout

  Scenario: RS-2 sluiten terwijl RS-1 dicht en koppelveld sec dicht is veilig
    # Beide kanten al onder zelfde spanning via koppelveld; sluiten is veilig.
    Gegeven een lege schakelbox
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    En koppelveld secundair is dicht
    Als ik trafo 1 RS-2 sluit
    Dan volgt geen schakelfout

  Scenario: Laatste RS openen terwijl koppelveld sec dicht geeft toch schakelfout
    # Eerst RS-1 open (veilig), dan RS-2 ook open: nu valt het hele 10kV-net
    # weg. Dat is wel een schakelfout (er liep nog stroom door RS-2).
    Gegeven een lege schakelbox
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-2 is dicht
    En koppelveld secundair is dicht
    Als ik trafo 1 RS-2 open
    Dan volgt een schakelfout

  Scenario: RS-C openen terwijl RS-D dicht en koppelveld prim dicht is veilig
    # 50kV variant: koppelveld prim biedt bypad voor primaire rails.
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En trafo 1 RS-C is dicht
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    Als ik trafo 1 RS-C open
    Dan volgt geen schakelfout
