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
