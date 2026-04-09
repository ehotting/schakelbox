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
