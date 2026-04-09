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
