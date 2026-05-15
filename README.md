# INDRA Plugin VATSIM

## Release version

In order to get everything to work in the release version along side the DLL file, there should be a file called "indra_saved_views.json"
an example of which is shown bellow:
```[
{"button":"S","name":"View S","zoomNm":120,"centerLat":36.692194444444,"centerLon":3.2151111111111},
{"button":"0","name":"View 0","zoomNm":80,"centerLat":36.692194444444,"centerLon":3.2151111111111},
{"button":"1/2","name":"View 1/2","zoomNm":60,"centerLat":36.692194444444,"centerLon":3.2151111111111},
{"button":"1","name":"View 1","zoomNm":100,"centerLat":36.692194444444,"centerLon":3.2151111111111},
{"button":"3","name":"View 3","zoomNm":150,"centerLat":36.692194444444,"centerLon":3.2151111111111},
{"button":"5","name":"View 5","zoomNm":200,"centerLat":36.692194444444,"centerLon":3.2151111111111},
{"button":"8","name":"View 8","zoomNm":250,"centerLat":36.692194444444,"centerLon":3.2151111111111},
{"button":"VIEW1","name":"View 1","zoomNm":120,"centerLat":36.692194444444,"centerLon":3.2151111111111},
{"button":"VIEW2","name":"View 2","zoomNm":300,"centerLat":36.692194444444,"centerLon":3.2151111111111}
]```

this defines the views each button will go to when they are clicked 


<img width="1918" height="365" alt="vINDRA" src="https://github.com/user-attachments/assets/50a8c4c0-f514-47c1-971e-cabb2ccd2a1a" />

Currently (Version v0.1.4) some buttons do not work, a list is shown bellow:

- Executive / Planner - Nothing (Beta version implements vacs)
- ARR / DEP - filter showing arrivals / departures (only works for APP / TWR)
- FPL - opens the selected aircrafts Euroscope flightplan
- CPDLC - Nothing
- ABS AIR - Nothing
- View 1 - Pans to View 1 defined in Indra_saved_views.json
- View 2 - Pans to View 2 defined in Indra_saved_views.json
- LM 0 - Nothing
- RINGS - Nothing
- AREAS - Lets the user turn on / off different visibility of things from the 'Display menu' within euroscope
- ELW - Nothing
- RTE - shows the Topsky route of the aircraft (Topsky **MUST** be loaded for this feature to work)
- RBL / OBI - Nothing
- DATBLK - Nothing as of now (planned change the Tag family)
- BRIGHT / F 3D - Nothing
- QDM - Draw a SQM
- SEP - Opens the Euroscope Seperate tool with VSEP
- METEO - opens a pannel with your selected metars (Full metar)(Read from euroscope)
- RBL ALM - Nothing
- MTCD - Toggles MTCD
- ALM - Turns on / off the conflict alarm within euroscope
- OVERLAP / FREETEXT - Nothing
- Sectors - Lets the user turn on / off different visibility of things from the 'Display menu' within euroscope
- Last pos - Nothing
- FINDER - Search an aircrafts callsign that is within view and it will draw a circle around and line to that aircraft
- SSR F - Search an aircrafts Squawk that is within view and it will draw a circle around and line to that aircraft
- Users - Nothing
- Range display - Displays the current range (+ / - zooms in and/or out)
- Arrows - pans the screen up/down/left/right
- EXP + / - Zooms in / out
- CEN - Centralises the screen to view 1
- S - 8 - Pans to view S - 8 defined in Indra_saved_views.json
- Messages - becasue the bar hides your euroscope messages it opens a popout where you can read, and reply to messages within euroscope 
