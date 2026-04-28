# language: nl
Functionaliteit: Debounce van digitale inputs

  Achtergrond:
    Gegeven een debouncer met startwaarde HOOG en drempel 25 ms

  Scenario: stabiele input verandert niet zonder reden
    Wanneer de input HOOG blijft gedurende 100 ms
    Dan is de gefilterde waarde HOOG

  Scenario: korte ruispuls wordt genegeerd
    Wanneer de input op tijdstip 10 ms naar LAAG gaat
    En de input op tijdstip 15 ms terug naar HOOG gaat
    Dan is de gefilterde waarde HOOG

  Scenario: stabiele wissel wordt na de drempel geaccepteerd
    Wanneer de input op tijdstip 10 ms naar LAAG gaat
    En de input LAAG blijft tot tijdstip 40 ms
    Dan is de gefilterde waarde LAAG

  Scenario: wissel onder de drempel wordt nog niet geaccepteerd
    Wanneer de input op tijdstip 10 ms naar LAAG gaat
    En de input LAAG blijft tot tijdstip 30 ms
    Dan is de gefilterde waarde HOOG

  Scenario: ruisburst tijdens wachttijd reset de teller
    Wanneer de input op tijdstip 10 ms naar LAAG gaat
    En de input op tijdstip 20 ms terug naar HOOG gaat
    En de input op tijdstip 25 ms naar LAAG gaat
    En de input LAAG blijft tot tijdstip 45 ms
    Dan is de gefilterde waarde HOOG

  Scenario: na acceptatie blijft waarde stabiel onder volgende ruis
    Wanneer de input op tijdstip 0 ms naar LAAG gaat
    En de input LAAG blijft tot tijdstip 30 ms
    En de input op tijdstip 35 ms kort naar HOOG gaat
    En de input op tijdstip 38 ms terug naar LAAG gaat
    Dan is de gefilterde waarde LAAG
