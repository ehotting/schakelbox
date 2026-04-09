# language: nl
Functionaliteit: Verkeerde schakelvolgorde
  Uitschakelen: eerst VS-sec, dan VS-prim.
  Inschakelen: eerst VS-prim, dan VS-sec.

  Scenario: VS-prim uit terwijl VS-sec nog in
    Gegeven een lege schakelbox
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    Als ik trafo 1 VS-prim open
    Dan volgt een volgorde-fout

  Scenario: VS-sec in terwijl VS-prim nog uit
    Gegeven een lege schakelbox
    Als ik trafo 1 VS-sec sluit
    Dan volgt een volgorde-fout

  Scenario: Correcte uitschakelvolgorde
    Gegeven een lege schakelbox
    En trafo 1 VS-prim is dicht
    En trafo 1 VS-sec is dicht
    Als ik trafo 1 VS-sec open
    Dan volgt geen volgorde-fout

  Scenario: Correcte inschakelvolgorde
    Gegeven een lege schakelbox
    Als ik trafo 1 VS-prim sluit
    En ik trafo 1 VS-sec sluit
    Dan volgt geen volgorde-fout

  Scenario: VS-prim uit terwijl VS-sec in op trafo 2
    Gegeven een lege schakelbox
    En trafo 2 VS-prim is dicht
    En trafo 2 VS-sec is dicht
    Als ik trafo 2 VS-prim open
    Dan volgt een volgorde-fout

  Scenario: VS-prim uit terwijl VS-sec in op trafo 3
    Gegeven een lege schakelbox
    En trafo 3 VS-prim is dicht
    En trafo 3 VS-sec is dicht
    Als ik trafo 3 VS-prim open
    Dan volgt een volgorde-fout

  Scenario: VS-sec in terwijl VS-prim uit op trafo 2
    Gegeven een lege schakelbox
    Als ik trafo 2 VS-sec sluit
    Dan volgt een volgorde-fout

  Scenario: Correcte uitschakelvolgorde op trafo 2
    Gegeven een lege schakelbox
    En trafo 2 VS-prim is dicht
    En trafo 2 VS-sec is dicht
    Als ik trafo 2 VS-sec open
    Dan volgt geen volgorde-fout

  Scenario: Correcte inschakelvolgorde op trafo 3
    Gegeven een lege schakelbox
    Als ik trafo 3 VS-prim sluit
    En ik trafo 3 VS-sec sluit
    Dan volgt geen volgorde-fout
