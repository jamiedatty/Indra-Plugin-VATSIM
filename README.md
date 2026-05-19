# INDRA Plugin VATSIM

## Beta Version

---

### bug reports:
- Within Maghreb vACC - Use 'Ops suggestions' thread
- Outside of Maghreb vACC - Make a issue using the bug report template

---
### vacs

The main difference between the Release and Beta versions as of version `0.1.4` is that the beta version has vacs support,

in order for this to work the user must have vacs open with **remote control enabled**

The way you do this is by going Settings -> Advanced settings -> and ensure it looks like the image bellow, if there is any issues with the base 0.0.0.0:9600, please change to 127.0.0.1:9600 (There shouldnt be)
<img width="366" height="112" alt="image" src="https://github.com/user-attachments/assets/ffe0cdac-5c22-4446-b869-ff13bbb493bc" />


An example image is shown bellow with the usage of each button

---
<img width="2000" height="50" alt="image Cropped" src="https://github.com/user-attachments/assets/4c73309c-6cb8-4fab-aef9-e8a5af9a5f35" />

---
Currently (Version v0.1.4) some buttons do not work, a list is shown bellow:

- Executive / Planner - Opens the vacs menu where the user can call people, take incoming calls and hang up
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
- DATBLK - Nothing as of now (planned change the Tag family) (says it does something but ignore it)
- BRIGHT / F 3D - Nothing
- QDM - Draw a QDM
- SEP - Opens the Euroscope Seperate tool with VSEP
- METEO - opens a pannel with your selected metars (Full metar)(Read from euroscope)
- RBL ALM - Nothing
- MTCD - Toggles MTCD
- ALM - Turns on / off the conflict alarm within euroscope
- OVERLAP - Nothing
- FREETEXT - Enters a topsky remark in the selected aircrafts tag
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
