# paso-chan
CEN3907C Senior Design Project: Paso-Chan

# Completed Work
1) Networking proof-of-concept
2) Consistent data storage model
3) Frontend UI for Paso-Chan desktop app

# Project Architecture
On one end of the data transmission, we have User 1's Paso-Chan. This Paso-Chan communicates data about its state via the Paso-Chan desktop app, which User 1 will have installed. The desktop app allows data to be transmitted to a relay server, which is responsible for syncing Paso-Chan's state data across both users' Paso-Chans and desktop apps. This data communication goes between User 1 and User 2's Paso-Chans and respective apps.

# Known Bugs
> Relay server is not properly port-forwarded, meaning both users must be connected to the same Wi-Fi network
