# language: nl
Functionaliteit: Uitval detectie
  Als een 10kV rail spanning verliest zitten klanten zonder stroom.

  Scenario: Rail verliest spanning door RS openen
    Gegeven de startpositie
    Als ik trafo 1 RS-2 open
    Dan volgt een uitval op Rail 2

  Scenario: Geen uitval als rail nog gevoed wordt
    Gegeven de startpositie
    Als ik trafo 1 RS-C sluit
    Dan volgt geen uitval

  Scenario: Beide rails verliezen spanning tegelijk
    Gegeven een lege schakelbox
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    En trafo 2 RS-D is dicht
    En trafo 2 VS-prim is dicht
    En trafo 2 VS-sec is dicht
    En trafo 2 RS-2 is dicht
    Als ik trafo 1 RS-1 open
    Dan volgt een uitval op Rail 1
    Als ik trafo 2 RS-2 open
    Dan volgt een uitval op Rail 2

  Scenario: Rail verliest spanning indirect door VS openen
    Gegeven de startpositie
    Als ik trafo 1 VS-sec open
    Dan heeft Rail 2 geen spanning

  Scenario: Rail houdt spanning want andere trafo neemt over
    Gegeven een lege schakelbox
    En koppelveld secundair is dicht
    En trafo 1 RS-D is dicht
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    En trafo 1 RS-1 is dicht
    En trafo 2 RS-D is dicht
    En trafo 2 VS-prim is dicht
    En trafo 2 VS-sec is dicht
    En trafo 2 RS-2 is dicht
    Als ik trafo 1 RS-1 open
    Dan volgt geen uitval
    En heeft Rail 1 spanning

  Scenario: Uitval op Rail 1 door RS-1 openen
    Gegeven de startpositie
    En trafo 2 RS-C is open
    En trafo 2 RS-1 is open
    Als ik trafo 1 RS-2 open
    Dan volgt een uitval op Rail 2
