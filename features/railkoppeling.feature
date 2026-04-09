# language: nl
Functionaliteit: Railkoppeling detectie
  Twee railscheiders tegelijk dicht zonder koppelveld is een schakelfout.
  Met koppelveld mag het maximaal 60 seconden.

  Scenario: Beide RS dicht zonder koppelveld is een schakelfout
    Gegeven een lege schakelbox
    En trafo 1 RS-C is dicht
    En trafo 1 RS-D is dicht
    Dan volgt een railkoppeling-fout

  Scenario: Beide RS dicht met koppelveld is toegestaan
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En trafo 1 RS-C is dicht
    En trafo 1 RS-D is dicht
    Dan volgt geen railkoppeling-fout

  Scenario: Beide RS 10kV dicht zonder koppelveld secundair
    Gegeven een lege schakelbox
    En trafo 1 RS-1 is dicht
    En trafo 1 RS-2 is dicht
    Dan volgt een railkoppeling-fout

  Scenario: Koppelveld openen terwijl beide RS dicht
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En trafo 1 RS-C is dicht
    En trafo 1 RS-D is dicht
    Als ik koppelveld primair open
    Dan volgt een railkoppeling-fout

  Scenario: Meerdere trafos met beide RS dicht zonder koppelveld primair
    Gegeven een lege schakelbox
    En trafo 1 RS-C is dicht
    En trafo 1 RS-D is dicht
    En trafo 2 RS-C is dicht
    En trafo 2 RS-D is dicht
    Dan volgt een railkoppeling-fout

  Scenario: Meerdere trafos met beide RS dicht met koppelveld primair
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En trafo 1 RS-C is dicht
    En trafo 1 RS-D is dicht
    En trafo 2 RS-C is dicht
    En trafo 2 RS-D is dicht
    Dan volgt geen railkoppeling-fout

  Scenario: Koppelveld openen terwijl meerdere trafos beide RS dicht
    Gegeven een lege schakelbox
    En koppelveld primair is dicht
    En trafo 1 RS-C is dicht
    En trafo 1 RS-D is dicht
    En trafo 2 RS-C is dicht
    En trafo 2 RS-D is dicht
    Als ik koppelveld primair open
    Dan volgt een railkoppeling-fout

  Scenario: Beide RS 10kV dicht met koppelveld secundair is toegestaan
    Gegeven een lege schakelbox
    En koppelveld secundair is dicht
    En trafo 1 RS-1 is dicht
    En trafo 1 RS-2 is dicht
    Dan volgt geen railkoppeling-fout

  Scenario: Eén RS per rail bij verschillende trafos is geen railkoppeling
    Gegeven een lege schakelbox
    En trafo 1 RS-C is dicht
    En trafo 2 RS-D is dicht
    Dan volgt geen railkoppeling-fout
